#include "GuRuntimeEffectComponent.h"

#include "AS_GuMasterAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "MentalResourceComponent.h"

UGuRuntimeEffectComponent::UGuRuntimeEffectComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
}

UGuRuntimeEffectComponent* UGuRuntimeEffectComponent::FindOrCreate(AActor* Actor)
{
    if (!IsValid(Actor)) return nullptr;
    if (UGuRuntimeEffectComponent* Existing = Actor->FindComponentByClass<UGuRuntimeEffectComponent>())
    {
        return Existing;
    }

    UGuRuntimeEffectComponent* Created = NewObject<UGuRuntimeEffectComponent>(Actor, FName(TEXT("GuRuntimeEffects")));
    if (!Created) return nullptr;
    Actor->AddInstanceComponent(Created);
    Created->RegisterComponent();
    return Created;
}

double UGuRuntimeEffectComponent::GetCurrentTime() const
{
    return GetWorld() ? static_cast<double>(GetWorld()->GetTimeSeconds()) : 0.0;
}

void UGuRuntimeEffectComponent::AddShield(UAbilitySystemComponent* ASC, const float Amount, const float DurationSeconds)
{
    if (!ASC || Amount <= 0.0f) return;

    FShieldLayer Layer;
    Layer.Id = FGuid::NewGuid();
    Layer.Remaining = Amount;
    Layer.ExpiresAt = DurationSeconds > 0.0f ? GetCurrentTime() + DurationSeconds : 0.0;
    ShieldLayers.Add(Layer);
    ShieldASC = ASC;
    RefreshShieldAttribute(ASC);
    RefreshTickState();
}

float UGuRuntimeEffectComponent::AbsorbDamage(UAbilitySystemComponent* ASC, const float IncomingDamage)
{
    if (!ASC || IncomingDamage <= 0.0f || ShieldLayers.IsEmpty()) return FMath::Max(0.0f, IncomingDamage);

    float RemainingDamage = IncomingDamage;
    for (int32 Index = ShieldLayers.Num() - 1; Index >= 0 && RemainingDamage > KINDA_SMALL_NUMBER; --Index)
    {
        FShieldLayer& Layer = ShieldLayers[Index];
        const float Absorbed = FMath::Min(Layer.Remaining, RemainingDamage);
        Layer.Remaining -= Absorbed;
        RemainingDamage -= Absorbed;
        if (Layer.Remaining <= KINDA_SMALL_NUMBER) ShieldLayers.RemoveAt(Index);
    }

    RefreshShieldAttribute(ASC);
    RefreshTickState();
    return FMath::Max(0.0f, RemainingDamage);
}

void UGuRuntimeEffectComponent::RefreshShieldAttribute(UAbilitySystemComponent* ASC)
{
    if (!ASC) return;
    float Total = 0.0f;
    for (const FShieldLayer& Layer : ShieldLayers) Total += FMath::Max(0.0f, Layer.Remaining);
    ASC->SetNumericAttributeBase(UAS_GuMasterAttributeSet::GetShieldAttribute(), Total);
}

void UGuRuntimeEffectComponent::AddMovementMultiplier(
    const float Multiplier,
    const float DurationSeconds,
    const bool bHarmful)
{
    ACharacter* Character = Cast<ACharacter>(GetOwner());
    UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
    if (!Movement || Multiplier < 0.0f) return;

    if (MovementLayers.IsEmpty()) BaseWalkSpeed = Movement->MaxWalkSpeed;

    FMovementLayer Layer;
    Layer.Id = FGuid::NewGuid();
    Layer.Multiplier = Multiplier;
    Layer.bHarmful = bHarmful;
    Layer.ExpiresAt = DurationSeconds > 0.0f ? GetCurrentTime() + DurationSeconds : 0.0;
    MovementLayers.Add(Layer);
    RefreshMovement();
    RefreshTickState();
}

void UGuRuntimeEffectComponent::RefreshMovement()
{
    ACharacter* Character = Cast<ACharacter>(GetOwner());
    UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
    if (!Movement) return;

    if (MovementLayers.IsEmpty())
    {
        if (BaseWalkSpeed > 0.0f) Movement->MaxWalkSpeed = BaseWalkSpeed;
        return;
    }

    float Product = 1.0f;
    for (const FMovementLayer& Layer : MovementLayers) Product *= FMath::Max(0.0f, Layer.Multiplier);
    Movement->MaxWalkSpeed = FMath::Max(0.0f, BaseWalkSpeed * Product);
}

void UGuRuntimeEffectComponent::AddPeriodicHealth(
    UAbilitySystemComponent* SourceASC,
    UAbilitySystemComponent* TargetASC,
    const float HealthDeltaPerTick,
    const float TickIntervalSeconds,
    const float DurationSeconds)
{
    if (!TargetASC || FMath::IsNearlyZero(HealthDeltaPerTick)) return;

    FPeriodicHealthLayer Layer;
    Layer.Id = FGuid::NewGuid();
    Layer.DeltaPerTick = HealthDeltaPerTick;
    Layer.TickInterval = FMath::Max(0.02f, TickIntervalSeconds);
    Layer.NextTickAt = GetCurrentTime() + Layer.TickInterval;
    Layer.ExpiresAt = DurationSeconds > 0.0f ? GetCurrentTime() + DurationSeconds : TNumericLimits<double>::Max();
    Layer.SourceASC = SourceASC;
    Layer.TargetASC = TargetASC;
    PeriodicHealthLayers.Add(MoveTemp(Layer));
    RefreshTickState();
}

void UGuRuntimeEffectComponent::ApplyPeriodicHealthTick(FPeriodicHealthLayer& Layer)
{
    UAbilitySystemComponent* TargetASC = Layer.TargetASC.Get();
    if (!TargetASC) return;

    const FGameplayAttribute HealthAttribute = UAS_GuMasterAttributeSet::GetHealthAttribute();
    const FGameplayAttribute MaxHealthAttribute = UAS_GuMasterAttributeSet::GetMaxHealthAttribute();
    const float Current = TargetASC->GetNumericAttribute(HealthAttribute);
    const float Maximum = FMath::Max(0.0f, TargetASC->GetNumericAttribute(MaxHealthAttribute));

    if (Layer.DeltaPerTick > 0.0f)
    {
        TargetASC->SetNumericAttributeBase(HealthAttribute, FMath::Clamp(Current + Layer.DeltaPerTick, 0.0f, Maximum));
        return;
    }

    float Damage = -Layer.DeltaPerTick;
    Damage = AbsorbDamage(TargetASC, Damage);
    if (Damage > KINDA_SMALL_NUMBER)
    {
        TargetASC->SetNumericAttributeBase(HealthAttribute, FMath::Max(0.0f, Current - Damage));
    }
}

void UGuRuntimeEffectComponent::AddEssenceRegeneration(
    UAbilitySystemComponent* ASC,
    const float FlatPerSecond,
    const float PercentOfMaximumPerSecond,
    const float DurationSeconds)
{
    if (!ASC || (FlatPerSecond <= 0.0f && PercentOfMaximumPerSecond <= 0.0f)) return;

    FEssenceRegenerationLayer Layer;
    Layer.Id = FGuid::NewGuid();
    Layer.FlatPerSecond = FMath::Max(0.0f, FlatPerSecond);
    Layer.PercentOfMaximumPerSecond = FMath::Max(0.0f, PercentOfMaximumPerSecond);
    Layer.ExpiresAt = DurationSeconds > 0.0f ? GetCurrentTime() + DurationSeconds : TNumericLimits<double>::Max();
    Layer.ASC = ASC;
    EssenceRegenerationLayers.Add(MoveTemp(Layer));
    RefreshTickState();
}

void UGuRuntimeEffectComponent::ApplyConcealment(
    const float Opacity,
    const float InDetectionResistance,
    const float DurationSeconds,
    const bool bBreakOnAttack)
{
    ConcealmentOpacity = FMath::Clamp(Opacity, 0.0f, 1.0f);
    DetectionResistance = FMath::Clamp(InDetectionResistance, 0.0f, 0.95f);
    bConcealmentBreakOnAttack = bBreakOnAttack;
    ConcealmentEndTime = DurationSeconds > 0.0f ? GetCurrentTime() + DurationSeconds : TNumericLimits<double>::Max();
    if (AActor* OwnerActor = GetOwner()) OwnerActor->Tags.AddUnique(FName(TEXT("Gu.Concealed")));
    RefreshTickState();
}

void UGuRuntimeEffectComponent::BreakConcealmentOnAttack()
{
    if (!IsConcealed() || !bConcealmentBreakOnAttack) return;
    ConcealmentEndTime = 0.0;
    ConcealmentOpacity = 1.0f;
    DetectionResistance = 0.0f;
    if (AActor* OwnerActor = GetOwner()) OwnerActor->Tags.Remove(FName(TEXT("Gu.Concealed")));
    RefreshTickState();
}

void UGuRuntimeEffectComponent::ApplyReveal(const float Strength, const float Range, const float DurationSeconds)
{
    RevealStrength = FMath::Max(0.0f, Strength);
    RevealRange = FMath::Max(0.0f, Range);
    RevealEndTime = DurationSeconds > 0.0f ? GetCurrentTime() + DurationSeconds : TNumericLimits<double>::Max();
    if (AActor* OwnerActor = GetOwner()) OwnerActor->Tags.AddUnique(FName(TEXT("Gu.Revealing")));
    RefreshTickState();
}

void UGuRuntimeEffectComponent::ApplyGuSuppression(const float DurationSeconds)
{
    FSuppressionLayer Layer;
    Layer.Id = FGuid::NewGuid();
    Layer.ExpiresAt = DurationSeconds > 0.0f ? GetCurrentTime() + DurationSeconds : TNumericLimits<double>::Max();
    SuppressionLayers.Add(Layer);
    RefreshSuppressionTag();
    RefreshTickState();
}

bool UGuRuntimeEffectComponent::IsGuSuppressed() const
{
    const double Now = GetCurrentTime();
    for (const FSuppressionLayer& Layer : SuppressionLayers)
    {
        if (Layer.ExpiresAt > Now) return true;
    }
    return false;
}

void UGuRuntimeEffectComponent::RefreshSuppressionTag()
{
    AActor* OwnerActor = GetOwner();
    if (!OwnerActor) return;
    const FName Tag(TEXT("Gu.Suppressed"));
    if (IsGuSuppressed()) OwnerActor->Tags.AddUnique(Tag);
    else OwnerActor->Tags.Remove(Tag);
}

void UGuRuntimeEffectComponent::ApplyMark(const FName MarkId, const float Strength, const float DurationSeconds)
{
    if (MarkId.IsNone()) return;

    // Keep applications as independent layers. Merging by Max(Strength) and
    // Max(Duration) can accidentally create a mark that never actually existed,
    // e.g. a strong short mark becoming strong for the lifetime of a weak long mark.
    FMarkLayer Layer;
    Layer.MarkId = MarkId;
    Layer.Strength = FMath::Max(0.0f, Strength);
    Layer.ExpiresAt = DurationSeconds > 0.0f
        ? GetCurrentTime() + DurationSeconds
        : TNumericLimits<double>::Max();
    MarkLayers.Add(MoveTemp(Layer));

    if (AActor* OwnerActor = GetOwner())
    {
        OwnerActor->Tags.AddUnique(FName(*FString::Printf(TEXT("Gu.Mark.%s"), *MarkId.ToString())));
    }
    RefreshTickState();
}

bool UGuRuntimeEffectComponent::HasMark(const FName MarkId) const
{
    const double Now = GetCurrentTime();
    return MarkLayers.ContainsByPredicate([MarkId, Now](const FMarkLayer& Layer)
    {
        return Layer.MarkId == MarkId && Layer.ExpiresAt > Now;
    });
}

float UGuRuntimeEffectComponent::GetMarkStrength(const FName MarkId) const
{
    const double Now = GetCurrentTime();
    float Strength = 0.0f;
    for (const FMarkLayer& Layer : MarkLayers)
    {
        if (Layer.MarkId == MarkId && Layer.ExpiresAt > Now) Strength = FMath::Max(Strength, Layer.Strength);
    }
    return Strength;
}

void UGuRuntimeEffectComponent::RemoveMarkTag(const FName MarkId)
{
    if (MarkId.IsNone()) return;
    if (AActor* OwnerActor = GetOwner())
    {
        OwnerActor->Tags.Remove(FName(*FString::Printf(TEXT("Gu.Mark.%s"), *MarkId.ToString())));
    }
}

void UGuRuntimeEffectComponent::CleanseHarmful(
    UAbilitySystemComponent* ASC,
    const bool bRemoveDamageOverTime,
    const bool bRemoveRestrictions,
    const bool bRemoveGuSuppression,
    const bool bRemoveMarks)
{
    if (bRemoveDamageOverTime)
    {
        PeriodicHealthLayers.RemoveAll([](const FPeriodicHealthLayer& Layer){ return Layer.DeltaPerTick < 0.0f; });
    }

    if (bRemoveRestrictions)
    {
        const int32 Removed = MovementLayers.RemoveAll([](const FMovementLayer& Layer){ return Layer.bHarmful; });
        if (Removed > 0) RefreshMovement();
    }

    if (bRemoveGuSuppression)
    {
        SuppressionLayers.Reset();
        RefreshSuppressionTag();
    }

    if (bRemoveMarks)
    {
        for (const FMarkLayer& Layer : MarkLayers) RemoveMarkTag(Layer.MarkId);
        MarkLayers.Reset();
    }

    if (ASC) RefreshShieldAttribute(ASC);
    RefreshTickState();
}

void UGuRuntimeEffectComponent::DispelBeneficial(
    UAbilitySystemComponent* ASC,
    const bool bRemoveShields,
    const bool bRemoveMovementBuffs,
    const bool bRemoveHealingOverTime,
    const bool bRemoveEssenceRegeneration,
    const bool bRemoveConcealment,
    const bool bRemoveReveal)
{
    if (bRemoveShields)
    {
        ShieldLayers.Reset();
        RefreshShieldAttribute(ASC ? ASC : ShieldASC.Get());
    }

    if (bRemoveMovementBuffs)
    {
        const int32 Removed = MovementLayers.RemoveAll([](const FMovementLayer& Layer){ return !Layer.bHarmful; });
        if (Removed > 0) RefreshMovement();
    }

    if (bRemoveHealingOverTime)
    {
        PeriodicHealthLayers.RemoveAll([](const FPeriodicHealthLayer& Layer){ return Layer.DeltaPerTick > 0.0f; });
    }

    if (bRemoveEssenceRegeneration) EssenceRegenerationLayers.Reset();

    if (bRemoveConcealment)
    {
        ConcealmentEndTime = 0.0;
        ConcealmentOpacity = 1.0f;
        DetectionResistance = 0.0f;
        if (AActor* OwnerActor = GetOwner()) OwnerActor->Tags.Remove(FName(TEXT("Gu.Concealed")));
    }

    if (bRemoveReveal)
    {
        RevealEndTime = 0.0;
        RevealStrength = 0.0f;
        RevealRange = 0.0f;
        if (AActor* OwnerActor = GetOwner()) OwnerActor->Tags.Remove(FName(TEXT("Gu.Revealing")));
    }

    RefreshTickState();
}

void UGuRuntimeEffectComponent::AddAttentionBoost(
    UMentalResourceComponent* Mental,
    const FName Key,
    const int32 SlotsGranted,
    const float DurationSeconds)
{
    if (!Mental || Key.IsNone() || SlotsGranted <= 0 || DurationSeconds <= 0.0f) return;

    for (int32 Index = AttentionBoostLayers.Num() - 1; Index >= 0; --Index)
    {
        if (AttentionBoostLayers[Index].Key == Key)
        {
            if (UMentalResourceComponent* OldMental = AttentionBoostLayers[Index].Mental.Get())
            {
                OldMental->SetTemporaryAttentionGrant(Key, 0);
            }
            AttentionBoostLayers.RemoveAt(Index);
        }
    }

    Mental->SetTemporaryAttentionGrant(Key, SlotsGranted);
    FAttentionBoostLayer Layer;
    Layer.Key = Key;
    Layer.Slots = SlotsGranted;
    Layer.ExpiresAt = GetCurrentTime() + DurationSeconds;
    Layer.Mental = Mental;
    AttentionBoostLayers.Add(MoveTemp(Layer));
    RefreshTickState();
}

void UGuRuntimeEffectComponent::RefreshTickState()
{
    bool bNeedsTick = !PeriodicHealthLayers.IsEmpty() || !EssenceRegenerationLayers.IsEmpty() || !AttentionBoostLayers.IsEmpty();
    for (const FShieldLayer& Layer : ShieldLayers) bNeedsTick |= Layer.ExpiresAt > 0.0;
    for (const FMovementLayer& Layer : MovementLayers) bNeedsTick |= Layer.ExpiresAt > 0.0;
    for (const FSuppressionLayer& Layer : SuppressionLayers)
    {
        bNeedsTick |= Layer.ExpiresAt > 0.0 && Layer.ExpiresAt < TNumericLimits<double>::Max();
    }
    for (const FMarkLayer& Layer : MarkLayers)
    {
        bNeedsTick |= Layer.ExpiresAt > 0.0 && Layer.ExpiresAt < TNumericLimits<double>::Max();
    }
    bNeedsTick |= ConcealmentEndTime > 0.0 && ConcealmentEndTime < TNumericLimits<double>::Max();
    bNeedsTick |= RevealEndTime > 0.0 && RevealEndTime < TNumericLimits<double>::Max();
    SetComponentTickEnabled(bNeedsTick);
}

void UGuRuntimeEffectComponent::TickComponent(
    const float DeltaTime,
    const ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    const double Now = GetCurrentTime();

    bool bShieldChanged = false;
    for (int32 Index = ShieldLayers.Num() - 1; Index >= 0; --Index)
    {
        if (ShieldLayers[Index].ExpiresAt > 0.0 && ShieldLayers[Index].ExpiresAt <= Now)
        {
            ShieldLayers.RemoveAt(Index);
            bShieldChanged = true;
        }
    }
    if (bShieldChanged) RefreshShieldAttribute(ShieldASC.Get());

    bool bMovementChanged = false;
    for (int32 Index = MovementLayers.Num() - 1; Index >= 0; --Index)
    {
        if (MovementLayers[Index].ExpiresAt > 0.0 && MovementLayers[Index].ExpiresAt <= Now)
        {
            MovementLayers.RemoveAt(Index);
            bMovementChanged = true;
        }
    }
    if (bMovementChanged) RefreshMovement();

    for (int32 Index = PeriodicHealthLayers.Num() - 1; Index >= 0; --Index)
    {
        FPeriodicHealthLayer& Layer = PeriodicHealthLayers[Index];
        if (!Layer.TargetASC.IsValid() || Layer.ExpiresAt <= Now)
        {
            PeriodicHealthLayers.RemoveAt(Index);
            continue;
        }

        int32 Safety = 0;
        while (Layer.NextTickAt <= Now && Layer.ExpiresAt > Layer.NextTickAt && Safety++ < 8)
        {
            ApplyPeriodicHealthTick(Layer);
            Layer.NextTickAt += Layer.TickInterval;
        }
    }

    for (int32 Index = EssenceRegenerationLayers.Num() - 1; Index >= 0; --Index)
    {
        FEssenceRegenerationLayer& Layer = EssenceRegenerationLayers[Index];
        UAbilitySystemComponent* ASC = Layer.ASC.Get();
        if (!ASC || Layer.ExpiresAt <= Now)
        {
            EssenceRegenerationLayers.RemoveAt(Index);
            continue;
        }

        const FGameplayAttribute EssenceAttribute = UAS_GuMasterAttributeSet::GetPrimevalEssenceAttribute();
        const FGameplayAttribute MaxEssenceAttribute = UAS_GuMasterAttributeSet::GetMaxPrimevalEssenceAttribute();
        const float Current = ASC->GetNumericAttribute(EssenceAttribute);
        const float Maximum = FMath::Max(0.0f, ASC->GetNumericAttribute(MaxEssenceAttribute));
        const float PerSecond = Layer.FlatPerSecond + Maximum * (Layer.PercentOfMaximumPerSecond / 100.0f);
        if (PerSecond > 0.0f)
        {
            ASC->SetNumericAttributeBase(EssenceAttribute, FMath::Clamp(Current + PerSecond * DeltaTime, 0.0f, Maximum));
        }
    }

    bool bSuppressionChanged = false;
    for (int32 Index = SuppressionLayers.Num() - 1; Index >= 0; --Index)
    {
        if (SuppressionLayers[Index].ExpiresAt <= Now)
        {
            SuppressionLayers.RemoveAt(Index);
            bSuppressionChanged = true;
        }
    }
    if (bSuppressionChanged) RefreshSuppressionTag();

    for (int32 Index = MarkLayers.Num() - 1; Index >= 0; --Index)
    {
        if (MarkLayers[Index].ExpiresAt <= Now)
        {
            const FName ExpiredMark = MarkLayers[Index].MarkId;
            MarkLayers.RemoveAt(Index);
            if (!HasMark(ExpiredMark)) RemoveMarkTag(ExpiredMark);
        }
    }

    for (int32 Index = AttentionBoostLayers.Num() - 1; Index >= 0; --Index)
    {
        if (AttentionBoostLayers[Index].ExpiresAt <= Now)
        {
            const FName Key = AttentionBoostLayers[Index].Key;
            if (UMentalResourceComponent* Mental = AttentionBoostLayers[Index].Mental.Get())
            {
                Mental->SetTemporaryAttentionGrant(Key, 0);
            }
            AttentionBoostLayers.RemoveAt(Index);
        }
    }

    if (ConcealmentEndTime > 0.0 && ConcealmentEndTime < TNumericLimits<double>::Max() && ConcealmentEndTime <= Now)
    {
        ConcealmentEndTime = 0.0;
        ConcealmentOpacity = 1.0f;
        DetectionResistance = 0.0f;
        if (AActor* OwnerActor = GetOwner()) OwnerActor->Tags.Remove(FName(TEXT("Gu.Concealed")));
    }

    if (RevealEndTime > 0.0 && RevealEndTime < TNumericLimits<double>::Max() && RevealEndTime <= Now)
    {
        RevealEndTime = 0.0;
        RevealStrength = 0.0f;
        RevealRange = 0.0f;
        if (AActor* OwnerActor = GetOwner()) OwnerActor->Tags.Remove(FName(TEXT("Gu.Revealing")));
    }

    RefreshTickState();
}
