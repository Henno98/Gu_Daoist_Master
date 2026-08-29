// Fill out your copyright notice in the Description page of Project Settings.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "StructUtils/InstancedStruct.h"


#include "UGuDefinition.generated.h"

USTRUCT(BlueprintType)
struct FGuMechanic
{
	GENERATED_BODY()
};


USTRUCT(BlueprintType)
struct FGuEssenceCostMechanic : public FGuMechanic
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gu")
	float Cost = 0.0f;
};

UCLASS(BlueprintType)
class GU_DAOIST_MASTER_API UGuDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gu")
	FText Name;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gu")
	int32 Rank = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gu")
	FGameplayTag Path;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gu")
	FGameplayTagContainer Tags;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gu|Mechanics")
	TArray<TInstancedStruct<FGuMechanic>> Mechanics;
};