#pragma once

#include "CoreMinimal.h"
#include "GuDefinitionTypes.h"
#include "GuEcologyPopulationTypes.generated.h"

class AWildGuWorldActor;

UENUM(BlueprintType)
enum class EGuEcologyResidentKind : uint8
{
    WildGu,
    Beast,
    Resource
};

/**
 * One logical habitat sampling area. It is simulation state, not visual authoring.
 * Landscape/PCG/region logic can register these later.
 */
USTRUCT(BlueprintType)
struct FGuEcologyPopulationSite
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Gu|World|Population")
    FName SiteId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Gu|World|Population")
    FName RegionId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Gu|World|Population")
    FVector Center = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Gu|World|Population", meta=(ClampMin="0.0"))
    float RadiusCm = 12000.0f;

    /** Maximum persistent residents controlled by this habitat site. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Gu|World|Population", meta=(ClampMin="0", ClampMax="128"))
    int32 Capacity = 12;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Gu|World|Population", meta=(ClampMin="1", ClampMax="9"))
    int32 MaxWildGuRank = 5;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Gu|World|Population")
    bool bAllowWildGu = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Gu|World|Population")
    bool bAllowBeasts = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Gu|World|Population")
    bool bAllowResources = true;

    /** Project XY samples to collision terrain before creating the resident. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Gu|World|Population")
    bool bProjectToGround = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Gu|World|Population")
    int32 Seed = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Gu|World|Population")
    bool bActive = true;

    /** Optional visual proxy class for wild Gu. Simulation does not depend on it. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gu|World|Population")
    TSubclassOf<AWildGuWorldActor> WildGuActorClass;
};

USTRUCT(BlueprintType)
struct FGuEcologyResident
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Gu|World|Population")
    FGuid EntityId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Gu|World|Population")
    FName SiteId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Gu|World|Population")
    FName RegionId = NAME_None;

    /** Slot identity makes reconciliation deterministic and prevents population churn. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Gu|World|Population")
    int32 SlotIndex = INDEX_NONE;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Gu|World|Population")
    EGuEcologyResidentKind Kind = EGuEcologyResidentKind::Resource;

    /** Gu definition id, beast species id, or resource ecology id. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Gu|World|Population")
    FName CandidateId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Gu|World|Population")
    FName CandidateKind = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Gu|World|Population")
    FTransform WorldTransform = FTransform::Identity;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Gu|World|Population")
    int32 SpawnSeed = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Gu|World|Population")
    float HabitatIntensity = 0.0f;
};

USTRUCT(BlueprintType)
struct FGuWorldCaptureResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Gu|World|Population")
    bool bSuccess = false;

    UPROPERTY(BlueprintReadOnly, Category="Gu|World|Population")
    bool bApertureRuptured = false;

    UPROPERTY(BlueprintReadOnly, Category="Gu|World|Population")
    bool bRequiresWillRefinement = false;

    UPROPERTY(BlueprintReadOnly, Category="Gu|World|Population")
    EGuContainer CapturedContainer = EGuContainer::World;

    UPROPERTY(BlueprintReadOnly, Category="Gu|World|Population")
    FGuid EntityId;

    UPROPERTY(BlueprintReadOnly, Category="Gu|World|Population")
    FString Error;
};
