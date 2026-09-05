#pragma once

#include "CoreMinimal.h"
#include "GuDefinitionTypes.generated.h"

UENUM(BlueprintType)
enum class EGuKind : uint8
{
    Mortal,
    Immortal
};

UENUM(BlueprintType)
enum class EGuActivationModel : uint8
{
    Instant,
    Maintained,
    PreparedMark,
    Trigger,
    Conversion,
    Infrastructure,
    CultivationModification,
    StoredCharged,
    PassiveCondition
};

UENUM(BlueprintType)
enum class EGuEssenceCostMode : uint8
{
    Absolute,
    PercentOfOwnRankTheoreticalAperture
};

UENUM(BlueprintType)
enum class EGuConsumeOn : uint8
{
    SuccessfulActivation,
    Attempt,
    ManualUse
};

UENUM(BlueprintType)
enum class EGuContainer : uint8
{
    Aperture,
    Storage,
    House,
    World,
    Consumed
,
    Escrow};

UENUM(BlueprintType)
enum class ERefinableKind : uint8
{
    Material,
    Gu,
    Beast,
    Plant,
    CreaturePart,
    Other
};

USTRUCT(BlueprintType)
struct FRefinementSemanticProfile
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    TMap<FName, float> Paths;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    TMap<FName, float> Properties;

    /**
     * True when Properties is a materialized snapshot derived from Paths /
     * Attributes / Traits for inspection and cross-system use. Refinement must
     * not add this map a second time because those source semantics already
     * generate the same physical-property pressure during analysis.
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
    bool bDerivedPropertySnapshot = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    TMap<FName, float> Attributes;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    TMap<FName, float> Traits;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    TMap<FName, float> Templates;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, meta=(ClampMin="0.0"))
    float DaoMass = 0.0f;
};

USTRUCT(BlueprintType)
struct FGuAppearancePalette
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FString Primary = TEXT("#8a7550");
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FString Secondary = TEXT("#c9a24a");
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FString Highlight = TEXT("#f1df9b");
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FString Shadow = TEXT("#251f18");
};

USTRUCT(BlueprintType)
struct FGuAppearanceAnatomy
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) int32 Segments = 4;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) int32 EyeCount = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) int32 LegPairs = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FName WingStyle = TEXT("none");
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) int32 HornCount = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) int32 HoleCount = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) int32 SpikeCount = 0;
};

USTRUCT(BlueprintType)
struct FGuAppearanceSurface
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FName Pattern = TEXT("gradient");
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) float Glow = 0.15f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) float Opacity = 1.0f;
};

USTRUCT(BlueprintType)
struct FGuAppearanceAnimation
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FName Idle = TEXT("float");
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FName Activation = TEXT("pulse");
};

USTRUCT(BlueprintType)
struct FGuAppearanceTransform
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) float Scale = 1.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) float RotationDegrees = 0.0f;
};

USTRUCT(BlueprintType)
struct FGuAppearanceSpec
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) int32 SchemaVersion = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FName Archetype = TEXT("orb");
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FName Silhouette = TEXT("pearl");
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FName Material = TEXT("jade");
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FGuAppearancePalette Palette;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FGuAppearanceAnatomy Anatomy;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FGuAppearanceSurface Surface;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FGuAppearanceAnimation Animation;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FGuAppearanceTransform Transform;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) int32 Seed = 1;
};

USTRUCT(BlueprintType)
struct FGuFeedingSpec
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FName FoodKey = TEXT("food");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, meta=(ClampMin="0.01"))
    float IntervalHours = 24.0f;
};

USTRUCT(BlueprintType)
struct FGuLifecycleSpec
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    bool bConsumable = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    EGuConsumeOn ConsumeOn = EGuConsumeOn::SuccessfulActivation;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, meta=(ClampMin="1"))
    int32 Charges = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FString ConsumedForm = TEXT("The Gu is expended and ceases to exist.");
};

USTRUCT(BlueprintType)
struct FGuIntrinsicConstraints
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, meta=(ClampMin="0"))
    int32 PrepareMs = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    bool bStationary = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    bool bContact = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, meta=(ClampMin="0.0"))
    float ContactRange = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, meta=(ClampMin="0.0", ClampMax="0.95"))
    float SelfCostLifePercent = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    bool bShortLived = false;
};

USTRUCT(BlueprintType)
struct FGuPowerProfile
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    float BaseBudget = -1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    float EffectiveBudget = -1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, meta=(ClampMin="1.0", ClampMax="3.0"))
    float ConstraintMultiplier = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    TMap<FName, float> Allocation;
};

USTRUCT(BlueprintType)
struct FGuRefinementAssistanceSpec
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    bool bEnabled = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    float ProgressPercent = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    float StabilityPerAction = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    float ImpurityReductionPerAction = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    float QualityBonus = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, meta=(ClampMin="1"))
    int32 ActionUses = 3;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    TArray<FName> Processes;
};

USTRUCT(BlueprintType)
struct FGuEffectProfile
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FString CoreEffect;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FString Input = TEXT("Primeval essence");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FString Carrier;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FString Operation;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FString TargetLink;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FString Manifestation;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    float Magnitude = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    float Range = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    float Area = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    int32 DurationMs = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FString OtherCost;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    TArray<FName> ValidTargets;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    int32 RankLimit = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FString Setup;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FString Environment;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FString Visibility;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FString Trace;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FString Failure;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FString Counterplay;
};

USTRUCT(BlueprintType)
struct FGuMechanicSpec
{
    GENERATED_BODY()

    // Stable mechanic identifier such as projectile, shield, refinement_assistance,
    // multitasking_boost, consumable_relic, or enslavement.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FName Type;

    // Kept as JSON at this boundary so player-created/refinement-created mechanics
    // can round-trip without inventing UObject classes at runtime. Systems compile
    // this into strongly typed execution data when the definition is registered.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FString ConfigJson = TEXT("{}");
};

USTRUCT(BlueprintType)
struct FGuKillerMoveSpec
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FName Template = TEXT("auto");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    bool bCoreCapable = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    float Power = -1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    TArray<FName> Attributes;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FString LinkMechanism;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    TArray<FName> SuitableRoles;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    TArray<FName> Contributes;
};

USTRUCT(BlueprintType)
struct FGuRefinementOrigin
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    int32 ProfileVersion = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FName Foundation;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FString SourceSignature;

    // JSON preserves the browser recipe ingredient payload until the dedicated
    // C++ recipe format is fully migrated.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FString IngredientsJson = TEXT("[]");
};

USTRUCT(BlueprintType)
struct FGuDefinitionRecord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FName Id;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FString Name;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, meta=(ClampMin="1", ClampMax="9"))
    int32 Rank = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    EGuKind Kind = EGuKind::Mortal;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FName Path;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    TArray<FName> SecondaryPaths;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FString PathRelation = TEXT("Pure-path Gu");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FName Category = TEXT("Utility");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FString Description = TEXT("A mysterious Gu worm.");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    TArray<FName> FunctionalRoles;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    EGuActivationModel ActivationModel = EGuActivationModel::Instant;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    EGuEssenceCostMode EssenceCostMode = EGuEssenceCostMode::Absolute;

    // Negative means "resolve from the mechanic/default balance".
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    float EssenceCost = -1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    bool bUnique = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FGuFeedingSpec Feeding;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FGuLifecycleSpec Lifecycle;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    TArray<FName> RefinementTraits;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FGuIntrinsicConstraints IntrinsicConstraints;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FGuPowerProfile PowerProfile;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FRefinementSemanticProfile RefinementProfile;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FGuRefinementAssistanceSpec RefinementAssistance;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FGuEffectProfile EffectProfile;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    TArray<FGuMechanicSpec> Mechanics;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FGuAppearanceSpec Appearance;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    TArray<FName> Tags;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FString Source;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    TArray<FName> KnownSynergies;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FGuRefinementOrigin RefinementOrigin;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    bool bHasRefinementOrigin = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FGuKillerMoveSpec KillerMove;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    bool bCustom = false;
};
