#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "RefinementTypes.h"
#include "GuDefinitionTypes.h"
#include "RefinementSubsystem.generated.h"

class AGuPlayerState;

UCLASS()
class GU_DAOIST_MASTER_API URefinementSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Refinement")
    bool AnalyzePhysicalInputs(const TArray<FGuid>& EntityIds, FRefinementAnalysis& OutAnalysis, FString& OutError) const;

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Refinement")
    bool AnalyzePhysicalSelections(const TArray<FRefinementInputSelection>& Inputs, FRefinementAnalysis& OutAnalysis, FString& OutError) const;

    UFUNCTION(BlueprintPure, Category="Refinement")
    FRefinementDirection ResolveFormationDirection(const TMap<FName, float>& PathScores, const TMap<FName, float>& PropertyScores) const;

    UFUNCTION(BlueprintCallable, Category="Refinement")
    void ApplyVerbToProperties(UPARAM(ref) TMap<FName, float>& PropertyScores, ERefinementVerb Verb, float Power = 1.0f) const;

    UFUNCTION(BlueprintCallable, Category="Refinement")
    void ApplyMethodPathPressure(UPARAM(ref) TMap<FName, float>& PathScores, const TMap<FName, float>& PropertyScores, ERefinementVerb Verb, FName MethodPath, float Power = 1.0f) const;

    UFUNCTION(BlueprintPure, Category="Refinement")
    FRefinementRetentionResult ResolveRetainedDaoMass(const FRefinementAnalysis& Analysis, const FRefinementProcessHealth& Health) const;

    UFUNCTION(BlueprintPure, Category="Refinement")
    float DaoMassRequiredForRank(int32 Rank) const;

    UFUNCTION(BlueprintPure, Category="Refinement")
    float TraitBudgetMultiplier(const TArray<FName>& Traits, FName Template = TEXT("attribute")) const;

    UFUNCTION(BlueprintPure, Category="Refinement")
    FRefinementPowerAllocation ResolvePowerAllocation(int32 Rank, FName Template, const TArray<FName>& Attributes, const TArray<FName>& Traits) const;

    UFUNCTION(BlueprintPure, Category="Refinement")
    FRefinementFailureRisk FailureRiskForRank(int32 Rank) const;

    /** Starts one server-authoritative physical refinement. IntendedDefinitionId may be empty for true experimentation. */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Refinement|Session")
    bool BeginRefinementSession(AGuPlayerState* PlayerState, const TArray<FGuid>& EntityIds, FName IntendedDefinitionId, bool bKnownRecipe, FString& OutError);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Refinement|Session")
    bool BeginRefinementSessionWithQuantities(AGuPlayerState* PlayerState, const TArray<FRefinementInputSelection>& Inputs, FName IntendedDefinitionId, bool bKnownRecipe, FString& OutError);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Refinement|Session")
    bool UseBasicRefinementAction(AGuPlayerState* PlayerState, ERefinementVerb Verb, FString& OutError);

    /** Brings an owned living refinement Gu into the active cauldron as a limited-use technique source. */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Refinement|Assistance")
    bool AttachGuRefinementAssistant(AGuPlayerState* PlayerState, FGuid GuEntityId, FString& OutError);

    /** Uses one prepared technique from an attached refinement Gu. */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Refinement|Assistance")
    bool UseGuRefinementAssistant(AGuPlayerState* PlayerState, FGuid GuEntityId, FString& OutError);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Refinement|Session")
    bool AbortRefinementSession(AGuPlayerState* PlayerState, FString& OutError);

    UFUNCTION(BlueprintPure, Category="Refinement|Session")
    bool HasActiveSession(const FString& OwnerId) const;

    /** Player-safe view only. Exact hidden procedure and physical measurements are omitted. */
    UFUNCTION(BlueprintPure, Category="Refinement|Session")
    FRefinementPublicState GetPublicState(const FString& OwnerId) const;

    /** Developer/editor diagnostics. Do not replicate this to ordinary clients. */
    UFUNCTION(BlueprintPure, Category="Refinement|Debug")
    bool GetDebugSessionState(const FString& OwnerId, FRefinementSessionState& OutState) const;

    UFUNCTION(BlueprintPure, Category="Refinement|Notebook")
    FRefinementNotebookSnapshot ExportNotebook() const;

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Refinement|Notebook")
    void RestoreNotebook(const FRefinementNotebookSnapshot& Snapshot);

    UFUNCTION(BlueprintPure, Category="Refinement|Notebook")
    TArray<FRefinementNotebookRecord> GetNotebookRecords() const;

private:
    struct FBasicActionSpec
    {
        ERefinementVerb Verb = ERefinementVerb::Process;
        float FocusCost = 0.0f;
        float ProgressPower = 0.0f;
        float TemperatureDelta = 0.0f;
        float TemperatureClampMin = -FLT_MAX;
        float StabilityDelta = 0.0f;
        float ImpurityDelta = 0.0f;
        float ImpurityRemovalCap = 0.0f;
        float PurifyContaminationFraction = 0.0f;
        float PurifyContaminationMax = 0.0f;
    };

    static int64 NowUnixMs();
    static void AddScore(TMap<FName, float>& Target, FName Key, float Amount);
    static void AddScores(TMap<FName, float>& Target, const TMap<FName, float>& Source, float Multiplier = 1.0f);
    static TArray<TPair<FName, float>> SortedScores(const TMap<FName, float>& Scores);
    static float TransferProperty(TMap<FName, float>& Scores, FName From, FName To, float Amount);
    static FString VerbName(ERefinementVerb Verb);
    static FBasicActionSpec BasicAction(ERefinementVerb Verb);

    static const TMap<FName, TMap<FName, float>>& PathProperties();
    static const TMap<FName, TMap<FName, float>>& AttributeProperties();
    static const TMap<FName, TMap<FName, float>>& TraitProperties();
    static const TMap<FName, TMap<FName, float>>& PropertyAttributes();
    static const TMap<FName, TMap<FName, float>>& PropertyTraits();
    static const TMap<FName, TMap<FName, float>>& PathTraits();

    static TMap<FName, float> DerivedPathScores(const TMap<FName, float>& PropertyScores);
    static TMap<FName, float> TraitsFromProperties(const TMap<FName, float>& PropertyScores);
    static void ContaminationSemantics(FName Path, float Amount, TMap<FName, float>& OutProperties, TMap<FName, float>& OutAttributes, TMap<FName, float>& OutTraits);
    static float PropertyPathAffinity(FName Path, FName Property);
    static bool PathsCompatible(FName A, FName B);

    FRefinementDirection EffectiveDirection(const FRefinementSessionState& Session) const;
    void RebuildDivergentAnalysis(FRefinementSessionState& Session) const;
    void GenerateProcedure(FRefinementSessionState& Session) const;
    float ProcedureAffinity(const FRefinementProcedureStep& Step, ERefinementVerb Verb) const;
    float SessionFidelity(const FRefinementSessionState& Session) const;
    float ContaminationTotal(const FRefinementSessionState& Session) const;
    float PurifyContamination(FRefinementSessionState& Session, float Fraction, float MaxAmount) const;
    void ApplyPhysicalAction(FRefinementSessionState& Session, const FBasicActionSpec& Action) const;
    void GuideNascentFormation(FRefinementSessionState& Session, ERefinementVerb Verb, float Affinity, float StageFraction, FName TechniquePath = NAME_None, bool bSourceTechnique = false) const;
    void ApplyTemperatureConsequences(FRefinementSessionState& Session, const FRefinementProcedureStep& Step, ERefinementVerb Verb, float BeforeTemperature) const;
    float RecordObservableFeedback(FRefinementSessionState& Session, ERefinementVerb Verb, float Affinity, float SemanticDelta, float StabilityDelta, float ImpurityDelta, float ProgressFraction, bool bOutsideTemperature) const;
    FString ObservableResponseLabel(const TArray<FRefinementObservation>& History) const;
    FString PublicFormLabel(const FRefinementSessionState& Session) const;
    FString PublicConditionLabel(const FRefinementSessionState& Session) const;
    void CheckStepCompletion(AGuPlayerState* PlayerState, FRefinementSessionState& Session);
    bool CheckFailure(AGuPlayerState* PlayerState, FRefinementSessionState& Session);
    void FinishSession(AGuPlayerState* PlayerState, FRefinementSessionState& Session);
    FRefinementOutcome ResolveOutcome(const FRefinementSessionState& Session) const;
    FRefinementPublicState BuildPublicState(const FRefinementSessionState& Session) const;
    void PublishPublicState(AGuPlayerState* PlayerState, const FRefinementSessionState* Session) const;
    void AddLog(FRefinementSessionState& Session, const FString& Message) const;
    void RecordNotebook(const FRefinementSessionState& Session);
    FName NotebookIdForSession(const FRefinementSessionState& Session) const;

    FName ResolveExperimentalTemplate(const FRefinementAnalysis& Analysis) const;
    FGuDefinitionRecord BuildExperimentalDefinition(const FRefinementSessionState& Session) const;
    bool SecureSuccessfulOutcome(AGuPlayerState* PlayerState, FRefinementSessionState& Session, FString& OutError);
    void ConsumeCommittedInputs(FRefinementSessionState& Session);
    void ResolveFailedCommittedInputs(FRefinementSessionState& Session);
    void ReleaseInputReservations(const FRefinementSessionState& Session);
    bool IsInputReserved(FGuid EntityId) const;

    UPROPERTY(Transient)
    TMap<FString, FRefinementSessionState> ActiveSessions;

    UPROPERTY(Transient)
    TMap<FName, FRefinementNotebookRecord> NotebookRecords;

    /** Prevents a physical ingredient from participating in two live refinements. */
    TSet<FGuid> ReservedInputEntities;
};
