#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GuInstanceObject.generated.h"

class UGuDefinition;

/**
 * GAS source-object bridge for one physical Gu worm.
 *
 * The DataAsset says what species it is. EntityId says which actual worm is
 * being activated, so lifecycle/condition/charges can live in ECS without
 * throwing away the project's existing GameplayAbility setup.
 */
UCLASS(BlueprintType)
class GU_DAOIST_MASTER_API UGuInstanceObject : public UObject
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintReadOnly, Category="Gu|ECS")
    FGuid EntityId;

    UPROPERTY(BlueprintReadOnly, Category="Gu|ECS")
    TObjectPtr<UGuDefinition> Definition;

    void Initialize(const FGuid& InEntityId, UGuDefinition* InDefinition)
    {
        EntityId = InEntityId;
        Definition = InDefinition;
    }
};
