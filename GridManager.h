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
// Etats visuels d'une case
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
// Type réel d'une case
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
// Données stockées par case
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
// Données utilisées pour
// gérer les cases en feu
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
// Gestion de la grille,
// des cases et des actions
// =========================

UCLASS()
class TRPG_GAME_API AGridManager : public AActor
{
	GENERATED_BODY()

public:

	// =========================
	// Constructeur / Tick
	// =========================

	AGridManager();

	virtual void Tick(float DeltaTime) override;

	// =========================
	// Références
	// =========================

	UPROPERTY(BlueprintReadOnly)
	ATurnManager* TurnManagerRef;

	UPROPERTY()
	UPlayerAction* CurrentAction;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tab")
	APlayerController* PlayerController;

	// =========================
	// Système de feu
	// =========================

	UPROPERTY()
	TArray<FFireTile> FireTiles;

	UPROPERTY(EditAnywhere)
	UNiagaraSystem* FireFX;

	UFUNCTION()
	void AddFireTile(int TileIndex);

	// =========================
	// Gestion des cases
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

	// Retourne le type d'une case
	ETileType FindTileType(int index);

	// =========================
	// Paramètres de la grille
	// =========================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tab")
	int X;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tab")
	int Y;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tab")
	float TileSize;

	// =========================
	// Conversion monde <-> grille
	// =========================

	UFUNCTION(BlueprintCallable)
	int GetIndexFromWorldPosition(
		FVector WorldPosition
	);

	FVector GetWorldPositionFromIndex(
		int Index
	);

	// =========================
	// Données des cases
	// =========================

	UPROPERTY()
	TMap<int, FTileData> TileMap;

protected:

	// =========================
	// Initialisation
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
	// Affichage des cases
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
	// Mesh de la grille
	// =========================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh")
	class UStaticMesh* Mesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh")
	class UInstancedStaticMeshComponent* ISM;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh")
	class UMaterialInterface* Material;

	// =========================
	// Hit Result souris
	// =========================

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tab")
	FHitResult Hit;
};