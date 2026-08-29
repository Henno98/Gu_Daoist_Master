// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_GuAbility.generated.h"


struct FGuEssenceCostMechanic;
class UGuDefinition;
/**
 * 
 */
UCLASS()
class GU_DAOIST_MASTER_API UGA_GuAbility : public UGameplayAbility
{
	GENERATED_BODY()
public:
	UGA_GuAbility();

protected:
	UGuDefinition* GetGuDefinition(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo) const;

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData
	) override;

	virtual bool CheckCost(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayTagContainer* OptionalRelevantTags = nullptr
	) const override;

	virtual void ApplyCost(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo
	) const override;

	const FGuEssenceCostMechanic* GetEssenceCostMechanic(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo) const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gu")
	TSubclassOf<UGameplayEffect> PrimevalEssenceCostEffect;
};
