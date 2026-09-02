#pragma once

#include "CoreMinimal.h"
#include "RefinementTypes.generated.h"

UENUM(BlueprintType)
enum class ERefinementVerb : uint8
{
    Process,
    Heat,
    Cool,
    Merge,
    Purify,
    Control,
    Condense
};

UENUM(BlueprintType)
enum class ERefinementCondition : uint8
{
    Stable,
    Harmonious,
    Strained,
    Critical,
    Complete,
    Collapsed
};

UENUM(BlueprintType)
enum class ERefinementOutcomeKind : uint8
{
    None,
    Failure,
    Intended,
    Divergent,
    Experimental,
    DiscoveredCanonical
};

UENUM(BlueprintType)
enum class ERefinementKnowledgeState : uint8
{
    Observed,
    Suspected,
    Known
};

USTRUCT(BlueprintType)
struct FRefinementDirection
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FName PrimaryPath;

    UPROPERTY(BlueprintReadOnly)
    float PathClarity = 0.0f;

    UPROPERTY(BlueprintReadOnly)
    float PropertyAlignment = 0.0f;

    UPROPERTY(BlueprintReadOnly)
    float Coherence = 0.0f;

    UPROPERTY(BlueprintReadOnly)
    TMap<FName, float> CombinedPathScores;

    UPROPERTY(BlueprintReadOnly)
    TArray<FName> TopProperties;
};

USTRUCT(BlueprintType)
struct FRefinementRetentionResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    float RawMass = 0.0f;
    UPROPERTY(BlueprintReadOnly)
    float RetainedMass = 0.0f;
    UPROPERTY(BlueprintReadOnly)
    float DiscardedMass = 0.0f;
    UPROPERTY(BlueprintReadOnly)
    float StructuralRetention = 0.0f;
    UPROPERTY(BlueprintReadOnly)
    float PathRetention = 0.0f;
    UPROPERTY(BlueprintReadOnly)
    float AttributeRetention = 0.0f;
    UPROPERTY(BlueprintReadOnly)
    float PropertyAlignment = 0.0f;
    UPROPERTY(BlueprintReadOnly)
    float Fidelity = 1.0f;
};

USTRUCT(BlueprintType)
struct FRefinementProcessHealth
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Fidelity = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MaximumImpurities = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MaxStability = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float LowestStability = 100.0f;
};

USTRUCT(BlueprintType)
struct FRefinementAnalysis
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    int32 ProfileVersion = 3;

    UPROPERTY(BlueprintReadOnly)
    TMap<FName, float> PathScores;
    UPROPERTY(BlueprintReadOnly)
    TMap<FName, float> PropertyScores;
    UPROPERTY(BlueprintReadOnly)
    TMap<FName, float> AttributeScores;
    UPROPERTY(BlueprintReadOnly)
    TMap<FName, float> TraitScores;
    UPROPERTY(BlueprintReadOnly)
    TMap<FName, float> TemplateScores;

    UPROPERTY(BlueprintReadOnly)
    TMap<FName, float> ContaminationPaths;
    UPROPERTY(BlueprintReadOnly)
    TMap<FName, float> ContaminationAttributes;
    UPROPERTY(BlueprintReadOnly)
    TMap<FName, float> ContaminationTraits;

    UPROPERTY(BlueprintReadOnly)
    float NativeDaoMass = 0.0f;

    UPROPERTY(BlueprintReadOnly)
    int32 HighestInputGuRank = 1;

    UPROPERTY(BlueprintReadOnly)
    FName FoundationDefinitionId;

    UPROPERTY(BlueprintReadOnly)
    FRefinementDirection NascentDirection;

    UPROPERTY(BlueprintReadOnly)
    FName PrimaryPath;

    UPROPERTY(BlueprintReadOnly)
    TArray<FName> SecondaryPaths;

    UPROPERTY(BlueprintReadOnly)
    TArray<FName> SurvivingAttributes;

    UPROPERTY(BlueprintReadOnly)
    TArray<FName> SurvivingTraits;

    UPROPERTY(BlueprintReadOnly)
    FName Template;

    UPROPERTY(BlueprintReadOnly)
    float PathCoherence = 0.0f;

    UPROPERTY(BlueprintReadOnly)
    FRefinementRetentionResult DaoMass;

    UPROPERTY(BlueprintReadOnly)
    int32 ResultRank = 1;
};

/** Hidden generated procedure stage. This is server/debug data, never player guidance. */
USTRUCT(BlueprintType)
struct FRefinementProcedureStep
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FString Name;

    UPROPERTY(BlueprintReadOnly)
    ERefinementVerb PrimaryProcess = ERefinementVerb::Process;

    UPROPERTY(BlueprintReadOnly)
    float RequiredProgress = 40.0f;

    UPROPERTY(BlueprintReadOnly)
    FVector2D TargetTemperature = FVector2D(20.0, 60.0);

    UPROPERTY(BlueprintReadOnly)
    TMap<FName, float> ProcessWeights;

    UPROPERTY(BlueprintReadOnly)
    float MaxImpurities = 30.0f;

    UPROPERTY(BlueprintReadOnly)
    FName TargetPath;

    UPROPERTY(BlueprintReadOnly)
    bool bRecoveryWindow = false;

    UPROPERTY(BlueprintReadOnly)
    FString SeedTag;
};

USTRUCT(BlueprintType)
struct FRefinementObservation
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    float Score = 0.0f;

    UPROPERTY(BlueprintReadOnly)
    int64 AtUnixMs = 0;

    UPROPERTY(BlueprintReadOnly)
    ERefinementVerb Verb = ERefinementVerb::Process;
};

USTRUCT(BlueprintType)
struct FRefinementActionRecord
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    ERefinementVerb Verb = ERefinementVerb::Process;

    UPROPERTY(BlueprintReadOnly)
    float Affinity = 0.0f;

    UPROPERTY(BlueprintReadOnly)
    bool bAccepted = false;

    UPROPERTY(BlueprintReadOnly)
    FName TechniquePath;

    UPROPERTY(BlueprintReadOnly)
    float StabilityAfter = 0.0f;

    UPROPERTY(BlueprintReadOnly)
    float ImpuritiesAfter = 0.0f;

    UPROPERTY(BlueprintReadOnly)
    float TemperatureAfter = 20.0f;

    UPROPERTY(BlueprintReadOnly)
    float FocusAfter = 0.0f;
};

USTRUCT(BlueprintType)
struct FRefinementOutcome
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    ERefinementOutcomeKind Kind = ERefinementOutcomeKind::None;

    UPROPERTY(BlueprintReadOnly)
    FName ResultDefinitionId;

    /** Physical Gu instance created when a successful refinement is secured. */
    UPROPERTY(BlueprintReadOnly)
    FGuid ResultEntityId;

    UPROPERTY(BlueprintReadOnly)
    FName ResultPath;

    UPROPERTY(BlueprintReadOnly)
    int32 ResultRank = 1;

    UPROPERTY(BlueprintReadOnly)
    FString Reason;

    UPROPERTY(BlueprintReadOnly)
    FString Message;

    UPROPERTY(BlueprintReadOnly)
    float Fidelity = 0.0f;

    /** Debug-only diagnostic. It is deterministic and is never rolled. */
    UPROPERTY(BlueprintReadOnly)
    float ReliabilityEstimate = 0.0f;

    UPROPERTY(BlueprintReadOnly)
    float ContaminationConflict = 0.0f;

    UPROPERTY(BlueprintReadOnly)
    float ContaminationTolerance = 0.0f;

    UPROPERTY(BlueprintReadOnly)
    FRefinementAnalysis Analysis;
};

/**
 * Player-safe replicated view. Exact procedure, path solution, temperature,
 * impurities, stability and semantic scores intentionally do not exist here.
 */
USTRUCT(BlueprintType)
struct FRefinementPublicState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    bool bActive = false;

    UPROPERTY(BlueprintReadOnly)
    bool bFinished = false;

    UPROPERTY(BlueprintReadOnly)
    bool bSucceeded = false;

    UPROPERTY(BlueprintReadOnly)
    int32 PhaseNumber = 0;

    UPROPERTY(BlueprintReadOnly)
    int32 PhaseCount = 0;

    UPROPERTY(BlueprintReadOnly)
    float Focus = 0.0f;

    UPROPERTY(BlueprintReadOnly)
    float MaxFocus = 100.0f;

    UPROPERTY(BlueprintReadOnly)
    FString Form = TEXT("Unformed");

    UPROPERTY(BlueprintReadOnly)
    FString Response = TEXT("Unreadable");

    UPROPERTY(BlueprintReadOnly)
    FString Condition = TEXT("Stable");

    UPROPERTY(BlueprintReadOnly)
    FString LastObservation;

    UPROPERTY(BlueprintReadOnly)
    FString ResultText;

    /** Source names and remaining prepared uses only. No hidden method diagnostics. */
    UPROPERTY(BlueprintReadOnly)
    TArray<FString> ActiveAssistance;
};

/** Quantity-aware selection of a physical Gu/material entity for one refinement. */
USTRUCT(BlueprintType)
struct FRefinementInputSelection
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FGuid EntityId;

    /** Gu instances must use 1. Material lots may contribute any available quantity. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="1"))
    int32 Quantity = 1;
};

/** Immutable ingredient identity captured when the physical inputs are committed. */
USTRUCT(BlueprintType)
struct FRefinementCommittedInput
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FGuid EntityId;

    UPROPERTY(BlueprintReadOnly)
    uint8 Kind = 0;

    UPROPERTY(BlueprintReadOnly)
    FName SourceId;

    UPROPERTY(BlueprintReadOnly)
    FName DefinitionId;

    UPROPERTY(BlueprintReadOnly)
    int32 Quantity = 1;

    UPROPERTY(BlueprintReadOnly)
    bool bFoundation = false;
};

USTRUCT(BlueprintType)
struct FRefinementAssistanceContribution
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FGuid SourceEntityId;

    UPROPERTY(BlueprintReadOnly)
    FName SourceDefinitionId;

    UPROPERTY(BlueprintReadOnly)
    FString Label;

    UPROPERTY(BlueprintReadOnly)
    FName Path = TEXT("Refinement");

    UPROPERTY(BlueprintReadOnly)
    int32 Rank = 1;

    UPROPERTY(BlueprintReadOnly)
    float ProgressPercent = 0.0f;

    UPROPERTY(BlueprintReadOnly)
    float StabilityPerAction = 0.0f;

    UPROPERTY(BlueprintReadOnly)
    float ImpurityReductionPerAction = 0.0f;

    UPROPERTY(BlueprintReadOnly)
    float QualityBonus = 0.0f;

    UPROPERTY(BlueprintReadOnly)
    int32 UsesRemaining = 0;

    UPROPERTY(BlueprintReadOnly)
    int32 MaximumUses = 0;

    UPROPERTY(BlueprintReadOnly)
    TArray<FName> Processes;
};

USTRUCT(BlueprintType)
struct FRefinementSessionState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FGuid SessionId;

    UPROPERTY(BlueprintReadOnly)
    FString OwnerId;

    UPROPERTY(BlueprintReadOnly)
    TArray<FGuid> InputEntityIds;

    UPROPERTY(BlueprintReadOnly)
    TArray<FRefinementInputSelection> InputSelections;

    /** Snapshot used for notebook/persistence after the consumed entities are destroyed. */
    UPROPERTY(BlueprintReadOnly)
    TArray<FRefinementCommittedInput> CommittedInputs;

    UPROPERTY(BlueprintReadOnly)
    FName IntendedDefinitionId;

    UPROPERTY(BlueprintReadOnly)
    FName IntendedPath;

    UPROPERTY(BlueprintReadOnly)
    int32 AttemptRank = 1;

    UPROPERTY(BlueprintReadOnly)
    bool bKnownRecipe = false;

    UPROPERTY(BlueprintReadOnly)
    FRefinementAnalysis InitialAnalysis;

    UPROPERTY(BlueprintReadOnly)
    TArray<FRefinementProcedureStep> HiddenProcedure;

    UPROPERTY(BlueprintReadOnly)
    int32 StepIndex = 0;

    UPROPERTY(BlueprintReadOnly)
    float StepProgress = 0.0f;

    UPROPERTY(BlueprintReadOnly)
    float Stability = 100.0f;

    UPROPERTY(BlueprintReadOnly)
    float MaxStability = 100.0f;

    UPROPERTY(BlueprintReadOnly)
    float LowestStability = 100.0f;

    UPROPERTY(BlueprintReadOnly)
    float Focus = 100.0f;

    UPROPERTY(BlueprintReadOnly)
    float MaxFocus = 100.0f;

    UPROPERTY(BlueprintReadOnly)
    float Impurities = 0.0f;

    UPROPERTY(BlueprintReadOnly)
    float MaximumImpurities = 0.0f;

    UPROPERTY(BlueprintReadOnly)
    float Temperature = 20.0f;

    UPROPERTY(BlueprintReadOnly)
    int32 TemperatureExcursions = 0;

    UPROPERTY(BlueprintReadOnly)
    int32 AcceptedActionCount = 0;

    UPROPERTY(BlueprintReadOnly)
    int32 OffMethodActionCount = 0;

    UPROPERTY(BlueprintReadOnly)
    TMap<FName, int32> ProcessCounts;

    UPROPERTY(BlueprintReadOnly)
    TMap<FName, float> NascentPathScores;

    UPROPERTY(BlueprintReadOnly)
    TMap<FName, float> NascentProperties;

    UPROPERTY(BlueprintReadOnly)
    TMap<FName, float> NascentAttributes;

    UPROPERTY(BlueprintReadOnly)
    TMap<FName, float> NascentTraits;

    UPROPERTY(BlueprintReadOnly)
    TMap<FName, float> ContaminationPaths;

    UPROPERTY(BlueprintReadOnly)
    TMap<FName, float> ContaminationAttributes;

    UPROPERTY(BlueprintReadOnly)
    TMap<FName, float> ContaminationTraits;

    UPROPERTY(BlueprintReadOnly)
    TArray<FRefinementObservation> Observations;

    UPROPERTY(BlueprintReadOnly)
    TArray<FRefinementActionRecord> ActionHistory;

    UPROPERTY(BlueprintReadOnly)
    TArray<FString> Log;

    UPROPERTY(BlueprintReadOnly)
    TArray<FRefinementAssistanceContribution> Assistance;

    UPROPERTY(BlueprintReadOnly)
    float AssistanceQualityBonus = 0.0f;

    UPROPERTY(BlueprintReadOnly)
    TMap<FName, int32> PathTechniqueCounts;

    UPROPERTY(BlueprintReadOnly)
    bool bQuietLull = false;

    UPROPERTY(BlueprintReadOnly)
    bool bInputsConsumed = false;

    UPROPERTY(BlueprintReadOnly)
    bool bFinished = false;

    UPROPERTY(BlueprintReadOnly)
    bool bSuccess = false;

    UPROPERTY(BlueprintReadOnly)
    int64 StartedAtUnixMs = 0;

    UPROPERTY(BlueprintReadOnly)
    FRefinementOutcome Outcome;
};

USTRUCT(BlueprintType)
struct FRefinementFailureRisk
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    float Destroy = 0.25f;

    UPROPERTY(BlueprintReadOnly)
    float Damage = 0.35f;

    UPROPERTY(BlueprintReadOnly)
    float Unharmed = 0.40f;
};

USTRUCT(BlueprintType)
struct FRefinementPowerAllocation
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    float BaseBudget = 100.0f;

    UPROPERTY(BlueprintReadOnly)
    float EffectiveBudget = 100.0f;

    UPROPERTY(BlueprintReadOnly)
    float ConstraintMultiplier = 1.0f;

    UPROPERTY(BlueprintReadOnly)
    TMap<FName, float> Allocation;

    UPROPERTY(BlueprintReadOnly)
    float Magnitude = 1.0f;

    UPROPERTY(BlueprintReadOnly)
    float Range = 0.0f;

    UPROPERTY(BlueprintReadOnly)
    float Area = 0.0f;

    UPROPERTY(BlueprintReadOnly)
    float DurationMs = 0.0f;

    UPROPERTY(BlueprintReadOnly)
    float SpeedMultiplier = 1.0f;
};

USTRUCT(BlueprintType)
struct FRefinementNotebookIngredient
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FGuid EntityId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    uint8 Kind = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FName SourceId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FName DefinitionId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, meta=(ClampMin="1"))
    int32 Quantity = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    bool bFoundation = false;
};

USTRUCT(BlueprintType)
struct FRefinementNotebookProcedureStep
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FString Name;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    ERefinementVerb Process = ERefinementVerb::Process;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    float RequiredProgress = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FVector2D TargetTemperature = FVector2D(20.0f, 60.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    float MaxImpurities = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    bool bRecoveryWindow = false;
};

USTRUCT(BlueprintType)
struct FRefinementNotebookRecord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FName Id;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    ERefinementKnowledgeState Status = ERefinementKnowledgeState::Observed;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FName ResultDefinitionId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FName ResultPath;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    int32 Rank = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    TArray<FRefinementNotebookIngredient> Ingredients;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    TArray<FRefinementNotebookProcedureStep> Procedure;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    int32 AttemptCount = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    int32 SuccessCount = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    int32 FailureCount = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    int64 FirstObservedAtUnixMs = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    int64 LastStudiedAtUnixMs = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FString LastOutcome;
};

USTRUCT(BlueprintType)
struct FRefinementNotebookSnapshot
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    int32 Version = 2;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    TArray<FRefinementNotebookRecord> Records;
};
