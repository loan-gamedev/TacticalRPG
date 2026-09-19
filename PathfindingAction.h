#pragma once

// =========================
// Includes
// =========================

#include "CoreMinimal.h"
#include "PlayerAction.h"
#include "PathfindingAction.generated.h"

// =========================
// Données utilisées par
// l'algorithme A*
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
// Action de déplacement.
// Gère le pathfinding A*
// et l'affichage du chemin.
// =========================

UCLASS()
class TRPG_GAME_API UPathfindingAction : public UPlayerAction
{
    GENERATED_BODY()

public:

    // Chemin final calculé
    UPROPERTY()
    TArray<int> FinalPath;

    // Cases accessibles par l'unité
    TArray<int> ReachableTiles;

    // Dernière destination sélectionnée
    int LastTargetIndex = -1;

    // =========================
    // Actions
    // =========================

    virtual void ExecuteAction(int Index) override;

    virtual void LeftClick(int Index) override;

    virtual void RightClick(int Index) override;

    // =========================
    // Données A*
    // =========================

    TPair<int, FNodeData> startIndex;

    TPair<int, FNodeData> current;

    TPair<int, FNodeData> target;

    // Cases à explorer
    TMap<int, FNodeData> OPEN;

    // Cases déjà explorées
    TMap<int, FNodeData> CLOSED;

    // Voisins du noeud courant
    TMap<int, FNodeData> NEIGHBOURS;

    // =========================
    // Affichage
    // =========================

    void ShowReachableTiles();

    void ClearReachableTiles();

    bool flag = false;

    // =========================
    // Fonctions A*
    // =========================

    // Récupère les voisins d'une case
    TMap<int, FNodeData> GetAllNeighbours(
        int NodeToCheck,
        int parentGCost
    );

    // Calcule les coûts d'un noeud
    FNodeData GetPositionOfTheTile(
        int Target,
        int nodeIndex,
        int parentGCost,
        bool isDiagonal
    );

    // Initialise les données du pathfinding
    void InitGrid();
};