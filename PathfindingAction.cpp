#include "PathfindingAction.h"
#include "GridManager.h"
#include "Player_Character.h"
#include "EngineUtils.h"
#include <cstdlib>
#include <algorithm>

#define ECC_Player ECC_GameTraceChannel2

// ============================================================================
// Logique d'exécution continue (Survol / Hover de la souris)
// ============================================================================
void UPathfindingAction::ExecuteAction(int Index)
{
    // Retire le hover de l'ancienne case survolée
    if (GridRef->isIndexValid(GridRef->PreviousIndex))
    {
        GridRef->RemoveStateToTile(ETileState::Hovered, GridRef->PreviousIndex);
    }

    // Applique le hover sur la nouvelle case sous le curseur
    if (GridRef->isIndexValid(Index))
    {
        GridRef->AddStateToTile(ETileState::Hovered, Index);
        GridRef->CurrentIndex = Index;
    }
    else
    {
        GridRef->CurrentIndex = -1;
    }
}

// ============================================================================
// Gestion du Clic Gauche : Sélection de personnage et calcul de chemin (A*)
// ============================================================================
void UPathfindingAction::LeftClick(int Index)
{
    bool bFoundPlayer = false;

    // ------------------------------------------------------------------------
    // Phase 1 : Recherche d'un personnage sur la case cliquée
    // ------------------------------------------------------------------------
    for (TActorIterator<APlayer_Character> It(GetWorld()); It; ++It)
    {
        APlayer_Character* FoundPlayer = *It;
        if (!FoundPlayer) continue;

        int PlayerTile = GridRef->GetIndexFromWorldPosition(FoundPlayer->GetActorLocation());
        if (PlayerTile != Index) continue;

        // Personnage trouvé !
        bFoundPlayer = true;
        ClearReachableTiles();

        // Gestion du changement de sélection de personnage
        if (PlayerCharacter && PlayerCharacter != FoundPlayer)
        {
            PlayerCharacter->UnselectPlayer();
        }

        PlayerCharacter = FoundPlayer;
        PlayerCharacter->SelectPlayer();
        ShowReachableTiles(); // Affiche sa zone de mouvement maximale

        break;
    }

    if (bFoundPlayer) return;

    // ------------------------------------------------------------------------
    // Phase 2 : Gestion des conditions de déplacement (Si aucun joueur cliqué)
    // ------------------------------------------------------------------------
    if (!PlayerCharacter || !PlayerCharacter->bIsMyTurn || PlayerCharacter->bHasMoved || !PlayerCharacter->bIsSelected)
    {
        return;
    }

    if (!GridRef || !GridRef->isIndexValid(Index)) return;

    // Vérification de la validité de la destination (Doit être dans la zone bleue/Reachable)
    if (!GridRef->TileMap[Index].StateArray.Contains(ETileState::Reachable))
    {
        return;
    }

    if (!GridRef->isIndexValid(startIndex.Key)) return;
    if (LastTargetIndex == Index) return;

    LastTargetIndex = Index;

    // ------------------------------------------------------------------------
    // Phase 3 : Algorithme de Pathfinding A*
    // ------------------------------------------------------------------------
    InitGrid();

    OPEN.Emplace(startIndex.Key, startIndex.Value);
    target.Key = Index;

    if (target.Key == startIndex.Key) return;

    while (!flag && OPEN.Num() > 0)
    {
        current = *OPEN.begin();

        // Recherche du nœud avec le coût total F le plus bas
        for (TPair<int, FNodeData>& index : OPEN)
        {
            if (index.Value.f_cost < current.Value.f_cost)
            {
                current.Key = index.Key;
                current.Value = index.Value;
            }
        }

        OPEN.Remove(current.Key);
        CLOSED.Emplace(current.Key, current.Value);

        // Destination atteinte, on sort de la boucle
        if (current.Key == target.Key)
        {
            flag = true;
            break;
        }

        NEIGHBOURS = GetAllNeighbours(current.Key, current.Value.g_cost);

        // Analyse des tuiles voisines
        for (TPair<int, FNodeData> neighbourToCheck : NEIGHBOURS)
        {
            int neighbourKey = neighbourToCheck.Key;
            FNodeData neighbourData = neighbourToCheck.Value;

            ETileType TileType = GridRef->FindTileType(neighbourKey);
            bool bIsEngineer = PlayerCharacter->UnitRowName == FName("Engineer");
            bool bIsCurrentTile = neighbourKey == PlayerCharacter->CurrentTileIndex;

            // Filtre des obstacles de la carte selon le profil du personnage
            if ((TileType == ETileType::Blocked && !bIsCurrentTile) ||
                (TileType == ETileType::Hole && !bIsEngineer))
            {
                continue;
            }

            if (CLOSED.Contains(neighbourKey)) continue;

            // Mise à jour ou ajout du nœud dans la liste OPEN
            if (!OPEN.Contains(neighbourKey) || neighbourData.f_cost < OPEN[neighbourKey].f_cost)
            {
                neighbourData.parent_index = current.Key;

                if (!OPEN.Contains(neighbourKey))
                    OPEN.Add(neighbourKey, neighbourData);
                else
                    OPEN[neighbourKey] = neighbourData;
            }
        }
    }

    // ------------------------------------------------------------------------
    // Phase 4 : Reconstruction et validation du chemin final
    // ------------------------------------------------------------------------
    FinalPath.Empty();

    if (flag)
    {
        int min = 0;
        int max = GridRef->X * GridRef->Y;

        // Remontée des parents pour recréer la chaîne du chemin parcouru
        while (target.Key != startIndex.Key && min < max)
        {
            if (!CLOSED.Contains(target.Key) || CLOSED[target.Key].parent_index == -1)
                break;

            FinalPath.Add(target.Key);
            target.Key = CLOSED[target.Key].parent_index;
            min++;
        }

        FinalPath.Add(startIndex.Key);
        Algo::Reverse(FinalPath); // Remise du chemin dans le bon sens (Départ -> Arrivée)

        // Sécurité : On refuse le mouvement si le chemin réel dépasse la stat du personnage
        if (FinalPath.Num() - 1 > PlayerCharacter->MoveRange)
        {
            return;
        }

        // Transmission du chemin au personnage et nettoyage du visuel de la grille
        PlayerCharacter->SetPath(FinalPath);
        ClearReachableTiles();
    }
}

// ============================================================================
// Gestion du Clic Droit : Annulation et Reset
// ============================================================================
void UPathfindingAction::RightClick(int Index)
{
    if (!GridRef) return;

    // Désélectionne visuellement la case ciblée
    if (GridRef->isIndexValid(Index))
    {
        GridRef->RemoveStateToTile(ETileState::Selected, Index);
    }

    InitGrid();
}

// ============================================================================
// Nettoyage et initialisation des structures de données de calcul
// ============================================================================
void UPathfindingAction::InitGrid()
{
    // Efface le tracé du chemin précédent sur la grille
    for (auto& neigh : CLOSED)
    {
        GridRef->RemoveStateToTile(ETileState::Way, neigh.Key);
    }

    OPEN.Empty();
    CLOSED.Empty();
    NEIGHBOURS.Empty();

    flag = false;

    // Définition du point de départ A* sur la position actuelle du joueur
    startIndex.Key = GridRef->GetIndexFromWorldPosition(PlayerCharacter->GetActorLocation());
    startIndex.Value = FNodeData(0, 0, 0, -1);
}

// ============================================================================
// Mathématiques A* : Récupération et évaluation des voisins (Grille 2D)
// ============================================================================
TMap<int, FNodeData> UPathfindingAction::GetAllNeighbours(int NodeToCheck, int parentGCost)
{
    TMap<int, FNodeData> NEWNEIGHBOURS;

    int col = NodeToCheck / GridRef->Y;
    int row = NodeToCheck % GridRef->Y;

    // Analyse de l'entourage direct (3x3 autour du nœud)
    for (int i = -1; i <= 1; i++)
    {
        for (int j = -1; j <= 1; j++)
        {
            if (i == 0 && j == 0) continue; // Ignore le centre

            int newCol = col + i;
            int newRow = row + j;

            // Restreint la recherche aux limites de la map
            if (newCol < 0 || newCol >= GridRef->X) continue;
            if (newRow < 0 || newRow >= GridRef->Y) continue;

            int neighbourIndex = newCol * GridRef->Y + newRow;
            bool isDiagonal = (i != 0 && j != 0);

            // Calcul des coûts heuristiques et ajout à la liste locale des voisins
            NEWNEIGHBOURS.Emplace(
                neighbourIndex,
                GetPositionOfTheTile(target.Key, neighbourIndex, parentGCost, isDiagonal)
            );
        }
    }

    return NEWNEIGHBOURS;
}

FNodeData UPathfindingAction::GetPositionOfTheTile(int Target, int nodeIndex, int parentGCost, bool isDiagonal)
{
    // Coût G : Coût du chemin depuis le point de départ (10 en ligne droite, 14 en diagonale)
    int g = parentGCost + (isDiagonal ? 14 : 10);

    // Coût H : Distance de Manhattan / Heuristique diagonale jusqu'à la cible
    int dx_h = abs((nodeIndex / GridRef->Y) - (Target / GridRef->Y));
    int dy_h = abs((nodeIndex % GridRef->Y) - (Target % GridRef->Y));
    int h = 14 * std::min(dx_h, dy_h) + 10 * abs(dx_h - dy_h);

    return FNodeData(g, h, g + h, -1);
}

// ============================================================================
// Gestion Visuelle : Affichage du pattern de portée (Mouvement)
// ============================================================================
void UPathfindingAction::ShowReachableTiles()
{
    if (!PlayerCharacter || PlayerCharacter->bHasMoved || PlayerCharacter->bIsMoving)
    {
        return;
    }

    int StartIndex = GridRef->GetIndexFromWorldPosition(PlayerCharacter->GetActorLocation());
    int StartCol = StartIndex / GridRef->Y;
    int StartRow = StartIndex % GridRef->Y;

    // Balayage de la zone délimitée par la portée de déplacement maximale
    for (int x = -PlayerCharacter->MoveRange; x <= PlayerCharacter->MoveRange; x++)
    {
        for (int y = -PlayerCharacter->MoveRange; y <= PlayerCharacter->MoveRange; y++)
        {
            int NewCol = StartCol + x;
            int NewRow = StartRow + y;

            if (NewCol < 0 || NewCol >= GridRef->X) continue;
            if (NewRow < 0 || NewRow >= GridRef->Y) continue;

            int Index = NewCol * GridRef->Y + NewRow;
            int Distance = FMath::Abs(x) + FMath::Abs(y);
            bool bCanMove = false;

            // Application du filtre de schéma selon la classe/type de déplacement du perso
            switch (PlayerCharacter->MovePattern)
            {
            case EMovePattern::Orthogonal:
                bCanMove = (Distance <= PlayerCharacter->MoveRange) && (x == 0 || y == 0);
                break;

            case EMovePattern::Diagonal:
                bCanMove = (FMath::Abs(x) == FMath::Abs(y)) && (FMath::Abs(x) <= PlayerCharacter->MoveRange);
                break;

            case EMovePattern::AllDirections:
                bCanMove = (Distance <= PlayerCharacter->MoveRange);
                break;
            }

            // Validation finale des propriétés de la case avant affichage
            if (bCanMove)
            {
                ETileType TileType = GridRef->FindTileType(Index);
                bool bIsEngineer = PlayerCharacter->UnitRowName == FName("Engineer");
                bool bIsCurrentTile = Index == PlayerCharacter->CurrentTileIndex;

                if ((TileType == ETileType::Blocked && !bIsCurrentTile) ||
                    (TileType == ETileType::Hole && !bIsEngineer))
                {
                    continue;
                }

                // Allumage visuel de la case valide détectée
                GridRef->AddStateToTile(ETileState::Reachable, Index);
                ReachableTiles.AddUnique(Index);
            }
        }
    }
}

void UPathfindingAction::ClearReachableTiles()
{
    for (int TileIndex : ReachableTiles)
    {
        GridRef->RemoveStateToTile(ETileState::Reachable, TileIndex);
    }

    ReachableTiles.Empty();
}

