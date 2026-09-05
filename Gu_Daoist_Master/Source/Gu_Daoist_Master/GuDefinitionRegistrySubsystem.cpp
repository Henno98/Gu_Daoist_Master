#include "GuDefinitionRegistrySubsystem.h"

#include "UGuDefinition.h"
#include "GuRulesLibrary.h"

namespace
{
    void AppendCanonicalNameArray(FString& Out, const TCHAR* Label, TArray<FName> Values)
    {
        Values.Sort([](const FName A, const FName B) { return A.ToString() < B.ToString(); });
        Out += Label;
        Out += TEXT("=");
        for (const FName Value : Values)
        {
            Out += Value.ToString().ToLower();
            Out += TEXT(",");
        }
        Out += TEXT("|");
    }

    void AppendCanonicalScoreMap(FString& Out, const TCHAR* Label, const TMap<FName, float>& Scores)
    {
        TArray<FString> Parts;
        Parts.Reserve(Scores.Num());
        for (const TPair<FName, float>& Pair : Scores)
        {
            Parts.Add(FString::Printf(TEXT("%s=%.6f"), *Pair.Key.ToString().ToLower(), Pair.Value));
        }
        Parts.Sort();
        Out += Label;
        Out += TEXT("=");
        Out += FString::Join(Parts, TEXT(","));
        Out += TEXT("|");
    }

    uint64 HashCanonicalSpeciesString(const FString& Text)
    {
        uint64 Hash = 1469598103934665603ull;
        FTCHARToUTF8 Utf8(*Text);
        for (int32 Index = 0; Index < Utf8.Length(); ++Index)
        {
            Hash ^= static_cast<uint8>(Utf8.Get()[Index]);
            Hash *= 1099511628211ull;
        }
        return Hash;
    }

    FName GameplayTagLeaf(const FGameplayTag& Tag)
    {
        if (!Tag.IsValid()) return NAME_None;
        FString Raw = Tag.ToString();
        FString Left;
        FString Right;
        while (Raw.Split(TEXT("."), &Left, &Right, ESearchCase::CaseSensitive, ESearchDir::FromStart))
        {
            Raw = Right;
        }
        Raw.TrimStartAndEndInline();
        return Raw.IsEmpty() ? NAME_None : FName(*Raw);
    }

    FString EscapeJsonForMechanicRecord(const FString& Input)
    {
        FString Value = Input;
        Value.ReplaceInline(TEXT("\\"), TEXT("\\\\"));
        Value.ReplaceInline(TEXT("\""), TEXT("\\\""));
        Value.ReplaceInline(TEXT("\n"), TEXT("\\n"));
        return Value;
    }

    void AddMechanic(FGuDefinitionRecord& Record, const TCHAR* Type, const FString& Config = TEXT("{}"))
    {
        FGuMechanicSpec Spec;
        Spec.Type = FName(Type);
        Spec.ConfigJson = Config;
        Record.Mechanics.Add(MoveTemp(Spec));
    }

    FString NamesToJsonArray(const TArray<FName>& Names)
    {
        FString Result = TEXT("[");
        bool bFirst = true;
        for (const FName Name : Names)
        {
            if (Name.IsNone()) continue;
            if (!bFirst) Result += TEXT(",");
            bFirst = false;
            Result += FString::Printf(TEXT("\"%s\""), *EscapeJsonForMechanicRecord(Name.ToString()));
        }
        Result += TEXT("]");
        return Result;
    }
}

FString UGuDefinitionRegistrySubsystem::NameKey(const FString& Name)
{
    return Name.TrimStartAndEnd().ToLower();
}

FName UGuDefinitionRegistrySubsystem::ResolveDefinitionAlias(const FName Id) const
{
    FName Current = Id;
    for (int32 Depth = 0; Depth < 8; ++Depth)
    {
        const FName* Next = DefinitionAliases.Find(Current);
        if (!Next || Next->IsNone() || *Next == Current) break;
        Current = *Next;
    }
    return Current;
}

void UGuDefinitionRegistrySubsystem::RetargetDefinitionIdentity(const FName OldId, const FName NewId)
{
    if (OldId.IsNone() || NewId.IsNone() || OldId == NewId) return;

    for (TPair<FName, FName>& Alias : DefinitionAliases)
    {
        if (Alias.Value == OldId) Alias.Value = NewId;
    }
    DefinitionAliases.Add(OldId, NewId);

    TObjectPtr<UGuDefinition> Asset;
    if (AuthoredAssetsById.RemoveAndCopyValue(OldId, Asset) && Asset)
    {
        Asset->StableDefinitionId = NewId;
        AuthoredAssetsById.Add(NewId, Asset);
    }

    TObjectPtr<UGuDefinition> RuntimeAsset;
    if (RuntimeAssetsById.RemoveAndCopyValue(OldId, RuntimeAsset) && RuntimeAsset)
    {
        RuntimeAsset->StableDefinitionId = NewId;
        RuntimeAssetsById.Add(NewId, RuntimeAsset);
    }

    if (RuntimeDefinitionIds.Remove(OldId) > 0)
    {
        RuntimeDefinitionIds.Add(NewId);
    }
    for (TPair<FString, FName>& Fingerprint : RuntimeIdByFingerprint)
    {
        if (Fingerprint.Value == OldId) Fingerprint.Value = NewId;
    }
}

FName UGuDefinitionRegistrySubsystem::DefinitionIdForAsset(const UGuDefinition* Asset)
{
    if (!IsValid(Asset)) return NAME_None;
    if (!Asset->StableDefinitionId.IsNone()) return Asset->StableDefinitionId;
    return Asset->GetFName();
}

bool UGuDefinitionRegistrySubsystem::BuildRecordFromAsset(
    const UGuDefinition* Asset,
    FGuDefinitionRecord& OutRecord,
    FString& OutError)
{
    if (!IsValid(Asset))
    {
        OutError = TEXT("Definition asset is null.");
        return false;
    }

    FGuDefinitionRecord Record;
    Record.Id = DefinitionIdForAsset(Asset);
    Record.Name = Asset->Name.IsEmpty() ? Asset->GetName() : Asset->Name.ToString();
    Record.Rank = FMath::Clamp(Asset->Rank, 1, 9);
    Record.Kind = Record.Rank >= 6 ? EGuKind::Immortal : EGuKind::Mortal;
    Record.Path = GameplayTagLeaf(Asset->Path);
    for (const FGameplayTag& SecondaryPathTag : Asset->SecondaryPaths.GetGameplayTagArray())
    {
        const FName SecondaryPath = GameplayTagLeaf(SecondaryPathTag);
        if (!SecondaryPath.IsNone() && SecondaryPath != Record.Path) Record.SecondaryPaths.AddUnique(SecondaryPath);
    }
    Record.PathRelation = Record.SecondaryPaths.IsEmpty() ? TEXT("Pure-path Gu") : TEXT("Multi-path Gu");
    Record.Category = TEXT("Authored Gu");
    Record.Description = FString::Printf(TEXT("Authored Gu definition from %s."), *Asset->GetPathName());
    Record.ActivationModel = Asset->ActivationModel;
    Record.Feeding = Asset->Feeding;
    Record.Lifecycle = Asset->Lifecycle;
    Record.RefinementTraits = Asset->RefinementTraits;
    Record.RefinementProfile = Asset->RefinementProfile;
    Record.RefinementAssistance = Asset->RefinementAssistance;
    Record.Appearance = Asset->Appearance;
    Record.Source = Asset->GetPathName();
    Record.bCustom = false;

    for (const FGameplayTag& Tag : Asset->Tags.GetGameplayTagArray())
    {
        if (Tag.IsValid()) Record.Tags.Add(FName(*Tag.ToString()));
    }

    bool bHasProjectile = false;
    bool bHasMelee = false;
    bool bHasArea = false;
    bool bHasDamage = false;
    bool bHasKnockback = false;
    bool bHasHeal = false;
    bool bHasShield = false;
    bool bHasMovement = false;
    bool bHasRestriction = false;
    bool bHasConcealment = false;
    bool bHasReveal = false;
    bool bHasBuff = false;
    bool bHasDamageOverTime = false;
    bool bHasHealOverTime = false;
    bool bHasEssenceChange = false;
    bool bHasEssenceRegeneration = false;
    bool bHasDisplacement = false;
    bool bHasGuSuppression = false;
    bool bHasCleanse = false;
    bool bHasDispel = false;
    bool bHasField = false;
    bool bHasChain = false;
    bool bHasMark = false;
    bool bHasAttentionBoost = false;
    bool bHasRefinementAssist = false;
    bool bHasSummon = false;
    float MaxDamage = 0.0f;
    float MaxKnockback = 0.0f;
    float MaxHealing = 0.0f;
    float MaxShield = 0.0f;
    float MaxBuffMagnitude = 0.0f;
    float MaxBuffDuration = 0.0f;

    Record.EffectProfile.Input = TEXT("Primeval essence");

    for (const TInstancedStruct<FGuMechanic>& Mechanic : Asset->Mechanics)
    {
        if (const FGuEssenceCostMechanic* Cost = Mechanic.GetPtr<FGuEssenceCostMechanic>())
        {
            Record.EssenceCost = Cost->Cost;
            AddMechanic(Record, TEXT("essence_cost"), FString::Printf(TEXT("{\"cost\":%.6f}"), Cost->Cost));
            continue;
        }

        if (const FGuProjectileMechanic* Projectile = Mechanic.GetPtr<FGuProjectileMechanic>())
        {
            bHasProjectile = true;
            Record.EffectProfile.Carrier = TEXT("projectile");
            Record.EffectProfile.Manifestation = TEXT("projectile");
            Record.EffectProfile.Range = FMath::Max(Record.EffectProfile.Range, Projectile->MaxRange);
            AddMechanic(
                Record,
                TEXT("projectile"),
                FString::Printf(
                    TEXT("{\"speed\":%.6f,\"range\":%.6f,\"radius\":%.6f,\"gravityScale\":%.6f}"),
                    Projectile->Speed,
                    Projectile->MaxRange,
                    Projectile->Radius,
                    Projectile->GravityScale));
            continue;
        }

        if (const FGuDamageMechanic* Damage = Mechanic.GetPtr<FGuDamageMechanic>())
        {
            bHasDamage = true;
            MaxDamage = FMath::Max(MaxDamage, Damage->Damage);
            Record.EffectProfile.CoreEffect = TEXT("damage");
            Record.EffectProfile.Operation = TEXT("harm");
            Record.EffectProfile.Magnitude = FMath::Max(Record.EffectProfile.Magnitude, Damage->Damage);
            AddMechanic(Record, TEXT("damage"), FString::Printf(TEXT("{\"damage\":%.6f}"), Damage->Damage));
            continue;
        }

        if (const FGuKnockbackMechanic* Knockback = Mechanic.GetPtr<FGuKnockbackMechanic>())
        {
            bHasKnockback = true;
            MaxKnockback = FMath::Max(MaxKnockback, Knockback->Strength);
            if (Record.EffectProfile.CoreEffect.IsEmpty())
            {
                Record.EffectProfile.CoreEffect = TEXT("knockback");
                Record.EffectProfile.Operation = TEXT("displace");
                Record.EffectProfile.Magnitude = FMath::Max(Record.EffectProfile.Magnitude, Knockback->Strength);
            }
            AddMechanic(
                Record,
                TEXT("knockback"),
                FString::Printf(
                    TEXT("{\"strength\":%.6f,\"verticalStrength\":%.6f}"),
                    Knockback->Strength,
                    Knockback->VerticalStrength));
            continue;
        }

        if (const FGuBuffMechanic* Buff = Mechanic.GetPtr<FGuBuffMechanic>())
        {
            bHasBuff = true;
            MaxBuffMagnitude = FMath::Max(MaxBuffMagnitude, FMath::Abs(Buff->Magnitude));
            MaxBuffDuration = FMath::Max(MaxBuffDuration, Buff->Duration);
            if (Record.EffectProfile.CoreEffect.IsEmpty())
            {
                Record.EffectProfile.CoreEffect = TEXT("stat_modifier");
                Record.EffectProfile.Operation = TEXT("modify_self");
                Record.EffectProfile.Magnitude = FMath::Max(Record.EffectProfile.Magnitude, FMath::Abs(Buff->Magnitude));
            }
            Record.EffectProfile.DurationMs = FMath::Max(
                Record.EffectProfile.DurationMs,
                FMath::RoundToInt(Buff->Duration * 1000.0f));

            const FString AttributeName = Buff->Attribute.IsValid() ? Buff->Attribute.GetName() : TEXT("unknown");
            const FString EscapedAttributeName = EscapeJsonForMechanicRecord(AttributeName);
            AddMechanic(
                Record,
                TEXT("stat_modifier"),
                FString::Printf(
                    TEXT("{\"stat\":\"%s\",\"magnitude\":%.6f,\"duration\":%.6f}"),
                    *EscapedAttributeName,
                    Buff->Magnitude,
                    Buff->Duration));
            continue;
        }

        if (const FGuMeleeMechanic* Melee = Mechanic.GetPtr<FGuMeleeMechanic>())
        {
            bHasMelee = true;
            Record.EffectProfile.Carrier = TEXT("melee");
            Record.EffectProfile.Manifestation = TEXT("melee");
            Record.EffectProfile.Range = FMath::Max(Record.EffectProfile.Range, Melee->Range);
            AddMechanic(
                Record,
                TEXT("melee"),
                FString::Printf(
                    TEXT("{\"range\":%.6f,\"radius\":%.6f,\"arcDegrees\":%.6f,\"maxTargets\":%d}"),
                    Melee->Range, Melee->Radius, Melee->ArcDegrees, Melee->MaxTargets));
            continue;
        }

        if (const FGuAreaMechanic* Area = Mechanic.GetPtr<FGuAreaMechanic>())
        {
            bHasArea = true;
            Record.EffectProfile.Carrier = TEXT("area");
            Record.EffectProfile.Manifestation = TEXT("area");
            Record.EffectProfile.Area = FMath::Max(Record.EffectProfile.Area, Area->Radius);
            AddMechanic(
                Record,
                TEXT("area"),
                FString::Printf(
                    TEXT("{\"radius\":%.6f,\"forwardOffset\":%.6f,\"maxTargets\":%d,\"includeSelf\":%s}"),
                    Area->Radius, Area->ForwardOffset, Area->MaxTargets, Area->bIncludeSelf ? TEXT("true") : TEXT("false")));
            continue;
        }

        if (const FGuHealMechanic* Heal = Mechanic.GetPtr<FGuHealMechanic>())
        {
            bHasHeal = true;
            MaxHealing = FMath::Max(MaxHealing, Heal->Amount);
            if (Record.EffectProfile.CoreEffect.IsEmpty())
            {
                Record.EffectProfile.CoreEffect = TEXT("heal");
                Record.EffectProfile.Operation = TEXT("restore");
                Record.EffectProfile.Magnitude = FMath::Max(Record.EffectProfile.Magnitude, Heal->Amount);
            }
            AddMechanic(Record, TEXT("heal"), FString::Printf(
                TEXT("{\"amount\":%.6f,\"recipient\":%d}"),
                Heal->Amount, static_cast<int32>(Heal->Recipient)));
            continue;
        }

        if (const FGuShieldMechanic* Shield = Mechanic.GetPtr<FGuShieldMechanic>())
        {
            bHasShield = true;
            MaxShield = FMath::Max(MaxShield, Shield->Amount);
            if (Record.EffectProfile.CoreEffect.IsEmpty())
            {
                Record.EffectProfile.CoreEffect = TEXT("shield");
                Record.EffectProfile.Operation = TEXT("protect");
                Record.EffectProfile.Magnitude = FMath::Max(Record.EffectProfile.Magnitude, Shield->Amount);
            }
            Record.EffectProfile.DurationMs = FMath::Max(Record.EffectProfile.DurationMs, FMath::RoundToInt(Shield->Duration * 1000.0f));
            AddMechanic(Record, TEXT("shield"), FString::Printf(
                TEXT("{\"amount\":%.6f,\"duration\":%.6f,\"recipient\":%d}"),
                Shield->Amount, Shield->Duration, static_cast<int32>(Shield->Recipient)));
            continue;
        }

        if (const FGuMovementMechanic* Movement = Mechanic.GetPtr<FGuMovementMechanic>())
        {
            bHasMovement = true;
            if (Record.EffectProfile.CoreEffect.IsEmpty())
            {
                Record.EffectProfile.CoreEffect = TEXT("movement");
                Record.EffectProfile.Operation = TEXT("move");
            }
            Record.EffectProfile.DurationMs = FMath::Max(Record.EffectProfile.DurationMs, FMath::RoundToInt(Movement->Duration * 1000.0f));
            const FString Mode = StaticEnum<EGuMovementMode>()
                ? StaticEnum<EGuMovementMode>()->GetNameStringByValue(static_cast<int64>(Movement->Mode))
                : TEXT("SpeedMultiplier");
            AddMechanic(Record, TEXT("movement"), FString::Printf(
                TEXT("{\"mode\":\"%s\",\"speedMultiplier\":%.6f,\"duration\":%.6f,\"dashSpeed\":%.6f,\"verticalSpeed\":%.6f,\"blinkDistance\":%.6f,\"recipient\":%d}"),
                *Mode, Movement->SpeedMultiplier, Movement->Duration, Movement->DashSpeed, Movement->VerticalSpeed, Movement->BlinkDistance, static_cast<int32>(Movement->Recipient)));
            continue;
        }

        if (const FGuRestrictionMechanic* Restriction = Mechanic.GetPtr<FGuRestrictionMechanic>())
        {
            bHasRestriction = true;
            if (Record.EffectProfile.CoreEffect.IsEmpty())
            {
                Record.EffectProfile.CoreEffect = TEXT("restriction");
                Record.EffectProfile.Operation = TEXT("suppress");
            }
            Record.EffectProfile.DurationMs = FMath::Max(Record.EffectProfile.DurationMs, FMath::RoundToInt(Restriction->Duration * 1000.0f));
            AddMechanic(Record, TEXT("restriction"), FString::Printf(
                TEXT("{\"movementMultiplier\":%.6f,\"duration\":%.6f}"),
                Restriction->MovementMultiplier, Restriction->Duration));
            continue;
        }

        if (const FGuConcealmentMechanic* Concealment = Mechanic.GetPtr<FGuConcealmentMechanic>())
        {
            bHasConcealment = true;
            if (Record.EffectProfile.CoreEffect.IsEmpty())
            {
                Record.EffectProfile.CoreEffect = TEXT("concealment");
                Record.EffectProfile.Operation = TEXT("conceal");
            }
            Record.EffectProfile.DurationMs = FMath::Max(Record.EffectProfile.DurationMs, FMath::RoundToInt(Concealment->Duration * 1000.0f));
            AddMechanic(Record, TEXT("conceal"), FString::Printf(
                TEXT("{\"opacity\":%.6f,\"detectionResistance\":%.6f,\"duration\":%.6f,\"breakOnAttack\":%s}"),
                Concealment->Opacity, Concealment->DetectionResistance, Concealment->Duration, Concealment->bBreakOnAttack ? TEXT("true") : TEXT("false")));
            continue;
        }

        if (const FGuRevealMechanic* Reveal = Mechanic.GetPtr<FGuRevealMechanic>())
        {
            bHasReveal = true;
            if (Record.EffectProfile.CoreEffect.IsEmpty())
            {
                Record.EffectProfile.CoreEffect = TEXT("reveal");
                Record.EffectProfile.Operation = TEXT("investigate");
            }
            Record.EffectProfile.Range = FMath::Max(Record.EffectProfile.Range, Reveal->Range);
            Record.EffectProfile.DurationMs = FMath::Max(Record.EffectProfile.DurationMs, FMath::RoundToInt(Reveal->Duration * 1000.0f));
            AddMechanic(Record, TEXT("reveal"), FString::Printf(
                TEXT("{\"range\":%.6f,\"duration\":%.6f,\"strength\":%.6f}"),
                Reveal->Range, Reveal->Duration, Reveal->Strength));
            continue;
        }

        if (const FGuDamageOverTimeMechanic* Periodic = Mechanic.GetPtr<FGuDamageOverTimeMechanic>())
        {
            bHasDamageOverTime = true;
            if (Record.EffectProfile.CoreEffect.IsEmpty())
            {
                Record.EffectProfile.CoreEffect = TEXT("damage_over_time");
                Record.EffectProfile.Operation = TEXT("erode");
            }
            Record.EffectProfile.DurationMs = FMath::Max(Record.EffectProfile.DurationMs, FMath::RoundToInt(Periodic->Duration * 1000.0f));
            AddMechanic(Record, TEXT("damage_over_time"), FString::Printf(
                TEXT("{\"damagePerTick\":%.6f,\"tickInterval\":%.6f,\"duration\":%.6f,\"recipient\":%d}"),
                Periodic->DamagePerTick, Periodic->TickInterval, Periodic->Duration, static_cast<int32>(Periodic->Recipient)));
            continue;
        }

        if (const FGuHealOverTimeMechanic* Periodic = Mechanic.GetPtr<FGuHealOverTimeMechanic>())
        {
            bHasHealOverTime = true;
            if (Record.EffectProfile.CoreEffect.IsEmpty())
            {
                Record.EffectProfile.CoreEffect = TEXT("healing_over_time");
                Record.EffectProfile.Operation = TEXT("regenerate");
            }
            Record.EffectProfile.DurationMs = FMath::Max(Record.EffectProfile.DurationMs, FMath::RoundToInt(Periodic->Duration * 1000.0f));
            AddMechanic(Record, TEXT("heal_over_time"), FString::Printf(
                TEXT("{\"healPerTick\":%.6f,\"tickInterval\":%.6f,\"duration\":%.6f,\"recipient\":%d}"),
                Periodic->HealPerTick, Periodic->TickInterval, Periodic->Duration, static_cast<int32>(Periodic->Recipient)));
            continue;
        }

        if (const FGuEssenceChangeMechanic* Essence = Mechanic.GetPtr<FGuEssenceChangeMechanic>())
        {
            bHasEssenceChange = true;
            if (Record.EffectProfile.CoreEffect.IsEmpty())
            {
                Record.EffectProfile.CoreEffect = Essence->Mode == EGuEssenceChangeMode::Restore ? TEXT("essence_restore") : TEXT("essence_drain");
                Record.EffectProfile.Operation = Essence->Mode == EGuEssenceChangeMode::Restore ? TEXT("restore_resource") : TEXT("drain_resource");
            }
            AddMechanic(Record, TEXT("essence_change"), FString::Printf(
                TEXT("{\"mode\":%d,\"amount\":%.6f,\"percentOfMaximum\":%s,\"recipient\":%d,\"transfer\":%s}"),
                static_cast<int32>(Essence->Mode), Essence->Amount,
                Essence->bPercentOfMaximum ? TEXT("true") : TEXT("false"), static_cast<int32>(Essence->Recipient),
                Essence->bTransferDrainedEssenceToSource ? TEXT("true") : TEXT("false")));
            continue;
        }

        if (const FGuEssenceRegenerationMechanic* Regen = Mechanic.GetPtr<FGuEssenceRegenerationMechanic>())
        {
            bHasEssenceRegeneration = true;
            if (Record.EffectProfile.CoreEffect.IsEmpty())
            {
                Record.EffectProfile.CoreEffect = TEXT("essence_regeneration");
                Record.EffectProfile.Operation = TEXT("produce_resource");
            }
            Record.EffectProfile.DurationMs = FMath::Max(Record.EffectProfile.DurationMs, FMath::RoundToInt(Regen->Duration * 1000.0f));
            AddMechanic(Record, TEXT("essence_regeneration"), FString::Printf(
                TEXT("{\"flatPerSecond\":%.6f,\"percentPerSecond\":%.6f,\"duration\":%.6f,\"recipient\":%d}"),
                Regen->FlatPerSecond, Regen->PercentOfMaximumPerSecond, Regen->Duration, static_cast<int32>(Regen->Recipient)));
            continue;
        }

        if (const FGuDisplacementMechanic* Displacement = Mechanic.GetPtr<FGuDisplacementMechanic>())
        {
            bHasDisplacement = true;
            if (Record.EffectProfile.CoreEffect.IsEmpty())
            {
                Record.EffectProfile.CoreEffect = TEXT("displacement");
                Record.EffectProfile.Operation = TEXT("displace");
            }
            AddMechanic(Record, TEXT("displacement"), FString::Printf(
                TEXT("{\"mode\":%d,\"strength\":%.6f,\"verticalStrength\":%.6f}"),
                static_cast<int32>(Displacement->Mode), Displacement->Strength, Displacement->VerticalStrength));
            continue;
        }

        if (const FGuGuSuppressionMechanic* Suppression = Mechanic.GetPtr<FGuGuSuppressionMechanic>())
        {
            bHasGuSuppression = true;
            if (Record.EffectProfile.CoreEffect.IsEmpty())
            {
                Record.EffectProfile.CoreEffect = TEXT("gu_suppression");
                Record.EffectProfile.Operation = TEXT("suppress_activation");
            }
            Record.EffectProfile.DurationMs = FMath::Max(Record.EffectProfile.DurationMs, FMath::RoundToInt(Suppression->Duration * 1000.0f));
            AddMechanic(Record, TEXT("gu_suppression"), FString::Printf(TEXT("{\"duration\":%.6f}"), Suppression->Duration));
            continue;
        }

        if (const FGuCleanseMechanic* Cleanse = Mechanic.GetPtr<FGuCleanseMechanic>())
        {
            bHasCleanse = true;
            if (Record.EffectProfile.CoreEffect.IsEmpty())
            {
                Record.EffectProfile.CoreEffect = TEXT("cleanse");
                Record.EffectProfile.Operation = TEXT("remove_harmful_state");
            }
            AddMechanic(Record, TEXT("cleanse"), FString::Printf(
                TEXT("{\"recipient\":%d,\"dot\":%s,\"restriction\":%s,\"suppression\":%s,\"marks\":%s}"),
                static_cast<int32>(Cleanse->Recipient),
                Cleanse->bRemoveDamageOverTime ? TEXT("true") : TEXT("false"),
                Cleanse->bRemoveRestrictions ? TEXT("true") : TEXT("false"),
                Cleanse->bRemoveGuSuppression ? TEXT("true") : TEXT("false"),
                Cleanse->bRemoveMarks ? TEXT("true") : TEXT("false")));
            continue;
        }

        if (const FGuDispelMechanic* Dispel = Mechanic.GetPtr<FGuDispelMechanic>())
        {
            bHasDispel = true;
            if (Record.EffectProfile.CoreEffect.IsEmpty())
            {
                Record.EffectProfile.CoreEffect = TEXT("dispel");
                Record.EffectProfile.Operation = TEXT("remove_beneficial_state");
            }
            AddMechanic(Record, TEXT("dispel"), FString::Printf(
                TEXT("{\"recipient\":%d,\"shields\":%s,\"movement\":%s,\"hot\":%s,\"essenceRegen\":%s,\"concealment\":%s,\"reveal\":%s}"),
                static_cast<int32>(Dispel->Recipient),
                Dispel->bRemoveShields ? TEXT("true") : TEXT("false"),
                Dispel->bRemoveMovementBuffs ? TEXT("true") : TEXT("false"),
                Dispel->bRemoveHealingOverTime ? TEXT("true") : TEXT("false"),
                Dispel->bRemoveEssenceRegeneration ? TEXT("true") : TEXT("false"),
                Dispel->bRemoveConcealment ? TEXT("true") : TEXT("false"),
                Dispel->bRemoveReveal ? TEXT("true") : TEXT("false")));
            continue;
        }

        if (const FGuFieldMechanic* Field = Mechanic.GetPtr<FGuFieldMechanic>())
        {
            bHasField = true;
            Record.EffectProfile.Carrier = TEXT("field");
            Record.EffectProfile.Manifestation = TEXT("field");
            Record.EffectProfile.Area = FMath::Max(Record.EffectProfile.Area, Field->Radius);
            Record.EffectProfile.DurationMs = FMath::Max(Record.EffectProfile.DurationMs, FMath::RoundToInt(Field->Duration * 1000.0f));
            AddMechanic(Record, TEXT("field"), FString::Printf(
                TEXT("{\"radius\":%.6f,\"forwardOffset\":%.6f,\"tickInterval\":%.6f,\"duration\":%.6f,\"maxTargets\":%d,\"includeSelf\":%s}"),
                Field->Radius, Field->ForwardOffset, Field->TickInterval, Field->Duration, Field->MaxTargetsPerPulse,
                Field->bIncludeSelf ? TEXT("true") : TEXT("false")));
            continue;
        }

        if (const FGuChainMechanic* Chain = Mechanic.GetPtr<FGuChainMechanic>())
        {
            bHasChain = true;
            AddMechanic(Record, TEXT("chain"), FString::Printf(
                TEXT("{\"jumpRadius\":%.6f,\"maxAdditionalTargets\":%d,\"magnitudeFalloff\":%.6f}"),
                Chain->JumpRadius, Chain->MaxAdditionalTargets, Chain->MagnitudeFalloff));
            continue;
        }

        if (const FGuMarkMechanic* Mark = Mechanic.GetPtr<FGuMarkMechanic>())
        {
            bHasMark = true;
            if (Record.EffectProfile.CoreEffect.IsEmpty())
            {
                Record.EffectProfile.CoreEffect = TEXT("mark");
                Record.EffectProfile.Operation = TEXT("link_target");
            }
            const FString EscapedMark = EscapeJsonForMechanicRecord(Mark->MarkId.ToString());
            AddMechanic(Record, TEXT("mark"), FString::Printf(
                TEXT("{\"markId\":\"%s\",\"strength\":%.6f,\"duration\":%.6f}"),
                *EscapedMark, Mark->Strength, Mark->Duration));
            continue;
        }

        if (const FGuAttentionBoostMechanic* Boost = Mechanic.GetPtr<FGuAttentionBoostMechanic>())
        {
            bHasAttentionBoost = true;
            if (Record.EffectProfile.CoreEffect.IsEmpty())
            {
                Record.EffectProfile.CoreEffect = TEXT("attention_boost");
                Record.EffectProfile.Operation = TEXT("divide_attention");
            }
            AddMechanic(Record, TEXT("multitasking_boost"), FString::Printf(
                TEXT("{\"slotsGranted\":%d,\"duration\":%.6f}"), Boost->SlotsGranted, Boost->Duration));
            continue;
        }

        if (const FGuRefinementAssistMechanic* Assist = Mechanic.GetPtr<FGuRefinementAssistMechanic>())
        {
            bHasRefinementAssist = true;
            Record.RefinementAssistance.bEnabled = true;
            Record.RefinementAssistance.ProgressPercent = Assist->ProgressPercent;
            Record.RefinementAssistance.StabilityPerAction = Assist->StabilityPerAction;
            Record.RefinementAssistance.ImpurityReductionPerAction = Assist->ImpurityReductionPerAction;
            Record.RefinementAssistance.QualityBonus = Assist->QualityBonus;
            Record.RefinementAssistance.ActionUses = FMath::Max(1, Assist->ActionUses);
            Record.RefinementAssistance.Processes = Assist->Processes;
            if (Record.RefinementAssistance.Processes.IsEmpty())
            {
                Record.RefinementAssistance.Processes = { FName(TEXT("Process")), FName(TEXT("Control")), FName(TEXT("Condense")) };
            }
            AddMechanic(Record, TEXT("refinement_assistance"), FString::Printf(
                TEXT("{\"progressPercent\":%.6f,\"stabilityPerAction\":%.6f,\"impurityReductionPerAction\":%.6f,\"qualityBonus\":%.6f,\"actionUses\":%d,\"processes\":%s}"),
                Assist->ProgressPercent, Assist->StabilityPerAction, Assist->ImpurityReductionPerAction, Assist->QualityBonus,
                FMath::Max(1, Assist->ActionUses), *NamesToJsonArray(Record.RefinementAssistance.Processes)));
            continue;
        }

        if (const FGuSummonMechanic* Summon = Mechanic.GetPtr<FGuSummonMechanic>())
        {
            bHasSummon = true;
            if (Record.EffectProfile.CoreEffect.IsEmpty())
            {
                Record.EffectProfile.CoreEffect = TEXT("summon");
                Record.EffectProfile.Operation = TEXT("manifest_actor");
            }
            const FString SpawnClass = EscapeJsonForMechanicRecord(GetNameSafe(Summon->ActorClass.Get()));
            AddMechanic(Record, TEXT("summon"), FString::Printf(
                TEXT("{\"actorClass\":\"%s\",\"count\":%d,\"spawnRadius\":%.6f,\"forwardOffset\":%.6f,\"lifetime\":%.6f}"),
                *SpawnClass, Summon->Count, Summon->SpawnRadius, Summon->ForwardOffset, Summon->Lifetime));
            continue;
        }

        const UScriptStruct* StructType = Mechanic.GetScriptStruct();
        const FString StructName = GetNameSafe(StructType);
        const FString EscapedStructName = EscapeJsonForMechanicRecord(StructName);
        AddMechanic(
            Record,
            TEXT("semantic_only"),
            FString::Printf(TEXT("{\"sourceStruct\":\"%s\"}"), *EscapedStructName));
    }

    // A Gu is still a refinable physical Gu even while an activation mechanic is
    // not implemented yet. Do not make ECS/refinement registration contingent on GAS support.
    if (Record.Mechanics.IsEmpty()) AddMechanic(Record, TEXT("semantic_only"));

    // Legacy authored assets predate the shared semantic schema. Materialize the
    // browser-equivalent Path/attribute/trait/template profile here so every
    // physical ECS worm receives the real semantics rather than an empty shell.
    if (!Asset->bRefinementSemanticsMaterialized)
    {
        Record.RefinementProfile = UGuRulesLibrary::BuildEffectiveGuRefinementProfile(Record);
    }
    else
    {
        UGuRulesLibrary::NormalizeSemanticProfile(Record.RefinementProfile);
        if (Record.RefinementProfile.Properties.IsEmpty())
        {
            UGuRulesLibrary::MaterializeDerivedPropertySnapshot(Record.RefinementProfile);
        }
    }

    // Populate the runtime effect/killer-move vocabulary from the same authored
    // mechanics that GAS executes. This gives graph compilation concrete payloads
    // instead of role labels with no physical meaning.
    const float SemanticArea = Record.RefinementProfile.Attributes.FindRef(TEXT("area"));
    const float SemanticRange = Record.RefinementProfile.Attributes.FindRef(TEXT("range"));
    const float SemanticSpeed = Record.RefinementProfile.Attributes.FindRef(TEXT("speed"));
    const float SemanticPrecision = Record.RefinementProfile.Attributes.FindRef(TEXT("precision"));

    if (Record.EffectProfile.Area <= 0.0f && SemanticArea > 0.0f)
    {
        Record.EffectProfile.Area = 120.0f + SemanticArea * 180.0f;
    }
    if (Record.EffectProfile.Range <= 0.0f && SemanticRange > 0.0f)
    {
        Record.EffectProfile.Range = 150.0f + SemanticRange * 350.0f;
    }

    if (bHasProjectile)
    {
        Record.KillerMove.Template = TEXT("projectile");
        Record.KillerMove.Contributes.AddUnique(TEXT("delivery"));
        Record.KillerMove.SuitableRoles.AddUnique(TEXT("core"));
        Record.KillerMove.SuitableRoles.AddUnique(TEXT("medium"));
        Record.KillerMove.SuitableRoles.AddUnique(TEXT("routing"));
    }
    else if (bHasField)
    {
        Record.KillerMove.Template = TEXT("field");
        Record.EffectProfile.Manifestation = TEXT("field");
        Record.KillerMove.SuitableRoles.AddUnique(TEXT("core"));
        Record.KillerMove.SuitableRoles.AddUnique(TEXT("boundary"));
        Record.KillerMove.SuitableRoles.AddUnique(TEXT("storage"));
    }
    else if (bHasArea)
    {
        Record.KillerMove.Template = TEXT("area");
        Record.EffectProfile.Manifestation = TEXT("area");
        Record.KillerMove.SuitableRoles.AddUnique(TEXT("core"));
        Record.KillerMove.SuitableRoles.AddUnique(TEXT("boundary"));
        Record.KillerMove.SuitableRoles.AddUnique(TEXT("output"));
    }
    else if (bHasMelee)
    {
        Record.KillerMove.Template = TEXT("melee");
        Record.EffectProfile.Manifestation = TEXT("melee");
        Record.KillerMove.SuitableRoles.AddUnique(TEXT("core"));
        Record.KillerMove.SuitableRoles.AddUnique(TEXT("output"));
    }
    else if (bHasDamage || bHasKnockback || bHasRestriction || bHasDamageOverTime || bHasDisplacement || bHasGuSuppression || bHasMark)
    {
        Record.KillerMove.Template = SemanticArea >= 0.55f ? TEXT("area") : TEXT("melee");
        Record.EffectProfile.Manifestation = Record.KillerMove.Template.ToString();
        Record.KillerMove.SuitableRoles.AddUnique(TEXT("core"));
        Record.KillerMove.SuitableRoles.AddUnique(TEXT("output"));
    }
    else if (bHasMovement)
    {
        Record.KillerMove.Template = TEXT("movement");
        Record.EffectProfile.Manifestation = TEXT("self");
        Record.KillerMove.SuitableRoles.AddUnique(TEXT("core"));
        Record.KillerMove.SuitableRoles.AddUnique(TEXT("medium"));
        Record.KillerMove.SuitableRoles.AddUnique(TEXT("routing"));
    }
    else if (bHasHeal || bHasShield || bHasConcealment || bHasReveal || bHasBuff || bHasHealOverTime || bHasEssenceChange || bHasEssenceRegeneration || bHasCleanse || bHasDispel || bHasAttentionBoost || bHasRefinementAssist || bHasSummon)
    {
        Record.KillerMove.Template = TEXT("self");
        Record.EffectProfile.Manifestation = TEXT("self");
        if (bHasShield)
        {
            Record.KillerMove.SuitableRoles.AddUnique(TEXT("stabilization"));
            Record.KillerMove.SuitableRoles.AddUnique(TEXT("buffer"));
        }
        if (bHasHeal) Record.KillerMove.SuitableRoles.AddUnique(TEXT("recovery"));
        if (bHasConcealment) Record.KillerMove.SuitableRoles.AddUnique(TEXT("concealment"));
        if (bHasReveal)
        {
            Record.KillerMove.SuitableRoles.AddUnique(TEXT("investigation_sensor"));
            Record.KillerMove.SuitableRoles.AddUnique(TEXT("targeting"));
        }
        if (bHasBuff) Record.KillerMove.SuitableRoles.AddUnique(TEXT("amplification"));
        if (bHasHealOverTime || bHasCleanse) Record.KillerMove.SuitableRoles.AddUnique(TEXT("recovery"));
        if (bHasEssenceRegeneration || bHasAttentionBoost) Record.KillerMove.SuitableRoles.AddUnique(TEXT("fuel"));
        if (bHasDispel) Record.KillerMove.SuitableRoles.AddUnique(TEXT("suppression"));
        if (bHasRefinementAssist) Record.KillerMove.SuitableRoles.AddUnique(TEXT("control"));
        if (bHasSummon) Record.KillerMove.SuitableRoles.AddUnique(TEXT("subordinate"));
    }

    if (bHasDamage || bHasKnockback || bHasRestriction || bHasDamageOverTime || bHasDisplacement || bHasGuSuppression || bHasMark)
    {
        Record.EffectProfile.ValidTargets.AddUnique(TEXT("Actor"));
        Record.EffectProfile.ValidTargets.AddUnique(TEXT("Character"));
        Record.EffectProfile.TargetLink = TEXT("world target");
    }
    if (bHasBuff && !bHasDamage && !bHasKnockback)
    {
        Record.EffectProfile.ValidTargets.AddUnique(TEXT("Self"));
        Record.EffectProfile.TargetLink = TEXT("self");
    }

    if (bHasDamage)
    {
        Record.KillerMove.Contributes.AddUnique(TEXT("damage"));
        Record.KillerMove.Power = FMath::Max(Record.KillerMove.Power, MaxDamage);
    }
    if (bHasKnockback)
    {
        Record.KillerMove.Contributes.AddUnique(TEXT("knockback"));
        Record.KillerMove.Power = FMath::Max(Record.KillerMove.Power, MaxKnockback * 0.02f);
        Record.KillerMove.SuitableRoles.AddUnique(TEXT("suppression"));
    }
    if (bHasBuff)
    {
        Record.KillerMove.Contributes.AddUnique(TEXT("stat_modifier"));
        Record.KillerMove.Power = FMath::Max(Record.KillerMove.Power, MaxBuffMagnitude);
    }
    if (bHasHeal)
    {
        Record.KillerMove.Contributes.AddUnique(TEXT("heal"));
        Record.KillerMove.Power = FMath::Max(Record.KillerMove.Power, MaxHealing);
    }
    if (bHasShield)
    {
        Record.KillerMove.Contributes.AddUnique(TEXT("shield"));
        Record.KillerMove.Power = FMath::Max(Record.KillerMove.Power, MaxShield);
    }
    if (bHasMovement) Record.KillerMove.Contributes.AddUnique(TEXT("movement"));
    if (bHasRestriction)
    {
        Record.KillerMove.Contributes.AddUnique(TEXT("restriction"));
        Record.KillerMove.SuitableRoles.AddUnique(TEXT("suppression"));
        Record.KillerMove.SuitableRoles.AddUnique(TEXT("control"));
    }
    if (bHasConcealment) Record.KillerMove.Contributes.AddUnique(TEXT("concealment"));
    if (bHasReveal) Record.KillerMove.Contributes.AddUnique(TEXT("reveal"));
    if (bHasDamageOverTime) Record.KillerMove.Contributes.AddUnique(TEXT("damage_over_time"));
    if (bHasHealOverTime) Record.KillerMove.Contributes.AddUnique(TEXT("heal_over_time"));
    if (bHasEssenceChange) Record.KillerMove.Contributes.AddUnique(TEXT("essence_change"));
    if (bHasEssenceRegeneration) Record.KillerMove.Contributes.AddUnique(TEXT("essence_regeneration"));
    if (bHasDisplacement) Record.KillerMove.Contributes.AddUnique(TEXT("displacement"));
    if (bHasGuSuppression) Record.KillerMove.Contributes.AddUnique(TEXT("gu_suppression"));
    if (bHasCleanse) Record.KillerMove.Contributes.AddUnique(TEXT("cleanse"));
    if (bHasDispel) Record.KillerMove.Contributes.AddUnique(TEXT("dispel"));
    if (bHasField) Record.KillerMove.Contributes.AddUnique(TEXT("field"));
    if (bHasChain) Record.KillerMove.Contributes.AddUnique(TEXT("chain"));
    if (bHasMark) Record.KillerMove.Contributes.AddUnique(TEXT("mark"));
    if (bHasAttentionBoost) Record.KillerMove.Contributes.AddUnique(TEXT("attention_boost"));
    if (bHasRefinementAssist) Record.KillerMove.Contributes.AddUnique(TEXT("refinement_assistance"));
    if (bHasSummon) Record.KillerMove.Contributes.AddUnique(TEXT("summon"));
    if (bHasGuSuppression || bHasDispel || bHasDisplacement) Record.KillerMove.SuitableRoles.AddUnique(TEXT("suppression"));
    if (bHasMark || bHasChain) Record.KillerMove.SuitableRoles.AddUnique(TEXT("link"));
    if (bHasField) Record.KillerMove.SuitableRoles.AddUnique(TEXT("boundary"));
    if (SemanticSpeed > 0.0f) Record.KillerMove.Attributes.AddUnique(TEXT("speed"));
    if (SemanticRange > 0.0f) Record.KillerMove.Attributes.AddUnique(TEXT("range"));
    if (SemanticArea > 0.0f) Record.KillerMove.Attributes.AddUnique(TEXT("area"));
    if (SemanticPrecision > 0.0f) Record.KillerMove.Attributes.AddUnique(TEXT("precision"));

    OutRecord = MoveTemp(Record);
    OutError.Reset();
    return true;
}

bool UGuDefinitionRegistrySubsystem::ValidateAndNormalize(FGuDefinitionRecord& Definition, FString& OutError)
{
    Definition.Name.TrimStartAndEndInline();
    if (Definition.Id.IsNone())
    {
        OutError = TEXT("A Gu definition requires a stable ID.");
        return false;
    }
    if (Definition.Name.IsEmpty())
    {
        OutError = TEXT("A Gu definition requires a name.");
        return false;
    }
    if (Definition.Mechanics.IsEmpty())
    {
        FGuMechanicSpec SemanticOnly;
        SemanticOnly.Type = TEXT("semantic_only");
        Definition.Mechanics.Add(SemanticOnly);
    }

    Definition.Rank = FMath::Clamp(Definition.Rank, 1, 9);
    Definition.Kind = Definition.Rank >= 6 ? EGuKind::Immortal : Definition.Kind;
    Definition.Path = UGuRulesLibrary::NormalizePath(Definition.Path);
    for (FName& Secondary : Definition.SecondaryPaths) Secondary = UGuRulesLibrary::NormalizePath(Secondary);
    Definition.SecondaryPaths.RemoveAll([](const FName Path){ return Path.IsNone(); });
    Definition.Lifecycle.Charges = FMath::Max(1, Definition.Lifecycle.Charges);
    Definition.Feeding.IntervalHours = FMath::Max(0.01f, Definition.Feeding.IntervalHours);
    Definition.RefinementAssistance.ActionUses = FMath::Max(1, Definition.RefinementAssistance.ActionUses);

    const bool bHasCanonicalRefinementProfile = Definition.RefinementProfile.DaoMass > 0.0f
        || !Definition.RefinementProfile.Paths.IsEmpty()
        || !Definition.RefinementProfile.Properties.IsEmpty()
        || !Definition.RefinementProfile.Attributes.IsEmpty()
        || !Definition.RefinementProfile.Traits.IsEmpty()
        || !Definition.RefinementProfile.Templates.IsEmpty();

    if (!bHasCanonicalRefinementProfile)
    {
        Definition.RefinementProfile = UGuRulesLibrary::BuildDefaultGuRefinementProfile(Definition);
    }
    else
    {
        UGuRulesLibrary::NormalizeSemanticProfile(Definition.RefinementProfile);
        if (Definition.RefinementProfile.Properties.IsEmpty())
        {
            UGuRulesLibrary::MaterializeDerivedPropertySnapshot(Definition.RefinementProfile);
        }
    }

    OutError.Reset();
    return true;
}

bool UGuDefinitionRegistrySubsystem::RegisterDefinition(
    const FGuDefinitionRecord& Definition,
    FString& OutError,
    const bool bReplaceExisting)
{
    FGuDefinitionRecord Clean = Definition;
    if (!ValidateAndNormalize(Clean, OutError)) return false;

    const FString NewNameKey = NameKey(Clean.Name);
    FString OldFingerprint;
    if (const FGuDefinitionRecord* ExistingById = DefinitionsById.Find(Clean.Id))
    {
        if (!bReplaceExisting)
        {
            OutError = FString::Printf(TEXT("Gu definition '%s' already exists."), *Clean.Id.ToString());
            return false;
        }
        IdByName.Remove(NameKey(ExistingById->Name));
        if (ExistingById->bCustom)
        {
            OldFingerprint = ComputeRuntimeSpeciesFingerprint(*ExistingById);
        }
    }

    if (const FName* ExistingNameId = IdByName.Find(NewNameKey))
    {
        if (*ExistingNameId != Clean.Id && !bReplaceExisting)
        {
            OutError = FString::Printf(TEXT("A Gu named '%s' already exists."), *Clean.Name);
            return false;
        }
        if (*ExistingNameId != Clean.Id)
        {
            const FName ReplacedId = *ExistingNameId;
            RetargetDefinitionIdentity(ReplacedId, Clean.Id);
            DefinitionsById.Remove(ReplacedId);
        }
    }

    FString NewFingerprint;
    if (Clean.bCustom)
    {
        NewFingerprint = ComputeRuntimeSpeciesFingerprint(Clean);
        if (const FName* ExistingFingerprintId = RuntimeIdByFingerprint.Find(NewFingerprint))
        {
            if (*ExistingFingerprintId != Clean.Id)
            {
                OutError = FString::Printf(
                    TEXT("Runtime Gu '%s' duplicates canonical species '%s'. Reuse the existing species instead of registering another definition."),
                    *Clean.Id.ToString(),
                    *ExistingFingerprintId->ToString());
                return false;
            }
        }
    }

    if (!OldFingerprint.IsEmpty() && OldFingerprint != NewFingerprint)
    {
        RuntimeIdByFingerprint.Remove(OldFingerprint);
    }

    DefinitionsById.Add(Clean.Id, Clean);
    IdByName.Add(NewNameKey, Clean.Id);
    if (Clean.bCustom)
    {
        RuntimeDefinitionIds.Add(Clean.Id);
        RuntimeIdByFingerprint.Add(NewFingerprint, Clean.Id);
    }
    else
    {
        RuntimeDefinitionIds.Remove(Clean.Id);
        for (auto It = RuntimeIdByFingerprint.CreateIterator(); It; ++It)
        {
            if (It.Value() == Clean.Id) It.RemoveCurrent();
        }
    }
    OutError.Reset();
    return true;
}

bool UGuDefinitionRegistrySubsystem::RegisterDefinitionAsset(
    const UGuDefinition* Asset,
    FString& OutError,
    const bool bReplaceExisting)
{
    FGuDefinitionRecord Record;
    if (!BuildRecordFromAsset(Asset, Record, OutError)) return false;

    const FName ObjectId = Asset->GetFName();
    bool bBindExistingCanonicalWithoutReplacing = false;

    // An implicit object-name ID is a compatibility alias, not a reason to create
    // a second species with the same authored display name. If that name already
    // has a canonical species, bind this executable DataAsset to that identity.
    if (Asset->StableDefinitionId.IsNone())
    {
        const FString DisplayKey = NameKey(Record.Name);
        if (const FName* ExistingNameId = IdByName.Find(DisplayKey))
        {
            if (*ExistingNameId != Record.Id)
            {
                Record.Id = *ExistingNameId;
                bBindExistingCanonicalWithoutReplacing = !bReplaceExisting;
            }
        }
    }

    if (!bBindExistingCanonicalWithoutReplacing)
    {
        if (!RegisterDefinition(Record, OutError, bReplaceExisting)) return false;
    }
    else if (!DefinitionsById.Contains(Record.Id))
    {
        OutError = TEXT("The canonical Gu definition disappeared while binding its authored DataAsset.");
        return false;
    }

    // Freeze the resolved runtime identity on the UObject so all later bridges
    // (character grant, GAS, spawners) ask for the same canonical ID. This does
    // not save/dirty the asset package by itself.
    UGuDefinition* MutableAsset = const_cast<UGuDefinition*>(Asset);
    MutableAsset->StableDefinitionId = Record.Id;

    DefinitionAliases.Add(ObjectId, Record.Id);

    RuntimeAssetsById.Remove(Record.Id);
    AuthoredAssetsById.Add(Record.Id, MutableAsset);
    OutError.Reset();
    return true;
}

bool UGuDefinitionRegistrySubsystem::RegisterRuntimeDefinitionAsset(
    const FGuDefinitionRecord& Definition,
    UGuDefinition* RuntimeAsset,
    FString& OutError,
    const bool bReplaceExisting)
{
    if (!IsValid(RuntimeAsset))
    {
        OutError = TEXT("Runtime Gu definition asset is null.");
        return false;
    }

    FGuDefinitionRecord RuntimeRecord = Definition;
    RuntimeRecord.bCustom = true;
    if (RuntimeRecord.Id.IsNone()) RuntimeRecord.Id = DefinitionIdForAsset(RuntimeAsset);
    RuntimeAsset->StableDefinitionId = RuntimeRecord.Id;

    if (!RegisterDefinition(RuntimeRecord, OutError, bReplaceExisting)) return false;

    AuthoredAssetsById.Remove(RuntimeRecord.Id);
    RuntimeAssetsById.Add(RuntimeRecord.Id, RuntimeAsset);
    DefinitionAliases.Add(RuntimeAsset->GetFName(), RuntimeRecord.Id);
    OutError.Reset();
    return true;
}

const FGuDefinitionRecord* UGuDefinitionRegistrySubsystem::FindDefinition(const FName IdOrName) const
{
    const FName ResolvedId = ResolveDefinitionAlias(IdOrName);
    if (const FGuDefinitionRecord* ById = DefinitionsById.Find(ResolvedId)) return ById;

    const FString Key = NameKey(IdOrName.ToString());
    if (const FName* Id = IdByName.Find(Key))
    {
        return DefinitionsById.Find(ResolveDefinitionAlias(*Id));
    }
    return nullptr;
}

const UGuDefinition* UGuDefinitionRegistrySubsystem::FindDefinitionAsset(const FName IdOrName) const
{
    const FGuDefinitionRecord* Definition = FindDefinition(IdOrName);
    if (!Definition) return nullptr;

    if (const TObjectPtr<UGuDefinition>* Asset = AuthoredAssetsById.Find(Definition->Id))
    {
        return Asset->Get();
    }
    if (const TObjectPtr<UGuDefinition>* RuntimeAsset = RuntimeAssetsById.Find(Definition->Id))
    {
        return RuntimeAsset->Get();
    }

    return nullptr;
}

bool UGuDefinitionRegistrySubsystem::HasDefinition(const FName IdOrName) const
{
    return FindDefinition(IdOrName) != nullptr;
}

bool UGuDefinitionRegistrySubsystem::GetDefinition(const FName IdOrName, FGuDefinitionRecord& OutDefinition) const
{
    if (const FGuDefinitionRecord* Found = FindDefinition(IdOrName))
    {
        OutDefinition = *Found;
        return true;
    }
    return false;
}

TArray<FGuDefinitionRecord> UGuDefinitionRegistrySubsystem::GetAllDefinitions() const
{
    TArray<FGuDefinitionRecord> Out;
    DefinitionsById.GenerateValueArray(Out);
    Out.Sort([](const FGuDefinitionRecord& A, const FGuDefinitionRecord& B)
    {
        return A.Id.ToString() < B.Id.ToString();
    });
    return Out;
}

FString UGuDefinitionRegistrySubsystem::ComputeRuntimeSpeciesFingerprint(const FGuDefinitionRecord& Definition)
{
    FString Canonical;
    Canonical.Reserve(2048);
    Canonical += FString::Printf(
        TEXT("rank=%d|kind=%d|path=%s|activation=%d|costmode=%d|cost=%.6f|unique=%d|"),
        Definition.Rank,
        static_cast<int32>(Definition.Kind),
        *Definition.Path.ToString().ToLower(),
        static_cast<int32>(Definition.ActivationModel),
        static_cast<int32>(Definition.EssenceCostMode),
        Definition.EssenceCost,
        Definition.bUnique ? 1 : 0);

    AppendCanonicalNameArray(Canonical, TEXT("secondary"), Definition.SecondaryPaths);
    AppendCanonicalNameArray(Canonical, TEXT("refinementTraits"), Definition.RefinementTraits);

    Canonical += FString::Printf(
        TEXT("feeding=%s:%.6f|lifecycle=%d:%d:%d:%s|constraints=%d:%d:%d:%.6f:%.6f:%d|"),
        *Definition.Feeding.FoodKey.ToString().ToLower(),
        Definition.Feeding.IntervalHours,
        Definition.Lifecycle.bConsumable ? 1 : 0,
        static_cast<int32>(Definition.Lifecycle.ConsumeOn),
        Definition.Lifecycle.Charges,
        *Definition.Lifecycle.ConsumedForm,
        Definition.IntrinsicConstraints.PrepareMs,
        Definition.IntrinsicConstraints.bStationary ? 1 : 0,
        Definition.IntrinsicConstraints.bContact ? 1 : 0,
        Definition.IntrinsicConstraints.ContactRange,
        Definition.IntrinsicConstraints.SelfCostLifePercent,
        Definition.IntrinsicConstraints.bShortLived ? 1 : 0);

    TArray<FString> Mechanics;
    Mechanics.Reserve(Definition.Mechanics.Num());
    for (const FGuMechanicSpec& Mechanic : Definition.Mechanics)
    {
        FString Config = Mechanic.ConfigJson;
        Config.ReplaceInline(TEXT(" "), TEXT(""));
        Config.ReplaceInline(TEXT("\t"), TEXT(""));
        Config.ReplaceInline(TEXT("\r"), TEXT(""));
        Config.ReplaceInline(TEXT("\n"), TEXT(""));
        Mechanics.Add(Mechanic.Type.ToString().ToLower() + TEXT(":") + Config.ToLower());
    }
    Mechanics.Sort();
    Canonical += TEXT("mechanics=") + FString::Join(Mechanics, TEXT(";")) + TEXT("|");

    AppendCanonicalScoreMap(Canonical, TEXT("paths"), Definition.RefinementProfile.Paths);
    AppendCanonicalScoreMap(Canonical, TEXT("properties"), Definition.RefinementProfile.Properties);
    AppendCanonicalScoreMap(Canonical, TEXT("attributes"), Definition.RefinementProfile.Attributes);
    AppendCanonicalScoreMap(Canonical, TEXT("traits"), Definition.RefinementProfile.Traits);
    AppendCanonicalScoreMap(Canonical, TEXT("templates"), Definition.RefinementProfile.Templates);
    Canonical += FString::Printf(TEXT("daomass=%.6f|"), Definition.RefinementProfile.DaoMass);

    Canonical += FString::Printf(
        TEXT("assist=%d:%.6f:%.6f:%.6f:%.6f:%d|"),
        Definition.RefinementAssistance.bEnabled ? 1 : 0,
        Definition.RefinementAssistance.ProgressPercent,
        Definition.RefinementAssistance.StabilityPerAction,
        Definition.RefinementAssistance.ImpurityReductionPerAction,
        Definition.RefinementAssistance.QualityBonus,
        Definition.RefinementAssistance.ActionUses);
    AppendCanonicalNameArray(Canonical, TEXT("assistProcesses"), Definition.RefinementAssistance.Processes);

    const uint64 Hash = HashCanonicalSpeciesString(Canonical);
    return LexToString(Hash);
}

bool UGuDefinitionRegistrySubsystem::FindEquivalentRuntimeDefinition(
    const FGuDefinitionRecord& Definition,
    FName& OutDefinitionId) const
{
    OutDefinitionId = NAME_None;
    const FString Fingerprint = ComputeRuntimeSpeciesFingerprint(Definition);
    if (const FName* Found = RuntimeIdByFingerprint.Find(Fingerprint))
    {
        if (DefinitionsById.Contains(*Found))
        {
            OutDefinitionId = *Found;
            return true;
        }
    }
    return false;
}

TArray<FGuDefinitionRecord> UGuDefinitionRegistrySubsystem::GetRuntimeDefinitions() const
{
    TArray<FGuDefinitionRecord> Out;
    Out.Reserve(RuntimeDefinitionIds.Num());
    for (const FName Id : RuntimeDefinitionIds)
    {
        if (const FGuDefinitionRecord* Found = DefinitionsById.Find(Id)) Out.Add(*Found);
    }
    Out.Sort([](const FGuDefinitionRecord& A, const FGuDefinitionRecord& B)
    {
        return A.Id.ToString() < B.Id.ToString();
    });
    return Out;
}

void UGuDefinitionRegistrySubsystem::ClearRuntimeDefinitions()
{
    for (const FName Id : RuntimeDefinitionIds)
    {
        if (const FGuDefinitionRecord* Found = DefinitionsById.Find(Id))
        {
            IdByName.Remove(NameKey(Found->Name));
        }
        DefinitionsById.Remove(Id);
        RuntimeAssetsById.Remove(Id);
    }
    RuntimeDefinitionIds.Reset();
    RuntimeIdByFingerprint.Reset();

    for (auto It = DefinitionAliases.CreateIterator(); It; ++It)
    {
        if (!DefinitionsById.Contains(It.Value())) It.RemoveCurrent();
    }
}
