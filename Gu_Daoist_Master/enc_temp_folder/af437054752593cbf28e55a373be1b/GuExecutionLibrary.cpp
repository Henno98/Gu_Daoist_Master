// Fill out your copyright notice in the Description page of Project Settings.

#include "GuExecutionLibrary.h"

#include "UGuDefinition.h"
#include "GameFramework/Character.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/EngineTypes.h"
#include "GuSystemConfig.h"
#include "GameplayTagContainer.h"


namespace
{
	struct FGuImpactContext
	{
		UGuDefinition* GuDefinition = nullptr;
		UAbilitySystemComponent* SourceASC = nullptr;
		UAbilitySystemComponent* TargetASC = nullptr;
		AActor* TargetActor = nullptr;

		const FHitResult* HitResult = nullptr;
	};
	struct FGuActivationContext
	{
		UGuDefinition* GuDefinition = nullptr;
		UAbilitySystemComponent* SourceASC = nullptr;
		AActor* SourceActor = nullptr;
		const UGuSystemConfig* SystemConfig = nullptr;
	};
	using FImpactExecutor = bool(*)(
		const TInstancedStruct<FGuMechanic>&,
		const FGuImpactContext&
		);
	using FActivationExecutor = bool(*)(
		const TInstancedStruct<FGuMechanic>&,
		const FGuActivationContext&
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

	bool ExecuteKnockback(
		const TInstancedStruct<FGuMechanic>& Mechanic,
		const FGuImpactContext& Context)
	{
		const FGuKnockbackMechanic* Knockback =
			Mechanic.GetPtr<FGuKnockbackMechanic>();

		if (!Knockback || !Context.TargetActor)
		{
			return false;
		}

		ACharacter* Character =
			Cast<ACharacter>(Context.TargetActor);

		if (!Character)
		{
			return false;
		}
		UCharacterMovementComponent* Movement =
			Character->GetCharacterMovement();

		if (!Movement)
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("ExecuteKnockback: Character has no CharacterMovementComponent")
			);

			return false;
		}

		UE_LOG(
			LogTemp,
			Warning,
			TEXT(
				"Knockback target state — Controller: %s, "
				"MovementMode: %d, Active: %s, "
				"RunPhysicsNoController: %s, Velocity: %s"
			),
			*GetNameSafe(Character->GetController()),
			static_cast<int32>(Movement->MovementMode),
			Movement->IsActive() ? TEXT("true") : TEXT("false"),
			Movement->bRunPhysicsWithNoController
			? TEXT("true")
			: TEXT("false"),
			*Movement->Velocity.ToString()
		);

		FVector Direction =
			Context.HitResult
			? (
				Context.HitResult->TraceEnd -
				Context.HitResult->TraceStart
				).GetSafeNormal()
			: FVector::ZeroVector;

		if (Direction.IsNearlyZero())
		{
			return false;
		}

		FVector LaunchVelocity =
			Direction * Knockback->Strength;

		LaunchVelocity.Z +=
			Knockback->VerticalStrength;

		Character->LaunchCharacter(
			LaunchVelocity,
			false,
			false
		);

		UE_LOG(
			LogTemp,
			Warning,
			TEXT("%s knocked back %s with strength %.1f"),
			*Context.GuDefinition->Name.ToString(),
			*Context.TargetActor->GetName(),
			Knockback->Strength
		);

		return true;
	}

	bool ExecuteBuff(
		const TInstancedStruct<FGuMechanic>& Mechanic,
		const FGuActivationContext& Context)
	{
		const FGuBuffMechanic* Buff =
			Mechanic.GetPtr<FGuBuffMechanic>();

		if (!Buff)
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("ExecuteBuff received wrong mechanic type")
			);

			return false;
		}

		if (!Context.SourceASC)
		{
			return false;
		}

		if (!Context.SystemConfig)
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("%s cannot execute buff: GuSystemConfig is null"),
				*Context.GuDefinition->Name.ToString()
			);

			return false;
		}

		if (!Buff->Attribute.IsValid())
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("%s contains buff with invalid attribute"),
				*Context.GuDefinition->Name.ToString()
			);

			return false;
		}

		if (Buff->Duration <= 0.0f)
		{
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("%s contains buff with invalid duration %.2f"),
				*Context.GuDefinition->Name.ToString(),
				Buff->Duration
			);

			return false;
		}

		TSubclassOf<UGameplayEffect> EffectClass =
			Context.SystemConfig->FindAdditiveBuffEffect(
				Buff->Attribute
			);

		if (!EffectClass)
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT(
					"%s has no additive buff GameplayEffect registered for %s"
				),
				*Context.GuDefinition->Name.ToString(),
				*Buff->Attribute.GetName()
			);

			return false;
		}

		FGameplayEffectContextHandle EffectContext =
			Context.SourceASC->MakeEffectContext();

		EffectContext.AddSourceObject(
			Context.GuDefinition
		);

		FGameplayEffectSpecHandle BuffSpec =
			Context.SourceASC->MakeOutgoingSpec(
				EffectClass,
				1.0f,
				EffectContext
			);

		if (!BuffSpec.IsValid() ||
			!BuffSpec.Data.IsValid())
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("Failed to create buff spec for %s"),
				*Context.GuDefinition->Name.ToString()
			);

			return false;
		}

		const FGameplayTag MagnitudeTag =
			FGameplayTag::RequestGameplayTag(
				FName("Data.Gu.Buff.Magnitude")
			);

		if (!MagnitudeTag.IsValid())
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("Data.Gu.Buff.Magnitude GameplayTag is invalid")
			);

			return false;
		}

		BuffSpec.Data->SetSetByCallerMagnitude(
			MagnitudeTag,
			Buff->Magnitude
		);

		BuffSpec.Data->SetDuration(
			Buff->Duration,
			true
		);

		Context.SourceASC->ApplyGameplayEffectSpecToSelf(
			*BuffSpec.Data.Get()
		);

		UE_LOG(
			LogTemp,
			Warning,
			TEXT(
				"%s buffed %s by %.1f for %.1f seconds"
			),
			*Context.GuDefinition->Name.ToString(),
			*Buff->Attribute.GetName(),
			Buff->Magnitude,
			Buff->Duration
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
			},
			{
				FGuKnockbackMechanic::StaticStruct(),
				&ExecuteKnockback
			},
		};

		return Executors;
	}
	const TMap<const UScriptStruct*, FActivationExecutor>&
		GetActivationExecutors()
	{
		static const TMap<
			const UScriptStruct*,
			FActivationExecutor
		> Executors =
		{
			{
				FGuBuffMechanic::StaticStruct(),
				&ExecuteBuff
			}
		};

		return Executors;
	}
}


bool UGuExecutionLibrary::ExecuteImpact(
	UGuDefinition* GuDefinition,
	UAbilitySystemComponent* SourceASC,
	AActor* TargetActor,
	const FHitResult& HitResult)
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
			TEXT("ExecuteImpact: SourceASC is null for Gu %s"),
			*GuDefinition->Name.ToString()
		);

		return false;
	}

	if (!TargetActor)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("ExecuteImpact: TargetActor is null for Gu %s"),
			*GuDefinition->Name.ToString()
		);

		return false;
	}

	if (TargetActor->IsActorBeingDestroyed())
	{
		return false;
	}

	UAbilitySystemComponent* TargetASC =
		UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(
			TargetActor
		);

	if (!TargetASC)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT(
				"ExecuteImpact: Target %s has no AbilitySystemComponent"
			),
			*TargetActor->GetName()
		);

		return false;
	}

	const FGuImpactContext Context
	{
		GuDefinition,
		SourceASC,
		TargetASC,
		TargetActor,
		&HitResult
	};

	const TMap<const UScriptStruct*, FImpactExecutor>& Executors =
		GetImpactExecutors();

	bool bExecutedAnything = false;

	for (const TInstancedStruct<FGuMechanic>& Mechanic :
		GuDefinition->Mechanics)
	{
		const UScriptStruct* MechanicType =
			Mechanic.GetScriptStruct();

		if (!MechanicType)
		{
			continue;
		}

		const FImpactExecutor* Executor =
			Executors.Find(MechanicType);

		if (!Executor)
		{
			continue;
		}

		bExecutedAnything |=
			(*Executor)(Mechanic, Context);
	}

	if (!bExecutedAnything)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT(
				"ExecuteImpact: %s hit %s but executed no impact mechanics"
			),
			*GuDefinition->Name.ToString(),
			*TargetActor->GetName()
		);
	}

	return bExecutedAnything;
}

bool UGuExecutionLibrary::ExecuteActivation(UGuDefinition* GuDefinition, UAbilitySystemComponent* SourceASC,
	AActor* SourceActor, const UGuSystemConfig* SystemConfig)
{

		if (!GuDefinition)
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("ExecuteActivation: GuDefinition is null")
			);

			return false;
		}

		if (!SourceASC)
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT(
					"ExecuteActivation: SourceASC is null for %s"
				),
				*GuDefinition->Name.ToString()
			);

			return false;
		}

		if (!SourceActor)
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT(
					"ExecuteActivation: SourceActor is null for %s"
				),
				*GuDefinition->Name.ToString()
			);

			return false;
		}

		const FGuActivationContext Context
		{
			GuDefinition,
			SourceASC,
			SourceActor,
			SystemConfig
		};

		const auto& Executors =
			GetActivationExecutors();

		bool bExecutedAnything = false;

		for (const TInstancedStruct<FGuMechanic>& Mechanic :
			GuDefinition->Mechanics)
		{
			const UScriptStruct* MechanicType =
				Mechanic.GetScriptStruct();

			if (!MechanicType)
			{
				continue;
			}

			const FActivationExecutor* Executor =
				Executors.Find(MechanicType);

			if (!Executor)
			{
				continue;
			}

			bExecutedAnything |=
				(*Executor)(Mechanic, Context);
		}

		return bExecutedAnything;
}



