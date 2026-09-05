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
 * This actor is an authoring/spawn instruction only. Every spawned Gu is a real
 * UGuEntitySubsystem entity, tracked as an ecology resident and projected through
 * AWildGuWorldActor. Destroying this spawner does not destroy the physical Gu.
 */
UCLASS(Blueprintable)
class GU_DAOIST_MASTER_API AWildGuSpawner : public AActor
{
    GENERATED_BODY()

public:
    AWildGuSpawner();

    virtual void PostInitializeComponents() override;
    virtual void BeginPlay() override;

    /** Authored Gu species. Preferred for ordinary level-authored Gu. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gu|Spawner")
    TObjectPtr<UGuDefinition> GuDefinition;

    /**
     * Optional registry id for runtime/procedural species. Used only when GuDefinition is null.
     * The id must already exist in UGuDefinitionRegistrySubsystem when spawning occurs.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gu|Spawner")
    FName DefinitionId = NAME_None;

    /** Visual/interaction Blueprint spawned for each physical Gu. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gu|Spawner")
    TSubclassOf<AWildGuWorldActor> WildGuActorClass;

    /** Logical world region used by streaming/world-population systems. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gu|Spawner")
    FName RegionId = TEXT("World.Region.Debug");

    /**
     * Stable logical id for this authored spawn site. Set this explicitly for save-stable content.
     * If left empty, the actor path is used as a deterministic fallback.
     */
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Gu|Spawner")
    FName SpawnerId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gu|Spawner", meta=(ClampMin="1", ClampMax="64"))
    int32 SpawnCount = 1;

    /** XY radius around the actor. Zero spawns exactly at the actor transform. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gu|Spawner", meta=(ClampMin="0.0"))
    float SpawnRadiusCm = 0.0f;

    /** Deterministic salt combined with SpawnerId + slot index. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gu|Spawner")
    int32 BaseSeed = 1;

    /** Snap randomized spawn positions to blocking world geometry below them. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gu|Spawner|Placement")
    bool bProjectToGround = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gu|Spawner|Placement", meta=(ClampMin="0.0"))
    float GroundTraceUpCm = 5000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gu|Spawner|Placement", meta=(ClampMin="0.0"))
    float GroundTraceDownCm = 20000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gu|Spawner")
    bool bSpawnOnBeginPlay = true;

    /** Small delay lets persistent-domain/world restore complete before authored spawn reconciliation. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gu|Spawner", meta=(ClampMin="0.0", ClampMax="10.0"))
    float InitialSpawnDelaySeconds = 0.10f;

    /** Physical ids currently resolved/created by this spawner during this world session. */
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Gu|Spawner|Runtime")
    TArray<FGuid> SpawnedEntityIds;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Gu|Spawner|Runtime")
    FString LastSpawnError;

    UPROPERTY(BlueprintAssignable, Category="Gu|Spawner")
    FGuWildGuSpawnedSignature OnWildGuSpawned;

    /** Reconciles every authored slot. Existing saved residents are reused rather than duplicated. */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Gu|Spawner")
    TArray<FGuid> SpawnWildGuBatch();

    /** Reconciles/spawns one specific slot. */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Gu|Spawner")
    FGuid SpawnWildGuSlot(int32 SlotIndex);

    UFUNCTION(BlueprintPure, Category="Gu|Spawner")
    FName GetResolvedSpawnerId() const;

    UFUNCTION(BlueprintPure, Category="Gu|Spawner")
    FName GetResolvedDefinitionId() const;

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Gu|Spawner")
    TObjectPtr<USceneComponent> SceneRoot;

    UFUNCTION(BlueprintImplementableEvent, Category="Gu|Spawner", meta=(DisplayName="Wild Gu Spawned"))
    void K2_OnWildGuSpawned(FGuid EntityId, AWildGuWorldActor* ProxyActor);

private:
    void HandleInitialSpawn();
    bool PrepareDefinition(FName& OutDefinitionId, FString& OutError) const;
    int32 StableSeedForSlot(int32 SlotIndex) const;
    FTransform SpawnTransformForSlot(int32 SlotIndex, int32 SpawnSeed) const;
    FVector ProjectLocationToGround(const FVector& Proposed) const;
};
