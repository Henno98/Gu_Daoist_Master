#include "RefinementSubsystem.h"
#include "GuDefinitionRegistrySubsystem.h"
#include "GuEntitySubsystem.h"
#include "GuRulesLibrary.h"
#include "GuPlayerState.h"
#include "GuProceduralGeneratorSubsystem.h"
#include "Gu_Daoist_MasterCharacter.h"
#include "MentalResourceComponent.h"
#include "Math/RandomStream.h"
#include "Misc/Crc.h"

namespace RefinementBalance
{
    constexpr float DaoMassPerRankSquared = 0.40f;
    constexpr float ContaminationPathInfluence = 0.34f;
    constexpr float ContaminationSemanticInfluence = 0.42f;
    constexpr float DerivedTraitInfluence = 0.24f;
    constexpr float ContaminationAttributeScale = 0.70f;
    constexpr float ContaminationTraitScale = 0.65f;
    constexpr float ContaminationPathTraitScale = 0.80f;
    constexpr float RetentionPathBase = 0.68f;
    constexpr float RetentionAttributeWeight = 0.16f;
    constexpr float RetentionPropertyWeight = 0.10f;
    constexpr float RetentionFidelityWeight = 0.06f;
    constexpr float ImpurityLossDivisor = 180.0f;
    constexpr float MaxImpurityLoss = 0.28f;
    constexpr float StabilityLossWeight = 0.16f;
    constexpr float MaxStabilityLoss = 0.16f;
    constexpr float MinimumFormationCoherence = 0.36f;
    constexpr float FormationCoherencePerRank = 0.018f;
    constexpr float MaximumFormationCoherence = 0.50f;
    constexpr float DivergentCoherenceOffset = 0.09f;
    constexpr float MinimumDivergentCoherence = 0.47f;
    constexpr float MaximumDivergentCoherence = 0.60f;
    constexpr float MinimumPropertyAlignment = 0.18f;
    constexpr float DivergentPropertyAlignment = 0.28f;
    constexpr float IntendedPathAlignment = 0.70f;
    constexpr float MinimumFidelity = 0.34f;
    constexpr float ContaminationToleranceBase = 1.05f;
    constexpr float ContaminationControlWeight = 0.35f;
    constexpr float ContaminationAttainmentWeight = 0.18f;
    constexpr float ContaminationRankPenalty = 0.055f;
    constexpr float ContaminationToleranceMin = 0.65f;
    constexpr float ContaminationToleranceMax = 2.4f;
    constexpr float CoherentForeignContribution = 0.12f;
    constexpr float ReliabilityBase = 0.12f;
    constexpr float ReliabilityFidelityWeight = 0.53f;
    constexpr float ReliabilitySemanticWeight = 0.34f;
    constexpr float ReliabilityContaminationPenalty = 0.28f;
    constexpr float ReliabilityMin = 0.02f;
    constexpr float ReliabilityMax = 0.995f;
}

void URefinementSubsystem::AddScore(TMap<FName, float>& Target, const FName Key, const float Amount)
{
    if (Key.IsNone() || !FMath::IsFinite(Amount) || Amount <= 0.0f) return;
    Target.FindOrAdd(Key) += Amount;
}

void URefinementSubsystem::AddScores(TMap<FName, float>& Target, const TMap<FName, float>& Source, const float Multiplier)
{
    for (const TPair<FName, float>& Pair : Source) AddScore(Target, Pair.Key, Pair.Value * Multiplier);
}

TArray<TPair<FName, float>> URefinementSubsystem::SortedScores(const TMap<FName, float>& Scores)
{
    TArray<TPair<FName, float>> Result;
    Result.Reserve(Scores.Num());
    for (const TPair<FName, float>& Pair : Scores)
    {
        if (!Pair.Key.IsNone() && Pair.Value > 0.0f) Result.Add(Pair);
    }
    Result.Sort([](const TPair<FName, float>& A, const TPair<FName, float>& B)
    {
        if (!FMath::IsNearlyEqual(A.Value, B.Value)) return A.Value > B.Value;
        return A.Key.ToString() < B.Key.ToString();
    });
    return Result;
}

const TMap<FName, TMap<FName, float>>& URefinementSubsystem::PathProperties()
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

const TMap<FName, TMap<FName, float>>& URefinementSubsystem::AttributeProperties()
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

const TMap<FName, TMap<FName, float>>& URefinementSubsystem::TraitProperties()
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

const TMap<FName, TMap<FName, float>>& URefinementSubsystem::PathTraits()
{
    static const TMap<FName, TMap<FName, float>> Data = {
        {TEXT("Fire"),{{TEXT("short_lived"),.18f},{TEXT("self_cost"),.08f},{TEXT("charged"),.12f}}},
        {TEXT("Ice"),{{TEXT("stationary"),.12f},{TEXT("stored"),.08f}}},
        {TEXT("Water"),{{TEXT("maintained"),.08f}}}, {TEXT("Wind"),{{TEXT("short_lived"),.08f}}},
        {TEXT("Earth"),{{TEXT("grounded"),.42f},{TEXT("stationary"),.2f}}},
        {TEXT("Wood"),{{TEXT("maintained"),.14f},{TEXT("stored"),.08f}}},
        {TEXT("Strength"),{{TEXT("contact"),.22f},{TEXT("self_cost"),.1f}}},
        {TEXT("Light"),{{TEXT("charged"),.1f},{TEXT("prepared"),.08f}}}, {TEXT("Moon"),{{TEXT("delayed"),.08f}}},
        {TEXT("Blood"),{{TEXT("contact"),.2f},{TEXT("target_specific"),.12f},{TEXT("self_cost"),.12f}}},
        {TEXT("Food"),{{TEXT("consumable"),.28f},{TEXT("stored"),.12f}}}, {TEXT("Transformation"),{{TEXT("maintained"),.12f}}},
        {TEXT("Refinement"),{{TEXT("prepared"),.16f}}}, {TEXT("Enslavement"),{{TEXT("target_specific"),.28f},{TEXT("maintained"),.14f}}},
        {TEXT("Soul"),{{TEXT("target_specific"),.14f},{TEXT("maintained"),.12f}}}, {TEXT("Poison"),{{TEXT("contact"),.12f},{TEXT("delayed"),.16f}}},
        {TEXT("Metal"),{{TEXT("contact"),.12f}}}, {TEXT("Dark"),{{TEXT("delayed"),.12f},{TEXT("maintained"),.12f}}},
        {TEXT("Shadow"),{{TEXT("attached"),.36f},{TEXT("contact"),.16f},{TEXT("delayed"),.08f}}},
        {TEXT("Qi"),{{TEXT("charged"),.1f},{TEXT("stored"),.1f}}}, {TEXT("Information"),{{TEXT("target_specific"),.2f},{TEXT("prepared"),.12f}}},
        {TEXT("Time"),{{TEXT("delayed"),.36f},{TEXT("trigger"),.16f}}}, {TEXT("Space"),{{TEXT("prepared"),.08f}}}, {TEXT("Luck"),{{TEXT("trigger"),.12f}}},
    };
    return Data;
}

TMap<FName, float> URefinementSubsystem::DerivedPathScores(const TMap<FName, float>& PropertyScores)
{
    TMap<FName, float> Out;
    for (const TPair<FName, TMap<FName, float>>& PathPair : PathProperties())
    {
        float Score = 0.0f;
        for (const TPair<FName, float>& Affinity : PathPair.Value)
        {
            Score += PropertyScores.FindRef(Affinity.Key) * Affinity.Value;
        }
        if (Score > 0.0f) Out.Add(PathPair.Key, Score);
    }
    return Out;
}

float URefinementSubsystem::PropertyPathAffinity(const FName Path, const FName Property)
{
    const TMap<FName, float>* Profile = PathProperties().Find(Path);
    if (!Profile) return 0.0f;
    return FMath::Clamp(Profile->FindRef(Property), 0.0f, 1.5f) / 1.5f;
}

bool URefinementSubsystem::PathsCompatible(const FName A, const FName B)
{
    // Default browser behavior without a character-specific compatibility exemption.
    return A.IsNone() || B.IsNone() || A == B;
}

FRefinementDirection URefinementSubsystem::ResolveFormationDirection(const TMap<FName, float>& PathScores, const TMap<FName, float>& PropertyScores) const
{
    FRefinementDirection Result;
    Result.CombinedPathScores = PathScores;
    const TMap<FName, float> Derived = DerivedPathScores(PropertyScores);
    for (const TPair<FName, float>& Pair : Derived) AddScore(Result.CombinedPathScores, Pair.Key, Pair.Value * 0.34f);

    const TArray<TPair<FName, float>> Paths = SortedScores(Result.CombinedPathScores);
    const TArray<TPair<FName, float>> Properties = SortedScores(PropertyScores);
    if (Paths.IsEmpty()) return Result;

    Result.PrimaryPath = Paths[0].Key;
    const float TopPath = Paths[0].Value;
    const float SecondPath = Paths.Num() > 1 ? Paths[1].Value : 0.0f;
    float PathTotal = 0.0f;
    for (const TPair<FName, float>& Pair : Paths) PathTotal += Pair.Value;
    PathTotal = FMath::Max(PathTotal, 1.0f);
    Result.PathClarity = FMath::Clamp(
        (TopPath - SecondPath * 0.42f) / FMath::Max3(TopPath, PathTotal * 0.38f, 0.000001f),
        0.0f,
        1.0f);

    const int32 RelevantCount = FMath::Min(6, Properties.Num());
    float PropertyTotal = 0.0f;
    float Aligned = 0.0f;
    for (int32 Index = 0; Index < RelevantCount; ++Index)
    {
        PropertyTotal += Properties[Index].Value;
        Aligned += Properties[Index].Value * PropertyPathAffinity(Result.PrimaryPath, Properties[Index].Key);
    }
    PropertyTotal = FMath::Max(PropertyTotal, 1.0f);
    Result.PropertyAlignment = FMath::Clamp(Aligned / PropertyTotal, 0.0f, 1.0f);
    Result.Coherence = FMath::Clamp(Result.PathClarity * 0.58f + Result.PropertyAlignment * 0.42f, 0.0f, 1.0f);

    const int32 TopCount = FMath::Min(3, Properties.Num());
    for (int32 Index = 0; Index < TopCount; ++Index) Result.TopProperties.Add(Properties[Index].Key);
    return Result;
}

float URefinementSubsystem::TransferProperty(TMap<FName, float>& Scores, const FName From, const FName To, const float Amount)
{
    const float Available = FMath::Max(0.0f, Scores.FindRef(From));
    const float Moved = FMath::Min(Available, FMath::Max(0.0f, Amount));
    if (Moved <= 0.0f) return 0.0f;
    Scores.Add(From, FMath::Max(0.0f, Available - Moved));
    Scores.FindOrAdd(To) += Moved;
    return Moved;
}

void URefinementSubsystem::ApplyVerbToProperties(TMap<FName, float>& P, const ERefinementVerb Verb, const float Power) const
{
    const float V = FMath::Max(0.1f, Power);
    switch (Verb)
    {
        case ERefinementVerb::Heat:
        {
            const float Cold = TransferProperty(P, TEXT("cold"), TEXT("fluid"), V * .52f);
            const float Solid = TransferProperty(P, TEXT("solid"), TEXT("fluid"), V * .38f);
            P.FindOrAdd(TEXT("flow")) += (Cold + Solid) * .22f;
            const float Melt = P.FindRef(TEXT("fluid"));
            const float Vaporized = Melt > 1.1f ? TransferProperty(P, TEXT("fluid"), TEXT("vapor"), FMath::Min(Melt * .24f, V * .42f)) : 0.0f;
            P.FindOrAdd(TEXT("heat")) += V * .24f;
            P.FindOrAdd(TEXT("motion")) += V * .12f + (Cold + Solid) * .08f + Vaporized * .2f;
            break;
        }
        case ERefinementVerb::Cool:
        {
            const float Heat = TransferProperty(P, TEXT("heat"), TEXT("cold"), V * .48f);
            const float Vapor = TransferProperty(P, TEXT("vapor"), TEXT("fluid"), V * .42f);
            const float Fluid = P.FindRef(TEXT("fluid"));
            if (Fluid > 1.0f) TransferProperty(P, TEXT("fluid"), TEXT("solid"), FMath::Min(Fluid * .22f, V * .36f));
            P.FindOrAdd(TEXT("stillness")) += V * .16f + (Heat + Vapor) * .05f;
            break;
        }
        case ERefinementVerb::Merge:
            P.FindOrAdd(TEXT("adhesion")) += V * .34f;
            P.FindOrAdd(TEXT("persistence")) += V * .12f;
            break;
        case ERefinementVerb::Purify:
        {
            const TArray<TPair<FName, float>> Ranked = SortedScores(P);
            for (int32 Index = 4; Index < Ranked.Num(); ++Index)
            {
                P.Add(Ranked[Index].Key, FMath::Max(0.0f, Ranked[Index].Value - V * .13f));
            }
            P.FindOrAdd(TEXT("precision")) += V * .25f;
            P.FindOrAdd(TEXT("stability")) += V * .10f;
            break;
        }
        case ERefinementVerb::Control:
        {
            const TArray<TPair<FName, float>> Ranked = SortedScores(P);
            for (int32 Index = 0; Index < FMath::Min(2, Ranked.Num()); ++Index) P.FindOrAdd(Ranked[Index].Key) += V * .12f;
            P.FindOrAdd(TEXT("stability")) += V * .22f;
            P.FindOrAdd(TEXT("control")) += V * .15f;
            break;
        }
        case ERefinementVerb::Condense:
        {
            const TArray<TPair<FName, float>> Ranked = SortedScores(P);
            for (int32 Index = 0; Index < FMath::Min(3, Ranked.Num()); ++Index) P.FindOrAdd(Ranked[Index].Key) += V * (Index == 0 ? .16f : .09f);
            P.FindOrAdd(TEXT("stability")) += V * .28f;
            P.FindOrAdd(TEXT("persistence")) += V * .18f;
            break;
        }
        case ERefinementVerb::Process:
        default:
            P.FindOrAdd(TEXT("precision")) += V * .12f;
            break;
    }
}

bool URefinementSubsystem::AnalyzePhysicalInputs(const TArray<FGuid>& EntityIds, FRefinementAnalysis& OutAnalysis, FString& OutError) const
{
    OutAnalysis = FRefinementAnalysis();
    if (const UWorld* World = GetWorld(); World && World->GetNetMode() == NM_Client)
    {
        OutError = TEXT("Authoritative refinement semantics are not exposed to clients.");
        return false;
    }
    if (EntityIds.IsEmpty())
    {
        OutError = TEXT("Refinement requires at least one physical ECS entity.");
        return false;
    }

    const UGameInstance* GI = GetGameInstance();
    const UGuEntitySubsystem* Entities = GI ? GI->GetSubsystem<UGuEntitySubsystem>() : nullptr;
    const UGuDefinitionRegistrySubsystem* Definitions = GI ? GI->GetSubsystem<UGuDefinitionRegistrySubsystem>() : nullptr;
    if (!Entities || !Definitions)
    {
        OutError = TEXT("Refinement domain subsystems are unavailable.");
        return false;
    }

    for (int32 Index = 0; Index < EntityIds.Num(); ++Index)
    {
        const FGuid EntityId = EntityIds[Index];
        FRefinementSemanticSnapshot Snapshot;
        if (!Entities->GetRefinementSemanticSnapshot(EntityId, Snapshot))
        {
            OutError = FString::Printf(TEXT("Entity %s is not refinable."), *EntityId.ToString());
            return false;
        }

        FRefinementSemanticProfile Semantic = Snapshot.Semantic;
        UGuRulesLibrary::NormalizeSemanticProfile(Semantic);

        if (Snapshot.Kind == ERefinableKind::Gu)
        {
            const FGuDefinitionRecord* Definition = Definitions->FindDefinition(Snapshot.DefinitionId);
            if (!Definition)
            {
                OutError = FString::Printf(TEXT("Gu entity %s references unknown definition '%s'."), *EntityId.ToString(), *Snapshot.DefinitionId.ToString());
                return false;
            }

            const float Rank = static_cast<float>(FMath::Max(1, Definition->Rank));
            const bool bFoundation = Index == 0;
            const float FoundationBoost = bFoundation ? 1.55f : 1.0f;
            if (bFoundation) OutAnalysis.FoundationDefinitionId = Definition->Id;

            if (!Semantic.Paths.IsEmpty())
            {
                for (const TPair<FName, float>& Pair : Semantic.Paths)
                {
                    AddScore(OutAnalysis.PathScores, Pair.Key, Rank * 1.35f * Pair.Value * FoundationBoost);
                    if (const TMap<FName, float>* Profile = PathProperties().Find(Pair.Key)) AddScores(OutAnalysis.PropertyScores, *Profile, Rank * .72f * Pair.Value * FoundationBoost);
                    if (const TMap<FName, float>* Traits = PathTraits().Find(Pair.Key)) AddScores(OutAnalysis.TraitScores, *Traits, Rank * .16f * Pair.Value);
                }
            }
            else if (!Definition->Path.IsNone())
            {
                AddScore(OutAnalysis.PathScores, Definition->Path, Rank * 1.35f * FoundationBoost);
                if (const TMap<FName, float>* Profile = PathProperties().Find(Definition->Path)) AddScores(OutAnalysis.PropertyScores, *Profile, Rank * .72f * FoundationBoost);
            }

            if (!Semantic.bDerivedPropertySnapshot)
            {
                AddScores(OutAnalysis.PropertyScores, Semantic.Properties, Rank * .5f * FoundationBoost);
            }
            for (const TPair<FName, float>& Pair : Semantic.Attributes)
            {
                AddScore(OutAnalysis.AttributeScores, Pair.Key, Rank * .78f * Pair.Value * (bFoundation ? 1.2f : 1.0f));
                if (const TMap<FName, float>* Profile = AttributeProperties().Find(Pair.Key)) AddScores(OutAnalysis.PropertyScores, *Profile, Rank * .48f * Pair.Value * (bFoundation ? 1.2f : 1.0f));
            }
            for (const TPair<FName, float>& Pair : Semantic.Traits)
            {
                AddScore(OutAnalysis.TraitScores, Pair.Key, Rank * .72f * Pair.Value * (bFoundation ? 1.18f : 1.0f));
                if (const TMap<FName, float>* Profile = TraitProperties().Find(Pair.Key)) AddScores(OutAnalysis.PropertyScores, *Profile, Rank * .28f * Pair.Value);
            }
            for (const TPair<FName, float>& Pair : Semantic.Templates) AddScore(OutAnalysis.TemplateScores, Pair.Key, Rank * 1.15f * Pair.Value * (bFoundation ? 1.8f : 1.0f));
            OutAnalysis.NativeDaoMass += FMath::Max(.01f, Semantic.DaoMass > 0.0f ? Semantic.DaoMass : Rank * Rank * 1.15f);
            OutAnalysis.HighestInputGuRank = FMath::Max(OutAnalysis.HighestInputGuRank, Definition->Rank);
        }
        else
        {
            for (const TPair<FName, float>& Pair : Semantic.Paths)
            {
                AddScore(OutAnalysis.PathScores, Pair.Key, Pair.Value);
                if (const TMap<FName, float>* Profile = PathProperties().Find(Pair.Key)) AddScores(OutAnalysis.PropertyScores, *Profile, Pair.Value * .48f);
                if (const TMap<FName, float>* Traits = PathTraits().Find(Pair.Key)) AddScores(OutAnalysis.TraitScores, *Traits, Pair.Value * .12f);
            }
            if (!Semantic.bDerivedPropertySnapshot)
            {
                AddScores(OutAnalysis.PropertyScores, Semantic.Properties);
            }
            for (const TPair<FName, float>& Pair : Semantic.Attributes)
            {
                AddScore(OutAnalysis.AttributeScores, Pair.Key, Pair.Value);
                if (const TMap<FName, float>* Profile = AttributeProperties().Find(Pair.Key)) AddScores(OutAnalysis.PropertyScores, *Profile, Pair.Value * .55f);
            }
            for (const TPair<FName, float>& Pair : Semantic.Traits)
            {
                AddScore(OutAnalysis.TraitScores, Pair.Key, Pair.Value);
                if (const TMap<FName, float>* Profile = TraitProperties().Find(Pair.Key)) AddScores(OutAnalysis.PropertyScores, *Profile, Pair.Value * .4f);
            }
            AddScores(OutAnalysis.TemplateScores, Semantic.Templates);
            OutAnalysis.NativeDaoMass += FMath::Max(.01f, Semantic.DaoMass > 0.0f ? Semantic.DaoMass : .2f);
        }

        AddScores(OutAnalysis.ContaminationPaths, Snapshot.Contamination.Paths);
        AddScores(OutAnalysis.ContaminationAttributes, Snapshot.Contamination.Attributes);
        AddScores(OutAnalysis.ContaminationTraits, Snapshot.Contamination.Traits);
    }

    if (OutAnalysis.PropertyScores.IsEmpty()) OutAnalysis.PropertyScores.Add(TEXT("stability"), .25f);

    // Contamination is pressure on formation, not a random roll. Exact semantic
    // contamination propagation is expanded in phase 2; path pressure is already
    // preserved here so contamination can change the resulting Dao direction.
    for (const TPair<FName, float>& Pair : OutAnalysis.ContaminationPaths)
    {
        AddScore(OutAnalysis.PathScores, Pair.Key, Pair.Value * RefinementBalance::ContaminationPathInfluence);
    }
    AddScores(OutAnalysis.AttributeScores, OutAnalysis.ContaminationAttributes);
    AddScores(OutAnalysis.TraitScores, OutAnalysis.ContaminationTraits);

    OutAnalysis.NascentDirection = ResolveFormationDirection(OutAnalysis.PathScores, OutAnalysis.PropertyScores);
    OutAnalysis.PathScores = OutAnalysis.NascentDirection.CombinedPathScores;
    OutAnalysis.PrimaryPath = OutAnalysis.NascentDirection.PrimaryPath;

    const TArray<TPair<FName, float>> Paths = SortedScores(OutAnalysis.PathScores);
    float PathTotal = 0.0f;
    for (const TPair<FName, float>& Pair : Paths) PathTotal += Pair.Value;
    const float TopPath = Paths.IsEmpty() ? 1.0f : Paths[0].Value;
    OutAnalysis.PathCoherence = OutAnalysis.NascentDirection.Coherence;
    for (int32 Index = 1; Index < Paths.Num() && OutAnalysis.SecondaryPaths.Num() < 2; ++Index)
    {
        if (Paths[Index].Value >= FMath::Max(.8f, TopPath * .46f)) OutAnalysis.SecondaryPaths.Add(Paths[Index].Key);
    }

    const TArray<TPair<FName, float>> Attributes = SortedScores(OutAnalysis.AttributeScores);
    const float TopAttribute = Attributes.IsEmpty() ? 0.0f : Attributes[0].Value;
    const float AttributeThreshold = FMath::Max(.72f, TopAttribute * .43f);
    const int32 MaxAttributes = FMath::Min(6, 2 + OutAnalysis.HighestInputGuRank);
    for (const TPair<FName, float>& Pair : Attributes)
    {
        if (Pair.Value >= AttributeThreshold && OutAnalysis.SurvivingAttributes.Num() < MaxAttributes) OutAnalysis.SurvivingAttributes.Add(Pair.Key);
    }

    const TArray<TPair<FName, float>> Traits = SortedScores(OutAnalysis.TraitScores);
    const float TopTrait = Traits.IsEmpty() ? 0.0f : Traits[0].Value;
    const float TraitThreshold = FMath::Max(.48f, TopTrait * .46f);
    for (const TPair<FName, float>& Pair : Traits)
    {
        if (Pair.Value >= TraitThreshold && OutAnalysis.SurvivingTraits.Num() < 4) OutAnalysis.SurvivingTraits.Add(Pair.Key);
    }

    const TArray<TPair<FName, float>> Templates = SortedScores(OutAnalysis.TemplateScores);
    OutAnalysis.Template = Templates.IsEmpty() ? NAME_None : Templates[0].Key;

    OutAnalysis.DaoMass = ResolveRetainedDaoMass(OutAnalysis, FRefinementProcessHealth());
    OutAnalysis.ResultRank = UGuRulesLibrary::ExperimentalFormationRankFromRetainedDaoMass(OutAnalysis.DaoMass.RetainedMass);
    OutError.Reset();
    return true;
}

bool URefinementSubsystem::AnalyzePhysicalSelections(const TArray<FRefinementInputSelection>& Inputs,FRefinementAnalysis& OutAnalysis,FString& OutError) const
{
    if(Inputs.IsEmpty()){OutError=TEXT("Refinement requires at least one physical input.");return false;}
    const UGuEntitySubsystem* Entities=GetGameInstance()?GetGameInstance()->GetSubsystem<UGuEntitySubsystem>():nullptr;
    if(!Entities){OutError=TEXT("Gu entity subsystem is unavailable.");return false;}
    TArray<FGuid> Expanded;
    for(const FRefinementInputSelection& Input:Inputs)
    {
        const int32 Quantity=FMath::Max(1,Input.Quantity);
        FRefinementSemanticSnapshot Snapshot;
        if(!Entities->GetRefinementSemanticSnapshot(Input.EntityId,Snapshot)){OutError=TEXT("A selected physical refinement entity no longer exists.");return false;}
        if(const FMaterialLotComponent* Lot=Entities->GetMaterialLot(Input.EntityId))
        {
            if(Lot->Quantity<Quantity){OutError=FString::Printf(TEXT("Material lot '%s' has only %d of the requested %d units."),*Lot->Item.ToString(),Lot->Quantity,Quantity);return false;}
        }
        else if(Quantity!=1)
        {
            OutError=TEXT("Only material-lot entities can contribute more than one unit from a single physical entity.");
            return false;
        }
        for(int32 Unit=0;Unit<Quantity;++Unit)Expanded.Add(Input.EntityId);
    }
    return AnalyzePhysicalInputs(Expanded,OutAnalysis,OutError);
}

FRefinementRetentionResult URefinementSubsystem::ResolveRetainedDaoMass(const FRefinementAnalysis& Analysis, const FRefinementProcessHealth& Health) const
{
    FRefinementRetentionResult Result;
    const FRefinementDirection Direction = Analysis.NascentDirection.PrimaryPath.IsNone()
        ? ResolveFormationDirection(Analysis.PathScores, Analysis.PropertyScores)
        : Analysis.NascentDirection;

    TSet<FName> SurvivingPaths;
    if (!Analysis.PrimaryPath.IsNone()) SurvivingPaths.Add(Analysis.PrimaryPath);
    for (const FName Path : Analysis.SecondaryPaths) if (!Path.IsNone()) SurvivingPaths.Add(Path);

    float PathTotal = 0.0f;
    float RetainedPathScore = 0.0f;
    for (const TPair<FName, float>& Pair : Direction.CombinedPathScores)
    {
        const float Value = FMath::Max(0.0f, Pair.Value);
        PathTotal += Value;
        bool bCompatible = SurvivingPaths.Contains(Pair.Key);
        if (!bCompatible)
        {
            for (const FName Kept : SurvivingPaths)
            {
                if (PathsCompatible(Pair.Key, Kept)) { bCompatible = true; break; }
            }
        }
        if (bCompatible) RetainedPathScore += Value;
    }
    Result.PathRetention = FMath::Clamp(RetainedPathScore / FMath::Max(PathTotal, 1.0f), 0.0f, 1.0f);

    float AttributeTotal = 0.0f;
    float RetainedAttributeScore = 0.0f;
    TSet<FName> SurvivingAttributes;
    for (const FName Attribute : Analysis.SurvivingAttributes) SurvivingAttributes.Add(Attribute);
    for (const TPair<FName, float>& Pair : Analysis.AttributeScores)
    {
        const float Value = FMath::Max(0.0f, Pair.Value);
        AttributeTotal += Value;
        if (SurvivingAttributes.Contains(Pair.Key)) RetainedAttributeScore += Value;
    }
    Result.AttributeRetention = AttributeTotal > 0.0f ? FMath::Clamp(RetainedAttributeScore / AttributeTotal, 0.0f, 1.0f) : 1.0f;
    Result.PropertyAlignment = FMath::Clamp(Direction.PropertyAlignment, 0.0f, 1.0f);
    Result.Fidelity = FMath::Clamp(Health.Fidelity, 0.0f, 1.0f);

    Result.StructuralRetention = FMath::Clamp(
        Result.PathRetention * (
            RefinementBalance::RetentionPathBase
            + Result.AttributeRetention * RefinementBalance::RetentionAttributeWeight
            + Result.PropertyAlignment * RefinementBalance::RetentionPropertyWeight
            + Result.Fidelity * RefinementBalance::RetentionFidelityWeight),
        0.0f,
        1.0f);

    float SurvivingContamination = 0.0f;
    for (const TPair<FName, float>& Pair : Analysis.ContaminationPaths) SurvivingContamination += FMath::Max(0.0f, Pair.Value);
    Result.RawMass = FMath::Max(0.0f, Analysis.NativeDaoMass) + SurvivingContamination;

    const float ImpurityLoss = FMath::Clamp(
        FMath::Max(0.0f, Health.MaximumImpurities) / RefinementBalance::ImpurityLossDivisor,
        0.0f,
        RefinementBalance::MaxImpurityLoss);
    const float StabilityDrop = FMath::Max(0.0f, Health.MaxStability - Health.LowestStability) / FMath::Max(1.0f, Health.MaxStability);
    const float StabilityLoss = FMath::Clamp(StabilityDrop * RefinementBalance::StabilityLossWeight, 0.0f, RefinementBalance::MaxStabilityLoss);

    Result.RetainedMass = FMath::Max(0.0f, Result.RawMass * Result.StructuralRetention * (1.0f - ImpurityLoss - StabilityLoss));
    Result.DiscardedMass = FMath::Max(0.0f, Result.RawMass - Result.RetainedMass);
    return Result;
}

int64 URefinementSubsystem::NowUnixMs()
{
    return FDateTime::UtcNow().ToUnixTimestamp() * 1000LL;
}

FString URefinementSubsystem::VerbName(const ERefinementVerb Verb)
{
    switch (Verb)
    {
        case ERefinementVerb::Heat: return TEXT("Heat");
        case ERefinementVerb::Cool: return TEXT("Cool");
        case ERefinementVerb::Merge: return TEXT("Merge");
        case ERefinementVerb::Purify: return TEXT("Purify");
        case ERefinementVerb::Control: return TEXT("Control");
        case ERefinementVerb::Condense: return TEXT("Condense");
        case ERefinementVerb::Process:
        default: return TEXT("Process");
    }
}

URefinementSubsystem::FBasicActionSpec URefinementSubsystem::BasicAction(const ERefinementVerb Verb)
{
    FBasicActionSpec Result;
    Result.Verb = Verb;
    switch (Verb)
    {
        case ERefinementVerb::Process:
            Result.FocusCost = 4.0f; Result.ProgressPower = 11.0f; Result.ImpurityDelta = 1.0f; break;
        case ERefinementVerb::Heat:
            Result.FocusCost = 5.0f; Result.ProgressPower = 9.0f; Result.TemperatureDelta = 16.0f; break;
        case ERefinementVerb::Cool:
            Result.FocusCost = 5.0f; Result.ProgressPower = 8.0f; Result.TemperatureDelta = -18.0f; Result.TemperatureClampMin = 20.0f; break;
        case ERefinementVerb::Merge:
            Result.FocusCost = 7.0f; Result.ProgressPower = 15.0f; Result.StabilityDelta = -4.0f; Result.ImpurityDelta = 2.0f; break;
        case ERefinementVerb::Purify:
            Result.FocusCost = 6.0f; Result.ProgressPower = 9.0f; Result.ImpurityRemovalCap = 6.0f;
            Result.PurifyContaminationFraction = 0.12f; Result.PurifyContaminationMax = 0.9f; break;
        case ERefinementVerb::Control:
            Result.FocusCost = 6.0f; Result.ProgressPower = 8.0f; Result.StabilityDelta = 6.0f; break;
        case ERefinementVerb::Condense:
            Result.FocusCost = 8.0f; Result.ProgressPower = 18.0f; Result.StabilityDelta = -3.0f; break;
    }
    return Result;
}

const TMap<FName, TMap<FName, float>>& URefinementSubsystem::PropertyAttributes()
{
    static const TMap<FName, TMap<FName, float>> Data = {
        {TEXT("heat"),{{TEXT("amplification"),.55f},{TEXT("speed"),.18f}}},
        {TEXT("cold"),{{TEXT("suppression"),.45f},{TEXT("stability"),.18f}}},
        {TEXT("solid"),{{TEXT("stability"),.48f},{TEXT("persistence"),.3f}}},
        {TEXT("fluid"),{{TEXT("efficiency"),.22f},{TEXT("range"),.18f},{TEXT("stability"),.12f}}},
        {TEXT("vapor"),{{TEXT("speed"),.42f},{TEXT("area"),.28f},{TEXT("concealment"),.15f}}},
        {TEXT("motion"),{{TEXT("speed"),.62f},{TEXT("range"),.18f}}},
        {TEXT("flow"),{{TEXT("range"),.42f},{TEXT("tracking"),.18f},{TEXT("efficiency"),.2f}}},
        {TEXT("stillness"),{{TEXT("concealment"),.28f},{TEXT("suppression"),.32f},{TEXT("stability"),.22f}}},
        {TEXT("hardness"),{{TEXT("stability"),.48f},{TEXT("penetration"),.28f}}},
        {TEXT("vitality"),{{TEXT("recovery"),.68f},{TEXT("persistence"),.18f}}},
        {TEXT("growth"),{{TEXT("recovery"),.42f},{TEXT("amplification"),.2f}}},
        {TEXT("adhesion"),{{TEXT("persistence"),.42f},{TEXT("link"),.28f}}},
        {TEXT("force"),{{TEXT("amplification"),.62f},{TEXT("penetration"),.22f}}},
        {TEXT("luminosity"),{{TEXT("precision"),.28f},{TEXT("range"),.22f}}},
        {TEXT("precision"),{{TEXT("precision"),.7f}}},
        {TEXT("blood"),{{TEXT("bleed"),.72f},{TEXT("amplification"),.18f}}},
        {TEXT("assimilation"),{{TEXT("efficiency"),.6f}}},
        {TEXT("adaptability"),{{TEXT("efficiency"),.28f},{TEXT("stability"),.18f}}},
        {TEXT("control"),{{TEXT("tracking"),.3f},{TEXT("suppression"),.32f},{TEXT("precision"),.18f}}},
        {TEXT("link"),{{TEXT("link"),.68f}}},
        {TEXT("persistence"),{{TEXT("persistence"),.7f},{TEXT("duration"),.3f}}},
        {TEXT("corrosion"),{{TEXT("poison"),.72f},{TEXT("penetration"),.22f}}},
        {TEXT("sharpness"),{{TEXT("penetration"),.62f},{TEXT("bleed"),.18f}}},
        {TEXT("expansion"),{{TEXT("area"),.52f},{TEXT("range"),.38f}}},
        {TEXT("concealment"),{{TEXT("concealment"),.72f}}},
        {TEXT("timing"),{{TEXT("timed"),.65f},{TEXT("duration"),.25f}}},
    };
    return Data;
}

const TMap<FName, TMap<FName, float>>& URefinementSubsystem::PropertyTraits()
{
    static const TMap<FName, TMap<FName, float>> Data = {
        {TEXT("stillness"),{{TEXT("stationary"),.34f},{TEXT("grounded"),.12f}}},
        {TEXT("solid"),{{TEXT("grounded"),.26f},{TEXT("stationary"),.08f}}},
        {TEXT("adhesion"),{{TEXT("contact"),.3f},{TEXT("attached"),.38f}}},
        {TEXT("precision"),{{TEXT("prepared"),.18f},{TEXT("target_specific"),.22f}}},
        {TEXT("control"),{{TEXT("maintained"),.18f},{TEXT("trigger"),.12f}}},
        {TEXT("link"),{{TEXT("attached"),.28f},{TEXT("target_specific"),.2f}}},
        {TEXT("persistence"),{{TEXT("maintained"),.26f},{TEXT("stored"),.18f}}},
        {TEXT("timing"),{{TEXT("delayed"),.28f},{TEXT("charged"),.18f},{TEXT("trigger"),.2f}}},
        {TEXT("force"),{{TEXT("self_cost"),.08f}}},
        {TEXT("expansion"),{{TEXT("short_lived"),.06f}}},
        {TEXT("adaptability"),{{TEXT("environment_bound"),.05f}}},
    };
    return Data;
}

TMap<FName, float> URefinementSubsystem::TraitsFromProperties(const TMap<FName, float>& PropertyScores)
{
    TMap<FName, float> Result;
    for (const TPair<FName, float>& Property : PropertyScores)
    {
        if (const TMap<FName, float>* TraitProfile = PropertyTraits().Find(Property.Key))
        {
            AddScores(Result, *TraitProfile, Property.Value);
        }
    }
    return Result;
}

void URefinementSubsystem::ContaminationSemantics(
    const FName Path,
    const float Amount,
    TMap<FName, float>& OutProperties,
    TMap<FName, float>& OutAttributes,
    TMap<FName, float>& OutTraits)
{
    const float Scale = FMath::Max(0.0f, Amount);
    if (const TMap<FName, float>* PathProfile = PathProperties().Find(Path)) AddScores(OutProperties, *PathProfile, Scale);
    for (const TPair<FName, float>& Property : OutProperties)
    {
        if (const TMap<FName, float>* AttrProfile = PropertyAttributes().Find(Property.Key))
        {
            AddScores(OutAttributes, *AttrProfile, Property.Value * RefinementBalance::ContaminationAttributeScale);
        }
        if (const TMap<FName, float>* TraitProfile = PropertyTraits().Find(Property.Key))
        {
            AddScores(OutTraits, *TraitProfile, Property.Value * RefinementBalance::ContaminationTraitScale);
        }
    }
    if (const TMap<FName, float>* PathTraitProfile = PathTraits().Find(Path))
    {
        AddScores(OutTraits, *PathTraitProfile, Scale * RefinementBalance::ContaminationPathTraitScale);
    }
}

void URefinementSubsystem::ApplyMethodPathPressure(
    TMap<FName, float>& PathScores,
    const TMap<FName, float>& PropertyScores,
    const ERefinementVerb Verb,
    const FName MethodPath,
    const float Power) const
{
    const float P = FMath::Max(0.1f, Power);
    if (MethodPath.IsNone() || MethodPath == TEXT("Refinement")) return;

    const float BeforeCold = PropertyScores.FindRef(TEXT("cold")) + PropertyScores.FindRef(TEXT("solid")) * .45f;
    const float Fluid = PropertyScores.FindRef(TEXT("fluid"));
    const float Vapor = PropertyScores.FindRef(TEXT("vapor"));
    const float Motion = PropertyScores.FindRef(TEXT("motion"));

    if (MethodPath == TEXT("Fire") && Verb == ERefinementVerb::Heat && (Fluid > BeforeCold * .42f || Vapor > .35f))
    {
        if (float* Ice = PathScores.Find(TEXT("Ice"))) *Ice = FMath::Max(0.0f, *Ice * (1.0f - FMath::Clamp(P * .075f, .08f, .3f)));
        if (Vapor + Motion > Fluid * 1.05f)
        {
            if (float* Water = PathScores.Find(TEXT("Water"))) *Water = FMath::Max(0.0f, *Water * (1.0f - FMath::Clamp(P * .035f, .04f, .14f)));
            AddScore(PathScores, TEXT("Wind"), P * .92f);
            AddScore(PathScores, TEXT("Water"), P * .18f);
            AddScore(PathScores, TEXT("Fire"), P * .10f);
        }
        else
        {
            AddScore(PathScores, TEXT("Water"), P * .96f);
            AddScore(PathScores, TEXT("Fire"), P * .08f);
        }
        return;
    }

    if ((MethodPath == TEXT("Ice") || MethodPath == TEXT("Water")) && Verb == ERefinementVerb::Cool)
    {
        const float Solid = PropertyScores.FindRef(TEXT("solid"));
        const float Cold = PropertyScores.FindRef(TEXT("cold"));
        if (Cold + Solid > Fluid * .8f)
        {
            AddScore(PathScores, TEXT("Ice"), P * .78f);
            if (MethodPath == TEXT("Water")) AddScore(PathScores, TEXT("Water"), P * .18f);
            return;
        }
    }
    AddScore(PathScores, MethodPath, P * .62f);
}

float URefinementSubsystem::DaoMassRequiredForRank(const int32 Rank) const
{
    const int32 R = FMath::Clamp(Rank, 1, 9);
    return static_cast<float>(R * R) * RefinementBalance::DaoMassPerRankSquared;
}

float URefinementSubsystem::TraitBudgetMultiplier(const TArray<FName>& Traits, const FName Template) const
{
    static const TMap<FName, float> Bonuses = {
        {TEXT("consumable"),1.0f},{TEXT("charged"),.38f},{TEXT("contact"),.22f},{TEXT("stationary"),.28f},
        {TEXT("self_cost"),.38f},{TEXT("short_lived"),.18f},{TEXT("prepared"),.18f},
    };
    static const float Diminishing[] = {1.0f,.68f,.46f,.30f,.18f,.10f};
    const TSet<FName> TargetedTemplates = {TEXT("projectile"),TEXT("melee"),TEXT("area"),TEXT("restriction")};
    const TSet<FName> DurationTemplates = {TEXT("shield"),TEXT("movement"),TEXT("conceal"),TEXT("reveal"),TEXT("buff"),TEXT("amplifier"),TEXT("restriction")};

    TSet<FName> Unique;
    TArray<float> Values;
    for (const FName Trait : Traits)
    {
        const FName Clean(*Trait.ToString().ToLower());
        if (Unique.Contains(Clean)) continue;
        Unique.Add(Clean);
        if (Clean == TEXT("contact") && !TargetedTemplates.Contains(Template)) continue;
        if (Clean == TEXT("short_lived") && !DurationTemplates.Contains(Template)) continue;
        const float Bonus = Bonuses.FindRef(Clean);
        if (Bonus > 0.0f) Values.Add(Bonus);
    }
    Values.Sort([](const float A, const float B){ return A > B; });
    float BonusTotal = 0.0f;
    for (int32 Index = 0; Index < Values.Num(); ++Index)
    {
        BonusTotal += Values[Index] * (Index < UE_ARRAY_COUNT(Diminishing) ? Diminishing[Index] : .06f);
    }
    return FMath::Clamp(1.0f + BonusTotal, 1.0f, 3.0f);
}

FRefinementPowerAllocation URefinementSubsystem::ResolvePowerAllocation(
    const int32 Rank,
    const FName Template,
    const TArray<FName>& Attributes,
    const TArray<FName>& Traits) const
{
    struct FRankBudget { float Budget; float Damage; float Range; float Area; float DurationMs; };
    static const FRankBudget Profiles[9] = {
        {100,22.5f,180,45,3000},{500,112.5f,240,60,3800},{2500,600,320,80,4800},
        {12500,3000,420,105,6200},{50000,12000,560,135,8000},{200000,48000,740,175,10500},
        {800000,192000,980,225,13500},{3200000,768000,1300,290,17500},{12800000,3072000,1750,380,23000},
    };
    const FRankBudget& Profile = Profiles[FMath::Clamp(Rank,1,9)-1];

    static const TMap<FName, TMap<FName,float>> BaseWeights = {
        {TEXT("projectile"),{{TEXT("magnitude"),6},{TEXT("range"),2},{TEXT("speed"),1},{TEXT("precision"),1}}},
        {TEXT("melee"),{{TEXT("magnitude"),7.4f},{TEXT("range"),.7f},{TEXT("speed"),.9f},{TEXT("precision"),.7f}}},
        {TEXT("area"),{{TEXT("magnitude"),5.2f},{TEXT("area"),3},{TEXT("range"),.7f},{TEXT("precision"),.5f}}},
        {TEXT("shield"),{{TEXT("magnitude"),6.4f},{TEXT("duration"),2.1f},{TEXT("stability"),1.5f}}},
        {TEXT("movement"),{{TEXT("speed"),6.8f},{TEXT("duration"),2.3f},{TEXT("efficiency"),.9f}}},
        {TEXT("heal"),{{TEXT("magnitude"),6.7f},{TEXT("recovery"),2.2f},{TEXT("efficiency"),1.1f}}},
        {TEXT("conceal"),{{TEXT("concealment"),5.5f},{TEXT("duration"),2.4f},{TEXT("precision"),1.1f},{TEXT("efficiency"),1}}},
        {TEXT("reveal"),{{TEXT("range"),4.2f},{TEXT("precision"),3.2f},{TEXT("duration"),1.7f},{TEXT("tracking"),.9f}}},
        {TEXT("buff"),{{TEXT("magnitude"),5.4f},{TEXT("duration"),2.7f},{TEXT("efficiency"),1.4f},{TEXT("stability"),.5f}}},
        {TEXT("amplifier"),{{TEXT("magnitude"),6.1f},{TEXT("duration"),2.1f},{TEXT("efficiency"),1.3f}}},
        {TEXT("restriction"),{{TEXT("suppression"),4.7f},{TEXT("duration"),2.4f},{TEXT("range"),1.2f},{TEXT("precision"),1.2f}}},
        {TEXT("attribute"),{{TEXT("magnitude"),4},{TEXT("precision"),2},{TEXT("efficiency"),2},{TEXT("stability"),2}}},
    };
    static const TMap<FName, TMap<FName,float>> Priorities = {
        {TEXT("amplification"),{{TEXT("magnitude"),2.4f}}},{TEXT("range"),{{TEXT("range"),2.1f}}},{TEXT("area"),{{TEXT("area"),2.2f}}},
        {TEXT("speed"),{{TEXT("speed"),2.0f}}},{TEXT("duration"),{{TEXT("duration"),1.9f}}},{TEXT("precision"),{{TEXT("precision"),1.8f}}},
        {TEXT("persistence"),{{TEXT("duration"),1.35f},{TEXT("stability"),.65f}}},{TEXT("tracking"),{{TEXT("tracking"),2.0f}}},
        {TEXT("penetration"),{{TEXT("penetration"),2.0f},{TEXT("magnitude"),.35f}}},{TEXT("stability"),{{TEXT("stability"),1.8f}}},
        {TEXT("efficiency"),{{TEXT("efficiency"),1.8f}}},{TEXT("concealment"),{{TEXT("concealment"),2.1f}}},
        {TEXT("suppression"),{{TEXT("suppression"),2.0f}}},{TEXT("bleed"),{{TEXT("bleed"),1.8f}}},{TEXT("poison"),{{TEXT("poison"),1.8f}}},
        {TEXT("timed"),{{TEXT("timing"),1.7f}}},{TEXT("recovery"),{{TEXT("recovery"),1.9f}}},{TEXT("link"),{{TEXT("link"),1.7f}}},
    };

    FRefinementPowerAllocation Result;
    Result.BaseBudget = Profile.Budget;
    Result.ConstraintMultiplier = TraitBudgetMultiplier(Traits, Template);
    Result.EffectiveBudget = Profile.Budget * Result.ConstraintMultiplier;
    TMap<FName,float> Weights = BaseWeights.Contains(Template) ? BaseWeights.FindChecked(Template) : BaseWeights.FindChecked(TEXT("attribute"));
    for (const FName Attribute : Attributes) if (const TMap<FName,float>* P = Priorities.Find(Attribute)) AddScores(Weights,*P);

    TSet<FName> TraitSet;
    for (const FName Trait : Traits) TraitSet.Add(FName(*Trait.ToString().ToLower()));
    const bool bTargeted = Template == TEXT("projectile") || Template == TEXT("melee") || Template == TEXT("area") || Template == TEXT("restriction");
    if (TraitSet.Contains(TEXT("contact")) && bTargeted)
    {
        const float Released = FMath::Max(0.0f, Weights.FindRef(TEXT("range")) * .8f);
        Weights.Add(TEXT("range"), Weights.FindRef(TEXT("range")) * .2f);
        Weights.FindOrAdd(TEXT("magnitude")) += Released * .72f;
        Weights.FindOrAdd(TEXT("precision")) += Released * .28f;
    }
    if (TraitSet.Contains(TEXT("short_lived")) && Weights.Contains(TEXT("duration")))
    {
        const float Released = Weights.FindRef(TEXT("duration")) * .6f;
        Weights.Add(TEXT("duration"), Weights.FindRef(TEXT("duration")) * .4f);
        Weights.FindOrAdd(TEXT("magnitude")) += Released * .7f;
        Weights.FindOrAdd(TEXT("efficiency")) += Released * .3f;
    }
    float Total = 0.0f;
    for (const TPair<FName,float>& Pair : Weights) Total += FMath::Max(0.0f,Pair.Value);
    Total = FMath::Max(1.0f,Total);
    for (const TPair<FName,float>& Pair : Weights) Result.Allocation.Add(Pair.Key,FMath::Max(0.0f,Pair.Value)/Total);

    const auto Project = [&](const FName Key, const float ReferenceShare, const float ReferenceValue)
    {
        const float Share = Result.Allocation.FindRef(Key);
        return (ReferenceShare > 0.0f && Share > 0.0f) ? ReferenceValue * Result.ConstraintMultiplier * (Share / ReferenceShare) : 0.0f;
    };
    Result.Magnitude = FMath::Max(1.0f,FMath::RoundToFloat(Project(TEXT("magnitude"),.60f,Profile.Damage)));
    Result.Range = FMath::Max(0.0f,FMath::RoundToFloat(Project(TEXT("range"),.18f,Profile.Range)));
    Result.Area = FMath::Max(0.0f,FMath::RoundToFloat(Project(TEXT("area"),.24f,Profile.Area)));
    Result.DurationMs = FMath::Max(0.0f,FMath::RoundToFloat(Project(TEXT("duration"),.22f,Profile.DurationMs)));
    const float SpeedShare = Result.Allocation.FindRef(TEXT("speed"));
    Result.SpeedMultiplier = SpeedShare > 0.0f ? FMath::Clamp(1.0f + Result.ConstraintMultiplier * (SpeedShare/.12f)*.3f,.55f,3.5f) : 1.0f;
    if (TraitSet.Contains(TEXT("contact")) && bTargeted) Result.Range = FMath::Min(Result.Range,70.0f);
    return Result;
}

float URefinementSubsystem::ContaminationTotal(const FRefinementSessionState& Session) const
{
    float Total = 0.0f;
    for (const TPair<FName,float>& Pair : Session.ContaminationPaths) Total += FMath::Max(0.0f,Pair.Value);
    return Total;
}

FRefinementDirection URefinementSubsystem::EffectiveDirection(const FRefinementSessionState& Session) const
{
    TMap<FName,float> Paths = Session.NascentPathScores;
    TMap<FName,float> Properties = Session.NascentProperties;
    for (const TPair<FName,float>& Pair : Session.ContaminationPaths)
    {
        AddScore(Paths,Pair.Key,FMath::Max(0.0f,Pair.Value)*RefinementBalance::ContaminationPathInfluence);
        TMap<FName,float> CP, CA, CT;
        ContaminationSemantics(Pair.Key,FMath::Max(0.0f,Pair.Value)*RefinementBalance::ContaminationSemanticInfluence,CP,CA,CT);
        AddScores(Properties,CP);
    }
    return ResolveFormationDirection(Paths,Properties);
}

float URefinementSubsystem::SessionFidelity(const FRefinementSessionState& Session) const
{
    const float Total = FMath::Max(1.0f,static_cast<float>(Session.ActionHistory.Num()));
    const float AcceptedRatio = Session.AcceptedActionCount / Total;
    const float DeviationPenalty = (Session.OffMethodActionCount/Total)*.42f + (Session.TemperatureExcursions/Total)*.18f
        + FMath::Clamp(Session.MaximumImpurities/100.0f,0.0f,1.0f)*.16f
        + FMath::Clamp(ContaminationTotal(Session)/30.0f,0.0f,1.0f)*.22f
        + FMath::Clamp((Session.MaxStability-Session.LowestStability)/FMath::Max(1.0f,Session.MaxStability),0.0f,1.0f)*.12f;
    return FMath::Clamp(.38f+AcceptedRatio*.68f-DeviationPenalty,0.0f,1.0f);
}

float URefinementSubsystem::PurifyContamination(FRefinementSessionState& Session, const float Fraction, const float MaxAmount) const
{
    const float Before = ContaminationTotal(Session);
    if (Before <= 0.0f) return 0.0f;
    const float Requested = FMath::Min3(Before,FMath::Max(0.0f,MaxAmount),Before*FMath::Clamp(Fraction,0.0f,1.0f));
    if (Requested <= 0.0f) return 0.0f;
    TArray<TPair<FName,float>> Entries = SortedScores(Session.ContaminationPaths);
    float Remaining = Requested;
    for (const TPair<FName,float>& Pair : Entries)
    {
        if (Remaining <= 0.0f) break;
        const float Take = FMath::Min(Pair.Value,Remaining);
        const float Next = FMath::Max(0.0f,Pair.Value-Take);
        if (Next <= KINDA_SMALL_NUMBER) Session.ContaminationPaths.Remove(Pair.Key); else Session.ContaminationPaths.Add(Pair.Key,Next);
        Remaining -= Take;
    }
    const float Removed = FMath::Max(0.0f,Requested-Remaining);
    const float Ratio = FMath::Clamp(Removed/Before,0.0f,1.0f);
    for (TMap<FName,float>* Table : {&Session.ContaminationAttributes,&Session.ContaminationTraits})
    {
        TArray<FName> Keys;
        Table->GenerateKeyArray(Keys);
        for (const FName Key : Keys)
        {
            const float Next = FMath::Max(0.0f,Table->FindRef(Key)*(1.0f-Ratio));
            if (Next < KINDA_SMALL_NUMBER) Table->Remove(Key); else Table->Add(Key,Next);
        }
    }
    return Removed;
}

void URefinementSubsystem::ApplyPhysicalAction(FRefinementSessionState& Session, const FBasicActionSpec& Action) const
{
    Session.Temperature += Action.TemperatureDelta;
    if (Action.TemperatureClampMin > -FLT_MAX/2.0f) Session.Temperature = FMath::Max(Action.TemperatureClampMin,Session.Temperature);
    Session.Stability += Action.StabilityDelta;
    if (Action.Verb == ERefinementVerb::Control) Session.Stability = FMath::Min(Session.MaxStability,Session.Stability);
    if (Action.ImpurityRemovalCap > 0.0f) Session.Impurities -= FMath::Min(Action.ImpurityRemovalCap,Session.Impurities);
    else Session.Impurities = FMath::Max(0.0f,Session.Impurities+Action.ImpurityDelta);
    if (Action.PurifyContaminationFraction > 0.0f)
    {
        const float Removed = PurifyContamination(Session,Action.PurifyContaminationFraction,Action.PurifyContaminationMax);
        if (Removed > .01f) AddLog(Session,TEXT("The mixture sheds a little residue."));
    }
}

void URefinementSubsystem::GuideNascentFormation(
    FRefinementSessionState& Session,
    const ERefinementVerb Verb,
    const float Affinity,
    const float StageFraction,
    const FName TechniquePath,
    const bool bSourceTechnique) const
{
    const float Fraction = FMath::Clamp(StageFraction,0.0f,1.0f);
    if (Fraction <= 0.0f) return;
    const FRefinementDirection Before = EffectiveDirection(Session);
    const float TechniqueScale = bSourceTechnique ? 1.12f : 1.0f;
    const float Power = FMath::Clamp(Fraction*(.9f+FMath::Clamp(Affinity,0.0f,1.25f)*.18f)*TechniqueScale,.02f,1.45f);
    ApplyVerbToProperties(Session.NascentProperties,Verb,Power);
    if (!TechniquePath.IsNone() && TechniquePath != TEXT("Refinement"))
    {
        if (const TMap<FName,float>* Profile = PathProperties().Find(TechniquePath)) AddScores(Session.NascentProperties,*Profile,Power*.11f);
        ApplyMethodPathPressure(Session.NascentPathScores,Session.NascentProperties,Verb,TechniquePath,Power);
    }
    const FRefinementDirection After = EffectiveDirection(Session);
    if (After.Coherence+.12f < Before.Coherence) Session.Stability -= FMath::Min(7.0f,(Before.Coherence-After.Coherence)*18.0f);
}

void URefinementSubsystem::ApplyTemperatureConsequences(
    FRefinementSessionState& Session,
    const FRefinementProcedureStep& Step,
    const ERefinementVerb Verb,
    const float BeforeTemperature) const
{
    const float Minimum = Step.TargetTemperature.X;
    const float Maximum = Step.TargetTemperature.Y;
    const bool bApproachingCold = Session.Temperature < Minimum && Verb == ERefinementVerb::Heat && Session.Temperature > BeforeTemperature;
    const bool bApproachingHot = Session.Temperature > Maximum && Verb == ERefinementVerb::Cool && Session.Temperature < BeforeTemperature;
    if (Session.Temperature < Minimum && !bApproachingCold)
    {
        Session.Stability -= 3.0f;
        Session.Impurities += 2.0f;
        AddLog(Session,TEXT("The refinement loses its rhythm and begins to strain."));
    }
    else if (Session.Temperature > Maximum && !bApproachingHot)
    {
        Session.Stability -= 9.0f;
        Session.Impurities += 5.0f;
        AddLog(Session,TEXT("The refinement bucks violently and part of the mixture is lost."));
    }
    Session.Temperature = FMath::Max(20.0f,Session.Temperature-3.0f);
}

float URefinementSubsystem::RecordObservableFeedback(
    FRefinementSessionState& Session,
    const ERefinementVerb Verb,
    const float Affinity,
    const float SemanticDelta,
    const float StabilityDelta,
    const float ImpurityDelta,
    const float ProgressFraction,
    const bool bOutsideTemperature) const
{
    const float Previous = Session.Observations.IsEmpty() ? 0.0f : Session.Observations.Last().Score;
    const FString Seed = FString::Printf(TEXT("%lld:%d:%d:%s"),Session.StartedAtUnixMs,Session.StepIndex,Session.ActionHistory.Num(),*VerbName(Verb));
    FRandomStream Rng(static_cast<int32>(FCrc::StrCrc32(*Seed)));
    const float Noise = (Rng.FRand()-.5f)*.20f;
    float Score=(FMath::Clamp(Affinity,0.0f,1.25f)-.58f)*.68f;
    Score+=FMath::Clamp(SemanticDelta*1.8f,-.34f,.34f);
    Score+=FMath::Clamp(StabilityDelta/18.0f,-.32f,.14f);
    Score-=FMath::Clamp(ImpurityDelta/12.0f,0.0f,.30f);
    Score+=FMath::Clamp(ProgressFraction*.28f,0.0f,.10f);
    if (bOutsideTemperature) Score-=.10f;
    Score+=FMath::Clamp(Noise,-.10f,.10f);
    Score+=FMath::Clamp(Previous,-1.0f,1.0f)*.12f;
    Score=FMath::Clamp(Score,-1.0f,1.0f);
    FRefinementObservation Observation;
    Observation.Score=Score; Observation.AtUnixMs=NowUnixMs(); Observation.Verb=Verb;
    Session.Observations.Add(Observation);
    if (Session.Observations.Num()>20) Session.Observations.RemoveAt(0,Session.Observations.Num()-20);
    if (Score>.50f) AddLog(Session,TEXT("The nascent Gu responds cleanly to your handling."));
    else if (Score>.18f) AddLog(Session,TEXT("The forming Gu seems receptive, though the response is subtle."));
    else if (Score<-.50f) AddLog(Session,TEXT("The nascent Gu shudders and nearly rejects your handling."));
    else if (Score<-.18f) AddLog(Session,TEXT("The forming structure resists your handling."));
    else AddLog(Session,TEXT("The reaction is difficult to read."));
    return Score;
}

FString URefinementSubsystem::ObservableResponseLabel(const TArray<FRefinementObservation>& History) const
{
    const int32 Count=FMath::Min(3,History.Num());
    if (Count<=0) return TEXT("Unreadable");
    float Trend=0.0f;
    if (Count==1) Trend=History.Last().Score;
    else if (Count==2) Trend=History[History.Num()-2].Score*.38f+History.Last().Score*.62f;
    else Trend=History[History.Num()-3].Score*.18f+History[History.Num()-2].Score*.30f+History.Last().Score*.52f;
    const float Last=History.Last().Score;
    if (Trend>.42f&&Last>.15f)return TEXT("Strong resonance");
    if (Trend>.16f)return TEXT("Faint resonance");
    if (Trend<-.42f&&Last<-.15f)return TEXT("Violent rejection");
    if (Trend<-.16f)return TEXT("Resistance");
    return TEXT("Uncertain");
}

FString URefinementSubsystem::PublicFormLabel(const FRefinementSessionState& Session) const
{
    if (Session.bFinished) return Session.bSuccess ? TEXT("Condensed") : TEXT("Dispersed");
    const float Ratio = Session.HiddenProcedure.IsEmpty() ? 0.0f : (static_cast<float>(Session.StepIndex) + FMath::Clamp(Session.StepProgress/FMath::Max(1.0f,Session.HiddenProcedure[FMath::Clamp(Session.StepIndex,0,Session.HiddenProcedure.Num()-1)].RequiredProgress),0.0f,1.0f))/Session.HiddenProcedure.Num();
    if (Ratio<.18f)return TEXT("Loose mixture");
    if (Ratio<.45f)return TEXT("Gathering outline");
    if (Ratio<.72f)return TEXT("Nascent body");
    return TEXT("Nearly condensed");
}

FString URefinementSubsystem::PublicConditionLabel(const FRefinementSessionState& Session) const
{
    if (Session.bFinished) return Session.bSuccess ? TEXT("Settled") : TEXT("Failed");
    if (Session.Stability<=20.0f || Session.Impurities>=75.0f) return TEXT("Critical");
    if (Session.Stability<50.0f || Session.Impurities>=40.0f) return TEXT("Strained");
    if (Session.Stability>=80.0f && Session.Impurities<=5.0f) return TEXT("Calm");
    return TEXT("Stable");
}

void URefinementSubsystem::AddLog(FRefinementSessionState& Session, const FString& Message) const
{
    Session.Log.Add(Message);
    if (Session.Log.Num()>80) Session.Log.RemoveAt(0,Session.Log.Num()-80);
}

void URefinementSubsystem::GenerateProcedure(FRefinementSessionState& Session) const
{
    Session.HiddenProcedure.Reset();
    int32 ComponentUnits=0;for(const FRefinementInputSelection& Selection:Session.InputSelections)ComponentUnits+=FMath::Max(1,Selection.Quantity);
    const int32 ComponentCount = FMath::Max(1,ComponentUnits);
    const int32 StageCount = FMath::Clamp(3+(Session.AttemptRank>=3?1:0)+(ComponentCount>=5?1:0),3,5);
    const FName TargetPath = Session.IntendedPath.IsNone() ? Session.InitialAnalysis.PrimaryPath : Session.IntendedPath;
    FString Signature = Session.IntendedDefinitionId.ToString()+TEXT("|")+TargetPath.ToString()+TEXT("|")+FString::FromInt(Session.AttemptRank);
    for (const FRefinementInputSelection& Selection : Session.InputSelections) Signature += TEXT("|")+Selection.EntityId.ToString(EGuidFormats::Digits)+TEXT("x")+FString::FromInt(Selection.Quantity);
    FRandomStream Random(static_cast<int32>(FCrc::StrCrc32(*Signature)));

    TMap<FName,float> CurrentPaths = Session.NascentPathScores;
    TMap<FName,float> CurrentProperties = Session.NascentProperties;

    const auto TargetAlignment = [&](const FRefinementDirection& Direction)
    {
        if (TargetPath.IsNone()) return Direction.Coherence;
        const TArray<TPair<FName,float>> Sorted = SortedScores(Direction.CombinedPathScores);
        const float Top = FMath::Max(.001f,Sorted.IsEmpty()?0.0f:Sorted[0].Value);
        return FMath::Clamp(Direction.CombinedPathScores.FindRef(TargetPath)/Top,0.0f,1.0f);
    };
    const auto TargetPropertyFit = [&](const TMap<FName,float>& Props)
    {
        const TMap<FName,float>* Profile = PathProperties().Find(TargetPath);
        if (!Profile || Profile->IsEmpty()) return .5f;
        float Total=0.0f,Aligned=0.0f;
        for (const TPair<FName,float>& Pair : Props)
        {
            const float V=FMath::Max(0.0f,Pair.Value);
            Total+=V; Aligned+=V*PropertyPathAffinity(TargetPath,Pair.Key);
        }
        return FMath::Clamp(Aligned/FMath::Max(1.0f,Total),0.0f,1.0f);
    };
    struct FSimulation
    {
        ERefinementVerb Verb=ERefinementVerb::Control;
        TMap<FName,float> Paths;
        TMap<FName,float> Props;
        float Score=0.0f;
        bool bContradicts=false;
        float TargetAlignment=0.0f;
        float PropertyFit=0.0f;
        float Coherence=0.0f;
    };
    const auto Simulate = [&](const ERefinementVerb Verb)
    {
        FSimulation S; S.Verb=Verb; S.Paths=CurrentPaths; S.Props=CurrentProperties;
        const FRefinementDirection Before=ResolveFormationDirection(CurrentPaths,CurrentProperties);
        const float BeforeTarget=TargetAlignment(Before),BeforeFit=TargetPropertyFit(CurrentProperties);
        ApplyVerbToProperties(S.Props,Verb,1.05f);
        const FRefinementDirection After=ResolveFormationDirection(S.Paths,S.Props);
        S.TargetAlignment=TargetAlignment(After); S.PropertyFit=TargetPropertyFit(S.Props); S.Coherence=After.Coherence;
        const float TargetLoss=FMath::Max(0.0f,BeforeTarget-S.TargetAlignment);
        const float PropertyLoss=FMath::Max(0.0f,BeforeFit-S.PropertyFit);
        const float CoherenceLoss=FMath::Max(0.0f,Before.Coherence-After.Coherence);
        const float TargetGain=FMath::Max(0.0f,S.TargetAlignment-BeforeTarget);
        const float PropertyGain=FMath::Max(0.0f,S.PropertyFit-BeforeFit);
        const float CoherenceGain=FMath::Max(0.0f,After.Coherence-Before.Coherence);
        S.bContradicts=TargetLoss>.10f||PropertyLoss>.09f||CoherenceLoss>.16f;
        S.Score=S.TargetAlignment*.34f+S.PropertyFit*.28f+After.Coherence*.23f+After.PropertyAlignment*.15f
            +TargetGain*.42f+PropertyGain*.34f+CoherenceGain*.24f-TargetLoss*.95f-PropertyLoss*.8f-CoherenceLoss*.55f;
        return S;
    };

    const auto MakeTemperature = [&](const ERefinementVerb Verb,const int32 Rank)
    {
        const int32 Jitter=FMath::RoundToInt((Random.FRand()-.5f)*8.0f);
        const int32 R=FMath::Max(1,Rank);
        switch(Verb)
        {
            case ERefinementVerb::Heat:return FVector2D(FMath::Max(35,52+R*2+Jitter),FMath::Max(62,82+R*2+Jitter));
            case ERefinementVerb::Cool:return FVector2D(20,FMath::Max(34,44+R+Jitter));
            case ERefinementVerb::Merge:return FVector2D(20,FMath::Max(58,70+R+Jitter));
            case ERefinementVerb::Purify:return FVector2D(20,FMath::Max(50,62+R+Jitter));
            case ERefinementVerb::Control:return FVector2D(20,FMath::Max(54,66+R+Jitter));
            case ERefinementVerb::Condense:return FVector2D(20,FMath::Max(52,64+R+Jitter));
            default:return FVector2D(20,FMath::Max(46,56+R+Jitter));
        }
    };
    const auto AddStep = [&](const ERefinementVerb Verb,const int32 Index)
    {
        FRefinementProcedureStep Step;
        Step.PrimaryProcess=Verb;
        Step.TargetPath=TargetPath;
        Step.TargetTemperature=MakeTemperature(Verb,Session.AttemptRank);
        const float Scale=1.0f+FMath::Max(0,Session.AttemptRank-1)*.11f;
        Step.RequiredProgress=FMath::RoundToFloat((38.0f+Index*13.0f+ComponentCount*2.0f)*Scale*(.9f+Random.FRand()*.2f));
        Step.MaxImpurities=FMath::RoundToFloat(24.0f+FMath::Min(10,Index*2)+Random.FRand()*5.0f);
        Step.ProcessWeights.Add(FName(*VerbName(Verb)),1.0f);
        // Coherent alternatives are deliberately hidden, but actions may still advance at partial affinity.
        for (const ERefinementVerb Candidate : {ERefinementVerb::Process,ERefinementVerb::Heat,ERefinementVerb::Cool,ERefinementVerb::Merge,ERefinementVerb::Purify,ERefinementVerb::Control})
        {
            if (Candidate==Verb) continue;
            FSimulation Sim=Simulate(Candidate);
            if (!Sim.bContradicts)
            {
                const float Weight=FMath::Clamp(.38f+Sim.TargetAlignment*.22f+Sim.PropertyFit*.2f+Sim.Coherence*.16f,.28f,.88f);
                if (Weight>=.58f) Step.ProcessWeights.Add(FName(*VerbName(Candidate)),Weight);
            }
        }
        if (Verb==ERefinementVerb::Condense)
        {
            Step.ProcessWeights.Reset(); Step.ProcessWeights.Add(TEXT("Condense"),1.0f); Step.ProcessWeights.Add(TEXT("Control"),.74f); Step.ProcessWeights.Add(TEXT("Purify"),.61f);
        }
        if (Index==0) Step.Name=TEXT("Prepare the Foundation");
        else if (Index==StageCount-1) Step.Name=TEXT("Condense the Gu Body");
        else
        {
            switch(Verb)
            {
                case ERefinementVerb::Heat:Step.Name=TEXT("Temper the Traces");break;
                case ERefinementVerb::Cool:Step.Name=TEXT("Quench the Reaction");break;
                case ERefinementVerb::Merge:Step.Name=TEXT("Reconcile the Components");break;
                case ERefinementVerb::Purify:Step.Name=TEXT("Separate Turbid Traces");break;
                case ERefinementVerb::Control:Step.Name=TEXT("Stabilize the Transformation");break;
                default:Step.Name=TEXT("Guide the Transformation");break;
            }
        }
        Step.SeedTag=FString::Printf(TEXT("%08x"),FCrc::StrCrc32(*FString::Printf(TEXT("%s:%d:%s"),*Signature,Index,*VerbName(Verb))));
        Step.bRecoveryWindow=Index<StageCount-1 && ResolveFormationDirection(CurrentPaths,CurrentProperties).Coherence>=.58f && Random.FRand()<(.18f+ResolveFormationDirection(CurrentPaths,CurrentProperties).Coherence*.28f);
        Session.HiddenProcedure.Add(Step);
        ApplyVerbToProperties(CurrentProperties,Verb,1.05f);
    };

    AddStep(ERefinementVerb::Process,0);
    ERefinementVerb Previous=ERefinementVerb::Process;
    for (int32 Index=1;Index<StageCount-1;++Index)
    {
        TArray<FSimulation> Sims;
        for (const ERefinementVerb Candidate : {ERefinementVerb::Heat,ERefinementVerb::Cool,ERefinementVerb::Merge,ERefinementVerb::Purify,ERefinementVerb::Control}) Sims.Add(Simulate(Candidate));
        Sims.Sort([&](const FSimulation& A,const FSimulation& B)
        {
            const float AJitter=(A.Verb==Previous?-0.1f:0.0f); const float BJitter=(B.Verb==Previous?-0.1f:0.0f);
            return A.Score+AJitter>B.Score+BJitter;
        });
        TArray<FSimulation> Pool;
        for (const FSimulation& Sim : Sims) if (!Sim.bContradicts && Sim.TargetAlignment>=.68f) Pool.Add(Sim);
        if (Pool.IsEmpty()) for (const FSimulation& Sim : Sims) if (!Sim.bContradicts) Pool.Add(Sim);
        if (Pool.IsEmpty()) for (const FSimulation& Sim : Sims) if (Sim.Verb==ERefinementVerb::Control||Sim.Verb==ERefinementVerb::Purify||Sim.Verb==ERefinementVerb::Merge) Pool.Add(Sim);
        const int32 PickCount=FMath::Min(3,Pool.Num());
        const int32 Pick=PickCount>1?Random.RandRange(0,PickCount-1):0;
        const ERefinementVerb Chosen=Pool.IsEmpty()?ERefinementVerb::Control:Pool[Pick].Verb;
        AddStep(Chosen,Index); Previous=Chosen;
    }
    AddStep(ERefinementVerb::Condense,StageCount-1);
}

float URefinementSubsystem::ProcedureAffinity(const FRefinementProcedureStep& Step,const ERefinementVerb Verb) const
{
    if (const float* Found=Step.ProcessWeights.Find(FName(*VerbName(Verb)))) return FMath::Clamp(*Found,0.0f,1.25f);
    return .28f;
}

void URefinementSubsystem::RebuildDivergentAnalysis(FRefinementSessionState& Session) const
{
    FRefinementAnalysis Analysis=Session.InitialAnalysis;
    Analysis.PathScores=Session.NascentPathScores;
    Analysis.PropertyScores=Session.NascentProperties;
    Analysis.AttributeScores=Session.NascentAttributes;
    AddScores(Analysis.AttributeScores,Session.ContaminationAttributes);
    Analysis.TraitScores=Session.NascentTraits;
    AddScores(Analysis.TraitScores,Session.ContaminationTraits);
    AddScores(Analysis.TraitScores,TraitsFromProperties(Session.NascentProperties),.28f);
    Analysis.ContaminationPaths=Session.ContaminationPaths;
    Analysis.ContaminationAttributes=Session.ContaminationAttributes;
    Analysis.ContaminationTraits=Session.ContaminationTraits;

    static const TMap<FName,TMap<FName,float>> ProcessInfluence={
        {TEXT("Process"),{{TEXT("precision"),.18f},{TEXT("efficiency"),.14f}}},{TEXT("Heat"),{{TEXT("amplification"),.26f},{TEXT("speed"),.08f}}},
        {TEXT("Cool"),{{TEXT("stability"),.24f},{TEXT("persistence"),.12f}}},{TEXT("Merge"),{{TEXT("persistence"),.18f},{TEXT("amplification"),.12f}}},
        {TEXT("Purify"),{{TEXT("precision"),.24f},{TEXT("stability"),.18f}}},{TEXT("Control"),{{TEXT("stability"),.28f},{TEXT("efficiency"),.12f}}},
        {TEXT("Condense"),{{TEXT("persistence"),.25f},{TEXT("stability"),.08f}}},
    };
    for (const TPair<FName,int32>& Pair : Session.ProcessCounts)
    {
        if (const TMap<FName,float>* Profile=ProcessInfluence.Find(Pair.Key)) AddScores(Analysis.AttributeScores,*Profile,FMath::Min(3,Pair.Value));
    }
    for (const TPair<FName,float>& Property : Analysis.PropertyScores)
    {
        if (const TMap<FName,float>* Profile=PropertyAttributes().Find(Property.Key)) AddScores(Analysis.AttributeScores,*Profile,FMath::Min(6.0f,Property.Value)*.38f);
    }
    const FRefinementDirection Direction=EffectiveDirection(Session);
    Analysis.NascentDirection=Direction; Analysis.PathScores=Direction.CombinedPathScores; Analysis.PrimaryPath=Direction.PrimaryPath; Analysis.PathCoherence=Direction.Coherence;
    const TArray<TPair<FName,float>> Paths=SortedScores(Analysis.PathScores);
    const float TopPath=Paths.IsEmpty()?1.0f:Paths[0].Value;
    Analysis.SecondaryPaths.Reset();
    for (int32 I=1;I<Paths.Num()&&Analysis.SecondaryPaths.Num()<2;++I) if (Paths[I].Value>=FMath::Max(.8f,TopPath*.46f)) Analysis.SecondaryPaths.Add(Paths[I].Key);
    const TArray<TPair<FName,float>> Attributes=SortedScores(Analysis.AttributeScores);
    const float TopAttribute=Attributes.IsEmpty()?0.0f:Attributes[0].Value,Threshold=FMath::Max(.72f,TopAttribute*.43f);
    const int32 MaxAttributes=FMath::Min(6,2+Analysis.HighestInputGuRank);
    Analysis.SurvivingAttributes.Reset();
    for (const TPair<FName,float>& Pair : Attributes) if (Pair.Value>=Threshold&&Analysis.SurvivingAttributes.Num()<MaxAttributes) Analysis.SurvivingAttributes.Add(Pair.Key);
    const TArray<TPair<FName,float>> Traits=SortedScores(Analysis.TraitScores);
    const float TopTrait=Traits.IsEmpty()?0.0f:Traits[0].Value,TraitThreshold=FMath::Max(.48f,TopTrait*.46f);
    Analysis.SurvivingTraits.Reset();
    for (const TPair<FName,float>& Pair : Traits) if (Pair.Value>=TraitThreshold&&Analysis.SurvivingTraits.Num()<4) Analysis.SurvivingTraits.Add(Pair.Key);
    FRefinementProcessHealth Health; Health.Fidelity=SessionFidelity(Session); Health.MaximumImpurities=Session.MaximumImpurities; Health.MaxStability=Session.MaxStability; Health.LowestStability=Session.LowestStability;
    Analysis.DaoMass=ResolveRetainedDaoMass(Analysis,Health);
    Analysis.ResultRank=UGuRulesLibrary::ExperimentalFormationRankFromRetainedDaoMass(Analysis.DaoMass.RetainedMass);
    Session.Outcome.Analysis=Analysis;
}

FRefinementOutcome URefinementSubsystem::ResolveOutcome(const FRefinementSessionState& Session) const
{
    FRefinementOutcome Result;
    Result.Fidelity=SessionFidelity(Session);
    FRefinementSessionState Mutable=Session;
    RebuildDivergentAnalysis(Mutable);
    const FRefinementAnalysis& Analysis=Mutable.Outcome.Analysis;
    Result.Analysis=Analysis;
    const FRefinementDirection Direction=EffectiveDirection(Session);
    Result.ResultPath=Direction.PrimaryPath;
    Result.ResultRank=Analysis.ResultRank;
    const int32 Rank=FMath::Max(1,Session.AttemptRank);
    const float MinFormation=FMath::Clamp(RefinementBalance::MinimumFormationCoherence+FMath::Max(0,Rank-1)*RefinementBalance::FormationCoherencePerRank,RefinementBalance::MinimumFormationCoherence,RefinementBalance::MaximumFormationCoherence);
    const float DivergentCoherence=FMath::Clamp(MinFormation+RefinementBalance::DivergentCoherenceOffset,RefinementBalance::MinimumDivergentCoherence,RefinementBalance::MaximumDivergentCoherence);
    if (Direction.PrimaryPath.IsNone()||Direction.Coherence<MinFormation||Direction.PropertyAlignment<RefinementBalance::MinimumPropertyAlignment)
    {
        Result.Kind=ERefinementOutcomeKind::Failure; Result.Reason=TEXT("directionless"); Result.Message=TEXT("The nascent properties never resolve into a clear Dao direction. The forming Gu disperses."); return Result;
    }
    if (!Session.bKnownRecipe||Session.IntendedDefinitionId.IsNone())
    {
        Result.Kind=ERefinementOutcomeKind::Experimental; Result.Message=TEXT("A coherent but unrecorded Gu structure forms."); return Result;
    }
    const TArray<TPair<FName,float>> SortedPaths=SortedScores(Direction.CombinedPathScores);
    const float TopScore=FMath::Max(.001f,SortedPaths.IsEmpty()?1.0f:SortedPaths[0].Value);
    const float IntendedScore=Direction.CombinedPathScores.FindRef(Session.IntendedPath);
    const float IntendedPathAlignment=FMath::Clamp(IntendedScore/TopScore,0.0f,1.0f);
    const float SemanticFit=FMath::Clamp(Direction.Coherence*.62f+IntendedPathAlignment*.38f,0.0f,1.0f);
    float Conflict=0.0f,CoherentForeign=0.0f;
    for (const TPair<FName,float>& Pair : Session.ContaminationPaths)
    {
        const float Amount=FMath::Max(0.0f,Pair.Value);
        if (Pair.Key==Session.IntendedPath||PathsCompatible(Pair.Key,Session.IntendedPath)) CoherentForeign+=Amount*RefinementBalance::CoherentForeignContribution;
        else Conflict+=Amount;
    }
    const float Tolerance=FMath::Clamp(RefinementBalance::ContaminationToleranceBase+1.0f*RefinementBalance::ContaminationControlWeight-FMath::Max(0,Rank-1)*RefinementBalance::ContaminationRankPenalty,RefinementBalance::ContaminationToleranceMin,RefinementBalance::ContaminationToleranceMax);
    const float Load=Conflict/FMath::Max(.01f,Tolerance);
    Result.ReliabilityEstimate=FMath::Clamp(RefinementBalance::ReliabilityBase+Result.Fidelity*RefinementBalance::ReliabilityFidelityWeight+SemanticFit*RefinementBalance::ReliabilitySemanticWeight-Load*RefinementBalance::ReliabilityContaminationPenalty,RefinementBalance::ReliabilityMin,RefinementBalance::ReliabilityMax);
    Result.ContaminationConflict=Conflict; Result.ContaminationTolerance=Tolerance;
    const float RequiredMass=DaoMassRequiredForRank(Rank),RetainedMass=Analysis.DaoMass.RetainedMass;
    const bool bIntendedStable=IntendedPathAlignment>=RefinementBalance::IntendedPathAlignment&&Direction.Coherence>=MinFormation&&Result.Fidelity>=RefinementBalance::MinimumFidelity&&Conflict<=Tolerance&&RetainedMass>=RequiredMass;
    if (bIntendedStable)
    {
        Result.Kind=ERefinementOutcomeKind::Intended; Result.ResultDefinitionId=Session.IntendedDefinitionId; Result.ResultPath=Session.IntendedPath; Result.ResultRank=Rank; Result.Message=TEXT("The intended Gu structure settles successfully."); return Result;
    }
    if (Direction.Coherence>=DivergentCoherence&&Direction.PropertyAlignment>=RefinementBalance::DivergentPropertyAlignment)
    {
        Result.Kind=ERefinementOutcomeKind::Divergent; Result.Message=TEXT("The altered structure coheres into a different Gu."); return Result;
    }
    Result.Kind=ERefinementOutcomeKind::Failure;
    if (Conflict>Tolerance) {Result.Reason=TEXT("contamination-collapse");Result.Message=TEXT("Foreign Dao marks overwhelm the forming structure, and no stable alternate Gu emerges.");}
    else if (RetainedMass<RequiredMass) {Result.Reason=TEXT("insufficient-retained-dao-mass");Result.Message=TEXT("Too much usable Dao structure is stripped away before condensation.");}
    else {Result.Reason=TEXT("incoherent-divergence");Result.Message=TEXT("The altered properties are not coherent enough to become another Gu.");}
    return Result;
}


FName URefinementSubsystem::ResolveExperimentalTemplate(const FRefinementAnalysis& Analysis) const
{
    static const TSet<FName> Executable = {
        TEXT("projectile"),TEXT("melee"),TEXT("area"),TEXT("shield"),TEXT("movement"),TEXT("heal"),
        TEXT("conceal"),TEXT("reveal"),TEXT("buff"),TEXT("amplifier"),TEXT("restriction"),TEXT("refinement")
    };
    TMap<FName,float> Scores;
    const auto Add = [&](const FName Key,const float Value)
    {
        const FName Normalized(*Key.ToString().ToLower());
        if (Executable.Contains(Normalized) && Value > 0.0f) Scores.FindOrAdd(Normalized) += Value;
    };
    for (const TPair<FName,float>& Pair : Analysis.TemplateScores) Add(Pair.Key,FMath::Max(0.0f,Pair.Value)*2.25f);

    TSet<FName> Surviving;
    for (const FName Attr : Analysis.SurvivingAttributes) Surviving.Add(FName(*Attr.ToString().ToLower()));
    const auto Attr = [&](const TCHAR* Name)
    {
        const FName Key(Name);
        return FMath::Max(0.0f,Analysis.AttributeScores.FindRef(Key)) * (Surviving.Contains(Key) ? 1.35f : .45f);
    };
    Add(TEXT("movement"),Attr(TEXT("speed"))*1.7f);
    Add(TEXT("area"),Attr(TEXT("area"))*1.7f);
    Add(TEXT("conceal"),Attr(TEXT("concealment"))*1.8f);
    Add(TEXT("heal"),Attr(TEXT("recovery"))*1.7f);
    Add(TEXT("restriction"),Attr(TEXT("suppression"))*1.65f);
    Add(TEXT("reveal"),Attr(TEXT("tracking"))*1.05f+Attr(TEXT("precision"))*.75f);
    Add(TEXT("projectile"),Attr(TEXT("range"))*.7f+Attr(TEXT("tracking"))*.35f+Attr(TEXT("penetration"))*.35f);
    Add(TEXT("melee"),Attr(TEXT("penetration"))*.8f+Attr(TEXT("bleed"))*.5f);
    Add(TEXT("amplifier"),Attr(TEXT("amplification"))*1.15f+Attr(TEXT("efficiency"))*.9f);
    Add(TEXT("shield"),Attr(TEXT("stability"))*1.15f+Attr(TEXT("persistence"))*.45f);
    Add(TEXT("area"),Attr(TEXT("poison"))*.45f+Attr(TEXT("bleed"))*.2f);

    const auto Prop = [&](const TCHAR* Name){return FMath::Max(0.0f,Analysis.PropertyScores.FindRef(FName(Name)));};
    Add(TEXT("movement"),Prop(TEXT("motion"))*.52f+Prop(TEXT("flow"))*.38f+Prop(TEXT("vapor"))*.28f);
    Add(TEXT("area"),Prop(TEXT("expansion"))*.58f+Prop(TEXT("vapor"))*.22f+Prop(TEXT("corrosion"))*.22f);
    Add(TEXT("shield"),Prop(TEXT("stability"))*.58f+Prop(TEXT("hardness"))*.5f+Prop(TEXT("solid"))*.32f+Prop(TEXT("stillness"))*.15f);
    Add(TEXT("heal"),Prop(TEXT("vitality"))*.62f+Prop(TEXT("growth"))*.5f);
    Add(TEXT("conceal"),Prop(TEXT("concealment"))*.68f+Prop(TEXT("stillness"))*.16f);
    Add(TEXT("reveal"),Prop(TEXT("precision"))*.45f+Prop(TEXT("luminosity"))*.24f+Prop(TEXT("link"))*.12f);
    Add(TEXT("restriction"),Prop(TEXT("control"))*.52f+Prop(TEXT("stillness"))*.25f+Prop(TEXT("cold"))*.18f);
    Add(TEXT("amplifier"),Prop(TEXT("force"))*.48f+Prop(TEXT("assimilation"))*.52f+Prop(TEXT("adaptability"))*.24f);
    Add(TEXT("melee"),Prop(TEXT("sharpness"))*.5f+Prop(TEXT("force"))*.22f+Prop(TEXT("adhesion"))*.12f);
    Add(TEXT("projectile"),Prop(TEXT("precision"))*.2f+Prop(TEXT("flow"))*.16f+Prop(TEXT("sharpness"))*.2f);
    if (Analysis.PrimaryPath == TEXT("Refinement")) Add(TEXT("refinement"),(Prop(TEXT("precision"))+Prop(TEXT("stability"))+Prop(TEXT("adhesion")))*.55f);

    const TArray<TPair<FName,float>> Sorted = SortedScores(Scores);
    return Sorted.IsEmpty() ? FName(TEXT("buff")) : Sorted[0].Key;
}

namespace
{
    FString RefinementBase36(uint32 Value)
    {
        static const TCHAR Digits[] = TEXT("0123456789abcdefghijklmnopqrstuvwxyz");
        if (Value == 0) return TEXT("0");
        FString Out;
        while (Value > 0)
        {
            Out.InsertAt(0,Digits[Value % 36]);
            Value /= 36;
        }
        return Out;
    }

    FString RefinementJsonStringArray(const TArray<FString>& Values)
    {
        if(Values.IsEmpty()) return TEXT("[]");
        return FString::Printf(TEXT("[\"%s\"]"),*FString::Join(Values,TEXT("\",\"")));
    }

    FString RefinementExperimentalSignature(const FRefinementAnalysis& Analysis,const FName Template)
    {
        TArray<FString> Secondary,Attrs,Traits;
        for (const FName V : Analysis.SecondaryPaths) Secondary.Add(V.ToString());
        for (const FName V : Analysis.SurvivingAttributes) Attrs.Add(V.ToString());
        for (const FName V : Analysis.SurvivingTraits) Traits.Add(V.ToString());
        Secondary.Sort(); Attrs.Sort(); Traits.Sort();
        // Mirrors JSON.stringify({path, secondaryPaths, attrs, traits, template, rank}) in v7.9.25.
        const FString Json = FString::Printf(TEXT("{\"path\":\"%s\",\"secondaryPaths\":%s,\"attrs\":%s,\"traits\":%s,\"template\":\"%s\",\"rank\":%d}"),
            *Analysis.PrimaryPath.ToString(),*RefinementJsonStringArray(Secondary),*RefinementJsonStringArray(Attrs),*RefinementJsonStringArray(Traits),*Template.ToString(),Analysis.ResultRank);
        uint32 Hash=2166136261u;
        for (const TCHAR Ch : Json) { Hash ^= static_cast<uint32>(Ch); Hash *= 16777619u; }
        return RefinementBase36(Hash);
    }

    FString RefinementAttributeWord(const FName Attribute)
    {
        static const TMap<FName,FString> Words = {
            {TEXT("amplification"),TEXT("Mighty")},{TEXT("range"),TEXT("Far-Reaching")},{TEXT("area"),TEXT("Expanding")},
            {TEXT("speed"),TEXT("Swift")},{TEXT("duration"),TEXT("Lasting")},{TEXT("precision"),TEXT("Precise")},
            {TEXT("persistence"),TEXT("Enduring")},{TEXT("tracking"),TEXT("Seeking")},{TEXT("penetration"),TEXT("Piercing")},
            {TEXT("stability"),TEXT("Steady")},{TEXT("efficiency"),TEXT("Frugal")},{TEXT("concealment"),TEXT("Hidden")},
            {TEXT("suppression"),TEXT("Binding")},{TEXT("bleed"),TEXT("Bloodletting")},{TEXT("poison"),TEXT("Venomous")},
            {TEXT("timed"),TEXT("Delayed")},{TEXT("recovery"),TEXT("Vital")},{TEXT("link"),TEXT("Linked")}
        };
        return Words.FindRef(FName(*Attribute.ToString().ToLower()));
    }

    FString RefinementTemplateNoun(const FName Template)
    {
        static const TMap<FName,FString> Words = {
            {TEXT("projectile"),TEXT("Arrow")},{TEXT("melee"),TEXT("Blade")},{TEXT("area"),TEXT("Wave")},
            {TEXT("shield"),TEXT("Guard")},{TEXT("movement"),TEXT("Step")},{TEXT("heal"),TEXT("Vitality")},
            {TEXT("conceal"),TEXT("Veil")},{TEXT("reveal"),TEXT("Eye")},{TEXT("buff"),TEXT("Strength")},
            {TEXT("amplifier"),TEXT("Force")},{TEXT("restriction"),TEXT("Binding")},{TEXT("refinement"),TEXT("Cauldron")}
        };
        if (const FString* Found=Words.Find(FName(*Template.ToString().ToLower()))) return *Found;
        return TEXT("Form");
    }

    bool RefinementHasName(const TArray<FName>& Values,const TCHAR* Name)
    {
        const FName Key(Name);
        for (const FName V : Values) if (FName(*V.ToString().ToLower()) == Key) return true;
        return false;
    }
}

FGuDefinitionRecord URefinementSubsystem::BuildExperimentalDefinition(const FRefinementSessionState& Session) const
{
    FRefinementAnalysis Analysis = Session.Outcome.Analysis;
    const FName Template = ResolveExperimentalTemplate(Analysis);
    Analysis.Template = Template;
    const FString Signature = RefinementExperimentalSignature(Analysis,Template);
    const FName Id(*FString::Printf(TEXT("experimental_%s"),*Signature));

    UGuDefinitionRegistrySubsystem* Registry = GetGameInstance()?GetGameInstance()->GetSubsystem<UGuDefinitionRegistrySubsystem>():nullptr;
    if (Registry)
    {
        FGuDefinitionRecord Existing;
        if (Registry->GetDefinition(Id,Existing)) return Existing;
    }

    FGuDefinitionRecord Definition;
    Definition.Id=Id;
    Definition.Rank=FMath::Clamp(Analysis.ResultRank,1,9);
    Definition.Kind=Definition.Rank>=6?EGuKind::Immortal:EGuKind::Mortal;
    Definition.Path=Analysis.PrimaryPath;
    Definition.SecondaryPaths=Analysis.SecondaryPaths;
    Definition.PathRelation=Analysis.SecondaryPaths.IsEmpty()?TEXT("Pure-path Gu"):TEXT("Multi-path Gu");
    Definition.bCustom=true;

    const FString StrongestAttribute = Analysis.SurvivingAttributes.IsEmpty()?TEXT(""):RefinementAttributeWord(Analysis.SurvivingAttributes[0]);
    const FString SecondaryAttribute = Analysis.SurvivingAttributes.Num()>1?RefinementAttributeWord(Analysis.SurvivingAttributes[1]):TEXT("");
    const FString PathWord = Analysis.PrimaryPath.IsNone()?TEXT("Nameless"):Analysis.PrimaryPath.ToString();
    const FString Noun=RefinementTemplateNoun(Template);
    const uint32 NamePattern = FCrc::StrCrc32(*FString::Printf(TEXT("name|%s"),*Signature)) % 5u;
    FString BaseName;
    if(StrongestAttribute.IsEmpty())
    {
        BaseName=FString::Printf(TEXT("%s %s Gu"),*PathWord,PathWord.Equals(Noun,ESearchCase::IgnoreCase)?TEXT("Essence"):*Noun);
    }
    else if(NamePattern==0u)
    {
        BaseName=FString::Printf(TEXT("%s %s Gu"),*StrongestAttribute,*Noun);
    }
    else if(NamePattern==1u)
    {
        BaseName=FString::Printf(TEXT("%s %s Gu"),*PathWord,*Noun);
    }
    else if(NamePattern==2u)
    {
        BaseName=FString::Printf(TEXT("%s %s %s Gu"),*StrongestAttribute,*PathWord,*Noun);
    }
    else if(NamePattern==3u && !SecondaryAttribute.IsEmpty())
    {
        BaseName=FString::Printf(TEXT("%s %s %s Gu"),*StrongestAttribute,*SecondaryAttribute,*Noun);
    }
    else
    {
        BaseName=FString::Printf(TEXT("%s %s %s Gu"),*PathWord,*StrongestAttribute,*Noun);
    }
    BaseName.ReplaceInline(TEXT("  "),TEXT(" "));
    if (Registry && Registry->HasDefinition(FName(*BaseName)))
    {
        const FString ExtraWord=!SecondaryAttribute.IsEmpty()?SecondaryAttribute:PathWord;
        const FString Expanded=FString::Printf(TEXT("%s %s Gu"),*BaseName.Replace(TEXT(" Gu"),TEXT("")),*ExtraWord);
        BaseName=Registry->HasDefinition(FName(*Expanded))
            ? FString::Printf(TEXT("%s %s Gu"),*BaseName.Replace(TEXT(" Gu"),TEXT("")),*Signature.Left(4).ToUpper())
            : Expanded;
    }
    Definition.Name=BaseName.Left(80);

    static const TMap<FName,FName> Categories = {
        {TEXT("projectile"),TEXT("Attack")},{TEXT("melee"),TEXT("Attack")},{TEXT("area"),TEXT("Attack")},
        {TEXT("shield"),TEXT("Defense")},{TEXT("movement"),TEXT("Movement")},{TEXT("heal"),TEXT("Healing")},
        {TEXT("conceal"),TEXT("Concealment")},{TEXT("reveal"),TEXT("Investigation")},{TEXT("buff"),TEXT("Support")},
        {TEXT("amplifier"),TEXT("Support")},{TEXT("restriction"),TEXT("Control")},{TEXT("refinement"),TEXT("Refinement")}
    };
    Definition.Category=Categories.FindRef(Template); if(Definition.Category.IsNone())Definition.Category=TEXT("Support");
    Definition.FunctionalRoles={Definition.Category};
    Definition.Description=FString::Printf(TEXT("An experimentally refined Rank %d %s-path Gu. Its final nature emerged from the selected refinement materials rather than a known recipe."),Definition.Rank,*Definition.Path.ToString());
    Definition.EssenceCostMode=EGuEssenceCostMode::PercentOfOwnRankTheoreticalAperture;
    Definition.EssenceCost=FMath::Min(35.0f,8.0f+Definition.Rank*2.0f+Analysis.SurvivingAttributes.Num()*.6f);

    Definition.RefinementTraits=Analysis.SurvivingTraits;
    const bool bCharged=RefinementHasName(Analysis.SurvivingTraits,TEXT("charged"));
    const bool bMaintained=RefinementHasName(Analysis.SurvivingTraits,TEXT("maintained"));
    const bool bTrigger=RefinementHasName(Analysis.SurvivingTraits,TEXT("trigger"));
    const bool bPrepared=RefinementHasName(Analysis.SurvivingTraits,TEXT("prepared"));
    Definition.ActivationModel=bCharged?EGuActivationModel::StoredCharged:bMaintained?EGuActivationModel::Maintained:bTrigger?EGuActivationModel::Trigger:bPrepared?EGuActivationModel::PreparedMark:EGuActivationModel::Instant;
    Definition.Lifecycle.bConsumable=RefinementHasName(Analysis.SurvivingTraits,TEXT("consumable"));
    Definition.Lifecycle.ConsumeOn=EGuConsumeOn::SuccessfulActivation;
    Definition.Lifecycle.Charges=1;
    Definition.Lifecycle.ConsumedForm=TEXT("The Gu expends its complete body to release the stored effect.");

    const bool bTargeted=Template==TEXT("projectile")||Template==TEXT("melee")||Template==TEXT("area")||Template==TEXT("restriction");
    Definition.IntrinsicConstraints.bStationary=RefinementHasName(Analysis.SurvivingTraits,TEXT("stationary"));
    Definition.IntrinsicConstraints.bContact=bTargeted&&RefinementHasName(Analysis.SurvivingTraits,TEXT("contact"));
    Definition.IntrinsicConstraints.ContactRange=Definition.IntrinsicConstraints.bContact?70.0f:0.0f;
    Definition.IntrinsicConstraints.SelfCostLifePercent=RefinementHasName(Analysis.SurvivingTraits,TEXT("self_cost"))?.08f:0.0f;
    Definition.IntrinsicConstraints.bShortLived=RefinementHasName(Analysis.SurvivingTraits,TEXT("short_lived"));
    Definition.IntrinsicConstraints.PrepareMs=bCharged?(2200+Definition.Rank*350):bPrepared?(1400+Definition.Rank*220):Definition.IntrinsicConstraints.bStationary?900:0;

    const FRefinementPowerAllocation Power=ResolvePowerAllocation(Definition.Rank,Template,Analysis.SurvivingAttributes,Analysis.SurvivingTraits);
    Definition.PowerProfile.BaseBudget=Power.BaseBudget;
    Definition.PowerProfile.EffectiveBudget=Power.EffectiveBudget;
    Definition.PowerProfile.ConstraintMultiplier=Power.ConstraintMultiplier;
    Definition.PowerProfile.Allocation=Power.Allocation;

    Definition.EffectProfile.CoreEffect=FString::Printf(TEXT("Condenses the selected materials into a %s-path %s manifestation."),*Definition.Path.ToString(),*Noun.ToLower());
    Definition.EffectProfile.Input=TEXT("Primeval essence");
    Definition.EffectProfile.Carrier=Noun;
    Definition.EffectProfile.Operation=TEXT("Activates the refined effect preserved by the successful experimental refinement.");
    Definition.EffectProfile.TargetLink=bTargeted?TEXT("Selected target or aim point"):TEXT("Self");
    Definition.EffectProfile.Manifestation=Noun;
    Definition.EffectProfile.Magnitude=Power.Magnitude;
    Definition.EffectProfile.Range=bTargeted?Power.Range:0.0f;
    Definition.EffectProfile.Area=(Template==TEXT("area")||RefinementHasName(Analysis.SurvivingAttributes,TEXT("area")))?Power.Area:0.0f;
    Definition.EffectProfile.DurationMs=FMath::RoundToInt(Power.DurationMs);
    Definition.EffectProfile.OtherCost=Definition.IntrinsicConstraints.SelfCostLifePercent>0.0f?TEXT("Activation strains or consumes part of the user/source according to the Gu formation."):TEXT("");
    Definition.EffectProfile.ValidTargets=bTargeted?TArray<FName>{TEXT("Enemy"),TEXT("Area")}:TArray<FName>{TEXT("Self")};
    Definition.EffectProfile.Failure=TEXT("The refined effect fails to form cleanly.");
    Definition.EffectProfile.Trace=FString::Printf(TEXT("Faint %s-path traces linger after activation."),*Definition.Path.ToString());

    FGuMechanicSpec Mechanic;
    if(Template==TEXT("projectile"))Mechanic.Type=TEXT("custom_projectile");
    else if(Template==TEXT("melee"))Mechanic.Type=TEXT("custom_melee");
    else if(Template==TEXT("area")||Template==TEXT("restriction"))Mechanic.Type=TEXT("custom_area");
    else if(Template==TEXT("shield"))Mechanic.Type=TEXT("custom_shield");
    else if(Template==TEXT("movement"))Mechanic.Type=TEXT("custom_movement");
    else if(Template==TEXT("heal"))Mechanic.Type=TEXT("custom_heal");
    else if(Template==TEXT("conceal"))Mechanic.Type=TEXT("custom_concealment");
    else if(Template==TEXT("reveal"))Mechanic.Type=TEXT("custom_reveal");
    else if(Template==TEXT("refinement"))Mechanic.Type=TEXT("refinement_assistance");
    else Mechanic.Type=TEXT("stat_modifier");
    Mechanic.ConfigJson=FString::Printf(TEXT("{\"magnitude\":%.3f,\"range\":%.3f,\"area\":%.3f,\"durationMs\":%d,\"speedMultiplier\":%.4f}"),Power.Magnitude,Power.Range,Power.Area,FMath::RoundToInt(Power.DurationMs),Power.SpeedMultiplier);
    Definition.Mechanics.Add(Mechanic);
    if(Template==TEXT("refinement"))
    {
        Definition.RefinementAssistance.bEnabled=true;
        Definition.RefinementAssistance.ProgressPercent=FMath::Max(4.0f,Definition.Rank*4.0f);
        Definition.RefinementAssistance.StabilityPerAction=FMath::Max(1.0f,Definition.Rank*.8f);
        Definition.RefinementAssistance.ImpurityReductionPerAction=FMath::Max(0.0f,FMath::RoundToFloat(Definition.Rank*.6f));
        Definition.RefinementAssistance.ActionUses=FMath::Max(2,Definition.Rank+1);
        Definition.RefinementAssistance.Processes={TEXT("Process"),TEXT("Merge"),TEXT("Purify"),TEXT("Control"),TEXT("Condense")};
    }

    Definition.RefinementProfile.Paths=Analysis.PathScores;
    Definition.RefinementProfile.Properties=Analysis.PropertyScores;
    Definition.RefinementProfile.Attributes=Analysis.AttributeScores;
    Definition.RefinementProfile.Traits=Analysis.TraitScores;
    Definition.RefinementProfile.Templates.Reset(); Definition.RefinementProfile.Templates.Add(Template,1.0f);
    Definition.RefinementProfile.DaoMass=FMath::Max(.01f,Analysis.DaoMass.RetainedMass);

    if(!Session.InitialAnalysis.FoundationDefinitionId.IsNone() && Registry)
    {
        if(const FGuDefinitionRecord* Foundation=Registry->FindDefinition(Session.InitialAnalysis.FoundationDefinitionId))
        {
            Definition.Feeding=Foundation->Feeding;
            Definition.Appearance=Foundation->Appearance;
        }
    }
    Definition.Appearance.Seed=static_cast<int32>(FCrc::StrCrc32(*Signature));
    const int32 SemanticComplexity = FMath::Clamp(
        1 + (Analysis.SurvivingAttributes.Num() + Analysis.SurvivingTraits.Num()) / 3,
        1,
        5);
    Definition.Tags={
        TEXT("experimental-refinement"),
        TEXT("compiler:semantic-v2"),
        FName(*FString::Printf(TEXT("complexity:%d"), SemanticComplexity))
    };
    for(const FName Attr : Analysis.SurvivingAttributes)Definition.Tags.Add(Attr);
    for(const FName Trait : Analysis.SurvivingTraits)Definition.Tags.Add(FName(*FString::Printf(TEXT("trait:%s"),*Trait.ToString())));
    Definition.Source=FString::Printf(TEXT("Experimental refinement profile v%d"),Analysis.ProfileVersion);
    Definition.bHasRefinementOrigin=true;
    Definition.RefinementOrigin.ProfileVersion=Analysis.ProfileVersion;
    Definition.RefinementOrigin.Foundation=Session.InitialAnalysis.FoundationDefinitionId;
    Definition.RefinementOrigin.SourceSignature=Signature;
    TArray<FString> IngredientJson;
    for(const FRefinementCommittedInput& Input:Session.CommittedInputs)
    {
        FString Name=Input.SourceId.ToString();
        Name.ReplaceInline(TEXT("\\"),TEXT("\\\\"));
        Name.ReplaceInline(TEXT("\""),TEXT("\\\""));
        const TCHAR* Kind=Input.Kind==static_cast<uint8>(ERefinableKind::Gu)?TEXT("gu"):TEXT("material");
        IngredientJson.Add(FString::Printf(TEXT("{\"kind\":\"%s\",\"name\":\"%s\",\"quantity\":%d,\"foundation\":%s}"),Kind,*Name,FMath::Max(1,Input.Quantity),Input.bFoundation?TEXT("true"):TEXT("false")));
    }
    Definition.RefinementOrigin.IngredientsJson=FString::Printf(TEXT("[%s]"),*FString::Join(IngredientJson,TEXT(",")));
    return Definition;
}

FRefinementFailureRisk URefinementSubsystem::FailureRiskForRank(const int32 Rank) const
{
    const int32 OverRank=FMath::Max(0,Rank-2);
    FRefinementFailureRisk Risk;
    Risk.Destroy=.25f+OverRank*.08f;
    Risk.Damage=.35f+OverRank*.04f;
    constexpr float MinUnharmed=.05f;
    const float HarmCap=1.0f-MinUnharmed;
    if(Risk.Destroy+Risk.Damage>HarmCap)
    {
        const float Scale=HarmCap/(Risk.Destroy+Risk.Damage);
        Risk.Destroy*=Scale;
        Risk.Damage*=Scale;
    }
    Risk.Unharmed=FMath::Max(MinUnharmed,1.0f-Risk.Destroy-Risk.Damage);
    // Match browser arithmetic, where damage is recomputed from the clamped unharmed share.
    Risk.Damage=1.0f-Risk.Destroy-Risk.Unharmed;
    return Risk;
}

bool URefinementSubsystem::IsInputReserved(const FGuid EntityId) const
{
    return ReservedInputEntities.Contains(EntityId);
}

void URefinementSubsystem::ReleaseInputReservations(const FRefinementSessionState& Session)
{
    for(const FGuid EntityId : Session.InputEntityIds) ReservedInputEntities.Remove(EntityId);
}

void URefinementSubsystem::ConsumeCommittedInputs(FRefinementSessionState& Session)
{
    if(Session.bInputsConsumed) return;
    if(UGuEntitySubsystem* Entities=GetGameInstance()?GetGameInstance()->GetSubsystem<UGuEntitySubsystem>():nullptr)
    {
        for(const FRefinementCommittedInput& Input:Session.CommittedInputs)
        {
            if(Entities->GetMaterialLot(Input.EntityId))
            {
                FString Ignore;Entities->ConsumeMaterialQuantity(Input.EntityId,Input.Quantity,Ignore);
            }
            else Entities->DestroyEntity(Input.EntityId);
        }
    }
    Session.bInputsConsumed=true;
    ReleaseInputReservations(Session);
}

void URefinementSubsystem::ResolveFailedCommittedInputs(FRefinementSessionState& Session)
{
    if(Session.bInputsConsumed) return;
    UGameInstance* GI=GetGameInstance();
    UGuEntitySubsystem* Entities=GI?GI->GetSubsystem<UGuEntitySubsystem>():nullptr;
    UGuDefinitionRegistrySubsystem* Registry=GI?GI->GetSubsystem<UGuDefinitionRegistrySubsystem>():nullptr;
    if(!Entities)
    {
        Session.bInputsConsumed=true;
        ReleaseInputReservations(Session);
        return;
    }

    const FRefinementFailureRisk Risk=FailureRiskForRank(Session.AttemptRank);
    const float DestroyChance=Risk.Destroy;
    const float DamageChance=Risk.Damage;

    FRandomStream Random(static_cast<int32>(HashCombine(GetTypeHash(Session.SessionId),GetTypeHash(Session.StartedAtUnixMs))));
    TArray<FString> Destroyed;
    TArray<FString> Damaged;
    int32 UnharmedCount=0;

    for(const FRefinementCommittedInput& Input:Session.CommittedInputs)
    {
        // Mortal materials are staked completely. Acquisition lots lose only the selected quantity.
        if(Entities->GetMaterialLot(Input.EntityId))
        {
            FString Ignore;
            Entities->ConsumeMaterialQuantity(Input.EntityId,Input.Quantity,Ignore);
            continue;
        }

        const bool bGu=Entities->GetGuInstance(Input.EntityId)!=nullptr || Input.Kind==static_cast<uint8>(ERefinableKind::Gu);
        if(!bGu)
        {
            Entities->DestroyEntity(Input.EntityId);
            continue;
        }

        FString DisplayName=Input.DefinitionId.ToString();
        if(Registry)
        {
            if(const FGuDefinitionRecord* Definition=Registry->FindDefinition(Input.DefinitionId)) DisplayName=Definition->Name;
        }
        const float Roll=Random.FRand();
        if(Roll<DestroyChance)
        {
            Entities->DestroyEntity(Input.EntityId);
            Destroyed.Add(DisplayName);
        }
        else if(Roll<DestroyChance+DamageChance)
        {
            if(FGuNourishmentComponent* Nourishment=Entities->GetMutableGuNourishment(Input.EntityId)) Nourishment->Hunger=FMath::Max(0.0f,Nourishment->Hunger-55.0f);
            Damaged.Add(DisplayName);
        }
        else
        {
            ++UnharmedCount;
        }
    }

    if(!Destroyed.IsEmpty()) AddLog(Session,FString::Printf(TEXT("Destroyed: %s."),*FString::Join(Destroyed,TEXT(", "))));
    if(!Damaged.IsEmpty()) AddLog(Session,FString::Printf(TEXT("Damaged: %s."),*FString::Join(Damaged,TEXT(", "))));
    if(Destroyed.IsEmpty()&&Damaged.IsEmpty()&&UnharmedCount>0) AddLog(Session,TEXT("The committed Gu survive the collapse unharmed."));

    Session.bInputsConsumed=true;
    ReleaseInputReservations(Session);
}

bool URefinementSubsystem::SecureSuccessfulOutcome(AGuPlayerState* PlayerState,FRefinementSessionState& Session,FString& OutError)
{
    if(!PlayerState||!Session.bSuccess){OutError=TEXT("Cannot secure a failed refinement.");return false;}
    UGameInstance* GI=GetGameInstance();
    UGuEntitySubsystem* Entities=GI?GI->GetSubsystem<UGuEntitySubsystem>():nullptr;
    UGuDefinitionRegistrySubsystem* Registry=GI?GI->GetSubsystem<UGuDefinitionRegistrySubsystem>():nullptr;
    UGuProceduralGeneratorSubsystem* Generator=GI?GI->GetSubsystem<UGuProceduralGeneratorSubsystem>():nullptr;
    if(!Entities||!Registry){OutError=TEXT("Gu domain systems are unavailable while securing the refinement.");return false;}

    FName ResultDefinitionId=Session.Outcome.ResultDefinitionId;
    if(Session.Outcome.Kind==ERefinementOutcomeKind::Divergent||Session.Outcome.Kind==ERefinementOutcomeKind::Experimental)
    {
        FGuDefinitionRecord Definition=BuildExperimentalDefinition(Session);
        ResultDefinitionId=Definition.Id;

        // Experimental/refinement-created species must be executable UGuDefinitions, not record-only ECS metadata.
        if(!Registry->FindDefinitionAsset(ResultDefinitionId))
        {
            if(!Generator)
            {
                OutError=TEXT("Procedural Gu compiler is unavailable while materializing the refined Gu definition.");
                return false;
            }
            UGuDefinition* RuntimeDefinition=nullptr;
            FString CompileError;
            FName CanonicalDefinitionId=ResultDefinitionId;
            if(!Generator->CompileAndRegisterRuntimeRecord(Definition,RuntimeDefinition,CompileError,true,&CanonicalDefinitionId))
            {
                OutError=FString::Printf(TEXT("The refined Gu formed semantically but could not become executable: %s"),*CompileError);
                return false;
            }
            ResultDefinitionId=CanonicalDefinitionId;
        }

        Session.Outcome.ResultDefinitionId=ResultDefinitionId;
        Session.Outcome.ResultPath=Definition.Path;
        Session.Outcome.ResultRank=Definition.Rank;
    }
    if(ResultDefinitionId.IsNone()||!Registry->HasDefinition(ResultDefinitionId)){OutError=TEXT("Successful refinement resolved no valid Gu definition.");return false;}

    // Upgrade any older runtime record-only definition on demand as it enters active gameplay.
    if(!Registry->FindDefinitionAsset(ResultDefinitionId))
    {
        const FGuDefinitionRecord* RuntimeRecord=Registry->FindDefinition(ResultDefinitionId);
        if(RuntimeRecord&&Generator)
        {
            UGuDefinition* RuntimeDefinition=nullptr;
            FString CompileError;
            FName CanonicalDefinitionId=ResultDefinitionId;
            if(!Generator->CompileAndRegisterRuntimeRecord(*RuntimeRecord,RuntimeDefinition,CompileError,true,&CanonicalDefinitionId))
            {
                OutError=FString::Printf(TEXT("The Gu definition exists in the domain registry but has no executable mechanic definition: %s"),*CompileError);
                return false;
            }
            ResultDefinitionId=CanonicalDefinitionId;
            Session.Outcome.ResultDefinitionId=CanonicalDefinitionId;
        }
    }
    if(!Registry->FindDefinitionAsset(ResultDefinitionId))
    {
        OutError=TEXT("Successful refinement resolved a Gu record, but no executable UGuDefinition exists for it.");
        return false;
    }

    // Inputs are destroyed immediately before the result is minted, so a failed registration never eats materials.
    ConsumeCommittedInputs(Session);
    const FGuid ResultEntity=Entities->CreateGuInstance(ResultDefinitionId,PlayerState->DomainCharacterId,EGuContainer::Aperture);
    if(!ResultEntity.IsValid())
    {
        OutError=TEXT("The Gu definition formed, but its physical instance could not be created.");
        return false;
    }
    Session.Outcome.ResultEntityId=ResultEntity;

    // If the refiner currently has a playable character, bind the new physical worm to the same generic Gu GAS ability immediately.
    if(AGu_Daoist_MasterCharacter* Character=Cast<AGu_Daoist_MasterCharacter>(PlayerState->GetPawn()))
    {
        if(UGuDefinition* ExecutableDefinition=const_cast<UGuDefinition*>(Registry->FindDefinitionAsset(ResultDefinitionId)))
        {
            FGameplayAbilitySpecHandle GrantedHandle;
            FString GrantError;
            if(!Character->GrantGuAbilityForEntity(ResultEntity,ExecutableDefinition,GrantedHandle,GrantError))
            {
                UE_LOG(LogTemp,Warning,TEXT("Refined Gu %s is physical and executable, but its runtime GAS binding was deferred: %s"),*ResultDefinitionId.ToString(),*GrantError);
            }
        }
    }

    if(FGuConditionComponent* Condition=Entities->GetMutableGuCondition(ResultEntity))
    {
        const float Score=Session.Stability-Session.Impurities+(Session.Focus/FMath::Max(1.0f,Session.MaxFocus))*10.0f+Session.AssistanceQualityBonus-ContaminationTotal(Session)*2.0f;
        Condition->Quality=Score>=90.0f?1.0f:Score>=55.0f?.82f:.62f;
        Condition->Durability=100.0f*Condition->Quality;
    }
    OutError.Reset();
    return true;
}

bool URefinementSubsystem::BeginRefinementSession(AGuPlayerState* PlayerState,const TArray<FGuid>& EntityIds,const FName IntendedDefinitionId,const bool bKnownRecipe,FString& OutError)
{
    TArray<FRefinementInputSelection> Inputs;
    Inputs.Reserve(EntityIds.Num());
    for(const FGuid EntityId:EntityIds){FRefinementInputSelection Selection;Selection.EntityId=EntityId;Selection.Quantity=1;Inputs.Add(Selection);}
    return BeginRefinementSessionWithQuantities(PlayerState,Inputs,IntendedDefinitionId,bKnownRecipe,OutError);
}

bool URefinementSubsystem::BeginRefinementSessionWithQuantities(AGuPlayerState* PlayerState,const TArray<FRefinementInputSelection>& Inputs,const FName IntendedDefinitionId,const bool bKnownRecipe,FString& OutError)
{
    if (!PlayerState||!PlayerState->HasAuthority()) {OutError=TEXT("Refinement sessions are server-authoritative.");return false;}
    const FString OwnerId=PlayerState->DomainCharacterId;
    if (OwnerId.IsEmpty()) {OutError=TEXT("Player has no persistent domain character ID.");return false;}
    if (const FRefinementSessionState* Existing=ActiveSessions.Find(OwnerId);Existing&&!Existing->bFinished) {OutError=TEXT("A refinement is already active.");return false;}
    const UGuEntitySubsystem* EntitySystem=GetGameInstance()?GetGameInstance()->GetSubsystem<UGuEntitySubsystem>():nullptr;
    if(!EntitySystem){OutError=TEXT("Gu entity subsystem is unavailable.");return false;}

    TArray<FRefinementInputSelection> NormalizedInputs;
    TArray<FGuid> UniqueEntityIds;
    for(const FRefinementInputSelection& Raw:Inputs)
    {
        if(!Raw.EntityId.IsValid()){OutError=TEXT("A refinement selection contains an invalid entity ID.");return false;}
        FRefinementInputSelection Selection=Raw;Selection.Quantity=FMath::Max(1,Selection.Quantity);
        if(IsInputReserved(Selection.EntityId)){OutError=TEXT("One of the selected physical ingredients is already committed to another refinement.");return false;}
        if(const FOwnedByComponent* Owned=EntitySystem->GetOwnedBy(Selection.EntityId);Owned&&!Owned->OwnerId.IsEmpty()&&Owned->OwnerId!=OwnerId)
        {
            OutError=TEXT("You cannot refine a Gu owned by another domain character.");
            return false;
        }
        if(const FMaterialLotComponent* Lot=EntitySystem->GetMaterialLot(Selection.EntityId))
        {
            if(Lot->Quantity<Selection.Quantity){OutError=FString::Printf(TEXT("Material lot '%s' has only %d units available."),*Lot->Item.ToString(),Lot->Quantity);return false;}
        }
        else if(Selection.Quantity!=1)
        {
            OutError=TEXT("Gu and non-lot refinables are individual physical entities and must use quantity 1.");
            return false;
        }
        NormalizedInputs.Add(Selection);
        UniqueEntityIds.AddUnique(Selection.EntityId);
    }

    FRefinementAnalysis Analysis;
    if (!AnalyzePhysicalSelections(NormalizedInputs,Analysis,OutError)) return false;

    FRefinementSessionState Session;
    Session.SessionId=FGuid::NewGuid(); Session.OwnerId=OwnerId; Session.InputEntityIds=UniqueEntityIds; Session.InputSelections=NormalizedInputs; Session.IntendedDefinitionId=IntendedDefinitionId; Session.bKnownRecipe=bKnownRecipe; Session.InitialAnalysis=Analysis;
    for(int32 Index=0;Index<NormalizedInputs.Num();++Index)
    {
        const FRefinementInputSelection& Selection=NormalizedInputs[Index];
        FRefinementSemanticSnapshot Snapshot;
        if(EntitySystem->GetRefinementSemanticSnapshot(Selection.EntityId,Snapshot))
        {
            FRefinementCommittedInput Input; Input.EntityId=Snapshot.EntityId; Input.Kind=static_cast<uint8>(Snapshot.Kind); Input.SourceId=Snapshot.SourceId; Input.DefinitionId=Snapshot.DefinitionId; Input.Quantity=Selection.Quantity; Input.bFoundation=Index==0&&Snapshot.Kind==ERefinableKind::Gu; Session.CommittedInputs.Add(Input);
        }
    }
    Session.AttemptRank=FMath::Max(1,Analysis.ResultRank); Session.IntendedPath=Analysis.PrimaryPath;
    if (bKnownRecipe&&!IntendedDefinitionId.IsNone())
    {
        const UGuDefinitionRegistrySubsystem* Registry=GetGameInstance()?GetGameInstance()->GetSubsystem<UGuDefinitionRegistrySubsystem>():nullptr;
        const FGuDefinitionRecord* Definition=Registry?Registry->FindDefinition(IntendedDefinitionId):nullptr;
        if (!Definition) {OutError=TEXT("Known refinement target references an unknown Gu definition.");return false;}
        Session.AttemptRank=Definition->Rank; Session.IntendedPath=Definition->Path;
        // A physically matching known formula supplies a nascent target scaffold, not a guaranteed result.
        const TArray<TPair<FName,float>> Paths=SortedScores(Analysis.PathScores);
        const float Top=FMath::Max(1.0f,Paths.IsEmpty()?1.0f:Paths[0].Value),Current=Analysis.PathScores.FindRef(Session.IntendedPath);
        const float Desired=FMath::Max(Top*1.28f,Current+Top*.30f);
        if (Current<Desired) AddScore(Analysis.PathScores,Session.IntendedPath,Desired-Current);
        if (const TMap<FName,float>* Profile=PathProperties().Find(Session.IntendedPath))
        {
            float ProfileTotal=0.0f; for (const TPair<FName,float>& Pair:*Profile) ProfileTotal+=Pair.Value;
            AddScores(Analysis.PropertyScores,*Profile,FMath::Max(1.05f,Top*.58f)/FMath::Max(1.0f,ProfileTotal));
        }
        Analysis.NascentDirection=ResolveFormationDirection(Analysis.PathScores,Analysis.PropertyScores); Analysis.PrimaryPath=Analysis.NascentDirection.PrimaryPath; Analysis.PathCoherence=Analysis.NascentDirection.Coherence;
        Session.InitialAnalysis=Analysis;
    }
    Session.NascentPathScores=Analysis.PathScores; Session.NascentProperties=Analysis.PropertyScores; Session.NascentAttributes=Analysis.AttributeScores; Session.NascentTraits=Analysis.TraitScores;
    Session.ContaminationPaths=Analysis.ContaminationPaths; Session.ContaminationAttributes=Analysis.ContaminationAttributes; Session.ContaminationTraits=Analysis.ContaminationTraits;
    Session.Stability=FMath::RoundToFloat(90.0f+Analysis.PathCoherence*10.0f); Session.MaxStability=Session.Stability; Session.LowestStability=Session.Stability; Session.Temperature=20.0f;
    Session.MaxFocus=PlayerState->MentalResources?PlayerState->MentalResources->GetFocusCapacity():100.0f; Session.Focus=Session.MaxFocus; Session.StartedAtUnixMs=NowUnixMs();
    if (PlayerState->MentalResources&&!PlayerState->MentalResources->ReserveAttention(TEXT("Refinement"),1.0f,TEXT("controlling a refinement"))) {OutError=TEXT("Your attention is already occupied.");return false;}
    for(const FGuid EntityId:UniqueEntityIds)ReservedInputEntities.Add(EntityId);
    GenerateProcedure(Session); AddLog(Session,bKnownRecipe?TEXT("The known refinement begins around the committed materials."):TEXT("No known recipe governs this mixture. You proceed by experimentation."));
    ActiveSessions.Add(OwnerId,Session); PublishPublicState(PlayerState,ActiveSessions.Find(OwnerId)); OutError.Reset(); return true;
}

bool URefinementSubsystem::UseBasicRefinementAction(AGuPlayerState* PlayerState,const ERefinementVerb Verb,FString& OutError)
{
    if (!PlayerState||!PlayerState->HasAuthority()) {OutError=TEXT("Refinement actions are server-authoritative.");return false;}
    FRefinementSessionState* Session=ActiveSessions.Find(PlayerState->DomainCharacterId);
    if (!Session||Session->bFinished) {OutError=TEXT("No active refinement session.");return false;}
    if (!Session->HiddenProcedure.IsValidIndex(Session->StepIndex)) {OutError=TEXT("Refinement has no active hidden phase.");return false;}
    const FBasicActionSpec Action=BasicAction(Verb);
    if (Session->Focus<Action.FocusCost) {OutError=TEXT("You cannot maintain the concentration required for that manipulation.");return false;}
    if (PlayerState->MentalResources&&!PlayerState->MentalResources->HasAttentionReservation(TEXT("Refinement"))&&!PlayerState->MentalResources->ReserveAttention(TEXT("Refinement"),1.0f,TEXT("controlling a refinement"))) {OutError=TEXT("You cannot divide your attention enough to manipulate the refinement.");return false;}
    Session->bQuietLull=false;
    const FRefinementProcedureStep Step=Session->HiddenProcedure[Session->StepIndex]; const float Affinity=ProcedureAffinity(Step,Verb); const bool bAccepted=Affinity>=.75f;
    const float BeforeStability=Session->Stability,BeforeImpurities=Session->Impurities,BeforeTemperature=Session->Temperature,BeforeProgress=Session->StepProgress; const FRefinementDirection BeforeDirection=EffectiveDirection(*Session);
    Session->Focus-=Action.FocusCost; if (PlayerState->MentalResources&&Action.FocusCost>0.0f) PlayerState->MentalResources->RecordRefinementFocusUse(Action.FocusCost);
    ApplyPhysicalAction(*Session,Action);
    const float Gain=Action.ProgressPower*(.24f+FMath::Clamp(Affinity,0.0f,1.25f)*.76f); const float Remaining=FMath::Max(0.0f,Step.RequiredProgress-BeforeProgress); const float StageFraction=FMath::Min(Gain,Remaining)/FMath::Max(1.0f,Step.RequiredProgress);
    Session->StepProgress+=Gain; AddLog(*Session,FString::Printf(TEXT("%s changes the nascent Gu."),*VerbName(Verb)));
    if (!bAccepted) {const float Severity=1.0f-FMath::Clamp(Affinity,0.0f,.75f)/.75f;Session->Stability-=3.0f+Severity*5.0f;Session->Impurities+=1.0f+Severity*4.0f;}
    if (bAccepted) Session->AcceptedActionCount++; else Session->OffMethodActionCount++;
    Session->ProcessCounts.FindOrAdd(FName(*VerbName(Verb)))++;
    GuideNascentFormation(*Session,Verb,Affinity,StageFraction);
    ApplyTemperatureConsequences(*Session,Step,Verb,BeforeTemperature);
    const FRefinementDirection AfterDirection=EffectiveDirection(*Session); const float SemanticDelta=(AfterDirection.Coherence-BeforeDirection.Coherence)*.7f+(AfterDirection.PropertyAlignment-BeforeDirection.PropertyAlignment)*.3f;
    const bool bOutside=Session->Temperature<Step.TargetTemperature.X||Session->Temperature>Step.TargetTemperature.Y;
    RecordObservableFeedback(*Session,Verb,Affinity,SemanticDelta,Session->Stability-BeforeStability,Session->Impurities-BeforeImpurities,FMath::Max(0.0f,Session->StepProgress-BeforeProgress)/FMath::Max(1.0f,Step.RequiredProgress),bOutside);
    if (bOutside) Session->TemperatureExcursions++;
    Session->LowestStability=FMath::Min(Session->LowestStability,Session->Stability); Session->MaximumImpurities=FMath::Max(Session->MaximumImpurities,Session->Impurities);
    FRefinementActionRecord Record; Record.Verb=Verb;Record.Affinity=Affinity;Record.bAccepted=bAccepted;Record.StabilityAfter=Session->Stability;Record.ImpuritiesAfter=Session->Impurities;Record.TemperatureAfter=Session->Temperature;Record.FocusAfter=Session->Focus;Session->ActionHistory.Add(Record);if(Session->ActionHistory.Num()>80)Session->ActionHistory.RemoveAt(0,Session->ActionHistory.Num()-80);
    if (!CheckFailure(PlayerState,*Session)) CheckStepCompletion(PlayerState,*Session);
    PublishPublicState(PlayerState,Session); OutError.Reset(); return true;
}

bool URefinementSubsystem::AttachGuRefinementAssistant(AGuPlayerState* PlayerState,const FGuid GuEntityId,FString& OutError)
{
    if(!PlayerState||!PlayerState->HasAuthority()){OutError=TEXT("Refinement assistance is server-authoritative.");return false;}
    FRefinementSessionState* Session=ActiveSessions.Find(PlayerState->DomainCharacterId);
    if(!Session||Session->bFinished){OutError=TEXT("Begin a refinement before activating this Gu.");return false;}
    UGameInstance* GI=GetGameInstance();
    UGuEntitySubsystem* Entities=GI?GI->GetSubsystem<UGuEntitySubsystem>():nullptr;
    UGuDefinitionRegistrySubsystem* Registry=GI?GI->GetSubsystem<UGuDefinitionRegistrySubsystem>():nullptr;
    if(!Entities||!Registry){OutError=TEXT("Gu domain systems are unavailable.");return false;}
    const FGuInstanceComponent* Instance=Entities->GetGuInstance(GuEntityId);
    const FGuConditionComponent* Condition=Entities->GetGuCondition(GuEntityId);
    const FOwnedByComponent* Owner=Entities->GetOwnedBy(GuEntityId);
    const FGuPlacementComponent* Placement=Entities->GetGuPlacement(GuEntityId);
    const FRefinementAssistantComponent* Assistant=Entities->GetRefinementAssistant(GuEntityId);
    if(!Instance||!Condition||!Condition->bAlive||!Owner||Owner->OwnerId!=PlayerState->DomainCharacterId){OutError=TEXT("That Gu is not a living Gu owned by this character.");return false;}
    if(!Placement||Placement->Container!=EGuContainer::Aperture){OutError=TEXT("The assisting Gu must be alive inside the aperture.");return false;}
    if(!Assistant){OutError=TEXT("That Gu has no refinement-assistance technique.");return false;}
    for(const FRefinementAssistanceContribution& Existing:Session->Assistance)if(Existing.SourceEntityId==GuEntityId&&Existing.UsesRemaining>0){OutError=TEXT("This Gu is already assisting the current refinement.");return false;}
    const FGuDefinitionRecord* Definition=Registry->FindDefinition(Instance->DefinitionId);
    if(!Definition){OutError=TEXT("The assisting Gu definition is missing.");return false;}

    FRefinementAssistanceContribution Contribution;
    Contribution.SourceEntityId=GuEntityId;
    Contribution.SourceDefinitionId=Instance->DefinitionId;
    Contribution.Label=Definition->Name;
    Contribution.Path=Definition->Path.IsNone()?FName(TEXT("Refinement")):Definition->Path;
    Contribution.Rank=FMath::Max(1,Definition->Rank);
    Contribution.ProgressPercent=FMath::Clamp(Assistant->ProgressPercent,0.0f,500.0f);
    Contribution.StabilityPerAction=FMath::Clamp(Assistant->StabilityPerAction,-100.0f,100.0f);
    Contribution.ImpurityReductionPerAction=FMath::Clamp(Assistant->ImpurityReductionPerAction,0.0f,100.0f);
    Contribution.QualityBonus=FMath::Clamp(Assistant->QualityBonus,0.0f,100.0f);
    Contribution.MaximumUses=FMath::Clamp(Assistant->ActionUses,1,99);
    Contribution.UsesRemaining=Contribution.MaximumUses;
    Contribution.Processes=Assistant->Processes;
    Session->Assistance.Add(Contribution);
    AddLog(*Session,FString::Printf(TEXT("%s joins the refinement formation and exposes its own cauldron technique."),*Contribution.Label));
    PublishPublicState(PlayerState,Session);
    OutError.Reset();
    return true;
}

bool URefinementSubsystem::UseGuRefinementAssistant(AGuPlayerState* PlayerState,const FGuid GuEntityId,FString& OutError)
{
    if(!PlayerState||!PlayerState->HasAuthority()){OutError=TEXT("Refinement assistance is server-authoritative.");return false;}
    FRefinementSessionState* Session=ActiveSessions.Find(PlayerState->DomainCharacterId);
    if(!Session||Session->bFinished||!Session->HiddenProcedure.IsValidIndex(Session->StepIndex)){OutError=TEXT("No active refinement phase can receive that technique.");return false;}
    FRefinementAssistanceContribution* Technique=nullptr;
    for(FRefinementAssistanceContribution& Candidate:Session->Assistance)if(Candidate.SourceEntityId==GuEntityId&&Candidate.UsesRemaining>0){Technique=&Candidate;break;}
    if(!Technique){OutError=TEXT("That Gu has no prepared refinement technique remaining in this session.");return false;}

    UGuEntitySubsystem* Entities=GetGameInstance()?GetGameInstance()->GetSubsystem<UGuEntitySubsystem>():nullptr;
    const FGuConditionComponent* Condition=Entities?Entities->GetGuCondition(GuEntityId):nullptr;
    if(!Condition||!Condition->bAlive){OutError=TEXT("The assisting Gu is no longer alive.");return false;}

    const FRefinementProcedureStep Step=Session->HiddenProcedure[Session->StepIndex];
    auto ToVerb=[](FName Process,ERefinementVerb& OutVerb)->bool
    {
        const FString P=Process.ToString();
        if(P.Equals(TEXT("Process"),ESearchCase::IgnoreCase)){OutVerb=ERefinementVerb::Process;return true;}
        if(P.Equals(TEXT("Heat"),ESearchCase::IgnoreCase)){OutVerb=ERefinementVerb::Heat;return true;}
        if(P.Equals(TEXT("Cool"),ESearchCase::IgnoreCase)){OutVerb=ERefinementVerb::Cool;return true;}
        if(P.Equals(TEXT("Merge"),ESearchCase::IgnoreCase)){OutVerb=ERefinementVerb::Merge;return true;}
        if(P.Equals(TEXT("Purify"),ESearchCase::IgnoreCase)){OutVerb=ERefinementVerb::Purify;return true;}
        if(P.Equals(TEXT("Control"),ESearchCase::IgnoreCase)){OutVerb=ERefinementVerb::Control;return true;}
        if(P.Equals(TEXT("Condense"),ESearchCase::IgnoreCase)){OutVerb=ERefinementVerb::Condense;return true;}
        return false;
    };
    ERefinementVerb Verb=ERefinementVerb::Control;
    float BestAffinity=-1.0f;
    if(Technique->Processes.IsEmpty()) BestAffinity=ProcedureAffinity(Step,Verb);
    else for(const FName Process:Technique->Processes){ERefinementVerb Candidate;if(ToVerb(Process,Candidate)){const float CandidateAffinity=ProcedureAffinity(Step,Candidate);if(CandidateAffinity>BestAffinity){BestAffinity=CandidateAffinity;Verb=Candidate;}}}
    if(BestAffinity<0.0f) BestAffinity=ProcedureAffinity(Step,Verb);

    const float FocusCost=FMath::Max(1.0f,FMath::RoundToFloat(3.0f+Technique->Rank));
    if(Session->Focus<FocusCost){OutError=TEXT("You cannot maintain the concentration required for that Gu technique.");return false;}
    if(PlayerState->MentalResources&&!PlayerState->MentalResources->HasAttentionReservation(TEXT("Refinement"))&&!PlayerState->MentalResources->ReserveAttention(TEXT("Refinement"),1.0f,TEXT("controlling a refinement"))){OutError=TEXT("You cannot divide your attention enough to manipulate the refinement.");return false;}

    const bool bAccepted=BestAffinity>=.75f;
    const float BeforeStability=Session->Stability,BeforeImpurities=Session->Impurities,BeforeTemperature=Session->Temperature,BeforeProgress=Session->StepProgress;
    const FRefinementDirection BeforeDirection=EffectiveDirection(*Session);
    Session->Focus-=FocusCost;
    if(PlayerState->MentalResources) PlayerState->MentalResources->RecordRefinementFocusUse(FocusCost);
    Session->Stability=FMath::Min(Session->MaxStability,Session->Stability+Technique->StabilityPerAction);
    Session->Impurities=FMath::Max(0.0f,Session->Impurities-Technique->ImpurityReductionPerAction);
    if(Technique->Path==TEXT("Fire")||Technique->Path==TEXT("Light")) Session->Temperature+=10.0f+Technique->Rank*2.0f;
    else if(Technique->Path==TEXT("Ice")||Technique->Path==TEXT("Water")) Session->Temperature=FMath::Max(20.0f,Session->Temperature-(8.0f+Technique->Rank*2.0f));

    const float ProgressPower=FMath::Clamp(8.0f+Technique->Rank*2.0f+Technique->ProgressPercent*.16f,6.0f,62.0f);
    const float Gain=ProgressPower*(.24f+FMath::Clamp(BestAffinity,0.0f,1.25f)*.76f);
    const float Remaining=FMath::Max(0.0f,Step.RequiredProgress-BeforeProgress);
    const float StageFraction=FMath::Min(Gain,Remaining)/FMath::Max(1.0f,Step.RequiredProgress);
    Session->StepProgress+=Gain;
    if(!bAccepted){const float Severity=1.0f-FMath::Clamp(BestAffinity,0.0f,.75f)/.75f;Session->Stability-=3.0f+Severity*5.0f;Session->Impurities+=1.0f+Severity*4.0f;Session->OffMethodActionCount++;}
    else Session->AcceptedActionCount++;
    Session->ProcessCounts.FindOrAdd(FName(*VerbName(Verb)))++;
    if(!Technique->Path.IsNone()&&Technique->Path!=TEXT("Refinement")) Session->PathTechniqueCounts.FindOrAdd(Technique->Path)++;
    Session->AssistanceQualityBonus+=Technique->QualityBonus*.04f;
    GuideNascentFormation(*Session,Verb,BestAffinity,StageFraction,Technique->Path,true);
    ApplyTemperatureConsequences(*Session,Step,Verb,BeforeTemperature);
    const FRefinementDirection AfterDirection=EffectiveDirection(*Session);
    const float SemanticDelta=(AfterDirection.Coherence-BeforeDirection.Coherence)*.7f+(AfterDirection.PropertyAlignment-BeforeDirection.PropertyAlignment)*.3f;
    const bool bOutside=Session->Temperature<Step.TargetTemperature.X||Session->Temperature>Step.TargetTemperature.Y;
    RecordObservableFeedback(*Session,Verb,BestAffinity,SemanticDelta,Session->Stability-BeforeStability,Session->Impurities-BeforeImpurities,FMath::Max(0.0f,Session->StepProgress-BeforeProgress)/FMath::Max(1.0f,Step.RequiredProgress),bOutside);
    if(bOutside)Session->TemperatureExcursions++;
    Session->LowestStability=FMath::Min(Session->LowestStability,Session->Stability);Session->MaximumImpurities=FMath::Max(Session->MaximumImpurities,Session->Impurities);
    FRefinementActionRecord Record;Record.Verb=Verb;Record.Affinity=BestAffinity;Record.bAccepted=bAccepted;Record.TechniquePath=Technique->Path;Record.StabilityAfter=Session->Stability;Record.ImpuritiesAfter=Session->Impurities;Record.TemperatureAfter=Session->Temperature;Record.FocusAfter=Session->Focus;Session->ActionHistory.Add(Record);if(Session->ActionHistory.Num()>80)Session->ActionHistory.RemoveAt(0,Session->ActionHistory.Num()-80);

    Technique->UsesRemaining=FMath::Max(0,Technique->UsesRemaining-1);
    Session->AssistanceQualityBonus+=Technique->QualityBonus/FMath::Max(1,Technique->MaximumUses);
    AddLog(*Session,Technique->UsesRemaining>0
        ? FString::Printf(TEXT("%s shapes the mixture directly (%d technique use%s remain)."),*Technique->Label,Technique->UsesRemaining,Technique->UsesRemaining==1?TEXT(""):TEXT("s"))
        : FString::Printf(TEXT("%s shapes the mixture directly and its prepared assistance is spent."),*Technique->Label));

    if(!CheckFailure(PlayerState,*Session)) CheckStepCompletion(PlayerState,*Session);
    PublishPublicState(PlayerState,Session);
    OutError.Reset();
    return true;
}

void URefinementSubsystem::CheckStepCompletion(AGuPlayerState* PlayerState,FRefinementSessionState& Session)
{
    if (!Session.HiddenProcedure.IsValidIndex(Session.StepIndex)) return;
    const FRefinementProcedureStep& Step=Session.HiddenProcedure[Session.StepIndex]; if(Session.StepProgress<Step.RequiredProgress)return;
    if (Session.Impurities>Step.MaxImpurities||Session.Temperature<Step.TargetTemperature.X||Session.Temperature>Step.TargetTemperature.Y) {Session.StepProgress=Step.RequiredProgress;AddLog(Session,TEXT("The nascent body refuses to settle and the current phase will not close."));return;}
    AddLog(Session,TEXT("The nascent Gu settles briefly, then enters another phase.")); Session.StepIndex++; Session.StepProgress=0.0f; Session.Focus=FMath::Min(Session.MaxFocus,Session.Focus+FMath::Max(12.0f,Session.MaxFocus*.16f));
    if (Session.StepIndex>=Session.HiddenProcedure.Num()) {FinishSession(PlayerState,Session);return;}
    if (Step.bRecoveryWindow) {Session.bQuietLull=true;if(PlayerState->MentalResources)PlayerState->MentalResources->ReleaseAttention(TEXT("Refinement"));AddLog(Session,TEXT("For a time, the nascent Gu can be left undisturbed while its form holds itself together."));}
    else AddLog(Session,TEXT("The forming Gu enters another phase."));
}

bool URefinementSubsystem::CheckFailure(AGuPlayerState* PlayerState,FRefinementSessionState& Session)
{
    if (Session.Stability>0.0f&&Session.Impurities<100.0f)return false;
    Session.bFinished=true;Session.bSuccess=false;Session.Outcome.Kind=ERefinementOutcomeKind::Failure;Session.Outcome.Reason=TEXT("physical-collapse");Session.Outcome.Message=TEXT("The refinement collapses. The committed materials are lost.");
    AddLog(Session,Session.Outcome.Message);
    ResolveFailedCommittedInputs(Session);
    Session.Assistance.Reset();
    if(PlayerState->MentalResources)PlayerState->MentalResources->ReleaseAttention(TEXT("Refinement"));RecordNotebook(Session);return true;
}

void URefinementSubsystem::FinishSession(AGuPlayerState* PlayerState,FRefinementSessionState& Session)
{
    Session.bFinished=true;
    Session.Outcome=ResolveOutcome(Session);
    Session.bSuccess=Session.Outcome.Kind!=ERefinementOutcomeKind::Failure;
    if(PlayerState->MentalResources)PlayerState->MentalResources->ReleaseAttention(TEXT("Refinement"));

    if(Session.bSuccess)
    {
        FString SecureError;
        if(!SecureSuccessfulOutcome(PlayerState,Session,SecureError))
        {
            ConsumeCommittedInputs(Session);
            Session.bSuccess=false;
            Session.Outcome.Kind=ERefinementOutcomeKind::Failure;
            Session.Outcome.Reason=TEXT("secure-result-failed");
            Session.Outcome.Message=FString::Printf(TEXT("The formation completed, but the resulting Gu could not be secured: %s"),*SecureError);
            AddLog(Session,Session.Outcome.Message);
        }
        else
        {
            const float Score=Session.Stability-Session.Impurities+(Session.Focus/FMath::Max(1.0f,Session.MaxFocus))*10.0f+Session.AssistanceQualityBonus-ContaminationTotal(Session)*2.0f;
            const TCHAR* Condition=Score>=90.0f?TEXT("healthy"):Score>=55.0f?TEXT("strained"):TEXT("severely strained");
            const UGuDefinitionRegistrySubsystem* Registry=GetGameInstance()?GetGameInstance()->GetSubsystem<UGuDefinitionRegistrySubsystem>():nullptr;
            const FGuDefinitionRecord* ResultDefinition=Registry?Registry->FindDefinition(Session.Outcome.ResultDefinitionId):nullptr;
            const FString ResultName=ResultDefinition?ResultDefinition->Name:Session.Outcome.ResultDefinitionId.ToString();
            Session.Outcome.Message=FString::Printf(TEXT("%s forms successfully. The newly formed body is %s."),*ResultName,Condition);
            AddLog(Session,Session.Outcome.Message);
        }
    }
    else
    {
        AddLog(Session,Session.Outcome.Message);
        ResolveFailedCommittedInputs(Session);
    }
    Session.Assistance.Reset();
    RecordNotebook(Session);
}

bool URefinementSubsystem::AbortRefinementSession(AGuPlayerState* PlayerState,FString& OutError)
{
    if(!PlayerState||!PlayerState->HasAuthority()){OutError=TEXT("Refinement actions are server-authoritative.");return false;}
    FRefinementSessionState* Session=ActiveSessions.Find(PlayerState->DomainCharacterId);if(!Session||Session->bFinished){OutError=TEXT("No active refinement session.");return false;}
    Session->bFinished=true;Session->bSuccess=false;Session->Outcome.Kind=ERefinementOutcomeKind::Failure;Session->Outcome.Reason=TEXT("aborted");Session->Outcome.Message=TEXT("You release control. The unfinished refinement collapses.");
    AddLog(*Session,Session->Outcome.Message);
    ResolveFailedCommittedInputs(*Session);
    Session->Assistance.Reset();
    if(PlayerState->MentalResources)PlayerState->MentalResources->ReleaseAttention(TEXT("Refinement"));RecordNotebook(*Session);PublishPublicState(PlayerState,Session);OutError.Reset();return true;
}

bool URefinementSubsystem::HasActiveSession(const FString& OwnerId) const
{
    const FRefinementSessionState* Session=ActiveSessions.Find(OwnerId);return Session&&!Session->bFinished;
}

FRefinementPublicState URefinementSubsystem::BuildPublicState(const FRefinementSessionState& Session) const
{
    FRefinementPublicState Public;Public.bActive=!Session.bFinished;Public.bFinished=Session.bFinished;Public.bSucceeded=Session.bSuccess;Public.PhaseNumber=Session.bKnownRecipe?FMath::Min(Session.StepIndex+1,Session.HiddenProcedure.Num()):0;Public.PhaseCount=Session.bKnownRecipe?Session.HiddenProcedure.Num():0;Public.Focus=Session.Focus;Public.MaxFocus=Session.MaxFocus;Public.Form=PublicFormLabel(Session);Public.Response=ObservableResponseLabel(Session.Observations);Public.Condition=PublicConditionLabel(Session);Public.LastObservation=Session.Log.IsEmpty()?TEXT(""):Session.Log.Last();
    for(const FRefinementAssistanceContribution& Assistance:Session.Assistance)if(Assistance.UsesRemaining>0)Public.ActiveAssistance.Add(FString::Printf(TEXT("%s (%d uses)"),*Assistance.Label,Assistance.UsesRemaining));
    if(Session.bFinished)Public.ResultText=Session.Outcome.Message;return Public;
}

FRefinementPublicState URefinementSubsystem::GetPublicState(const FString& OwnerId) const
{
    if(const FRefinementSessionState* Session=ActiveSessions.Find(OwnerId))return BuildPublicState(*Session);return FRefinementPublicState();
}

bool URefinementSubsystem::GetDebugSessionState(const FString& OwnerId,FRefinementSessionState& OutState) const
{
    if(const FRefinementSessionState* Session=ActiveSessions.Find(OwnerId)){OutState=*Session;return true;}return false;
}

void URefinementSubsystem::PublishPublicState(AGuPlayerState* PlayerState,const FRefinementSessionState* Session) const
{
    if(!PlayerState||!PlayerState->HasAuthority())return;if(Session)PlayerState->SetRefinementPublicState(BuildPublicState(*Session));else PlayerState->ClearRefinementPublicState();
}

FName URefinementSubsystem::NotebookIdForSession(const FRefinementSessionState& Session) const
{
    if(!Session.IntendedDefinitionId.IsNone())return FName(*FString::Printf(TEXT("known_%s"),*Session.IntendedDefinitionId.ToString()));
    const FRefinementAnalysis& A=Session.Outcome.Analysis;FString Sig=FString::Printf(TEXT("experimental_%s_%d_%s"),*A.PrimaryPath.ToString(),A.ResultRank,*A.Template.ToString());for(const FName Attr:A.SurvivingAttributes)Sig+=TEXT("_")+Attr.ToString();for(const FName Trait:A.SurvivingTraits)Sig+=TEXT("_")+Trait.ToString();return FName(*FString::Printf(TEXT("experimental_%08x"),FCrc::StrCrc32(*Sig)));
}

void URefinementSubsystem::RecordNotebook(const FRefinementSessionState& Session)
{
    const FName Id=NotebookIdForSession(Session);FRefinementNotebookRecord& Record=NotebookRecords.FindOrAdd(Id);const int64 Now=NowUnixMs();if(Record.Id.IsNone()){Record.Id=Id;Record.FirstObservedAtUnixMs=Now;}
    Record.LastStudiedAtUnixMs=Now;Record.AttemptCount++;if(Session.bSuccess){Record.SuccessCount++;Record.Status=ERefinementKnowledgeState::Known;}else{Record.FailureCount++;if(Record.Status==ERefinementKnowledgeState::Observed)Record.Status=ERefinementKnowledgeState::Suspected;}
    Record.ResultDefinitionId=Session.Outcome.ResultDefinitionId;Record.ResultPath=Session.Outcome.ResultPath;Record.Rank=Session.Outcome.ResultRank;Record.LastOutcome=Session.Outcome.Message;Record.Procedure.Reset();for(const FRefinementProcedureStep& Step:Session.HiddenProcedure){FRefinementNotebookProcedureStep Saved;Saved.Name=Step.Name;Saved.Process=Step.PrimaryProcess;Saved.RequiredProgress=Step.RequiredProgress;Saved.TargetTemperature=Step.TargetTemperature;Saved.MaxImpurities=Step.MaxImpurities;Saved.bRecoveryWindow=Step.bRecoveryWindow;Record.Procedure.Add(Saved);}
    Record.Ingredients.Reset();for(const FRefinementCommittedInput& Input:Session.CommittedInputs){FRefinementNotebookIngredient Ing;Ing.EntityId=Input.EntityId;Ing.Kind=Input.Kind;Ing.SourceId=Input.SourceId;Ing.DefinitionId=Input.DefinitionId;Ing.Quantity=Input.Quantity;Ing.bFoundation=Input.bFoundation;Record.Ingredients.Add(Ing);}
}

FRefinementNotebookSnapshot URefinementSubsystem::ExportNotebook() const
{
    FRefinementNotebookSnapshot Snapshot;NotebookRecords.GenerateValueArray(Snapshot.Records);Snapshot.Records.Sort([](const FRefinementNotebookRecord& A,const FRefinementNotebookRecord& B){return A.Id.ToString()<B.Id.ToString();});return Snapshot;
}

void URefinementSubsystem::RestoreNotebook(const FRefinementNotebookSnapshot& Snapshot)
{
    NotebookRecords.Reset();for(const FRefinementNotebookRecord& Record:Snapshot.Records)if(!Record.Id.IsNone())NotebookRecords.Add(Record.Id,Record);
}

TArray<FRefinementNotebookRecord> URefinementSubsystem::GetNotebookRecords() const
{
    return ExportNotebook().Records;
}
