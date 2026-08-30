// Fill out your copyright notice in the Description page of Project Settings.


#include "GuSystemConfig.h"

TSubclassOf<UGameplayEffect>
UGuSystemConfig::FindAdditiveBuffEffect(
	const FGameplayAttribute& Attribute) const
{
	if (!Attribute.IsValid())
	{
		return nullptr;
	}

	for (const FGuBuffEffectBinding& Binding :
		AdditiveBuffEffects)
	{
		if (Binding.Attribute == Attribute)
		{
			return Binding.EffectClass;
		}
	}

	return nullptr;
}