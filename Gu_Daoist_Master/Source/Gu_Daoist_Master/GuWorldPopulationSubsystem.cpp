#include "GuWorldPopulationSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GuEntitySubsystem.h"
#include "WildGuWorldActor.h"

DEFINE_LOG_CATEGORY_STATIC(LogGuWorldPopulation, Log, All);

bool UGuWorldPopulationSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    const UWorld* World = Cast<UWorld>(Outer);
    return World != nullptr && World->IsGameWorld();
}

void UGuWorldPopulationSubsystem::Deinitialize()
{
    DestroyAllProxies();
    WorldGu.Reset();
    ActiveRegions.Reset();

    Super::Deinitialize();
}

bool UGuWorldPopulationSubsystem::PhysicalEntityExists(const FGuid& EntityId) const
{
    if (!EntityId.IsValid())
    {
        return false;
    }

    const UWorld* World = GetWorld();
    const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
    const UGuEntitySubsystem* Entities = GameInstance ? GameInstance->GetSubsystem<UGuEntitySubsystem>() : nullptr;
    return Entities != nullptr && Entities->GetGuInstance(EntityId) != nullptr;
}

bool UGuWorldPopulationSubsystem::RegisterExistingWildGu(
    const FGuid& EntityId,
    FName DefinitionId,
    FName RegionId,
    int32 SpawnSeed,
    const FTransform& WorldTransform,
    TSubclassOf<AWildGuWorldActor> ActorClass,
    bool bSpawnProxy)
{
    if (!PhysicalEntityExists(EntityId))
    {
        UE_LOG(LogGuWorldPopulation, Warning,
            TEXT("RegisterExistingWildGu rejected %s: no physical Gu entity exists."),
            *EntityId.ToString());
        return false;
    }

    if (DefinitionId.IsNone() || RegionId.IsNone())
    {
        UE_LOG(LogGuWorldPopulation, Warning,
            TEXT("RegisterExistingWildGu rejected %s: DefinitionId and RegionId are required."),
            *EntityId.ToString());
        return false;
    }

    FGuWildWorldRuntimeEntry& Entry = WorldGu.FindOrAdd(EntityId);
    Entry.Snapshot.EntityId = EntityId;
    Entry.Snapshot.DefinitionId = DefinitionId;
    Entry.Snapshot.RegionId = RegionId;
    Entry.Snapshot.SpawnSeed = SpawnSeed;
    Entry.Snapshot.WorldTransform = WorldTransform;
    Entry.ActorClass = ActorClass;

    if (AWildGuWorldActor* ExistingProxy = Entry.ProxyActor.Get())
    {
        ExistingProxy->SetActorTransform(WorldTransform);
        ExistingProxy->InitializeWorldGu(EntityId, DefinitionId, RegionId, SpawnSeed);
        return true;
    }

    if (bSpawnProxy && (ActiveRegions.Contains(RegionId) || ActiveRegions.IsEmpty()))
    {
        SpawnProxyForEntity(EntityId);
    }

    return true;
}

bool UGuWorldPopulationSubsystem::UnregisterWildGu(const FGuid& EntityId, bool bDestroyProxy)
{
    FGuWildWorldRuntimeEntry* Entry = WorldGu.Find(EntityId);
    if (!Entry)
    {
        return false;
    }

    if (bDestroyProxy)
    {
        if (AWildGuWorldActor* Proxy = Entry->ProxyActor.Get())
        {
            Proxy->Destroy();
        }
    }

    WorldGu.Remove(EntityId);
    return true;
}

AWildGuWorldActor* UGuWorldPopulationSubsystem::SpawnProxyForEntity(const FGuid& EntityId)
{
    FGuWildWorldRuntimeEntry* Entry = WorldGu.Find(EntityId);
    UWorld* World = GetWorld();
    if (!Entry || !World)
    {
        return nullptr;
    }

    // Proxies are replicated actors. Only authority may materialize them; clients
    // receive the server-spawned proxy through normal actor replication.
    if (World->GetNetMode() == NM_Client)
    {
        return nullptr;
    }

    if (!PhysicalEntityExists(EntityId))
    {
        UE_LOG(LogGuWorldPopulation, Warning,
            TEXT("Cannot spawn world proxy for %s: physical Gu entity is missing."),
            *EntityId.ToString());
        return nullptr;
    }

    if (AWildGuWorldActor* ExistingProxy = Entry->ProxyActor.Get())
    {
        return ExistingProxy;
    }

    UClass* SpawnClass = Entry->ActorClass ? Entry->ActorClass.Get() : AWildGuWorldActor::StaticClass();

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    SpawnParams.ObjectFlags |= RF_Transient;

    AWildGuWorldActor* Proxy = World->SpawnActor<AWildGuWorldActor>(
        SpawnClass,
        Entry->Snapshot.WorldTransform,
        SpawnParams);

    if (!Proxy)
    {
        return nullptr;
    }

    Proxy->InitializeWorldGu(
        Entry->Snapshot.EntityId,
        Entry->Snapshot.DefinitionId,
        Entry->Snapshot.RegionId,
        Entry->Snapshot.SpawnSeed);

    Entry->ProxyActor = Proxy;
    return Proxy;
}

bool UGuWorldPopulationSubsystem::DespawnProxyForEntity(const FGuid& EntityId)
{
    FGuWildWorldRuntimeEntry* Entry = WorldGu.Find(EntityId);
    if (!Entry)
    {
        return false;
    }

    AWildGuWorldActor* Proxy = Entry->ProxyActor.Get();
    if (!Proxy)
    {
        Entry->ProxyActor.Reset();
        return true;
    }

    Entry->Snapshot.WorldTransform = Proxy->GetActorTransform();
    Proxy->Destroy();
    Entry->ProxyActor.Reset();
    return true;
}

bool UGuWorldPopulationSubsystem::UpdateWorldTransform(
    const FGuid& EntityId,
    const FTransform& NewTransform,
    bool bMoveProxy)
{
    FGuWildWorldRuntimeEntry* Entry = WorldGu.Find(EntityId);
    if (!Entry)
    {
        return false;
    }

    Entry->Snapshot.WorldTransform = NewTransform;

    if (bMoveProxy)
    {
        if (AWildGuWorldActor* Proxy = Entry->ProxyActor.Get())
        {
            Proxy->SetActorTransform(NewTransform);
        }
    }

    return true;
}

void UGuWorldPopulationSubsystem::SetRegionActive(FName RegionId, bool bActive)
{
    if (RegionId.IsNone())
    {
        return;
    }

    if (bActive)
    {
        ActiveRegions.Add(RegionId);

        for (const TPair<FGuid, FGuWildWorldRuntimeEntry>& Pair : WorldGu)
        {
            if (Pair.Value.Snapshot.RegionId == RegionId)
            {
                SpawnProxyForEntity(Pair.Key);
            }
        }
        return;
    }

    ActiveRegions.Remove(RegionId);

    TArray<FGuid> ToDespawn;
    for (const TPair<FGuid, FGuWildWorldRuntimeEntry>& Pair : WorldGu)
    {
        if (Pair.Value.Snapshot.RegionId == RegionId)
        {
            ToDespawn.Add(Pair.Key);
        }
    }

    for (const FGuid& EntityId : ToDespawn)
    {
        DespawnProxyForEntity(EntityId);
    }
}

bool UGuWorldPopulationSubsystem::IsRegionActive(FName RegionId) const
{
    return ActiveRegions.Contains(RegionId);
}

bool UGuWorldPopulationSubsystem::HasWorldGu(const FGuid& EntityId) const
{
    return WorldGu.Contains(EntityId);
}

bool UGuWorldPopulationSubsystem::GetWorldSnapshot(
    const FGuid& EntityId,
    FGuWildWorldSnapshot& OutSnapshot) const
{
    const FGuWildWorldRuntimeEntry* Entry = WorldGu.Find(EntityId);
    if (!Entry)
    {
        return false;
    }

    OutSnapshot = Entry->Snapshot;
    if (const AWildGuWorldActor* Proxy = Entry->ProxyActor.Get())
    {
        OutSnapshot.WorldTransform = Proxy->GetActorTransform();
    }
    return true;
}

void UGuWorldPopulationSubsystem::GetEntitiesInRegion(FName RegionId, TArray<FGuid>& OutEntityIds) const
{
    OutEntityIds.Reset();

    for (const TPair<FGuid, FGuWildWorldRuntimeEntry>& Pair : WorldGu)
    {
        if (Pair.Value.Snapshot.RegionId == RegionId)
        {
            OutEntityIds.Add(Pair.Key);
        }
    }
}

void UGuWorldPopulationSubsystem::ExportSnapshots(TArray<FGuWildWorldSnapshot>& OutSnapshots) const
{
    OutSnapshots.Reset();
    OutSnapshots.Reserve(WorldGu.Num());

    for (const TPair<FGuid, FGuWildWorldRuntimeEntry>& Pair : WorldGu)
    {
        FGuWildWorldSnapshot Snapshot = Pair.Value.Snapshot;
        if (const AWildGuWorldActor* Proxy = Pair.Value.ProxyActor.Get())
        {
            Snapshot.WorldTransform = Proxy->GetActorTransform();
        }
        OutSnapshots.Add(MoveTemp(Snapshot));
    }
}

void UGuWorldPopulationSubsystem::RestoreSnapshots(
    const TArray<FGuWildWorldSnapshot>& Snapshots,
    bool bSpawnActiveRegions)
{
    DestroyAllProxies();
    WorldGu.Reset();

    for (const FGuWildWorldSnapshot& Snapshot : Snapshots)
    {
        if (!Snapshot.EntityId.IsValid() || !PhysicalEntityExists(Snapshot.EntityId))
        {
            continue;
        }

        FGuWildWorldRuntimeEntry& Entry = WorldGu.Add(Snapshot.EntityId);
        Entry.Snapshot = Snapshot;
    }

    if (!bSpawnActiveRegions)
    {
        return;
    }

    for (const TPair<FGuid, FGuWildWorldRuntimeEntry>& Pair : WorldGu)
    {
        if (ActiveRegions.IsEmpty() || ActiveRegions.Contains(Pair.Value.Snapshot.RegionId))
        {
            SpawnProxyForEntity(Pair.Key);
        }
    }
}

void UGuWorldPopulationSubsystem::DestroyAllProxies()
{
    for (TPair<FGuid, FGuWildWorldRuntimeEntry>& Pair : WorldGu)
    {
        if (AWildGuWorldActor* Proxy = Pair.Value.ProxyActor.Get())
        {
            Pair.Value.Snapshot.WorldTransform = Proxy->GetActorTransform();
            Proxy->Destroy();
        }
        Pair.Value.ProxyActor.Reset();
    }
}
