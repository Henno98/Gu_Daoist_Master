#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "KillerMoveTypes.h"
#include "TimerManager.h"
#include "KillerMoveSubsystem.generated.h"

class AGuPlayerState;
class UKillerMoveDefinition;

/**
 * Server-authoritative killer-move activation runtime.
 *
 * A formula names Gu species/roles. Beginning a move binds those requirements to
 * actual owned ECS Gu entities. Timed input then manipulates those physical worms.
 * The typed effect graph is compiled from the participating Gu roles. A completed
 * choreography is then resolved through the existing authored Gu/GAS mechanics,
 * beginning with projectile-carried killer moves.
 */
UCLASS()
class GU_DAOIST_MASTER_API UKillerMoveSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Deinitialize() override;

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Gu|Killer Move")
    bool BeginKillerMove(AGuPlayerState* PlayerState, const FKillerMoveDefinitionRecord& Definition, FString& OutError);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Gu|Killer Move")
    bool BeginKillerMoveAsset(AGuPlayerState* PlayerState, const UKillerMoveDefinition* Definition, FString& OutError);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Gu|Killer Move")
    bool SubmitInput(AGuPlayerState* PlayerState, int32 SlotIndex, EKillerMoveInputEvent Event, FString& OutError);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Gu|Killer Move")
    bool CancelKillerMove(AGuPlayerState* PlayerState, FString& OutError);

    UFUNCTION(BlueprintPure, Category="Gu|Killer Move")
    bool HasActiveKillerMove(const FString& OwnerId) const;

    /** Development harness: uses the first owned aperture Gu, plus a second if available. */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Gu|Killer Move|Debug")
    bool BeginDebugKillerMove(AGuPlayerState* PlayerState, FString& OutError);

    /** Browser-compatible role -> graph semantics. */
    static FName BranchForRole(EKillerMoveRole Role);
    static FName RelationForRole(EKillerMoveRole Role);

    bool BuildEffectGraph(FKillerMoveDefinitionRecord& InOutDefinition, FString& OutError) const;

private:
    struct FRuntimeSession
    {
        FGuid SessionId;
        TWeakObjectPtr<AGuPlayerState> PlayerState;
        FString OwnerId;
        FKillerMoveDefinitionRecord Definition;
        TArray<FGuid> BoundGuEntities;
        TSet<int32> HeldSlotIndices;
        int32 StepIndex = 0;
        float StartedServerWorldTime = 0.0f;
        float Stability = 100.0f;
        float QualitySum = 0.0f;
        int32 AcceptedSteps = 0;
        int32 SkippedSteps = 0;
        FTimerHandle DeadlineTimer;
    };

    TMap<FString, FRuntimeSession> Sessions;

    float ServerWorldTime() const;
    float EffectiveWindow(const AGuPlayerState* PlayerState, const FKillerMoveInputStep& Step) const;
    FName AttentionKey(const FRuntimeSession& Session, int32 SlotIndex) const;
    void ScheduleDeadline(FRuntimeSession& Session);
    void HandleDeadline(FString OwnerId);
    bool AdvancePastMissedStep(FRuntimeSession& Session, FString& OutError);
    void PushPublicState(FRuntimeSession& Session, const FString& StatusText);
    void ReleaseAllAttention(FRuntimeSession& Session);
    void FinishSession(FRuntimeSession& Session, EKillerMoveRunState EndState, const FString& Message);
    void CompleteSession(FRuntimeSession& Session);
    bool ResolveCompletedEffect(FRuntimeSession& Session, float ExecutionQuality, FString& OutSummary, FString& OutError);
    bool ResolvePhysicalGu(FRuntimeSession& Session, FString& OutError);
};
