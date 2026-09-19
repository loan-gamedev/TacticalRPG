#pragma once

// =========================
// Includes
// =========================

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PlayerAction.h"
#include "NiagaraSystem.h"
#include "GridManager.generated.h"

class ATurnManager;

// =========================
// Visual states of a tile
// =========================

UENUM(BlueprintType)
enum class ETileState : uint8
{
	Selected,
	Hovered,
	Way,
	Occuped,
	Possible,
	Reachable,
	Fire,
	None UMETA(DisplayName = "Not Hovered"),
	MAX UMETA(Hidden)
};

ENUM_RANGE_BY_COUNT(ETileState, ETileState::MAX);

// =========================
// Actual tile type
// =========================

UENUM(BlueprintType)
enum class ETileType : uint8
{
	Occuped,
	Blocked,
	Normal,
	Hole,
	Fire,
	Max UMETA(Hidden)
};

// =========================
// Data stored for each tile
// =========================

USTRUCT(BlueprintType)
struct FTileData
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	TArray<ETileState> StateArray;

	ETileType TypeArray;
};

// =========================
// Data used to
// manage fire tiles
// =========================

USTRUCT()
struct FFireTile
{
	GENERATED_BODY()

	UPROPERTY()
	int TileIndex;

	UPROPERTY()
	int TurnsRemaining;

	UPROPERTY()
	UNiagaraComponent* FireComponent;
};

// =========================
// Grid management,
// tiles, and actions
// =========================

UCLASS()
class TRPG_GAME_API AGridManager : public AActor
{
	GENERATED_BODY()

public:

	// =========================
	// Constructor / Tick
	// =========================

	AGridManager();

	virtual void Tick(float DeltaTime) override;

	// =========================
	// References
	// =========================

	UPROPERTY(BlueprintReadOnly)
	ATurnManager* TurnManagerRef;

	UPROPERTY()
	UPlayerAction* CurrentAction;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tab")
	APlayerController* PlayerController;

	// =========================
	// Fire system
	// =========================

	UPROPERTY()
	TArray<FFireTile> FireTiles;

	UPROPERTY(EditAnywhere)
	UNiagaraSystem* FireFX;

	UFUNCTION()
	void AddFireTile(int TileIndex);

	// =========================
	// Tile management
	// =========================

	bool isIndexValid(int index);

	int CurrentIndex = -999;

	int PreviousIndex = CurrentIndex;

	void AddStateToTile(
		ETileState stateToAdd,
		int InstanceIndex
	);

	void RemoveStateToTile(
		ETileState stateToRemove,
		int InstanceIndex
	);

	void ClearCurrentSelection();

	UFUNCTION(BlueprintCallable)
	void AddTypeToTile(
		ETileType typeToAdd,
		int InstanceIndex
	);

	// Returns the type of a tile
	ETileType FindTileType(int index);

	// =========================
	// Grid settings
	// =========================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tab")
	int X;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tab")
	int Y;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tab")
	float TileSize;

	// =========================
	// World <-> grid conversion
	// =========================

	UFUNCTION(BlueprintCallable)
	int GetIndexFromWorldPosition(
		FVector WorldPosition
	);

	FVector GetWorldPositionFromIndex(
		int Index
	);

	// =========================
	// Tile data
	// =========================

	UPROPERTY()
	TMap<int, FTileData> TileMap;

protected:

	// =========================
	// Initialization
	// =========================

	virtual void BeginPlay() override;

	UFUNCTION()
	void OnConstruction(
		const FTransform& Transform
	) override;

	// =========================
	// Input
	// =========================

	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadWrite,
		Category = "Input System"
	)
	class UInputMappingContext* DefaultMappingContext;

	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadWrite,
		Category = "Input System"
	)
	class UInputAction* SelectAction;

	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadWrite,
		Category = "Input System"
	)
	class UInputAction* UnSelectAction;

	void OnSelect(
		const struct FInputActionValue& Value
	);

	void OnUnSelect(
		const struct FInputActionValue& Value
	);

	// =========================
	// Tile display
	// =========================

	void UnHoverTile(int index);

	void ChangeColor(
		int InstanceIndex,
		FLinearColor color
	);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colors")
	TMap<ETileState, FLinearColor> ColorMap;

	FLinearColor GetColorFromStates(
		TArray<ETileState> ArrayToCheck
	);

	void UpdateTileVisuals(
		int InstanceIndex
	);

	// =========================
	// Grid mesh
	// =========================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh")
	class UStaticMesh* Mesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh")
	class UInstancedStaticMeshComponent* ISM;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh")
	class UMaterialInterface* Material;

	// =========================
	// Mouse Hit Result
	// =========================

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tab")
	FHitResult Hit;
};