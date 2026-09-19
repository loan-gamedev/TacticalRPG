#pragma once

// =========================
// Includes
// =========================

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "UnitStruct.h"
#include "UnitInterface.h"
#include "Units.generated.h"

// =========================
// Delegates
// =========================

// Appelé lorsqu'une unité perd ou gagne de la vie
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FOnHealthChanged,
    float,
    NewHealthPercent
);

// Appelé lorsqu'une unité meurt
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FOnUnitDied,
    class AUnits*,
    DeadUnit
);

class AGridManager;

// =========================
// Classe de base de toutes
// les unités du jeu.
// (joueurs et ennemis)
// =========================

UCLASS()
class TRPG_GAME_API AUnits : public ACharacter, public IUnitInterface
{
    GENERATED_BODY()

public:

    AUnits();

    virtual void Tick(float DeltaTime) override;

    // =========================
    // Références
    // =========================

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    AGridManager* GridManagerRef;

    // =========================
    // Déplacement / Combat
    // =========================

    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    EMovePattern MovePattern;

    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    EAttackPattern AttackPattern;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
    EUnitAnimState AnimState;

    UFUNCTION()
    virtual void MoveAlongPath(
        float DeltaTime
    );

    UFUNCTION()
    virtual void SetPath(
        const TArray<int>& NewPath
    );

    UPROPERTY()
    TArray<int> CurrentPath;

    UPROPERTY()
    int CurrentPathIndex = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsMoving = false;

    UFUNCTION()
    bool IsTargetInAttackRange(
        AUnits* Target
    );

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float AcceptanceDistance = 5.f;

    // =========================
    // Statistiques
    // =========================

    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    float CurrentHealth = 0.f;

    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    float MaxHealth = 0.f;

    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    float Damage = 0.f;

    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    int32 MoveRange = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    int32 AttackRange = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    float MoveSpeed = 0.f;

    // =========================
    // Combat
    // =========================

    UFUNCTION(BlueprintCallable, Category = "Combat")
    virtual void ReceiveDamage(float Amount);

    UFUNCTION(BlueprintCallable, Category = "Combat")
    virtual void Die();

    FTimerHandle TimerHandle;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float AttackTime = 2.f;

    // Dégâts appliqués par les cases en feu
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float DamageFire = 10.f;

    // =========================
    // Gestion des tours
    // =========================

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turn")
    bool bSkipTurn = false;

    UPROPERTY(BlueprintReadOnly)
    bool bIsMyTurn = false;

    UPROPERTY(BlueprintReadOnly)
    bool bHasMoved = false;

    UPROPERTY(BlueprintReadOnly)
    bool bHasAttacked = false;

    bool bHasUsedSkill = false;

    // =========================
    // Compétences
    // =========================

    UPROPERTY(BlueprintReadOnly)
    ESkillType SkillType;

    UPROPERTY(BlueprintReadOnly)
    TSubclassOf<AActor> BombClass;

    bool bIsDash = false;

    // =========================
    // XP / Progression
    // =========================

    UPROPERTY(BlueprintReadOnly)
    int CurrentXP = 0;

    UPROPERTY(BlueprintReadOnly)
    int XPToNextLevel = 100;

    UPROPERTY(BlueprintReadOnly)
    int Level = 1;

    UPROPERTY(BlueprintReadOnly)
    int XPReward;

    UFUNCTION()
    void GainXP(int Amount);

    UFUNCTION()
    void LevelUp();

    // =========================
    // Interface utilisateur
    // =========================

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
    UTexture2D* PortraitTexture;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TSubclassOf<UUserWidget> DamageWidgetClass;

    UFUNCTION()
    void ShowDamageText(float DamageAmount);

    // =========================
    // Data Driven
    // =========================

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data")
    TObjectPtr<UDataTable> UnitDataTable;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data")
    FName UnitRowName;

    // =========================
    // Events
    // =========================

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnHealthChanged OnHealthChanged;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnUnitDied OnUnitDied;

    // =========================
    // Divers
    // =========================

    UPROPERTY(BlueprintReadOnly)
    int32 CurrentTileIndex = 0;

    UPROPERTY(BlueprintReadOnly)
    FString Name;

protected:

    // =========================
    // Initialisation
    // =========================

    virtual void BeginPlay() override;

    virtual void OnConstruction(
        const FTransform& Transform
    ) override;
};