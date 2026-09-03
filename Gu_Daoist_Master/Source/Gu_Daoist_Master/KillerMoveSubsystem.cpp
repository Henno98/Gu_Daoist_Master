#include "KillerMoveSubsystem.h"

#include "AS_GuMasterAttributeSet.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayAbilitySpec.h"
#include "DrawDebugHelpers.h"
#include "GA_GuAbility.h"
#include "GuExecutionLibrary.h"
#include "GuSystemConfig.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Engine/GameInstance.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GuDefinitionRegistrySubsystem.h"
#include "GuEntitySubsystem.h"
#include "GuPlayerState.h"
#include "GuRuntimeEffectComponent.h"
#include "Gu_Projectile.h"
#include "KillerMoveDefinition.h"
#include "MentalResourceComponent.h"
#include "TimerManager.h"
#include "UGuDefinition.h"

namespace
{
    FString RoleLabel(const EKillerMoveRole Role)
    {
        if (const UEnum* Enum = StaticEnum<EKillerMoveRole>())
        {
            return Enum->GetDisplayNameTextByValue(static_cast<int64>(Role)).ToString();
        }
        return TEXT("Control");
    }

    bool IsDeliveryTargetRole(const EKillerMoveRole Role)
    {
        return Role == EKillerMoveRole::Targeting
            || Role == EKillerMoveRole::InvestigationSensor
            || Role == EKillerMoveRole::RecognitionValidation
            || Role == EKillerMoveRole::Routing
            || Role == EKillerMoveRole::Boundary
            || Role == EKillerMoveRole::Anchor
            || Role == EKillerMoveRole::Trigger
            || Role == EKillerMoveRole::Storage
            || Role == EKillerMoveRole::Termination
            || Role == EKillerMoveRole::Switching;
    }

    float KillerMoveRuntimeSemanticScore(const FRefinementSemanticProfile& Profile, const FName Key)
    {
        if (const float* Value = Profile.Attributes.Find(Key)) return FMath::Max(0.0f, *Value);
        return 0.0f;
    }

    float KillerMoveRuntimeRoleCostWeight(const EKillerMoveRole Role)
    {
        switch (Role)
        {
        case EKillerMoveRole::Core: return 1.0f;
        case EKillerMoveRole::Output: return 0.80f;
        case EKillerMoveRole::Amplification: return 0.75f;
        case EKillerMoveRole::Medium: return 0.70f;
        case EKillerMoveRole::Fuel: return 0.45f;
        case EKillerMoveRole::Targeting:
        case EKillerMoveRole::InvestigationSensor:
        case EKillerMoveRole::RecognitionValidation: return 0.55f;
        case EKillerMoveRole::Stabilization:
        case EKillerMoveRole::Safety:
        case EKillerMoveRole::Buffer:
        case EKillerMoveRole::Recovery: return 0.50f;
        default: return 0.60f;
        }
    }

    EKillerMoveRole KillerMoveRuntimeInferSupportRole(const FGuDefinitionRecord& Definition)
    {
        const FRefinementSemanticProfile& Profile = Definition.RefinementProfile;
        const float Speed = KillerMoveRuntimeSemanticScore(Profile, TEXT("speed"));
        const float Range = KillerMoveRuntimeSemanticScore(Profile, TEXT("range"));
        const float Area = KillerMoveRuntimeSemanticScore(Profile, TEXT("area"));
        const float Amplification = KillerMoveRuntimeSemanticScore(Profile, TEXT("amplification"));
        const float Suppression = KillerMoveRuntimeSemanticScore(Profile, TEXT("suppression"));
        const float Stability = KillerMoveRuntimeSemanticScore(Profile, TEXT("stability"));
        const float Precision = KillerMoveRuntimeSemanticScore(Profile, TEXT("precision"));

        if (Suppression >= 0.65f) return EKillerMoveRole::Suppression;
        if (Speed >= 0.65f && Speed > Amplification + 0.10f) return EKillerMoveRole::Routing;
        if (Precision >= 0.65f && Precision > Amplification + 0.15f) return EKillerMoveRole::Targeting;
        if (Stability >= 0.65f) return EKillerMoveRole::Stabilization;
        if (Area >= 0.70f && Area > Amplification + 0.15f) return EKillerMoveRole::Boundary;
        if (Range >= 0.70f && Range > Amplification + 0.15f) return EKillerMoveRole::Routing;
        return EKillerMoveRole::Amplification;
    }

    const FGuProjectileMechanic* KillerMoveRuntimeFindProjectile(const UGuDefinition* Definition)
    {
        if (!Definition) return nullptr;
        for (const TInstancedStruct<FGuMechanic>& Mechanic : Definition->Mechanics)
        {
            if (const FGuProjectileMechanic* Projectile = Mechanic.GetPtr<FGuProjectileMechanic>()) return Projectile;
        }
        return nullptr;
    }

    float KillerMoveRuntimeEssenceCost(const UGuDefinition* Definition)
    {
        if (!Definition) return 0.0f;
        for (const TInstancedStruct<FGuMechanic>& Mechanic : Definition->Mechanics)
        {
            if (const FGuEssenceCostMechanic* Cost = Mechanic.GetPtr<FGuEssenceCostMechanic>()) return FMath::Max(0.0f, Cost->Cost);
        }
        return 0.0f;
    }

    template <typename TMechanic>
    void KillerMoveRuntimeAddMechanic(UGuDefinition* Definition, const TMechanic& Value)
    {
        if (!Definition) return;
        TInstancedStruct<FGuMechanic> Entry;
        Entry.InitializeAs<TMechanic>(Value);
        Definition->Mechanics.Add(MoveTemp(Entry));
    }

    bool KillerMoveRuntimeParseJson(const FString& Json, TSharedPtr<FJsonObject>& OutObject)
    {
        OutObject.Reset();
        if (Json.IsEmpty()) return false;
        const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
        return FJsonSerializer::Deserialize(Reader, OutObject) && OutObject.IsValid();
    }

    float KillerMoveRuntimeJsonNumber(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, const float Fallback = 0.0f)
    {
        if (!Object.IsValid()) return Fallback;
        double Value = static_cast<double>(Fallback);
        return Object->TryGetNumberField(Field, Value) ? static_cast<float>(Value) : Fallback;
    }

    FString KillerMoveRuntimeJsonString(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field)
    {
        FString Value;
        return Object.IsValid() && Object->TryGetStringField(Field, Value) ? Value : FString();
    }

    void KillerMoveRuntimePopulateConcreteEffects(const FGuDefinitionRecord& Definition, FKillerMoveEffectNode& Node)
    {
        Node.Effects.Reset();
        for (const FGuMechanicSpec& Mechanic : Definition.Mechanics)
        {
            TSharedPtr<FJsonObject> Json;
            KillerMoveRuntimeParseJson(Mechanic.ConfigJson, Json);

            FKillerMoveConcreteEffect Effect;
            Effect.SourceMechanic = Mechanic.Type;

            if (Mechanic.Type == TEXT("projectile"))
            {
                Effect.Type = EKillerMoveConcreteEffectType::ProjectileCarrier;
                Effect.Magnitude = KillerMoveRuntimeJsonNumber(Json, TEXT("speed"));
                Effect.Range = KillerMoveRuntimeJsonNumber(Json, TEXT("range"), Definition.EffectProfile.Range);
                Effect.Radius = KillerMoveRuntimeJsonNumber(Json, TEXT("radius"), Definition.EffectProfile.Area);
                Effect.Detail = FString::Printf(
                    TEXT("Projectile carrier: speed %.0f, range %.0f."),
                    Effect.Magnitude,
                    Effect.Range);
            }
            else if (Mechanic.Type == TEXT("damage"))
            {
                Effect.Type = EKillerMoveConcreteEffectType::Damage;
                Effect.Magnitude = KillerMoveRuntimeJsonNumber(Json, TEXT("damage"), Definition.EffectProfile.Magnitude);
                Effect.Detail = FString::Printf(TEXT("%.1f damage."), Effect.Magnitude);
            }
            else if (Mechanic.Type == TEXT("damage_over_time"))
            {
                Effect.Type = EKillerMoveConcreteEffectType::DamageOverTime;
                Effect.Magnitude = KillerMoveRuntimeJsonNumber(Json, TEXT("damagePerTick"));
                Effect.Range = KillerMoveRuntimeJsonNumber(Json, TEXT("tickInterval"));
                Effect.SecondaryMagnitude = KillerMoveRuntimeJsonNumber(Json, TEXT("duration"));
                Effect.Detail = FString::Printf(TEXT("%.1f damage every %.2fs for %.1fs."), Effect.Magnitude, Effect.Range, Effect.SecondaryMagnitude);
            }
            else if (Mechanic.Type == TEXT("knockback"))
            {
                Effect.Type = EKillerMoveConcreteEffectType::Knockback;
                Effect.Magnitude = KillerMoveRuntimeJsonNumber(Json, TEXT("strength"));
                Effect.SecondaryMagnitude = KillerMoveRuntimeJsonNumber(Json, TEXT("verticalStrength"));
                Effect.Detail = FString::Printf(
                    TEXT("%.0f knockback (vertical %.0f)."),
                    Effect.Magnitude,
                    Effect.SecondaryMagnitude);
            }
            else if (Mechanic.Type == TEXT("displacement"))
            {
                Effect.Type = EKillerMoveConcreteEffectType::Displacement;
                Effect.Magnitude = KillerMoveRuntimeJsonNumber(Json, TEXT("strength"));
                Effect.SecondaryMagnitude = KillerMoveRuntimeJsonNumber(Json, TEXT("verticalStrength"));
                Effect.Detail = FString::Printf(TEXT("Displacement %.0f (vertical %.0f)."), Effect.Magnitude, Effect.SecondaryMagnitude);
            }
            else if (Mechanic.Type == TEXT("melee"))
            {
                Effect.Type = EKillerMoveConcreteEffectType::MeleeCarrier;
                Effect.Range = KillerMoveRuntimeJsonNumber(Json, TEXT("range"), Definition.EffectProfile.Range);
                Effect.Radius = KillerMoveRuntimeJsonNumber(Json, TEXT("radius"));
                Effect.Magnitude = KillerMoveRuntimeJsonNumber(Json, TEXT("arcDegrees"));
                Effect.Detail = FString::Printf(TEXT("Melee carrier: range %.0f, radius %.0f."), Effect.Range, Effect.Radius);
            }
            else if (Mechanic.Type == TEXT("area"))
            {
                Effect.Type = EKillerMoveConcreteEffectType::AreaCarrier;
                Effect.Radius = KillerMoveRuntimeJsonNumber(Json, TEXT("radius"), Definition.EffectProfile.Area);
                Effect.Range = KillerMoveRuntimeJsonNumber(Json, TEXT("forwardOffset"));
                Effect.Detail = FString::Printf(TEXT("Area carrier: radius %.0f."), Effect.Radius);
            }
            else if (Mechanic.Type == TEXT("field"))
            {
                Effect.Type = EKillerMoveConcreteEffectType::FieldCarrier;
                Effect.Radius = KillerMoveRuntimeJsonNumber(Json, TEXT("radius"), Definition.EffectProfile.Area);
                Effect.Range = KillerMoveRuntimeJsonNumber(Json, TEXT("forwardOffset"));
                Effect.Magnitude = KillerMoveRuntimeJsonNumber(Json, TEXT("tickInterval"));
                Effect.SecondaryMagnitude = KillerMoveRuntimeJsonNumber(Json, TEXT("duration"));
                Effect.Detail = FString::Printf(TEXT("Field: radius %.0f, pulse %.2fs for %.1fs."), Effect.Radius, Effect.Magnitude, Effect.SecondaryMagnitude);
            }
            else if (Mechanic.Type == TEXT("heal"))
            {
                Effect.Type = EKillerMoveConcreteEffectType::Heal;
                Effect.Magnitude = KillerMoveRuntimeJsonNumber(Json, TEXT("amount"));
                Effect.Detail = FString::Printf(TEXT("%.1f healing."), Effect.Magnitude);
            }
            else if (Mechanic.Type == TEXT("heal_over_time"))
            {
                Effect.Type = EKillerMoveConcreteEffectType::HealOverTime;
                Effect.Magnitude = KillerMoveRuntimeJsonNumber(Json, TEXT("healPerTick"));
                Effect.Range = KillerMoveRuntimeJsonNumber(Json, TEXT("tickInterval"));
                Effect.SecondaryMagnitude = KillerMoveRuntimeJsonNumber(Json, TEXT("duration"));
                Effect.Detail = FString::Printf(TEXT("%.1f healing every %.2fs for %.1fs."), Effect.Magnitude, Effect.Range, Effect.SecondaryMagnitude);
            }
            else if (Mechanic.Type == TEXT("shield"))
            {
                Effect.Type = EKillerMoveConcreteEffectType::Shield;
                Effect.Magnitude = KillerMoveRuntimeJsonNumber(Json, TEXT("amount"));
                Effect.SecondaryMagnitude = KillerMoveRuntimeJsonNumber(Json, TEXT("duration"));
                Effect.Detail = FString::Printf(TEXT("%.1f shield for %.1fs."), Effect.Magnitude, Effect.SecondaryMagnitude);
            }
            else if (Mechanic.Type == TEXT("movement"))
            {
                Effect.Type = EKillerMoveConcreteEffectType::Movement;
                Effect.Magnitude = KillerMoveRuntimeJsonNumber(Json, TEXT("speedMultiplier"), 1.0f);
                Effect.Range = KillerMoveRuntimeJsonNumber(Json, TEXT("blinkDistance"));
                Effect.Detail = FString::Printf(TEXT("Movement effect x%.2f."), Effect.Magnitude);
            }
            else if (Mechanic.Type == TEXT("restriction"))
            {
                Effect.Type = EKillerMoveConcreteEffectType::Restriction;
                Effect.Magnitude = KillerMoveRuntimeJsonNumber(Json, TEXT("movementMultiplier"), 1.0f);
                Effect.SecondaryMagnitude = KillerMoveRuntimeJsonNumber(Json, TEXT("duration"));
                Effect.Detail = FString::Printf(TEXT("Restriction x%.2f for %.1fs."), Effect.Magnitude, Effect.SecondaryMagnitude);
            }
            else if (Mechanic.Type == TEXT("gu_suppression"))
            {
                Effect.Type = EKillerMoveConcreteEffectType::GuSuppression;
                Effect.SecondaryMagnitude = KillerMoveRuntimeJsonNumber(Json, TEXT("duration"));
                Effect.Detail = FString::Printf(TEXT("Gu suppression for %.1fs."), Effect.SecondaryMagnitude);
            }
            else if (Mechanic.Type == TEXT("essence_change"))
            {
                Effect.Type = EKillerMoveConcreteEffectType::EssenceChange;
                Effect.Magnitude = KillerMoveRuntimeJsonNumber(Json, TEXT("amount"));
                Effect.SecondaryMagnitude = KillerMoveRuntimeJsonNumber(Json, TEXT("mode"));
                Effect.Detail = FString::Printf(TEXT("Primeval essence change %.1f."), Effect.Magnitude);
            }
            else if (Mechanic.Type == TEXT("essence_regeneration"))
            {
                Effect.Type = EKillerMoveConcreteEffectType::EssenceRegeneration;
                Effect.Magnitude = KillerMoveRuntimeJsonNumber(Json, TEXT("flatPerSecond"));
                Effect.Range = KillerMoveRuntimeJsonNumber(Json, TEXT("percentPerSecond"));
                Effect.SecondaryMagnitude = KillerMoveRuntimeJsonNumber(Json, TEXT("duration"));
                Effect.Detail = FString::Printf(TEXT("Essence regeneration %.1f/s + %.2f%%/s for %.1fs."), Effect.Magnitude, Effect.Range, Effect.SecondaryMagnitude);
            }
            else if (Mechanic.Type == TEXT("cleanse"))
            {
                Effect.Type = EKillerMoveConcreteEffectType::Cleanse;
                Effect.Detail = TEXT("Cleanse harmful Gu states.");
            }
            else if (Mechanic.Type == TEXT("dispel"))
            {
                Effect.Type = EKillerMoveConcreteEffectType::Dispel;
                Effect.Detail = TEXT("Dispel beneficial Gu states.");
            }
            else if (Mechanic.Type == TEXT("conceal"))
            {
                Effect.Type = EKillerMoveConcreteEffectType::Concealment;
                Effect.Magnitude = KillerMoveRuntimeJsonNumber(Json, TEXT("detectionResistance"));
                Effect.SecondaryMagnitude = KillerMoveRuntimeJsonNumber(Json, TEXT("duration"));
                Effect.Detail = FString::Printf(TEXT("Concealment %.0f%% for %.1fs."), Effect.Magnitude * 100.0f, Effect.SecondaryMagnitude);
            }
            else if (Mechanic.Type == TEXT("reveal"))
            {
                Effect.Type = EKillerMoveConcreteEffectType::Reveal;
                Effect.Magnitude = KillerMoveRuntimeJsonNumber(Json, TEXT("strength"), 1.0f);
                Effect.Range = KillerMoveRuntimeJsonNumber(Json, TEXT("range"));
                Effect.SecondaryMagnitude = KillerMoveRuntimeJsonNumber(Json, TEXT("duration"));
                Effect.Detail = FString::Printf(TEXT("Reveal range %.0f for %.1fs."), Effect.Range, Effect.SecondaryMagnitude);
            }
            else if (Mechanic.Type == TEXT("chain"))
            {
                Effect.Type = EKillerMoveConcreteEffectType::Chain;
                Effect.Range = KillerMoveRuntimeJsonNumber(Json, TEXT("jumpRadius"));
                Effect.Magnitude = KillerMoveRuntimeJsonNumber(Json, TEXT("maxAdditionalTargets"));
                Effect.SecondaryMagnitude = KillerMoveRuntimeJsonNumber(Json, TEXT("magnitudeFalloff"), 1.0f);
                Effect.Detail = FString::Printf(TEXT("Chain to %.0f additional targets within %.0f."), Effect.Magnitude, Effect.Range);
            }
            else if (Mechanic.Type == TEXT("mark"))
            {
                Effect.Type = EKillerMoveConcreteEffectType::Mark;
                Effect.Magnitude = KillerMoveRuntimeJsonNumber(Json, TEXT("strength"));
                Effect.SecondaryMagnitude = KillerMoveRuntimeJsonNumber(Json, TEXT("duration"));
                Effect.Detail = FString::Printf(TEXT("Mark %s strength %.2f for %.1fs."), *KillerMoveRuntimeJsonString(Json, TEXT("markId")), Effect.Magnitude, Effect.SecondaryMagnitude);
            }
            else if (Mechanic.Type == TEXT("multitasking_boost"))
            {
                Effect.Type = EKillerMoveConcreteEffectType::AttentionBoost;
                Effect.Magnitude = KillerMoveRuntimeJsonNumber(Json, TEXT("slotsGranted"));
                Effect.SecondaryMagnitude = KillerMoveRuntimeJsonNumber(Json, TEXT("duration"));
                Effect.Detail = FString::Printf(TEXT("+%.0f attention slots for %.1fs."), Effect.Magnitude, Effect.SecondaryMagnitude);
            }
            else if (Mechanic.Type == TEXT("refinement_assistance"))
            {
                Effect.Type = EKillerMoveConcreteEffectType::RefinementAssist;
                Effect.Magnitude = KillerMoveRuntimeJsonNumber(Json, TEXT("progressPercent"));
                Effect.SecondaryMagnitude = KillerMoveRuntimeJsonNumber(Json, TEXT("qualityBonus"));
                Effect.Detail = FString::Printf(TEXT("Refinement assistance: +%.1f%% progress."), Effect.Magnitude);
            }
            else if (Mechanic.Type == TEXT("summon"))
            {
                Effect.Type = EKillerMoveConcreteEffectType::Summon;
                Effect.Magnitude = KillerMoveRuntimeJsonNumber(Json, TEXT("count"), 1.0f);
                Effect.Range = KillerMoveRuntimeJsonNumber(Json, TEXT("spawnRadius"));
                Effect.SecondaryMagnitude = KillerMoveRuntimeJsonNumber(Json, TEXT("lifetime"));
                Effect.Detail = FString::Printf(TEXT("Summon %.0f x %s."), Effect.Magnitude, *KillerMoveRuntimeJsonString(Json, TEXT("actorClass")));
            }
            else if (Mechanic.Type == TEXT("stat_modifier"))
            {
                Effect.Type = EKillerMoveConcreteEffectType::StatModifier;
                Effect.Magnitude = KillerMoveRuntimeJsonNumber(Json, TEXT("magnitude"));
                Effect.SecondaryMagnitude = KillerMoveRuntimeJsonNumber(Json, TEXT("duration"));
                const FString Stat = KillerMoveRuntimeJsonString(Json, TEXT("stat"));
                Effect.Detail = FString::Printf(
                    TEXT("%s %+.1f for %.1fs."),
                    Stat.IsEmpty() ? TEXT("Stat") : *Stat,
                    Effect.Magnitude,
                    Effect.SecondaryMagnitude);
            }
            else
            {
                continue;
            }

            Node.Effects.Add(MoveTemp(Effect));
        }
    }

    FString KillerMoveRuntimeEffectPreview(const FKillerMoveEffectGraph& Graph)
    {
        int32 Projectiles = 0;
        int32 MeleeCarriers = 0;
        int32 AreaCarriers = 0;
        int32 FieldCarriers = 0;
        int32 Damage = 0;
        int32 DamageOverTime = 0;
        int32 Knockback = 0;
        int32 Displacement = 0;
        int32 Healing = 0;
        int32 HealingOverTime = 0;
        int32 Shields = 0;
        int32 Movement = 0;
        int32 Restrictions = 0;
        int32 GuSuppression = 0;
        int32 EssenceEffects = 0;
        int32 Cleanses = 0;
        int32 Dispels = 0;
        int32 Chains = 0;
        int32 Marks = 0;
        int32 AttentionBoosts = 0;
        int32 RefinementAssists = 0;
        int32 Summons = 0;
        int32 Concealment = 0;
        int32 Reveal = 0;
        int32 Buffs = 0;
        float TotalDamage = 0.0f;
        float TotalHealing = 0.0f;
        float TotalShield = 0.0f;
        float StrongestKnockback = 0.0f;

        for (const FKillerMoveEffectNode& Node : Graph.Nodes)
        {
            for (const FKillerMoveConcreteEffect& Effect : Node.Effects)
            {
                switch (Effect.Type)
                {
                case EKillerMoveConcreteEffectType::ProjectileCarrier:
                    ++Projectiles;
                    break;
                case EKillerMoveConcreteEffectType::MeleeCarrier:
                    ++MeleeCarriers;
                    break;
                case EKillerMoveConcreteEffectType::AreaCarrier:
                    ++AreaCarriers;
                    break;
                case EKillerMoveConcreteEffectType::FieldCarrier:
                    ++FieldCarriers;
                    break;
                case EKillerMoveConcreteEffectType::Damage:
                    ++Damage;
                    TotalDamage += FMath::Max(0.0f, Effect.Magnitude);
                    break;
                case EKillerMoveConcreteEffectType::DamageOverTime:
                    ++DamageOverTime;
                    break;
                case EKillerMoveConcreteEffectType::Knockback:
                    ++Knockback;
                    StrongestKnockback = FMath::Max(StrongestKnockback, Effect.Magnitude);
                    break;
                case EKillerMoveConcreteEffectType::Displacement:
                    ++Displacement;
                    break;
                case EKillerMoveConcreteEffectType::Heal:
                    ++Healing;
                    TotalHealing += FMath::Max(0.0f, Effect.Magnitude);
                    break;
                case EKillerMoveConcreteEffectType::HealOverTime:
                    ++HealingOverTime;
                    break;
                case EKillerMoveConcreteEffectType::Shield:
                    ++Shields;
                    TotalShield += FMath::Max(0.0f, Effect.Magnitude);
                    break;
                case EKillerMoveConcreteEffectType::Movement:
                    ++Movement;
                    break;
                case EKillerMoveConcreteEffectType::Restriction:
                    ++Restrictions;
                    break;
                case EKillerMoveConcreteEffectType::GuSuppression:
                    ++GuSuppression;
                    break;
                case EKillerMoveConcreteEffectType::EssenceChange:
                case EKillerMoveConcreteEffectType::EssenceRegeneration:
                    ++EssenceEffects;
                    break;
                case EKillerMoveConcreteEffectType::Cleanse:
                    ++Cleanses;
                    break;
                case EKillerMoveConcreteEffectType::Dispel:
                    ++Dispels;
                    break;
                case EKillerMoveConcreteEffectType::Chain:
                    ++Chains;
                    break;
                case EKillerMoveConcreteEffectType::Mark:
                    ++Marks;
                    break;
                case EKillerMoveConcreteEffectType::AttentionBoost:
                    ++AttentionBoosts;
                    break;
                case EKillerMoveConcreteEffectType::RefinementAssist:
                    ++RefinementAssists;
                    break;
                case EKillerMoveConcreteEffectType::Summon:
                    ++Summons;
                    break;
                case EKillerMoveConcreteEffectType::Concealment:
                    ++Concealment;
                    break;
                case EKillerMoveConcreteEffectType::Reveal:
                    ++Reveal;
                    break;
                case EKillerMoveConcreteEffectType::StatModifier:
                    ++Buffs;
                    break;
                default:
                    break;
                }
            }
        }

        TArray<FString> Parts;
        if (Projectiles > 0) Parts.Add(TEXT("projectile carrier"));
        if (MeleeCarriers > 0) Parts.Add(TEXT("melee carrier"));
        if (AreaCarriers > 0) Parts.Add(TEXT("area carrier"));
        if (FieldCarriers > 0) Parts.Add(TEXT("persistent field"));
        if (Damage > 0) Parts.Add(FString::Printf(TEXT("%.1f raw damage payload"), TotalDamage));
        if (DamageOverTime > 0) Parts.Add(TEXT("damage over time"));
        if (Knockback > 0) Parts.Add(FString::Printf(TEXT("%.0f knockback"), StrongestKnockback));
        if (Displacement > 0) Parts.Add(TEXT("displacement"));
        if (Healing > 0) Parts.Add(FString::Printf(TEXT("%.1f healing"), TotalHealing));
        if (HealingOverTime > 0) Parts.Add(TEXT("healing over time"));
        if (Shields > 0) Parts.Add(FString::Printf(TEXT("%.1f shield"), TotalShield));
        if (Movement > 0) Parts.Add(TEXT("movement"));
        if (Restrictions > 0) Parts.Add(TEXT("restriction"));
        if (GuSuppression > 0) Parts.Add(TEXT("Gu suppression"));
        if (EssenceEffects > 0) Parts.Add(TEXT("essence manipulation"));
        if (Cleanses > 0) Parts.Add(TEXT("cleanse"));
        if (Dispels > 0) Parts.Add(TEXT("dispel"));
        if (Chains > 0) Parts.Add(TEXT("chain delivery"));
        if (Marks > 0) Parts.Add(TEXT("target mark"));
        if (AttentionBoosts > 0) Parts.Add(TEXT("attention boost"));
        if (RefinementAssists > 0) Parts.Add(TEXT("refinement assistance"));
        if (Summons > 0) Parts.Add(TEXT("summon"));
        if (Concealment > 0) Parts.Add(TEXT("concealment"));
        if (Reveal > 0) Parts.Add(TEXT("investigation/reveal"));
        if (Buffs > 0) Parts.Add(FString::Printf(TEXT("%d stat modifier%s"), Buffs, Buffs == 1 ? TEXT("") : TEXT("s")));
        return Parts.IsEmpty() ? TEXT("No concrete executable effect is compiled yet.") : FString::Join(Parts, TEXT(" | "));
    }

    const UGuSystemConfig* KillerMoveRuntimeFindSystemConfig(UAbilitySystemComponent* ASC)
    {
        if (!ASC) return nullptr;

        for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
        {
            if (const UGA_GuAbility* AbilityCDO = Cast<UGA_GuAbility>(Spec.Ability.Get()))
            {
                if (UGuSystemConfig* Config = AbilityCDO->GetGuSystemConfig()) return Config;
            }

            if (UGameplayAbility* Instance = Spec.GetPrimaryInstance())
            {
                if (const UGA_GuAbility* GuAbility = Cast<UGA_GuAbility>(Instance))
                {
                    if (UGuSystemConfig* Config = GuAbility->GetGuSystemConfig()) return Config;
                }
            }
        }

        return nullptr;
    }

    bool KillerMoveRuntimeHasImpactMechanic(const UGuDefinition* Definition)
    {
        if (!Definition) return false;
        for (const TInstancedStruct<FGuMechanic>& Mechanic : Definition->Mechanics)
        {
            if (Mechanic.GetPtr<FGuDamageMechanic>()
                || Mechanic.GetPtr<FGuKnockbackMechanic>()
                || Mechanic.GetPtr<FGuDisplacementMechanic>()
                || Mechanic.GetPtr<FGuRestrictionMechanic>()
                || Mechanic.GetPtr<FGuGuSuppressionMechanic>()
                || Mechanic.GetPtr<FGuChainMechanic>()
                || Mechanic.GetPtr<FGuMarkMechanic>()) return true;
            if (const FGuHealMechanic* Value = Mechanic.GetPtr<FGuHealMechanic>(); Value && Value->Recipient == EGuMechanicRecipient::ImpactTarget) return true;
            if (const FGuShieldMechanic* Value = Mechanic.GetPtr<FGuShieldMechanic>(); Value && Value->Recipient == EGuMechanicRecipient::ImpactTarget) return true;
            if (const FGuMovementMechanic* Value = Mechanic.GetPtr<FGuMovementMechanic>(); Value && Value->Recipient == EGuMechanicRecipient::ImpactTarget) return true;
            if (const FGuDamageOverTimeMechanic* Value = Mechanic.GetPtr<FGuDamageOverTimeMechanic>(); Value && Value->Recipient == EGuMechanicRecipient::ImpactTarget) return true;
            if (const FGuHealOverTimeMechanic* Value = Mechanic.GetPtr<FGuHealOverTimeMechanic>(); Value && Value->Recipient == EGuMechanicRecipient::ImpactTarget) return true;
            if (const FGuEssenceChangeMechanic* Value = Mechanic.GetPtr<FGuEssenceChangeMechanic>(); Value && Value->Recipient == EGuMechanicRecipient::ImpactTarget) return true;
            if (const FGuEssenceRegenerationMechanic* Value = Mechanic.GetPtr<FGuEssenceRegenerationMechanic>(); Value && Value->Recipient == EGuMechanicRecipient::ImpactTarget) return true;
            if (const FGuCleanseMechanic* Value = Mechanic.GetPtr<FGuCleanseMechanic>(); Value && Value->Recipient == EGuMechanicRecipient::ImpactTarget) return true;
            if (const FGuDispelMechanic* Value = Mechanic.GetPtr<FGuDispelMechanic>(); Value && Value->Recipient == EGuMechanicRecipient::ImpactTarget) return true;
        }
        return false;
    }

    bool KillerMoveRuntimeHasActivationMechanic(const UGuDefinition* Definition)
    {
        if (!Definition) return false;
        for (const TInstancedStruct<FGuMechanic>& Mechanic : Definition->Mechanics)
        {
            if (Mechanic.GetPtr<FGuBuffMechanic>()
                || Mechanic.GetPtr<FGuConcealmentMechanic>()
                || Mechanic.GetPtr<FGuRevealMechanic>()
                || Mechanic.GetPtr<FGuAttentionBoostMechanic>()
                || Mechanic.GetPtr<FGuFieldMechanic>()
                || Mechanic.GetPtr<FGuSummonMechanic>()) return true;
            if (const FGuHealMechanic* Value = Mechanic.GetPtr<FGuHealMechanic>(); Value && Value->Recipient == EGuMechanicRecipient::Self) return true;
            if (const FGuShieldMechanic* Value = Mechanic.GetPtr<FGuShieldMechanic>(); Value && Value->Recipient == EGuMechanicRecipient::Self) return true;
            if (const FGuMovementMechanic* Value = Mechanic.GetPtr<FGuMovementMechanic>(); Value && Value->Recipient == EGuMechanicRecipient::Self) return true;
            if (const FGuDamageOverTimeMechanic* Value = Mechanic.GetPtr<FGuDamageOverTimeMechanic>(); Value && Value->Recipient == EGuMechanicRecipient::Self) return true;
            if (const FGuHealOverTimeMechanic* Value = Mechanic.GetPtr<FGuHealOverTimeMechanic>(); Value && Value->Recipient == EGuMechanicRecipient::Self) return true;
            if (const FGuEssenceChangeMechanic* Value = Mechanic.GetPtr<FGuEssenceChangeMechanic>(); Value && Value->Recipient == EGuMechanicRecipient::Self) return true;
            if (const FGuEssenceRegenerationMechanic* Value = Mechanic.GetPtr<FGuEssenceRegenerationMechanic>(); Value && Value->Recipient == EGuMechanicRecipient::Self) return true;
            if (const FGuCleanseMechanic* Value = Mechanic.GetPtr<FGuCleanseMechanic>(); Value && Value->Recipient == EGuMechanicRecipient::Self) return true;
            if (const FGuDispelMechanic* Value = Mechanic.GetPtr<FGuDispelMechanic>(); Value && Value->Recipient == EGuMechanicRecipient::Self) return true;
        }
        return false;
    }

    bool KillerMoveRuntimeHasFieldCarrier(const UGuDefinition* Definition)
    {
        if (!Definition) return false;
        for (const TInstancedStruct<FGuMechanic>& Mechanic : Definition->Mechanics)
        {
            if (Mechanic.GetPtr<FGuFieldMechanic>()) return true;
        }
        return false;
    }

    int32 KillerMoveRuntimeExecuteOverlapImpact(
        UWorld* World,
        UGuDefinition* Composite,
        UAbilitySystemComponent* SourceASC,
        AActor* SourceActor,
        const FVector& Center,
        const float Radius)
    {
        if (!World || !Composite || !SourceASC || !SourceActor || Radius <= 0.0f) return 0;

        FCollisionObjectQueryParams Objects;
        Objects.AddObjectTypesToQuery(ECC_Pawn);
        Objects.AddObjectTypesToQuery(ECC_WorldDynamic);

        FCollisionQueryParams Params(SCENE_QUERY_STAT(KillerMoveArea), false, SourceActor);
        TArray<FOverlapResult> Overlaps;
        World->OverlapMultiByObjectType(
            Overlaps,
            Center,
            FQuat::Identity,
            Objects,
            FCollisionShape::MakeSphere(Radius),
            Params);

        TSet<AActor*> HitActors;
        int32 ExecutedTargets = 0;
        for (const FOverlapResult& Overlap : Overlaps)
        {
            AActor* Target = Overlap.GetActor();
            if (!Target || Target == SourceActor || HitActors.Contains(Target)) continue;
            HitActors.Add(Target);

            FHitResult SyntheticHit;
            SyntheticHit.TraceStart = SourceActor->GetActorLocation();
            SyntheticHit.TraceEnd = Target->GetActorLocation();
            SyntheticHit.Location = Target->GetActorLocation();
            SyntheticHit.ImpactPoint = Target->GetActorLocation();
            SyntheticHit.ImpactNormal = (SourceActor->GetActorLocation() - Target->GetActorLocation()).GetSafeNormal();

            if (UGuExecutionLibrary::ExecuteImpact(Composite, SourceASC, Target, SyntheticHit))
            {
                ++ExecutedTargets;
            }
        }

        return ExecutedTargets;
    }

    int32 KillerMoveRuntimeExecuteSweepImpact(
        UWorld* World,
        UGuDefinition* Composite,
        UAbilitySystemComponent* SourceASC,
        AActor* SourceActor,
        const FVector& Start,
        const FVector& End,
        const float Radius)
    {
        if (!World || !Composite || !SourceASC || !SourceActor || Radius <= 0.0f) return 0;

        FCollisionObjectQueryParams Objects;
        Objects.AddObjectTypesToQuery(ECC_Pawn);
        Objects.AddObjectTypesToQuery(ECC_WorldDynamic);

        FCollisionQueryParams Params(SCENE_QUERY_STAT(KillerMoveSweep), false, SourceActor);
        TArray<FHitResult> Hits;
        World->SweepMultiByObjectType(
            Hits,
            Start,
            End,
            FQuat::Identity,
            Objects,
            FCollisionShape::MakeSphere(Radius),
            Params);

        TSet<AActor*> HitActors;
        int32 ExecutedTargets = 0;
        for (const FHitResult& Hit : Hits)
        {
            AActor* Target = Hit.GetActor();
            if (!Target || Target == SourceActor || HitActors.Contains(Target)) continue;
            HitActors.Add(Target);
            if (UGuExecutionLibrary::ExecuteImpact(Composite, SourceASC, Target, Hit))
            {
                ++ExecutedTargets;
            }
        }

        return ExecutedTargets;
    }
}

void UKillerMoveSubsystem::Deinitialize()
{
    if (UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr)
    {
        for (TPair<FString, FRuntimeSession>& Pair : Sessions)
        {
            World->GetTimerManager().ClearTimer(Pair.Value.DeadlineTimer);
            ReleaseAllAttention(Pair.Value);
        }
    }
    Sessions.Reset();
    Super::Deinitialize();
}

float UKillerMoveSubsystem::ServerWorldTime() const
{
    const UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
    if (!World) return 0.0f;
    if (const AGameStateBase* GS = World->GetGameState()) return GS->GetServerWorldTimeSeconds();
    return World->GetTimeSeconds();
}

float UKillerMoveSubsystem::EffectiveWindow(const AGuPlayerState* PlayerState, const FKillerMoveInputStep& Step) const
{
    const int32 FocusLevel = PlayerState && PlayerState->MentalResources
        ? FMath::Max(1, PlayerState->MentalResources->FocusControlLevel)
        : 1;
    const float FocusBonus = FMath::Min(0.25f, static_cast<float>(FocusLevel - 1) * 0.020f);
    return FMath::Clamp(Step.TimingWindow + FocusBonus, 0.08f, 2.0f);
}

FName UKillerMoveSubsystem::AttentionKey(const FRuntimeSession& Session, const int32 SlotIndex) const
{
    return FName(*FString::Printf(TEXT("killer-move:%s:%s:%d"), *Session.OwnerId, *Session.SessionId.ToString(EGuidFormats::Digits), SlotIndex));
}

FName UKillerMoveSubsystem::BranchForRole(const EKillerMoveRole Role)
{
    switch (Role)
    {
    case EKillerMoveRole::Core:
    case EKillerMoveRole::Output:
    case EKillerMoveRole::Amplification:
    case EKillerMoveRole::Suppression:
    case EKillerMoveRole::Concealment:
        return TEXT("phenomenon");
    case EKillerMoveRole::Medium:
    case EKillerMoveRole::Link:
    case EKillerMoveRole::Routing:
    case EKillerMoveRole::Boundary:
    case EKillerMoveRole::Anchor:
    case EKillerMoveRole::Conversion:
    case EKillerMoveRole::Switching:
        return TEXT("delivery");
    case EKillerMoveRole::Targeting:
    case EKillerMoveRole::InvestigationSensor:
    case EKillerMoveRole::RecognitionValidation:
        return TEXT("targeting");
    case EKillerMoveRole::Timing:
    case EKillerMoveRole::Trigger:
    case EKillerMoveRole::Storage:
    case EKillerMoveRole::Termination:
        return TEXT("timing");
    case EKillerMoveRole::Stabilization:
    case EKillerMoveRole::Safety:
    case EKillerMoveRole::Buffer:
    case EKillerMoveRole::Recovery:
        return TEXT("structure");
    case EKillerMoveRole::Fuel:
        return TEXT("resource");
    default:
        return TEXT("control");
    }
}

FName UKillerMoveSubsystem::RelationForRole(const EKillerMoveRole Role)
{
    switch (Role)
    {
    case EKillerMoveRole::Output: return TEXT("reinforces");
    case EKillerMoveRole::Amplification: return TEXT("amplifies");
    case EKillerMoveRole::Medium: return TEXT("carries");
    case EKillerMoveRole::Targeting: return TEXT("guides");
    case EKillerMoveRole::InvestigationSensor: return TEXT("detects");
    case EKillerMoveRole::Control: return TEXT("controls");
    case EKillerMoveRole::Timing: return TEXT("schedules");
    case EKillerMoveRole::Trigger: return TEXT("triggers");
    case EKillerMoveRole::Boundary: return TEXT("bounds");
    case EKillerMoveRole::Stabilization: return TEXT("stabilizes");
    case EKillerMoveRole::Safety: return TEXT("safeguards");
    case EKillerMoveRole::Buffer: return TEXT("buffers");
    case EKillerMoveRole::Subordinate: return TEXT("commands");
    case EKillerMoveRole::Conversion: return TEXT("converts");
    case EKillerMoveRole::Fuel: return TEXT("fuels");
    case EKillerMoveRole::Storage: return TEXT("stores");
    case EKillerMoveRole::Routing: return TEXT("routes");
    case EKillerMoveRole::Anchor: return TEXT("anchors");
    case EKillerMoveRole::Link: return TEXT("binds");
    case EKillerMoveRole::RecognitionValidation: return TEXT("validates");
    case EKillerMoveRole::Concealment: return TEXT("conceals");
    case EKillerMoveRole::Suppression: return TEXT("suppresses");
    case EKillerMoveRole::Recovery: return TEXT("recovers");
    case EKillerMoveRole::Termination: return TEXT("terminates");
    case EKillerMoveRole::Switching: return TEXT("switches");
    default: return TEXT("modifies");
    }
}

bool UKillerMoveSubsystem::BuildEffectGraph(FKillerMoveDefinitionRecord& InOutDefinition, FString& OutError) const
{
    OutError.Reset();
    InOutDefinition.EffectGraph = FKillerMoveEffectGraph();
    if (InOutDefinition.GuSlots.Num() < 1)
    {
        OutError = TEXT("A killer move requires at least one Gu slot.");
        return false;
    }

    UGuDefinitionRegistrySubsystem* Registry = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UGuDefinitionRegistrySubsystem>()
        : nullptr;
    if (!Registry)
    {
        OutError = TEXT("Gu definition registry is unavailable.");
        return false;
    }

    int32 CoreIndex = INDEX_NONE;
    int32 FirstMediumIndex = INDEX_NONE;
    for (int32 Index = 0; Index < InOutDefinition.GuSlots.Num(); ++Index)
    {
        const FKillerMoveGuSlot& Slot = InOutDefinition.GuSlots[Index];
        if (Slot.SlotId.IsNone())
        {
            OutError = FString::Printf(TEXT("Killer-move slot %d has no SlotId."), Index + 1);
            return false;
        }
        if (Slot.GuDefinitionId.IsNone())
        {
            OutError = FString::Printf(TEXT("Killer-move slot '%s' has no Gu definition."), *Slot.SlotId.ToString());
            return false;
        }
        FGuDefinitionRecord GuDefinition;
        if (!Registry->GetDefinition(Slot.GuDefinitionId, GuDefinition))
        {
            OutError = FString::Printf(TEXT("Unknown Gu definition '%s'."), *Slot.GuDefinitionId.ToString());
            return false;
        }

        FKillerMoveEffectNode Node;
        Node.NodeId = FName(*FString::Printf(TEXT("component:%s"), *Slot.SlotId.ToString()));
        Node.SlotId = Slot.SlotId;
        Node.GuDefinitionId = GuDefinition.Id;
        Node.Role = Index == 0 ? EKillerMoveRole::Core : Slot.Role;
        Node.Branch = BranchForRole(Node.Role);
        Node.Path = GuDefinition.Path;
        KillerMoveRuntimePopulateConcreteEffects(GuDefinition, Node);
        InOutDefinition.EffectGraph.Nodes.Add(Node);
        if (Node.Role == EKillerMoveRole::Core && CoreIndex == INDEX_NONE) CoreIndex = Index;
        if (Node.Role == EKillerMoveRole::Medium && FirstMediumIndex == INDEX_NONE) FirstMediumIndex = Index;
    }

    if (CoreIndex == INDEX_NONE) CoreIndex = 0;
    InOutDefinition.EffectGraph.Nodes[CoreIndex].Role = EKillerMoveRole::Core;
    InOutDefinition.EffectGraph.Nodes[CoreIndex].Branch = BranchForRole(EKillerMoveRole::Core);
    InOutDefinition.EffectGraph.RootNodeId = InOutDefinition.EffectGraph.Nodes[CoreIndex].NodeId;

    const FKillerMoveEffectNode& Core = InOutDefinition.EffectGraph.Nodes[CoreIndex];
    const int32 DeliveryTargetIndex = FirstMediumIndex != INDEX_NONE ? FirstMediumIndex : CoreIndex;

    for (int32 Index = 0; Index < InOutDefinition.EffectGraph.Nodes.Num(); ++Index)
    {
        if (Index == CoreIndex) continue;
        const FKillerMoveEffectNode& Node = InOutDefinition.EffectGraph.Nodes[Index];
        int32 TargetIndex = CoreIndex;
        if (Node.Role == EKillerMoveRole::Link || IsDeliveryTargetRole(Node.Role)) TargetIndex = DeliveryTargetIndex;
        if (TargetIndex == Index) TargetIndex = CoreIndex;
        const FKillerMoveEffectNode& Target = InOutDefinition.EffectGraph.Nodes[TargetIndex];

        FKillerMoveEffectEdge Edge;
        Edge.Relation = RelationForRole(Node.Role);
        if (Node.Role == EKillerMoveRole::Medium)
        {
            // Browser parity: the core phenomenon flows into the carrier/medium.
            Edge.FromNodeId = Core.NodeId;
            Edge.ToNodeId = Node.NodeId;
            Edge.Label = FString::Printf(TEXT("%s is carried by %s."), *Core.SlotId.ToString(), *Node.SlotId.ToString());
        }
        else
        {
            Edge.FromNodeId = Node.NodeId;
            Edge.ToNodeId = Target.NodeId;
            Edge.Label = FString::Printf(TEXT("%s %s %s."), *Node.SlotId.ToString(), *Edge.Relation.ToString(), *Target.SlotId.ToString());
        }
        InOutDefinition.EffectGraph.Edges.Add(Edge);
    }
    return true;
}

bool UKillerMoveSubsystem::ResolvePhysicalGu(FRuntimeSession& Session, FString& OutError)
{
    UGuEntitySubsystem* Entities = GetGameInstance() ? GetGameInstance()->GetSubsystem<UGuEntitySubsystem>() : nullptr;
    if (!Entities)
    {
        OutError = TEXT("Gu ECS is unavailable.");
        return false;
    }

    Session.BoundGuEntities.Reset();
    TSet<FGuid> UsedEntities;
    for (const FKillerMoveGuSlot& Slot : Session.Definition.GuSlots)
    {
        FGuid EntityId = Slot.PreferredEntityId;
        if (EntityId.IsValid())
        {
            const FOwnedByComponent* Owner = Entities->GetOwnedBy(EntityId);
            const FGuInstanceComponent* Instance = Entities->GetGuInstance(EntityId);
            const FGuPlacementComponent* Placement = Entities->GetGuPlacement(EntityId);
            if (!Owner || Owner->OwnerId != Session.OwnerId || !Instance || Instance->DefinitionId != Slot.GuDefinitionId
                || !Placement || Placement->Container != EGuContainer::Aperture)
            {
                EntityId = FGuid();
            }
        }
        if (!EntityId.IsValid())
        {
            Entities->FindOwnedGuInstance(Slot.GuDefinitionId, Session.OwnerId, EGuContainer::Aperture, EntityId, true);
        }
        if (!EntityId.IsValid())
        {
            if (Slot.bRequired)
            {
                OutError = FString::Printf(TEXT("Required Gu '%s' is not alive in the aperture."), *Slot.GuDefinitionId.ToString());
                return false;
            }
            Session.BoundGuEntities.Add(FGuid());
            continue;
        }
        if (UsedEntities.Contains(EntityId))
        {
            OutError = FString::Printf(TEXT("One physical Gu cannot fill multiple killer-move slots (%s)."), *Slot.SlotId.ToString());
            return false;
        }
        FString CanUseError;
        if (!Entities->CanUseGu(EntityId, CanUseError))
        {
            OutError = FString::Printf(TEXT("%s cannot participate: %s"), *Slot.SlotId.ToString(), *CanUseError);
            return false;
        }
        UsedEntities.Add(EntityId);
        Session.BoundGuEntities.Add(EntityId);
    }
    return true;
}

bool UKillerMoveSubsystem::BeginKillerMoveAsset(AGuPlayerState* PlayerState, const UKillerMoveDefinition* Definition, FString& OutError)
{
    if (!Definition)
    {
        OutError = TEXT("No killer-move definition supplied.");
        return false;
    }
    return BeginKillerMove(PlayerState, Definition->Definition, OutError);
}

bool UKillerMoveSubsystem::BeginKillerMove(AGuPlayerState* PlayerState, const FKillerMoveDefinitionRecord& Definition, FString& OutError)
{
    OutError.Reset();
    if (!PlayerState || !PlayerState->HasAuthority())
    {
        OutError = TEXT("Killer moves must begin on the authority.");
        return false;
    }
    if (PlayerState->DomainCharacterId.IsEmpty())
    {
        OutError = TEXT("Player has no domain character ID.");
        return false;
    }
    if (HasActiveKillerMove(PlayerState->DomainCharacterId))
    {
        OutError = TEXT("A killer move is already being formed.");
        return false;
    }

    if (const AController* OwningController = Cast<AController>(PlayerState->GetOwner()))
    {
        if (const APawn* Pawn = OwningController->GetPawn())
        {
            if (const UGuRuntimeEffectComponent* Runtime = Pawn->FindComponentByClass<UGuRuntimeEffectComponent>(); Runtime && Runtime->IsGuSuppressed())
            {
                OutError = TEXT("Gu suppression prevents killer-move formation.");
                return false;
            }
        }
    }

    FKillerMoveDefinitionRecord Compiled = Definition;
    if (Compiled.Id.IsNone()) Compiled.Id = TEXT("runtime_killer_move");
    if (Compiled.Name.IsEmpty()) Compiled.Name = FText::FromName(Compiled.Id);
    if (Compiled.GuSlots.Num() < 1 || Compiled.Choreography.Num() < 1)
    {
        OutError = TEXT("A killer move needs Gu slots and an activation choreography.");
        return false;
    }
    Compiled.GuSlots[0].Role = EKillerMoveRole::Core;
    if (!BuildEffectGraph(Compiled, OutError)) return false;

    Compiled.Choreography.Sort([](const FKillerMoveInputStep& A, const FKillerMoveInputStep& B)
    {
        return A.TargetTime < B.TargetTime;
    });

    FRuntimeSession Session;
    Session.SessionId = FGuid::NewGuid();
    Session.PlayerState = PlayerState;
    Session.OwnerId = PlayerState->DomainCharacterId;
    Session.Definition = Compiled;
    Session.StartedServerWorldTime = ServerWorldTime();
    if (!ResolvePhysicalGu(Session, OutError)) return false;

    const FString OwnerId = Session.OwnerId;
    Sessions.Add(OwnerId, MoveTemp(Session));
    FRuntimeSession& Stored = Sessions.FindChecked(OwnerId);
    PushPublicState(Stored, TEXT("Killer move forming. Follow the timed Gu inputs."));
    ScheduleDeadline(Stored);
    return true;
}

bool UKillerMoveSubsystem::HasActiveKillerMove(const FString& OwnerId) const
{
    const FRuntimeSession* Session = Sessions.Find(OwnerId);
    return Session && Session->StepIndex < Session->Definition.Choreography.Num();
}

void UKillerMoveSubsystem::ScheduleDeadline(FRuntimeSession& Session)
{
    UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
    if (!World || !Session.Definition.Choreography.IsValidIndex(Session.StepIndex)) return;

    World->GetTimerManager().ClearTimer(Session.DeadlineTimer);
    const FKillerMoveInputStep& Step = Session.Definition.Choreography[Session.StepIndex];
    const float Window = EffectiveWindow(Session.PlayerState.Get(), Step);
    const float Deadline = Session.StartedServerWorldTime + Step.TargetTime + Window * 1.5f;
    const float Delay = FMath::Max(0.01f, Deadline - ServerWorldTime());
    FTimerDelegate Delegate;
    Delegate.BindUObject(this, &UKillerMoveSubsystem::HandleDeadline, Session.OwnerId);
    World->GetTimerManager().SetTimer(Session.DeadlineTimer, Delegate, Delay, false);
}

void UKillerMoveSubsystem::HandleDeadline(FString OwnerId)
{
    FRuntimeSession* Session = Sessions.Find(OwnerId);
    if (!Session) return;
    FString Error;
    AdvancePastMissedStep(*Session, Error);
}

bool UKillerMoveSubsystem::AdvancePastMissedStep(FRuntimeSession& Session, FString& OutError)
{
    if (!Session.Definition.Choreography.IsValidIndex(Session.StepIndex)) return false;
    const FKillerMoveInputStep& Step = Session.Definition.Choreography[Session.StepIndex];
    if (Step.bCritical)
    {
        OutError = FString::Printf(TEXT("Missed critical timing: %s %s."),
            Step.Event == EKillerMoveInputEvent::Pressed ? TEXT("press") : TEXT("release"), *Step.SlotId.ToString());
        FinishSession(Session, EKillerMoveRunState::Failed, OutError);
        return false;
    }

    Session.Stability = FMath::Max(0.0f, Session.Stability - 30.0f);
    if (Step.Event == EKillerMoveInputEvent::Released)
    {
        const int32 SlotIndex = Session.Definition.GuSlots.IndexOfByPredicate([&Step](const FKillerMoveGuSlot& Slot)
        {
            return Slot.SlotId == Step.SlotId;
        });
        if (SlotIndex != INDEX_NONE && Session.HeldSlotIndices.Contains(SlotIndex))
        {
            if (AGuPlayerState* PS = Session.PlayerState.Get(); PS && PS->MentalResources)
            {
                PS->MentalResources->ReleaseAttention(AttentionKey(Session, SlotIndex));
            }
            Session.HeldSlotIndices.Remove(SlotIndex);
        }
    }
    ++Session.SkippedSteps;
    ++Session.StepIndex;
    if (Session.StepIndex >= Session.Definition.Choreography.Num())
    {
        CompleteSession(Session);
        return true;
    }
    PushPublicState(Session, TEXT("A non-critical Gu timing was missed; the formation continues imperfectly."));
    ScheduleDeadline(Session);
    return true;
}

bool UKillerMoveSubsystem::SubmitInput(AGuPlayerState* PlayerState, const int32 SlotIndex, const EKillerMoveInputEvent Event, FString& OutError)
{
    OutError.Reset();
    if (!PlayerState || !PlayerState->HasAuthority())
    {
        OutError = TEXT("Killer-move input must be resolved on the authority.");
        return false;
    }
    FRuntimeSession* Session = Sessions.Find(PlayerState->DomainCharacterId);
    if (!Session || !Session->Definition.Choreography.IsValidIndex(Session->StepIndex))
    {
        OutError = TEXT("No killer move is currently forming.");
        return false;
    }
    if (!Session->Definition.GuSlots.IsValidIndex(SlotIndex))
    {
        OutError = TEXT("Invalid killer-move Gu slot.");
        return false;
    }

    const FKillerMoveInputStep& Step = Session->Definition.Choreography[Session->StepIndex];
    const int32 ExpectedSlotIndex = Session->Definition.GuSlots.IndexOfByPredicate([&Step](const FKillerMoveGuSlot& Slot)
    {
        return Slot.SlotId == Step.SlotId;
    });
    const float Now = ServerWorldTime();
    const float Elapsed = Now - Session->StartedServerWorldTime;
    const float Window = EffectiveWindow(PlayerState, Step);
    const float SoftFailureWindow = Window * 1.5f;
    const float Offset = Elapsed - Step.TargetTime;

    if (SlotIndex != ExpectedSlotIndex || Event != Step.Event)
    {
        Session->Stability = FMath::Max(0.0f, Session->Stability - 15.0f);
        if (Session->Stability <= 0.0f)
        {
            FinishSession(*Session, EKillerMoveRunState::Failed, TEXT("The killer move collapsed under conflicting Gu operations."));
        }
        else
        {
            PushPublicState(*Session, TEXT("Wrong Gu operation. Formation destabilized."));
        }
        OutError = TEXT("Wrong killer-move input.");
        return false;
    }

    if (Offset < -SoftFailureWindow)
    {
        if (Event == EKillerMoveInputEvent::Released && Session->HeldSlotIndices.Contains(SlotIndex))
        {
            if (PlayerState->MentalResources) PlayerState->MentalResources->ReleaseAttention(AttentionKey(*Session, SlotIndex));
            Session->HeldSlotIndices.Remove(SlotIndex);
            Session->Stability = FMath::Max(0.0f, Session->Stability - 22.0f);
            const FString Message = FString::Printf(TEXT("%s was released too early."), *Step.SlotId.ToString());
            if (Step.bCritical) FinishSession(*Session, EKillerMoveRunState::Failed, Message);
            else
            {
                ++Session->SkippedSteps;
                ++Session->StepIndex;
                if (Session->StepIndex >= Session->Definition.Choreography.Num()) CompleteSession(*Session);
                else
                {
                    PushPublicState(*Session, Message);
                    ScheduleDeadline(*Session);
                }
            }
            OutError = Message;
            return false;
        }
        Session->Stability = FMath::Max(0.0f, Session->Stability - 8.0f);
        PushPublicState(*Session, TEXT("Too early. The Gu did not synchronize."));
        OutError = TEXT("Input was too early.");
        return false;
    }
    if (Offset > SoftFailureWindow)
    {
        return AdvancePastMissedStep(*Session, OutError);
    }

    UMentalResourceComponent* Mental = PlayerState->MentalResources;
    const FKillerMoveGuSlot& Slot = Session->Definition.GuSlots[SlotIndex];
    const FName ReservationKey = AttentionKey(*Session, SlotIndex);

    if (Event == EKillerMoveInputEvent::Pressed)
    {
        if (!Mental || !Mental->ReserveAttention(ReservationKey, Slot.AttentionCost, FString::Printf(TEXT("Killer move: %s"), *Slot.SlotId.ToString())))
        {
            Session->Stability = FMath::Max(0.0f, Session->Stability - 25.0f);
            const FString Message = FString::Printf(TEXT("Insufficient attention to control %s."), *Slot.SlotId.ToString());
            if (Step.bCritical) FinishSession(*Session, EKillerMoveRunState::Failed, Message);
            else PushPublicState(*Session, Message);
            OutError = Message;
            return false;
        }
        if (Step.bHoldAttention) Session->HeldSlotIndices.Add(SlotIndex);
        else Mental->ReleaseAttention(ReservationKey);
    }
    else
    {
        if (Mental) Mental->ReleaseAttention(ReservationKey);
        Session->HeldSlotIndices.Remove(SlotIndex);
    }

    const bool bStrainedTiming = FMath::Abs(Offset) > Window;
    const float Accuracy = FMath::Clamp(1.0f - FMath::Abs(Offset) / FMath::Max(SoftFailureWindow, KINDA_SMALL_NUMBER), 0.0f, 1.0f);
    Session->QualitySum += Accuracy;
    ++Session->AcceptedSteps;
    Session->Stability = FMath::Max(0.0f, Session->Stability - (1.0f - Accuracy) * 8.0f - (bStrainedTiming ? 5.0f : 0.0f));
    ++Session->StepIndex;

    if (Session->StepIndex >= Session->Definition.Choreography.Num())
    {
        CompleteSession(*Session);
        return true;
    }

    const FString TimingMessage = bStrainedTiming
        ? FString::Printf(TEXT("Input strained but accepted (%+.0f ms)."), Offset * 1000.0f)
        : FString::Printf(TEXT("Input accepted (%+.0f ms)."), Offset * 1000.0f);
    PushPublicState(*Session, TimingMessage);
    if (AGuPlayerState* PS = Session->PlayerState.Get())
    {
        FKillerMovePublicState Public = PS->KillerMovePublicState;
        Public.LastInputOffsetMs = Offset * 1000.0f;
        PS->SetKillerMovePublicState(Public);
    }
    ScheduleDeadline(*Session);
    return true;
}

void UKillerMoveSubsystem::PushPublicState(FRuntimeSession& Session, const FString& StatusText)
{
    AGuPlayerState* PS = Session.PlayerState.Get();
    if (!PS) return;

    FKillerMovePublicState Public;
    Public.State = EKillerMoveRunState::Forming;
    Public.KillerMoveId = Session.Definition.Id;
    Public.Name = Session.Definition.Name.ToString();
    Public.CurrentStep = FMath::Min(Session.StepIndex + 1, Session.Definition.Choreography.Num());
    Public.TotalSteps = Session.Definition.Choreography.Num();
    Public.Stability = Session.Stability;
    Public.ExecutionQuality = Session.AcceptedSteps > 0 ? Session.QualitySum / static_cast<float>(Session.AcceptedSteps) : 0.0f;
    Public.StatusText = StatusText;
    Public.EffectPreview = KillerMoveRuntimeEffectPreview(Session.Definition.EffectGraph);

    UGuDefinitionRegistrySubsystem* Registry = GetGameInstance() ? GetGameInstance()->GetSubsystem<UGuDefinitionRegistrySubsystem>() : nullptr;
    for (const FKillerMoveGuSlot& Slot : Session.Definition.GuSlots)
    {
        FKillerMovePublicSlot PublicSlot;
        PublicSlot.SlotId = Slot.SlotId;
        PublicSlot.Role = Slot.Role;
        FGuDefinitionRecord Definition;
        PublicSlot.GuName = Registry && Registry->GetDefinition(Slot.GuDefinitionId, Definition) ? Definition.Name : Slot.GuDefinitionId.ToString();
        Public.Slots.Add(PublicSlot);
    }

    if (Session.Definition.Choreography.IsValidIndex(Session.StepIndex))
    {
        const FKillerMoveInputStep& Step = Session.Definition.Choreography[Session.StepIndex];
        Public.ExpectedSlotIndex = Session.Definition.GuSlots.IndexOfByPredicate([&Step](const FKillerMoveGuSlot& Slot)
        {
            return Slot.SlotId == Step.SlotId;
        });
        Public.ExpectedEvent = Step.Event;
        Public.TimingWindow = EffectiveWindow(PS, Step);
        Public.ExpectedServerWorldTime = Session.StartedServerWorldTime + Step.TargetTime;
    }
    PS->SetKillerMovePublicState(Public);
}

void UKillerMoveSubsystem::ReleaseAllAttention(FRuntimeSession& Session)
{
    AGuPlayerState* PS = Session.PlayerState.Get();
    if (!PS || !PS->MentalResources) return;
    for (int32 SlotIndex = 0; SlotIndex < Session.Definition.GuSlots.Num(); ++SlotIndex)
    {
        PS->MentalResources->ReleaseAttention(AttentionKey(Session, SlotIndex));
    }
    Session.HeldSlotIndices.Reset();
}

void UKillerMoveSubsystem::FinishSession(FRuntimeSession& Session, const EKillerMoveRunState EndState, const FString& Message)
{
    UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
    if (World) World->GetTimerManager().ClearTimer(Session.DeadlineTimer);
    ReleaseAllAttention(Session);

    if (AGuPlayerState* PS = Session.PlayerState.Get())
    {
        FKillerMovePublicState Public = PS->KillerMovePublicState;
        Public.State = EndState;
        Public.CurrentStep = Public.TotalSteps;
        Public.ExpectedSlotIndex = INDEX_NONE;
        Public.ExpectedServerWorldTime = 0.0f;
        Public.TimingWindow = 0.0f;
        Public.Stability = Session.Stability;
        Public.ExecutionQuality = Session.Definition.Choreography.Num() > 0
            ? Session.QualitySum / static_cast<float>(Session.Definition.Choreography.Num())
            : 0.0f;
        Public.StatusText = Message;
        PS->SetKillerMovePublicState(Public);
    }

    const FString OwnerId = Session.OwnerId;
    Sessions.Remove(OwnerId);
}

bool UKillerMoveSubsystem::ResolveCompletedEffect(
    FRuntimeSession& Session,
    const float ExecutionQuality,
    FString& OutSummary,
    FString& OutError)
{
    OutSummary.Reset();
    OutError.Reset();

    AGuPlayerState* PlayerState = Session.PlayerState.Get();
    UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
    UGuDefinitionRegistrySubsystem* Registry = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UGuDefinitionRegistrySubsystem>()
        : nullptr;
    UGuEntitySubsystem* Entities = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UGuEntitySubsystem>()
        : nullptr;
    AController* Controller = PlayerState ? Cast<AController>(PlayerState->GetOwner()) : nullptr;
    APawn* SourcePawn = Controller ? Controller->GetPawn() : nullptr;
    UAbilitySystemComponent* SourceASC = SourcePawn
        ? UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(SourcePawn)
        : nullptr;

    if (!PlayerState || !World || !Registry || !Entities || !SourcePawn || !SourceASC)
    {
        OutError = TEXT("The killer move cannot resolve because its world/GAS context is unavailable.");
        return false;
    }

    if (Session.Definition.GuSlots.IsEmpty())
    {
        OutError = TEXT("The killer move has no Gu components.");
        return false;
    }

    if (Session.BoundGuEntities.Num() != Session.Definition.GuSlots.Num())
    {
        OutError = TEXT("The killer move lost its physical Gu bindings before manifestation.");
        return false;
    }

    // Choreography takes real time. Re-check every physical worm at the exact
    // manifestation boundary rather than assuming its begin-state stayed valid.
    for (int32 Index = 0; Index < Session.BoundGuEntities.Num(); ++Index)
    {
        const FGuid EntityId = Session.BoundGuEntities[Index];
        const FKillerMoveGuSlot& Slot = Session.Definition.GuSlots[Index];
        if (!EntityId.IsValid())
        {
            if (Slot.bRequired)
            {
                OutError = FString::Printf(TEXT("Required Gu '%s' is no longer bound."), *Slot.SlotId.ToString());
                return false;
            }
            continue;
        }

        const FOwnedByComponent* Owner = Entities->GetOwnedBy(EntityId);
        const FGuPlacementComponent* Placement = Entities->GetGuPlacement(EntityId);
        const FGuInstanceComponent* Instance = Entities->GetGuInstance(EntityId);
        FString CanUseError;
        if (!Owner || Owner->OwnerId != Session.OwnerId
            || !Placement || Placement->Container != EGuContainer::Aperture
            || !Instance || Instance->DefinitionId != Slot.GuDefinitionId
            || !Entities->CanUseGu(EntityId, CanUseError))
        {
            OutError = FString::Printf(
                TEXT("%s changed during formation and can no longer participate%s%s."),
                *Slot.SlotId.ToString(),
                CanUseError.IsEmpty() ? TEXT("") : TEXT(": "),
                CanUseError.IsEmpty() ? TEXT("") : *CanUseError);
            return false;
        }
    }

    TArray<const UGuDefinition*> Assets;
    TArray<FGuDefinitionRecord> Records;
    Assets.Reserve(Session.Definition.GuSlots.Num());
    Records.Reserve(Session.Definition.GuSlots.Num());

    for (const FKillerMoveGuSlot& Slot : Session.Definition.GuSlots)
    {
        FGuDefinitionRecord Record;
        if (!Registry->GetDefinition(Slot.GuDefinitionId, Record))
        {
            OutError = FString::Printf(TEXT("Killer-move component '%s' lost its definition."), *Slot.SlotId.ToString());
            return false;
        }
        const UGuDefinition* Asset = Registry->FindDefinitionAsset(Record.Id);
        if (!Asset)
        {
            OutError = FString::Printf(
                TEXT("%s is a runtime/generated Gu. Its domain semantics exist, but generated Gu -> GAS compilation is not ported yet."),
                *Record.Name);
            return false;
        }
        Assets.Add(Asset);
        Records.Add(MoveTemp(Record));
    }

    int32 CoreIndex = Session.Definition.GuSlots.IndexOfByPredicate([](const FKillerMoveGuSlot& Slot)
    {
        return Slot.Role == EKillerMoveRole::Core;
    });
    if (CoreIndex == INDEX_NONE) CoreIndex = 0;

    const UGuDefinition* CoreAsset = Assets[CoreIndex];
    const FGuDefinitionRecord& CoreRecord = Records[CoreIndex];

    // The browser method treats a Medium as an engineered carrier. Prefer a
    // Medium's projectile form, otherwise keep the core Gu's native carrier.
    int32 CarrierIndex = INDEX_NONE;
    for (int32 Index = 0; Index < Assets.Num(); ++Index)
    {
        if (Session.Definition.GuSlots[Index].Role == EKillerMoveRole::Medium
            && KillerMoveRuntimeFindProjectile(Assets[Index]))
        {
            CarrierIndex = Index;
            break;
        }
    }
    if (CarrierIndex == INDEX_NONE && KillerMoveRuntimeFindProjectile(CoreAsset)) CarrierIndex = CoreIndex;
    if (CarrierIndex == INDEX_NONE)
    {
        for (int32 Index = 0; Index < Assets.Num(); ++Index)
        {
            if (KillerMoveRuntimeFindProjectile(Assets[Index]))
            {
                CarrierIndex = Index;
                break;
            }
        }
    }
    const FGuProjectileMechanic* CarrierSource = CarrierIndex != INDEX_NONE
        ? KillerMoveRuntimeFindProjectile(Assets[CarrierIndex])
        : nullptr;

    if (CarrierSource && (!CarrierSource->ProjectileClass || !CarrierSource->Mesh || !CarrierSource->Material))
    {
        OutError = TEXT("The killer move selected a projectile carrier, but that carrier is incomplete.");
        return false;
    }

    const float Quality = FMath::Clamp(ExecutionQuality, 0.0f, 1.0f);
    float PowerMultiplier = FMath::Lerp(0.72f, 1.0f, Quality);
    float SpeedMultiplier = 1.0f;
    float RangeMultiplier = 1.0f;
    float RadiusMultiplier = 1.0f;
    float TotalEssenceCost = 0.0f;
    float AreaSemantic = 0.0f;
    float RangeSemantic = 0.0f;
    float PrecisionSemantic = 0.0f;

    for (int32 Index = 0; Index < Records.Num(); ++Index)
    {
        const FKillerMoveGuSlot& Slot = Session.Definition.GuSlots[Index];
        const FGuDefinitionRecord& Record = Records[Index];
        const FRefinementSemanticProfile& Profile = Record.RefinementProfile;
        const float Amplification = KillerMoveRuntimeSemanticScore(Profile, TEXT("amplification"));
        const float Speed = KillerMoveRuntimeSemanticScore(Profile, TEXT("speed"));
        const float Range = KillerMoveRuntimeSemanticScore(Profile, TEXT("range"));
        const float Area = KillerMoveRuntimeSemanticScore(Profile, TEXT("area"));
        const float Precision = KillerMoveRuntimeSemanticScore(Profile, TEXT("precision"));

        AreaSemantic = FMath::Max(AreaSemantic, Area);
        RangeSemantic = FMath::Max(RangeSemantic, Range);
        PrecisionSemantic = FMath::Max(PrecisionSemantic, Precision);

        TotalEssenceCost += KillerMoveRuntimeEssenceCost(Assets[Index])
            * KillerMoveRuntimeRoleCostWeight(Index == CoreIndex ? EKillerMoveRole::Core : Slot.Role);

        if (Index == CoreIndex) continue;
        switch (Slot.Role)
        {
        case EKillerMoveRole::Output:
            PowerMultiplier *= 1.10f + FMath::Min(0.30f, Amplification * 0.10f);
            break;
        case EKillerMoveRole::Amplification:
            PowerMultiplier *= 1.15f + FMath::Min(0.40f, FMath::Max(Amplification, Speed * 0.35f) * 0.12f);
            break;
        case EKillerMoveRole::Medium:
            SpeedMultiplier *= 1.0f + FMath::Min(0.45f, Speed * 0.18f);
            RangeMultiplier *= 1.0f + FMath::Min(0.35f, Range * 0.16f);
            RadiusMultiplier *= 1.0f + FMath::Min(0.25f, Area * 0.12f);
            break;
        case EKillerMoveRole::Routing:
            SpeedMultiplier *= 1.0f + FMath::Min(0.35f, Speed * 0.16f);
            RangeMultiplier *= 1.0f + FMath::Min(0.25f, Range * 0.12f);
            break;
        case EKillerMoveRole::Boundary:
        case EKillerMoveRole::Anchor:
            RangeMultiplier *= 1.0f + FMath::Min(0.22f, Range * 0.10f);
            RadiusMultiplier *= 1.0f + FMath::Min(0.18f, Area * 0.09f);
            break;
        default:
            break;
        }
    }

    PowerMultiplier = FMath::Clamp(PowerMultiplier, 0.25f, 3.0f);
    SpeedMultiplier = FMath::Clamp(SpeedMultiplier, 0.25f, 3.0f);
    RangeMultiplier = FMath::Clamp(RangeMultiplier, 0.25f, 3.0f);
    RadiusMultiplier = FMath::Clamp(RadiusMultiplier, 0.25f, 2.0f);

    const FGameplayAttribute EssenceAttribute = UAS_GuMasterAttributeSet::GetPrimevalEssenceAttribute();
    const float CurrentEssence = SourceASC->GetNumericAttribute(EssenceAttribute);
    if (CurrentEssence + KINDA_SMALL_NUMBER < TotalEssenceCost)
    {
        OutError = FString::Printf(
            TEXT("The killer move formed, but requires %.1f primeval essence and only %.1f remains."),
            TotalEssenceCost,
            CurrentEssence);
        return false;
    }

    UGuDefinition* Composite = NewObject<UGuDefinition>(this, NAME_None, RF_Transient);
    if (!Composite)
    {
        OutError = TEXT("Failed to create the killer move's transient Gu manifestation.");
        return false;
    }
    Composite->Name = Session.Definition.Name;
    Composite->Rank = FMath::Max(Session.Definition.Rank, CoreAsset->Rank);
    Composite->Path = CoreAsset->Path;
    Composite->StableDefinitionId = Session.Definition.Id;
    Composite->ActivationModel = EGuActivationModel::Instant;
    Composite->RefinementProfile = CoreRecord.RefinementProfile;
    Composite->Appearance = CoreAsset->Appearance;

    FGuProjectileMechanic Projectile;
    if (CarrierSource)
    {
        Projectile = *CarrierSource;
        Projectile.Speed *= SpeedMultiplier;
        Projectile.MaxRange *= RangeMultiplier;
        Projectile.Radius *= RadiusMultiplier;
        Projectile.SphereRadius *= RadiusMultiplier;
        Projectile.BoxExtent *= RadiusMultiplier;
        Projectile.CapsuleRadius *= RadiusMultiplier;
        Projectile.CapsuleHalfHeight *= RadiusMultiplier;
        KillerMoveRuntimeAddMechanic(Composite, Projectile);
    }

    int32 ImpactMechanics = 0;
    int32 ActivationMechanics = 0;
    for (const TInstancedStruct<FGuMechanic>& Mechanic : CoreAsset->Mechanics)
    {
        if (const FGuDamageMechanic* Damage = Mechanic.GetPtr<FGuDamageMechanic>())
        {
            FGuDamageMechanic CombinedDamage = *Damage;
            CombinedDamage.Damage *= PowerMultiplier;
            KillerMoveRuntimeAddMechanic(Composite, CombinedDamage);
            ++ImpactMechanics;
        }
        else if (const FGuKnockbackMechanic* Knockback = Mechanic.GetPtr<FGuKnockbackMechanic>())
        {
            FGuKnockbackMechanic CombinedKnockback = *Knockback;
            CombinedKnockback.Strength *= PowerMultiplier;
            CombinedKnockback.VerticalStrength *= PowerMultiplier;
            KillerMoveRuntimeAddMechanic(Composite, CombinedKnockback);
            ++ImpactMechanics;
        }
        else if (const FGuBuffMechanic* Buff = Mechanic.GetPtr<FGuBuffMechanic>())
        {
            FGuBuffMechanic CombinedBuff = *Buff;
            CombinedBuff.Magnitude *= PowerMultiplier;
            CombinedBuff.Duration *= FMath::Lerp(0.80f, 1.15f, Quality);
            KillerMoveRuntimeAddMechanic(Composite, CombinedBuff);
            ++ActivationMechanics;
        }
        else if (const FGuHealMechanic* Heal = Mechanic.GetPtr<FGuHealMechanic>())
        {
            FGuHealMechanic Combined = *Heal;
            Combined.Amount *= PowerMultiplier;
            KillerMoveRuntimeAddMechanic(Composite, Combined);
            if (Combined.Recipient == EGuMechanicRecipient::Self) ++ActivationMechanics; else ++ImpactMechanics;
        }
        else if (const FGuShieldMechanic* Shield = Mechanic.GetPtr<FGuShieldMechanic>())
        {
            FGuShieldMechanic Combined = *Shield;
            Combined.Amount *= PowerMultiplier;
            Combined.Duration *= FMath::Lerp(0.80f, 1.15f, Quality);
            KillerMoveRuntimeAddMechanic(Composite, Combined);
            if (Combined.Recipient == EGuMechanicRecipient::Self) ++ActivationMechanics; else ++ImpactMechanics;
        }
        else if (const FGuMovementMechanic* Movement = Mechanic.GetPtr<FGuMovementMechanic>())
        {
            FGuMovementMechanic Combined = *Movement;
            Combined.SpeedMultiplier = 1.0f + (Combined.SpeedMultiplier - 1.0f) * PowerMultiplier;
            Combined.DashSpeed *= PowerMultiplier;
            Combined.BlinkDistance *= RangeMultiplier;
            Combined.Duration *= FMath::Lerp(0.80f, 1.15f, Quality);
            KillerMoveRuntimeAddMechanic(Composite, Combined);
            if (Combined.Recipient == EGuMechanicRecipient::Self) ++ActivationMechanics; else ++ImpactMechanics;
        }
        else if (const FGuRestrictionMechanic* Restriction = Mechanic.GetPtr<FGuRestrictionMechanic>())
        {
            FGuRestrictionMechanic Combined = *Restriction;
            Combined.MovementMultiplier = FMath::Clamp(1.0f - (1.0f - Combined.MovementMultiplier) * PowerMultiplier, 0.0f, 1.0f);
            Combined.Duration *= FMath::Lerp(0.80f, 1.15f, Quality);
            KillerMoveRuntimeAddMechanic(Composite, Combined);
            ++ImpactMechanics;
        }
        else if (const FGuDamageOverTimeMechanic* DamageOverTime = Mechanic.GetPtr<FGuDamageOverTimeMechanic>())
        {
            FGuDamageOverTimeMechanic Combined = *DamageOverTime;
            Combined.DamagePerTick *= PowerMultiplier;
            Combined.Duration *= FMath::Lerp(0.80f, 1.15f, Quality);
            KillerMoveRuntimeAddMechanic(Composite, Combined);
            if (Combined.Recipient == EGuMechanicRecipient::Self) ++ActivationMechanics; else ++ImpactMechanics;
        }
        else if (const FGuHealOverTimeMechanic* HealOverTime = Mechanic.GetPtr<FGuHealOverTimeMechanic>())
        {
            FGuHealOverTimeMechanic Combined = *HealOverTime;
            Combined.HealPerTick *= PowerMultiplier;
            Combined.Duration *= FMath::Lerp(0.80f, 1.15f, Quality);
            KillerMoveRuntimeAddMechanic(Composite, Combined);
            if (Combined.Recipient == EGuMechanicRecipient::Self) ++ActivationMechanics; else ++ImpactMechanics;
        }
        else if (const FGuEssenceChangeMechanic* Essence = Mechanic.GetPtr<FGuEssenceChangeMechanic>())
        {
            FGuEssenceChangeMechanic Combined = *Essence;
            Combined.Amount *= PowerMultiplier;
            KillerMoveRuntimeAddMechanic(Composite, Combined);
            if (Combined.Recipient == EGuMechanicRecipient::Self) ++ActivationMechanics; else ++ImpactMechanics;
        }
        else if (const FGuEssenceRegenerationMechanic* Regen = Mechanic.GetPtr<FGuEssenceRegenerationMechanic>())
        {
            FGuEssenceRegenerationMechanic Combined = *Regen;
            Combined.FlatPerSecond *= PowerMultiplier;
            Combined.PercentOfMaximumPerSecond *= PowerMultiplier;
            Combined.Duration *= FMath::Lerp(0.80f, 1.15f, Quality);
            KillerMoveRuntimeAddMechanic(Composite, Combined);
            if (Combined.Recipient == EGuMechanicRecipient::Self) ++ActivationMechanics; else ++ImpactMechanics;
        }
        else if (const FGuDisplacementMechanic* Displacement = Mechanic.GetPtr<FGuDisplacementMechanic>())
        {
            FGuDisplacementMechanic Combined = *Displacement;
            Combined.Strength *= PowerMultiplier;
            Combined.VerticalStrength *= PowerMultiplier;
            KillerMoveRuntimeAddMechanic(Composite, Combined);
            ++ImpactMechanics;
        }
        else if (const FGuGuSuppressionMechanic* Suppression = Mechanic.GetPtr<FGuGuSuppressionMechanic>())
        {
            FGuGuSuppressionMechanic Combined = *Suppression;
            Combined.Duration *= FMath::Lerp(0.80f, 1.20f, Quality) * PowerMultiplier;
            KillerMoveRuntimeAddMechanic(Composite, Combined);
            ++ImpactMechanics;
        }
        else if (const FGuCleanseMechanic* Cleanse = Mechanic.GetPtr<FGuCleanseMechanic>())
        {
            KillerMoveRuntimeAddMechanic(Composite, *Cleanse);
            if (Cleanse->Recipient == EGuMechanicRecipient::Self) ++ActivationMechanics; else ++ImpactMechanics;
        }
        else if (const FGuDispelMechanic* Dispel = Mechanic.GetPtr<FGuDispelMechanic>())
        {
            KillerMoveRuntimeAddMechanic(Composite, *Dispel);
            if (Dispel->Recipient == EGuMechanicRecipient::Self) ++ActivationMechanics; else ++ImpactMechanics;
        }
        else if (const FGuFieldMechanic* Field = Mechanic.GetPtr<FGuFieldMechanic>())
        {
            FGuFieldMechanic Combined = *Field;
            Combined.Radius *= RadiusMultiplier;
            Combined.ForwardOffset *= RangeMultiplier;
            Combined.Duration *= FMath::Lerp(0.80f, 1.15f, Quality);
            KillerMoveRuntimeAddMechanic(Composite, Combined);
            ++ActivationMechanics;
        }
        else if (const FGuChainMechanic* Chain = Mechanic.GetPtr<FGuChainMechanic>())
        {
            FGuChainMechanic Combined = *Chain;
            Combined.JumpRadius *= RangeMultiplier;
            Combined.MaxAdditionalTargets = FMath::Max(1, FMath::RoundToInt(static_cast<float>(Combined.MaxAdditionalTargets) * FMath::Lerp(0.8f, 1.2f, Quality)));
            KillerMoveRuntimeAddMechanic(Composite, Combined);
        }
        else if (const FGuMarkMechanic* Mark = Mechanic.GetPtr<FGuMarkMechanic>())
        {
            FGuMarkMechanic Combined = *Mark;
            Combined.Strength *= PowerMultiplier;
            Combined.Duration *= FMath::Lerp(0.80f, 1.15f, Quality);
            KillerMoveRuntimeAddMechanic(Composite, Combined);
            ++ImpactMechanics;
        }
        else if (const FGuAttentionBoostMechanic* Attention = Mechanic.GetPtr<FGuAttentionBoostMechanic>())
        {
            FGuAttentionBoostMechanic Combined = *Attention;
            Combined.Duration *= FMath::Lerp(0.80f, 1.15f, Quality);
            KillerMoveRuntimeAddMechanic(Composite, Combined);
            ++ActivationMechanics;
        }
        else if (const FGuSummonMechanic* Summon = Mechanic.GetPtr<FGuSummonMechanic>())
        {
            FGuSummonMechanic Combined = *Summon;
            Combined.Count = FMath::Max(1, FMath::RoundToInt(static_cast<float>(Combined.Count) * FMath::Lerp(0.8f, 1.2f, Quality)));
            Combined.SpawnRadius *= RadiusMultiplier;
            Combined.ForwardOffset *= RangeMultiplier;
            KillerMoveRuntimeAddMechanic(Composite, Combined);
            ++ActivationMechanics;
        }
        else if (const FGuConcealmentMechanic* Concealment = Mechanic.GetPtr<FGuConcealmentMechanic>())
        {
            FGuConcealmentMechanic Combined = *Concealment;
            Combined.DetectionResistance = FMath::Clamp(Combined.DetectionResistance * PowerMultiplier, 0.0f, 0.95f);
            Combined.Duration *= FMath::Lerp(0.80f, 1.15f, Quality);
            KillerMoveRuntimeAddMechanic(Composite, Combined);
            ++ActivationMechanics;
        }
        else if (const FGuRevealMechanic* Reveal = Mechanic.GetPtr<FGuRevealMechanic>())
        {
            FGuRevealMechanic Combined = *Reveal;
            Combined.Strength *= PowerMultiplier;
            Combined.Range *= RangeMultiplier;
            Combined.Duration *= FMath::Lerp(0.80f, 1.15f, Quality);
            KillerMoveRuntimeAddMechanic(Composite, Combined);
            ++ActivationMechanics;
        }
    }

    // Supporting Gu contribute only the mechanics appropriate to their graph role.
    // This keeps a killer move compositional instead of simply firing every Gu
    // independently at full strength.
    for (int32 Index = 0; Index < Assets.Num(); ++Index)
    {
        if (Index == CoreIndex) continue;
        const EKillerMoveRole Role = Session.Definition.GuSlots[Index].Role;

        float ImpactWeight = 0.0f;
        float BuffWeight = 0.0f;
        float RecoveryWeight = 0.0f;
        float ShieldWeight = 0.0f;
        float MovementWeight = 0.0f;
        float ConcealmentWeight = 0.0f;
        float RevealWeight = 0.0f;
        float RestrictionWeight = 0.0f;
        float PeriodicWeight = 0.0f;
        float ControlWeight = 0.0f;
        float ResourceWeight = 0.0f;
        float CleanseWeight = 0.0f;
        float FieldWeight = 0.0f;
        float ChainWeight = 0.0f;
        float MarkWeight = 0.0f;
        float AttentionWeight = 0.0f;
        float SummonWeight = 0.0f;
        switch (Role)
        {
        case EKillerMoveRole::Output:
            ImpactWeight = 0.65f;
            PeriodicWeight = 0.65f;
            ControlWeight = 0.35f;
            BuffWeight = 0.45f;
            break;
        case EKillerMoveRole::Suppression:
            ImpactWeight = 0.55f;
            PeriodicWeight = 0.45f;
            RestrictionWeight = 0.80f;
            ControlWeight = 0.85f;
            break;
        case EKillerMoveRole::Amplification:
            ImpactWeight = 0.30f;
            PeriodicWeight = 0.30f;
            BuffWeight = 0.55f;
            ChainWeight = 0.40f;
            break;
        case EKillerMoveRole::Stabilization:
        case EKillerMoveRole::Safety:
        case EKillerMoveRole::Buffer:
            BuffWeight = 0.75f;
            ShieldWeight = 0.80f;
            CleanseWeight = 0.60f;
            AttentionWeight = 0.35f;
            break;
        case EKillerMoveRole::Recovery:
            BuffWeight = 0.55f;
            RecoveryWeight = 0.85f;
            PeriodicWeight = 0.85f;
            CleanseWeight = 0.75f;
            ResourceWeight = 0.50f;
            break;
        case EKillerMoveRole::Medium:
        case EKillerMoveRole::Routing:
            MovementWeight = 0.70f;
            ChainWeight = 0.45f;
            break;
        case EKillerMoveRole::Concealment:
            ConcealmentWeight = 0.85f;
            break;
        case EKillerMoveRole::Targeting:
        case EKillerMoveRole::InvestigationSensor:
        case EKillerMoveRole::RecognitionValidation:
            RevealWeight = 0.75f;
            MarkWeight = 0.80f;
            ChainWeight = 0.55f;
            break;
        case EKillerMoveRole::Control:
            ImpactWeight = 0.25f;
            BuffWeight = 0.35f;
            RestrictionWeight = 0.50f;
            ControlWeight = 0.65f;
            MarkWeight = 0.40f;
            break;
        case EKillerMoveRole::Fuel:
            ResourceWeight = 0.90f;
            break;
        case EKillerMoveRole::Link:
            MarkWeight = 0.85f;
            ChainWeight = 0.80f;
            break;
        case EKillerMoveRole::Boundary:
        case EKillerMoveRole::Anchor:
        case EKillerMoveRole::Storage:
            FieldWeight = 0.80f;
            break;
        case EKillerMoveRole::Subordinate:
            SummonWeight = 0.90f;
            break;
        default:
            break;
        }

        for (const TInstancedStruct<FGuMechanic>& Mechanic : Assets[Index]->Mechanics)
        {
            if (ImpactWeight > 0.0f)
            {
                if (const FGuDamageMechanic* Damage = Mechanic.GetPtr<FGuDamageMechanic>())
                {
                    FGuDamageMechanic AddedDamage = *Damage;
                    AddedDamage.Damage *= ImpactWeight * FMath::Lerp(0.75f, 1.0f, Quality);
                    KillerMoveRuntimeAddMechanic(Composite, AddedDamage);
                    ++ImpactMechanics;
                    continue;
                }
                if (const FGuKnockbackMechanic* Knockback = Mechanic.GetPtr<FGuKnockbackMechanic>())
                {
                    FGuKnockbackMechanic AddedKnockback = *Knockback;
                    AddedKnockback.Strength *= ImpactWeight * FMath::Lerp(0.80f, 1.0f, Quality);
                    AddedKnockback.VerticalStrength *= ImpactWeight * FMath::Lerp(0.80f, 1.0f, Quality);
                    KillerMoveRuntimeAddMechanic(Composite, AddedKnockback);
                    ++ImpactMechanics;
                    continue;
                }
            }

            if (BuffWeight > 0.0f)
            {
                if (const FGuBuffMechanic* Buff = Mechanic.GetPtr<FGuBuffMechanic>())
                {
                    FGuBuffMechanic AddedBuff = *Buff;
                    AddedBuff.Magnitude *= BuffWeight * FMath::Lerp(0.80f, 1.0f, Quality);
                    AddedBuff.Duration *= FMath::Lerp(0.80f, 1.10f, Quality);
                    KillerMoveRuntimeAddMechanic(Composite, AddedBuff);
                    ++ActivationMechanics;
                    continue;
                }
            }

            if (RecoveryWeight > 0.0f)
            {
                if (const FGuHealMechanic* Heal = Mechanic.GetPtr<FGuHealMechanic>())
                {
                    FGuHealMechanic Added = *Heal;
                    Added.Amount *= RecoveryWeight * FMath::Lerp(0.80f, 1.0f, Quality);
                    KillerMoveRuntimeAddMechanic(Composite, Added);
                    if (Added.Recipient == EGuMechanicRecipient::Self) ++ActivationMechanics; else ++ImpactMechanics;
                    continue;
                }
            }
            if (ShieldWeight > 0.0f)
            {
                if (const FGuShieldMechanic* Shield = Mechanic.GetPtr<FGuShieldMechanic>())
                {
                    FGuShieldMechanic Added = *Shield;
                    Added.Amount *= ShieldWeight * FMath::Lerp(0.80f, 1.0f, Quality);
                    Added.Duration *= FMath::Lerp(0.80f, 1.10f, Quality);
                    KillerMoveRuntimeAddMechanic(Composite, Added);
                    if (Added.Recipient == EGuMechanicRecipient::Self) ++ActivationMechanics; else ++ImpactMechanics;
                    continue;
                }
            }
            if (MovementWeight > 0.0f)
            {
                if (const FGuMovementMechanic* Movement = Mechanic.GetPtr<FGuMovementMechanic>())
                {
                    FGuMovementMechanic Added = *Movement;
                    Added.SpeedMultiplier = 1.0f + (Added.SpeedMultiplier - 1.0f) * MovementWeight;
                    Added.DashSpeed *= MovementWeight;
                    Added.BlinkDistance *= MovementWeight;
                    KillerMoveRuntimeAddMechanic(Composite, Added);
                    if (Added.Recipient == EGuMechanicRecipient::Self) ++ActivationMechanics; else ++ImpactMechanics;
                    continue;
                }
            }
            if (ConcealmentWeight > 0.0f)
            {
                if (const FGuConcealmentMechanic* Concealment = Mechanic.GetPtr<FGuConcealmentMechanic>())
                {
                    FGuConcealmentMechanic Added = *Concealment;
                    Added.DetectionResistance = FMath::Clamp(Added.DetectionResistance * ConcealmentWeight, 0.0f, 0.95f);
                    KillerMoveRuntimeAddMechanic(Composite, Added);
                    ++ActivationMechanics;
                    continue;
                }
            }
            if (RevealWeight > 0.0f)
            {
                if (const FGuRevealMechanic* Reveal = Mechanic.GetPtr<FGuRevealMechanic>())
                {
                    FGuRevealMechanic Added = *Reveal;
                    Added.Strength *= RevealWeight;
                    Added.Range *= FMath::Max(0.25f, RevealWeight);
                    KillerMoveRuntimeAddMechanic(Composite, Added);
                    ++ActivationMechanics;
                    continue;
                }
            }
            if (PeriodicWeight > 0.0f)
            {
                if (const FGuDamageOverTimeMechanic* Periodic = Mechanic.GetPtr<FGuDamageOverTimeMechanic>())
                {
                    FGuDamageOverTimeMechanic Added = *Periodic;
                    Added.DamagePerTick *= PeriodicWeight * FMath::Lerp(0.80f, 1.0f, Quality);
                    Added.Duration *= FMath::Lerp(0.80f, 1.10f, Quality);
                    KillerMoveRuntimeAddMechanic(Composite, Added);
                    if (Added.Recipient == EGuMechanicRecipient::Self) ++ActivationMechanics; else ++ImpactMechanics;
                    continue;
                }
                if (const FGuHealOverTimeMechanic* Periodic = Mechanic.GetPtr<FGuHealOverTimeMechanic>())
                {
                    FGuHealOverTimeMechanic Added = *Periodic;
                    Added.HealPerTick *= PeriodicWeight * FMath::Lerp(0.80f, 1.0f, Quality);
                    Added.Duration *= FMath::Lerp(0.80f, 1.10f, Quality);
                    KillerMoveRuntimeAddMechanic(Composite, Added);
                    if (Added.Recipient == EGuMechanicRecipient::Self) ++ActivationMechanics; else ++ImpactMechanics;
                    continue;
                }
            }
            if (ControlWeight > 0.0f)
            {
                if (const FGuDisplacementMechanic* Displacement = Mechanic.GetPtr<FGuDisplacementMechanic>())
                {
                    FGuDisplacementMechanic Added = *Displacement;
                    Added.Strength *= ControlWeight * FMath::Lerp(0.80f, 1.0f, Quality);
                    Added.VerticalStrength *= ControlWeight * FMath::Lerp(0.80f, 1.0f, Quality);
                    KillerMoveRuntimeAddMechanic(Composite, Added);
                    ++ImpactMechanics;
                    continue;
                }
                if (const FGuGuSuppressionMechanic* Suppression = Mechanic.GetPtr<FGuGuSuppressionMechanic>())
                {
                    FGuGuSuppressionMechanic Added = *Suppression;
                    Added.Duration *= ControlWeight * FMath::Lerp(0.85f, 1.10f, Quality);
                    KillerMoveRuntimeAddMechanic(Composite, Added);
                    ++ImpactMechanics;
                    continue;
                }
                if (const FGuDispelMechanic* Dispel = Mechanic.GetPtr<FGuDispelMechanic>())
                {
                    KillerMoveRuntimeAddMechanic(Composite, *Dispel);
                    if (Dispel->Recipient == EGuMechanicRecipient::Self) ++ActivationMechanics; else ++ImpactMechanics;
                    continue;
                }
            }
            if (ResourceWeight > 0.0f)
            {
                if (const FGuEssenceChangeMechanic* Essence = Mechanic.GetPtr<FGuEssenceChangeMechanic>())
                {
                    FGuEssenceChangeMechanic Added = *Essence;
                    Added.Amount *= ResourceWeight * FMath::Lerp(0.80f, 1.0f, Quality);
                    KillerMoveRuntimeAddMechanic(Composite, Added);
                    if (Added.Recipient == EGuMechanicRecipient::Self) ++ActivationMechanics; else ++ImpactMechanics;
                    continue;
                }
                if (const FGuEssenceRegenerationMechanic* Regen = Mechanic.GetPtr<FGuEssenceRegenerationMechanic>())
                {
                    FGuEssenceRegenerationMechanic Added = *Regen;
                    Added.FlatPerSecond *= ResourceWeight;
                    Added.PercentOfMaximumPerSecond *= ResourceWeight;
                    KillerMoveRuntimeAddMechanic(Composite, Added);
                    if (Added.Recipient == EGuMechanicRecipient::Self) ++ActivationMechanics; else ++ImpactMechanics;
                    continue;
                }
            }
            if (CleanseWeight > 0.0f)
            {
                if (const FGuCleanseMechanic* Cleanse = Mechanic.GetPtr<FGuCleanseMechanic>())
                {
                    KillerMoveRuntimeAddMechanic(Composite, *Cleanse);
                    if (Cleanse->Recipient == EGuMechanicRecipient::Self) ++ActivationMechanics; else ++ImpactMechanics;
                    continue;
                }
            }
            if (FieldWeight > 0.0f)
            {
                if (const FGuFieldMechanic* Field = Mechanic.GetPtr<FGuFieldMechanic>())
                {
                    FGuFieldMechanic Added = *Field;
                    Added.Radius *= FMath::Max(0.25f, FieldWeight) * RadiusMultiplier;
                    Added.ForwardOffset *= RangeMultiplier;
                    Added.Duration *= FMath::Lerp(0.80f, 1.10f, Quality);
                    KillerMoveRuntimeAddMechanic(Composite, Added);
                    ++ActivationMechanics;
                    continue;
                }
            }
            if (ChainWeight > 0.0f)
            {
                if (const FGuChainMechanic* Chain = Mechanic.GetPtr<FGuChainMechanic>())
                {
                    FGuChainMechanic Added = *Chain;
                    Added.JumpRadius *= FMath::Max(0.25f, ChainWeight) * RangeMultiplier;
                    Added.MaxAdditionalTargets = FMath::Max(1, FMath::RoundToInt(static_cast<float>(Added.MaxAdditionalTargets) * FMath::Max(0.5f, ChainWeight)));
                    KillerMoveRuntimeAddMechanic(Composite, Added);
                    continue;
                }
            }
            if (MarkWeight > 0.0f)
            {
                if (const FGuMarkMechanic* Mark = Mechanic.GetPtr<FGuMarkMechanic>())
                {
                    FGuMarkMechanic Added = *Mark;
                    Added.Strength *= MarkWeight * FMath::Lerp(0.80f, 1.0f, Quality);
                    KillerMoveRuntimeAddMechanic(Composite, Added);
                    ++ImpactMechanics;
                    continue;
                }
            }
            if (AttentionWeight > 0.0f)
            {
                if (const FGuAttentionBoostMechanic* Attention = Mechanic.GetPtr<FGuAttentionBoostMechanic>())
                {
                    FGuAttentionBoostMechanic Added = *Attention;
                    Added.SlotsGranted = FMath::Max(1, FMath::RoundToInt(static_cast<float>(Added.SlotsGranted) * FMath::Max(0.5f, AttentionWeight)));
                    KillerMoveRuntimeAddMechanic(Composite, Added);
                    ++ActivationMechanics;
                    continue;
                }
            }
            if (SummonWeight > 0.0f)
            {
                if (const FGuSummonMechanic* Summon = Mechanic.GetPtr<FGuSummonMechanic>())
                {
                    FGuSummonMechanic Added = *Summon;
                    Added.Count = FMath::Max(1, FMath::RoundToInt(static_cast<float>(Added.Count) * FMath::Max(0.5f, SummonWeight)));
                    KillerMoveRuntimeAddMechanic(Composite, Added);
                    ++ActivationMechanics;
                    continue;
                }
            }
            if (RestrictionWeight > 0.0f)
            {
                if (const FGuRestrictionMechanic* Restriction = Mechanic.GetPtr<FGuRestrictionMechanic>())
                {
                    FGuRestrictionMechanic Added = *Restriction;
                    Added.MovementMultiplier = FMath::Lerp(1.0f, Added.MovementMultiplier, RestrictionWeight);
                    KillerMoveRuntimeAddMechanic(Composite, Added);
                    ++ImpactMechanics;
                    continue;
                }
            }
        }
    }

    if (ImpactMechanics <= 0 && ActivationMechanics <= 0)
    {
        OutError = TEXT("The killer move formed, but none of its Gu contribute a currently executable mechanic.");
        return false;
    }

    const UGuSystemConfig* SystemConfig = KillerMoveRuntimeFindSystemConfig(SourceASC);

    // Commit the combined cost only once the manifestation is actually ready.
    // Individual press-time costs can be layered on later without changing the
    // compiled effect graph.
    SourceASC->SetNumericAttributeBase(
        EssenceAttribute,
        FMath::Max(0.0f, CurrentEssence - TotalEssenceCost));

    bool bActivationExecuted = false;
    if (ActivationMechanics > 0)
    {
        bActivationExecuted = UGuExecutionLibrary::ExecuteActivation(
            Composite,
            SourceASC,
            SourcePawn,
            SystemConfig);

        if (!bActivationExecuted && !SystemConfig && ImpactMechanics <= 0)
        {
            SourceASC->SetNumericAttributeBase(EssenceAttribute, CurrentEssence);
            OutError = TEXT(
                "This killer move is a self/stat effect, but no GuSystemConfig could be found "
                "on an active Gu ability to resolve its buff GameplayEffects.");
            return false;
        }
    }

    FString ManifestationLabel;
    int32 AffectedTargets = 0;
    const bool bFieldCarrier = KillerMoveRuntimeHasFieldCarrier(Composite);

    if (CarrierSource)
    {
        const FVector SpawnLocation =
            SourcePawn->GetActorLocation()
            + SourcePawn->GetActorForwardVector() * 100.0f;
        const FTransform SpawnTransform(
            SourcePawn->GetActorRotation(),
            SpawnLocation);

        AGu_Projectile* SpawnedProjectile =
            World->SpawnActorDeferred<AGu_Projectile>(
                Projectile.ProjectileClass,
                SpawnTransform,
                SourcePawn,
                SourcePawn,
                ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

        if (!SpawnedProjectile)
        {
            SourceASC->SetNumericAttributeBase(EssenceAttribute, CurrentEssence);
            OutError = TEXT("The killer move formed but its projectile manifestation could not spawn.");
            return false;
        }

        SpawnedProjectile->FinishSpawning(SpawnTransform);
        SpawnedProjectile->InitializeProjectile(
            Projectile,
            Composite,
            SourceASC);

        ManifestationLabel = bFieldCarrier
            ? FString::Printf(TEXT("projectile + persistent field (speed %.0f, range %.0f)"), Projectile.Speed, Projectile.MaxRange)
            : FString::Printf(TEXT("projectile (speed %.0f, range %.0f)"), Projectile.Speed, Projectile.MaxRange);
    }
    else if (bFieldCarrier && bActivationExecuted)
    {
        ManifestationLabel = TEXT("persistent field");
    }
    else if (ImpactMechanics > 0)
    {
        const bool bAreaMove = AreaSemantic >= 0.55f
            || Records.ContainsByPredicate([](const FGuDefinitionRecord& Record)
            {
                return Record.KillerMove.Template == TEXT("area")
                    || Record.KillerMove.Template == TEXT("field");
            });

        if (bAreaMove)
        {
            const float ForwardDistance = FMath::Clamp(
                (170.0f + RangeSemantic * 260.0f) * RangeMultiplier,
                150.0f,
                900.0f);
            const float AreaRadius = FMath::Clamp(
                (130.0f + AreaSemantic * 220.0f) * RadiusMultiplier,
                120.0f,
                650.0f);
            const FVector Center =
                SourcePawn->GetActorLocation()
                + SourcePawn->GetActorForwardVector() * ForwardDistance;

            AffectedTargets = KillerMoveRuntimeExecuteOverlapImpact(
                World,
                Composite,
                SourceASC,
                SourcePawn,
                Center,
                AreaRadius);

#if !UE_BUILD_SHIPPING
            DrawDebugSphere(
                World,
                Center,
                AreaRadius,
                24,
                FColor::White,
                false,
                0.75f,
                0,
                2.0f);
#endif

            ManifestationLabel = FString::Printf(
                TEXT("area burst (radius %.0f, hit %d target%s)"),
                AreaRadius,
                AffectedTargets,
                AffectedTargets == 1 ? TEXT("") : TEXT("s"));
        }
        else
        {
            const float SweepRange = FMath::Clamp(
                (150.0f + RangeSemantic * 300.0f + PrecisionSemantic * 40.0f) * RangeMultiplier,
                140.0f,
                700.0f);
            const float SweepRadius = FMath::Clamp(
                (55.0f + AreaSemantic * 100.0f) * RadiusMultiplier,
                45.0f,
                180.0f);
            const FVector Start = SourcePawn->GetActorLocation();
            const FVector End =
                Start
                + SourcePawn->GetActorForwardVector() * SweepRange;

            AffectedTargets = KillerMoveRuntimeExecuteSweepImpact(
                World,
                Composite,
                SourceASC,
                SourcePawn,
                Start,
                End,
                SweepRadius);

#if !UE_BUILD_SHIPPING
            DrawDebugLine(
                World,
                Start,
                End,
                FColor::White,
                false,
                0.60f,
                0,
                3.0f);
            DrawDebugSphere(
                World,
                End,
                SweepRadius,
                16,
                FColor::White,
                false,
                0.60f,
                0,
                2.0f);
#endif

            ManifestationLabel = FString::Printf(
                TEXT("melee/sweep (range %.0f, hit %d target%s)"),
                SweepRange,
                AffectedTargets,
                AffectedTargets == 1 ? TEXT("") : TEXT("s"));
        }
    }
    else if (bActivationExecuted)
    {
#if !UE_BUILD_SHIPPING
        DrawDebugSphere(
            World,
            SourcePawn->GetActorLocation(),
            90.0f,
            16,
            FColor::White,
            false,
            0.60f,
            0,
            2.0f);
#endif
        ManifestationLabel = TEXT("self effect");
    }
    else
    {
        SourceASC->SetNumericAttributeBase(EssenceAttribute, CurrentEssence);
        OutError = TEXT("The killer move compiled mechanics but none could manifest in the world.");
        return false;
    }

    for (const FGuid& EntityId : Session.BoundGuEntities)
    {
        if (!EntityId.IsValid()) continue;
        FString LifecycleError;
        if (!Entities->NotifySuccessfulGuActivation(EntityId, LifecycleError)
            && !LifecycleError.IsEmpty())
        {
            UE_LOG(
                LogTemp,
                Warning,
                TEXT("Killer-move Gu lifecycle settlement: %s"),
                *LifecycleError);
        }
    }

    OutSummary = FString::Printf(
        TEXT("Released %s: power x%.2f, speed x%.2f, range x%.2f, cost %.1f essence%s."),
        *ManifestationLabel,
        PowerMultiplier,
        SpeedMultiplier,
        RangeMultiplier,
        TotalEssenceCost,
        bActivationExecuted ? TEXT(", activation effects applied") : TEXT(""));
    return true;
}

void UKillerMoveSubsystem::CompleteSession(FRuntimeSession& Session)
{
    const float Quality = Session.Definition.Choreography.Num() > 0
        ? Session.QualitySum / static_cast<float>(Session.Definition.Choreography.Num())
        : 0.0f;

    FString EffectSummary;
    FString EffectError;
    if (!ResolveCompletedEffect(Session, Quality, EffectSummary, EffectError))
    {
        const FString Message = FString::Printf(
            TEXT("Killer move formed at %.0f%% execution quality, but failed to manifest: %s"),
            Quality * 100.0f,
            *EffectError);
        FinishSession(Session, EKillerMoveRunState::Failed, Message);
        return;
    }

    const FString Message = FString::Printf(
        TEXT("Killer move formed and released at %.0f%% execution quality. %s"),
        Quality * 100.0f,
        *EffectSummary);
    FinishSession(Session, EKillerMoveRunState::Completed, Message);
}

bool UKillerMoveSubsystem::CancelKillerMove(AGuPlayerState* PlayerState, FString& OutError)
{
    OutError.Reset();
    if (!PlayerState || !PlayerState->HasAuthority())
    {
        OutError = TEXT("Killer-move cancellation must run on the authority.");
        return false;
    }
    FRuntimeSession* Session = Sessions.Find(PlayerState->DomainCharacterId);
    if (!Session)
    {
        OutError = TEXT("No killer move is currently forming.");
        return false;
    }
    FinishSession(*Session, EKillerMoveRunState::Cancelled, TEXT("Killer move cancelled; attention released."));
    return true;
}

bool UKillerMoveSubsystem::BeginDebugKillerMove(AGuPlayerState* PlayerState, FString& OutError)
{
#if UE_BUILD_SHIPPING
    OutError = TEXT("Debug killer moves are unavailable in Shipping builds.");
    return false;
#else
    if (!PlayerState)
    {
        OutError = TEXT("No GuPlayerState.");
        return false;
    }
    UGuEntitySubsystem* Entities = GetGameInstance() ? GetGameInstance()->GetSubsystem<UGuEntitySubsystem>() : nullptr;
    UGuDefinitionRegistrySubsystem* Registry = GetGameInstance() ? GetGameInstance()->GetSubsystem<UGuDefinitionRegistrySubsystem>() : nullptr;
    if (!Entities || !Registry)
    {
        OutError = TEXT("Killer-move domain is unavailable.");
        return false;
    }

    TArray<FGuid> Owned = Entities->QueryGuEntitiesForOwner(PlayerState->DomainCharacterId, EGuContainer::Aperture, true);
    if (Owned.Num() < 1)
    {
        OutError = TEXT("No living Gu are present in the aperture.");
        return false;
    }
    Owned.SetNum(FMath::Min(2, Owned.Num()));

    FKillerMoveDefinitionRecord Definition;
    Definition.Id = TEXT("debug_timed_killer_move");
    Definition.Name = FText::FromString(TEXT("Debug Timed Killer Move"));
    Definition.Rank = 1;

    for (int32 Index = 0; Index < Owned.Num(); ++Index)
    {
        const FGuInstanceComponent* Instance = Entities->GetGuInstance(Owned[Index]);
        if (!Instance) continue;
        FKillerMoveGuSlot Slot;
        Slot.SlotId = Index == 0 ? TEXT("Core") : TEXT("Support");
        Slot.GuDefinitionId = Instance->DefinitionId;
        Slot.PreferredEntityId = Owned[Index];
        Slot.Role = EKillerMoveRole::Core;
        if (Index > 0)
        {
            FGuDefinitionRecord SupportDefinition;
            Slot.Role = Registry->GetDefinition(Instance->DefinitionId, SupportDefinition)
                ? KillerMoveRuntimeInferSupportRole(SupportDefinition)
                : EKillerMoveRole::Amplification;
        }
        Slot.AttentionCost = 1.0f;
        Definition.GuSlots.Add(Slot);
    }

    if (Definition.GuSlots.Num() < 1)
    {
        OutError = TEXT("Owned Gu could not be resolved.");
        return false;
    }

    const bool bCanOverlap = Definition.GuSlots.Num() >= 2
        && PlayerState->MentalResources
        && PlayerState->MentalResources->GetAttentionCapacity() >= 2;

    auto AddStep = [&Definition](const FName SlotId, const EKillerMoveInputEvent Event, const float TargetTime,
        const float Window, const bool bCritical, const bool bHoldAttention)
    {
        FKillerMoveInputStep Step;
        Step.SlotId = SlotId;
        Step.Event = Event;
        Step.TargetTime = TargetTime;
        Step.TimingWindow = Window;
        Step.bCritical = bCritical;
        Step.bHoldAttention = bHoldAttention;
        Definition.Choreography.Add(Step);
    };

    if (bCanOverlap)
    {
        AddStep(TEXT("Core"), EKillerMoveInputEvent::Pressed, 0.80f, 0.42f, true, true);
        AddStep(TEXT("Support"), EKillerMoveInputEvent::Pressed, 1.25f, 0.40f, false, true);
        AddStep(TEXT("Support"), EKillerMoveInputEvent::Released, 1.55f, 0.40f, false, false);
        AddStep(TEXT("Core"), EKillerMoveInputEvent::Released, 1.95f, 0.42f, true, false);
    }
    else if (Definition.GuSlots.Num() >= 2)
    {
        AddStep(TEXT("Core"), EKillerMoveInputEvent::Pressed, 0.70f, 0.44f, true, true);
        AddStep(TEXT("Core"), EKillerMoveInputEvent::Released, 1.00f, 0.42f, true, false);
        AddStep(TEXT("Support"), EKillerMoveInputEvent::Pressed, 1.35f, 0.42f, false, true);
        AddStep(TEXT("Support"), EKillerMoveInputEvent::Released, 1.65f, 0.42f, false, false);
    }
    else
    {
        AddStep(TEXT("Core"), EKillerMoveInputEvent::Pressed, 0.75f, 0.44f, true, true);
        AddStep(TEXT("Core"), EKillerMoveInputEvent::Released, 1.20f, 0.42f, true, false);
        AddStep(TEXT("Core"), EKillerMoveInputEvent::Pressed, 1.60f, 0.40f, false, true);
        AddStep(TEXT("Core"), EKillerMoveInputEvent::Released, 1.82f, 0.40f, false, false);
    }
    return BeginKillerMove(PlayerState, Definition, OutError);
#endif
}
