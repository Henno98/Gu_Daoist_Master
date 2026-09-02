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

    FString EscapeJsonString(FString Value)
    {
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
            AddMechanic(
                Record,
                TEXT("projectile"),
                FString::Printf(
                    TEXT("{\"speed\":%.6f,\"range\":%.6f,\"gravityScale\":%.6f}"),
                    Projectile->Speed,
                    Projectile->MaxRange,
                    Projectile->GravityScale));
            continue;
        }

        if (const FGuDamageMechanic* Damage = Mechanic.GetPtr<FGuDamageMechanic>())
        {
            AddMechanic(Record, TEXT("damage"), FString::Printf(TEXT("{\"damage\":%.6f}"), Damage->Damage));
            continue;
        }

        if (const FGuKnockbackMechanic* Knockback = Mechanic.GetPtr<FGuKnockbackMechanic>())
        {
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
            const FString AttributeName = Buff->Attribute.IsValid() ? Buff->Attribute.GetName() : TEXT("unknown");
            AddMechanic(
                Record,
                TEXT("stat_modifier"),
                FString::Printf(
                    TEXT("{\"stat\":\"%s\",\"magnitude\":%.6f,\"duration\":%.6f}"),
                    *EscapeJsonString(AttributeName),
                    Buff->Magnitude,
                    Buff->Duration));
            continue;
        }

        const UScriptStruct* StructType = Mechanic.GetScriptStruct();
        AddMechanic(
            Record,
            TEXT("semantic_only"),
            FString::Printf(TEXT("{\"sourceStruct\":\"%s\"}"), *EscapeJsonString(GetNameSafe(StructType))));
    }

    // A Gu is still a refinable physical Gu even while an activation mechanic is
    // not implemented yet. Do not make ECS/refinement registration contingent on GAS support.
    if (Record.Mechanics.IsEmpty()) AddMechanic(Record, TEXT("semantic_only"));

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
