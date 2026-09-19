#include "Units.h"
#include "UnitInterface.h"
#include "Engine/DataTable.h"
#include "Kismet/GameplayStatics.h"
#include "Blueprint/UserWidget.h"
#include "TurnManager.h"
#include "Components/WidgetComponent.h"
#include "GridManager.h"
#include "UnitDataAsset.h"

// ============================================================================
// Constructeur : Configuration initiale de l'unité
// ============================================================================
AUnits::AUnits()
{
	PrimaryActorTick.bCanEverTick = true;
}

// ============================================================================
// Initialisation du jeu : Liaison avec les managers globaux
// ============================================================================
void AUnits::BeginPlay()
{
	Super::BeginPlay();

	// Récupération de la référence du Grid Manager présent sur la map
	GridManagerRef = Cast<AGridManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AGridManager::StaticClass()));
}

// ============================================================================
// Phase de Construction : Chargement des statistiques via Data Table
// ============================================================================
void AUnits::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (!UnitDataTable)
	{
		UE_LOG(LogTemp, Error, TEXT("NO UNIT DATA TABLE"));
		return;
	}

	// Recherche de la ligne correspondant au type d'unité (ex: Guerrier, Mage...)
	FSUnit* Row = UnitDataTable->FindRow<FSUnit>(UnitRowName, TEXT(""));

	if (!Row)
	{
		UE_LOG(LogTemp, Error, TEXT("ROW NOT FOUND"));
		return;
	}

	if (!GetMesh()) return;

	// Attribution des stats de la Data Table aux variables de l'instance
	CurrentHealth = Row->Health;
	MaxHealth = Row->MaxHealth;
	Damage = Row->Damage;
	MoveRange = Row->MoveRange;
	AttackRange = Row->AttackRange;
	MoveSpeed = Row->MoveSpeed;
	MovePattern = Row->MovePattern;
	AttackPattern = Row->AttackPattern;
	SkillType = Row->SkillType;
	BombClass = Row->BombClass;
	XPReward = Row->XPReward;
	Level = Row->Level;
	Name = Row->Name;
}

void AUnits::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

// ============================================================================
// Gestion des déplacements et de la navigation
// ============================================================================
void AUnits::SetPath(const TArray<int>& NewPath)
{
	CurrentPath = NewPath;
	CurrentPathIndex = 0;
	bIsMoving = true;

	// Déclenchement de l'animation de marche si ce n'est pas un dash rapide
	if (!bIsDash)
	{
		AnimState = EUnitAnimState::WALK;
	}
}

void AUnits::MoveAlongPath(float DeltaTime)
{
	// Logique de déplacement interpolé le long des cases (implémentée en Blueprint ou à compléter)
}

// ============================================================================
// Système de combat : Encaissement des dégâts et cycle de vie
// ============================================================================
void AUnits::ReceiveDamage(float Amount)
{
	// Affichage du texte flottant et réaction visuelle
	ShowDamageText(Amount);
	AnimState = EUnitAnimState::HIT;

	CurrentHealth -= Amount;

	// Notification pour mettre à jour la barre de vie (UI)
	OnHealthChanged.Broadcast(CurrentHealth / MaxHealth);

	if (CurrentHealth <= 0.f)
	{
		Die();
		return;
	}

	// Configuration du retour à l'état Idle après l'animation d'impact
	FTimerHandle HitTimer;
	GetWorld()->GetTimerManager().SetTimer(
		HitTimer,
		[this]()
		{
			if (AnimState != EUnitAnimState::DEATH)
			{
				AnimState = EUnitAnimState::IDLE;
			}
		},
		1.5f,
		false
	);
}

void AUnits::Die()
{
	OnUnitDied.Broadcast(this);

	// Libération de la case sur la grille pour la rendre à nouveau traversable
	int TileIndex = GridManagerRef->GetIndexFromWorldPosition(GetActorLocation());
	GridManagerRef->AddTypeToTile(ETileType::Normal, TileIndex);

	AnimState = EUnitAnimState::DEATH;

	// Désactivation physique et programmation de la destruction de l'Actor
	SetActorEnableCollision(false);
	SetLifeSpan(2.f);
}

// ============================================================================
// Progression : Gestion de l'expérience (XP) et Gain de niveau
// ============================================================================
void AUnits::GainXP(int Amount)
{
	CurrentXP += Amount;

	UE_LOG(LogTemp, Warning, TEXT("%s XP : %d"), *GetName(), CurrentXP);

	if (CurrentXP >= XPToNextLevel)
	{
		LevelUp();
	}
}

void AUnits::LevelUp()
{
	CurrentXP -= XPToNextLevel;
	Level++;

	// Palier dynamique de niveau et augmentation des statistiques globales
	XPToNextLevel = Level * 100;
	MaxHealth += 20.f;
	CurrentHealth = MaxHealth; // Soigne l'unité au passage de niveau
	Damage += 5.f;

	UE_LOG(LogTemp, Warning, TEXT("%s LEVEL UP : %d"), *GetName(), Level);

	// Actualisation des informations affichées sur l'interface de combat
	ATurnManager* TurnManager = Cast<ATurnManager>(UGameplayStatics::GetActorOfClass(GetWorld(), ATurnManager::StaticClass()));

	if (TurnManager && TurnManager->CurrentUnit == this)
	{
		TurnManager->RefreshHUD();
	}
}

// ============================================================================
// Interface Utilisateur : Affichage des popups de dégâts (Screen Space)
// ============================================================================
void AUnits::ShowDamageText(float DamageAmount)
{
	if (!DamageWidgetClass) return;

	UUserWidget* Widget = CreateWidget(GetWorld(), DamageWidgetClass);
	if (!Widget) return;

	Widget->AddToViewport();

	// Envoi de la valeur numérique à la fonction de mise à jour du Widget Blueprint
	UFunction* Function = Widget->FindFunction(FName("SetDamage"));
	if (Function)
	{
		struct FDamageParams
		{
			FText Damage;
		};

		FDamageParams Params;
		FString DamageString = FString::Printf(TEXT("-%d"), (int)DamageAmount);
		Params.Damage = FText::FromString(DamageString);

		Widget->ProcessEvent(Function, &Params);
	}

	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC) return;

	// Projection de la position 3D de la tête de l'unité vers des coordonnées écran 2D
	FVector2D ScreenPosition;
	UGameplayStatics::ProjectWorldToScreen(
		PC,
		GetActorLocation() + FVector(0, 0, 150),
		ScreenPosition
	);

	Widget->SetPositionInViewport(ScreenPosition);
}

// ============================================================================
// Portée d'action : Calcul des distances selon le schéma d'attaque
// ============================================================================
bool AUnits::IsTargetInAttackRange(AUnits* Target)
{
	if (!Target) return false;

	// Extraction des coordonnées matricielles (Colonnes / Lignes)
	int MyIndex = GridManagerRef->GetIndexFromWorldPosition(GetActorLocation());
	int TargetIndex = GridManagerRef->GetIndexFromWorldPosition(Target->GetActorLocation());

	int MyCol = MyIndex / GridManagerRef->Y;
	int MyRow = MyIndex % GridManagerRef->Y;

	int TargetCol = TargetIndex / GridManagerRef->Y;
	int TargetRow = TargetIndex % GridManagerRef->Y;

	// Calcul des distances absolues sur les axes X et Y (Distance de Manhattan)
	int DX = FMath::Abs(MyCol - TargetCol);
	int DY = FMath::Abs(MyRow - TargetRow);

	// Filtrage géométrique selon la forme géométrique du pattern d'attaque
	switch (AttackPattern)
	{
	case EAttackPattern::Cross:
		return ((DX == 0 || DY == 0) && (DX + DY <= AttackRange));

	case EAttackPattern::Circle:
		return (DX + DY <= AttackRange);

	case EAttackPattern::Line:
		return (DX == DY && DX <= AttackRange);
	}

	return false;
}
