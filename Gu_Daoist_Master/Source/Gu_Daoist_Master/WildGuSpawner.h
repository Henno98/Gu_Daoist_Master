#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WildGuSpawner.generated.h"

class AWildGuWorldActor;
class UGuDefinition;
class USceneComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FGuWildGuSpawnedSignature,
    FGuid, EntityId,
    AWildGuWorldActor*, ProxyActor);

/**
 * Level-authored bridge into the physical wild-Gu simulation.
 *
 * This actor is only the spawn instruction. Every spawned Gu becomes a real
 * UGuEntitySubsystem entity, an ecology resident, and an AWildGuWorldActor proxy.
 */
UCLASS(Blueprintable)
class GU_DAOIST_MASTER_API AWildGuSpawner : public AActor
{
    GENERATED_BODY()

public:
    AWildGuSpawner();

    virtual void PostInitializeComponents() override;
    virtual void BeginPlay() override;

    // ---------------------------------------------------------------------
    // Species
    // ---------------------------------------------------------------------

    /** Gu species to physically spawn. Preferred for authored level content. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gu|Spawner|Species",
        meta=(ExposeOnSpawn="true", DisplayName="Gu Definition"))
    TObjectPtr<UGuDefinition> GuDefinition;

    /**
     * Registry ID fallback for procedural/runtime species.
     * Used only when GuDefinition is null.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gu|Spawner|Species",
        meta=(ExposeOnSpawn="true", AdvancedDisplay))
    FName DefinitionId = NAME_None;

    /** Visual/interaction actor used to project the physical Gu into the world. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gu|Spawner|Species",
        meta=(ExposeOnSpawn="true"))
    TSubclassOf<AWildGuWorldActor> WildGuActorClass;

    // ---------------------------------------------------------------------
    // World identity
    // ---------------------------------------------------------------------

    /** Logical world region used by ecology/world-population systems. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gu|Spawner|World",
        meta=(ExposeOnSpawn="true"))
    FName RegionId = TEXT("World.Region.Debug");

    /**
     * Stable authored spawn-site ID. Set this explicitly for save-stable content.
     * If empty, the placed actor path becomes the deterministic fallback.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gu|Spawner|World",
        meta=(ExposeOnSpawn="true"))
    FName SpawnerId = NAME_None;

    // ---------------------------------------------------------------------
    // Spawn layout
    // ---------------------------------------------------------------------

    /** Number of physical Gu represented by this spawn site. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gu|Spawner|Spawn",
        meta=(ExposeOnSpawn="true", ClampMin="1", ClampMax="64", UIMin="1", UIMax="64"))
    int32 SpawnCount = 1;

    /** XY radius around the spawner. Zero spawns exactly at the actor transform. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gu|Spawner|Spawn",
        meta=(ExposeOnSpawn="true", ClampMin="0.0", UIMin="0.0"))
    float SpawnRadiusCm = 0.0f;

    /** Deterministic salt combined with SpawnerId and slot index. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gu|Spawner|Spawn",
        meta=(ExposeOnSpawn="true"))
    int32 BaseSeed = 1;

    // ---------------------------------------------------------------------
    // Placement
    // ---------------------------------------------------------------------

    /** Snap randomized positions to blocking world geometry underneath them. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gu|Spawner|Placement",
        meta=(ExposeOnSpawn="true"))
    bool bProjectToGround = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gu|Spawner|Placement",
        meta=(ClampMin="0.0", AdvancedDisplay))
    float GroundTraceUpCm = 5000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gu|Spawner|Placement",
        meta=(ClampMin="0.0", AdvancedDisplay))
    float GroundTraceDownCm = 20000.0f;

    // ---------------------------------------------------------------------
    // Startup
    // ---------------------------------------------------------------------

    /** Automatically reconcile/spawn this site when play begins. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gu|Spawner|Startup",
        meta=(ExposeOnSpawn="true"))
    bool bSpawnOnBeginPlay = true;

    /** Small persistence-load grace period before authored spawn reconciliation. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gu|Spawner|Startup",
        meta=(ClampMin="0.0", ClampMax="10.0", AdvancedDisplay))
    float InitialSpawnDelaySeconds = 0.10f;

    // ---------------------------------------------------------------------
    // Runtime state
    // ---------------------------------------------------------------------

    /** Physical entity IDs currently resolved by this spawn site. */
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Gu|Spawner|Runtime")
    TArray<FGuid> SpawnedEntityIds;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Gu|Spawner|Runtime")
    FString LastSpawnError;

    UPROPERTY(BlueprintAssignable, Category="Gu|Spawner|Events")
    FGuWildGuSpawnedSignature OnWildGuSpawned;

    /** Reconciles every authored slot. Saved residents are reused rather than duplicated. */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Gu|Spawner")
    TArray<FGuid> SpawnWildGuBatch();

    /** Reconciles/spawns one specific authored slot. */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Gu|Spawner")
    FGuid SpawnWildGuSlot(int32 SlotIndex);

    UFUNCTION(BlueprintPure, Category="Gu|Spawner")
    FName GetResolvedSpawnerId() const;

    UFUNCTION(BlueprintPure, Category="Gu|Spawner")
    FName GetResolvedDefinitionId() const;

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Gu|Spawner")
    TObjectPtr<USceneComponent> SceneRoot;

    UFUNCTION(BlueprintImplementableEvent, Category="Gu|Spawner|Events", meta=(DisplayName="Wild Gu Spawned"))
    void K2_OnWildGuSpawned(FGuid EntityId, AWildGuWorldActor* ProxyActor);

private:
    void HandleInitialSpawn();
    bool PrepareDefinition(FName& OutDefinitionId, FString& OutError) const;
    int32 StableSeedForSlot(int32 SlotIndex) const;
    FTransform SpawnTransformForSlot(int32 SlotIndex, int32 SpawnSeed) const;
    FVector ProjectLocationToGround(const FVector& Proposed) const;
};
