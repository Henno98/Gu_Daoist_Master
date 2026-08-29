// Fill out your copyright notice in the Description page of Project Settings.


#include "GA_GuAbility.h"
#include "Gu_Projectile.h"
#include "AS_GuMasterAttributeSet.h"
#include "UGuDefinition.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

UGA_GuAbility::UGA_GuAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

UGuDefinition* UGA_GuAbility::GetGuDefinition(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo) const
{
	if (!ActorInfo || !ActorInfo->AbilitySystemComponent.IsValid())
	{
		return nullptr;
	}

	const FGameplayAbilitySpec* Spec =
		ActorInfo->AbilitySystemComponent->FindAbilitySpecFromHandle(Handle);

	if (!Spec)
	{
		return nullptr;
	}

	return Cast<UGuDefinition>(Spec->SourceObject.Get());
}

void UGA_GuAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(
		Handle,
		ActorInfo,
		ActivationInfo,
		TriggerEventData
	);
	UE_LOG(LogTemp, Warning, TEXT("UGA_GuAbility::ActivateAbility entered"));
	UGuDefinition* GuDefinition = GetGuDefinition(Handle, ActorInfo);

	if (!GuDefinition)
	{
		EndAbility(
			Handle,
			ActorInfo,
			ActivationInfo,
			true,
			true
		);

		return;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const FGuProjectileMechanic* ProjectileMechanic = nullptr;

	for (const TInstancedStruct<FGuMechanic>& Mechanic : GuDefinition->Mechanics)
	{
		if (const FGuProjectileMechanic* ProjectileData =
			Mechanic.GetPtr<FGuProjectileMechanic>())
		{
			ProjectileMechanic = ProjectileData;
			break;
		}
	}

	if (!ProjectileMechanic)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("%s has no FGuProjectileMechanic"),
			*GuDefinition->Name.ToString()
		);

		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!ProjectileMechanic->ProjectileClass)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("Projectile mechanic has no ProjectileClass assigned")
		);

		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	AActor* Avatar = ActorInfo->AvatarActor.Get();

	if (!Avatar)
	{
		UE_LOG(LogTemp, Error, TEXT("No AvatarActor"));

		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const FVector SpawnLocation =
		Avatar->GetActorLocation()
		+ Avatar->GetActorForwardVector() * 100.0f;

	const FTransform SpawnTransform(
		Avatar->GetActorRotation(),
		SpawnLocation
	);

	AGu_Projectile* SpawnedProjectile =
		GetWorld()->SpawnActorDeferred<AGu_Projectile>(
			ProjectileMechanic->ProjectileClass,
			SpawnTransform,
			Avatar,
			Cast<APawn>(Avatar),
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn
		);

	if (!SpawnedProjectile)
	{
		UE_LOG(LogTemp, Error, TEXT("SpawnActorDeferred failed"));

		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	SpawnedProjectile->FinishSpawning(SpawnTransform);

	SpawnedProjectile->InitializeProjectile(
		*ProjectileMechanic,
		GuDefinition,
		ActorInfo->AbilitySystemComponent.Get()
	);

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("Projectile spawned: %s"),
		*SpawnedProjectile->GetName()
	);
	UE_LOG(
		LogTemp,
		Warning,
		TEXT("Activated Gu: %s"),
		*GuDefinition->Name.ToString()
	);

	EndAbility(
		Handle,
		ActorInfo,
		ActivationInfo,
		true,
		false
	);
}

bool UGA_GuAbility::CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	FGameplayTagContainer* OptionalRelevantTags) const
{

	if (!Super::CheckCost(Handle, ActorInfo, OptionalRelevantTags))
	{
		return false;
	}

	const FGuEssenceCostMechanic* EssenceCost =
		GetEssenceCostMechanic(Handle, ActorInfo);

	if (!EssenceCost)
	{
		return true;
	}

	const UAbilitySystemComponent* ASC =
		ActorInfo->AbilitySystemComponent.Get();

	if (!ASC)
	{
		return false;
	}

	const float CurrentEssence =
		ASC->GetNumericAttribute(
			UAS_GuMasterAttributeSet::GetPrimevalEssenceAttribute()
		);

	return CurrentEssence >= EssenceCost->Cost;
}

void UGA_GuAbility::ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo) const
{
	Super::ApplyCost(
		Handle,
		ActorInfo,
		ActivationInfo
	);

	const FGuEssenceCostMechanic* EssenceCost =
		GetEssenceCostMechanic(Handle, ActorInfo);

	if (!EssenceCost || !PrimevalEssenceCostEffect)
	{
		return;
	}

	FGameplayEffectSpecHandle CostSpec =
		MakeOutgoingGameplayEffectSpec(
			PrimevalEssenceCostEffect,
			GetAbilityLevel(Handle, ActorInfo)
		);

	if (!CostSpec.IsValid())
	{
		return;
	}

	const FGameplayTag EssenceCostTag =
		FGameplayTag::RequestGameplayTag(
			FName("Data.Gu.EssenceCost")
		);

	CostSpec.Data->SetSetByCallerMagnitude(
		EssenceCostTag,
		-EssenceCost->Cost
	);

	ApplyGameplayEffectSpecToOwner(
		Handle,
		ActorInfo,
		ActivationInfo,
		CostSpec
	);

	const float RemainingEssence =
		ActorInfo->AbilitySystemComponent->GetNumericAttribute(
			UAS_GuMasterAttributeSet::GetPrimevalEssenceAttribute()
		);

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("Primeval Essence after cost: %.1f"),
		RemainingEssence
	);
}

const FGuEssenceCostMechanic* UGA_GuAbility::GetEssenceCostMechanic(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo) const
{
	const UGuDefinition* GuDefinition =
		GetGuDefinition(Handle, ActorInfo);

	if (!GuDefinition)
	{
		return nullptr;
	}

	for (const TInstancedStruct<FGuMechanic>& Mechanic : GuDefinition->Mechanics)
	{
		if (const FGuEssenceCostMechanic* Cost =
			Mechanic.GetPtr<FGuEssenceCostMechanic>())
		{
			return Cost;
		}
	}

	return nullptr;
}