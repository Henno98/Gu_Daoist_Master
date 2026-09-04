#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GuExecutionLibrary.generated.h"

class UGuDefinition;
class UAbilitySystemComponent;
struct FHitResult;
class UGuSystemConfig;

UCLASS()
class GU_DAOIST_MASTER_API UGuExecutionLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /** Executes payload mechanics that apply when a carrier reaches a target. */
    static bool ExecuteImpact(
        UGuDefinition* GuDefinition,
        UAbilitySystemComponent* SourceASC,
        AActor* TargetActor,
        const FHitResult& HitResult);

    /**
     * Executes only terminal impact payloads. Carrier mechanics such as Field/Area/Melee
     * are deliberately ignored. Persistent carriers use this to pulse without recursively
     * spawning themselves.
     */
    static bool ExecutePayloadImpact(
        UGuDefinition* GuDefinition,
        UAbilitySystemComponent* SourceASC,
        AActor* TargetActor,
        const FHitResult& HitResult,
        float MagnitudeScale = 1.0f,
        bool bAllowChain = true);

    /** Executes self effects and instant non-projectile carriers such as melee/area. */
    static bool ExecuteActivation(
        UGuDefinition* GuDefinition,
        UAbilitySystemComponent* SourceASC,
        AActor* SourceActor,
        const UGuSystemConfig* SystemConfig);
};
