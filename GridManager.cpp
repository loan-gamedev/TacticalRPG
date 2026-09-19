#include "GridManager.h"
#include <Components/InstancedStaticMeshComponent.h>
#include "TurnManager.h"
#include <Kismet/GameplayStatics.h>
#include <Components/PrimitiveComponent.h>
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include <EnhancedInputSubsystems.h>
#include <EnhancedInputComponent.h>
#include "PlayerAction.h"

#define ECC_Grid ECC_GameTraceChannel1
#define ECC_Player ECC_GameTraceChannel2

// ============================================================================
// Constructeur : Initialisation des composants et de la physique
// ============================================================================
AGridManager::AGridManager()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
	ISM = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("ISM"));
	ISM->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);

	// Configuration des collisions pour la grille et le joueur
	ISM->SetCollisionResponseToChannel(
		ECollisionChannel::ECC_Grid,
		ECollisionResponse::ECR_Block
	);

	ISM->SetCollisionResponseToChannel(
		ECollisionChannel::ECC_Player,
		ECollisionResponse::ECR_Block
	);

	ISM->SetCollisionResponseToChannel(
		ECC_Visibility,
		ECR_Ignore
	);

	// Nombre de floats requis pour les données personnalisées du shader (Couleurs)
	ISM->NumCustomDataFloats = 4;
}

// ============================================================================
// Gestion des états des cases (Reachable, Hovered, etc.)
// ============================================================================
void AGridManager::AddStateToTile(ETileState stateToAdd, int InstanceIndex)
{
	if (TileMap.Contains(InstanceIndex))
	{
		TileMap[InstanceIndex].StateArray.Emplace(stateToAdd);
		UpdateTileVisuals(InstanceIndex);
	}
}

void AGridManager::RemoveStateToTile(ETileState stateToRemove, int InstanceIndex)
{
	if (TileMap.Contains(InstanceIndex) && TileMap[InstanceIndex].StateArray.Contains(stateToRemove)) {
		TileMap[InstanceIndex].StateArray.Remove(stateToRemove);
		UpdateTileVisuals(InstanceIndex);
	}
}

// ============================================================================
// Gestion des types de cases (Normal, Blocked, Fire, etc.)
// ============================================================================
void AGridManager::AddTypeToTile(ETileType typeToAdd, int InstanceIndex)
{
	if (TileMap.Contains(InstanceIndex))
	{
		TileMap[InstanceIndex].TypeArray = typeToAdd;
		UpdateTileVisuals(InstanceIndex);
	}
}

ETileType AGridManager::FindTileType(int index)
{
	return TileMap[index].TypeArray;
}

// ============================================================================
// Calcul visuel et mise à jour des couleurs
// ============================================================================
FLinearColor AGridManager::GetColorFromStates(TArray<ETileState> ArrayToCheck)
{
	ETileState lastState = ETileState::None;

	// Détermination de l'état prioritaire
	for (ETileState State : ArrayToCheck)
	{
		if (State < lastState)
		{
			lastState = State;
		}
	}

	if (ColorMap.Contains(lastState))
	{
		return FLinearColor(ColorMap[lastState]);
	}
	else
		return FLinearColor(0, 0, 0, 0);
}

void AGridManager::UpdateTileVisuals(int InstanceIndex)
{
	if (TileMap.Contains(InstanceIndex))
	{
		ChangeColor(InstanceIndex, GetColorFromStates(TileMap[InstanceIndex].StateArray));
	}
}

void AGridManager::ChangeColor(int InstanceIndex, FLinearColor color)
{
	// Envoi des données RGBA au Custom Data du Shader
	ISM->SetCustomDataValue(InstanceIndex, 0, color.R);
	ISM->SetCustomDataValue(InstanceIndex, 1, color.G);
	ISM->SetCustomDataValue(InstanceIndex, 2, color.B);
	ISM->SetCustomDataValue(InstanceIndex, 3, color.A);
	ISM->SetCustomDataValue(InstanceIndex, 4, color.A);
}

// ============================================================================
// Génération procédurale de la grille (Éditeur / Construction)
// ============================================================================
void AGridManager::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// Réinitialisation de l'Instanced Static Mesh
	ISM->ClearInstances();
	ISM->RegisterComponent();
	ISM->SetStaticMesh(Mesh);
	ISM->SetMaterial(0, Material);

	int index = 0;

	// Création du maillage de la grille en X et Y
	for (int i = 0; i < X; i++)
	{
		for (int j = 0; j < Y; j++)
		{
			FVector Pos(i * TileSize, j * TileSize, 0.0f);
			FTransform Trans = FTransform(FRotator(0, 0, 0), Pos, FVector(TileSize / 100, TileSize / 100, TileSize / 100));
			ISM->AddInstance(Trans);

			// Initialisation des données de structure pour chaque case
			TileMap.Emplace(index, FTileData(TArray<ETileState> {ETileState::None}, ETileType::Normal));
			index++;
		}
	}
}

bool AGridManager::isIndexValid(int index)
{
	return index != -999;
}

// ============================================================================
// Initialisation du jeu, des contrôles et des références
// ============================================================================
void AGridManager::BeginPlay()
{
	Super::BeginPlay();

	PlayerController = UGameplayStatics::GetPlayerController(GetWorld(), 0);

	// Récupération du Turn Manager
	TurnManagerRef = Cast<ATurnManager>(UGameplayStatics::GetActorOfClass(GetWorld(), ATurnManager::StaticClass()));

	// Activation de la souris et des événements de survol
	if (PlayerController)
	{
		PlayerController->bShowMouseCursor = true;
		PlayerController->bEnableClickEvents = true;
		PlayerController->bEnableMouseOverEvents = true;
	}

	EnableInput(PlayerController);

	// Configuration d'Enhanced Input (Mapping Context)
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
	{
		if (DefaultMappingContext)
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("Mapping Context must be added inside Blueprint"));
		}
	}

	// Liaison des actions de clic (Sélection / Désélection)
	if (UEnhancedInputComponent* EIC = CastChecked<UEnhancedInputComponent>(InputComponent))
	{
		EIC->BindAction(SelectAction, ETriggerEvent::Started, this, &AGridManager::OnSelect);
		EIC->BindAction(UnSelectAction, ETriggerEvent::Started, this, &AGridManager::OnUnSelect);
	}
}

// ============================================================================
// Traitement des inputs (Clic gauche / Clic droit)
// ============================================================================
void AGridManager::OnSelect(const FInputActionValue& Value)
{
	if (!CurrentAction) return;
	if (!IsValid(CurrentAction)) return;

	int clickedIndex = CurrentIndex;
	CurrentAction->LeftClick(clickedIndex);
}

void AGridManager::OnUnSelect(const FInputActionValue& Value)
{
	if (!CurrentAction) return;
	if (!IsValid(CurrentAction)) return;

	CurrentAction->RightClick(CurrentIndex);
}

// ============================================================================
// Mise à jour logique et détection du survol de la souris
// ============================================================================
void AGridManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!PlayerController) return;

	// Raycast sous la souris pour trouver la case survolée
	bool bHit = PlayerController->GetHitResultUnderCursor(ECC_Grid, false, Hit);

	if (bHit && Hit.Item != CurrentIndex)
	{
		PreviousIndex = CurrentIndex;
		CurrentIndex = Hit.Item;
	}

	if (!IsValid(CurrentAction)) return;

	// Exécution continue de l'action sur la case actuelle (ex: prévisualisation)
	CurrentAction->ExecuteAction(CurrentIndex);
}

// ============================================================================
// Outils de conversion (Index <--> Coordonnées Monde)
// ============================================================================
FVector AGridManager::GetWorldPositionFromIndex(int Index)
{
	int col = Index / Y;
	int row = Index % Y;

	FVector Origin = GetActorLocation();

	return Origin + FVector(
		col * TileSize,
		row * TileSize,
		100.f
	);
}

int AGridManager::GetIndexFromWorldPosition(FVector WorldPosition)
{
	FVector Local = WorldPosition - GetActorLocation();

	int col = FMath::RoundToInt(Local.X / TileSize);
	int row = FMath::RoundToInt(Local.Y / TileSize);

	return (col * Y) + row;
}

void AGridManager::ClearCurrentSelection()
{
	if (CurrentAction)
	{
		CurrentAction->PlayerCharacter = nullptr;
	}
}

// ============================================================================
// Mécaniques de jeu spécifiques (Effets de statut sur les tuiles)
// ============================================================================
void AGridManager::AddFireTile(int TileIndex)
{
	// Sécurité : On ne met pas le feu à un obstacle bloquant
	if (FindTileType(TileIndex) == ETileType::Blocked)
	{
		return;
	}

	// Application du type Feu et spawn des particules Niagara
	AddTypeToTile(ETileType::Fire, TileIndex);

	UNiagaraComponent* FX = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		GetWorld(),
		FireFX,
		GetWorldPositionFromIndex(TileIndex)
	);

	// Enregistrement des données de la case en feu (durée de vie)
	FFireTile NewFire;
	NewFire.FireComponent = FX;
	NewFire.TileIndex = TileIndex;
	NewFire.TurnsRemaining = 3;

	FireTiles.Add(NewFire);
}

