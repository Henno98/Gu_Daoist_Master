#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GuEcologyPopulationTypes.h"
#include "GuEcologyPopulationSubsystem.generated.h"

class UGuWorldDaoEcologySubsystem;
class UGuEntitySubsystem;

/**
 * Materializes ecology candidates into persistent ECS residents.
 *
 * Authority:
 * - UGuWorldDaoEcologySubsystem decides what the habitat supports.
 * - UGuEntitySubsystem owns physical entity identity/lifecycle.
 * - UGuWorldPopulationSubsystem owns disposable wild-Gu Actor proxies.
 * - this subsystem owns only ecology population membership + world transforms.
 */
UCLASS()
class GU_DAOIST_MASTER_API UGuEcologyPopulationSubsystem : public UTickableWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override;
    virtual bool IsTickable() const override;

    /** Default is deliberately slow. Population simulation should not run every frame. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gu|World|Population", meta=(ClampMin="1.0"))
    float ReconcileIntervalSeconds = 10.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gu|World|Population")
    bool bAutoReconcile = true;

    UFUNCTION(BlueprintCallable, Category="Gu|World|Population")
    bool RegisterPopulationSite(const FGuEcologyPopulationSite& Site);

    UFUNCTION(BlueprintCallable, Category="Gu|World|Population")
    bool RemovePopulationSite(FName SiteId, bool bDestroyResidents = false);

    UFUNCTION(BlueprintCallable, Category="Gu|World|Population")
    bool SetPopulationSiteActive(FName SiteId, bool bActive);

    UFUNCTION(BlueprintCallable, Category="Gu|World|Population")
    void ReconcileAllSites();

    UFUNCTION(BlueprintCallable, Category="Gu|World|Population")
    bool ReconcileSite(FName SiteId);

    /**
     * Register currently known authored/runtime Gu definitions as ecology habitat rules.
     * Mirrors browser v7.9.84: path-bearing Gu are eligible except Relic Gu.
     */
    UFUNCTION(BlueprintCallable, Category="Gu|World|Population")
    int32 RefreshWildGuHabitatRules();
    /**
     * Legacy C++ entry point retained for source compatibility. TargetContainer
     * is compatibility-only: capture now means physical restraint, not ownership.
     */
    FGuWorldCaptureResult CaptureWildGu(
        FGuid EntityId,
        const FString& NewOwnerId,
        EGuContainer TargetContainer = EGuContainer::Aperture);

    /** Physically restrains a tracked wild Gu while preserving its FGuid. */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Gu|World|Population")
    FGuWorldCaptureResult PhysicallyCaptureWildGu(FGuid EntityId, const FString& CaptorId);

    // WildGuSpawner v2 authored-resident bridge.
    /**
     * Registers a level-authored physical Gu as a first-class ecology resident.
     * This preserves the same FGuid and makes normal CaptureWildGu() work.
     * A zero-capacity metadata site is retained so save/restore retains authored
     * population membership without letting ecology reconciliation spawn extras.
     */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Gu|World|Population")
    bool RegisterAuthoredWildGuResident(
        FGuid EntityId,
        FName SiteId,
        FName RegionId,
        FName DefinitionId,
        int32 SlotIndex,
        const FTransform& WorldTransform,
        int32 SpawnSeed,
        TSubclassOf<AWildGuWorldActor> ActorClass,
        bool bSpawnProxy = true);

    UFUNCTION(BlueprintPure, Category="Gu|World|Population")
    bool GetResident(FGuid EntityId, FGuEcologyResident& OutResident) const;

    UFUNCTION(BlueprintCallable, Category="Gu|World|Population")
    void GetResidentsInSite(FName SiteId, TArray<FGuEcologyResident>& OutResidents) const;

    UFUNCTION(BlueprintCallable, Category="Gu|World|Population")
    void GetResidentsInRegion(FName RegionId, TArray<FGuEcologyResident>& OutResidents) const;

    /** Save bridge. Restore ECS entities before restoring these population records. */
    void ExportState(TArray<FGuEcologyPopulationSite>& OutSites, TArray<FGuEcologyResident>& OutResidents) const;
    void RestoreState(const TArray<FGuEcologyPopulationSite>& InSites, const TArray<FGuEcologyResident>& InResidents);

    /** Pure balance helper, exposed for automation tests. */
    static float SpawnChanceForIntensity(float Intensity);

private:
    bool HasAuthority() const;
    FVector SampleSlotLocation(const FGuEcologyPopulationSite& Site, int32 SlotIndex, int32& OutSeed) const;
    FVector ResolveGroundLocation(const FGuEcologyPopulationSite& Site, const FVector& Proposed) const;
    bool HasResidentInSlot(FName SiteId, int32 SlotIndex) const;
    void PurgeMissingEntities();
    bool SpawnResidentForSlot(const FGuEcologyPopulationSite& Site, int32 SlotIndex);
    FRefinementSemanticProfile SemanticProfileForCandidate(const struct FGuDaoEcologyCandidate& Candidate) const;

    UPROPERTY(Transient)
    TMap<FName, FGuEcologyPopulationSite> Sites;

    UPROPERTY(Transient)
    TMap<FGuid, FGuEcologyResident> Residents;

    float ReconcileAccumulator = 0.0f;
    bool bInitialRuleRefreshDone = false;
};
