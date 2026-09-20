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
// Constructor: Component and physics initialization
// ============================================================================
AGridManager::AGridManager()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
	ISM = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("ISM"));
	ISM->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);

	// Collision setup for the grid and player
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

	// Number of floats required for shader custom data (colors)
	ISM->NumCustomDataFloats = 4;
}

// ============================================================================
// Tile state management (Reachable, Hovered, etc.)
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
// Tile type management (Normal, Blocked, Fire, etc.)
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
// Visual calculation and color updates
// ============================================================================
FLinearColor AGridManager::GetColorFromStates(TArray<ETileState> ArrayToCheck)
{
	ETileState lastState = ETileState::None;

	// Determining the highest-priority state
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
	// Sending RGBA values to the shader Custom Data
	ISM->SetCustomDataValue(InstanceIndex, 0, color.R);
	ISM->SetCustomDataValue(InstanceIndex, 1, color.G);
	ISM->SetCustomDataValue(InstanceIndex, 2, color.B);
	ISM->SetCustomDataValue(InstanceIndex, 3, color.A);
	ISM->SetCustomDataValue(InstanceIndex, 4, color.A);
}

// ============================================================================
// Procedural grid generation (Editor / Construction)
// ============================================================================
void AGridManager::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// Resetting the Instanced Static Mesh
	ISM->ClearInstances();
	ISM->RegisterComponent();
	ISM->SetStaticMesh(Mesh);
	ISM->SetMaterial(0, Material);

	int index = 0;

	// Creating the grid mesh along X and Y
	for (int i = 0; i < X; i++)
	{
		for (int j = 0; j < Y; j++)
		{
			FVector Pos(i * TileSize, j * TileSize, 0.0f);
			FTransform Trans = FTransform(FRotator(0, 0, 0), Pos, FVector(TileSize / 100, TileSize / 100, TileSize / 100));
			ISM->AddInstance(Trans);

			// Initializing structure data for each tile
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
// Game, input, and reference initialization
// ============================================================================
void AGridManager::BeginPlay()
{
	Super::BeginPlay();

	PlayerController = UGameplayStatics::GetPlayerController(GetWorld(), 0);

	// Retrieving the Turn Manager
	TurnManagerRef = Cast<ATurnManager>(UGameplayStatics::GetActorOfClass(GetWorld(), ATurnManager::StaticClass()));

	// Enabling mouse cursor and hover events
	if (PlayerController)
	{
		PlayerController->bShowMouseCursor = true;
		PlayerController->bEnableClickEvents = true;
		PlayerController->bEnableMouseOverEvents = true;
	}

	EnableInput(PlayerController);

	// Enhanced Input setup (Mapping Context)
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

	// Binding click actions (Select / Deselect)
	if (UEnhancedInputComponent* EIC = CastChecked<UEnhancedInputComponent>(InputComponent))
	{
		EIC->BindAction(SelectAction, ETriggerEvent::Started, this, &AGridManager::OnSelect);
		EIC->BindAction(UnSelectAction, ETriggerEvent::Started, this, &AGridManager::OnUnSelect);
	}
}

// ============================================================================
// Input handling (Left click / Right click)
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
// Logic update and mouse hover detection
// ============================================================================
void AGridManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!PlayerController) return;

	// Raycast under the mouse to find the hovered tile
	bool bHit = PlayerController->GetHitResultUnderCursor(ECC_Grid, false, Hit);

	if (bHit && Hit.Item != CurrentIndex)
	{
		PreviousIndex = CurrentIndex;
		CurrentIndex = Hit.Item;
	}

	if (!IsValid(CurrentAction)) return;

	// Continuously executing the action on the current tile (e.g. preview)
	CurrentAction->ExecuteAction(CurrentIndex);
}

// ============================================================================
// Conversion utilities (Index <--> World Coordinates)
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
// Specific gameplay mechanics (Tile status effects)
// ============================================================================
void AGridManager::AddFireTile(int TileIndex)
{
	// Safety: Do not set a blocking obstacle on fire
	if (FindTileType(TileIndex) == ETileType::Blocked)
	{
		return;
	}

	// Applying the Fire type and spawning Niagara particles
	AddTypeToTile(ETileType::Fire, TileIndex);

	UNiagaraComponent* FX = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		GetWorld(),
		FireFX,
		GetWorldPositionFromIndex(TileIndex)
	);

	// Storing fire tile data (lifetime)
	FFireTile NewFire;
	NewFire.FireComponent = FX;
	NewFire.TileIndex = TileIndex;
	NewFire.TurnsRemaining = 3;

	FireTiles.Add(NewFire);
}

