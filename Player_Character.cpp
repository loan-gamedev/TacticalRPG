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
// Constructeur
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
// Initialisation
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
// Helper générique : appelle une fonction Blueprint du widget
// via reflection, en évitant la répétition de FindFunction/ProcessEvent
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
// Sélection du joueur
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

    // Struct params génériques réutilisés pour tous les champs texte
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
// Désélection du joueur
// =========================

void APlayer_Character::UnselectPlayer()
{
	bIsSelected = false;
}

// =========================
// Déplacement sur le chemin
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
		// Restauration de l'ancienne case
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
		// Sauvegarde du type réel
		// =========================

		PreviousTileType =
			GridManagerRef->FindTileType(
				NewTile
			);

		// =========================
		// Vérification du feu
		// =========================

		bool bWasFire =
			(
				PreviousTileType
				==
				ETileType::Fire
				);

		// =========================
		// Blocage de la nouvelle case
		// =========================

		GridManagerRef->AddTypeToTile(
			ETileType::Blocked,
			NewTile
		);

		// =========================
		// Création du feu sur l'ancienne case
		// =========================

		GridManagerRef->AddFireTile(
			CurrentTileIndex
		);

		CurrentTileIndex =
			NewTile;

		// =========================
		// Application des dégâts
		// =========================

		if (bWasFire)
		{
			ReceiveDamage(DamageFire);
		}

		CurrentPathIndex++;
	}
}

// =========================
// Clic sur le joueur
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
// Retour à l'état idle
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