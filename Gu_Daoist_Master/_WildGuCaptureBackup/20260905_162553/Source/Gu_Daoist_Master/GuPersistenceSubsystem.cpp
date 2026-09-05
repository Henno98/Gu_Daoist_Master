#include "GuPersistenceSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GuDefinitionRegistrySubsystem.h"
#include "GuDomainSaveGame.h"
#include "GuEntitySubsystem.h"
#include "GuProceduralGeneratorSubsystem.h"
#include "Kismet/GameplayStatics.h"

const FString UGuPersistenceSubsystem::SlotName = TEXT("GuDomain_Autosave_v1");

void UGuPersistenceSubsystem::Deinitialize()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(AutosaveTimer);
    }

    if (bLoaded && bDirty && !bIsLoading)
    {
        FString SaveError;
        if (!SaveNow(SaveError))
        {
            UE_LOG(LogTemp, Error, TEXT("Gu domain shutdown save failed: %s"), *SaveError);
        }
    }

    Super::Deinitialize();
}

bool UGuPersistenceSubsystem::EnsureLoaded(FString& OutError)
{
    if (const UWorld* World = GetWorld(); World && World->GetNetMode() == NM_Client)
    {
        OutError = TEXT("Gu-domain persistence is server-authoritative; clients receive replicated gameplay state.");
        return false;
    }

    if (bLoaded)
    {
        OutError.Reset();
        return true;
    }
    if (bIsLoading)
    {
        OutError.Reset();
        return true;
    }
    if (bLoadAttempted && !bLoaded)
    {
        OutError = TEXT("A previous Gu-domain load attempt failed during this GameInstance.");
        return false;
    }

    bLoadAttempted = true;
    bIsLoading = true;

    UGameInstance* GI = GetGameInstance();
    UGuDefinitionRegistrySubsystem* Registry = GI ? GI->GetSubsystem<UGuDefinitionRegistrySubsystem>() : nullptr;
    UGuEntitySubsystem* Entities = GI ? GI->GetSubsystem<UGuEntitySubsystem>() : nullptr;
    UGuProceduralGeneratorSubsystem* Generator = GI ? GI->GetSubsystem<UGuProceduralGeneratorSubsystem>() : nullptr;
    if (!Registry || !Entities || !Generator)
    {
        bIsLoading = false;
        OutError = TEXT("Gu persistence cannot access the registry, ECS, or procedural compiler.");
        return false;
    }

    if (!UGameplayStatics::DoesSaveGameExist(SlotName, UserIndex))
    {
        bIsLoading = false;
        bLoaded = true;
        bDirty = false;
        OutError.Reset();
        UE_LOG(LogTemp, Log, TEXT("No Gu-domain save exists yet; starting a fresh persistent domain."));
        return true;
    }

    UGuDomainSaveGame* Save = Cast<UGuDomainSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, UserIndex));
    if (!Save)
    {
        bIsLoading = false;
        OutError = TEXT("The Gu-domain save slot exists but could not be loaded as UGuDomainSaveGame.");
        return false;
    }

    if (Save->SaveVersion > 2)
    {
        bIsLoading = false;
        OutError = FString::Printf(TEXT("Gu-domain save version %d is newer than this build supports."), Save->SaveVersion);
        return false;
    }

    Registry->ClearRuntimeDefinitions();

    TMap<FName, FName> DefinitionRemap;
    for (const FGuDefinitionRecord& SavedDefinition : Save->RuntimeDefinitions)
    {
        if (SavedDefinition.Id.IsNone()) continue;

        UGuDefinition* RuntimeDefinition = nullptr;
        FName CanonicalId = SavedDefinition.Id;
        FString CompileError;
        if (!Generator->CompileAndRegisterRuntimeRecord(SavedDefinition, RuntimeDefinition, CompileError, true, &CanonicalId))
        {
            Registry->ClearRuntimeDefinitions();
            Entities->ResetAllEntities();
            bIsLoading = false;
            OutError = FString::Printf(
                TEXT("Could not restore persistent Gu species '%s': %s"),
                *SavedDefinition.Id.ToString(),
                *CompileError);
            return false;
        }

        // A saved species is a contract. Generator changes must not silently mutate an old Gu.
        // If v1 generation changes incompatibly, add a versioned migration/compiler instead.
        if (CanonicalId == SavedDefinition.Id)
        {
            const FGuDefinitionRecord* RestoredRecord = Registry->FindDefinition(CanonicalId);
            const FString SavedFingerprint = UGuDefinitionRegistrySubsystem::ComputeRuntimeSpeciesFingerprint(SavedDefinition);
            const FString RestoredFingerprint = RestoredRecord
                ? UGuDefinitionRegistrySubsystem::ComputeRuntimeSpeciesFingerprint(*RestoredRecord)
                : FString();
            if (!RestoredRecord || SavedFingerprint != RestoredFingerprint)
            {
                Registry->ClearRuntimeDefinitions();
                Entities->ResetAllEntities();
                bIsLoading = false;
                OutError = FString::Printf(
                    TEXT("Persistent Gu species '%s' no longer recompiles to its saved mechanics. Preserve the old generator version or add a save migration; the Gu was not silently changed."),
                    *SavedDefinition.Id.ToString());
                return false;
            }
        }

        if (CanonicalId != SavedDefinition.Id)
        {
            DefinitionRemap.Add(SavedDefinition.Id, CanonicalId);
        }
    }

    TArray<FGuEntitySnapshot> Snapshots = Save->EntitySnapshots;
    if (!DefinitionRemap.IsEmpty())
    {
        for (FGuEntitySnapshot& Snapshot : Snapshots)
        {
            if (Snapshot.bHasGuInstance)
            {
                if (const FName* Canonical = DefinitionRemap.Find(Snapshot.GuInstance.DefinitionId))
                {
                    Snapshot.GuInstance.DefinitionId = *Canonical;
                }
            }
            if (Snapshot.bHasRefinable)
            {
                if (const FName* Canonical = DefinitionRemap.Find(Snapshot.Refinable.DefinitionId))
                {
                    Snapshot.Refinable.DefinitionId = *Canonical;
                }
                if (const FName* Canonical = DefinitionRemap.Find(Snapshot.Refinable.SourceId))
                {
                    Snapshot.Refinable.SourceId = *Canonical;
                }
            }
        }
    }

    FString RestoreError;
    if (!Entities->RestoreSnapshots(Snapshots, RestoreError))
    {
        Registry->ClearRuntimeDefinitions();
        bIsLoading = false;
        OutError = FString::Printf(TEXT("Could not restore persistent Gu ECS state: %s"), *RestoreError);
        return false;
    }

    bIsLoading = false;
    bLoaded = true;
    bDirty = !DefinitionRemap.IsEmpty();
    OutError.Reset();

    UE_LOG(
        LogTemp,
        Log,
        TEXT("Restored Gu domain: %d runtime species, %d ECS entities%s."),
        Registry->GetRuntimeDefinitions().Num(),
        Snapshots.Num(),
        DefinitionRemap.IsEmpty() ? TEXT("") : TEXT(" (duplicate species compacted)"));

    if (bDirty) RequestAutosave(0.1f);
    return true;
}

bool UGuPersistenceSubsystem::SaveNow(FString& OutError)
{
    if (const UWorld* World = GetWorld(); World && World->GetNetMode() == NM_Client)
    {
        OutError = TEXT("Gu-domain persistence is server-authoritative.");
        return false;
    }

    if (bIsLoading)
    {
        OutError.Reset();
        return true;
    }

    if (!bLoaded)
    {
        if (!EnsureLoaded(OutError)) return false;
    }

    UGameInstance* GI = GetGameInstance();
    UGuDefinitionRegistrySubsystem* Registry = GI ? GI->GetSubsystem<UGuDefinitionRegistrySubsystem>() : nullptr;
    UGuEntitySubsystem* Entities = GI ? GI->GetSubsystem<UGuEntitySubsystem>() : nullptr;
    if (!Registry || !Entities)
    {
        OutError = TEXT("Gu persistence cannot access the registry or ECS while saving.");
        return false;
    }

    UGuDomainSaveGame* Save = Cast<UGuDomainSaveGame>(UGameplayStatics::CreateSaveGameObject(UGuDomainSaveGame::StaticClass()));
    if (!Save)
    {
        OutError = TEXT("Could not allocate the Gu-domain SaveGame object.");
        return false;
    }

    Save->SaveVersion = 2;
    Save->RuntimeDefinitions = Registry->GetRuntimeDefinitions();
    Save->EntitySnapshots = Entities->ExportSnapshots();
    Save->SavedAtUnixMs = FDateTime::UtcNow().ToUnixTimestamp() * 1000ll;

    if (!UGameplayStatics::SaveGameToSlot(Save, SlotName, UserIndex))
    {
        OutError = TEXT("SaveGameToSlot failed for the persistent Gu domain.");
        return false;
    }

    bDirty = false;
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(AutosaveTimer);
    }
    OutError.Reset();
    return true;
}

void UGuPersistenceSubsystem::RequestAutosave(const float DelaySeconds)
{
    bDirty = true;
    if (!bLoaded || bIsLoading) return;

    UWorld* World = GetWorld();
    if (!World)
    {
        FString SaveError;
        if (!SaveNow(SaveError))
        {
            UE_LOG(LogTemp, Error, TEXT("Gu autosave failed: %s"), *SaveError);
        }
        return;
    }

    World->GetTimerManager().SetTimer(
        AutosaveTimer,
        this,
        &UGuPersistenceSubsystem::HandleAutosave,
        FMath::Max(0.05f, DelaySeconds),
        false);
}

void UGuPersistenceSubsystem::HandleAutosave()
{
    if (!bDirty || bIsLoading) return;

    FString SaveError;
    if (!SaveNow(SaveError))
    {
        UE_LOG(LogTemp, Error, TEXT("Gu autosave failed: %s"), *SaveError);
    }
}
