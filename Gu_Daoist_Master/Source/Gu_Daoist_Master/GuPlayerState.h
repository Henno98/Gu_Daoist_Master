#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "MentalResourceComponent.h"
#include "RefinementTypes.h"
#include "KillerMoveTypes.h"
#include "GuPlayerState.generated.h"


/** Owner-only public projection of one physical aperture Gu.
 *  The authoritative ECS entity remains in UGuEntitySubsystem on the server.
 *  This compact projection exists so remote owning clients can render/select their Gu
 *  without duplicating the authoritative ECS into a client GameInstance subsystem.
 */
USTRUCT(BlueprintType)
struct FGuPublicInventoryEntry
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) FGuid EntityId;
    UPROPERTY(BlueprintReadOnly) FName DefinitionId;
    UPROPERTY(BlueprintReadOnly) FString Name;
    UPROPERTY(BlueprintReadOnly) int32 Rank = 1;
    UPROPERTY(BlueprintReadOnly) FName Path;
    UPROPERTY(BlueprintReadOnly) bool bAlive = true;
    UPROPERTY(BlueprintReadOnly) float Durability = 100.0f;
    UPROPERTY(BlueprintReadOnly) float Quality = 1.0f;
    UPROPERTY(BlueprintReadOnly) int32 ActivationCount = 0;
    UPROPERTY(BlueprintReadOnly) float Hunger = 100.0f;
    UPROPERTY(BlueprintReadOnly) FName FoodKey = TEXT("food");
    UPROPERTY(BlueprintReadOnly) float FeedingIntervalHours = 24.0f;
    UPROPERTY(BlueprintReadOnly) int32 RemainingCharges = -1;

    /** Preformatted owner-safe semantic summary for clients that do not own the server ECS/registry. */
    UPROPERTY(BlueprintReadOnly) FString SemanticsSummary;
};

/**
 * Persistent domain state that should survive pawn replacement.
 *
 * Primeval essence remains in the existing GAS AttributeSet on the character.
 * This PlayerState owns only domain identity, cultivation rank, mental resources,
 * and the player-safe replicated refinement projection.
 */
UCLASS()
class GU_DAOIST_MASTER_API AGuPlayerState : public APlayerState
{
    GENERATED_BODY()

public:
    AGuPlayerState();

    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Gu|Cultivation")
    TObjectPtr<UMentalResourceComponent> MentalResources;

    /** Cultivation rank used by mental/focus rules. GAS still owns essence. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, SaveGame, Category="Gu|Cultivation", meta=(ClampMin="1", ClampMax="9"))
    int32 CultivationRank = 1;

    /** Stable domain/server character identifier, independent of the transient pawn. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_DomainCharacterId, Category="Gu|Persistence")
    FString DomainCharacterId;

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Gu|Persistence")
    void SetDomainCharacterId(const FString& NewCharacterId);

    /** Owner-only aperture projection. Server ECS remains authoritative. */
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_OwnedGuInventory, Category="Gu|Inventory")
    TArray<FGuPublicInventoryEntry> OwnedGuInventory;

    /** Physical Gu selected by the player for the ordinary Activate Gu input. */
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_ActiveGuEntityId, Category="Gu|Inventory")
    FGuid ActiveGuEntityId;

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Gu|Inventory")
    void SetOwnedGuInventory(const TArray<FGuPublicInventoryEntry>& NewInventory);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Gu|Inventory")
    void SetActiveGuEntityId(FGuid NewActiveGuEntityId);

    const FGuPublicInventoryEntry* FindPublicGu(FGuid EntityId) const;

    /** Player-safe refinement projection. Hidden procedure/semantic state remains server-only. */
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_RefinementPublicState, Category="Gu|Refinement")
    FRefinementPublicState RefinementPublicState;

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Gu|Refinement")
    void SetRefinementPublicState(const FRefinementPublicState& NewState);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Gu|Refinement")
    void ClearRefinementPublicState();

    /** Player-safe killer-move timing projection. Physical Gu bindings remain server-only. */
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_KillerMovePublicState, Category="Gu|Killer Move")
    FKillerMovePublicState KillerMovePublicState;

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Gu|Killer Move")
    void SetKillerMovePublicState(const FKillerMovePublicState& NewState);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Gu|Killer Move")
    void ClearKillerMovePublicState();

private:
    UFUNCTION()
    void OnRep_DomainCharacterId();

    UFUNCTION()
    void OnRep_OwnedGuInventory();

    UFUNCTION()
    void OnRep_ActiveGuEntityId();

    UFUNCTION()
    void OnRep_RefinementPublicState();

    UFUNCTION()
    void OnRep_KillerMovePublicState();

    void PropagateDomainCharacterId();
};
