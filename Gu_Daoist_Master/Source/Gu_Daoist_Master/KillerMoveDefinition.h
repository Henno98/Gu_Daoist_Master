#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "KillerMoveTypes.h"
#include "KillerMoveDefinition.generated.h"

/** Authored killer-move formula. Learned/runtime moves use FKillerMoveDefinitionRecord directly. */
UCLASS(BlueprintType)
class GU_DAOIST_MASTER_API UKillerMoveDefinition : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Killer Move")
    FKillerMoveDefinitionRecord Definition;
};
