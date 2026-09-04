#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WildGuWorldActor.generated.h"

class USceneComponent;
class USphereComponent;

/**
 * Lightweight Unreal presentation for one already-existing physical Gu ECS entity.
 *
 * The actor does NOT own the Gu's lifecycle. Destroying/unloading this actor must not
 * destroy the ECS entity. The FGuid is the durable identity; this actor is only the
 * streamed/replicated world representation.
 */
UCLASS(Blueprintable)
class AWildGuWorldActor : public AActor
{
    GENERATED_BODY()

public:
    AWildGuWorldActor();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    void InitializeWorldGu(
        const FGuid& InEntityId,
        FName InDefinitionId,
        FName InRegionId,
        int32 InSpawnSeed);

    UFUNCTION(BlueprintPure, Category = "Gu|World")
    FGuid GetGuEntityId() const { return GuEntityId; }

    UFUNCTION(BlueprintPure, Category = "Gu|World")
    FName GetGuDefinitionId() const { return DefinitionId; }

    UFUNCTION(BlueprintPure, Category = "Gu|World")
    FName GetGuRegionId() const { return RegionId; }

    UFUNCTION(BlueprintPure, Category = "Gu|World")
    int32 GetGuSpawnSeed() const { return SpawnSeed; }

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gu|World")
    TObjectPtr<USceneComponent> SceneRoot;

    /** Generic proximity/interaction volume. Visuals can be supplied by a derived BP. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gu|World")
    TObjectPtr<USphereComponent> InteractionBounds;

    UPROPERTY(ReplicatedUsing = OnRep_GuBinding, VisibleInstanceOnly, BlueprintReadOnly, Category = "Gu|World")
    FGuid GuEntityId;

    UPROPERTY(ReplicatedUsing = OnRep_GuBinding, VisibleInstanceOnly, BlueprintReadOnly, Category = "Gu|World")
    FName DefinitionId = NAME_None;

    UPROPERTY(ReplicatedUsing = OnRep_GuBinding, VisibleInstanceOnly, BlueprintReadOnly, Category = "Gu|World")
    FName RegionId = NAME_None;

    UPROPERTY(ReplicatedUsing = OnRep_GuBinding, VisibleInstanceOnly, BlueprintReadOnly, Category = "Gu|World")
    int32 SpawnSeed = 0;

    UFUNCTION()
    void OnRep_GuBinding();

    /** Hook for BP visuals, idle behaviour and species-specific presentation. */
    UFUNCTION(BlueprintImplementableEvent, Category = "Gu|World", meta = (DisplayName = "World Gu Binding Changed"))
    void K2_OnWorldGuBindingChanged();
};
