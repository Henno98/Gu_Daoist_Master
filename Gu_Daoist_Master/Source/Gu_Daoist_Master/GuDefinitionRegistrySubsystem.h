#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GuDefinitionTypes.h"
#include "GuDefinitionRegistrySubsystem.generated.h"

class UGuDefinition;

/**
 * Runtime registry joining the original UGuDefinition DataAssets to the domain ECS.
 *
 * UGuDefinition remains the authored/GAS-facing species asset. The registry builds
 * a plain FGuDefinitionRecord from it so physical instances and refinement never
 * need to depend on a UObject asset existing as mutable instance state.
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

    UFUNCTION(BlueprintPure, Category="Gu|Definitions")
    bool HasDefinition(FName IdOrName) const;

    UFUNCTION(BlueprintCallable, Category="Gu|Definitions")
    bool GetDefinition(FName IdOrName, FGuDefinitionRecord& OutDefinition) const;

    const FGuDefinitionRecord* FindDefinition(FName IdOrName) const;

    UFUNCTION(BlueprintPure, Category="Gu|Definitions")
    TArray<FGuDefinitionRecord> GetAllDefinitions() const;

    /** Runtime/refinement-created definitions only. */
    UFUNCTION(BlueprintPure, Category="Gu|Definitions")
    TArray<FGuDefinitionRecord> GetRuntimeDefinitions() const;

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
    TMap<FString, FName> IdByName;
    TSet<FName> RuntimeDefinitionIds;
};
