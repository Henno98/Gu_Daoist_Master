// Fill out your copyright notice in the Description page of Project Settings.
#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "AS_GuMasterAttributeSet.generated.h"

struct FGameplayEffectModCallbackData;

#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
    GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
    GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
    GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
    GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

UCLASS()
class GU_DAOIST_MASTER_API UAS_GuMasterAttributeSet : public UAttributeSet
{
    GENERATED_BODY()

public:
    UAS_GuMasterAttributeSet();

    virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
    virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gu|Essence")
    FGameplayAttributeData PrimevalEssence;
    ATTRIBUTE_ACCESSORS(UAS_GuMasterAttributeSet, PrimevalEssence)

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gu|Essence")
    FGameplayAttributeData MaxPrimevalEssence;
    ATTRIBUTE_ACCESSORS(UAS_GuMasterAttributeSet, MaxPrimevalEssence)

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Attributes|Health")
    FGameplayAttributeData Health;
    ATTRIBUTE_ACCESSORS(UAS_GuMasterAttributeSet, Health)

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Attributes|Health")
    FGameplayAttributeData MaxHealth;
    ATTRIBUTE_ACCESSORS(UAS_GuMasterAttributeSet, MaxHealth)

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Attributes|Defense")
    FGameplayAttributeData Defense;
    ATTRIBUTE_ACCESSORS(UAS_GuMasterAttributeSet, Defense)

    /** Temporary protective capacity. Damage consumes Shield before Health. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Attributes|Shield")
    FGameplayAttributeData Shield;
    ATTRIBUTE_ACCESSORS(UAS_GuMasterAttributeSet, Shield)
};
