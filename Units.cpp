#include "Units.h"
#include "UnitInterface.h"
#include "Engine/DataTable.h"
#include "Kismet/GameplayStatics.h"
#include "Blueprint/UserWidget.h"
#include "TurnManager.h"
#include "Components/WidgetComponent.h"
#include "GridManager.h"
#include "UnitDataAsset.h"

// ============================================================================
// Constructor: Initial unit setup
// ============================================================================
AUnits::AUnits()
{
	PrimaryActorTick.bCanEverTick = true;
}

// ============================================================================
// Game initialization: Linking global managers
// ============================================================================
void AUnits::BeginPlay()
{
	Super::BeginPlay();

	// Retrieving the Grid Manager reference present in the map
	GridManagerRef = Cast<AGridManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AGridManager::StaticClass()));
}

// ============================================================================
// Construction phase: Loading stats from the Data Table
// ============================================================================
void AUnits::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (!UnitDataTable)
	{
		UE_LOG(LogTemp, Error, TEXT("NO UNIT DATA TABLE"));
		return;
	}

	// Finding the row matching the unit type (e.g. Warrior, Mage...)
	FSUnit* Row = UnitDataTable->FindRow<FSUnit>(UnitRowName, TEXT(""));

	if (!Row)
	{
		UE_LOG(LogTemp, Error, TEXT("ROW NOT FOUND"));
		return;
	}

	if (!GetMesh()) return;

	// Assigning Data Table stats to instance variables
	CurrentHealth = Row->Health;
	MaxHealth = Row->MaxHealth;
	Damage = Row->Damage;
	MoveRange = Row->MoveRange;
	AttackRange = Row->AttackRange;
	MoveSpeed = Row->MoveSpeed;
	MovePattern = Row->MovePattern;
	AttackPattern = Row->AttackPattern;
	SkillType = Row->SkillType;
	BombClass = Row->BombClass;
	XPReward = Row->XPReward;
	Level = Row->Level;
	Name = Row->Name;
}

void AUnits::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

// ============================================================================
// Movement and navigation management
// ============================================================================
void AUnits::SetPath(const TArray<int>& NewPath)
{
	CurrentPath = NewPath;
	CurrentPathIndex = 0;
	bIsMoving = true;

	// Triggering the walk animation unless this is a fast dash
	if (!bIsDash)
	{
		AnimState = EUnitAnimState::WALK;
	}
}

void AUnits::MoveAlongPath(float DeltaTime)
{
	// Interpolated movement logic along tiles (implemented in Blueprint or to be completed)
}

// ============================================================================
// Combat system: Damage handling and lifecycle
// ============================================================================
void AUnits::ReceiveDamage(float Amount)
{
	// Displaying floating damage text and visual feedback
	ShowDamageText(Amount);
	AnimState = EUnitAnimState::HIT;

	CurrentHealth -= Amount;

	// Notification to update the health bar (UI)
	OnHealthChanged.Broadcast(CurrentHealth / MaxHealth);

	if (CurrentHealth <= 0.f)
	{
		Die();
		return;
	}

	// Setting up the return to Idle after the hit animation
	FTimerHandle HitTimer;
	GetWorld()->GetTimerManager().SetTimer(
		HitTimer,
		[this]()
		{
			if (AnimState != EUnitAnimState::DEATH)
			{
				AnimState = EUnitAnimState::IDLE;
			}
		},
		1.5f,
		false
	);
}

void AUnits::Die()
{
	OnUnitDied.Broadcast(this);

	// Freeing the grid tile to make it traversable again
	int TileIndex = GridManagerRef->GetIndexFromWorldPosition(GetActorLocation());
	GridManagerRef->AddTypeToTile(ETileType::Normal, TileIndex);

	AnimState = EUnitAnimState::DEATH;

	// Disabling collision and scheduling Actor destruction
	SetActorEnableCollision(false);
	SetLifeSpan(2.f);
}

// ============================================================================
// Progression: Experience (XP) and level-up management
// ============================================================================
void AUnits::GainXP(int Amount)
{
	CurrentXP += Amount;

	UE_LOG(LogTemp, Warning, TEXT("%s XP : %d"), *GetName(), CurrentXP);

	if (CurrentXP >= XPToNextLevel)
	{
		LevelUp();
	}
}

void AUnits::LevelUp()
{
	CurrentXP -= XPToNextLevel;
	Level++;

	// Dynamic level threshold and global stat increases
	XPToNextLevel = Level * 100;
	MaxHealth += 20.f;
	CurrentHealth = MaxHealth; // Heals the unit on level up
	Damage += 5.f;

	UE_LOG(LogTemp, Warning, TEXT("%s LEVEL UP : %d"), *GetName(), Level);

	// Updating information displayed on the combat UI
	ATurnManager* TurnManager = Cast<ATurnManager>(UGameplayStatics::GetActorOfClass(GetWorld(), ATurnManager::StaticClass()));

	if (TurnManager && TurnManager->CurrentUnit == this)
	{
		TurnManager->RefreshHUD();
	}
}

// ============================================================================
// User Interface: Displaying damage popups (Screen Space)
// ============================================================================
void AUnits::ShowDamageText(float DamageAmount)
{
	if (!DamageWidgetClass) return;

	UUserWidget* Widget = CreateWidget(GetWorld(), DamageWidgetClass);
	if (!Widget) return;

	Widget->AddToViewport();

	// Sending the numeric value to the Blueprint Widget update function
	UFunction* Function = Widget->FindFunction(FName("SetDamage"));
	if (Function)
	{
		struct FDamageParams
		{
			FText Damage;
		};

		FDamageParams Params;
		FString DamageString = FString::Printf(TEXT("-%d"), (int)DamageAmount);
		Params.Damage = FText::FromString(DamageString);

		Widget->ProcessEvent(Function, &Params);
	}

	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC) return;

	// Projecting the unit head position from 3D world space to 2D screen coordinates
	FVector2D ScreenPosition;
	UGameplayStatics::ProjectWorldToScreen(
		PC,
		GetActorLocation() + FVector(0, 0, 150),
		ScreenPosition
	);

	Widget->SetPositionInViewport(ScreenPosition);
}

// ============================================================================
// Action range: Distance calculation based on the attack pattern
// ============================================================================
bool AUnits::IsTargetInAttackRange(AUnits* Target)
{
	if (!Target) return false;

	// Extracting grid coordinates (Columns / Rows)
	int MyIndex = GridManagerRef->GetIndexFromWorldPosition(GetActorLocation());
	int TargetIndex = GridManagerRef->GetIndexFromWorldPosition(Target->GetActorLocation());

	int MyCol = MyIndex / GridManagerRef->Y;
	int MyRow = MyIndex % GridManagerRef->Y;

	int TargetCol = TargetIndex / GridManagerRef->Y;
	int TargetRow = TargetIndex % GridManagerRef->Y;

	// Calculating absolute distances on the X and Y axes (Manhattan distance)
	int DX = FMath::Abs(MyCol - TargetCol);
	int DY = FMath::Abs(MyRow - TargetRow);

	// Geometric filtering based on the attack pattern shape
	switch (AttackPattern)
	{
	case EAttackPattern::Cross:
		return ((DX == 0 || DY == 0) && (DX + DY <= AttackRange));

	case EAttackPattern::Circle:
		return (DX + DY <= AttackRange);

	case EAttackPattern::Line:
		return (DX == DY && DX <= AttackRange);
	}

	return false;
}
