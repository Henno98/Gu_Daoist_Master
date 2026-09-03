#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GuDefinitionTypes.h"
#include "GuProceduralGeneratorSubsystem.generated.h"

class AGu_Daoist_MasterCharacter;
class UGuDefinition;

/** High-level purpose used to bias the generated mechanic composition. */
UENUM(BlueprintType)
enum class EProceduralGuRole : uint8
{
    Auto,
    Offense,
    Defense,
    Movement,
    Healing,
    Control,
    Investigation,
    Concealment,
    Resource,
    Refinement,
    Support
};

USTRUCT(BlueprintType)
struct FProceduralGuGenerationRequest
{
    GENERATED_BODY()

    /** Must use the project path namespace, e.g. Data.Paths.Moon. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gu|Generation", meta=(Categories="Data.Paths"))
    FGameplayTag PrimaryPath;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gu|Generation", meta=(Categories="Data.Paths"))
    FGameplayTagContainer SecondaryPaths;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gu|Generation", meta=(ClampMin="1", ClampMax="9"))
    int32 Rank = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gu|Generation")
    EProceduralGuRole Role = EProceduralGuRole::Auto;

    /** Zero chooses a fresh random seed. Non-zero seeds are deterministic. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gu|Generation")
    int32 Seed = 0;

    /** Zero derives complexity from Rank. 1-5 explicitly controls secondary mechanic count. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gu|Generation", meta=(ClampMin="0", ClampMax="5"))
    int32 Complexity = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gu|Generation")
    bool bAllowConsumable = true;
};

USTRUCT(BlueprintType)
struct FProceduralGuGenerationResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Gu|Generation")
    FName DefinitionId;

    UPROPERTY(BlueprintReadOnly, Category="Gu|Generation")
    FGuid EntityId;

    UPROPERTY(BlueprintReadOnly, Category="Gu|Generation")
    FString Name;

    UPROPERTY(BlueprintReadOnly, Category="Gu|Generation")
    int32 Seed = 0;

    UPROPERTY(BlueprintReadOnly, Category="Gu|Generation")
    EProceduralGuRole Role = EProceduralGuRole::Auto;

    UPROPERTY(BlueprintReadOnly, Category="Gu|Generation")
    FString Summary;

    /** True when generation resolved to an already-known canonical runtime species. */
    UPROPERTY(BlueprintReadOnly, Category="Gu|Generation")
    bool bReusedExistingSpecies = false;

    /** Transient executable definition. The registry owns a strong reference after registration. */
    UPROPERTY(BlueprintReadOnly, Transient, Category="Gu|Generation")
    TObjectPtr<UGuDefinition> Definition = nullptr;
};

/**
 * Deterministic procedural Gu compiler.
 *
 * It generates an executable transient UGuDefinition, converts it into the same
 * FGuDefinitionRecord used by ECS/refinement, registers both together, and can
 * immediately create/grant a physical Gu instance. This is the bridge that lets
 * thousands of Gu share a finite mechanic vocabulary instead of requiring one
 * hand-authored DataAsset/ability class per species.
 */
UCLASS()
class GU_DAOIST_MASTER_API UGuProceduralGeneratorSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Gu|Generation")
    bool GenerateAndRegisterGu(const FProceduralGuGenerationRequest& Request, FProceduralGuGenerationResult& OutResult, FString& OutError);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Gu|Generation")
    bool GenerateAndGrantGu(AGu_Daoist_MasterCharacter* Character, const FProceduralGuGenerationRequest& Request, FProceduralGuGenerationResult& OutResult, FString& OutError);

    /** Converts a refinement/runtime domain record into an executable transient UGuDefinition and registers it. */
    bool CompileAndRegisterRuntimeRecord(const FGuDefinitionRecord& SourceRecord, UGuDefinition*& OutDefinition, FString& OutError, bool bReplaceExisting = true, FName* OutCanonicalDefinitionId = nullptr);

    static FString RoleToString(EProceduralGuRole Role);
    static bool TryParseRole(const FString& Text, EProceduralGuRole& OutRole);

private:
    bool BuildGeneratedDefinition(const FProceduralGuGenerationRequest& Request, UGuDefinition*& OutDefinition, FGuDefinitionRecord& OutRecord, int32& OutEffectiveSeed, EProceduralGuRole& OutRole, FString& OutError);
    bool BuildDefinitionFromRuntimeRecord(const FGuDefinitionRecord& SourceRecord, UGuDefinition*& OutDefinition, FGuDefinitionRecord& OutRecord, FString& OutError);
};
