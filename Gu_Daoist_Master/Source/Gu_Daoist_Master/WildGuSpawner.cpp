#include "WildGuSpawner.h"

#include "Components/SceneComponent.h"
#include "CollisionQueryParams.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GuDefinitionRegistrySubsystem.h"
#include "GuEcologyPopulationSubsystem.h"
#include "GuEcologyPopulationTypes.h"
#include "GuEntitySubsystem.h"
#include "GuPersistenceSubsystem.h"
#include "GuWorldPopulationSubsystem.h"
#include "Misc/Crc.h"
#include "TimerManager.h"
#include "UGuDefinition.h"
#include "WildGuWorldActor.h"

DEFINE_LOG_CATEGORY_STATIC(LogWildGuSpawner, Log, All);

AWildGuSpawner::AWildGuSpawner()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = false;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);
}


void AWildGuSpawner::PostInitializeComponents()
{
    Super::PostInitializeComponents();

    // Register the authored species as early as practical so the persistent ECS
    // loader can resolve saved physical instances of a Gu whose only level reference
    // happens to be this spawn point. No physical entity is created here.
    if (HasAuthority() && IsValid(GuDefinition))
    {
        FName ResolvedDefinitionId;
        FString BootstrapError;
        if (!PrepareDefinition(ResolvedDefinitionId, BootstrapError))
        {
            UE_LOG(LogWildGuSpawner, Warning, TEXT("%s definition bootstrap: %s"), *GetName(), *BootstrapError);
        }
    }
}

void AWildGuSpawner::BeginPlay()
{
    Super::BeginPlay();

    if (!bSpawnOnBeginPlay || !HasAuthority())
    {
        return;
    }

    if (InitialSpawnDelaySeconds <= KINDA_SMALL_NUMBER)
    {
        HandleInitialSpawn();
        return;
    }

    FTimerHandle Timer;
    GetWorldTimerManager().SetTimer(
        Timer,
        this,
        &AWildGuSpawner::HandleInitialSpawn,
        InitialSpawnDelaySeconds,
        false);
}

void AWildGuSpawner::HandleInitialSpawn()
{
    UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
    UGuPersistenceSubsystem* Persistence = GI ? GI->GetSubsystem<UGuPersistenceSubsystem>() : nullptr;

    // The character/domain bootstrap owns persistence loading. Do not start a competing
    // load from a level actor. If it is not ready yet, retry after a short tick.
    if (Persistence && !Persistence->IsLoaded())
    {
        FTimerHandle RetryTimer;
        GetWorldTimerManager().SetTimer(RetryTimer, this, &AWildGuSpawner::HandleInitialSpawn, 0.10f, false);
        return;
    }

    SpawnWildGuBatch();
}

FName AWildGuSpawner::GetResolvedSpawnerId() const
{
    if (!SpawnerId.IsNone())
    {
        return SpawnerId;
    }

    // Actor paths for placed level actors are deterministic enough for a fallback,
    // but authored persistent content should still set SpawnerId explicitly.
    return FName(*GetPathName());
}

FName AWildGuSpawner::GetResolvedDefinitionId() const
{
    if (IsValid(GuDefinition))
    {
        return UGuDefinitionRegistrySubsystem::DefinitionIdForAsset(GuDefinition);
    }
    return DefinitionId;
}

bool AWildGuSpawner::PrepareDefinition(FName& OutDefinitionId, FString& OutError) const
{
    OutDefinitionId = NAME_None;

    UWorld* World = GetWorld();
    UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
    UGuDefinitionRegistrySubsystem* Registry = GI ? GI->GetSubsystem<UGuDefinitionRegistrySubsystem>() : nullptr;
    if (!Registry)
    {
        OutError = TEXT("Gu definition registry is unavailable.");
        return false;
    }

    if (IsValid(GuDefinition))
    {
        OutDefinitionId = UGuDefinitionRegistrySubsystem::DefinitionIdForAsset(GuDefinition);
        if (OutDefinitionId.IsNone())
        {
            OutError = TEXT("The selected GuDefinition does not resolve to a valid definition id.");
            return false;
        }

        // An authored spawn point is allowed to bootstrap its selected DataAsset into the
        // registry. Do not replace a different already-registered species silently.
        if (!Registry->HasDefinition(OutDefinitionId))
        {
            if (!Registry->RegisterDefinitionAsset(GuDefinition, OutError, false))
            {
                return false;
            }
        }
        else if (!Registry->FindDefinitionAsset(OutDefinitionId))
        {
            OutError = FString::Printf(
                TEXT("Definition '%s' exists in the registry but has no authored executable asset bridge."),
                *OutDefinitionId.ToString());
            return false;
        }

        OutError.Reset();
        return true;
    }

    OutDefinitionId = DefinitionId;
    if (OutDefinitionId.IsNone())
    {
        OutError = TEXT("Set GuDefinition or DefinitionId on the Wild Gu Spawner.");
        return false;
    }

    if (!Registry->HasDefinition(OutDefinitionId))
    {
        OutError = FString::Printf(
            TEXT("Runtime/procedural definition '%s' is not registered yet."),
            *OutDefinitionId.ToString());
        return false;
    }

    OutError.Reset();
    return true;
}

int32 AWildGuSpawner::StableSeedForSlot(const int32 SlotIndex) const
{
    const FString Key = FString::Printf(
        TEXT("%s|%d|%d"),
        *GetResolvedSpawnerId().ToString(),
        BaseSeed,
        SlotIndex);

    return static_cast<int32>(FCrc::StrCrc32(*Key) & 0x7fffffffU);
}

FVector AWildGuSpawner::ProjectLocationToGround(const FVector& Proposed) const
{
    if (!bProjectToGround)
    {
        return Proposed;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return Proposed;
    }

    FHitResult Hit;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(WildGuSpawnerGround), false, this);
    const FVector Start = Proposed + FVector(0.0f, 0.0f, GroundTraceUpCm);
    const FVector End = Proposed - FVector(0.0f, 0.0f, GroundTraceDownCm);

    if (World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
    {
        return Hit.ImpactPoint;
    }

    return Proposed;
}

FTransform AWildGuSpawner::SpawnTransformForSlot(const int32 SlotIndex, const int32 SpawnSeed) const
{
    FVector Location = GetActorLocation();

    if (SpawnRadiusCm > KINDA_SMALL_NUMBER)
    {
        FRandomStream Random(SpawnSeed);
        const float Angle = Random.FRandRange(0.0f, 2.0f * PI);
        const float Radius = SpawnRadiusCm * FMath::Sqrt(Random.FRand());
        Location.X += FMath::Cos(Angle) * Radius;
        Location.Y += FMath::Sin(Angle) * Radius;
    }

    Location = ProjectLocationToGround(Location);
    return FTransform(GetActorRotation(), Location, FVector::OneVector);
}

TArray<FGuid> AWildGuSpawner::SpawnWildGuBatch()
{
    TArray<FGuid> Result;
    SpawnedEntityIds.Reset();
    LastSpawnError.Reset();

    if (!HasAuthority())
    {
        LastSpawnError = TEXT("Wild Gu spawning is server-authoritative.");
        return Result;
    }

    const int32 Count = FMath::Clamp(SpawnCount, 1, 64);
    for (int32 SlotIndex = 0; SlotIndex < Count; ++SlotIndex)
    {
        const FGuid EntityId = SpawnWildGuSlot(SlotIndex);
        if (EntityId.IsValid())
        {
            Result.Add(EntityId);
        }
    }

    return Result;
}

FGuid AWildGuSpawner::SpawnWildGuSlot(const int32 SlotIndex)
{
    LastSpawnError.Reset();

    if (!HasAuthority())
    {
        LastSpawnError = TEXT("Wild Gu spawning is server-authoritative.");
        return FGuid();
    }

    if (SlotIndex < 0)
    {
        LastSpawnError = TEXT("Wild Gu spawn slot cannot be negative.");
        return FGuid();
    }

    UWorld* World = GetWorld();
    UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
    UGuEntitySubsystem* Entities = GI ? GI->GetSubsystem<UGuEntitySubsystem>() : nullptr;
    UGuPersistenceSubsystem* Persistence = GI ? GI->GetSubsystem<UGuPersistenceSubsystem>() : nullptr;
    UGuEcologyPopulationSubsystem* EcologyPopulation = World ? World->GetSubsystem<UGuEcologyPopulationSubsystem>() : nullptr;
    UGuWorldPopulationSubsystem* WorldPopulation = World ? World->GetSubsystem<UGuWorldPopulationSubsystem>() : nullptr;

    if (!World || !GI || !Entities || !EcologyPopulation || !WorldPopulation)
    {
        LastSpawnError = TEXT("Wild Gu spawner cannot access ECS/ecology/world-population subsystems.");
        UE_LOG(LogWildGuSpawner, Warning, TEXT("%s: %s"), *GetName(), *LastSpawnError);
        return FGuid();
    }

    if (Persistence && !Persistence->IsLoaded())
    {
        LastSpawnError = TEXT("Persistent Gu domain is not loaded yet; authored wild Gu spawn was deferred/rejected.");
        return FGuid();
    }

    FName ResolvedDefinitionId;
    if (!PrepareDefinition(ResolvedDefinitionId, LastSpawnError))
    {
        UE_LOG(LogWildGuSpawner, Warning, TEXT("%s: %s"), *GetName(), *LastSpawnError);
        return FGuid();
    }

    if (RegionId.IsNone())
    {
        LastSpawnError = TEXT("Wild Gu Spawner requires a RegionId.");
        UE_LOG(LogWildGuSpawner, Warning, TEXT("%s: %s"), *GetName(), *LastSpawnError);
        return FGuid();
    }

    const FName SiteId = GetResolvedSpawnerId();
    const int32 SpawnSeed = StableSeedForSlot(SlotIndex);

    // If ecology persistence already restored this authored slot, reuse the same FGuid.
    TArray<FGuEcologyResident> ExistingResidents;
    EcologyPopulation->GetResidentsInSite(SiteId, ExistingResidents);
    for (const FGuEcologyResident& Resident : ExistingResidents)
    {
        if (Resident.Kind == EGuEcologyResidentKind::WildGu
            && Resident.SlotIndex == SlotIndex
            && Resident.CandidateId == ResolvedDefinitionId
            && Entities->HasEntity(Resident.EntityId))
        {
            // Restore may have materialized the native base proxy because actor-class
            // metadata is level-authored. Reassert this spawner's BP class cleanly.
            WorldPopulation->DespawnProxyForEntity(Resident.EntityId);

            EcologyPopulation->RegisterAuthoredWildGuResident(
                Resident.EntityId,
                SiteId,
                RegionId,
                ResolvedDefinitionId,
                SlotIndex,
                Resident.WorldTransform,
                SpawnSeed,
                WildGuActorClass,
                true);

            AWildGuWorldActor* Proxy = WorldPopulation->SpawnProxyForEntity(Resident.EntityId);
            SpawnedEntityIds.AddUnique(Resident.EntityId);
            OnWildGuSpawned.Broadcast(Resident.EntityId, Proxy);
            K2_OnWildGuSpawned(Resident.EntityId, Proxy);
            return Resident.EntityId;
        }
    }

    const FTransform SpawnTransform = SpawnTransformForSlot(SlotIndex, SpawnSeed);
    const FGuid EntityId = Entities->CreateGuInstance(ResolvedDefinitionId, FString(), EGuContainer::World);
    if (!EntityId.IsValid())
    {
        LastSpawnError = FString::Printf(
            TEXT("Could not create physical wild Gu '%s'. Check the log for ECS/registry validation."),
            *ResolvedDefinitionId.ToString());
        UE_LOG(LogWildGuSpawner, Warning, TEXT("%s: %s"), *GetName(), *LastSpawnError);
        return FGuid();
    }

    if (!EcologyPopulation->RegisterAuthoredWildGuResident(
            EntityId,
            SiteId,
            RegionId,
            ResolvedDefinitionId,
            SlotIndex,
            SpawnTransform,
            SpawnSeed,
            WildGuActorClass,
            true))
    {
        Entities->DestroyEntity(EntityId);
        LastSpawnError = TEXT("Ecology population rejected the authored wild Gu resident.");
        UE_LOG(LogWildGuSpawner, Warning, TEXT("%s: %s"), *GetName(), *LastSpawnError);
        return FGuid();
    }

    AWildGuWorldActor* Proxy = WorldPopulation->SpawnProxyForEntity(EntityId);
    SpawnedEntityIds.AddUnique(EntityId);
    OnWildGuSpawned.Broadcast(EntityId, Proxy);
    K2_OnWildGuSpawned(EntityId, Proxy);

    UE_LOG(
        LogWildGuSpawner,
        Log,
        TEXT("Spawned authored wild Gu %s, slot %d, entity %s, region %s."),
        *ResolvedDefinitionId.ToString(),
        SlotIndex,
        *EntityId.ToString(),
        *RegionId.ToString());

    return EntityId;
}
