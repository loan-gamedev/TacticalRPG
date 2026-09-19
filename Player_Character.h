// Player_Character.h

#pragma once

// =========================
// Includes
// =========================

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Units.h"
#include "GridManager.h"
#include "Player_Character.generated.h"

// =========================
// Etats d'animation utilisés
// par le personnage
// =========================

UENUM(BlueprintType)
enum class EAnimationState : uint8
{
	IDLE,
	WALK,
	ATTACK
};

// =========================
// Classe du joueur contrôlée
// par le système de tours
// =========================

UCLASS()
class TRPG_GAME_API APlayer_Character : public AUnits
{
	GENERATED_BODY()

public:

	APlayer_Character();

protected:

	// =========================
	// Initialisation
	// =========================

	virtual void BeginPlay() override;

public:

	// =========================
	// Tick
	// =========================

	virtual void Tick(float DeltaTime) override;

	// =========================
	// Interaction
	// =========================

	// Appelé lors d'un clic sur l'unité
	UFUNCTION()
	void OnPlayerClicked(
		UPrimitiveComponent* TouchedComponent,
		FKey ButtonPressed
	);

	// =========================
	// Interface
	// =========================

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<UUserWidget> StatsWidgetClass;

	UPROPERTY()
	UUserWidget* StatsWidget;

	// =========================
	// Sélection
	// =========================

	UPROPERTY(BlueprintReadOnly)
	bool bIsSelected = false;

	// =========================
	// Déplacement
	// =========================

	// Déplace l'unité le long du chemin calculé
	void MoveAlongPath(float DeltaTime);

	// =========================
	// Fonctions
	// =========================

	void SelectPlayer();

	void UnselectPlayer();

	// Sauvegarde le type réel de la case
	// avant qu'elle soit occupée
	UPROPERTY()
	ETileType PreviousTileType;

	UFUNCTION()
	void ResetToIdle();

	// =========================
	// Variables de déplacement
	// =========================

	FTimerHandle AttackTimer;

	int CurrentTileIndex = -1;

	float BaseMoveSpeed;

	// Déplacement généré par une compétence
	bool bSkillMovement = false;

	// Empêche certaines actions pendant une animation
	bool bLockActions = false;
};