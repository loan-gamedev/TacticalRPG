#include "Player_Character.h"
#include "DrawDebugHelpers.h"
#include "GridManager.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UnitStruct.h"
#include "TurnManager.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/GameplayStatics.h"
#include "Components/CapsuleComponent.h"

#define ECC_Player ECC_GameTraceChannel2

// =========================
// Constructor
// =========================

APlayer_Character::APlayer_Character()
{
	PrimaryActorTick.bCanEverTick = true;

	PreviousTileType = ETileType::Normal;

	GetCapsuleComponent()->SetCollisionResponseToChannel(
		ECC_Visibility,
		ECR_Block
	);

	GetCapsuleComponent()->OnClicked.AddDynamic(
		this,
		&APlayer_Character::OnPlayerClicked
	);
}

// =========================
// Initialization
// =========================

void APlayer_Character::BeginPlay()
{
	Super::BeginPlay();

	GetCharacterMovement()->DisableMovement();

	CurrentTileIndex =
		GridManagerRef->GetIndexFromWorldPosition(
			GetActorLocation()
		);

	PreviousTileType =
		GridManagerRef->FindTileType(
			CurrentTileIndex
		);

	GridManagerRef->AddTypeToTile(
		ETileType::Blocked,
		CurrentTileIndex
	);

	BaseMoveSpeed = MoveSpeed;
}

// =========================
// Tick
// =========================

void APlayer_Character::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	MoveAlongPath(DeltaTime);
}

// =========================
// Generic helper: calls a Blueprint function on the widget
// via reflection, avoiding repeated FindFunction/ProcessEvent calls
// =========================

template<typename ParamType, typename SetterFunc>
static void CallWidgetFunction(UUserWidget* Widget, FName FunctionName, SetterFunc Setter)
{
    if (!Widget) return;

    UFunction* Function = Widget->FindFunction(FunctionName);
    if (!Function) return;

    ParamType Params;
    Setter(Params);
    Widget->ProcessEvent(Function, &Params);
}

// =========================
// Player selection
// =========================

void APlayer_Character::SelectPlayer()
{
    bIsSelected = true;

    UE_LOG(LogTemp, Warning, TEXT("PLAYER SELECTED"));

    ATurnManager* TurnManager = Cast<ATurnManager>(
        UGameplayStatics::GetActorOfClass(GetWorld(), ATurnManager::StaticClass())
    );

    if (!TurnManager || !TurnManager->HUDWidget) return;

    UWidget* FoundWidget = TurnManager->HUDWidget->GetWidgetFromName(FName("UnitWidget"));
    UUserWidget* UnitWidget = Cast<UUserWidget>(FoundWidget);

    if (!UnitWidget) return;

    // Generic parameter struct reused for all text fields
    struct FTextParams { FText InText; };

    CallWidgetFunction<FTextParams>(UnitWidget, FName("SetName"), [this](FTextParams& P) {
        P.InText = FText::FromString(Name);
    });

    CallWidgetFunction<FTextParams>(UnitWidget, FName("SetLevel"), [this](FTextParams& P) {
        P.InText = FText::AsNumber(Level);
    });

    CallWidgetFunction<FTextParams>(UnitWidget, FName("SetHP"), [this](FTextParams& P) {
        P.InText = FText::FromString(FString::Printf(TEXT("%d / %d"), (int)CurrentHealth, (int)MaxHealth));
    });

    CallWidgetFunction<FTextParams>(UnitWidget, FName("SetDamage"), [this](FTextParams& P) {
        P.InText = FText::AsNumber(Damage);
    });

    CallWidgetFunction<FTextParams>(UnitWidget, FName("SetType"), [this](FTextParams& P) {
        P.InText = FText::FromName(UnitRowName);
    });

    struct FImageParams { UTexture2D* Texture; };

    CallWidgetFunction<FImageParams>(UnitWidget, FName("SetImage"), [this](FImageParams& P) {
        P.Texture = PortraitTexture;
    });
}

// =========================
// Player deselection
// =========================

void APlayer_Character::UnselectPlayer()
{
	bIsSelected = false;
}

// =========================
// Movement along the path
// =========================

void APlayer_Character::MoveAlongPath(float DeltaTime)
{
	if (!GridManagerRef) return;

	if (CurrentPath.Num() <= 0)
	{
		bIsMoving = false;
		bLockActions = false;
		bIsDash = false;
		MoveSpeed = BaseMoveSpeed;

		if (
			AnimState != EUnitAnimState::SKILL
			&&
			AnimState != EUnitAnimState::ATTACK
			&&
			AnimState != EUnitAnimState::DEATH
			&&
			AnimState != EUnitAnimState::HIT
			)
		{
			AnimState =
				EUnitAnimState::IDLE;
		}
		return;
	}

	if (CurrentPathIndex >= CurrentPath.Num())
	{
		bIsMoving = false;

		bLockActions = false;

		if (!bSkillMovement)
		{
			bHasMoved = true;
		}

		bSkillMovement = false;

		if (bIsDash)
		{
			MoveSpeed = BaseMoveSpeed;

			bIsDash = false;
		}

		if (
			AnimState != EUnitAnimState::SKILL
			&&
			AnimState != EUnitAnimState::ATTACK
			&&
			AnimState != EUnitAnimState::DEATH
			&&
			AnimState != EUnitAnimState::HIT
			)
		{
			AnimState =
				EUnitAnimState::IDLE;
		}

		CurrentPath.Empty();

		UE_LOG(
			LogTemp,
			Warning,
			TEXT("DESTINATION REACHED")
		);

		return;
	}

	int TileIndex =
		CurrentPath[CurrentPathIndex];

	FVector TargetLocation =
		GridManagerRef->GetWorldPositionFromIndex(
			TileIndex
		);



	FRotator LookRotation =
		(TargetLocation - GetActorLocation()).Rotation();

	SetActorRotation(
		FMath::RInterpTo(
			GetActorRotation(),
			LookRotation,
			DeltaTime,
			10.f
		)
	);

	FVector NewLocation =
		FMath::VInterpConstantTo(
			GetActorLocation(),
			TargetLocation,
			DeltaTime,
			MoveSpeed
		);

	SetActorLocation(NewLocation);

	float Distance =
		FVector::Dist(
			NewLocation,
			TargetLocation
		);

	if (Distance <= AcceptanceDistance)
	{
		int NewTile =
			GridManagerRef->GetIndexFromWorldPosition(
				TargetLocation
			);

		// =========================
		// Restoring the previous tile
		// =========================

		if (
			PreviousTileType
			==
			ETileType::Hole
			)
		{
			GridManagerRef->AddTypeToTile(
				ETileType::Hole,
				CurrentTileIndex
			);
		}
		else
		{
			GridManagerRef->AddTypeToTile(
				ETileType::Normal,
				CurrentTileIndex
			);
		}

		// =========================
		// Saving the actual tile type
		// =========================

		PreviousTileType =
			GridManagerRef->FindTileType(
				NewTile
			);

		// =========================
		// Fire check
		// =========================

		bool bWasFire =
			(
				PreviousTileType
				==
				ETileType::Fire
				);

		// =========================
		// Blocking the new tile
		// =========================

		GridManagerRef->AddTypeToTile(
			ETileType::Blocked,
			NewTile
		);

		// =========================
		// Creating fire on the previous tile
		// =========================

		GridManagerRef->AddFireTile(
			CurrentTileIndex
		);

		CurrentTileIndex =
			NewTile;

		// =========================
		// Applying damage
		// =========================

		if (bWasFire)
		{
			ReceiveDamage(DamageFire);
		}

		CurrentPathIndex++;
	}
}

// =========================
// Player click
// =========================

void APlayer_Character::OnPlayerClicked(
	UPrimitiveComponent* TouchedComponent,
	FKey ButtonPressed
)
{
	bIsSelected = true;

	UE_LOG(LogTemp, Warning, TEXT("PLAYER SELECTED"));
}

// =========================
// Return to idle state
// =========================

void APlayer_Character::ResetToIdle()
{
	if (
		bIsMoving
		||
		bSkillMovement
		)
	{
		return;
	}

	AnimState =
		EUnitAnimState::IDLE;
}