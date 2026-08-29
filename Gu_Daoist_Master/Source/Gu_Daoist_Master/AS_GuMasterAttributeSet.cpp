// Fill out your copyright notice in the Description page of Project Settings.


#include "AS_GuMasterAttributeSet.h"

UAS_GuMasterAttributeSet::UAS_GuMasterAttributeSet()
{
}

void UAS_GuMasterAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{


    Super::PreAttributeChange(Attribute, NewValue);

    if (Attribute == GetPrimevalEssenceAttribute())
    {
        NewValue = FMath::Clamp(
            NewValue,
            0.0f,
            GetMaxPrimevalEssence()
        );
    }

    if (Attribute == GetMaxPrimevalEssenceAttribute())
    {
        NewValue = FMath::Max(NewValue, 0.0f);
    }
}
