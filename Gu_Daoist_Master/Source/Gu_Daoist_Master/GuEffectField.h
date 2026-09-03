#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GuEffectField.generated.h"

class UAbilitySystemComponent;
class UGuDefinition;
struct FGuFieldMechanic;

/**
 * Lightweight persistent carrier for field/mist/domain-like mortal Gu effects.
 * The field contains no bespoke payload logic: each pulse simply delivers the
 * owning UGuDefinition's ordinary impact mechanics to actors inside the area.
 */
UCLASS()
class GU_DAOIST_MASTER_API AGuEffectField : public AActor
{
    GENERATED_BODY()

public:
    AGuEffectField();

    virtual void Tick(float DeltaSeconds) override;

    void InitializeField(
        const FGuFieldMechanic& FieldMechanic,
        UGuDefinition* InDefinition,
        UAbilitySystemComponent* InSourceASC,
        AActor* InSourceActor);

private:
    void Pulse();

    UPROPERTY(Transient)
    TObjectPtr<UGuDefinition> Definition;

    TWeakObjectPtr<UAbilitySystemComponent> SourceASC;
    TWeakObjectPtr<AActor> SourceActor;

    float Radius = 300.0f;
    float TickInterval = 0.5f;
    int32 MaxTargetsPerPulse = 0;
    bool bIncludeSelf = false;
    double NextPulseAt = 0.0;
};
