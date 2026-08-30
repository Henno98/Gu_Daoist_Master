// Fill out your copyright notice in the Description page of Project Settings.

#include "GuExecutionLibrary.h"

#include "UGuDefinition.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "GameplayTagContainer.h"


namespace
{
	struct FGuImpactContext
	{
		UGuDefinition* GuDefinition = nullptr;
		UAbilitySystemComponent* SourceASC = nullptr;
		UAbilitySystemComponent* TargetASC = nullptr;
		AActor* TargetActor = nullptr;
	};

	using FImpactExecutor = bool(*)(
		const TInstancedStruct<FGuMechanic>&,
		const FGuImpactContext&
		);

	bool ExecuteDamage(
		const TInstancedStruct<FGuMechanic>& Mechanic,
		const FGuImpactContext& Context)
	{
		const FGuDamageMechanic* Damage =
			Mechanic.GetPtr<FGuDamageMechanic>();

		if (!Damage)
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("ExecuteDamage received wrong mechanic type")
			);

			return false;
		}

		if (Damage->Damage <= 0.0f)
		{
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("%s has invalid damage value %.1f"),
				*Context.GuDefinition->Name.ToString(),
				Damage->Damage
			);

			return false;
		}

		if (!Damage->EffectClass)
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("%s damage mechanic has no EffectClass"),
				*Context.GuDefinition->Name.ToString()
			);

			return false;
		}

		FGameplayEffectContextHandle EffectContext =
			Context.SourceASC->MakeEffectContext();

		EffectContext.AddSourceObject(
			Context.GuDefinition
		);

		FGameplayEffectSpecHandle DamageSpec =
			Context.SourceASC->MakeOutgoingSpec(
				Damage->EffectClass,
				1.0f,
				EffectContext
			);

		if (!DamageSpec.IsValid() ||
			!DamageSpec.Data.IsValid())
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("Failed to create damage spec for %s"),
				*Context.GuDefinition->Name.ToString()
			);

			return false;
		}

		const FGameplayTag DamageTag =
			FGameplayTag::RequestGameplayTag(
				FName("Data.Gu.Damage")
			);

		if (!DamageTag.IsValid())
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("Data.Gu.Damage GameplayTag is invalid")
			);

			return false;
		}

		DamageSpec.Data->SetSetByCallerMagnitude(
			DamageTag,
			-Damage->Damage
		);

		Context.SourceASC->ApplyGameplayEffectSpecToTarget(
			*DamageSpec.Data.Get(),
			Context.TargetASC
		);

		UE_LOG(
			LogTemp,
			Warning,
			TEXT("%s dealt %.1f damage to %s"),
			*Context.GuDefinition->Name.ToString(),
			Damage->Damage,
			*Context.TargetActor->GetName()
		);

		return true;
	}


	const TMap<const UScriptStruct*, FImpactExecutor>&
		GetImpactExecutors()
	{
		static const TMap<
			const UScriptStruct*,
			FImpactExecutor
		> Executors =
		{
			{
				FGuDamageMechanic::StaticStruct(),
				&ExecuteDamage
			}
		};

		return Executors;
	}
}






bool UGuExecutionLibrary::ExecuteImpact(
	UGuDefinition* GuDefinition,
	UAbilitySystemComponent* SourceASC,
	AActor* TargetActor
	)
{
	if (!GuDefinition)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("ExecuteImpact: GuDefinition is null")
		);

		return false;
	}

	if (!SourceASC)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("ExecuteImpact: SourceASC is null for %s"),
			*GuDefinition->Name.ToString()
		);

		return false;
	}

	if (!TargetActor ||
		TargetActor->IsActorBeingDestroyed())
	{
		return false;
	}

	UAbilitySystemComponent* TargetASC =
		UAbilitySystemBlueprintLibrary::
		GetAbilitySystemComponent(TargetActor);

	if (!TargetASC)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("%s has no AbilitySystemComponent"),
			*TargetActor->GetName()
		);

		return false;
	}

	const FGuImpactContext Context
	{
		GuDefinition,
		SourceASC,
		TargetASC,
		TargetActor
	};

	const auto& Executors =
		GetImpactExecutors();

	bool bExecutedAnything = false;

	for (const TInstancedStruct<FGuMechanic>& Mechanic :
		GuDefinition->Mechanics)
	{
		const UScriptStruct* MechanicType =
			Mechanic.GetScriptStruct();

		if (!MechanicType)
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("%s contains an invalid mechanic"),
				*GuDefinition->Name.ToString()
			);

			continue;
		}

		const FImpactExecutor* Executor =
			Executors.Find(MechanicType);

		// Perfectly normal:
		// EssenceCost and Projectile are not impact mechanics.
		if (!Executor)
		{
			continue;
		}

		bExecutedAnything |=
			(*Executor)(Mechanic, Context);
	}

	return bExecutedAnything;
}