// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "AttributeSet.h"
#include "GuSystemConfig.generated.h"

class UGameplayEffect;

USTRUCT(BlueprintType)
struct FGuBuffEffectBinding
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Buff")
	FGameplayAttribute Attribute;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Buff")
	TSubclassOf<UGameplayEffect> EffectClass;
};


UCLASS(BlueprintType)
class GU_DAOIST_MASTER_API UGuSystemConfig
	: public UDataAsset
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Buff")
	TArray<FGuBuffEffectBinding> AdditiveBuffEffects;

	TSubclassOf<UGameplayEffect> FindAdditiveBuffEffect(
		const FGameplayAttribute& Attribute
	) const;
};