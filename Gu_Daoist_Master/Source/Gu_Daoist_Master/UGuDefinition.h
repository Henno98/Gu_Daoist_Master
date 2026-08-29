// Fill out your copyright notice in the Description page of Project Settings.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "StructUtils/InstancedStruct.h"


#include "UGuDefinition.generated.h"


class AGu_Projectile;
class UGameplayEffect;


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
USTRUCT(BlueprintType)
struct FGuProjectileMechanic : public FGuMechanic
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<AGu_Projectile> ProjectileClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile")
	float Speed = 1500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile")
	float MaxRange = 1000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile")
	float Radius = 10.0f;
};

USTRUCT(BlueprintType)
struct FGuDamageMechanic : public FGuMechanic
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	float Damage = 10.0f;
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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gu")
	TSubclassOf<UGameplayEffect> PrimevalEssenceCostEffect;
};