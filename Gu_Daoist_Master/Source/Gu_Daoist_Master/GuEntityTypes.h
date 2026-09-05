#pragma once

#include "CoreMinimal.h"
#include "GuDefinitionTypes.h"
#include "GuEntityTypes.generated.h"

USTRUCT(BlueprintType)
struct FRefinableEntityComponent
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    ERefinableKind Kind = ERefinableKind::Material;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FName SourceId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FName DefinitionId;
};

USTRUCT(BlueprintType)
struct FMaterialLotComponent
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FString LotId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FName Item;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, meta=(ClampMin="0"))
    int32 Quantity = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FGuid SourceEntityId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FName SourceKind;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    int64 CreatedAtUnixMs = 0;
};

USTRUCT(BlueprintType)
struct FDaoMarkProfileComponent
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    TMap<FName, float> Paths;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    float DaoMass = 0.0f;
};

USTRUCT(BlueprintType)
struct FRefinementPropertiesComponent
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    TMap<FName, float> Scores;
};

USTRUCT(BlueprintType)
struct FRefinementAttributesComponent
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    TMap<FName, float> Scores;
};

USTRUCT(BlueprintType)
struct FRefinementTraitsComponent
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    TMap<FName, float> Scores;
};

USTRUCT(BlueprintType)
struct FRefinementTemplatesComponent
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    TMap<FName, float> Scores;
};

USTRUCT(BlueprintType)
struct FDaoContaminationComponent
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    TMap<FName, float> Paths;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    TMap<FName, float> Attributes;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    TMap<FName, float> Traits;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    float Total = 0.0f;
};

USTRUCT(BlueprintType)
struct FGuInstanceComponent
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FName DefinitionId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    int64 CreatedAtUnixMs = 0;
};

USTRUCT(BlueprintType)
struct FGuConditionComponent
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    bool bAlive = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    float Quality = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    float Durability = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    int32 ActivationCount = 0;

    // Free-form per-instance state remains JSON during the migration. Once a
    // mechanic earns a stable runtime representation, move it into its own ECS component.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FString CustomStateJson = TEXT("{}");
};

USTRUCT(BlueprintType)
struct FGuVisualStateComponent
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FName Animation = TEXT("breathe");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    int64 ActivationUntilUnixMs = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FName Condition = TEXT("healthy");
};

USTRUCT(BlueprintType)
struct FGuNourishmentComponent
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    float Hunger = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    int64 LastUpdateUnixMs = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    int64 ZeroSinceUnixMs = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FName FoodKey = TEXT("food");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    float IntervalHours = 24.0f;
};

USTRUCT(BlueprintType)
struct FOwnedByComponent
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FString OwnerId = TEXT("player");
};

USTRUCT(BlueprintType)
struct FGuStatusComponent
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    TArray<FName> States;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FString HolderId = TEXT("player");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FName Visibility = TEXT("Secret");
};

USTRUCT(BlueprintType)
struct FGuLifecycleComponent
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    bool bConsumable = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    EGuConsumeOn ConsumeOn = EGuConsumeOn::SuccessfulActivation;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    int32 MaxCharges = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FString ConsumedForm;
};

USTRUCT(BlueprintType)
struct FGuChargesComponent
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    int32 Remaining = 1;
};

UENUM(BlueprintType)
enum class EGuWillState : uint8
{
    Wild,
    Captured,
    Refining,
    Refined
};

/** Physical custody and spiritual ownership are deliberately separate. */
USTRUCT(BlueprintType)
struct FGuWillComponent
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    EGuWillState State = EGuWillState::Refined;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FString CaptorId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FString MasterId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, meta=(ClampMin="0.0", ClampMax="100.0"))
    float RefinementProgress = 100.0f;

    /** Reserved for rank/species/will-strength tuning. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, meta=(ClampMin="0.0"))
    float Resistance = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    int64 CapturedAtUnixMs = 0;
};

USTRUCT(BlueprintType)
struct FGuPlacementComponent
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    EGuContainer Container = EGuContainer::Aperture;
};

USTRUCT(BlueprintType)
struct FGuEnslavementControllerComponent
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    TArray<FGuid> BoundBeastIds;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FName CommandMode = TEXT("direct");
};

USTRUCT(BlueprintType)
struct FMultitaskingBoostComponent
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    int32 SlotsGranted = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FString OwnerId = TEXT("player");
};

USTRUCT(BlueprintType)
struct FRefinementAssistantComponent
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    float ProgressPercent = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    float StabilityPerAction = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    float ImpurityReductionPerAction = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    float QualityBonus = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    int32 ActionUses = 3;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    TArray<FName> Processes;
};

USTRUCT(BlueprintType)
struct FRefinementSemanticSnapshot
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FGuid EntityId;

    UPROPERTY(BlueprintReadOnly)
    ERefinableKind Kind = ERefinableKind::Other;

    UPROPERTY(BlueprintReadOnly)
    FName SourceId;

    UPROPERTY(BlueprintReadOnly)
    FName DefinitionId;

    UPROPERTY(BlueprintReadOnly)
    FRefinementSemanticProfile Semantic;

    UPROPERTY(BlueprintReadOnly)
    FDaoContaminationComponent Contamination;
};

USTRUCT(BlueprintType)
struct FGuEntitySnapshot
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FGuid EntityId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    bool bHasRefinable = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FRefinableEntityComponent Refinable;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    bool bHasMaterialLot = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FMaterialLotComponent MaterialLot;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    bool bHasDaoMarks = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FDaoMarkProfileComponent DaoMarks;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FRefinementPropertiesComponent RefinementProperties;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FRefinementAttributesComponent RefinementAttributes;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FRefinementTraitsComponent RefinementTraits;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FRefinementTemplatesComponent RefinementTemplates;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    bool bHasContamination = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FDaoContaminationComponent Contamination;

    /** Entity-wide ownership/placement flags added in physical-domain save v2. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    bool bHasOwner = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    bool bHasPlacement = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    bool bHasGuInstance = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FGuInstanceComponent GuInstance;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FGuConditionComponent GuCondition;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FGuVisualStateComponent GuVisualState;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FGuNourishmentComponent GuNourishment;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FOwnedByComponent OwnedBy;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FGuStatusComponent GuStatus;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FGuLifecycleComponent GuLifecycle;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FGuChargesComponent GuCharges;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FGuPlacementComponent GuPlacement;

    /** Explicit Gu-will state added in physical-domain save v3. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    bool bHasGuWill = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FGuWillComponent GuWill;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    bool bHasEnslavementController = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FGuEnslavementControllerComponent EnslavementController;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    bool bHasMultitaskingBoost = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FMultitaskingBoostComponent MultitaskingBoost;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    bool bHasRefinementAssistant = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FRefinementAssistantComponent RefinementAssistant;
};
