#pragma once

#include "CoreMinimal.h"
#include "GuHUDTypes.generated.h"

UENUM(BlueprintType)
enum class EGuHUDTab : uint8
{
    None,
    Gu,
    Refinement,
    KillerMove
};
