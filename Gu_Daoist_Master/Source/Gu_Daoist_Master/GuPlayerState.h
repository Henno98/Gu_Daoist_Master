#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "MentalResourceComponent.h"
#include "RefinementTypes.h"
#include "KillerMoveTypes.h"
#include "GuPlayerState.generated.h"

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
    void OnRep_RefinementPublicState();

    UFUNCTION()
    void OnRep_KillerMovePublicState();

    void PropagateDomainCharacterId();
};
