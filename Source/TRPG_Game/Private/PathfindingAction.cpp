#include "PathfindingAction.h"
#include "GridManager.h"
#include "Player_Character.h"
#include "EngineUtils.h"
#include <cstdlib>
#include <algorithm>

#define ECC_Player ECC_GameTraceChannel2

// ============================================================================
// Continuous execution logic (Mouse hover)
// ============================================================================
void UPathfindingAction::ExecuteAction(int Index)
{
    // Removes hover from the previously hovered tile
    if (GridRef->isIndexValid(GridRef->PreviousIndex))
    {
        GridRef->RemoveStateToTile(ETileState::Hovered, GridRef->PreviousIndex);
    }

    // Applies hover to the new tile under the cursor
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
// Left Click Handling: Character selection and path calculation (A*)
// ============================================================================
void UPathfindingAction::LeftClick(int Index)
{
    bool bFoundPlayer = false;

    // ------------------------------------------------------------------------
    // Phase 1: Finding a character on the clicked tile
    // ------------------------------------------------------------------------
    for (TActorIterator<APlayer_Character> It(GetWorld()); It; ++It)
    {
        APlayer_Character* FoundPlayer = *It;
        if (!FoundPlayer) continue;

        int PlayerTile = GridRef->GetIndexFromWorldPosition(FoundPlayer->GetActorLocation());
        if (PlayerTile != Index) continue;

        // Character found!
        bFoundPlayer = true;
        ClearReachableTiles();

        // Handling character selection changes
        if (PlayerCharacter && PlayerCharacter != FoundPlayer)
        {
            PlayerCharacter->UnselectPlayer();
        }

        PlayerCharacter = FoundPlayer;
        PlayerCharacter->SelectPlayer();
        ShowReachableTiles(); // Displays the maximum movement range

        break;
    }

    if (bFoundPlayer) return;

    // ------------------------------------------------------------------------
    // Phase 2: Handling movement conditions (if no player was clicked)
    // ------------------------------------------------------------------------
    if (!PlayerCharacter || !PlayerCharacter->bIsMyTurn || PlayerCharacter->bHasMoved || !PlayerCharacter->bIsSelected)
    {
        return;
    }

    if (!GridRef || !GridRef->isIndexValid(Index)) return;

    // Checking destination validity (must be in the blue/Reachable area)
    if (!GridRef->TileMap[Index].StateArray.Contains(ETileState::Reachable))
    {
        return;
    }

    if (!GridRef->isIndexValid(startIndex.Key)) return;
    if (LastTargetIndex == Index) return;

    LastTargetIndex = Index;

    // ------------------------------------------------------------------------
    // Phase 3: A* pathfinding algorithm
    // ------------------------------------------------------------------------
    InitGrid();

    OPEN.Emplace(startIndex.Key, startIndex.Value);
    target.Key = Index;

    if (target.Key == startIndex.Key) return;

    while (!flag && OPEN.Num() > 0)
    {
        current = *OPEN.begin();

        // Finding the node with the lowest total F cost
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

        // Destination reached, exit the loop
        if (current.Key == target.Key)
        {
            flag = true;
            break;
        }

        NEIGHBOURS = GetAllNeighbours(current.Key, current.Value.g_cost);

        // Analyzing neighboring tiles
        for (TPair<int, FNodeData> neighbourToCheck : NEIGHBOURS)
        {
            int neighbourKey = neighbourToCheck.Key;
            FNodeData neighbourData = neighbourToCheck.Value;

            ETileType TileType = GridRef->FindTileType(neighbourKey);
            bool bIsEngineer = PlayerCharacter->UnitRowName == FName("Engineer");
            bool bIsCurrentTile = neighbourKey == PlayerCharacter->CurrentTileIndex;

            // Filtering map obstacles based on the character profile
            if ((TileType == ETileType::Blocked && !bIsCurrentTile) ||
                (TileType == ETileType::Hole && !bIsEngineer))
            {
                continue;
            }

            if (CLOSED.Contains(neighbourKey)) continue;

            // Updating or adding the node to the OPEN list
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
    // Phase 4: Final path reconstruction and validation
    // ------------------------------------------------------------------------
    FinalPath.Empty();

    if (flag)
    {
        int min = 0;
        int max = GridRef->X * GridRef->Y;

        // Following parent nodes to rebuild the path chain
        while (target.Key != startIndex.Key && min < max)
        {
            if (!CLOSED.Contains(target.Key) || CLOSED[target.Key].parent_index == -1)
                break;

            FinalPath.Add(target.Key);
            target.Key = CLOSED[target.Key].parent_index;
            min++;
        }

        FinalPath.Add(startIndex.Key);
        Algo::Reverse(FinalPath); // Reversing the path into the correct order (Start -> Destination)

        // Safety: Reject movement if the actual path exceeds the character movement stat
        if (FinalPath.Num() - 1 > PlayerCharacter->MoveRange)
        {
            return;
        }

        // Sending the path to the character and clearing the grid visuals
        PlayerCharacter->SetPath(FinalPath);
        ClearReachableTiles();
    }
}

// ============================================================================
// Right Click Handling: Cancel and Reset
// ============================================================================
void UPathfindingAction::RightClick(int Index)
{
    if (!GridRef) return;

    // Visually deselects the targeted tile
    if (GridRef->isIndexValid(Index))
    {
        GridRef->RemoveStateToTile(ETileState::Selected, Index);
    }

    InitGrid();
}

// ============================================================================
// Cleaning and initializing calculation data structures
// ============================================================================
void UPathfindingAction::InitGrid()
{
    // Clears the previous path visualization from the grid
    for (auto& neigh : CLOSED)
    {
        GridRef->RemoveStateToTile(ETileState::Way, neigh.Key);
    }

    OPEN.Empty();
    CLOSED.Empty();
    NEIGHBOURS.Empty();

    flag = false;

    // Setting the A* start point to the player current position
    startIndex.Key = GridRef->GetIndexFromWorldPosition(PlayerCharacter->GetActorLocation());
    startIndex.Value = FNodeData(0, 0, 0, -1);
}

// ============================================================================
// A* Mathematics: Retrieving and evaluating neighbors (2D Grid)
// ============================================================================
TMap<int, FNodeData> UPathfindingAction::GetAllNeighbours(int NodeToCheck, int parentGCost)
{
    TMap<int, FNodeData> NEWNEIGHBOURS;

    int col = NodeToCheck / GridRef->Y;
    int row = NodeToCheck % GridRef->Y;

    // Analyzing the immediate surroundings (3x3 around the node)
    for (int i = -1; i <= 1; i++)
    {
        for (int j = -1; j <= 1; j++)
        {
            if (i == 0 && j == 0) continue; // Ignores the center

            int newCol = col + i;
            int newRow = row + j;

            // Restricts the search to the map boundaries
            if (newCol < 0 || newCol >= GridRef->X) continue;
            if (newRow < 0 || newRow >= GridRef->Y) continue;

            int neighbourIndex = newCol * GridRef->Y + newRow;
            bool isDiagonal = (i != 0 && j != 0);

            // Calculating heuristic costs and adding to the local neighbor list
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
    // G cost: Path cost from the start point (10 straight, 14 diagonal)
    int g = parentGCost + (isDiagonal ? 14 : 10);

    // H cost: Manhattan distance / diagonal heuristic to the target
    int dx_h = abs((nodeIndex / GridRef->Y) - (Target / GridRef->Y));
    int dy_h = abs((nodeIndex % GridRef->Y) - (Target % GridRef->Y));
    int h = 14 * std::min(dx_h, dy_h) + 10 * abs(dx_h - dy_h);

    return FNodeData(g, h, g + h, -1);
}

// ============================================================================
// Visual Management: Displaying the movement range pattern
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

    // Scanning the area defined by the maximum movement range
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

            // Applying the pattern filter based on the character movement class/type
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

            // Final validation of tile properties before display
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

                // Visually highlighting the detected valid tile
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

