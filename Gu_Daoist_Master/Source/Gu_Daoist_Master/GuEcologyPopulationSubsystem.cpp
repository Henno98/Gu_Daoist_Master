#include "GuEcologyPopulationSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"
#include "GuDefinitionRegistrySubsystem.h"
#include "GuEntitySubsystem.h"
#include "GuWorldDaoEcologySubsystem.h"
#include "GuWorldPopulationSubsystem.h"
#include "UGuDefinition.h"
#include "WildGuWorldActor.h"

DEFINE_LOG_CATEGORY_STATIC(LogGuEcologyPopulation, Log, All);

namespace
{
    uint32 StableSiteSlotHash(const FName SiteId, const int32 SiteSeed, const int32 SlotIndex, const uint32 Salt)
    {
        uint32 H = GetTypeHash(SiteId);
        H = HashCombineFast(H, GetTypeHash(SiteSeed));
        H = HashCombineFast(H, GetTypeHash(SlotIndex));
        H = HashCombineFast(H, Salt);
        return H;
    }

    bool LooksLikeRelicGu(const FName DefinitionId, const FString& DisplayName)
    {
        return DefinitionId.ToString().Contains(TEXT("relic"), ESearchCase::IgnoreCase)
            || DisplayName.Contains(TEXT("Relic Gu"), ESearchCase::IgnoreCase);
    }

    struct FSpawnChoice
    {
        FGuDaoEcologyCandidate Candidate;
        EGuEcologyResidentKind Kind = EGuEcologyResidentKind::Resource;
        float Score = 0.0f;
    };
}

bool UGuEcologyPopulationSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    const UWorld* World = Cast<UWorld>(Outer);
    return World != nullptr && World->IsGameWorld();
}

void UGuEcologyPopulationSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    ReconcileAccumulator = 0.0f;
    bInitialRuleRefreshDone = false;
}

void UGuEcologyPopulationSubsystem::Deinitialize()
{
    Sites.Reset();
    Residents.Reset();
    Super::Deinitialize();
}

TStatId UGuEcologyPopulationSubsystem::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(UGuEcologyPopulationSubsystem, STATGROUP_Tickables);
}

bool UGuEcologyPopulationSubsystem::IsTickable() const
{
    const UWorld* World = GetWorld();
    return bAutoReconcile && World && World->IsGameWorld() && World->GetNetMode() != NM_Client;
}

void UGuEcologyPopulationSubsystem::Tick(const float DeltaTime)
{
    if (!HasAuthority()) return;

    if (!bInitialRuleRefreshDone)
    {
        RefreshWildGuHabitatRules();
        bInitialRuleRefreshDone = true;
    }

    ReconcileAccumulator += FMath::Max(0.0f, DeltaTime);
    if (ReconcileAccumulator < FMath::Max(1.0f, ReconcileIntervalSeconds))
    {
        return;
    }

    ReconcileAccumulator = 0.0f;
    ReconcileAllSites();
}

bool UGuEcologyPopulationSubsystem::HasAuthority() const
{
    const UWorld* World = GetWorld();
    return World && World->IsGameWorld() && World->GetNetMode() != NM_Client;
}

bool UGuEcologyPopulationSubsystem::RegisterPopulationSite(const FGuEcologyPopulationSite& Site)
{
    if (!HasAuthority() || Site.SiteId.IsNone() || Site.RegionId.IsNone())
    {
        return false;
    }

    FGuEcologyPopulationSite Clean = Site;
    Clean.RadiusCm = FMath::Max(0.0f, Clean.RadiusCm);
    Clean.Capacity = FMath::Clamp(Clean.Capacity, 0, 128);
    Clean.MaxWildGuRank = FMath::Clamp(Clean.MaxWildGuRank, 1, 9);
    if (Clean.Seed == 0) Clean.Seed = static_cast<int32>(GetTypeHash(Clean.SiteId));

    Sites.Add(Clean.SiteId, Clean);
    return true;
}

bool UGuEcologyPopulationSubsystem::RemovePopulationSite(const FName SiteId, const bool bDestroyResidents)
{
    if (!HasAuthority() || !Sites.Contains(SiteId))
    {
        return false;
    }

    TArray<FGuid> ToRemove;
    for (const TPair<FGuid, FGuEcologyResident>& Pair : Residents)
    {
        if (Pair.Value.SiteId == SiteId) ToRemove.Add(Pair.Key);
    }

    UWorld* World = GetWorld();
    UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
    UGuEntitySubsystem* Entities = GI ? GI->GetSubsystem<UGuEntitySubsystem>() : nullptr;
    UGuWorldPopulationSubsystem* WorldPopulation = World ? World->GetSubsystem<UGuWorldPopulationSubsystem>() : nullptr;

    for (const FGuid& EntityId : ToRemove)
    {
        const FGuEcologyResident* Resident = Residents.Find(EntityId);
        if (Resident && Resident->Kind == EGuEcologyResidentKind::WildGu && WorldPopulation)
        {
            WorldPopulation->UnregisterWildGu(EntityId, true);
        }

        if (bDestroyResidents && Entities)
        {
            Entities->DestroyEntity(EntityId);
        }

        Residents.Remove(EntityId);
    }

    Sites.Remove(SiteId);
    return true;
}

bool UGuEcologyPopulationSubsystem::SetPopulationSiteActive(const FName SiteId, const bool bActive)
{
    if (!HasAuthority()) return false;
    FGuEcologyPopulationSite* Site = Sites.Find(SiteId);
    if (!Site) return false;
    Site->bActive = bActive;
    return true;
}

float UGuEcologyPopulationSubsystem::SpawnChanceForIntensity(const float Intensity)
{
    const float I = FMath::Max(0.0f, Intensity);
    if (I <= 0.0f) return 0.0f;

    // Candidate threshold already says "this habitat can support it".
    // This curve controls abundance, not eligibility.
    return FMath::Clamp(I / (I + 1.5f), 0.0f, 0.92f);
}

FVector UGuEcologyPopulationSubsystem::SampleSlotLocation(
    const FGuEcologyPopulationSite& Site,
    const int32 SlotIndex,
    int32& OutSeed) const
{
    const uint32 H = StableSiteSlotHash(Site.SiteId, Site.Seed, SlotIndex, 0xA11CEu);
    OutSeed = static_cast<int32>(H);
    FRandomStream Random(OutSeed);

    const float Angle = Random.FRandRange(0.0f, 2.0f * PI);
    const float Radius = FMath::Sqrt(Random.FRand()) * Site.RadiusCm;
    FVector Point = Site.Center;
    Point.X += FMath::Cos(Angle) * Radius;
    Point.Y += FMath::Sin(Angle) * Radius;
    return ResolveGroundLocation(Site, Point);
}

FVector UGuEcologyPopulationSubsystem::ResolveGroundLocation(
    const FGuEcologyPopulationSite& Site,
    const FVector& Proposed) const
{
    UWorld* World = GetWorld();
    if (!World || !Site.bProjectToGround)
    {
        return Proposed;
    }

    const FVector Start(Proposed.X, Proposed.Y, Proposed.Z + 100000.0f);
    const FVector End(Proposed.X, Proposed.Y, Proposed.Z - 100000.0f);

    FHitResult Hit;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(GuEcologyPopulationGround), false);
    if (World->LineTraceSingleByChannel(Hit, Start, End, ECC_WorldStatic, Params))
    {
        return Hit.ImpactPoint + FVector(0.0f, 0.0f, 35.0f);
    }
    return Proposed;
}

bool UGuEcologyPopulationSubsystem::HasResidentInSlot(const FName SiteId, const int32 SlotIndex) const
{
    for (const TPair<FGuid, FGuEcologyResident>& Pair : Residents)
    {
        if (Pair.Value.SiteId == SiteId && Pair.Value.SlotIndex == SlotIndex)
        {
            return true;
        }
    }
    return false;
}

void UGuEcologyPopulationSubsystem::PurgeMissingEntities()
{
    if (!HasAuthority()) return;

    UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
    UGuEntitySubsystem* Entities = GI ? GI->GetSubsystem<UGuEntitySubsystem>() : nullptr;
    UGuWorldPopulationSubsystem* WorldPopulation = GetWorld() ? GetWorld()->GetSubsystem<UGuWorldPopulationSubsystem>() : nullptr;
    if (!Entities) return;

    TArray<FGuid> Missing;
    for (const TPair<FGuid, FGuEcologyResident>& Pair : Residents)
    {
        if (!Entities->HasEntity(Pair.Key))
        {
            Missing.Add(Pair.Key);
        }
    }

    for (const FGuid& EntityId : Missing)
    {
        if (WorldPopulation) WorldPopulation->UnregisterWildGu(EntityId, true);
        Residents.Remove(EntityId);
    }
}

FRefinementSemanticProfile UGuEcologyPopulationSubsystem::SemanticProfileForCandidate(
    const FGuDaoEcologyCandidate& Candidate) const
{
    FRefinementSemanticProfile Profile;
    if (Candidate.Path.IsValid())
    {
        Profile.Paths.Add(Candidate.Path.GetTagName(), 1.0f);
    }

    Profile.Properties.Add(TEXT("habitat_intensity"), FMath::Max(0.0f, Candidate.Intensity));
    Profile.Properties.Add(TEXT("local_dao_density"), FMath::Max(0.0f, Candidate.LocalDensity));
    Profile.Properties.Add(TEXT("path_share"), FMath::Clamp(Candidate.PathShare, 0.0f, 1.0f));
    Profile.Properties.Add(TEXT("maturity_years"), FMath::Max(0.0f, Candidate.MaturityYears));

    if (!Candidate.Kind.IsNone())
    {
        Profile.Traits.Add(Candidate.Kind, 1.0f);
    }
    if (!Candidate.Substrate.IsNone())
    {
        Profile.Traits.Add(FName(*FString::Printf(TEXT("substrate:%s"), *Candidate.Substrate.ToString())), 1.0f);
    }

    Profile.DaoMass = FMath::Max(
        0.01f,
        static_cast<float>(FMath::Max(1, Candidate.Rank)) *
        FMath::Max(1.0f, Candidate.Intensity));
    return Profile;
}

bool UGuEcologyPopulationSubsystem::SpawnResidentForSlot(
    const FGuEcologyPopulationSite& Site,
    const int32 SlotIndex)
{
    if (!HasAuthority() || !Site.bActive || HasResidentInSlot(Site.SiteId, SlotIndex))
    {
        return false;
    }

    UWorld* World = GetWorld();
    UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
    UGuWorldDaoEcologySubsystem* Ecology = World ? World->GetSubsystem<UGuWorldDaoEcologySubsystem>() : nullptr;
    UGuWorldPopulationSubsystem* WorldPopulation = World ? World->GetSubsystem<UGuWorldPopulationSubsystem>() : nullptr;
    UGuEntitySubsystem* Entities = GI ? GI->GetSubsystem<UGuEntitySubsystem>() : nullptr;
    UGuDefinitionRegistrySubsystem* Registry = GI ? GI->GetSubsystem<UGuDefinitionRegistrySubsystem>() : nullptr;

    if (!Ecology || !Entities)
    {
        return false;
    }

    int32 SpawnSeed = 0;
    const FVector SpawnLocation = SampleSlotLocation(Site, SlotIndex, SpawnSeed);
    FRandomStream Random(static_cast<int32>(StableSiteSlotHash(Site.SiteId, Site.Seed, SlotIndex, 0xB10B1u)));

    TArray<FSpawnChoice> Choices;

    if (Site.bAllowWildGu)
    {
        for (const FGuDaoEcologyCandidate& Candidate : Ecology->GetWildGuCandidatesAt(SpawnLocation, Site.MaxWildGuRank))
        {
            if (Candidate.Id.IsNone()) continue;
            if (Registry && !Registry->HasDefinition(Candidate.Id)) continue;

            FSpawnChoice Choice;
            Choice.Candidate = Candidate;
            Choice.Kind = EGuEcologyResidentKind::WildGu;
            Choice.Score = Candidate.Intensity * Random.FRandRange(0.82f, 1.18f);
            Choices.Add(MoveTemp(Choice));
        }
    }

    if (Site.bAllowBeasts)
    {
        for (const FGuDaoEcologyCandidate& Candidate : Ecology->GetBeastCandidatesAt(SpawnLocation))
        {
            FSpawnChoice Choice;
            Choice.Candidate = Candidate;
            Choice.Kind = EGuEcologyResidentKind::Beast;
            Choice.Score = Candidate.Intensity * Random.FRandRange(0.82f, 1.18f);
            Choices.Add(MoveTemp(Choice));
        }
    }

    if (Site.bAllowResources)
    {
        for (const FGuDaoEcologyCandidate& Candidate : Ecology->GetResourceCandidatesAt(SpawnLocation))
        {
            FSpawnChoice Choice;
            Choice.Candidate = Candidate;
            Choice.Kind = EGuEcologyResidentKind::Resource;
            Choice.Score = Candidate.Intensity * Random.FRandRange(0.82f, 1.18f);
            Choices.Add(MoveTemp(Choice));
        }
    }

    if (Choices.IsEmpty())
    {
        return false;
    }

    Choices.Sort([](const FSpawnChoice& A, const FSpawnChoice& B)
    {
        return A.Score > B.Score;
    });

    const FSpawnChoice& Selected = Choices[0];
    if (Random.FRand() > SpawnChanceForIntensity(Selected.Candidate.Intensity))
    {
        return false;
    }

    FGuid EntityId;

    if (Selected.Kind == EGuEcologyResidentKind::WildGu)
    {
        EntityId = Entities->CreateGuInstance(Selected.Candidate.Id, FString(), EGuContainer::World);
        if (!EntityId.IsValid())
        {
            return false;
        }

        FString TransferError;
        if (!Entities->TransferGuOwnershipAndPlacement(EntityId, FString(), EGuContainer::World, TransferError))
        {
            Entities->DestroyEntity(EntityId);
            UE_LOG(LogGuEcologyPopulation, Warning,
                TEXT("Could not mark wild Gu %s as world-owned: %s"),
                *Selected.Candidate.Id.ToString(), *TransferError);
            return false;
        }

        if (WorldPopulation)
        {
            const FTransform Transform(FRotator::ZeroRotator, SpawnLocation);
            WorldPopulation->RegisterExistingWildGu(
                EntityId,
                Selected.Candidate.Id,
                Site.RegionId,
                SpawnSeed,
                Transform,
                Site.WildGuActorClass,
                true);
        }
    }
    else
    {
        const FRefinementSemanticProfile Semantic = SemanticProfileForCandidate(Selected.Candidate);
        const ERefinableKind RefinableKind =
            Selected.Kind == EGuEcologyResidentKind::Beast
                ? ERefinableKind::Beast
                : (Selected.Candidate.Kind == TEXT("plant") ? ERefinableKind::Plant : ERefinableKind::Other);

        EntityId = Entities->CreateRefinableEntity(
            Semantic,
            RefinableKind,
            Selected.Candidate.Id,
            Selected.Candidate.Id);

        if (!EntityId.IsValid())
        {
            return false;
        }
    }

    FGuEcologyResident Resident;
    Resident.EntityId = EntityId;
    Resident.SiteId = Site.SiteId;
    Resident.RegionId = Site.RegionId;
    Resident.SlotIndex = SlotIndex;
    Resident.Kind = Selected.Kind;
    Resident.CandidateId = Selected.Candidate.Id;
    Resident.CandidateKind = Selected.Candidate.Kind;
    Resident.WorldTransform = FTransform(FRotator::ZeroRotator, SpawnLocation);
    Resident.SpawnSeed = SpawnSeed;
    Resident.HabitatIntensity = Selected.Candidate.Intensity;

    Residents.Add(EntityId, MoveTemp(Resident));

    UE_LOG(LogGuEcologyPopulation, Verbose,
        TEXT("Ecology materialized %s (%s) in site %s, slot %d, entity %s."),
        *Selected.Candidate.Name.ToString(),
        *Selected.Candidate.Id.ToString(),
        *Site.SiteId.ToString(),
        SlotIndex,
        *EntityId.ToString());

    return true;
}

bool UGuEcologyPopulationSubsystem::ReconcileSite(const FName SiteId)
{
    if (!HasAuthority()) return false;

    const FGuEcologyPopulationSite* Site = Sites.Find(SiteId);
    if (!Site || !Site->bActive)
    {
        return false;
    }

    PurgeMissingEntities();

    for (int32 SlotIndex = 0; SlotIndex < Site->Capacity; ++SlotIndex)
    {
        if (!HasResidentInSlot(SiteId, SlotIndex))
        {
            SpawnResidentForSlot(*Site, SlotIndex);
        }
    }
    return true;
}

void UGuEcologyPopulationSubsystem::ReconcileAllSites()
{
    if (!HasAuthority()) return;

    PurgeMissingEntities();

    TArray<FName> SiteIds;
    Sites.GenerateKeyArray(SiteIds);
    SiteIds.Sort([](const FName A, const FName B)
    {
        return A.LexicalLess(B);
    });

    for (const FName SiteId : SiteIds)
    {
        ReconcileSite(SiteId);
    }
}

int32 UGuEcologyPopulationSubsystem::RefreshWildGuHabitatRules()
{
    if (!HasAuthority()) return 0;

    UWorld* World = GetWorld();
    UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
    UGuWorldDaoEcologySubsystem* Ecology = World ? World->GetSubsystem<UGuWorldDaoEcologySubsystem>() : nullptr;
    UGuDefinitionRegistrySubsystem* Registry = GI ? GI->GetSubsystem<UGuDefinitionRegistrySubsystem>() : nullptr;
    if (!Ecology || !Registry)
    {
        return 0;
    }

    int32 Registered = 0;
    const TArray<FGuDefinitionRecord> Definitions = Registry->GetAllDefinitions();
    for (const FGuDefinitionRecord& Record : Definitions)
    {
        if (Record.Id.IsNone() || LooksLikeRelicGu(Record.Id, Record.Name))
        {
            continue;
        }

        const UGuDefinition* Definition = Registry->FindDefinitionAsset(Record.Id);
        if (!Definition || !Definition->Path.IsValid())
        {
            continue;
        }

        if (Ecology->RegisterWildGuDefinition(Record.Id, Definition))
        {
            ++Registered;
        }
    }

    UE_LOG(LogGuEcologyPopulation, Log,
        TEXT("Registered %d non-Relic Gu species with world Dao ecology."), Registered);
    return Registered;
}

FGuWorldCaptureResult UGuEcologyPopulationSubsystem::CaptureWildGu(
    const FGuid EntityId,
    const FString& NewOwnerId,
    const EGuContainer TargetContainer)
{
    // Compatibility wrapper. Physical capture no longer grants spiritual ownership.
    (void)TargetContainer;
    return PhysicallyCaptureWildGu(EntityId, NewOwnerId);
}

FGuWorldCaptureResult UGuEcologyPopulationSubsystem::PhysicallyCaptureWildGu(
    const FGuid EntityId,
    const FString& CaptorId)
{
    FGuWorldCaptureResult Result;
    Result.EntityId = EntityId;

    if (!HasAuthority())
    {
        Result.Error = TEXT("Wild Gu capture is server-authoritative.");
        return Result;
    }

    FGuEcologyResident* Resident = Residents.Find(EntityId);
    if (!Resident || Resident->Kind != EGuEcologyResidentKind::WildGu)
    {
        Result.Error = TEXT("The requested entity is not a tracked wild Gu.");
        return Result;
    }

    if (CaptorId.IsEmpty())
    {
        Result.Error = TEXT("A physical captor id is required.");
        return Result;
    }

    UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
    UGuEntitySubsystem* Entities = GI ? GI->GetSubsystem<UGuEntitySubsystem>() : nullptr;
    UGuWorldPopulationSubsystem* WorldPopulation = GetWorld() ? GetWorld()->GetSubsystem<UGuWorldPopulationSubsystem>() : nullptr;
    if (!Entities)
    {
        Result.Error = TEXT("Gu entity subsystem is unavailable.");
        return Result;
    }

    FString CaptureError;
    if (!Entities->MarkGuCaptured(EntityId, CaptorId, CaptureError))
    {
        Result.Error = CaptureError;
        return Result;
    }

    if (WorldPopulation)
    {
        WorldPopulation->UnregisterWildGu(EntityId, true);
    }
    Residents.Remove(EntityId);

    Result.bSuccess = true;
    Result.bRequiresWillRefinement = true;
    Result.CapturedContainer = EGuContainer::Captured;
    return Result;
}

// WildGuSpawner v2 authored-resident bridge.
bool UGuEcologyPopulationSubsystem::RegisterAuthoredWildGuResident(
    const FGuid EntityId,
    const FName SiteId,
    const FName RegionId,
    const FName DefinitionId,
    const int32 SlotIndex,
    const FTransform& WorldTransform,
    const int32 SpawnSeed,
    TSubclassOf<AWildGuWorldActor> ActorClass,
    const bool bSpawnProxy)
{
    if (!HasAuthority()
        || !EntityId.IsValid()
        || SiteId.IsNone()
        || RegionId.IsNone()
        || DefinitionId.IsNone()
        || SlotIndex < 0)
    {
        return false;
    }

    UWorld* World = GetWorld();
    UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
    UGuEntitySubsystem* Entities = GI ? GI->GetSubsystem<UGuEntitySubsystem>() : nullptr;
    UGuWorldPopulationSubsystem* WorldPopulation = World ? World->GetSubsystem<UGuWorldPopulationSubsystem>() : nullptr;
    if (!Entities || !WorldPopulation)
    {
        return false;
    }

    const FGuInstanceComponent* Instance = Entities->GetGuInstance(EntityId);
    if (!Instance || Instance->DefinitionId != DefinitionId)
    {
        UE_LOG(LogGuEcologyPopulation, Warning,
            TEXT("Authored wild Gu registration rejected %s: physical definition does not match %s."),
            *EntityId.ToString(), *DefinitionId.ToString());
        return false;
    }

    // A site id must not silently convert a natural ecology site into an authored point.
    if (const FGuEcologyPopulationSite* ExistingSite = Sites.Find(SiteId))
    {
        if (ExistingSite->Capacity > 0 || ExistingSite->RegionId != RegionId)
        {
            UE_LOG(LogGuEcologyPopulation, Warning,
                TEXT("Authored wild Gu site '%s' collides with an existing population site."),
                *SiteId.ToString());
            return false;
        }
    }

    FString TransferError;
    if (!Entities->TransferGuOwnershipAndPlacement(EntityId, FString(), EGuContainer::World, TransferError))
    {
        UE_LOG(LogGuEcologyPopulation, Warning,
            TEXT("Authored wild Gu %s could not enter world ownership: %s"),
            *EntityId.ToString(), *TransferError);
        return false;
    }

    if (!WorldPopulation->RegisterExistingWildGu(
            EntityId,
            DefinitionId,
            RegionId,
            SpawnSeed,
            WorldTransform,
            ActorClass,
            bSpawnProxy))
    {
        return false;
    }

    // Capacity zero is intentional: this site is persistence/authoring metadata only.
    // ReconcileSite() therefore cannot materialize random ecology residents here.
    FGuEcologyPopulationSite AuthoredSite;
    AuthoredSite.SiteId = SiteId;
    AuthoredSite.RegionId = RegionId;
    AuthoredSite.Center = WorldTransform.GetLocation();
    AuthoredSite.RadiusCm = 0.0f;
    AuthoredSite.Capacity = 0;
    AuthoredSite.MaxWildGuRank = 9;
    AuthoredSite.bAllowWildGu = false;
    AuthoredSite.bAllowBeasts = false;
    AuthoredSite.bAllowResources = false;
    AuthoredSite.bProjectToGround = false;
    AuthoredSite.Seed = SpawnSeed;
    AuthoredSite.bActive = true;
    AuthoredSite.WildGuActorClass = ActorClass;
    Sites.Add(SiteId, MoveTemp(AuthoredSite));

    FGuEcologyResident Resident;
    Resident.EntityId = EntityId;
    Resident.SiteId = SiteId;
    Resident.RegionId = RegionId;
    Resident.SlotIndex = SlotIndex;
    Resident.Kind = EGuEcologyResidentKind::WildGu;
    Resident.CandidateId = DefinitionId;
    Resident.CandidateKind = TEXT("authored-wild-gu");
    Resident.WorldTransform = WorldTransform;
    Resident.SpawnSeed = SpawnSeed;
    Resident.HabitatIntensity = 1.0f;
    Residents.Add(EntityId, MoveTemp(Resident));

    return true;
}

bool UGuEcologyPopulationSubsystem::GetResident(
    const FGuid EntityId,
    FGuEcologyResident& OutResident) const
{
    const FGuEcologyResident* Resident = Residents.Find(EntityId);
    if (!Resident) return false;
    OutResident = *Resident;
    return true;
}

void UGuEcologyPopulationSubsystem::GetResidentsInSite(
    const FName SiteId,
    TArray<FGuEcologyResident>& OutResidents) const
{
    OutResidents.Reset();
    for (const TPair<FGuid, FGuEcologyResident>& Pair : Residents)
    {
        if (Pair.Value.SiteId == SiteId) OutResidents.Add(Pair.Value);
    }
}

void UGuEcologyPopulationSubsystem::GetResidentsInRegion(
    const FName RegionId,
    TArray<FGuEcologyResident>& OutResidents) const
{
    OutResidents.Reset();
    for (const TPair<FGuid, FGuEcologyResident>& Pair : Residents)
    {
        if (Pair.Value.RegionId == RegionId) OutResidents.Add(Pair.Value);
    }
}

void UGuEcologyPopulationSubsystem::ExportState(
    TArray<FGuEcologyPopulationSite>& OutSites,
    TArray<FGuEcologyResident>& OutResidents) const
{
    Sites.GenerateValueArray(OutSites);
    Residents.GenerateValueArray(OutResidents);

    OutSites.Sort([](const FGuEcologyPopulationSite& A, const FGuEcologyPopulationSite& B)
    {
        return A.SiteId.LexicalLess(B.SiteId);
    });

    OutResidents.Sort([](const FGuEcologyResident& A, const FGuEcologyResident& B)
    {
        if (A.SiteId != B.SiteId) return A.SiteId.LexicalLess(B.SiteId);
        return A.SlotIndex < B.SlotIndex;
    });
}

void UGuEcologyPopulationSubsystem::RestoreState(
    const TArray<FGuEcologyPopulationSite>& InSites,
    const TArray<FGuEcologyResident>& InResidents)
{
    if (!HasAuthority()) return;

    Sites.Reset();
    Residents.Reset();

    for (const FGuEcologyPopulationSite& Site : InSites)
    {
        if (!Site.SiteId.IsNone() && !Site.RegionId.IsNone())
        {
            Sites.Add(Site.SiteId, Site);
        }
    }

    UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
    UGuEntitySubsystem* Entities = GI ? GI->GetSubsystem<UGuEntitySubsystem>() : nullptr;
    UGuWorldPopulationSubsystem* WorldPopulation = GetWorld() ? GetWorld()->GetSubsystem<UGuWorldPopulationSubsystem>() : nullptr;

    if (!Entities) return;

    for (const FGuEcologyResident& Resident : InResidents)
    {
        if (!Resident.EntityId.IsValid() || !Entities->HasEntity(Resident.EntityId))
        {
            continue;
        }

        Residents.Add(Resident.EntityId, Resident);

        if (Resident.Kind == EGuEcologyResidentKind::WildGu && WorldPopulation)
        {
            const FGuEcologyPopulationSite* Site = Sites.Find(Resident.SiteId);
            TSubclassOf<AWildGuWorldActor> ActorClass;
            if (Site) ActorClass = Site->WildGuActorClass;
            WorldPopulation->RegisterExistingWildGu(
                Resident.EntityId,
                Resident.CandidateId,
                Resident.RegionId,
                Resident.SpawnSeed,
                Resident.WorldTransform,
                ActorClass,
                Site ? Site->bActive : true);
        }
    }

    bInitialRuleRefreshDone = false;
}
