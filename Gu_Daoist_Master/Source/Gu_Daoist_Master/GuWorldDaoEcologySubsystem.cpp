#include "GuWorldDaoEcologySubsystem.h"

#include "Engine/World.h"
#include "UGuDefinition.h"

DEFINE_LOG_CATEGORY_STATIC(LogGuWorldDaoEcology, Log, All);

namespace
{
    constexpr float TraceUnits[10] = {
        0.0f, 0.00025f, 0.0007f, 0.002f, 0.018f, 0.18f, 2.4f, 18.0f, 120.0f, 850.0f
    };
    constexpr float TraceHalfLives[10] = {
        0.0f, 120.0f, 180.0f, 280.0f, 520.0f, 1100.0f, 4200.0f, 15000.0f, 60000.0f, 240000.0f
    };
    constexpr float WildDensities[10] = {
        0.0f, 45.0f, 80.0f, 135.0f, 240.0f, 420.0f, 900.0f, 1800.0f, 4000.0f, 9000.0f
    };
    constexpr float WildMaturities[10] = {
        0.0f, 3.0f, 12.0f, 35.0f, 100.0f, 260.0f, 700.0f, 1800.0f, 5000.0f, 12000.0f
    };
    constexpr float BioticStanding[10] = {
        0.0f, 2.2f, 5.4f, 13.0f, 31.0f, 78.0f, 210.0f, 620.0f, 1900.0f, 6000.0f
    };

    int32 ClampRank(const int32 Rank)
    {
        return FMath::Clamp(Rank, 1, 9);
    }

    void AddMatureMark(
        TMap<FGameplayTag, float>& Marks,
        TMap<FGameplayTag, float>& Ages,
        const FGameplayTag& Path,
        const float Amount,
        const float MaturityYears)
    {
        if (!Path.IsValid() || FMath::IsNearlyZero(Amount))
        {
            return;
        }

        // Mirrors v7.9.84 addSignedMarks(): positive additions blend the local
        // maturity age by Dao mass; negative removal does not invent a new age.
        const float Before = FMath::Max(0.0f, Marks.FindRef(Path));
        const float BeforeAge = FMath::Max(0.0f, Ages.FindRef(Path));
        const float After = FMath::Max(0.0f, Before + Amount);
        Marks.FindOrAdd(Path) = After;

        if (Amount > 0.001f)
        {
            Ages.FindOrAdd(Path) = After > 0.0f
                ? (Before * BeforeAge + Amount * FMath::Max(0.0f, MaturityYears)) / After
                : FMath::Max(0.0f, MaturityYears);
        }
    }

    float DistanceToSegment(const FVector2D& Point, const FVector2D& A, const FVector2D& B)
    {
        const FVector2D AB = B - A;
        const float Denom = AB.SizeSquared();
        if (Denom <= KINDA_SMALL_NUMBER)
        {
            return FVector2D::Distance(Point, A);
        }
        const float T = FMath::Clamp(FVector2D::DotProduct(Point - A, AB) / Denom, 0.0f, 1.0f);
        return FVector2D::Distance(Point, A + AB * T);
    }

    bool ContainsSubstrate(const TArray<FName>& Values, const FName Value)
    {
        return Values.Contains(Value);
    }
}

bool UGuWorldDaoEcologySubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    const UWorld* World = Cast<UWorld>(Outer);
    return World != nullptr && World->IsGameWorld();
}

void UGuWorldDaoEcologySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    RegisterDefaultRules();
}

void UGuWorldDaoEcologySubsystem::Deinitialize()
{
    RegionFields.Reset();
    DynamicEvents.Reset();
    ActivityFields.Reset();
    ConflictRules.Reset();
    WildGuRules.Reset();
    ResourceRules.Reset();
    BeastRules.Reset();
    ProfileCache.Reset();
    EventSpatialBuckets.Reset();
    GlobalEventIds.Reset();
    bEventIndexDirty = true;
    Super::Deinitialize();
}

float UGuWorldDaoEcologySubsystem::TraceUnitsForRank(const int32 Rank)
{
    return TraceUnits[ClampRank(Rank)];
}

float UGuWorldDaoEcologySubsystem::TraceHalfLifeYearsForRank(const int32 Rank)
{
    return TraceHalfLives[ClampRank(Rank)];
}

float UGuWorldDaoEcologySubsystem::WildGuDensityForRank(const int32 Rank)
{
    return WildDensities[ClampRank(Rank)];
}

float UGuWorldDaoEcologySubsystem::WildGuMaturityYearsForRank(const int32 Rank)
{
    return WildMaturities[ClampRank(Rank)];
}

float UGuWorldDaoEcologySubsystem::DaoRetentionFraction(const float AgeYears, const float HalfLifeYears)
{
    const float Age = FMath::Max(0.0f, AgeYears);
    const float HalfLife = FMath::Max(0.0001f, HalfLifeYears);
    return FMath::Pow(0.5f, Age / HalfLife);
}

float UGuWorldDaoEcologySubsystem::ContinuousDaoStock(const float RatePerYear, const float Years, const float HalfLifeYears)
{
    const float Rate = FMath::Max(0.0f, RatePerYear);
    const float Age = FMath::Max(0.0f, Years);
    const float HalfLife = FMath::Max(0.0001f, HalfLifeYears);
    if (Rate <= 0.0f || Age <= 0.0f)
    {
        return 0.0f;
    }
    const float K = FMath::Loge(2.0f) / HalfLife;
    return Rate / K * (1.0f - FMath::Exp(-K * Age));
}

float UGuWorldDaoEcologySubsystem::DecayDaoStock(const float Amount, const float Years, const float HalfLifeYears)
{
    return FMath::Max(0.0f, Amount) * DaoRetentionFraction(Years, HalfLifeYears);
}

FGuDaoSuccessionState UGuWorldDaoEcologySubsystem::EvaluateSuccession(
    const FGameplayTag& Path,
    const float Density,
    const float TotalDensity,
    const float MaturityYears)
{
    FGuDaoSuccessionState State;
    State.Path = Path;
    State.Density = FMath::Max(0.0f, Density);
    State.Share = State.Density / FMath::Max(0.0001f, TotalDensity);
    State.MaturityYears = FMath::Max(0.0f, MaturityYears);
    State.Stage = EGuDaoSuccessionStage::Trace;

    if (State.Density >= 40.0f && State.Share >= 0.12f)
    {
        State.Stage = EGuDaoSuccessionStage::Influenced;
    }
    if (State.Density >= 75.0f && State.Share >= 0.20f && State.MaturityYears >= 8.0f)
    {
        State.Stage = EGuDaoSuccessionStage::Established;
    }
    if (State.Density >= 110.0f && State.Share >= 0.30f && State.MaturityYears >= 25.0f)
    {
        State.Stage = EGuDaoSuccessionStage::Aligned;
    }
    if (State.Density >= 210.0f && State.Share >= 0.40f && State.MaturityYears >= 60.0f)
    {
        State.Stage = EGuDaoSuccessionStage::Transformed;
    }
    if (State.Density >= 420.0f && State.Share >= 0.50f && State.MaturityYears >= 160.0f)
    {
        State.Stage = EGuDaoSuccessionStage::PathDomain;
    }
    return State;
}

FGameplayTag UGuWorldDaoEcologySubsystem::PathTag(const TCHAR* Name) const
{
    return FGameplayTag::RequestGameplayTag(
        FName(*FString::Printf(TEXT("Data.Paths.%s"), Name)),
        false);
}

void UGuWorldDaoEcologySubsystem::RegisterDefaultRules()
{
    ConflictRules.Reset();
    ResourceRules.Reset();
    BeastRules.Reset();

    const auto AddConflict = [this](const TCHAR* A, const TCHAR* B, const float Strength)
    {
        const FGameplayTag PathA = PathTag(A);
        const FGameplayTag PathB = PathTag(B);
        if (!PathA.IsValid() || !PathB.IsValid()) return;
        FGuDaoConflictRule Rule;
        Rule.PathA = PathA;
        Rule.PathB = PathB;
        Rule.Strength = Strength;
        ConflictRules.Add(Rule);
    };
    AddConflict(TEXT("Fire"), TEXT("Water"), 0.90f);
    AddConflict(TEXT("Fire"), TEXT("Ice"), 0.90f);
    AddConflict(TEXT("Light"), TEXT("Dark"), 0.82f);
    AddConflict(TEXT("Wood"), TEXT("Metal"), 0.48f);
    AddConflict(TEXT("Earth"), TEXT("Wood"), 0.24f);
    AddConflict(TEXT("Lightning"), TEXT("Water"), 0.18f);

    const auto Names = [](std::initializer_list<const TCHAR*> Values)
    {
        TArray<FName> Out;
        Out.Reserve(static_cast<int32>(Values.size()));
        for (const TCHAR* Value : Values) Out.Add(FName(Value));
        return Out;
    };

    const auto AddResource = [this, &Names](
        const TCHAR* Id, const TCHAR* Display, const TCHAR* PathName, const int32 Rank,
        std::initializer_list<const TCHAR*> Substrates, const float Density,
        const float Share, const float Years, const TCHAR* Kind)
    {
        const FGameplayTag Path = PathTag(PathName);
        if (!Path.IsValid()) return;
        FGuSubstrateResourceRule Rule;
        Rule.Id = FName(Id);
        Rule.Name = FText::FromString(Display);
        Rule.Path = Path;
        Rule.Rank = Rank;
        Rule.Substrates = Names(Substrates);
        Rule.DensityRequired = Density;
        Rule.ShareRequired = Share;
        Rule.MaturityYears = Years;
        Rule.Kind = FName(Kind);
        ResourceRules.Add(Rule);
    };

    AddResource(TEXT("moon-shoot"), TEXT("Moon Shoot"), TEXT("Moon"), 1, {TEXT("bamboo"), TEXT("dense-bamboo")}, 58, .16f, 5, TEXT("plant"));
    AddResource(TEXT("crescent-bamboo"), TEXT("Crescent Bamboo"), TEXT("Moon"), 2, {TEXT("bamboo"), TEXT("dense-bamboo")}, 105, .25f, 18, TEXT("plant"));
    AddResource(TEXT("moon-water"), TEXT("Moon Water"), TEXT("Moon"), 1, {TEXT("still-water"), TEXT("flowing-water")}, 62, .17f, 6, TEXT("liquid"));
    AddResource(TEXT("condensed-moonlight"), TEXT("Condensed Moonlight"), TEXT("Moon"), 2, {TEXT("still-water")}, 125, .28f, 24, TEXT("essence"));
    AddResource(TEXT("lunar-stone"), TEXT("Lunar Stone"), TEXT("Moon"), 1, {TEXT("mountain"), TEXT("rock-face"), TEXT("rocky")}, 82, .20f, 12, TEXT("mineral"));
    AddResource(TEXT("moon-veined-rock"), TEXT("Moon-veined Rock"), TEXT("Moon"), 2, {TEXT("mountain"), TEXT("rock-face"), TEXT("rocky")}, 145, .30f, 30, TEXT("mineral"));
    AddResource(TEXT("moon-dew"), TEXT("Moon Dew"), TEXT("Moon"), 1, {TEXT("valley"), TEXT("wild-grass")}, 55, .15f, 5, TEXT("liquid"));
    AddResource(TEXT("crescent-grass"), TEXT("Crescent Grass"), TEXT("Moon"), 2, {TEXT("valley"), TEXT("wild-grass")}, 95, .23f, 15, TEXT("plant"));
    AddResource(TEXT("earth-root"), TEXT("Stone Root"), TEXT("Earth"), 1, {TEXT("bamboo"), TEXT("dense-bamboo"), TEXT("forest")}, 95, .26f, 14, TEXT("plant"));
    AddResource(TEXT("earth-ore-knot"), TEXT("Earth Ore Knot"), TEXT("Earth"), 2, {TEXT("mountain"), TEXT("rock-face"), TEXT("rocky")}, 120, .32f, 20, TEXT("mineral"));
    AddResource(TEXT("water-jade"), TEXT("Water Jade"), TEXT("Water"), 2, {TEXT("still-water"), TEXT("flowing-water")}, 100, .28f, 18, TEXT("mineral"));
    AddResource(TEXT("wood-spirit-shoot"), TEXT("Spirit Shoot"), TEXT("Wood"), 1, {TEXT("bamboo"), TEXT("dense-bamboo"), TEXT("forest")}, 90, .28f, 12, TEXT("plant"));
    AddResource(TEXT("poison-bamboo-sap"), TEXT("Venom Bamboo Sap"), TEXT("Poison"), 1, {TEXT("bamboo"), TEXT("dense-bamboo")}, 70, .20f, 10, TEXT("liquid"));
    AddResource(TEXT("sword-stone"), TEXT("Sword-marked Stone"), TEXT("Sword"), 2, {TEXT("mountain"), TEXT("rock-face"), TEXT("rocky")}, 110, .28f, 14, TEXT("mineral"));

    const auto AddBeast = [this, &Names](
        const TCHAR* Id, const TCHAR* Display, const TCHAR* PathName, const int32 Rank,
        std::initializer_list<const TCHAR*> Substrates, const float Density,
        const float Share, const float Years, const TCHAR* Mode)
    {
        const FGameplayTag Path = PathTag(PathName);
        if (!Path.IsValid()) return;
        FGuBeastSuccessionRule Rule;
        Rule.Id = FName(Id);
        Rule.Name = FText::FromString(Display);
        Rule.Path = Path;
        Rule.Rank = Rank;
        Rule.Substrates = Names(Substrates);
        Rule.DensityRequired = Density;
        Rule.ShareRequired = Share;
        Rule.MaturityYears = Years;
        Rule.Mode = FName(Mode);
        BeastRules.Add(Rule);
    };

    AddBeast(TEXT("stonehide-boar"), TEXT("Stonehide Boar"), TEXT("Earth"), 1, {TEXT("mountain"), TEXT("rocky"), TEXT("valley"), TEXT("wild-grass")}, 72, .20f, 8, TEXT("attract-or-emerge"));
    AddBeast(TEXT("rockback-bear"), TEXT("Rockback Bear"), TEXT("Earth"), 2, {TEXT("mountain"), TEXT("rock-face"), TEXT("rocky")}, 120, .30f, 24, TEXT("adapt-or-emerge"));
    AddBeast(TEXT("burrowing-stone-lizard"), TEXT("Burrowing Stone Lizard"), TEXT("Earth"), 2, {TEXT("mountain"), TEXT("rocky")}, 145, .34f, 30, TEXT("emerge"));
    AddBeast(TEXT("moonshade-wolf"), TEXT("Moonshade Wolf"), TEXT("Moon"), 1, {TEXT("bamboo"), TEXT("dense-bamboo"), TEXT("forest"), TEXT("wild-grass")}, 70, .20f, 8, TEXT("adapt-or-emerge"));
    AddBeast(TEXT("crescent-bamboo-deer"), TEXT("Crescent Bamboo Deer"), TEXT("Moon"), 1, {TEXT("bamboo"), TEXT("dense-bamboo")}, 92, .24f, 14, TEXT("emerge"));
    AddBeast(TEXT("moonfin-carp"), TEXT("Moonfin Carp"), TEXT("Moon"), 1, {TEXT("still-water"), TEXT("flowing-water")}, 75, .20f, 10, TEXT("emerge"));
    AddBeast(TEXT("moonpool-turtle"), TEXT("Moonpool Turtle"), TEXT("Moon"), 2, {TEXT("still-water")}, 130, .30f, 28, TEXT("emerge"));
    AddBeast(TEXT("grove-antler-deer"), TEXT("Grove-antler Deer"), TEXT("Wood"), 1, {TEXT("forest"), TEXT("bamboo"), TEXT("dense-bamboo")}, 95, .28f, 12, TEXT("attract-or-emerge"));
    AddBeast(TEXT("streamscale-crocodile"), TEXT("Streamscale Crocodile"), TEXT("Water"), 2, {TEXT("still-water"), TEXT("flowing-water")}, 120, .30f, 24, TEXT("attract-or-emerge"));
    AddBeast(TEXT("venom-bamboo-snake"), TEXT("Venom Bamboo Snake"), TEXT("Poison"), 1, {TEXT("bamboo"), TEXT("dense-bamboo"), TEXT("forest")}, 65, .20f, 10, TEXT("adapt-or-emerge"));
    AddBeast(TEXT("swordback-mantis"), TEXT("Swordback Mantis"), TEXT("Sword"), 2, {TEXT("rocky"), TEXT("mountain"), TEXT("wild-grass")}, 125, .30f, 22, TEXT("emerge"));
}

void UGuWorldDaoEcologySubsystem::InvalidateProfiles() const
{
    ProfileCache.Reset();
}

void UGuWorldDaoEcologySubsystem::InvalidateEventIndex()
{
    bEventIndexDirty = true;
    EventSpatialBuckets.Reset();
    GlobalEventIds.Reset();
    InvalidateProfiles();
}

bool UGuWorldDaoEcologySubsystem::RegisterRegionField(const FGuDaoRegionField& Field)
{
    if (Field.Id.IsNone() || Field.Radii.X <= 0.0f || Field.Radii.Y <= 0.0f)
    {
        return false;
    }
    FGuDaoRegionField Clean = Field;
    Clean.Radii.X = FMath::Max(1.0f, Clean.Radii.X);
    Clean.Radii.Y = FMath::Max(1.0f, Clean.Radii.Y);
    Clean.FalloffPower = FMath::Max(0.1f, Clean.FalloffPower);
    Clean.MaturityYears = FMath::Max(0.0f, Clean.MaturityYears);
    RegionFields.Add(Clean.Id, MoveTemp(Clean));
    InvalidateProfiles();
    return true;
}

bool UGuWorldDaoEcologySubsystem::RemoveRegionField(const FName RegionId)
{
    const bool bRemoved = RegionFields.Remove(RegionId) > 0;
    if (bRemoved) InvalidateProfiles();
    return bRemoved;
}

void UGuWorldDaoEcologySubsystem::ClearRegionFields()
{
    RegionFields.Reset();
    InvalidateProfiles();
}

bool UGuWorldDaoEcologySubsystem::RegisterActivityField(const FGuDaoActivityField& Field)
{
    if (Field.Id.IsNone() || Field.Radii.X <= 0.0f || Field.Radii.Y <= 0.0f || Field.Usages.IsEmpty())
    {
        return false;
    }

    FGuDaoActivityField Clean = Field;
    Clean.Radii.X = FMath::Max(1.0f, Clean.Radii.X);
    Clean.Radii.Y = FMath::Max(1.0f, Clean.Radii.Y);
    Clean.FalloffPower = FMath::Max(0.1f, Clean.FalloffPower);
    Clean.ActiveYears = FMath::Max(0.0f, Clean.ActiveYears);
    Clean.InactiveYears = FMath::Max(0.0f, Clean.InactiveYears);
    for (FGuDaoActivityUse& Use : Clean.Usages)
    {
        Use.Rank = ClampRank(Use.Rank);
        Use.ActivationsPerDay = FMath::Max(0.0f, Use.ActivationsPerDay);
        Use.Retention = FMath::Max(0.0f, Use.Retention);
    }
    ActivityFields.Add(Clean.Id, MoveTemp(Clean));
    InvalidateProfiles();
    return true;
}

bool UGuWorldDaoEcologySubsystem::RemoveActivityField(const FName ActivityId)
{
    const bool bRemoved = ActivityFields.Remove(ActivityId) > 0;
    if (bRemoved) InvalidateProfiles();
    return bRemoved;
}

void UGuWorldDaoEcologySubsystem::ClearActivityFields()
{
    ActivityFields.Reset();
    InvalidateProfiles();
}

FGuid UGuWorldDaoEcologySubsystem::RecordDaoEvent(const FGuWorldDaoEvent& Event)
{
    UWorld* World = GetWorld();
    if (!World || World->GetNetMode() == NM_Client)
    {
        return FGuid();
    }

    FGuWorldDaoEvent Clean = Event;
    if (!Clean.Id.IsValid()) Clean.Id = FGuid::NewGuid();
    Clean.SourceRank = ClampRank(Clean.SourceRank);
    Clean.AgeYears = FMath::Max(0.0f, Clean.AgeYears);
    Clean.Geometry.Radius = FMath::Max(1.0f, Clean.Geometry.Radius);
    Clean.Geometry.Radii.X = FMath::Max(1.0f, Clean.Geometry.Radii.X);
    Clean.Geometry.Radii.Y = FMath::Max(1.0f, Clean.Geometry.Radii.Y);
    Clean.Geometry.Width = FMath::Max(1.0f, Clean.Geometry.Width);
    Clean.Geometry.FalloffPower = FMath::Max(0.1f, Clean.Geometry.FalloffPower);

    for (auto It = Clean.DaoDeposit.CreateIterator(); It; ++It)
    {
        if (!It.Key().IsValid() || FMath::IsNearlyZero(It.Value())) It.RemoveCurrent();
    }
    if (Clean.DaoDeposit.IsEmpty())
    {
        return FGuid();
    }

    const FGuid EventId = Clean.Id;
    DynamicEvents.Add(EventId, MoveTemp(Clean));
    InvalidateEventIndex();
    return EventId;
}

bool UGuWorldDaoEcologySubsystem::RemoveDaoEvent(const FGuid& EventId)
{
    const bool bRemoved = DynamicEvents.Remove(EventId) > 0;
    if (bRemoved) InvalidateEventIndex();
    return bRemoved;
}

void UGuWorldDaoEcologySubsystem::ClearDynamicEvents()
{
    DynamicEvents.Reset();
    InvalidateEventIndex();
}

FGuid UGuWorldDaoEcologySubsystem::RecordGuActivation(
    const UGuDefinition* Definition,
    const FVector WorldLocation,
    const float Activations,
    const float Retention,
    const float RadiusCm)
{
    TMap<FGameplayTag, float> PathWeights;
    if (Definition && Definition->Path.IsValid())
    {
        PathWeights.Add(Definition->Path, 1.0f);
    }
    return RecordGuActivationWeighted(Definition, WorldLocation, PathWeights, Activations, Retention, RadiusCm);
}

FGuid UGuWorldDaoEcologySubsystem::RecordGuActivationWeighted(
    const UGuDefinition* Definition,
    const FVector& WorldLocation,
    const TMap<FGameplayTag, float>& PathWeights,
    const float Activations,
    const float Retention,
    const float RadiusCm)
{
    if (!Definition || Activations <= 0.0f || Retention <= 0.0f)
    {
        return FGuid();
    }

    float TotalWeight = 0.0f;
    for (const TPair<FGameplayTag, float>& Pair : PathWeights)
    {
        if (Pair.Key.IsValid()) TotalWeight += FMath::Max(0.0f, Pair.Value);
    }
    if (TotalWeight <= 0.0f)
    {
        return FGuid();
    }

    const int32 Rank = ClampRank(Definition->Rank);
    const float Mass = TraceUnitsForRank(Rank) * Activations * Retention;

    FGuWorldDaoEvent Event;
    Event.Id = FGuid::NewGuid();
    Event.Kind = TEXT("gu-trace");
    Event.SourceId = Definition->GetFName();
    Event.SourceRank = Rank;
    Event.AgeYears = 0.0f;
    Event.bPermanent = false;
    Event.HalfLifeYears = TraceHalfLifeYearsForRank(Rank);
    Event.Geometry.Type = EGuDaoEventGeometryType::Circle;
    Event.Geometry.Center = FVector2D(WorldLocation.X, WorldLocation.Y);
    Event.Geometry.Radius = FMath::Max(1200.0f, RadiusCm);
    Event.Geometry.FalloffPower = 1.65f;

    for (const TPair<FGameplayTag, float>& Pair : PathWeights)
    {
        if (!Pair.Key.IsValid()) continue;
        const float Weight = FMath::Max(0.0f, Pair.Value);
        if (Weight <= 0.0f) continue;
        Event.DaoDeposit.Add(Pair.Key, Mass * Weight / TotalWeight);
    }

    return RecordDaoEvent(Event);
}

bool UGuWorldDaoEcologySubsystem::RegisterWildGuHabitatRule(const FGuWildGuHabitatRule& Rule)
{
    if (Rule.DefinitionId.IsNone() || !Rule.Path.IsValid() || Rule.Rank < 1 || Rule.Rank > 9)
    {
        return false;
    }
    FGuWildGuHabitatRule Clean = Rule;
    Clean.DensityRequired = FMath::Max(0.0f, Clean.DensityRequired);
    Clean.ShareRequired = FMath::Clamp(Clean.ShareRequired, 0.0f, 1.0f);
    Clean.MaturityYears = FMath::Max(0.0f, Clean.MaturityYears);

    const int32 Existing = WildGuRules.IndexOfByPredicate([&Clean](const FGuWildGuHabitatRule& Candidate)
    {
        return Candidate.DefinitionId == Clean.DefinitionId;
    });
    if (Existing != INDEX_NONE) WildGuRules[Existing] = Clean;
    else WildGuRules.Add(Clean);
    InvalidateProfiles();
    return true;
}

bool UGuWorldDaoEcologySubsystem::RegisterWildGuDefinition(FName DefinitionId, const UGuDefinition* Definition)
{
    if (!Definition || !Definition->Path.IsValid()) return false;
    if (DefinitionId.IsNone()) DefinitionId = Definition->GetFName();
    const int32 Rank = ClampRank(Definition->Rank);

    FGuWildGuHabitatRule Rule;
    Rule.DefinitionId = DefinitionId;
    Rule.Name = Definition->Name;
    Rule.Path = Definition->Path;
    Rule.Rank = Rank;
    Rule.DensityRequired = WildGuDensityForRank(Rank);
    Rule.ShareRequired = FMath::Min(0.58f, 0.16f + Rank * 0.035f);
    Rule.MaturityYears = WildGuMaturityYearsForRank(Rank);
    return RegisterWildGuHabitatRule(Rule);
}

void UGuWorldDaoEcologySubsystem::ClearWildGuHabitatRules()
{
    WildGuRules.Reset();
    InvalidateProfiles();
}

void UGuWorldDaoEcologySubsystem::AdvanceEcologyYears(const float Years)
{
    if (Years <= 0.0f) return;
    UWorld* World = GetWorld();
    if (!World || World->GetNetMode() == NM_Client) return;

    TArray<FGuid> ToRemove;
    for (TPair<FGuid, FGuWorldDaoEvent>& Pair : DynamicEvents)
    {
        FGuWorldDaoEvent& Event = Pair.Value;
        if (!Event.bPermanent) Event.AgeYears += Years;

        if (!Event.bPermanent && EventPersistence(Event) < 0.000001f)
        {
            ToRemove.Add(Pair.Key);
        }
    }
    for (const FGuid& Id : ToRemove) DynamicEvents.Remove(Id);

    if (!ToRemove.IsEmpty()) InvalidateEventIndex();
    else InvalidateProfiles();
}

float UGuWorldDaoEcologySubsystem::RegionScale(const FGuDaoRegionField& Field, const FVector2D& Point) const
{
    const float Rx = FMath::Max(1.0f, Field.Radii.X);
    const float Ry = FMath::Max(1.0f, Field.Radii.Y);
    const FVector2D D = Point - Field.Center;
    const float Normalized = FMath::Sqrt(FMath::Square(D.X / Rx) + FMath::Square(D.Y / Ry));
    if (Normalized >= 1.0f) return 0.0f;
    return FMath::Pow(FMath::Max(0.0f, 1.0f - Normalized * Normalized), FMath::Max(0.1f, Field.FalloffPower));
}

float UGuWorldDaoEcologySubsystem::ActivityScale(const FGuDaoActivityField& Field, const FVector2D& Point) const
{
    const float Rx = FMath::Max(1.0f, Field.Radii.X);
    const float Ry = FMath::Max(1.0f, Field.Radii.Y);
    const FVector2D D = Point - Field.Center;
    const float Normalized = FMath::Sqrt(FMath::Square(D.X / Rx) + FMath::Square(D.Y / Ry));
    if (Normalized >= 1.0f) return 0.0f;
    return FMath::Pow(FMath::Max(0.0f, 1.0f - Normalized * Normalized), FMath::Max(0.1f, Field.FalloffPower));
}

FName UGuWorldDaoEcologySubsystem::ResolveSubstrateAt(const FVector2D& Point) const
{
    FName Best = TEXT("wild-grass");
    float BestScale = 0.0f;
    for (const TPair<FName, FGuDaoRegionField>& Pair : RegionFields)
    {
        const float Scale = RegionScale(Pair.Value, Point);
        if (Scale > BestScale && !Pair.Value.Substrate.IsNone())
        {
            BestScale = Scale;
            Best = Pair.Value.Substrate;
        }
    }
    return Best;
}

void UGuWorldDaoEcologySubsystem::AddSubstrateBaseline(
    const FName Substrate,
    TMap<FGameplayTag, float>& Marks,
    TMap<FGameplayTag, float>& Ages) const
{
    const auto Add = [this, &Marks, &Ages](const TCHAR* PathName, const float Amount)
    {
        AddMatureMark(Marks, Ages, PathTag(PathName), Amount, 500.0f);
    };

    if (Substrate == TEXT("dense-bamboo"))
    {
        Add(TEXT("Wood"), 74); Add(TEXT("Earth"), 13); Add(TEXT("Water"), 15); Add(TEXT("Qi"), 7);
    }
    else if (Substrate == TEXT("bamboo") || Substrate == TEXT("forest"))
    {
        Add(TEXT("Wood"), 52); Add(TEXT("Earth"), 13); Add(TEXT("Water"), 11); Add(TEXT("Qi"), 6);
    }
    else if (Substrate == TEXT("mountain"))
    {
        Add(TEXT("Earth"), 68); Add(TEXT("Metal"), 24); Add(TEXT("Wind"), 11); Add(TEXT("Wood"), 7);
    }
    else if (Substrate == TEXT("rock-face"))
    {
        Add(TEXT("Earth"), 92); Add(TEXT("Metal"), 36); Add(TEXT("Wind"), 17); Add(TEXT("Wood"), 3);
    }
    else if (Substrate == TEXT("foothills"))
    {
        Add(TEXT("Earth"), 32); Add(TEXT("Wood"), 16); Add(TEXT("Metal"), 8); Add(TEXT("Water"), 5);
    }
    else if (Substrate == TEXT("rocky"))
    {
        Add(TEXT("Earth"), 52); Add(TEXT("Metal"), 30); Add(TEXT("Wind"), 10); Add(TEXT("Wood"), 3);
    }
    else if (Substrate == TEXT("valley"))
    {
        Add(TEXT("Wood"), 24); Add(TEXT("Water"), 28); Add(TEXT("Earth"), 12); Add(TEXT("Qi"), 5);
    }
    else if (Substrate == TEXT("still-water"))
    {
        Add(TEXT("Water"), 78); Add(TEXT("Earth"), 4); Add(TEXT("Wood"), 5);
    }
    else if (Substrate == TEXT("flowing-water"))
    {
        Add(TEXT("Water"), 52); Add(TEXT("Wind"), 13); Add(TEXT("Qi"), 4); Add(TEXT("Wood"), 4);
    }
    else if (Substrate == TEXT("settled"))
    {
        Add(TEXT("Earth"), 18); Add(TEXT("Wood"), 12); Add(TEXT("Human"), 18); Add(TEXT("Formation"), 4);
    }
    else
    {
        Add(TEXT("Wood"), 24); Add(TEXT("Earth"), 18); Add(TEXT("Water"), 7); Add(TEXT("Qi"), 5);
    }
}

float UGuWorldDaoEcologySubsystem::EventScale(const FGuWorldDaoEvent& Event, const FVector2D& Point) const
{
    const FGuDaoEventGeometry& Geometry = Event.Geometry;
    float NormalizedDistance = 2.0f;

    if (Geometry.Type == EGuDaoEventGeometryType::Circle)
    {
        NormalizedDistance = FVector2D::Distance(Point, Geometry.Center) / FMath::Max(1.0f, Geometry.Radius);
    }
    else if (Geometry.Type == EGuDaoEventGeometryType::Ellipse)
    {
        const FVector2D D = Point - Geometry.Center;
        const float Rx = FMath::Max(1.0f, Geometry.Radii.X);
        const float Ry = FMath::Max(1.0f, Geometry.Radii.Y);
        NormalizedDistance = FMath::Sqrt(FMath::Square(D.X / Rx) + FMath::Square(D.Y / Ry));
        if (NormalizedDistance >= 1.0f) return 0.0f;
        return FMath::Pow(FMath::Max(0.0f, 1.0f - NormalizedDistance * NormalizedDistance), 1.35f);
    }
    else
    {
        if (Geometry.Points.Num() < 2) return 0.0f;
        float MinDistance = TNumericLimits<float>::Max();
        for (int32 Index = 0; Index + 1 < Geometry.Points.Num(); ++Index)
        {
            MinDistance = FMath::Min(MinDistance, DistanceToSegment(Point, Geometry.Points[Index], Geometry.Points[Index + 1]));
        }
        NormalizedDistance = MinDistance / FMath::Max(1.0f, Geometry.Width);
    }

    if (NormalizedDistance >= 1.0f) return 0.0f;
    return FMath::Pow(FMath::Max(0.0f, 1.0f - NormalizedDistance), FMath::Max(0.1f, Geometry.FalloffPower));
}

float UGuWorldDaoEcologySubsystem::EventPersistence(const FGuWorldDaoEvent& Event) const
{
    if (Event.bPermanent) return 1.0f;
    const float HalfLife = Event.HalfLifeYears > 0.0f
        ? Event.HalfLifeYears
        : (Event.Kind == FName(TEXT("gu-trace")) ? TraceHalfLifeYearsForRank(Event.SourceRank) : 5000.0f);
    return DaoRetentionFraction(Event.AgeYears, HalfLife);
}

FBox2D UGuWorldDaoEcologySubsystem::EventBounds(const FGuWorldDaoEvent& Event) const
{
    const FGuDaoEventGeometry& Geometry = Event.Geometry;
    if (Geometry.Type == EGuDaoEventGeometryType::Circle)
    {
        const FVector2D R(FMath::Max(1.0f, Geometry.Radius));
        return FBox2D(Geometry.Center - R, Geometry.Center + R);
    }
    if (Geometry.Type == EGuDaoEventGeometryType::Ellipse)
    {
        const FVector2D R(FMath::Max(1.0f, Geometry.Radii.X), FMath::Max(1.0f, Geometry.Radii.Y));
        return FBox2D(Geometry.Center - R, Geometry.Center + R);
    }

    if (Geometry.Points.IsEmpty())
    {
        const FVector2D R(FMath::Max(1.0f, Geometry.Width));
        return FBox2D(Geometry.Center - R, Geometry.Center + R);
    }

    FBox2D Bounds(ForceInit);
    for (const FVector2D& Point : Geometry.Points) Bounds += Point;
    const FVector2D W(FMath::Max(1.0f, Geometry.Width));
    return FBox2D(Bounds.Min - W, Bounds.Max + W);
}

void UGuWorldDaoEcologySubsystem::RebuildEventSpatialIndex() const
{
    EventSpatialBuckets.Reset();
    GlobalEventIds.Reset();
    bEventIndexDirty = false;

    if (DynamicEvents.Num() < EventIndexThreshold) return;

    for (const TPair<FGuid, FGuWorldDaoEvent>& Pair : DynamicEvents)
    {
        const FBox2D Bounds = EventBounds(Pair.Value);
        if (!Bounds.bIsValid)
        {
            GlobalEventIds.Add(Pair.Key);
            continue;
        }

        const int32 MinX = FMath::FloorToInt(Bounds.Min.X / EventIndexCellSizeCm);
        const int32 MaxX = FMath::FloorToInt(Bounds.Max.X / EventIndexCellSizeCm);
        const int32 MinY = FMath::FloorToInt(Bounds.Min.Y / EventIndexCellSizeCm);
        const int32 MaxY = FMath::FloorToInt(Bounds.Max.Y / EventIndexCellSizeCm);
        const int64 CellCount = static_cast<int64>(MaxX - MinX + 1) * static_cast<int64>(MaxY - MinY + 1);

        if (CellCount > MaxCellsPerIndexedEvent)
        {
            GlobalEventIds.Add(Pair.Key);
            continue;
        }

        for (int32 Y = MinY; Y <= MaxY; ++Y)
        {
            for (int32 X = MinX; X <= MaxX; ++X)
            {
                EventSpatialBuckets.FindOrAdd(FIntPoint(X, Y)).Add(Pair.Key);
            }
        }
    }
}

void UGuWorldDaoEcologySubsystem::GatherNearbyEventIds(const FVector2D& Point, TArray<FGuid>& OutIds) const
{
    OutIds.Reset();
    if (DynamicEvents.Num() < EventIndexThreshold)
    {
        DynamicEvents.GenerateKeyArray(OutIds);
        return;
    }

    if (bEventIndexDirty) RebuildEventSpatialIndex();

    TSet<FGuid> Unique = GlobalEventIds;
    const FIntPoint Cell(
        FMath::FloorToInt(Point.X / EventIndexCellSizeCm),
        FMath::FloorToInt(Point.Y / EventIndexCellSizeCm));
    if (const TArray<FGuid>* Bucket = EventSpatialBuckets.Find(Cell))
    {
        for (const FGuid& Id : *Bucket) Unique.Add(Id);
    }
    OutIds.Reserve(Unique.Num());
    for (const FGuid& Id : Unique) OutIds.Add(Id);
}

float UGuWorldDaoEcologySubsystem::ComputeInteractionPressure(const TMap<FGameplayTag, float>& Marks) const
{
    float Total = 0.0f;
    for (const TPair<FGameplayTag, float>& Pair : Marks) Total += FMath::Max(0.0f, Pair.Value);
    Total = FMath::Max(0.0001f, Total);

    float Pressure = 0.0f;
    for (const FGuDaoConflictRule& Rule : ConflictRules)
    {
        const float A = FMath::Max(0.0f, Marks.FindRef(Rule.PathA));
        const float B = FMath::Max(0.0f, Marks.FindRef(Rule.PathB));
        if (A > 0.0f && B > 0.0f)
        {
            Pressure += FMath::Min(A, B) * FMath::Max(0.0f, Rule.Strength) / Total;
        }
    }
    return FMath::Min(1.0f, Pressure);
}

void UGuWorldDaoEcologySubsystem::ApplyConflictTurnover(
    TMap<FGameplayTag, float>& Marks,
    TMap<FGameplayTag, float>& Ages,
    TMap<FGameplayTag, float>& OutLosses) const
{
    OutLosses.Reset();
    for (const FGuDaoConflictRule& Rule : ConflictRules)
    {
        const float A = FMath::Max(0.0f, Marks.FindRef(Rule.PathA));
        const float B = FMath::Max(0.0f, Marks.FindRef(Rule.PathB));
        if (A <= 0.0f || B <= 0.0f) continue;

        const float Loss = FMath::Min(A, B) * FMath::Max(0.0f, Rule.Strength) * 0.11f;
        if (Loss <= 0.001f) continue;

        Marks.FindOrAdd(Rule.PathA) = FMath::Max(0.0f, A - Loss);
        Marks.FindOrAdd(Rule.PathB) = FMath::Max(0.0f, B - Loss);
        OutLosses.FindOrAdd(Rule.PathA) += Loss;
        OutLosses.FindOrAdd(Rule.PathB) += Loss;
        if (Marks.FindRef(Rule.PathA) <= 0.001f) Ages.FindOrAdd(Rule.PathA) = 0.0f;
        if (Marks.FindRef(Rule.PathB) <= 0.001f) Ages.FindOrAdd(Rule.PathB) = 0.0f;
    }
}

FGuDaoEcologyProfile UGuWorldDaoEcologySubsystem::BuildProfileAt(const FVector2D& Point) const
{
    FGuDaoEcologyProfile Profile;
    Profile.Substrate = ResolveSubstrateAt(Point);
    AddSubstrateBaseline(Profile.Substrate, Profile.Marks, Profile.PathMaturityYears);

    for (const TPair<FName, FGuDaoRegionField>& Pair : RegionFields)
    {
        const FGuDaoRegionField& Field = Pair.Value;
        const float Scale = RegionScale(Field, Point);
        if (Scale <= 0.0f) continue;
        for (const TPair<FGameplayTag, float>& Mark : Field.DaoMarks)
        {
            AddMatureMark(
                Profile.Marks,
                Profile.PathMaturityYears,
                Mark.Key,
                Mark.Value * Scale,
                Field.MaturityYears);
        }
    }

    // Long-term clan/settlement activity is accumulated from actual Gu-use rates.
    // Continuous input approaches a turnover equilibrium instead of growing forever.
    for (const TPair<FName, FGuDaoActivityField>& Pair : ActivityFields)
    {
        const FGuDaoActivityField& Field = Pair.Value;
        const float Scale = ActivityScale(Field, Point);
        if (Scale <= 0.0f) continue;

        for (const FGuDaoActivityUse& Use : Field.Usages)
        {
            if (!Use.Path.IsValid() || Use.ActivationsPerDay <= 0.0f || Use.Retention <= 0.0f) continue;
            const int32 Rank = ClampRank(Use.Rank);
            const float AnnualRate = TraceUnitsForRank(Rank) * Use.ActivationsPerDay * 365.2425f * Use.Retention;
            const float HalfLife = TraceHalfLifeYearsForRank(Rank);
            const float ActiveStock = ContinuousDaoStock(AnnualRate, Field.ActiveYears, HalfLife);
            const float CurrentStock = DecayDaoStock(ActiveStock, Field.InactiveYears, HalfLife);
            AddMatureMark(
                Profile.Marks,
                Profile.PathMaturityYears,
                Use.Path,
                CurrentStock * Scale,
                Field.ActiveYears);
        }
    }

    TArray<FGuid> EventIds;
    GatherNearbyEventIds(Point, EventIds);
    for (const FGuid& EventId : EventIds)
    {
        const FGuWorldDaoEvent* Event = DynamicEvents.Find(EventId);
        if (!Event) continue;
        const float Scale = EventScale(*Event, Point) * EventPersistence(*Event);
        if (Scale <= 0.000001f) continue;
        for (const TPair<FGameplayTag, float>& Mark : Event->DaoDeposit)
        {
            AddMatureMark(
                Profile.Marks,
                Profile.PathMaturityYears,
                Mark.Key,
                Mark.Value * Scale,
                Event->AgeYears);
        }
    }

    // v7.9.84 feeds mature local life back into Dao density once before final
    // conflict turnover. This makes ecology self-reinforcing without an infinite loop.
    FinalizeProfile(Profile);
    PopulateDerivedEcology(Profile);
    AddBioticFeedback(Profile);
    ApplyConflictTurnover(Profile.Marks, Profile.PathMaturityYears, Profile.ConflictLosses);
    FinalizeProfile(Profile);
    PopulateDerivedEcology(Profile);
    return Profile;
}

void UGuWorldDaoEcologySubsystem::FinalizeProfile(FGuDaoEcologyProfile& Profile) const
{
    TArray<FGameplayTag> ToRemove;
    Profile.TotalDensity = 0.0f;
    for (TPair<FGameplayTag, float>& Pair : Profile.Marks)
    {
        Pair.Value = FMath::Max(0.0f, Pair.Value);
        if (Pair.Value <= 0.05f)
        {
            ToRemove.Add(Pair.Key);
            continue;
        }
        Profile.TotalDensity += Pair.Value;
    }
    for (const FGameplayTag& Path : ToRemove)
    {
        Profile.Marks.Remove(Path);
        Profile.PathMaturityYears.Remove(Path);
    }

    TArray<TPair<FGameplayTag, float>> Entries;
    Entries.Reserve(Profile.Marks.Num());
    for (const TPair<FGameplayTag, float>& Pair : Profile.Marks) Entries.Add(Pair);
    Entries.Sort([](const TPair<FGameplayTag, float>& A, const TPair<FGameplayTag, float>& B)
    {
        return A.Value > B.Value;
    });

    if (!Entries.IsEmpty())
    {
        Profile.DominantPath = Entries[0].Key;
        Profile.DominantDensity = Entries[0].Value;
    }
    if (Entries.Num() > 1) Profile.SecondaryPath = Entries[1].Key;

    const float Total = FMath::Max(0.0001f, Profile.TotalDensity);
    const float SecondaryDensity = Entries.Num() > 1 ? Entries[1].Value : 0.0f;
    Profile.Coherence = Profile.DominantDensity / Total;
    Profile.Conflict = FMath::Max(0.0f, 1.0f - (Profile.DominantDensity + SecondaryDensity * 0.45f) / Total);
    Profile.InteractionPressure = ComputeInteractionPressure(Profile.Marks);
    Profile.DominantSuccession = EvaluateSuccession(
        Profile.DominantPath,
        Profile.DominantDensity,
        Profile.TotalDensity,
        Profile.PathMaturityYears.FindRef(Profile.DominantPath));
}

void UGuWorldDaoEcologySubsystem::AddBioticFeedback(FGuDaoEcologyProfile& Profile) const
{
    for (const FGuDaoEcologyCandidate& Resource : Profile.ResourceCandidates)
    {
        // Browser v7.9.84 feeds substrate succession resources back into Dao density;
        // generic potential-only features do not create standing biotic mass by themselves.
        if (Resource.Kind == FName(TEXT("resource-potential"))) continue;
        if (!Resource.Path.IsValid()) continue;
        const int32 Rank = ClampRank(Resource.Rank);
        const float Scale = FMath::Min(2.5f, FMath::Max(0.0f, Resource.Intensity) * 0.22f);
        const float Amount = BioticStanding[Rank] * Scale;
        if (Amount <= 0.05f) continue;
        AddMatureMark(Profile.Marks, Profile.PathMaturityYears, Resource.Path, Amount,
            FMath::Max(Resource.MaturityYears, Profile.PathMaturityYears.FindRef(Resource.Path)));
    }

    for (const FGuDaoEcologyCandidate& Beast : Profile.BeastCandidates)
    {
        if (!Beast.Path.IsValid()) continue;
        const int32 Rank = ClampRank(Beast.Rank);
        const float Scale = FMath::Min(2.5f, FMath::Max(0.0f, Beast.Intensity) * 0.16f);
        const float Amount = BioticStanding[Rank] * Scale;
        if (Amount <= 0.05f) continue;
        AddMatureMark(Profile.Marks, Profile.PathMaturityYears, Beast.Path, Amount,
            FMath::Max(Beast.MaturityYears, Profile.PathMaturityYears.FindRef(Beast.Path)));
    }

    for (const FGuDaoEcologyCandidate& WildGu : Profile.WildGuCandidates)
    {
        if (!WildGu.Path.IsValid()) continue;
        const int32 Rank = ClampRank(WildGu.Rank);
        const float Scale = FMath::Min(1.6f, FMath::Max(0.0f, WildGu.Intensity) * 0.08f);
        const float Amount = BioticStanding[Rank] * Scale;
        if (Amount <= 0.05f) continue;
        AddMatureMark(Profile.Marks, Profile.PathMaturityYears, WildGu.Path, Amount,
            FMath::Max(WildGu.MaturityYears, Profile.PathMaturityYears.FindRef(WildGu.Path)));
    }
}

void UGuWorldDaoEcologySubsystem::PopulateDerivedEcology(FGuDaoEcologyProfile& Profile) const
{
    Profile.ResourceCandidates.Reset();
    Profile.BeastCandidates.Reset();
    Profile.WildGuCandidates.Reset();
    Profile.BeastPathAttraction.Reset();
    Profile.LandscapeTraits.Reset();

    const float Total = FMath::Max(0.0001f, Profile.TotalDensity);
    const float Interaction = FMath::Max(0.0f, Profile.InteractionPressure);

    for (const FGuSubstrateResourceRule& Rule : ResourceRules)
    {
        if (!ContainsSubstrate(Rule.Substrates, Profile.Substrate)) continue;
        const float Density = FMath::Max(0.0f, Profile.Marks.FindRef(Rule.Path));
        const float Share = Density / Total;
        const float Age = FMath::Max(0.0f, Profile.PathMaturityYears.FindRef(Rule.Path));
        if (Density < Rule.DensityRequired || Share < Rule.ShareRequired || Age < Rule.MaturityYears) continue;

        FGuDaoEcologyCandidate Candidate;
        Candidate.Id = Rule.Id;
        Candidate.Name = Rule.Name;
        Candidate.Path = Rule.Path;
        Candidate.Rank = Rule.Rank;
        Candidate.Kind = Rule.Kind;
        Candidate.Substrate = Profile.Substrate;
        Candidate.LocalDensity = Density;
        Candidate.PathShare = Share;
        Candidate.MaturityYears = Age;
        Candidate.Intensity = FMath::Min(6.0f,
            FMath::Min(6.0f, Density / FMath::Max(0.0001f, Rule.DensityRequired)) *
            FMath::Min(3.0f, Share / FMath::Max(0.0001f, Rule.ShareRequired)) *
            FMath::Min(2.0f, FMath::Max(1.0f, Age / FMath::Max(0.0001f, Rule.MaturityYears))) /
            (1.0f + Interaction * 1.2f));
        Profile.ResourceCandidates.Add(Candidate);
    }
    // Generic Great-Dao resource formation. A newly introduced Path can shape a
    // habitat without requiring one handcrafted resource rule for every Path.
    for (const TPair<FGameplayTag, float>& Pair : Profile.Marks)
    {
        const float Density = FMath::Max(0.0f, Pair.Value);
        const float Share = Density / Total;
        const float Age = FMath::Max(0.0f, Profile.PathMaturityYears.FindRef(Pair.Key));
        if (Density < 65.0f || Share < 0.18f || Age < 8.0f) continue;

        FString PathName = Pair.Key.ToString();
        PathName.RemoveFromStart(TEXT("Data.Paths."));
        FGuDaoEcologyCandidate Candidate;
        Candidate.Id = FName(*FString::Printf(TEXT("%s-dao-resource-ground"), *PathName.ToLower()));
        Candidate.Name = FText::FromString(FString::Printf(TEXT("%s-path resource formation ground"), *PathName));
        Candidate.Path = Pair.Key;
        Candidate.Rank = 1;
        Candidate.Kind = TEXT("resource-potential");
        Candidate.Substrate = Profile.Substrate;
        Candidate.LocalDensity = Density;
        Candidate.PathShare = Share;
        Candidate.MaturityYears = Age;
        Candidate.Intensity = FMath::Min(6.0f, Density / 65.0f * Share / 0.18f) /
            (1.0f + Interaction * 0.5f);
        Profile.ResourceCandidates.Add(Candidate);
    }

    Profile.ResourceCandidates.Sort([](const FGuDaoEcologyCandidate& A, const FGuDaoEcologyCandidate& B)
    {
        return A.Intensity > B.Intensity;
    });

    for (const FGuBeastSuccessionRule& Rule : BeastRules)
    {
        if (!ContainsSubstrate(Rule.Substrates, Profile.Substrate)) continue;
        const float Density = FMath::Max(0.0f, Profile.Marks.FindRef(Rule.Path));
        const float Share = Density / Total;
        const float Age = FMath::Max(0.0f, Profile.PathMaturityYears.FindRef(Rule.Path));
        if (Density < Rule.DensityRequired || Share < Rule.ShareRequired || Age < Rule.MaturityYears) continue;

        FGuDaoEcologyCandidate Candidate;
        Candidate.Id = Rule.Id;
        Candidate.Name = Rule.Name;
        Candidate.Path = Rule.Path;
        Candidate.Rank = Rule.Rank;
        Candidate.Kind = Rule.Mode;
        Candidate.Substrate = Profile.Substrate;
        Candidate.LocalDensity = Density;
        Candidate.PathShare = Share;
        Candidate.MaturityYears = Age;
        Candidate.Intensity = FMath::Min(6.0f,
            FMath::Min(6.0f, Density / FMath::Max(0.0001f, Rule.DensityRequired)) *
            FMath::Min(3.0f, Share / FMath::Max(0.0001f, Rule.ShareRequired)) *
            FMath::Min(2.0f, FMath::Max(1.0f, Age / FMath::Max(0.0001f, Rule.MaturityYears))) /
            (1.0f + Interaction));
        Profile.BeastCandidates.Add(Candidate);
    }
    Profile.BeastCandidates.Sort([](const FGuDaoEcologyCandidate& A, const FGuDaoEcologyCandidate& B)
    {
        if (!FMath::IsNearlyEqual(A.Intensity, B.Intensity)) return A.Intensity > B.Intensity;
        return A.Rank < B.Rank;
    });

    for (const FGuWildGuHabitatRule& Rule : WildGuRules)
    {
        const float Density = FMath::Max(0.0f, Profile.Marks.FindRef(Rule.Path));
        const float Share = Density / Total;
        const float Age = FMath::Max(0.0f, Profile.PathMaturityYears.FindRef(Rule.Path));
        if (Density < Rule.DensityRequired || Share < Rule.ShareRequired || Age < Rule.MaturityYears) continue;

        FGuDaoEcologyCandidate Candidate;
        Candidate.Id = Rule.DefinitionId;
        Candidate.Name = Rule.Name;
        Candidate.Path = Rule.Path;
        Candidate.Rank = Rule.Rank;
        Candidate.Kind = TEXT("wild-gu");
        Candidate.Substrate = Profile.Substrate;
        Candidate.LocalDensity = Density;
        Candidate.PathShare = Share;
        Candidate.MaturityYears = Age;
        Candidate.Intensity =
            FMath::Min(6.0f, Density / FMath::Max(0.0001f, Rule.DensityRequired)) *
            FMath::Min(3.0f, Share / FMath::Max(0.0001f, Rule.ShareRequired)) *
            FMath::Min(2.0f, FMath::Max(1.0f, Age / FMath::Max(0.0001f, Rule.MaturityYears))) /
            (1.0f + Interaction * 1.4f);
        Profile.WildGuCandidates.Add(Candidate);
    }
    Profile.WildGuCandidates.Sort([](const FGuDaoEcologyCandidate& A, const FGuDaoEcologyCandidate& B)
    {
        if (!FMath::IsNearlyEqual(A.Intensity, B.Intensity)) return A.Intensity > B.Intensity;
        return A.Rank < B.Rank;
    });

    for (const TPair<FGameplayTag, float>& Pair : Profile.Marks)
    {
        const float Density = FMath::Max(0.0f, Pair.Value);
        const float Share = Density / Total;
        const float Age = FMath::Max(0.0f, Profile.PathMaturityYears.FindRef(Pair.Key));

        if (Density >= 28.0f && Share >= 0.08f)
        {
            FGuDaoPathAttraction Attraction;
            Attraction.Path = Pair.Key;
            Attraction.Density = Density;
            Attraction.Share = Share;
            Attraction.MaturityYears = Age;
            const float Maturity = 1.0f + FMath::Min(1.5f, Age / 120.0f);
            Attraction.HabitatMultiplier = FMath::Min(8.0f,
                (0.55f + Density / 90.0f) * (0.65f + Share * 1.8f) * Maturity /
                (1.0f + Interaction * 0.7f));
            Profile.BeastPathAttraction.Add(Attraction);
        }

        const FGuDaoSuccessionState State = EvaluateSuccession(Pair.Key, Density, Total, Age);
        if (State.Stage == EGuDaoSuccessionStage::Aligned ||
            State.Stage == EGuDaoSuccessionStage::Transformed ||
            State.Stage == EGuDaoSuccessionStage::PathDomain)
        {
            FGuDaoLandscapeTrait Trait;
            Trait.Path = Pair.Key;
            Trait.Stage = State.Stage;
            Trait.Intensity = FMath::Min(6.0f,
                Density / 110.0f * Share / 0.30f);
            Profile.LandscapeTraits.Add(Trait);
        }
    }
    Profile.BeastPathAttraction.Sort([](const FGuDaoPathAttraction& A, const FGuDaoPathAttraction& B)
    {
        return A.HabitatMultiplier > B.HabitatMultiplier;
    });
    Profile.LandscapeTraits.Sort([](const FGuDaoLandscapeTrait& A, const FGuDaoLandscapeTrait& B)
    {
        return A.Intensity > B.Intensity;
    });
}

FGuDaoEcologyProfile UGuWorldDaoEcologySubsystem::GetProfileAt(const FVector WorldLocation) const
{
    const FVector2D Point(WorldLocation.X, WorldLocation.Y);
    const FIntPoint Key(
        FMath::FloorToInt(Point.X / QueryCellSizeCm),
        FMath::FloorToInt(Point.Y / QueryCellSizeCm));

    if (const FGuDaoEcologyProfile* Cached = ProfileCache.Find(Key))
    {
        return *Cached;
    }

    const FVector2D SamplePoint(
        (static_cast<float>(Key.X) + 0.5f) * QueryCellSizeCm,
        (static_cast<float>(Key.Y) + 0.5f) * QueryCellSizeCm);
    FGuDaoEcologyProfile Result = BuildProfileAt(SamplePoint);
    if (ProfileCache.Num() >= MaxProfileCacheEntries)
    {
        ProfileCache.Reset();
    }
    ProfileCache.Add(Key, Result);
    return Result;
}

TArray<FGuDaoEcologyCandidate> UGuWorldDaoEcologySubsystem::GetWildGuCandidatesAt(
    const FVector WorldLocation,
    const int32 MaxRank) const
{
    TArray<FGuDaoEcologyCandidate> Result = GetProfileAt(WorldLocation).WildGuCandidates;
    Result.RemoveAll([MaxRank](const FGuDaoEcologyCandidate& Candidate)
    {
        return Candidate.Rank > FMath::Clamp(MaxRank, 1, 9);
    });
    return Result;
}

TArray<FGuDaoEcologyCandidate> UGuWorldDaoEcologySubsystem::GetResourceCandidatesAt(const FVector WorldLocation) const
{
    return GetProfileAt(WorldLocation).ResourceCandidates;
}

TArray<FGuDaoEcologyCandidate> UGuWorldDaoEcologySubsystem::GetBeastCandidatesAt(const FVector WorldLocation) const
{
    return GetProfileAt(WorldLocation).BeastCandidates;
}

void UGuWorldDaoEcologySubsystem::ExportDynamicEvents(TArray<FGuWorldDaoEvent>& OutEvents) const
{
    DynamicEvents.GenerateValueArray(OutEvents);
    OutEvents.Sort([](const FGuWorldDaoEvent& A, const FGuWorldDaoEvent& B)
    {
        return A.Id.ToString() < B.Id.ToString();
    });
}

void UGuWorldDaoEcologySubsystem::RestoreDynamicEvents(const TArray<FGuWorldDaoEvent>& Events)
{
    UWorld* World = GetWorld();
    if (!World || World->GetNetMode() == NM_Client) return;

    DynamicEvents.Reset();
    for (const FGuWorldDaoEvent& Event : Events)
    {
        if (!Event.Id.IsValid() || Event.DaoDeposit.IsEmpty()) continue;
        DynamicEvents.Add(Event.Id, Event);
    }
    InvalidateEventIndex();
}
