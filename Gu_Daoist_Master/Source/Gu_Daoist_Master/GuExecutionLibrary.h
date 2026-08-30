// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GuExecutionLibrary.generated.h"

class UGuDefinition;
class UAbilitySystemComponent;


UCLASS()
class GU_DAOIST_MASTER_API UGuExecutionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	static bool ExecuteImpact(
		UGuDefinition* GuDefinition,
		UAbilitySystemComponent* SourceASC,
		AActor* TargetActor
	);
};