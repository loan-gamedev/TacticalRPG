#pragma once

// =========================
// Includes
// =========================

#include "CoreMinimal.h"
#include "PlayerAction.h"
#include "PathfindingAction.generated.h"

// =========================
// Data used by
// the A* algorithm
// =========================

USTRUCT(BlueprintType)
struct FNodeData
{
    GENERATED_BODY()

    int g_cost = 0;
    int h_cost = 0;
    int f_cost = 0;
    int parent_index = -1;

    FNodeData() {}

    FNodeData(
        int g,
        int h,
        int f,
        int p
    )
        : g_cost(g),
        h_cost(h),
        f_cost(f),
        parent_index(p)
    {}
};

class AUnits;

// =========================
// Movement action.
// Handles A* pathfinding
// and path visualization.
// =========================

UCLASS()
class TRPG_GAME_API UPathfindingAction : public UPlayerAction
{
    GENERATED_BODY()

public:

    // Final calculated path
    UPROPERTY()
    TArray<int> FinalPath;

    // Tiles reachable by the unit
    TArray<int> ReachableTiles;

    // Last selected destination
    int LastTargetIndex = -1;

    // =========================
    // Actions
    // =========================

    virtual void ExecuteAction(int Index) override;

    virtual void LeftClick(int Index) override;

    virtual void RightClick(int Index) override;

    // =========================
    // A* data
    // =========================

    TPair<int, FNodeData> startIndex;

    TPair<int, FNodeData> current;

    TPair<int, FNodeData> target;

    // Tiles to explore
    TMap<int, FNodeData> OPEN;

    // Already explored tiles
    TMap<int, FNodeData> CLOSED;

    // Neighbors of the current node
    TMap<int, FNodeData> NEIGHBOURS;

    // =========================
    // Display
    // =========================

    void ShowReachableTiles();

    void ClearReachableTiles();

    bool flag = false;

    // =========================
    // A* functions
    // =========================

    // Gets the neighbors of a tile
    TMap<int, FNodeData> GetAllNeighbours(
        int NodeToCheck,
        int parentGCost
    );

    // Calculates the costs of a node
    FNodeData GetPositionOfTheTile(
        int Target,
        int nodeIndex,
        int parentGCost,
        bool isDiagonal
    );

    // Initializes pathfinding data
    void InitGrid();
};