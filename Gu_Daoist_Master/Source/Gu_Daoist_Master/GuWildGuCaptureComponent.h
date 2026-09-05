#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GuDefinitionTypes.h"
#include "GuWildGuCaptureComponent.generated.h"

class AGuPlayerState;
class AWildGuWorldActor;

/**
 * Player-owned network facade for physically capturing wild Gu.
 *
 * Capture does NOT refine the Gu. A captured Gu keeps the same physical FGuid,
 * leaves the world/ecology projection, enters EGuContainer::Captured, and remains
 * unusable until its will has been refined.
 *
 * Add this component to the player Character/Pawn Blueprint. Blueprint decides how
 * interaction, animations, tools, restraint conditions, etc. are presented.
 */
UCLASS(BlueprintType, Blueprintable, ClassGroup=(Gu), meta=(BlueprintSpawnableComponent))
class GU_DAOIST_MASTER_API UGuWildGuCaptureComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UGuWildGuCaptureComponent();

    /** Maximum distance for the simple native physical-capture gate. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gu|Capture", meta=(ClampMin="10.0"))
    float CaptureRangeCm = 200.0f;

    /** Last Gu confirmed captured on this local owning client. */
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Gu|Capture")
    FGuid LastCapturedEntityId;

    /** Capture a specific world projection. Safe to call from the owning client. */
    UFUNCTION(BlueprintCallable, Category="Gu|Capture")
    bool RequestCaptureWildGu(AWildGuWorldActor* Target);

    /** Convenience test/interaction helper: captures the nearest wild Gu inside CaptureRangeCm. */
    UFUNCTION(BlueprintCallable, Category="Gu|Capture")
    bool RequestCaptureNearestWildGu();

    /**
     * Blueprint extension point for nets, traps, stun states, special capture tools, etc.
     * Native code always enforces range first; this hook can only add requirements.
     * It is evaluated again on the server, so a client cannot bypass it.
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Gu|Capture", meta=(DisplayName="Can Physically Capture Wild Gu"))
    bool CanPhysicallyCaptureWildGu(AWildGuWorldActor* Target, FString& OutReason) const;
    virtual bool CanPhysicallyCaptureWildGu_Implementation(AWildGuWorldActor* Target, FString& OutReason) const;

    /**
     * DEVELOPMENT TEST SEAM ONLY.
     * Completes will refinement for a captured Gu without using the refinement gameplay.
     */
    UFUNCTION(BlueprintCallable, Category="Gu|Capture|Debug", meta=(DevelopmentOnly))
    bool RequestDebugInstantRefineCapturedGu(
        FGuid EntityId,
        EGuContainer TargetContainer = EGuContainer::Storage);

    /** Development helper operating on LastCapturedEntityId. */
    UFUNCTION(BlueprintCallable, Category="Gu|Capture|Debug", meta=(DevelopmentOnly))
    bool RequestDebugInstantRefineLastCapturedGu(
        EGuContainer TargetContainer = EGuContainer::Storage);

protected:
    UFUNCTION(Server, Reliable)
    void ServerCaptureWildGu(AWildGuWorldActor* Target);

    UFUNCTION(Server, Reliable)
    void ServerDebugInstantRefineCapturedGu(FGuid EntityId, EGuContainer TargetContainer);

    UFUNCTION(Client, Reliable)
    void ClientCaptureSucceeded(FGuid EntityId, FName DefinitionId);

    UFUNCTION(Client, Reliable)
    void ClientCaptureFailed(const FString& Error);

    UFUNCTION(Client, Reliable)
    void ClientWillRefinementSucceeded(FGuid EntityId);

    UFUNCTION(Client, Reliable)
    void ClientWillRefinementFailed(const FString& Error);

    UFUNCTION(BlueprintImplementableEvent, Category="Gu|Capture", meta=(DisplayName="Wild Gu Captured"))
    void K2_OnWildGuCaptured(FGuid EntityId, FName DefinitionId);

    UFUNCTION(BlueprintImplementableEvent, Category="Gu|Capture", meta=(DisplayName="Wild Gu Capture Failed"))
    void K2_OnWildGuCaptureFailed(const FString& Error);

    UFUNCTION(BlueprintImplementableEvent, Category="Gu|Capture", meta=(DisplayName="Wild Gu Will Refined"))
    void K2_OnWildGuWillRefined(FGuid EntityId);

    UFUNCTION(BlueprintImplementableEvent, Category="Gu|Capture", meta=(DisplayName="Wild Gu Will Refinement Failed"))
    void K2_OnWildGuWillRefinementFailed(const FString& Error);

private:
    AGuPlayerState* ResolveGuPlayerState() const;
    AWildGuWorldActor* FindNearestCaptureTarget() const;
    bool ValidateCaptureTarget(AWildGuWorldActor* Target, FString& OutError) const;
    bool ExecuteCaptureOnAuthority(AWildGuWorldActor* Target, FString& OutError);
    bool ExecuteInstantRefineOnAuthority(FGuid EntityId, EGuContainer TargetContainer, FString& OutError);
};
