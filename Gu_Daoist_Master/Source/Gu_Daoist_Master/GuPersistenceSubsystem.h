#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "TimerManager.h"
#include "GuPersistenceSubsystem.generated.h"

/**
 * Autosaves the Gu domain independently from map/world actors.
 * Runtime Gu species are reconstructed first, then physical ECS snapshots are restored.
 */
UCLASS()
class GU_DAOIST_MASTER_API UGuPersistenceSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Deinitialize() override;

    UFUNCTION(BlueprintCallable, Category="Gu|Persistence")
    bool EnsureLoaded(FString& OutError);

    UFUNCTION(BlueprintCallable, Category="Gu|Persistence")
    bool SaveNow(FString& OutError);

    /** Coalesces frequent ECS mutations into one save. */
    void RequestAutosave(float DelaySeconds = 0.35f);

    UFUNCTION(BlueprintPure, Category="Gu|Persistence")
    bool IsLoaded() const { return bLoaded; }

    bool IsLoading() const { return bIsLoading; }

private:
    void HandleAutosave();

    bool bLoadAttempted = false;
    bool bLoaded = false;
    bool bIsLoading = false;
    bool bDirty = false;
    FTimerHandle AutosaveTimer;

    static const FString SlotName;
    static constexpr int32 UserIndex = 0;
};
