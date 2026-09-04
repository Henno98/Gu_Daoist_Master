#include "GuExecutionLibrary.h"

#include "AS_GuMasterAttributeSet.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "CollisionQueryParams.h"
#include "Engine/EngineTypes.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "GameplayEffect.h"
#include "GameplayTagContainer.h"
#include "GuEffectField.h"
#include "GuPlayerState.h"
#include "GuRuntimeEffectComponent.h"
#include "GuSystemConfig.h"
#include "MentalResourceComponent.h"
#include "UGuDefinition.h"

namespace
{
    struct FGuImpactContext
    {
        UGuDefinition* Definition = nullptr;
        UAbilitySystemComponent* SourceASC = nullptr;
        UAbilitySystemComponent* TargetASC = nullptr;
        AActor* TargetActor = nullptr;
        const FHitResult* Hit = nullptr;
        float MagnitudeScale = 1.0f;
    };

    struct FGuActivationContext
    {
        UGuDefinition* Definition = nullptr;
        UAbilitySystemComponent* SourceASC = nullptr;
        AActor* SourceActor = nullptr;
        const UGuSystemConfig* SystemConfig = nullptr;
    };

    using FImpactExecutor = bool(*)(const TInstancedStruct<FGuMechanic>&, const FGuImpactContext&);
    using FActivationExecutor = bool(*)(const TInstancedStruct<FGuMechanic>&, const FGuActivationContext&);

    bool GuExecExecutePayloadInternal(
        UGuDefinition* GuDefinition,
        UAbilitySystemComponent* SourceASC,
        AActor* TargetActor,
        const FHitResult& HitResult,
        float MagnitudeScale,
        bool bAllowChain);

    bool GuExecResolveDeliveredImpact(
        UGuDefinition* Definition,
        UAbilitySystemComponent* SourceASC,
        AActor* TargetActor,
        const FHitResult& HitResult,
        float MagnitudeScale,
        bool bAllowChain,
        bool bAllowMeleeCarrier);

    FVector GuExecEffectDirection(const AActor* Actor, const FHitResult* Hit)
    {
        if (Hit)
        {
            const FVector TraceDirection = (Hit->TraceEnd - Hit->TraceStart).GetSafeNormal();
            if (!TraceDirection.IsNearlyZero()) return TraceDirection;
        }
        return Actor ? Actor->GetActorForwardVector() : FVector::ForwardVector;
    }

    bool GuExecApplyHeal(UAbilitySystemComponent* ASC, const float Amount)
    {
        if (!ASC || Amount <= 0.0f) return false;
        const FGameplayAttribute Health = UAS_GuMasterAttributeSet::GetHealthAttribute();
        const FGameplayAttribute MaxHealth = UAS_GuMasterAttributeSet::GetMaxHealthAttribute();
        const float Current = ASC->GetNumericAttribute(Health);
        const float Maximum = ASC->GetNumericAttribute(MaxHealth);
        ASC->SetNumericAttributeBase(Health, FMath::Clamp(Current + Amount, 0.0f, FMath::Max(0.0f, Maximum)));
        return true;
    }

    bool GuExecApplyEssenceChange(
        UAbilitySystemComponent* TargetASC,
        UAbilitySystemComponent* SourceASC,
        const FGuEssenceChangeMechanic& Essence,
        const float MagnitudeScale)
    {
        if (!TargetASC || Essence.Amount <= 0.0f) return false;

        const FGameplayAttribute EssenceAttribute = UAS_GuMasterAttributeSet::GetPrimevalEssenceAttribute();
        const FGameplayAttribute MaxEssenceAttribute = UAS_GuMasterAttributeSet::GetMaxPrimevalEssenceAttribute();
        const float Current = TargetASC->GetNumericAttribute(EssenceAttribute);
        const float Maximum = FMath::Max(0.0f, TargetASC->GetNumericAttribute(MaxEssenceAttribute));
        const float BaseAmount = Essence.bPercentOfMaximum ? Maximum * Essence.Amount / 100.0f : Essence.Amount;
        const float Amount = FMath::Max(0.0f, BaseAmount * FMath::Max(0.0f, MagnitudeScale));

        if (Essence.Mode == EGuEssenceChangeMode::Restore)
        {
            TargetASC->SetNumericAttributeBase(EssenceAttribute, FMath::Clamp(Current + Amount, 0.0f, Maximum));
            return true;
        }

        const float Drained = FMath::Min(Current, Amount);
        TargetASC->SetNumericAttributeBase(EssenceAttribute, FMath::Max(0.0f, Current - Drained));
        if (Essence.bTransferDrainedEssenceToSource && SourceASC && SourceASC != TargetASC && Drained > 0.0f)
        {
            const float SourceCurrent = SourceASC->GetNumericAttribute(EssenceAttribute);
            const float SourceMax = FMath::Max(0.0f, SourceASC->GetNumericAttribute(MaxEssenceAttribute));
            SourceASC->SetNumericAttributeBase(EssenceAttribute, FMath::Clamp(SourceCurrent + Drained, 0.0f, SourceMax));
        }
        return true;
    }

    bool GuExecApplyShield(AActor* Actor, UAbilitySystemComponent* ASC, const float Amount, const float Duration)
    {
        UGuRuntimeEffectComponent* Runtime = UGuRuntimeEffectComponent::FindOrCreate(Actor);
        if (!Runtime || !ASC || Amount <= 0.0f) return false;
        Runtime->AddShield(ASC, Amount, Duration);
        return true;
    }

    bool GuExecApplyMovement(
        AActor* TargetActor,
        const FGuMovementMechanic& MovementMechanic,
        const FVector& Direction)
    {
        ACharacter* Character = Cast<ACharacter>(TargetActor);
        if (!Character) return false;

        switch (MovementMechanic.Mode)
        {
        case EGuMovementMode::SpeedMultiplier:
            if (UGuRuntimeEffectComponent* Runtime = UGuRuntimeEffectComponent::FindOrCreate(Character))
            {
                Runtime->AddMovementMultiplier(MovementMechanic.SpeedMultiplier, MovementMechanic.Duration, false);
                return true;
            }
            return false;

        case EGuMovementMode::Dash:
        {
            FVector Velocity = Direction.GetSafeNormal() * FMath::Max(0.0f, MovementMechanic.DashSpeed);
            Velocity.Z += MovementMechanic.VerticalSpeed;
            Character->LaunchCharacter(Velocity, true, true);
            return true;
        }

        case EGuMovementMode::Blink:
        {
            const FVector Destination = Character->GetActorLocation()
                + Direction.GetSafeNormal() * FMath::Max(0.0f, MovementMechanic.BlinkDistance);
            FHitResult SweepHit;
            return Character->SetActorLocation(Destination, true, &SweepHit, ETeleportType::TeleportPhysics);
        }
        }

        return false;
    }

    bool GuExecExecuteDamage(const TInstancedStruct<FGuMechanic>& Mechanic, const FGuImpactContext& Context)
    {
        const FGuDamageMechanic* Damage = Mechanic.GetPtr<FGuDamageMechanic>();
        if (!Damage || !Context.SourceASC || !Context.TargetASC || !Context.TargetActor || Damage->Damage <= 0.0f) return false;

        float HealthDamage = Damage->Damage * FMath::Max(0.0f, Context.MagnitudeScale);
        if (UGuRuntimeEffectComponent* Runtime = Context.TargetActor->FindComponentByClass<UGuRuntimeEffectComponent>())
        {
            HealthDamage = Runtime->AbsorbDamage(Context.TargetASC, HealthDamage);
        }
        else
        {
            const FGameplayAttribute ShieldAttribute = UAS_GuMasterAttributeSet::GetShieldAttribute();
            const float ExistingShield = Context.TargetASC->GetNumericAttribute(ShieldAttribute);
            if (ExistingShield > 0.0f)
            {
                const float Absorbed = FMath::Min(ExistingShield, HealthDamage);
                Context.TargetASC->SetNumericAttributeBase(ShieldAttribute, ExistingShield - Absorbed);
                HealthDamage -= Absorbed;
            }
        }

        if (HealthDamage <= KINDA_SMALL_NUMBER) return true;

        if (!Damage->EffectClass)
        {
            const FGameplayAttribute Health = UAS_GuMasterAttributeSet::GetHealthAttribute();
            const float Current = Context.TargetASC->GetNumericAttribute(Health);
            Context.TargetASC->SetNumericAttributeBase(Health, FMath::Max(0.0f, Current - HealthDamage));
            return true;
        }

        FGameplayEffectContextHandle EffectContext = Context.SourceASC->MakeEffectContext();
        EffectContext.AddSourceObject(Context.Definition);
        FGameplayEffectSpecHandle Spec = Context.SourceASC->MakeOutgoingSpec(Damage->EffectClass, 1.0f, EffectContext);
        if (!Spec.IsValid() || !Spec.Data.IsValid()) return false;

        const FGameplayTag DamageTag = FGameplayTag::RequestGameplayTag(FName("Data.Gu.Damage"));
        if (!DamageTag.IsValid()) return false;
        Spec.Data->SetSetByCallerMagnitude(DamageTag, -HealthDamage);
        Context.SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), Context.TargetASC);
        return true;
    }

    bool GuExecExecuteKnockback(const TInstancedStruct<FGuMechanic>& Mechanic, const FGuImpactContext& Context)
    {
        const FGuKnockbackMechanic* Knockback = Mechanic.GetPtr<FGuKnockbackMechanic>();
        ACharacter* Character = Knockback ? Cast<ACharacter>(Context.TargetActor) : nullptr;
        if (!Character) return false;

        const float Scale = FMath::Max(0.0f, Context.MagnitudeScale);
        FVector Velocity = GuExecEffectDirection(Context.TargetActor, Context.Hit) * Knockback->Strength * Scale;
        Velocity.Z += Knockback->VerticalStrength * Scale;
        Character->LaunchCharacter(Velocity, false, false);
        return true;
    }

    bool GuExecExecuteDisplacement(const TInstancedStruct<FGuMechanic>& Mechanic, const FGuImpactContext& Context)
    {
        const FGuDisplacementMechanic* Displacement = Mechanic.GetPtr<FGuDisplacementMechanic>();
        ACharacter* Character = Displacement ? Cast<ACharacter>(Context.TargetActor) : nullptr;
        AActor* SourceActor = Context.SourceASC ? Context.SourceASC->GetAvatarActor() : nullptr;
        if (!Character || !SourceActor) return false;

        // For composed carriers the effect origin is encoded in TraceStart: field
        // centre, area centre, melee origin, previous chain target, etc. Falling back
        // to the caster preserves ordinary direct-use behavior.
        FVector EffectOrigin = SourceActor->GetActorLocation();
        if (Context.Hit)
        {
            EffectOrigin = FVector(
                static_cast<double>(Context.Hit->TraceStart.X),
                static_cast<double>(Context.Hit->TraceStart.Y),
                static_cast<double>(Context.Hit->TraceStart.Z));
        }

        FVector Direction = FVector::ZeroVector;
        switch (Displacement->Mode)
        {
        case EGuDisplacementMode::AwayFromSource:
            Direction = (Character->GetActorLocation() - EffectOrigin).GetSafeNormal();
            break;
        case EGuDisplacementMode::TowardSource:
            Direction = (EffectOrigin - Character->GetActorLocation()).GetSafeNormal();
            break;
        case EGuDisplacementMode::Upward:
            Direction = FVector::UpVector;
            break;
        case EGuDisplacementMode::Downward:
            Direction = FVector::DownVector;
            break;
        }

        const float Scale = FMath::Max(0.0f, Context.MagnitudeScale);
        FVector Velocity = Direction * Displacement->Strength * Scale;
        Velocity.Z += Displacement->VerticalStrength * Scale;
        Character->LaunchCharacter(Velocity, false, false);
        return true;
    }

    bool GuExecExecuteImpactHeal(const TInstancedStruct<FGuMechanic>& Mechanic, const FGuImpactContext& Context)
    {
        const FGuHealMechanic* Heal = Mechanic.GetPtr<FGuHealMechanic>();
        return Heal && Heal->Recipient == EGuMechanicRecipient::ImpactTarget
            ? GuExecApplyHeal(Context.TargetASC, Heal->Amount * FMath::Max(0.0f, Context.MagnitudeScale))
            : false;
    }

    bool GuExecExecuteImpactShield(const TInstancedStruct<FGuMechanic>& Mechanic, const FGuImpactContext& Context)
    {
        const FGuShieldMechanic* Shield = Mechanic.GetPtr<FGuShieldMechanic>();
        return Shield && Shield->Recipient == EGuMechanicRecipient::ImpactTarget
            ? GuExecApplyShield(Context.TargetActor, Context.TargetASC, Shield->Amount * FMath::Max(0.0f, Context.MagnitudeScale), Shield->Duration)
            : false;
    }

    bool GuExecExecuteImpactMovement(const TInstancedStruct<FGuMechanic>& Mechanic, const FGuImpactContext& Context)
    {
        const FGuMovementMechanic* Movement = Mechanic.GetPtr<FGuMovementMechanic>();
        if (!Movement || Movement->Recipient != EGuMechanicRecipient::ImpactTarget) return false;

        FGuMovementMechanic Scaled = *Movement;
        const float Scale = FMath::Max(0.0f, Context.MagnitudeScale);
        if (Scaled.Mode == EGuMovementMode::SpeedMultiplier)
        {
            Scaled.SpeedMultiplier = 1.0f + (Scaled.SpeedMultiplier - 1.0f) * Scale;
        }
        else if (Scaled.Mode == EGuMovementMode::Dash)
        {
            Scaled.DashSpeed *= Scale;
            Scaled.VerticalSpeed *= Scale;
        }
        else if (Scaled.Mode == EGuMovementMode::Blink)
        {
            Scaled.BlinkDistance *= Scale;
        }
        return GuExecApplyMovement(Context.TargetActor, Scaled, GuExecEffectDirection(Context.TargetActor, Context.Hit));
    }

    bool GuExecExecuteRestriction(const TInstancedStruct<FGuMechanic>& Mechanic, const FGuImpactContext& Context)
    {
        const FGuRestrictionMechanic* Restriction = Mechanic.GetPtr<FGuRestrictionMechanic>();
        if (!Restriction || !Context.TargetActor) return false;
        UGuRuntimeEffectComponent* Runtime = UGuRuntimeEffectComponent::FindOrCreate(Context.TargetActor);
        if (!Runtime) return false;
        const float Scale = FMath::Clamp(Context.MagnitudeScale, 0.0f, 1.0f);
        const float Multiplier = 1.0f - (1.0f - FMath::Clamp(Restriction->MovementMultiplier, 0.0f, 1.0f)) * Scale;
        Runtime->AddMovementMultiplier(Multiplier, Restriction->Duration, true);
        return true;
    }

    bool GuExecExecuteImpactDamageOverTime(const TInstancedStruct<FGuMechanic>& Mechanic, const FGuImpactContext& Context)
    {
        const FGuDamageOverTimeMechanic* Periodic = Mechanic.GetPtr<FGuDamageOverTimeMechanic>();
        if (!Periodic || Periodic->Recipient != EGuMechanicRecipient::ImpactTarget || !Context.TargetActor) return false;
        UGuRuntimeEffectComponent* Runtime = UGuRuntimeEffectComponent::FindOrCreate(Context.TargetActor);
        if (!Runtime) return false;
        Runtime->AddPeriodicHealth(
            Context.SourceASC,
            Context.TargetASC,
            -Periodic->DamagePerTick * FMath::Max(0.0f, Context.MagnitudeScale),
            Periodic->TickInterval,
            Periodic->Duration);
        return true;
    }

    bool GuExecExecuteImpactHealOverTime(const TInstancedStruct<FGuMechanic>& Mechanic, const FGuImpactContext& Context)
    {
        const FGuHealOverTimeMechanic* Periodic = Mechanic.GetPtr<FGuHealOverTimeMechanic>();
        if (!Periodic || Periodic->Recipient != EGuMechanicRecipient::ImpactTarget || !Context.TargetActor) return false;
        UGuRuntimeEffectComponent* Runtime = UGuRuntimeEffectComponent::FindOrCreate(Context.TargetActor);
        if (!Runtime) return false;
        Runtime->AddPeriodicHealth(
            Context.SourceASC,
            Context.TargetASC,
            Periodic->HealPerTick * FMath::Max(0.0f, Context.MagnitudeScale),
            Periodic->TickInterval,
            Periodic->Duration);
        return true;
    }

    bool GuExecExecuteImpactEssenceChange(const TInstancedStruct<FGuMechanic>& Mechanic, const FGuImpactContext& Context)
    {
        const FGuEssenceChangeMechanic* Essence = Mechanic.GetPtr<FGuEssenceChangeMechanic>();
        return Essence && Essence->Recipient == EGuMechanicRecipient::ImpactTarget
            ? GuExecApplyEssenceChange(Context.TargetASC, Context.SourceASC, *Essence, Context.MagnitudeScale)
            : false;
    }

    bool GuExecExecuteImpactEssenceRegeneration(const TInstancedStruct<FGuMechanic>& Mechanic, const FGuImpactContext& Context)
    {
        const FGuEssenceRegenerationMechanic* Regen = Mechanic.GetPtr<FGuEssenceRegenerationMechanic>();
        if (!Regen || Regen->Recipient != EGuMechanicRecipient::ImpactTarget || !Context.TargetActor) return false;
        UGuRuntimeEffectComponent* Runtime = UGuRuntimeEffectComponent::FindOrCreate(Context.TargetActor);
        if (!Runtime) return false;
        const float Scale = FMath::Max(0.0f, Context.MagnitudeScale);
        Runtime->AddEssenceRegeneration(
            Context.TargetASC,
            Regen->FlatPerSecond * Scale,
            Regen->PercentOfMaximumPerSecond * Scale,
            Regen->Duration);
        return true;
    }

    bool GuExecExecuteGuSuppression(const TInstancedStruct<FGuMechanic>& Mechanic, const FGuImpactContext& Context)
    {
        const FGuGuSuppressionMechanic* Suppression = Mechanic.GetPtr<FGuGuSuppressionMechanic>();
        UGuRuntimeEffectComponent* Runtime = Suppression ? UGuRuntimeEffectComponent::FindOrCreate(Context.TargetActor) : nullptr;
        if (!Suppression || !Runtime) return false;
        Runtime->ApplyGuSuppression(Suppression->Duration * FMath::Max(0.1f, Context.MagnitudeScale));
        return true;
    }

    bool GuExecExecuteImpactCleanse(const TInstancedStruct<FGuMechanic>& Mechanic, const FGuImpactContext& Context)
    {
        const FGuCleanseMechanic* Cleanse = Mechanic.GetPtr<FGuCleanseMechanic>();
        UGuRuntimeEffectComponent* Runtime = Cleanse && Cleanse->Recipient == EGuMechanicRecipient::ImpactTarget
            ? UGuRuntimeEffectComponent::FindOrCreate(Context.TargetActor)
            : nullptr;
        if (!Cleanse || !Runtime) return false;
        Runtime->CleanseHarmful(
            Context.TargetASC,
            Cleanse->bRemoveDamageOverTime,
            Cleanse->bRemoveRestrictions,
            Cleanse->bRemoveGuSuppression,
            Cleanse->bRemoveMarks);
        return true;
    }

    bool GuExecExecuteImpactDispel(const TInstancedStruct<FGuMechanic>& Mechanic, const FGuImpactContext& Context)
    {
        const FGuDispelMechanic* Dispel = Mechanic.GetPtr<FGuDispelMechanic>();
        UGuRuntimeEffectComponent* Runtime = Dispel && Dispel->Recipient == EGuMechanicRecipient::ImpactTarget
            ? UGuRuntimeEffectComponent::FindOrCreate(Context.TargetActor)
            : nullptr;
        if (!Dispel || !Runtime) return false;
        Runtime->DispelBeneficial(
            Context.TargetASC,
            Dispel->bRemoveShields,
            Dispel->bRemoveMovementBuffs,
            Dispel->bRemoveHealingOverTime,
            Dispel->bRemoveEssenceRegeneration,
            Dispel->bRemoveConcealment,
            Dispel->bRemoveReveal);
        return true;
    }

    bool GuExecExecuteMark(const TInstancedStruct<FGuMechanic>& Mechanic, const FGuImpactContext& Context)
    {
        const FGuMarkMechanic* Mark = Mechanic.GetPtr<FGuMarkMechanic>();
        UGuRuntimeEffectComponent* Runtime = Mark ? UGuRuntimeEffectComponent::FindOrCreate(Context.TargetActor) : nullptr;
        if (!Mark || !Runtime || Mark->MarkId.IsNone()) return false;
        Runtime->ApplyMark(Mark->MarkId, Mark->Strength * FMath::Max(0.0f, Context.MagnitudeScale), Mark->Duration);
        return true;
    }

    bool GuExecExecuteBuff(const TInstancedStruct<FGuMechanic>& Mechanic, const FGuActivationContext& Context)
    {
        const FGuBuffMechanic* Buff = Mechanic.GetPtr<FGuBuffMechanic>();
        if (!Buff || !Context.SourceASC || !Context.SystemConfig || !Buff->Attribute.IsValid() || Buff->Duration <= 0.0f) return false;

        TSubclassOf<UGameplayEffect> EffectClass = Context.SystemConfig->FindAdditiveBuffEffect(Buff->Attribute);
        if (!EffectClass) return false;

        FGameplayEffectContextHandle EffectContext = Context.SourceASC->MakeEffectContext();
        EffectContext.AddSourceObject(Context.Definition);
        FGameplayEffectSpecHandle Spec = Context.SourceASC->MakeOutgoingSpec(EffectClass, 1.0f, EffectContext);
        if (!Spec.IsValid() || !Spec.Data.IsValid()) return false;

        const FGameplayTag MagnitudeTag = FGameplayTag::RequestGameplayTag(FName("Data.Gu.Buff.Magnitude"));
        if (!MagnitudeTag.IsValid()) return false;
        Spec.Data->SetSetByCallerMagnitude(MagnitudeTag, Buff->Magnitude);
        Spec.Data->SetDuration(Buff->Duration, true);
        Context.SourceASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
        return true;
    }

    bool GuExecExecuteSelfHeal(const TInstancedStruct<FGuMechanic>& Mechanic, const FGuActivationContext& Context)
    {
        const FGuHealMechanic* Heal = Mechanic.GetPtr<FGuHealMechanic>();
        return Heal && Heal->Recipient == EGuMechanicRecipient::Self ? GuExecApplyHeal(Context.SourceASC, Heal->Amount) : false;
    }

    bool GuExecExecuteSelfShield(const TInstancedStruct<FGuMechanic>& Mechanic, const FGuActivationContext& Context)
    {
        const FGuShieldMechanic* Shield = Mechanic.GetPtr<FGuShieldMechanic>();
        return Shield && Shield->Recipient == EGuMechanicRecipient::Self
            ? GuExecApplyShield(Context.SourceActor, Context.SourceASC, Shield->Amount, Shield->Duration)
            : false;
    }

    bool GuExecExecuteSelfMovement(const TInstancedStruct<FGuMechanic>& Mechanic, const FGuActivationContext& Context)
    {
        const FGuMovementMechanic* Movement = Mechanic.GetPtr<FGuMovementMechanic>();
        return Movement && Movement->Recipient == EGuMechanicRecipient::Self
            ? GuExecApplyMovement(Context.SourceActor, *Movement, Context.SourceActor->GetActorForwardVector())
            : false;
    }

    bool GuExecExecuteSelfDamageOverTime(const TInstancedStruct<FGuMechanic>& Mechanic, const FGuActivationContext& Context)
    {
        const FGuDamageOverTimeMechanic* Periodic = Mechanic.GetPtr<FGuDamageOverTimeMechanic>();
        if (!Periodic || Periodic->Recipient != EGuMechanicRecipient::Self) return false;
        UGuRuntimeEffectComponent* Runtime = UGuRuntimeEffectComponent::FindOrCreate(Context.SourceActor);
        if (!Runtime) return false;
        Runtime->AddPeriodicHealth(Context.SourceASC, Context.SourceASC, -Periodic->DamagePerTick, Periodic->TickInterval, Periodic->Duration);
        return true;
    }

    bool GuExecExecuteSelfHealOverTime(const TInstancedStruct<FGuMechanic>& Mechanic, const FGuActivationContext& Context)
    {
        const FGuHealOverTimeMechanic* Periodic = Mechanic.GetPtr<FGuHealOverTimeMechanic>();
        if (!Periodic || Periodic->Recipient != EGuMechanicRecipient::Self) return false;
        UGuRuntimeEffectComponent* Runtime = UGuRuntimeEffectComponent::FindOrCreate(Context.SourceActor);
        if (!Runtime) return false;
        Runtime->AddPeriodicHealth(Context.SourceASC, Context.SourceASC, Periodic->HealPerTick, Periodic->TickInterval, Periodic->Duration);
        return true;
    }

    bool GuExecExecuteSelfEssenceChange(const TInstancedStruct<FGuMechanic>& Mechanic, const FGuActivationContext& Context)
    {
        const FGuEssenceChangeMechanic* Essence = Mechanic.GetPtr<FGuEssenceChangeMechanic>();
        return Essence && Essence->Recipient == EGuMechanicRecipient::Self
            ? GuExecApplyEssenceChange(Context.SourceASC, Context.SourceASC, *Essence, 1.0f)
            : false;
    }

    bool GuExecExecuteSelfEssenceRegeneration(const TInstancedStruct<FGuMechanic>& Mechanic, const FGuActivationContext& Context)
    {
        const FGuEssenceRegenerationMechanic* Regen = Mechanic.GetPtr<FGuEssenceRegenerationMechanic>();
        if (!Regen || Regen->Recipient != EGuMechanicRecipient::Self) return false;
        UGuRuntimeEffectComponent* Runtime = UGuRuntimeEffectComponent::FindOrCreate(Context.SourceActor);
        if (!Runtime) return false;
        Runtime->AddEssenceRegeneration(Context.SourceASC, Regen->FlatPerSecond, Regen->PercentOfMaximumPerSecond, Regen->Duration);
        return true;
    }

    bool GuExecExecuteSelfCleanse(const TInstancedStruct<FGuMechanic>& Mechanic, const FGuActivationContext& Context)
    {
        const FGuCleanseMechanic* Cleanse = Mechanic.GetPtr<FGuCleanseMechanic>();
        UGuRuntimeEffectComponent* Runtime = Cleanse && Cleanse->Recipient == EGuMechanicRecipient::Self
            ? UGuRuntimeEffectComponent::FindOrCreate(Context.SourceActor)
            : nullptr;
        if (!Cleanse || !Runtime) return false;
        Runtime->CleanseHarmful(
            Context.SourceASC,
            Cleanse->bRemoveDamageOverTime,
            Cleanse->bRemoveRestrictions,
            Cleanse->bRemoveGuSuppression,
            Cleanse->bRemoveMarks);
        return true;
    }

    bool GuExecExecuteSelfDispel(const TInstancedStruct<FGuMechanic>& Mechanic, const FGuActivationContext& Context)
    {
        const FGuDispelMechanic* Dispel = Mechanic.GetPtr<FGuDispelMechanic>();
        UGuRuntimeEffectComponent* Runtime = Dispel && Dispel->Recipient == EGuMechanicRecipient::Self
            ? UGuRuntimeEffectComponent::FindOrCreate(Context.SourceActor)
            : nullptr;
        if (!Dispel || !Runtime) return false;
        Runtime->DispelBeneficial(
            Context.SourceASC,
            Dispel->bRemoveShields,
            Dispel->bRemoveMovementBuffs,
            Dispel->bRemoveHealingOverTime,
            Dispel->bRemoveEssenceRegeneration,
            Dispel->bRemoveConcealment,
            Dispel->bRemoveReveal);
        return true;
    }

    bool GuExecExecuteConcealment(const TInstancedStruct<FGuMechanic>& Mechanic, const FGuActivationContext& Context)
    {
        const FGuConcealmentMechanic* Conceal = Mechanic.GetPtr<FGuConcealmentMechanic>();
        UGuRuntimeEffectComponent* Runtime = Conceal ? UGuRuntimeEffectComponent::FindOrCreate(Context.SourceActor) : nullptr;
        if (!Conceal || !Runtime) return false;
        Runtime->ApplyConcealment(Conceal->Opacity, Conceal->DetectionResistance, Conceal->Duration, Conceal->bBreakOnAttack);
        return true;
    }

    bool GuExecExecuteReveal(const TInstancedStruct<FGuMechanic>& Mechanic, const FGuActivationContext& Context)
    {
        const FGuRevealMechanic* Reveal = Mechanic.GetPtr<FGuRevealMechanic>();
        UGuRuntimeEffectComponent* Runtime = Reveal ? UGuRuntimeEffectComponent::FindOrCreate(Context.SourceActor) : nullptr;
        if (!Reveal || !Runtime) return false;
        Runtime->ApplyReveal(Reveal->Strength, Reveal->Range, Reveal->Duration);
        return true;
    }

    bool GuExecExecuteAttentionBoost(const TInstancedStruct<FGuMechanic>& Mechanic, const FGuActivationContext& Context)
    {
        const FGuAttentionBoostMechanic* Boost = Mechanic.GetPtr<FGuAttentionBoostMechanic>();
        APawn* Pawn = Boost ? Cast<APawn>(Context.SourceActor) : nullptr;
        AGuPlayerState* PlayerState = Pawn ? Pawn->GetPlayerState<AGuPlayerState>() : nullptr;
        UGuRuntimeEffectComponent* Runtime = PlayerState && PlayerState->MentalResources
            ? UGuRuntimeEffectComponent::FindOrCreate(Context.SourceActor)
            : nullptr;
        if (!Boost || !Runtime || !PlayerState || !PlayerState->MentalResources) return false;

        const FName DefinitionId = Context.Definition && !Context.Definition->StableDefinitionId.IsNone()
            ? Context.Definition->StableDefinitionId
            : Context.Definition ? Context.Definition->GetFName() : FName(TEXT("Gu"));
        const FName Key(*FString::Printf(TEXT("GuAttention.%s"), *DefinitionId.ToString()));
        Runtime->AddAttentionBoost(PlayerState->MentalResources, Key, Boost->SlotsGranted, Boost->Duration);
        return true;
    }

    bool GuExecSpawnSummonsAtLocation(
        const FGuSummonMechanic& Summon,
        const FGuActivationContext& Context,
        const FVector& Location,
        const FVector& ForwardDirection,
        const bool bApplyForwardOffset)
    {
        UWorld* World = Context.SourceActor ? Context.SourceActor->GetWorld() : nullptr;
        if (!World || !Summon.ActorClass || Summon.Count <= 0) return false;

        const FVector Forward = ForwardDirection.GetSafeNormal().IsNearlyZero()
            ? Context.SourceActor->GetActorForwardVector().GetSafeNormal()
            : ForwardDirection.GetSafeNormal();
        const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward).GetSafeNormal();
        const FVector SafeRight = Right.IsNearlyZero() ? Context.SourceActor->GetActorRightVector() : Right;
        const FVector Center = Location + (bApplyForwardOffset ? Forward * Summon.ForwardOffset : FVector::ZeroVector);
        bool bSpawnedAny = false;
        const int32 Count = FMath::Clamp(Summon.Count, 1, 64);

        for (int32 Index = 0; Index < Count; ++Index)
        {
            const float Angle = Count > 1 ? (2.0f * PI * static_cast<float>(Index) / static_cast<float>(Count)) : 0.0f;
            const float Radius = Count > 1 ? Summon.SpawnRadius : 0.0f;
            const FVector Offset = Forward * (FMath::Cos(Angle) * Radius) + SafeRight * (FMath::Sin(Angle) * Radius);

            FActorSpawnParameters Params;
            Params.Owner = Context.SourceActor;
            Params.Instigator = Cast<APawn>(Context.SourceActor);
            Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
            AActor* Spawned = World->SpawnActor<AActor>(
                Summon.ActorClass,
                Center + Offset,
                Forward.Rotation(),
                Params);
            if (!Spawned) continue;
            if (Summon.Lifetime > 0.0f) Spawned->SetLifeSpan(Summon.Lifetime);
            bSpawnedAny = true;
        }
        return bSpawnedAny;
    }

    bool GuExecExecuteSummon(const TInstancedStruct<FGuMechanic>& Mechanic, const FGuActivationContext& Context)
    {
        const FGuSummonMechanic* Summon = Mechanic.GetPtr<FGuSummonMechanic>();
        if (!Summon || !Context.SourceActor) return false;
        return GuExecSpawnSummonsAtLocation(
            *Summon,
            Context,
            Context.SourceActor->GetActorLocation(),
            Context.SourceActor->GetActorForwardVector(),
            true);
    }

    TArray<AActor*> GuExecFindTargetsInArea(AActor* SourceActor, const FVector& Center, const float Radius, const bool bIncludeSelf)
    {
        TArray<AActor*> Targets;
        UWorld* World = SourceActor ? SourceActor->GetWorld() : nullptr;
        if (!World || Radius <= 0.0f) return Targets;

        FCollisionObjectQueryParams Objects;
        Objects.AddObjectTypesToQuery(ECC_Pawn);
        FCollisionQueryParams Query(SCENE_QUERY_STAT(GuAreaCarrier), false, bIncludeSelf ? nullptr : SourceActor);
        TArray<FOverlapResult> Overlaps;
        if (!World->OverlapMultiByObjectType(Overlaps, Center, FQuat::Identity, Objects, FCollisionShape::MakeSphere(Radius), Query))
        {
            if (bIncludeSelf && SourceActor) Targets.Add(SourceActor);
            return Targets;
        }

        for (const FOverlapResult& Overlap : Overlaps)
        {
            AActor* Actor = Overlap.GetActor();
            if (!IsValid(Actor) || (!bIncludeSelf && Actor == SourceActor)) continue;
            if (UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Actor)) Targets.AddUnique(Actor);
        }
        if (bIncludeSelf && SourceActor && UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(SourceActor)) Targets.AddUnique(SourceActor);
        return Targets;
    }

    bool GuExecExecuteAreaAtLocation(
        const FGuAreaMechanic& Area,
        const FGuActivationContext& Context,
        const FVector& Center)
    {
        if (!Context.SourceActor) return false;

        TArray<AActor*> Targets = GuExecFindTargetsInArea(Context.SourceActor, Center, Area.Radius, Area.bIncludeSelf);
        Targets.Sort([Center](const AActor& A, const AActor& B)
        {
            return FVector::DistSquared(A.GetActorLocation(), Center) < FVector::DistSquared(B.GetActorLocation(), Center);
        });

        const int32 Limit = Area.MaxTargets > 0 ? FMath::Min(Area.MaxTargets, Targets.Num()) : Targets.Num();
        bool bExecutedAnything = false;
        for (int32 Index = 0; Index < Limit; ++Index)
        {
            FHitResult Hit;
            Hit.TraceStart = Center;
            Hit.TraceEnd = Targets[Index]->GetActorLocation();
            Hit.ImpactPoint = Targets[Index]->GetActorLocation();
            bExecutedAnything |= GuExecExecutePayloadInternal(
                Context.Definition,
                Context.SourceASC,
                Targets[Index],
                Hit,
                1.0f,
                true);
        }
        // A carrier manifested even when there happened to be no valid targets.
        return bExecutedAnything || Limit == 0;
    }

    bool GuExecExecuteArea(const TInstancedStruct<FGuMechanic>& Mechanic, const FGuActivationContext& Context)
    {
        const FGuAreaMechanic* Area = Mechanic.GetPtr<FGuAreaMechanic>();
        if (!Area || !Context.SourceActor) return false;

        const FVector Center = Context.SourceActor->GetActorLocation()
            + Context.SourceActor->GetActorForwardVector() * Area->ForwardOffset;
        return GuExecExecuteAreaAtLocation(*Area, Context, Center);
    }

    bool GuExecSpawnFieldAtLocation(
        const FGuFieldMechanic& Field,
        const FGuActivationContext& Context,
        const FVector& Location)
    {
        UWorld* World = Context.SourceActor ? Context.SourceActor->GetWorld() : nullptr;
        if (!World) return false;

        FActorSpawnParameters Params;
        Params.Owner = Context.SourceActor;
        Params.Instigator = Cast<APawn>(Context.SourceActor);
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        AGuEffectField* FieldActor = World->SpawnActor<AGuEffectField>(
            AGuEffectField::StaticClass(),
            Location,
            FRotator::ZeroRotator,
            Params);
        if (!FieldActor) return false;
        FieldActor->InitializeField(Field, Context.Definition, Context.SourceASC, Context.SourceActor);
        return true;
    }

    bool GuExecExecuteField(const TInstancedStruct<FGuMechanic>& Mechanic, const FGuActivationContext& Context)
    {
        const FGuFieldMechanic* Field = Mechanic.GetPtr<FGuFieldMechanic>();
        if (!Field || !Context.SourceActor) return false;

        const FVector Location = Context.SourceActor->GetActorLocation()
            + Context.SourceActor->GetActorForwardVector() * Field->ForwardOffset;
        return GuExecSpawnFieldAtLocation(*Field, Context, Location);
    }

    bool GuExecExecuteMeleeAtLocation(
        const FGuMeleeMechanic& Melee,
        const FGuActivationContext& Context,
        const FVector& Origin,
        const FVector& ForwardDirection,
        const bool bComposeSecondaryCarriers)
    {
        if (!Context.SourceActor) return false;

        TArray<AActor*> Candidates = GuExecFindTargetsInArea(
            Context.SourceActor,
            Origin,
            Melee.Range + Melee.Radius,
            false);
        const FVector Forward = ForwardDirection.GetSafeNormal().IsNearlyZero()
            ? Context.SourceActor->GetActorForwardVector().GetSafeNormal()
            : ForwardDirection.GetSafeNormal();
        const float MinDot = FMath::Cos(FMath::DegreesToRadians(FMath::Clamp(Melee.ArcDegrees, 1.0f, 360.0f) * 0.5f));

        Candidates.RemoveAll([&](const AActor* Actor)
        {
            if (!Actor) return true;
            const FVector Offset = Actor->GetActorLocation() - Origin;
            const float Distance = Offset.Size();
            if (Distance > Melee.Range + Melee.Radius) return true;
            if (Melee.ArcDegrees >= 359.9f) return false;
            return FVector::DotProduct(Forward, Offset.GetSafeNormal()) < MinDot;
        });
        Candidates.Sort([Origin](const AActor& A, const AActor& B)
        {
            return FVector::DistSquared(A.GetActorLocation(), Origin) < FVector::DistSquared(B.GetActorLocation(), Origin);
        });

        const int32 Limit = Melee.MaxTargets > 0 ? FMath::Min(Melee.MaxTargets, Candidates.Num()) : Candidates.Num();
        bool bExecutedAnything = false;
        for (int32 Index = 0; Index < Limit; ++Index)
        {
            FHitResult Hit;
            Hit.TraceStart = Origin;
            Hit.TraceEnd = Candidates[Index]->GetActorLocation();
            Hit.ImpactPoint = Candidates[Index]->GetActorLocation();
            if (bComposeSecondaryCarriers)
            {
                bExecutedAnything |= GuExecResolveDeliveredImpact(
                    Context.Definition,
                    Context.SourceASC,
                    Candidates[Index],
                    Hit,
                    1.0f,
                    true,
                    false);
            }
            else
            {
                bExecutedAnything |= GuExecExecutePayloadInternal(
                    Context.Definition,
                    Context.SourceASC,
                    Candidates[Index],
                    Hit,
                    1.0f,
                    true);
            }
        }
        return bExecutedAnything || Limit == 0;
    }

    bool GuExecExecuteMelee(const TInstancedStruct<FGuMechanic>& Mechanic, const FGuActivationContext& Context)
    {
        const FGuMeleeMechanic* Melee = Mechanic.GetPtr<FGuMeleeMechanic>();
        if (!Melee || !Context.SourceActor) return false;
        return GuExecExecuteMeleeAtLocation(
            *Melee,
            Context,
            Context.SourceActor->GetActorLocation(),
            Context.SourceActor->GetActorForwardVector(),
            true);
    }

    bool GuExecHasProjectileCarrier(const UGuDefinition* Definition)
    {
        if (!Definition) return false;
        for (const TInstancedStruct<FGuMechanic>& Mechanic : Definition->Mechanics)
        {
            if (Mechanic.GetPtr<FGuProjectileMechanic>()) return true;
        }
        return false;
    }

    bool GuExecHasMeleeCarrier(const UGuDefinition* Definition)
    {
        if (!Definition) return false;
        for (const TInstancedStruct<FGuMechanic>& Mechanic : Definition->Mechanics)
        {
            if (Mechanic.GetPtr<FGuMeleeMechanic>()) return true;
        }
        return false;
    }

    bool GuExecIsDeferredCarrierType(const UScriptStruct* Type)
    {
        return Type == FGuMeleeMechanic::StaticStruct()
            || Type == FGuAreaMechanic::StaticStruct()
            || Type == FGuFieldMechanic::StaticStruct()
            || Type == FGuSummonMechanic::StaticStruct();
    }

    const TMap<const UScriptStruct*, FImpactExecutor>& GuExecImpactExecutors()
    {
        static const TMap<const UScriptStruct*, FImpactExecutor> Executors =
        {
            { FGuDamageMechanic::StaticStruct(), &GuExecExecuteDamage },
            { FGuKnockbackMechanic::StaticStruct(), &GuExecExecuteKnockback },
            { FGuDisplacementMechanic::StaticStruct(), &GuExecExecuteDisplacement },
            { FGuHealMechanic::StaticStruct(), &GuExecExecuteImpactHeal },
            { FGuShieldMechanic::StaticStruct(), &GuExecExecuteImpactShield },
            { FGuMovementMechanic::StaticStruct(), &GuExecExecuteImpactMovement },
            { FGuRestrictionMechanic::StaticStruct(), &GuExecExecuteRestriction },
            { FGuDamageOverTimeMechanic::StaticStruct(), &GuExecExecuteImpactDamageOverTime },
            { FGuHealOverTimeMechanic::StaticStruct(), &GuExecExecuteImpactHealOverTime },
            { FGuEssenceChangeMechanic::StaticStruct(), &GuExecExecuteImpactEssenceChange },
            { FGuEssenceRegenerationMechanic::StaticStruct(), &GuExecExecuteImpactEssenceRegeneration },
            { FGuGuSuppressionMechanic::StaticStruct(), &GuExecExecuteGuSuppression },
            { FGuCleanseMechanic::StaticStruct(), &GuExecExecuteImpactCleanse },
            { FGuDispelMechanic::StaticStruct(), &GuExecExecuteImpactDispel },
            { FGuMarkMechanic::StaticStruct(), &GuExecExecuteMark }
        };
        return Executors;
    }

    const TMap<const UScriptStruct*, FActivationExecutor>& GuExecActivationExecutors()
    {
        static const TMap<const UScriptStruct*, FActivationExecutor> Executors =
        {
            { FGuBuffMechanic::StaticStruct(), &GuExecExecuteBuff },
            { FGuHealMechanic::StaticStruct(), &GuExecExecuteSelfHeal },
            { FGuShieldMechanic::StaticStruct(), &GuExecExecuteSelfShield },
            { FGuMovementMechanic::StaticStruct(), &GuExecExecuteSelfMovement },
            { FGuDamageOverTimeMechanic::StaticStruct(), &GuExecExecuteSelfDamageOverTime },
            { FGuHealOverTimeMechanic::StaticStruct(), &GuExecExecuteSelfHealOverTime },
            { FGuEssenceChangeMechanic::StaticStruct(), &GuExecExecuteSelfEssenceChange },
            { FGuEssenceRegenerationMechanic::StaticStruct(), &GuExecExecuteSelfEssenceRegeneration },
            { FGuCleanseMechanic::StaticStruct(), &GuExecExecuteSelfCleanse },
            { FGuDispelMechanic::StaticStruct(), &GuExecExecuteSelfDispel },
            { FGuConcealmentMechanic::StaticStruct(), &GuExecExecuteConcealment },
            { FGuRevealMechanic::StaticStruct(), &GuExecExecuteReveal },
            { FGuAttentionBoostMechanic::StaticStruct(), &GuExecExecuteAttentionBoost },
            { FGuSummonMechanic::StaticStruct(), &GuExecExecuteSummon },
            { FGuMeleeMechanic::StaticStruct(), &GuExecExecuteMelee },
            { FGuAreaMechanic::StaticStruct(), &GuExecExecuteArea },
            { FGuFieldMechanic::StaticStruct(), &GuExecExecuteField }
        };
        return Executors;
    }

    bool GuExecResolveDeliveredImpact(
        UGuDefinition* Definition,
        UAbilitySystemComponent* SourceASC,
        AActor* TargetActor,
        const FHitResult& HitResult,
        const float MagnitudeScale,
        const bool bAllowChain,
        const bool bAllowMeleeCarrier)
    {
        if (!Definition || !SourceASC) return false;

        AActor* SourceActor = SourceASC->GetAvatarActor();
        if (!SourceActor) return false;

        // Transport carriers should resolve secondary carriers at the actual swept
        // carrier location, not at the target actor's pivot. StaticMeshActor pivots can
        // legitimately live at world origin even when their geometry is elsewhere.
        const FVector HitLocation(
            static_cast<double>(HitResult.Location.X),
            static_cast<double>(HitResult.Location.Y),
            static_cast<double>(HitResult.Location.Z));
        const FVector ImpactLocation(
            static_cast<double>(HitResult.ImpactPoint.X),
            static_cast<double>(HitResult.ImpactPoint.Y),
            static_cast<double>(HitResult.ImpactPoint.Z));
        const FVector TraceEndLocation(
            static_cast<double>(HitResult.TraceEnd.X),
            static_cast<double>(HitResult.TraceEnd.Y),
            static_cast<double>(HitResult.TraceEnd.Z));

        FVector DeliveryLocation = HitLocation;
        if (DeliveryLocation.IsNearlyZero() && !ImpactLocation.IsNearlyZero())
        {
            DeliveryLocation = ImpactLocation;
        }
        if (DeliveryLocation.IsNearlyZero() && !TraceEndLocation.IsNearlyZero())
        {
            DeliveryLocation = TraceEndLocation;
        }
        if (DeliveryLocation.IsNearlyZero() && TargetActor)
        {
            DeliveryLocation = TargetActor->GetActorLocation();
        }

        UE_LOG(
            LogTemp,
            Warning,
            TEXT("Gu carrier delivery - Hit.Location=%s ImpactPoint=%s TraceEnd=%s Resolved=%s TargetOrigin=%s"),
            *HitLocation.ToString(),
            *ImpactLocation.ToString(),
            *TraceEndLocation.ToString(),
            *DeliveryLocation.ToString(),
            TargetActor ? *TargetActor->GetActorLocation().ToString() : TEXT("<none>"));

        FVector DeliveryDirection = (HitResult.TraceEnd - HitResult.TraceStart).GetSafeNormal();
        if (DeliveryDirection.IsNearlyZero()) DeliveryDirection = SourceActor->GetActorForwardVector();

        const FGuActivationContext CarrierContext { Definition, SourceASC, SourceActor, nullptr };
        bool bExecutedAnything = false;
        bool bHasPayloadCarrier = false;

        // Transport carriers such as Projectile establish the delivery point. Spatial/temporal
        // carriers on the same Gu then manifest at that point instead of firing independently
        // from the caster. This is the core mechanic-composition rule.
        for (const TInstancedStruct<FGuMechanic>& Mechanic : Definition->Mechanics)
        {
            if (const FGuFieldMechanic* Field = Mechanic.GetPtr<FGuFieldMechanic>())
            {
                bHasPayloadCarrier = true;
                bExecutedAnything |= GuExecSpawnFieldAtLocation(*Field, CarrierContext, DeliveryLocation);
            }
            else if (const FGuAreaMechanic* Area = Mechanic.GetPtr<FGuAreaMechanic>())
            {
                bHasPayloadCarrier = true;
                bExecutedAnything |= GuExecExecuteAreaAtLocation(*Area, CarrierContext, DeliveryLocation);
            }
            else if (const FGuMeleeMechanic* Melee = Mechanic.GetPtr<FGuMeleeMechanic>())
            {
                if (bAllowMeleeCarrier)
                {
                    bHasPayloadCarrier = true;
                    bExecutedAnything |= GuExecExecuteMeleeAtLocation(
                        *Melee,
                        CarrierContext,
                        DeliveryLocation,
                        DeliveryDirection,
                        false);
                }
            }
            else if (const FGuSummonMechanic* Summon = Mechanic.GetPtr<FGuSummonMechanic>())
            {
                bExecutedAnything |= GuExecSpawnSummonsAtLocation(
                    *Summon,
                    CarrierContext,
                    DeliveryLocation,
                    DeliveryDirection,
                    false);
            }
        }

        // If another carrier took ownership of the payload (field/area/melee), it will deliver
        // the terminal mechanics itself. Otherwise this is an ordinary direct impact.
        if (!bHasPayloadCarrier && TargetActor)
        {
            bExecutedAnything |= GuExecExecutePayloadInternal(
                Definition,
                SourceASC,
                TargetActor,
                HitResult,
                MagnitudeScale,
                bAllowChain);
        }

        return bExecutedAnything;
    }

    bool GuExecExecuteChain(
        UGuDefinition* Definition,
        UAbilitySystemComponent* SourceASC,
        AActor* InitialTarget,
        const FHitResult& InitialHit,
        const float InitialScale)
    {
        const FGuChainMechanic* Chain = nullptr;
        for (const TInstancedStruct<FGuMechanic>& Mechanic : Definition->Mechanics)
        {
            if (const FGuChainMechanic* Found = Mechanic.GetPtr<FGuChainMechanic>())
            {
                Chain = Found;
                break;
            }
        }
        if (!Chain || Chain->MaxAdditionalTargets <= 0 || Chain->JumpRadius <= 0.0f) return false;

        AActor* SourceActor = SourceASC ? SourceASC->GetAvatarActor() : nullptr;
        AActor* CurrentTarget = InitialTarget;
        TSet<TWeakObjectPtr<AActor>> Visited;
        if (SourceActor) Visited.Add(TWeakObjectPtr<AActor>(SourceActor));
        if (InitialTarget) Visited.Add(TWeakObjectPtr<AActor>(InitialTarget));

        bool bExecuted = false;
        float Scale = FMath::Max(0.0f, InitialScale);
        const int32 MaxJumps = FMath::Clamp(Chain->MaxAdditionalTargets, 1, 32);
        for (int32 Jump = 0; Jump < MaxJumps && CurrentTarget; ++Jump)
        {
            TArray<AActor*> Candidates = GuExecFindTargetsInArea(SourceActor, CurrentTarget->GetActorLocation(), Chain->JumpRadius, false);
            Candidates.RemoveAll([&Visited](AActor* Candidate)
            {
                return !Candidate || Visited.Contains(TWeakObjectPtr<AActor>(Candidate));
            });
            if (Candidates.IsEmpty()) break;

            const FVector Origin = CurrentTarget->GetActorLocation();
            Candidates.Sort([Origin](const AActor& A, const AActor& B)
            {
                return FVector::DistSquared(A.GetActorLocation(), Origin) < FVector::DistSquared(B.GetActorLocation(), Origin);
            });

            AActor* NextTarget = Candidates[0];
            Visited.Add(TWeakObjectPtr<AActor>(NextTarget));
            Scale *= FMath::Max(0.0f, Chain->MagnitudeFalloff);

            FHitResult ChainHit;
            ChainHit.TraceStart = Origin;
            ChainHit.TraceEnd = NextTarget->GetActorLocation();
            ChainHit.ImpactPoint = NextTarget->GetActorLocation();
            bExecuted |= GuExecExecutePayloadInternal(Definition, SourceASC, NextTarget, ChainHit, Scale, false);
            CurrentTarget = NextTarget;
        }
        return bExecuted;
    }

    bool GuExecExecutePayloadInternal(
        UGuDefinition* GuDefinition,
        UAbilitySystemComponent* SourceASC,
        AActor* TargetActor,
        const FHitResult& HitResult,
        const float MagnitudeScale,
        const bool bAllowChain)
    {
        if (!GuDefinition || !SourceASC || !TargetActor || TargetActor->IsActorBeingDestroyed()) return false;

        UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);
        if (!TargetASC) return false;

        const FGuImpactContext Context { GuDefinition, SourceASC, TargetASC, TargetActor, &HitResult, FMath::Max(0.0f, MagnitudeScale) };
        bool bExecutedAnything = false;
        for (const TInstancedStruct<FGuMechanic>& Mechanic : GuDefinition->Mechanics)
        {
            const UScriptStruct* Type = Mechanic.GetScriptStruct();
            const FImpactExecutor* Executor = Type ? GuExecImpactExecutors().Find(Type) : nullptr;
            if (Executor) bExecutedAnything |= (*Executor)(Mechanic, Context);
        }

        if (bAllowChain)
        {
            bExecutedAnything |= GuExecExecuteChain(GuDefinition, SourceASC, TargetActor, HitResult, Context.MagnitudeScale);
        }
        return bExecutedAnything;
    }
}

bool UGuExecutionLibrary::ExecuteImpact(
    UGuDefinition* GuDefinition,
    UAbilitySystemComponent* SourceASC,
    AActor* TargetActor,
    const FHitResult& HitResult)
{
    const bool bExecutedAnything = GuExecResolveDeliveredImpact(
        GuDefinition,
        SourceASC,
        TargetActor,
        HitResult,
        1.0f,
        true,
        true);
    if (bExecutedAnything && SourceASC)
    {
        if (AActor* SourceActor = SourceASC->GetAvatarActor())
        {
            if (UGuRuntimeEffectComponent* Runtime = SourceActor->FindComponentByClass<UGuRuntimeEffectComponent>())
            {
                Runtime->BreakConcealmentOnAttack();
            }
        }
    }
    return bExecutedAnything;
}

bool UGuExecutionLibrary::ExecutePayloadImpact(
    UGuDefinition* GuDefinition,
    UAbilitySystemComponent* SourceASC,
    AActor* TargetActor,
    const FHitResult& HitResult,
    const float MagnitudeScale,
    const bool bAllowChain)
{
    return GuExecExecutePayloadInternal(
        GuDefinition,
        SourceASC,
        TargetActor,
        HitResult,
        FMath::Max(0.0f, MagnitudeScale),
        bAllowChain);
}

bool UGuExecutionLibrary::ExecuteActivation(
    UGuDefinition* GuDefinition,
    UAbilitySystemComponent* SourceASC,
    AActor* SourceActor,
    const UGuSystemConfig* SystemConfig)
{
    if (!GuDefinition || !SourceASC || !SourceActor) return false;

    const FGuActivationContext Context { GuDefinition, SourceASC, SourceActor, SystemConfig };
    const bool bProjectileDelivery = GuExecHasProjectileCarrier(GuDefinition);
    const bool bMeleeDelivery = !bProjectileDelivery && GuExecHasMeleeCarrier(GuDefinition);
    bool bExecutedAnything = false;

    // First execute true activation/self mechanics. Carrier mechanics are handled as a
    // composition stage below so they do not all independently originate at the caster.
    for (const TInstancedStruct<FGuMechanic>& Mechanic : GuDefinition->Mechanics)
    {
        const UScriptStruct* Type = Mechanic.GetScriptStruct();
        if (!Type || GuExecIsDeferredCarrierType(Type)) continue;

        const FActivationExecutor* Executor = GuExecActivationExecutors().Find(Type);
        if (Executor) bExecutedAnything |= (*Executor)(Mechanic, Context);
    }

    // Projectile is the outer transport carrier and is spawned by UGA_GuAbility. Any
    // spatial/temporal carriers wait for its impact and manifest at that destination.
    if (bProjectileDelivery) return bExecutedAnything;

    // Melee is the next transport tier. A successful contact becomes the destination for
    // Field/Area/Summon mechanics, which lets combinations such as Melee + Field compose
    // without separately creating the field at the caster.
    if (bMeleeDelivery)
    {
        for (const TInstancedStruct<FGuMechanic>& Mechanic : GuDefinition->Mechanics)
        {
            if (!Mechanic.GetPtr<FGuMeleeMechanic>()) continue;
            const FActivationExecutor* Executor = GuExecActivationExecutors().Find(FGuMeleeMechanic::StaticStruct());
            if (Executor) bExecutedAnything |= (*Executor)(Mechanic, Context);
        }
        return bExecutedAnything;
    }

    // Without a transport carrier, destination carriers originate from the user normally.
    for (const TInstancedStruct<FGuMechanic>& Mechanic : GuDefinition->Mechanics)
    {
        const UScriptStruct* Type = Mechanic.GetScriptStruct();
        if (!Type || !GuExecIsDeferredCarrierType(Type)) continue;

        const FActivationExecutor* Executor = GuExecActivationExecutors().Find(Type);
        if (Executor) bExecutedAnything |= (*Executor)(Mechanic, Context);
    }

    return bExecutedAnything;
}
