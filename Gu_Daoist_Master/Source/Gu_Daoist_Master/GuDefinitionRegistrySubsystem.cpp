#include "GuDefinitionRegistrySubsystem.h"

#include "UGuDefinition.h"
#include "GuRulesLibrary.h"

namespace
{
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
}

FString UGuDefinitionRegistrySubsystem::NameKey(const FString& Name)
{
    return Name.TrimStartAndEnd().ToLower();
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
    bool bHasDamage = false;
    bool bHasKnockback = false;
    bool bHasBuff = false;
    float MaxDamage = 0.0f;
    float MaxKnockback = 0.0f;
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
    else if (bHasDamage || bHasKnockback)
    {
        Record.KillerMove.Template = SemanticArea >= 0.55f ? TEXT("area") : TEXT("melee");
        Record.EffectProfile.Manifestation = Record.KillerMove.Template.ToString();
        Record.KillerMove.SuitableRoles.AddUnique(TEXT("core"));
        Record.KillerMove.SuitableRoles.AddUnique(TEXT("output"));
    }
    else if (bHasBuff)
    {
        Record.KillerMove.Template = TEXT("self");
        Record.EffectProfile.Manifestation = TEXT("self");
        Record.KillerMove.SuitableRoles.AddUnique(TEXT("stabilization"));
        Record.KillerMove.SuitableRoles.AddUnique(TEXT("amplification"));
        Record.KillerMove.SuitableRoles.AddUnique(TEXT("recovery"));
    }

    if (bHasDamage || bHasKnockback)
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
    if (const FGuDefinitionRecord* ExistingById = DefinitionsById.Find(Clean.Id))
    {
        if (!bReplaceExisting)
        {
            OutError = FString::Printf(TEXT("Gu definition '%s' already exists."), *Clean.Id.ToString());
            return false;
        }
        IdByName.Remove(NameKey(ExistingById->Name));
    }

    if (const FName* ExistingNameId = IdByName.Find(NewNameKey))
    {
        if (*ExistingNameId != Clean.Id && !bReplaceExisting)
        {
            OutError = FString::Printf(TEXT("A Gu named '%s' already exists."), *Clean.Name);
            return false;
        }
        if (*ExistingNameId != Clean.Id) DefinitionsById.Remove(*ExistingNameId);
    }

    DefinitionsById.Add(Clean.Id, Clean);
    IdByName.Add(NewNameKey, Clean.Id);
    if (Clean.bCustom) RuntimeDefinitionIds.Add(Clean.Id);
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
    return RegisterDefinition(Record, OutError, bReplaceExisting);
}

const FGuDefinitionRecord* UGuDefinitionRegistrySubsystem::FindDefinition(const FName IdOrName) const
{
    if (const FGuDefinitionRecord* ById = DefinitionsById.Find(IdOrName)) return ById;
    const FString Key = NameKey(IdOrName.ToString());
    if (const FName* Id = IdByName.Find(Key)) return DefinitionsById.Find(*Id);
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
    }
    RuntimeDefinitionIds.Reset();
}
