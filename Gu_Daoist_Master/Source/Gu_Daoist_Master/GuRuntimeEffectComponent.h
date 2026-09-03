#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GuRuntimeEffectComponent.generated.h"

class UAbilitySystemComponent;
class UMentalResourceComponent;

/**
 * Transient world-state produced by Gu mechanics.
 *
 * The physical Gu itself still lives in the domain ECS. This component stores
 * effects currently acting on a spawned Actor: shields, restrictions, periodic
 * damage/healing, essence generation, concealment, marks, suppression, etc.
 */
UCLASS(ClassGroup=(Gu), meta=(BlueprintSpawnableComponent))
class GU_DAOIST_MASTER_API UGuRuntimeEffectComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UGuRuntimeEffectComponent();

    static UGuRuntimeEffectComponent* FindOrCreate(AActor* Actor);

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    void AddShield(UAbilitySystemComponent* ASC, float Amount, float DurationSeconds);
    float AbsorbDamage(UAbilitySystemComponent* ASC, float IncomingDamage);

    /** bHarmful distinguishes restrictions from speed buffs so cleanse/dispel can remove the right layers. */
    void AddMovementMultiplier(float Multiplier, float DurationSeconds, bool bHarmful = false);

    void AddPeriodicHealth(
        UAbilitySystemComponent* SourceASC,
        UAbilitySystemComponent* TargetASC,
        float HealthDeltaPerTick,
        float TickIntervalSeconds,
        float DurationSeconds);

    void AddEssenceRegeneration(
        UAbilitySystemComponent* ASC,
        float FlatPerSecond,
        float PercentOfMaximumPerSecond,
        float DurationSeconds);

    void ApplyConcealment(float Opacity, float DetectionResistance, float DurationSeconds, bool bBreakOnAttack);
    void BreakConcealmentOnAttack();

    void ApplyReveal(float Strength, float Range, float DurationSeconds);

    void ApplyGuSuppression(float DurationSeconds);

    UFUNCTION(BlueprintPure, Category="Gu|Effects")
    bool IsGuSuppressed() const;

    void ApplyMark(FName MarkId, float Strength, float DurationSeconds);

    UFUNCTION(BlueprintPure, Category="Gu|Effects")
    bool HasMark(FName MarkId) const;

    UFUNCTION(BlueprintPure, Category="Gu|Effects")
    float GetMarkStrength(FName MarkId) const;

    /** Removes hostile runtime layers created by Gu mechanics. */
    void CleanseHarmful(
        UAbilitySystemComponent* ASC,
        bool bRemoveDamageOverTime,
        bool bRemoveRestrictions,
        bool bRemoveGuSuppression,
        bool bRemoveMarks);

    /** Removes beneficial runtime layers created by Gu mechanics. */
    void DispelBeneficial(
        UAbilitySystemComponent* ASC,
        bool bRemoveShields,
        bool bRemoveMovementBuffs,
        bool bRemoveHealingOverTime,
        bool bRemoveEssenceRegeneration,
        bool bRemoveConcealment,
        bool bRemoveReveal);

    /** Temporary non-ECS attention grant, useful for short-duration multitasking Gu. */
    void AddAttentionBoost(UMentalResourceComponent* Mental, FName Key, int32 SlotsGranted, float DurationSeconds);

    UFUNCTION(BlueprintPure, Category="Gu|Effects")
    bool IsConcealed() const { return ConcealmentEndTime > GetCurrentTime(); }

    UFUNCTION(BlueprintPure, Category="Gu|Effects")
    float GetConcealmentOpacity() const { return IsConcealed() ? ConcealmentOpacity : 1.0f; }

    UFUNCTION(BlueprintPure, Category="Gu|Effects")
    float GetDetectionResistance() const { return IsConcealed() ? DetectionResistance : 0.0f; }

    UFUNCTION(BlueprintPure, Category="Gu|Effects")
    bool IsRevealing() const { return RevealEndTime > GetCurrentTime(); }

    UFUNCTION(BlueprintPure, Category="Gu|Effects")
    float GetRevealRange() const { return IsRevealing() ? RevealRange : 0.0f; }

    UFUNCTION(BlueprintPure, Category="Gu|Effects")
    float GetRevealStrength() const { return IsRevealing() ? RevealStrength : 0.0f; }

private:
    struct FShieldLayer
    {
        FGuid Id;
        float Remaining = 0.0f;
        double ExpiresAt = 0.0;
    };

    struct FMovementLayer
    {
        FGuid Id;
        float Multiplier = 1.0f;
        bool bHarmful = false;
        double ExpiresAt = 0.0;
    };

    struct FPeriodicHealthLayer
    {
        FGuid Id;
        float DeltaPerTick = 0.0f;
        float TickInterval = 0.5f;
        double NextTickAt = 0.0;
        double ExpiresAt = 0.0;
        TWeakObjectPtr<UAbilitySystemComponent> SourceASC;
        TWeakObjectPtr<UAbilitySystemComponent> TargetASC;
    };

    struct FEssenceRegenerationLayer
    {
        FGuid Id;
        float FlatPerSecond = 0.0f;
        float PercentOfMaximumPerSecond = 0.0f;
        double ExpiresAt = 0.0;
        TWeakObjectPtr<UAbilitySystemComponent> ASC;
    };

    struct FSuppressionLayer
    {
        FGuid Id;
        double ExpiresAt = 0.0;
    };

    struct FMarkLayer
    {
        FName MarkId = NAME_None;
        float Strength = 0.0f;
        double ExpiresAt = 0.0;
    };

    struct FAttentionBoostLayer
    {
        FName Key = NAME_None;
        int32 Slots = 0;
        double ExpiresAt = 0.0;
        TWeakObjectPtr<UMentalResourceComponent> Mental;
    };

    double GetCurrentTime() const;
    void RefreshShieldAttribute(UAbilitySystemComponent* ASC);
    void RefreshMovement();
    void RefreshSuppressionTag();
    void RefreshTickState();
    void ApplyPeriodicHealthTick(FPeriodicHealthLayer& Layer);
    void RemoveMarkTag(FName MarkId);

    TArray<FShieldLayer> ShieldLayers;
    TArray<FMovementLayer> MovementLayers;
    TArray<FPeriodicHealthLayer> PeriodicHealthLayers;
    TArray<FEssenceRegenerationLayer> EssenceRegenerationLayers;
    TArray<FSuppressionLayer> SuppressionLayers;
    TArray<FMarkLayer> MarkLayers;
    TArray<FAttentionBoostLayer> AttentionBoostLayers;

    TWeakObjectPtr<UAbilitySystemComponent> ShieldASC;
    float BaseWalkSpeed = 0.0f;

    float ConcealmentOpacity = 1.0f;
    float DetectionResistance = 0.0f;
    bool bConcealmentBreakOnAttack = true;
    double ConcealmentEndTime = 0.0;

    float RevealStrength = 0.0f;
    float RevealRange = 0.0f;
    double RevealEndTime = 0.0;
};
