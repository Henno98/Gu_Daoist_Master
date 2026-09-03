#include "AS_GuMasterAttributeSet.h"
#include "GameplayEffectExtension.h"

UAS_GuMasterAttributeSet::UAS_GuMasterAttributeSet()
{
}

void UAS_GuMasterAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
    Super::PreAttributeChange(Attribute, NewValue);

    if (Attribute == GetPrimevalEssenceAttribute())
    {
        NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxPrimevalEssence());
    }
    else if (Attribute == GetMaxPrimevalEssenceAttribute())
    {
        NewValue = FMath::Max(NewValue, 0.0f);
    }
    else if (Attribute == GetHealthAttribute())
    {
        NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxHealth());
    }
    else if (Attribute == GetMaxHealthAttribute())
    {
        NewValue = FMath::Max(NewValue, 0.0f);
    }
    else if (Attribute == GetShieldAttribute())
    {
        NewValue = FMath::Max(NewValue, 0.0f);
    }
}

void UAS_GuMasterAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
    Super::PostGameplayEffectExecute(Data);

    if (Data.EvaluatedData.Attribute == GetHealthAttribute())
    {
        SetHealth(FMath::Clamp(GetHealth(), 0.0f, GetMaxHealth()));
        UE_LOG(LogTemp, Warning, TEXT("Health after effect: %.1f / %.1f"), GetHealth(), GetMaxHealth());
    }
    else if (Data.EvaluatedData.Attribute == GetPrimevalEssenceAttribute())
    {
        SetPrimevalEssence(FMath::Clamp(GetPrimevalEssence(), 0.0f, GetMaxPrimevalEssence()));
    }
    else if (Data.EvaluatedData.Attribute == GetShieldAttribute())
    {
        SetShield(FMath::Max(0.0f, GetShield()));
    }
}
