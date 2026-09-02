// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "AS_GuMasterAttributeSet.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "Gu_Daoist_MasterCharacter.generated.h"

class UInputComponent;
class USkeletalMeshComponent;
class UCameraComponent;
class UInputAction;
struct FInputActionValue;
class UGameplayEffect;
class UGameplayAbility;
class UGuDefinition;
class UGuInstanceObject;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

UCLASS(abstract)
class AGu_Daoist_MasterCharacter : public ACharacter, public IAbilitySystemInterface
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
    USkeletalMeshComponent* FirstPersonMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
    UCameraComponent* FirstPersonCameraComponent;

protected:
    virtual void BeginPlay() override;

    UPROPERTY(EditAnywhere, Category="Input")
    UInputAction* JumpAction;

    UPROPERTY(EditAnywhere, Category="Input")
    UInputAction* MoveAction;

    UPROPERTY(EditAnywhere, Category="Input")
    UInputAction* LookAction;

    UPROPERTY(EditAnywhere, Category="Input")
    UInputAction* MouseLookAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input")
    TObjectPtr<UInputAction> ActivateGuAction;

    FGameplayAbilitySpecHandle TestGuAbilityHandle;

public:
    AGu_Daoist_MasterCharacter();

    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
    void ActivateTestGu(const FInputActionValue& Value);
    virtual void PossessedBy(AController* NewController) override;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Abilities")
    TSubclassOf<UGameplayEffect> InitialAttributesEffect;

    UPROPERTY()
    TObjectPtr<UAS_GuMasterAttributeSet> AttributeSet;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Gu")
    TSubclassOf<UGameplayAbility> GuAbilityClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Gu")
    TObjectPtr<UGuDefinition> TestGuDefinition;

    /** Physical ECS entity backing the current test Gu ability. */
    UPROPERTY(BlueprintReadOnly, Category="Gu|ECS")
    FGuid TestGuEntityId;

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Abilities")
    TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

    /** Keeps the GAS SourceObject bridge alive for this granted ability. */
    UPROPERTY(Transient)
    TObjectPtr<UGuInstanceObject> TestGuInstanceObject;

    void MoveInput(const FInputActionValue& Value);
    void LookInput(const FInputActionValue& Value);

    UFUNCTION(BlueprintCallable, Category="Input")
    virtual void DoAim(float Yaw, float Pitch);

    UFUNCTION(BlueprintCallable, Category="Input")
    virtual void DoMove(float Right, float Forward);

    UFUNCTION(BlueprintCallable, Category="Input")
    virtual void DoJumpStart();

    UFUNCTION(BlueprintCallable, Category="Input")
    virtual void DoJumpEnd();

    virtual void SetupPlayerInputComponent(UInputComponent* InputComponent) override;

public:
    USkeletalMeshComponent* GetFirstPersonMesh() const { return FirstPersonMesh; }
    UCameraComponent* GetFirstPersonCameraComponent() const { return FirstPersonCameraComponent; }
};
