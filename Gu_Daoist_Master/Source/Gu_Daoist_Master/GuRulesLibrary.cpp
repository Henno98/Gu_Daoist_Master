#include "GuRulesLibrary.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

float UGuRulesLibrary::GameplayEssenceQualityRelativeToRank1(const int32 Rank)
{
    return FMath::Pow(10.0f, static_cast<float>(FMath::Max(0, Rank - 1)));
}

float UGuRulesLibrary::TheoreticalEssenceCap(const int32 Rank)
{
    return FMath::RoundToFloat(BaseEssenceCap * GameplayEssenceQualityRelativeToRank1(Rank));
}

float UGuRulesLibrary::PersonalEssenceCap(const int32 Rank, const float GradeFillPercent)
{
    return TheoreticalEssenceCap(Rank) * FMath::Clamp(GradeFillPercent, 0.0f, 100.0f) / 100.0f;
}

float UGuRulesLibrary::GuAperturePressure(const int32 GuRank, const int32 HolderRank)
{
    const int32 Gap = FMath::Clamp(GuRank, 1, 9) - FMath::Max(1, HolderRank);
    const float Pressure = Gap > 0
        ? AperturePressureSameRank * FMath::Pow(AperturePressureHigherRankMultiplier, static_cast<float>(Gap))
        : AperturePressureSameRank / FMath::Pow(AperturePressureLowerRankDivisor, static_cast<float>(-Gap));
    return FMath::RoundToFloat(Pressure * 10000.0f) / 10000.0f;
}

int32 UGuRulesLibrary::MentalFoundationCap(const int32 Rank)
{
    const int32 R = FMath::Max(1, Rank);
    if (R <= 5) return 20;
    switch (R)
    {
        case 6: return 60;
        case 7: return 120;
        case 8: return 240;
        case 9: return 480;
        default: return FMath::RoundToInt(480.0f * FMath::Pow(2.0f, static_cast<float>(R - 9)));
    }
}

int32 UGuRulesLibrary::FocusBranchCap(const int32 MentalFoundation, const int32 Rank)
{
    return FMath::Max(1, FMath::Min(MentalFoundationCap(Rank), MentalFoundation + 2));
}

int32 UGuRulesLibrary::MentalFocusCapacity(const int32 MentalFoundation, const int32 FocusControlLevel)
{
    return FMath::RoundToInt(80.0f + FMath::Max(1, MentalFoundation) * 8.0f + FMath::Max(1, FocusControlLevel) * 12.0f);
}

int32 UGuRulesLibrary::MultitaskingNaturalCap(const int32 Rank)
{
    const int32 R = FMath::Max(1, Rank);
    if (R <= 5) return 15;
    switch (R)
    {
        case 6: return 100;
        case 7: return 200;
        case 8: return 400;
        case 9: return 800;
        default: return FMath::RoundToInt(800.0f * FMath::Pow(2.0f, static_cast<float>(R - 9)));
    }
}

float UGuRulesLibrary::RefinementDaoMassRequiredForRank(const int32 Rank)
{
    const float R = static_cast<float>(FMath::Clamp(Rank, 1, 9));
    return R * R * 0.40f;
}

int32 UGuRulesLibrary::ExperimentalFormationRankFromRetainedDaoMass(const float RetainedDaoMass)
{
    int32 Rank = 1;
    for (int32 Candidate = 2; Candidate <= 9; ++Candidate)
    {
        if (RetainedDaoMass + KINDA_SMALL_NUMBER < RefinementDaoMassRequiredForRank(Candidate)) break;
        Rank = Candidate;
    }
    return Rank;
}

namespace
{
void AddSemanticScore(TMap<FName, float>& Target, const TCHAR* Key, const float Value = 1.0f)
{
    if (Value <= 0.0f) return;
    Target.FindOrAdd(FName(Key)) += Value;
}

TSharedPtr<FJsonObject> ParseMechanicConfig(const FString& Json)
{
    TSharedPtr<FJsonObject> Object;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json.IsEmpty() ? TEXT("{}") : Json);
    FJsonSerializer::Deserialize(Reader, Object);
    return Object;
}

FString JsonString(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field)
{
    FString Value;
    if (Object.IsValid()) Object->TryGetStringField(Field, Value);
    return Value;
}

bool JsonBool(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field)
{
    bool Value = false;
    return Object.IsValid() && Object->TryGetBoolField(Field, Value) && Value;
}

float JsonNumber(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field)
{
    double Value = 0.0;
    return Object.IsValid() && Object->TryGetNumberField(Field, Value) ? static_cast<float>(Value) : 0.0f;
}

void ApplyTemplateSemantics(FRefinementSemanticProfile& Profile, const FString& Template, const bool bMaintained)
{
    const FString T = Template.ToLower();
    if (T.IsEmpty() || T == TEXT("unsupported") || T == TEXT("auto")) return;
    Profile.Templates.FindOrAdd(FName(*T)) += 1.0f;

    if (T == TEXT("projectile"))
    {
        AddSemanticScore(Profile.Attributes, TEXT("range"), .55f);
        AddSemanticScore(Profile.Attributes, TEXT("speed"), .4f);
        AddSemanticScore(Profile.Attributes, TEXT("precision"), .3f);
        AddSemanticScore(Profile.Attributes, TEXT("amplification"), .45f);
    }
    else if (T == TEXT("melee"))
    {
        AddSemanticScore(Profile.Attributes, TEXT("penetration"), .5f);
        AddSemanticScore(Profile.Attributes, TEXT("amplification"), .45f);
        AddSemanticScore(Profile.Traits, TEXT("contact"), .7f);
    }
    else if (T == TEXT("area"))
    {
        AddSemanticScore(Profile.Attributes, TEXT("area"), .9f);
        AddSemanticScore(Profile.Attributes, TEXT("amplification"), .4f);
    }
    else if (T == TEXT("shield"))
    {
        AddSemanticScore(Profile.Attributes, TEXT("stability"), .85f);
        AddSemanticScore(Profile.Attributes, TEXT("persistence"), .55f);
    }
    else if (T == TEXT("heal")) AddSemanticScore(Profile.Attributes, TEXT("recovery"), .9f);
    else if (T == TEXT("movement"))
    {
        AddSemanticScore(Profile.Attributes, TEXT("speed"), .85f);
        AddSemanticScore(Profile.Attributes, TEXT("duration"), .4f);
    }
    else if (T == TEXT("conceal"))
    {
        AddSemanticScore(Profile.Attributes, TEXT("concealment"), .9f);
        AddSemanticScore(Profile.Attributes, TEXT("persistence"), .35f);
    }
    else if (T == TEXT("reveal"))
    {
        AddSemanticScore(Profile.Attributes, TEXT("precision"), .7f);
        AddSemanticScore(Profile.Attributes, TEXT("range"), .5f);
        AddSemanticScore(Profile.Attributes, TEXT("tracking"), .35f);
    }
    else if (T == TEXT("amplifier") || T == TEXT("buff")) AddSemanticScore(Profile.Attributes, TEXT("amplification"), .75f);
    else if (T == TEXT("restriction"))
    {
        AddSemanticScore(Profile.Attributes, TEXT("suppression"), .8f);
        AddSemanticScore(Profile.Attributes, TEXT("persistence"), .4f);
    }

    if (bMaintained)
    {
        AddSemanticScore(Profile.Attributes, TEXT("duration"), .35f);
        AddSemanticScore(Profile.Attributes, TEXT("persistence"), .35f);
        AddSemanticScore(Profile.Traits, TEXT("maintained"), .8f);
    }
}

FString TemplateForMechanic(const FString& Type, const TSharedPtr<FJsonObject>& Config)
{
    if (Type.Equals(TEXT("legacy_cast"), ESearchCase::IgnoreCase))
    {
        const FString Handler = JsonString(Config, TEXT("handlerId")).ToLower();
        if (Handler == TEXT("icicle") || Handler == TEXT("pine_needle")) return TEXT("projectile");
        if (Handler == TEXT("moon_raiment") || Handler == TEXT("water_shield")) return TEXT("shield");
        if (Handler == TEXT("moon_poison")) return TEXT("area");
        if (Handler == TEXT("whirlwind")) return TEXT("movement");
        if (Handler == TEXT("blood_essence")) return TEXT("heal");
        if (Handler == TEXT("moonshadow") || Handler == TEXT("blue_bird")) return TEXT("restriction");
        if (Handler == TEXT("grand_bear") || Handler == TEXT("wood_charm") || Handler == TEXT("frost_demon") || Handler == TEXT("plunder")) return TEXT("amplifier");
        if (Handler == TEXT("ice_blade") || Handler == TEXT("green_vine")) return TEXT("melee");
        if (Handler == TEXT("stealth_rock") || Handler == TEXT("stealth_scales")) return TEXT("conceal");
        if (Handler == TEXT("snake_communication")) return TEXT("reveal");
        return FString();
    }

    const FString T = Type.ToLower();
    if (T == TEXT("fire") || T == TEXT("custom_projectile") || T == TEXT("projectile")) return TEXT("projectile");
    if (T == TEXT("knockback")) return TEXT("movement");
    if (T == TEXT("melee") || T == TEXT("custom_melee")) return TEXT("melee");
    if (T == TEXT("aoe") || T == TEXT("custom_area") || T == TEXT("area")) return TEXT("area");
    if (T == TEXT("shield") || T == TEXT("custom_shield")) return TEXT("shield");
    if (T == TEXT("heal") || T == TEXT("custom_heal")) return TEXT("heal");
    if (T == TEXT("speed") || T == TEXT("blink") || T == TEXT("custom_movement") || T == TEXT("movement")) return TEXT("movement");
    if (T == TEXT("conceal") || T == TEXT("custom_concealment")) return TEXT("conceal");
    if (T == TEXT("reveal") || T == TEXT("custom_reveal")) return TEXT("reveal");
    if (T == TEXT("amp") || T == TEXT("essence_regeneration") || T == TEXT("cultivation_channel")) return TEXT("amplifier");
    if (T == TEXT("stat_modifier")) return TEXT("buff");
    if (T == TEXT("refinement_assistance")) return TEXT("refinement");
    return FString();
}


const TMap<FName, TMap<FName, float>>& SemanticPathProperties()
{
    static const TMap<FName, TMap<FName, float>> Data = {
        {TEXT("Fire"), {{TEXT("heat"),1.35f},{TEXT("motion"),.35f},{TEXT("expansion"),.3f}}},
        {TEXT("Ice"), {{TEXT("cold"),1.35f},{TEXT("solid"),.7f},{TEXT("stillness"),.45f}}},
        {TEXT("Water"), {{TEXT("fluid"),.95f},{TEXT("flow"),1.2f},{TEXT("cold"),.2f}}},
        {TEXT("Wind"), {{TEXT("vapor"),.9f},{TEXT("motion"),1.25f},{TEXT("flow"),.55f}}},
        {TEXT("Earth"), {{TEXT("solid"),1.0f},{TEXT("hardness"),1.05f},{TEXT("stability"),.75f}}},
        {TEXT("Wood"), {{TEXT("vitality"),1.05f},{TEXT("growth"),1.15f},{TEXT("adhesion"),.35f}}},
        {TEXT("Strength"), {{TEXT("force"),1.2f},{TEXT("hardness"),.7f}}},
        {TEXT("Light"), {{TEXT("luminosity"),1.2f},{TEXT("precision"),.65f},{TEXT("heat"),.15f}}},
        {TEXT("Moon"), {{TEXT("luminosity"),.75f},{TEXT("cold"),.35f},{TEXT("precision"),.35f}}},
        {TEXT("Blood"), {{TEXT("blood"),1.25f},{TEXT("vitality"),.6f},{TEXT("adhesion"),.25f}}},
        {TEXT("Food"), {{TEXT("assimilation"),1.2f},{TEXT("vitality"),.35f}}},
        {TEXT("Transformation"), {{TEXT("adaptability"),1.2f},{TEXT("adhesion"),.55f}}},
        {TEXT("Refinement"), {{TEXT("precision"),1.0f},{TEXT("stability"),1.0f},{TEXT("adhesion"),.55f}}},
        {TEXT("Enslavement"), {{TEXT("control"),1.2f},{TEXT("link"),.8f}}},
        {TEXT("Soul"), {{TEXT("persistence"),1.0f},{TEXT("control"),.55f}}},
        {TEXT("Poison"), {{TEXT("corrosion"),1.15f},{TEXT("persistence"),.4f}}},
        {TEXT("Metal"), {{TEXT("hardness"),1.0f},{TEXT("sharpness"),.85f},{TEXT("solid"),.55f}}},
        {TEXT("Dark"), {{TEXT("concealment"),1.2f},{TEXT("stillness"),.62f},{TEXT("corrosion"),.42f},{TEXT("persistence"),.28f}}},
        {TEXT("Shadow"), {{TEXT("concealment"),1.28f},{TEXT("adhesion"),.58f},{TEXT("stillness"),.45f},{TEXT("flow"),.3f}}},
        {TEXT("Qi"), {{TEXT("flow"),.85f},{TEXT("expansion"),.7f},{TEXT("motion"),.55f}}},
        {TEXT("Information"), {{TEXT("precision"),.85f},{TEXT("link"),.65f},{TEXT("persistence"),.35f}}},
        {TEXT("Time"), {{TEXT("persistence"),.8f},{TEXT("timing"),1.2f}}},
        {TEXT("Space"), {{TEXT("expansion"),1.0f},{TEXT("precision"),.45f}}},
        {TEXT("Luck"), {{TEXT("adaptability"),.7f},{TEXT("flow"),.55f}}},
    };
    return Data;
}

const TMap<FName, TMap<FName, float>>& SemanticAttributeProperties()
{
    static const TMap<FName, TMap<FName, float>> Data = {
        {TEXT("amplification"),{{TEXT("force"),.55f},{TEXT("expansion"),.4f}}},
        {TEXT("range"),{{TEXT("expansion"),.65f},{TEXT("flow"),.25f}}},
        {TEXT("area"),{{TEXT("expansion"),.8f}}},
        {TEXT("speed"),{{TEXT("motion"),.8f},{TEXT("flow"),.3f}}},
        {TEXT("duration"),{{TEXT("persistence"),.8f}}},
        {TEXT("precision"),{{TEXT("precision"),.85f}}},
        {TEXT("persistence"),{{TEXT("persistence"),.95f},{TEXT("stability"),.25f}}},
        {TEXT("tracking"),{{TEXT("control"),.55f},{TEXT("precision"),.4f}}},
        {TEXT("penetration"),{{TEXT("sharpness"),.7f},{TEXT("force"),.35f}}},
        {TEXT("stability"),{{TEXT("stability"),.9f},{TEXT("solid"),.2f}}},
        {TEXT("efficiency"),{{TEXT("assimilation"),.5f},{TEXT("precision"),.25f}}},
        {TEXT("concealment"),{{TEXT("concealment"),.9f},{TEXT("stillness"),.2f}}},
        {TEXT("suppression"),{{TEXT("control"),.75f},{TEXT("stillness"),.25f}}},
        {TEXT("bleed"),{{TEXT("blood"),.8f},{TEXT("flow"),.2f}}},
        {TEXT("poison"),{{TEXT("corrosion"),.85f}}},
        {TEXT("timed"),{{TEXT("timing"),.85f},{TEXT("persistence"),.25f}}},
        {TEXT("recovery"),{{TEXT("vitality"),.85f},{TEXT("growth"),.25f}}},
        {TEXT("link"),{{TEXT("link"),.85f},{TEXT("adhesion"),.25f}}},
    };
    return Data;
}

const TMap<FName, TMap<FName, float>>& SemanticTraitProperties()
{
    static const TMap<FName, TMap<FName, float>> Data = {
        {TEXT("consumable"),{{TEXT("force"),.22f},{TEXT("expansion"),.18f}}},
        {TEXT("charged"),{{TEXT("timing"),.62f},{TEXT("persistence"),.24f}}},
        {TEXT("stored"),{{TEXT("persistence"),.52f},{TEXT("stability"),.22f}}},
        {TEXT("contact"),{{TEXT("adhesion"),.72f},{TEXT("precision"),.22f}}},
        {TEXT("stationary"),{{TEXT("stillness"),.82f},{TEXT("stability"),.3f}}},
        {TEXT("grounded"),{{TEXT("solid"),.62f},{TEXT("stability"),.38f}}},
        {TEXT("delayed"),{{TEXT("timing"),.88f},{TEXT("persistence"),.22f}}},
        {TEXT("maintained"),{{TEXT("persistence"),.68f},{TEXT("control"),.28f}}},
        {TEXT("prepared"),{{TEXT("precision"),.42f},{TEXT("timing"),.4f}}},
        {TEXT("environment_bound"),{{TEXT("adaptability"),.18f},{TEXT("stability"),.2f}}},
        {TEXT("target_specific"),{{TEXT("precision"),.62f},{TEXT("link"),.35f}}},
        {TEXT("self_cost"),{{TEXT("force"),.38f},{TEXT("expansion"),.12f}}},
        {TEXT("short_lived"),{{TEXT("motion"),.32f},{TEXT("expansion"),.2f}}},
        {TEXT("trigger"),{{TEXT("timing"),.72f},{TEXT("control"),.35f}}},
        {TEXT("attached"),{{TEXT("adhesion"),.82f},{TEXT("link"),.42f}}},
    };
    return Data;
}

void AddPropertyRelation(TMap<FName, float>& Target, const TMap<FName, float>* Relation, const float Weight)
{
    if (!Relation || Weight <= 0.0f) return;
    for (const TPair<FName, float>& Pair : *Relation)
    {
        if (!Pair.Key.IsNone() && Pair.Value > 0.0f) Target.FindOrAdd(Pair.Key) += Pair.Value * Weight;
    }
}
}

FRefinementSemanticProfile UGuRulesLibrary::BuildDefaultGuRefinementProfile(const FGuDefinitionRecord& Definition)
{
    FRefinementSemanticProfile Profile;
    const int32 Rank = FMath::Clamp(Definition.Rank, 1, 9);

    const FName Primary = NormalizePath(Definition.Path);
    if (!Primary.IsNone()) Profile.Paths.Add(Primary, 1.0f);
    for (const FName SecondaryRaw : Definition.SecondaryPaths)
    {
        const FName Secondary = NormalizePath(SecondaryRaw);
        if (!Secondary.IsNone()) Profile.Paths.FindOrAdd(Secondary) += .38f;
    }

    for (const FName Attribute : Definition.KillerMove.Attributes)
    {
        const FString Clean = Attribute.ToString().TrimStartAndEnd().ToLower();
        if (!Clean.IsEmpty()) Profile.Attributes.FindOrAdd(FName(*Clean)) += 1.0f;
    }
    for (const FName Trait : Definition.RefinementTraits)
    {
        const FString Clean = Trait.ToString().TrimStartAndEnd().ToLower();
        if (!Clean.IsEmpty()) Profile.Traits.FindOrAdd(FName(*Clean)) += 1.0f;
    }

    if (Definition.Lifecycle.bConsumable) AddSemanticScore(Profile.Traits, TEXT("consumable"));
    switch (Definition.ActivationModel)
    {
        case EGuActivationModel::StoredCharged: AddSemanticScore(Profile.Traits, TEXT("charged")); break;
        case EGuActivationModel::Maintained: AddSemanticScore(Profile.Traits, TEXT("maintained")); break;
        case EGuActivationModel::Trigger: AddSemanticScore(Profile.Traits, TEXT("trigger")); break;
        case EGuActivationModel::PreparedMark: AddSemanticScore(Profile.Traits, TEXT("prepared")); break;
        default: break;
    }

    if (Definition.IntrinsicConstraints.PrepareMs > 0) AddSemanticScore(Profile.Traits, TEXT("prepared"));
    if (Definition.IntrinsicConstraints.bStationary) AddSemanticScore(Profile.Traits, TEXT("stationary"));
    if (Definition.IntrinsicConstraints.bContact) AddSemanticScore(Profile.Traits, TEXT("contact"));
    if (Definition.IntrinsicConstraints.SelfCostLifePercent > 0.0f) AddSemanticScore(Profile.Traits, TEXT("self_cost"));
    if (Definition.IntrinsicConstraints.bShortLived) AddSemanticScore(Profile.Traits, TEXT("short_lived"));
    if (!Definition.EffectProfile.Environment.TrimStartAndEnd().IsEmpty()) AddSemanticScore(Profile.Traits, TEXT("environment_bound"));

    for (const FGuMechanicSpec& Mechanic : Definition.Mechanics)
    {
        const FString Type = Mechanic.Type.ToString();
        const TSharedPtr<FJsonObject> Config = ParseMechanicConfig(Mechanic.ConfigJson);
        ApplyTemplateSemantics(Profile, TemplateForMechanic(Type, Config), JsonBool(Config, TEXT("maintained")));

        const FString LowerType = Type.ToLower();
        if (LowerType == TEXT("custom_projectile"))
        {
            if (JsonBool(Config, TEXT("homing"))) AddSemanticScore(Profile.Attributes, TEXT("tracking"), .8f);
            if (JsonBool(Config, TEXT("bleed"))) AddSemanticScore(Profile.Attributes, TEXT("bleed"), .8f);
            if (JsonBool(Config, TEXT("poison"))) AddSemanticScore(Profile.Attributes, TEXT("poison"), .8f);
            if (JsonBool(Config, TEXT("explodes")) || JsonNumber(Config, TEXT("explosionRadius")) > 0.0f) AddSemanticScore(Profile.Attributes, TEXT("area"), .7f);
        }
        else if (LowerType == TEXT("custom_melee"))
        {
            if (JsonBool(Config, TEXT("bleed"))) AddSemanticScore(Profile.Attributes, TEXT("bleed"), .8f);
            if (JsonBool(Config, TEXT("poison"))) AddSemanticScore(Profile.Attributes, TEXT("poison"), .8f);
        }
        else if (LowerType == TEXT("custom_area"))
        {
            if (JsonBool(Config, TEXT("poison"))) AddSemanticScore(Profile.Attributes, TEXT("poison"), .8f);
            if (JsonBool(Config, TEXT("slow"))) AddSemanticScore(Profile.Attributes, TEXT("suppression"), .6f);
        }
        else if (LowerType == TEXT("stat_modifier"))
        {
            const FString Stat = JsonString(Config, TEXT("stat")).ToLower();
            if (Stat == TEXT("damage")) AddSemanticScore(Profile.Attributes, TEXT("amplification"), .8f);
            else if (Stat == TEXT("movement")) AddSemanticScore(Profile.Attributes, TEXT("speed"), .8f);
            else if (Stat == TEXT("healing") || Stat == TEXT("essence_regeneration")) AddSemanticScore(Profile.Attributes, TEXT("recovery"), .7f);
            else if (Stat == TEXT("essence_efficiency")) AddSemanticScore(Profile.Attributes, TEXT("efficiency"), .8f);
            else if (Stat == TEXT("investigation")) AddSemanticScore(Profile.Attributes, TEXT("precision"), .65f);
        }
    }

    if (Profile.Templates.IsEmpty())
    {
        const FString ExplicitTemplate = Definition.KillerMove.Template.ToString().TrimStartAndEnd().ToLower();
        if (!ExplicitTemplate.IsEmpty() && ExplicitTemplate != TEXT("auto") && ExplicitTemplate != TEXT("unsupported"))
        {
            Profile.Templates.Add(FName(*ExplicitTemplate), 1.0f);
        }
        else if (!Definition.KillerMove.Attributes.IsEmpty())
        {
            Profile.Templates.Add(FName(TEXT("attribute")), 1.0f);
        }
    }

    Profile.DaoMass = static_cast<float>(Rank * Rank) * 1.15f;
    NormalizeSemanticProfile(Profile);
    MaterializeDerivedPropertySnapshot(Profile);
    return Profile;
}

void UGuRulesLibrary::MaterializeDerivedPropertySnapshot(FRefinementSemanticProfile& Profile)
{
    // Keep explicitly authored/generated physical properties authoritative.
    if (!Profile.Properties.IsEmpty() && !Profile.bDerivedPropertySnapshot) return;

    Profile.Properties.Reset();
    for (const TPair<FName, float>& Pair : Profile.Paths)
    {
        AddPropertyRelation(Profile.Properties, SemanticPathProperties().Find(NormalizePath(Pair.Key)), Pair.Value);
    }
    for (const TPair<FName, float>& Pair : Profile.Attributes)
    {
        AddPropertyRelation(Profile.Properties, SemanticAttributeProperties().Find(Pair.Key), Pair.Value);
    }
    for (const TPair<FName, float>& Pair : Profile.Traits)
    {
        AddPropertyRelation(Profile.Properties, SemanticTraitProperties().Find(Pair.Key), Pair.Value);
    }

    Profile.bDerivedPropertySnapshot = true;
    NormalizeScoreMap(Profile.Properties);
}

FRefinementSemanticProfile UGuRulesLibrary::BuildEffectiveGuRefinementProfile(const FGuDefinitionRecord& Definition)
{
    FRefinementSemanticProfile Effective = BuildDefaultGuRefinementProfile(Definition);
    const FRefinementSemanticProfile& Authored = Definition.RefinementProfile;

    auto AddMap = [](TMap<FName, float>& Target, const TMap<FName, float>& Source)
    {
        for (const TPair<FName, float>& Pair : Source)
        {
            if (!Pair.Key.IsNone() && FMath::IsFinite(Pair.Value) && Pair.Value > 0.0f)
            {
                Target.FindOrAdd(Pair.Key) += Pair.Value;
            }
        }
    };

    AddMap(Effective.Paths, Authored.Paths);
    AddMap(Effective.Attributes, Authored.Attributes);
    AddMap(Effective.Traits, Authored.Traits);
    AddMap(Effective.Templates, Authored.Templates);

    if (!Authored.Properties.IsEmpty())
    {
        // Explicit properties are real extra semantic evidence, exactly as in the
        // browser model. Do not mark them as a derived display snapshot.
        Effective.Properties = Authored.Properties;
        Effective.bDerivedPropertySnapshot = false;
    }
    else
    {
        Effective.Properties.Reset();
        Effective.bDerivedPropertySnapshot = true;
        MaterializeDerivedPropertySnapshot(Effective);
    }

    if (Authored.DaoMass > 0.0f) Effective.DaoMass = Authored.DaoMass;
    NormalizeSemanticProfile(Effective);
    return Effective;
}

void UGuRulesLibrary::NormalizeScoreMap(TMap<FName, float>& Scores)
{
    TArray<FName> Remove;
    for (const TPair<FName, float>& Pair : Scores)
    {
        if (Pair.Key.IsNone() || !FMath::IsFinite(Pair.Value) || Pair.Value <= 0.0f)
        {
            Remove.Add(Pair.Key);
        }
    }
    for (const FName Key : Remove) Scores.Remove(Key);
}

FName UGuRulesLibrary::NormalizePath(const FName Path)
{
    if (Path.IsNone()) return NAME_None;
    const FString Raw = Path.ToString().TrimStartAndEnd();
    if (Raw.IsEmpty()) return NAME_None;
    FString Clean = Raw;
    if (Clean.EndsWith(TEXT(" path"), ESearchCase::IgnoreCase))
    {
        Clean = Clean.LeftChop(5);
        Clean.TrimEndInline();
    }
    if (Clean.Equals(TEXT("unclassified"), ESearchCase::IgnoreCase)
        || Clean.Equals(TEXT("unknown"), ESearchCase::IgnoreCase)
        || Clean.Equals(TEXT("none"), ESearchCase::IgnoreCase)
        || Clean.Equals(TEXT("undefined"), ESearchCase::IgnoreCase)
        || Clean.Equals(TEXT("null"), ESearchCase::IgnoreCase))
    {
        return NAME_None;
    }
    return FName(*Clean);
}

void UGuRulesLibrary::NormalizeSemanticProfile(FRefinementSemanticProfile& Profile)
{
    NormalizeScoreMap(Profile.Paths);
    NormalizeScoreMap(Profile.Properties);
    NormalizeScoreMap(Profile.Attributes);
    NormalizeScoreMap(Profile.Traits);
    NormalizeScoreMap(Profile.Templates);

    TMap<FName, float> CleanPaths;
    for (const TPair<FName, float>& Pair : Profile.Paths)
    {
        const FName Normalized = NormalizePath(Pair.Key);
        if (!Normalized.IsNone()) CleanPaths.FindOrAdd(Normalized) += Pair.Value;
    }
    Profile.Paths = MoveTemp(CleanPaths);
    Profile.DaoMass = FMath::Max(0.0f, FMath::IsFinite(Profile.DaoMass) ? Profile.DaoMass : 0.0f);
}
