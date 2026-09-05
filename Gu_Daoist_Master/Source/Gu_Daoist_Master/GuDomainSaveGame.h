#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "GuDefinitionTypes.h"
#include "GuEntityTypes.h"
#include "GuDomainSaveGame.generated.h"

/** Persistent Gu-domain payload. Runtime species and physical ECS instances are saved separately. */
UCLASS()
class GU_DAOIST_MASTER_API UGuDomainSaveGame : public USaveGame
{
    GENERATED_BODY()

public:
    UPROPERTY(SaveGame)
    int32 SaveVersion = 3;

    /** Procedural/refinement-created species only. Authored DataAssets remain project content. */
    UPROPERTY(SaveGame)
    TArray<FGuDefinitionRecord> RuntimeDefinitions;

    /** Physical refinables/Gu, including per-instance condition, contamination, charges and placement. */
    UPROPERTY(SaveGame)
    TArray<FGuEntitySnapshot> EntitySnapshots;

    UPROPERTY(SaveGame)
    int64 SavedAtUnixMs = 0;
};
