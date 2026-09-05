#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GuEcologyPopulationTypes.h"
#include "ApertureComponent.h"
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
     * Capture = ownership/container transition of the SAME FGuid.
     * The world proxy is removed; the physical Gu is not destroyed/re-created.
     */
    UFUNCTION(BlueprintCallable, Category="Gu|World|Population")
    FGuWorldCaptureResult CaptureWildGu(
        FGuid EntityId,
        const FString& NewOwnerId,
        UApertureComponent* Aperture,
        EGuContainer TargetContainer = EGuContainer::Aperture);

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
