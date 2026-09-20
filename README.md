# Tactical RPG - Unreal Engine 5 Prototype

Grid-based tactical RPG prototype in C++/UE5. Academic project at HEAJ (Belgium).

## Features

- Custom A* pathfinding (diagonal cost, terrain-aware obstacles)
- Turn-based combat with movement/attack patterns (orthogonal, diagonal, cross, line...)
- Data-driven units via `UDataTable` (stats, no code changes to add a unit)
- Fire tiles (environmental hazard, damage over time)
- XP / leveling system

## Architecture

- **GridManager** — grid data, tile states/types, input routing
- **PathfindingAction** — A* pathfinding, click handling
- **Units** — base unit class (stats, combat, XP)
- **Player_Character** — player-controlled unit (movement, selection, HUD)

## Author

**Loan Dzelili** — [loan-gamedev.github.io](https://loan-gamedev.github.io)
