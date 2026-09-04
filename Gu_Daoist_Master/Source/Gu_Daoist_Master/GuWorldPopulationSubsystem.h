#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GuWorldPopulationSubsystem.generated.h"

class AWildGuWorldActor;

/** Save-friendly world state for a physical Gu. Actor pointers deliberately stay out. */
USTRUCT(BlueprintType)
struct FGuWildWorldSnapshot
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gu|World")
    FGuid EntityId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gu|World")
    FName DefinitionId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gu|World")
    FName RegionId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gu|World")
    int32 SpawnSeed = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gu|World")
    FTransform WorldTransform = FTransform::Identity;
};

USTRUCT()
struct FGuWildWorldRuntimeEntry
{
    GENERATED_BODY()

    UPROPERTY()
    FGuWildWorldSnapshot Snapshot;

    UPROPERTY()
    TSubclassOf<AWildGuWorldActor> ActorClass;

    UPROPERTY()
    TWeakObjectPtr<AWildGuWorldActor> ProxyActor;
};

/**
 * World projection layer for physical Gu ECS entities.
 *
 * The Gu entity remains authoritative in UGuEntitySubsystem. This subsystem owns only
 * world-placement metadata and the disposable Actor proxy used for rendering,
 * interaction, relevancy and level streaming.
 */
UCLASS()
class UGuWorldPopulationSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void Deinitialize() override;

    /**
     * Registers an ECS Gu as present in this world. The EntityId must already exist in
     * UGuEntitySubsystem. This function never creates a second physical Gu.
     */
    UFUNCTION(BlueprintCallable, Category = "Gu|World")
    bool RegisterExistingWildGu(
        const FGuid& EntityId,
        FName DefinitionId,
        FName RegionId,
        int32 SpawnSeed,
        const FTransform& WorldTransform,
        TSubclassOf<AWildGuWorldActor> ActorClass,
        bool bSpawnProxy = true);

    /** Removes world presence only. The physical ECS Gu remains alive. */
    UFUNCTION(BlueprintCallable, Category = "Gu|World")
    bool UnregisterWildGu(const FGuid& EntityId, bool bDestroyProxy = true);

    UFUNCTION(BlueprintCallable, Category = "Gu|World")
    AWildGuWorldActor* SpawnProxyForEntity(const FGuid& EntityId);

    UFUNCTION(BlueprintCallable, Category = "Gu|World")
    bool DespawnProxyForEntity(const FGuid& EntityId);

    UFUNCTION(BlueprintCallable, Category = "Gu|World")
    bool UpdateWorldTransform(const FGuid& EntityId, const FTransform& NewTransform, bool bMoveProxy = true);

    /** Activating a region materializes its registered Gu proxies; deactivating streams them out. */
    UFUNCTION(BlueprintCallable, Category = "Gu|World")
    void SetRegionActive(FName RegionId, bool bActive);

    UFUNCTION(BlueprintPure, Category = "Gu|World")
    bool IsRegionActive(FName RegionId) const;

    UFUNCTION(BlueprintPure, Category = "Gu|World")
    bool HasWorldGu(const FGuid& EntityId) const;

    UFUNCTION(BlueprintPure, Category = "Gu|World")
    bool GetWorldSnapshot(const FGuid& EntityId, FGuWildWorldSnapshot& OutSnapshot) const;

    UFUNCTION(BlueprintCallable, Category = "Gu|World")
    void GetEntitiesInRegion(FName RegionId, TArray<FGuid>& OutEntityIds) const;

    /** Save-game bridge. Call from the existing domain save pipeline. */
    void ExportSnapshots(TArray<FGuWildWorldSnapshot>& OutSnapshots) const;

    /** Restore after UGuEntitySubsystem has restored its physical entities. */
    void RestoreSnapshots(const TArray<FGuWildWorldSnapshot>& Snapshots, bool bSpawnActiveRegions);

private:
    bool PhysicalEntityExists(const FGuid& EntityId) const;
    void DestroyAllProxies();

    UPROPERTY(Transient)
    TMap<FGuid, FGuWildWorldRuntimeEntry> WorldGu;

    UPROPERTY(Transient)
    TSet<FName> ActiveRegions;
};
