#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GuWorldDaoEcologyTypes.h"
#include "GuWorldDaoEcologySubsystem.generated.h"

class UGuDefinition;

/**
 * Map-independent environmental Dao simulation.
 *
 * Landscape/PCG/world authoring supplies substrate fields. Runtime Gu use and
 * historical effects supply signed Dao events. Resource, beast and wild-Gu
 * potential are derived from the resulting local profile rather than spawned as
 * arbitrary POIs.
 */
UCLASS()
class GU_DAOIST_MASTER_API UGuWorldDaoEcologySubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // Browser v7.9.84 parity constants/formulas. Values are project simulation defaults.
    static float TraceUnitsForRank(int32 Rank);
    static float TraceHalfLifeYearsForRank(int32 Rank);
    static float WildGuDensityForRank(int32 Rank);
    static float WildGuMaturityYearsForRank(int32 Rank);
    static float DaoRetentionFraction(float AgeYears, float HalfLifeYears);
    static float ContinuousDaoStock(float RatePerYear, float Years, float HalfLifeYears);
    static float DecayDaoStock(float Amount, float Years, float HalfLifeYears);
    static FGuDaoSuccessionState EvaluateSuccession(
        const FGameplayTag& Path,
        float Density,
        float TotalDensity,
        float MaturityYears);

    UFUNCTION(BlueprintCallable, Category = "Gu|Dao Ecology")
    bool RegisterRegionField(const FGuDaoRegionField& Field);

    UFUNCTION(BlueprintCallable, Category = "Gu|Dao Ecology")
    bool RemoveRegionField(FName RegionId);

    UFUNCTION(BlueprintCallable, Category = "Gu|Dao Ecology")
    void ClearRegionFields();

    /** Continuous clan/settlement Gu use accumulated with exponential turnover. */
    UFUNCTION(BlueprintCallable, Category = "Gu|Dao Ecology")
    bool RegisterActivityField(const FGuDaoActivityField& Field);

    UFUNCTION(BlueprintCallable, Category = "Gu|Dao Ecology")
    bool RemoveActivityField(FName ActivityId);

    UFUNCTION(BlueprintCallable, Category = "Gu|Dao Ecology")
    void ClearActivityFields();

    UFUNCTION(BlueprintCallable, Category = "Gu|Dao Ecology")
    FGuid RecordDaoEvent(const FGuWorldDaoEvent& Event);

    UFUNCTION(BlueprintCallable, Category = "Gu|Dao Ecology")
    bool RemoveDaoEvent(const FGuid& EventId);

    UFUNCTION(BlueprintCallable, Category = "Gu|Dao Ecology")
    void ClearDynamicEvents();

    /**
     * Records one successful Gu activation at a physical world location.
     * Only authority mutates environmental Dao state.
     */
    UFUNCTION(BlueprintCallable, Category = "Gu|Dao Ecology")
    FGuid RecordGuActivation(
        const UGuDefinition* Definition,
        FVector WorldLocation,
        float Activations = 1.0f,
        float Retention = 1.0f,
        float RadiusCm = 9000.0f);

    /** Explicit weighted-path variant for future multi-path effects. Weights are normalized. */
    FGuid RecordGuActivationWeighted(
        const UGuDefinition* Definition,
        const FVector& WorldLocation,
        const TMap<FGameplayTag, float>& PathWeights,
        float Activations = 1.0f,
        float Retention = 1.0f,
        float RadiusCm = 9000.0f);

    /** Advance loose environmental residue. Permanent/engraved events are unaffected. */
    UFUNCTION(BlueprintCallable, Category = "Gu|Dao Ecology")
    void AdvanceEcologyYears(float Years);

    UFUNCTION(BlueprintPure, Category = "Gu|Dao Ecology")
    FGuDaoEcologyProfile GetProfileAt(FVector WorldLocation) const;

    UFUNCTION(BlueprintPure, Category = "Gu|Dao Ecology")
    TArray<FGuDaoEcologyCandidate> GetWildGuCandidatesAt(FVector WorldLocation, int32 MaxRank = 9) const;

    UFUNCTION(BlueprintPure, Category = "Gu|Dao Ecology")
    TArray<FGuDaoEcologyCandidate> GetResourceCandidatesAt(FVector WorldLocation) const;

    UFUNCTION(BlueprintPure, Category = "Gu|Dao Ecology")
    TArray<FGuDaoEcologyCandidate> GetBeastCandidatesAt(FVector WorldLocation) const;

    UFUNCTION(BlueprintCallable, Category = "Gu|Dao Ecology")
    bool RegisterWildGuHabitatRule(const FGuWildGuHabitatRule& Rule);

    /** Convenience bridge for the existing definition registry. Call for non-Relic species. */
    UFUNCTION(BlueprintCallable, Category = "Gu|Dao Ecology")
    bool RegisterWildGuDefinition(FName DefinitionId, const UGuDefinition* Definition);

    UFUNCTION(BlueprintCallable, Category = "Gu|Dao Ecology")
    void ClearWildGuHabitatRules();

    /** Save bridge for the existing domain persistence layer. */
    void ExportDynamicEvents(TArray<FGuWorldDaoEvent>& OutEvents) const;
    void RestoreDynamicEvents(const TArray<FGuWorldDaoEvent>& Events);

private:
    void RegisterDefaultRules();
    void InvalidateProfiles() const;
    void InvalidateEventIndex();
    void RebuildEventSpatialIndex() const;

    FGameplayTag PathTag(const TCHAR* Name) const;
    FName ResolveSubstrateAt(const FVector2D& Point) const;
    void AddSubstrateBaseline(FName Substrate, TMap<FGameplayTag, float>& Marks, TMap<FGameplayTag, float>& Ages) const;
    FGuDaoEcologyProfile BuildProfileAt(const FVector2D& Point) const;
    void FinalizeProfile(FGuDaoEcologyProfile& Profile) const;
    void PopulateDerivedEcology(FGuDaoEcologyProfile& Profile) const;

    float RegionScale(const FGuDaoRegionField& Field, const FVector2D& Point) const;
    float ActivityScale(const FGuDaoActivityField& Field, const FVector2D& Point) const;
    void AddBioticFeedback(FGuDaoEcologyProfile& Profile) const;
    float EventScale(const FGuWorldDaoEvent& Event, const FVector2D& Point) const;
    float EventPersistence(const FGuWorldDaoEvent& Event) const;
    FBox2D EventBounds(const FGuWorldDaoEvent& Event) const;
    void GatherNearbyEventIds(const FVector2D& Point, TArray<FGuid>& OutIds) const;

    float ComputeInteractionPressure(const TMap<FGameplayTag, float>& Marks) const;
    void ApplyConflictTurnover(TMap<FGameplayTag, float>& Marks, TMap<FGameplayTag, float>& Ages, TMap<FGameplayTag, float>& OutLosses) const;

    UPROPERTY(Transient)
    TMap<FName, FGuDaoRegionField> RegionFields;

    UPROPERTY(Transient)
    TMap<FGuid, FGuWorldDaoEvent> DynamicEvents;

    UPROPERTY(Transient)
    TMap<FName, FGuDaoActivityField> ActivityFields;

    UPROPERTY(Transient)
    TArray<FGuDaoConflictRule> ConflictRules;

    UPROPERTY(Transient)
    TArray<FGuWildGuHabitatRule> WildGuRules;

    UPROPERTY(Transient)
    TArray<FGuSubstrateResourceRule> ResourceRules;

    UPROPERTY(Transient)
    TArray<FGuBeastSuccessionRule> BeastRules;

    mutable TMap<FIntPoint, FGuDaoEcologyProfile> ProfileCache;
    mutable TMap<FIntPoint, TArray<FGuid>> EventSpatialBuckets;
    mutable TSet<FGuid> GlobalEventIds;
    mutable bool bEventIndexDirty = true;

    static constexpr float QueryCellSizeCm = 12800.0f;
    static constexpr float EventIndexCellSizeCm = 102400.0f;
    static constexpr int32 EventIndexThreshold = 64;
    static constexpr int32 MaxCellsPerIndexedEvent = 512;
    static constexpr int32 MaxProfileCacheEntries = 8192;
};
