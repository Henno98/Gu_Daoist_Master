#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GuDefinitionTypes.h"
#include "GuDefinitionRegistrySubsystem.generated.h"

class UGuDefinition;

/**
 * Runtime registry joining the original UGuDefinition DataAssets to the domain ECS.
 *
 * UGuDefinition remains the executable/GAS-facing species definition. Authored
 * DataAssets and transient procedural species both normalize into FGuDefinitionRecord,
 * while physical instances/refinement keep mutable state in ECS rather than the UObject.
 */
UCLASS()
class GU_DAOIST_MASTER_API UGuDefinitionRegistrySubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category="Gu|Definitions")
    bool RegisterDefinition(const FGuDefinitionRecord& Definition, FString& OutError, bool bReplaceExisting = false);

    UFUNCTION(BlueprintCallable, Category="Gu|Definitions")
    bool RegisterDefinitionAsset(const UGuDefinition* Asset, FString& OutError, bool bReplaceExisting = false);

    /** Registers a transient executable UGuDefinition together with its richer runtime/domain record. */
    bool RegisterRuntimeDefinitionAsset(const FGuDefinitionRecord& Definition, UGuDefinition* RuntimeAsset, FString& OutError, bool bReplaceExisting = false);

    UFUNCTION(BlueprintPure, Category="Gu|Definitions")
    bool HasDefinition(FName IdOrName) const;

    UFUNCTION(BlueprintCallable, Category="Gu|Definitions")
    bool GetDefinition(FName IdOrName, FGuDefinitionRecord& OutDefinition) const;

    const FGuDefinitionRecord* FindDefinition(FName IdOrName) const;

    /** Returns the executable authored or transient runtime UGuDefinition for this species. */
    const UGuDefinition* FindDefinitionAsset(FName IdOrName) const;

    UFUNCTION(BlueprintPure, Category="Gu|Definitions")
    TArray<FGuDefinitionRecord> GetAllDefinitions() const;

    /** Runtime/refinement-created definitions only. */
    UFUNCTION(BlueprintPure, Category="Gu|Definitions")
    TArray<FGuDefinitionRecord> GetRuntimeDefinitions() const;

    /** Stable functional fingerprint for runtime/procedural species duplicate prevention. */
    static FString ComputeRuntimeSpeciesFingerprint(const FGuDefinitionRecord& Definition);

    /** Finds an already-registered runtime species with the same canonical functional fingerprint. */
    bool FindEquivalentRuntimeDefinition(const FGuDefinitionRecord& Definition, FName& OutDefinitionId) const;

    void ClearRuntimeDefinitions();

    /** Stable domain ID used by existing DataAssets and their physical ECS instances. */
    UFUNCTION(BlueprintPure, Category="Gu|Definitions")
    static FName DefinitionIdForAsset(const UGuDefinition* Asset);

    /** Converts the original project DataAsset schema into the shared domain schema. */
    static bool BuildRecordFromAsset(const UGuDefinition* Asset, FGuDefinitionRecord& OutRecord, FString& OutError);

private:
    static FString NameKey(const FString& Name);
    static bool ValidateAndNormalize(FGuDefinitionRecord& InOutDefinition, FString& OutError);

    TMap<FName, FGuDefinitionRecord> DefinitionsById;
    UPROPERTY(Transient)
    TMap<FName, TObjectPtr<UGuDefinition>> AuthoredAssetsById;

    /** Strong references keep transient procedurally/refinement-generated executable definitions alive. */
    UPROPERTY(Transient)
    TMap<FName, TObjectPtr<UGuDefinition>> RuntimeAssetsById;

    TMap<FString, FName> IdByName;
    TSet<FName> RuntimeDefinitionIds;
    TMap<FString, FName> RuntimeIdByFingerprint;
};
