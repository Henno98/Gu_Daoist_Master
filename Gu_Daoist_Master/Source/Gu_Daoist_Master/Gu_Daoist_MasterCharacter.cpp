// Copyright Epic Games, Inc. All Rights Reserved.

#include "Gu_Daoist_MasterCharacter.h"

#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "Engine/GameInstance.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerState.h"
#include "GameplayAbilitySpec.h"
#include "GuDefinitionRegistrySubsystem.h"
#include "GuEntitySubsystem.h"
#include "GuInstanceObject.h"
#include "GuPlayerState.h"
#include "Gu_Daoist_Master.h"
#include "InputActionValue.h"
#include "UGuDefinition.h"

void AGu_Daoist_MasterCharacter::BeginPlay()
{
    Super::BeginPlay();

    if (AbilitySystemComponent)
    {
        AbilitySystemComponent->InitAbilityActorInfo(this, this);
    }

    if (HasAuthority() && InitialAttributesEffect && AbilitySystemComponent)
    {
        FGameplayEffectContextHandle EffectContext = AbilitySystemComponent->MakeEffectContext();
        EffectContext.AddSourceObject(this);
        FGameplayEffectSpecHandle SpecHandle = AbilitySystemComponent->MakeOutgoingSpec(
            InitialAttributesEffect,
            1.0f,
            EffectContext);

        if (SpecHandle.IsValid())
        {
            AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
        }
    }

    if (AttributeSet)
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("Primeval Essence: %.1f / %.1f"),
            AttributeSet->GetPrimevalEssence(),
            AttributeSet->GetMaxPrimevalEssence());
    }
}

AGu_Daoist_MasterCharacter::AGu_Daoist_MasterCharacter()
{
    GetCapsuleComponent()->InitCapsuleSize(55.f, 96.0f);

    FirstPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("First Person Mesh"));
    FirstPersonMesh->SetupAttachment(GetMesh());
    FirstPersonMesh->SetOnlyOwnerSee(true);
    FirstPersonMesh->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;
    FirstPersonMesh->SetCollisionProfileName(FName("NoCollision"));

    FirstPersonCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("First Person Camera"));
    FirstPersonCameraComponent->SetupAttachment(FirstPersonMesh, FName("head"));
    FirstPersonCameraComponent->SetRelativeLocationAndRotation(
        FVector(-2.8f, 5.89f, 0.0f),
        FRotator(0.0f, 90.0f, -90.0f));
    FirstPersonCameraComponent->bUsePawnControlRotation = true;
    FirstPersonCameraComponent->bEnableFirstPersonFieldOfView = true;
    FirstPersonCameraComponent->bEnableFirstPersonScale = true;
    FirstPersonCameraComponent->FirstPersonFieldOfView = 70.0f;
    FirstPersonCameraComponent->FirstPersonScale = 0.6f;

    GetMesh()->SetOwnerNoSee(true);
    GetMesh()->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::WorldSpaceRepresentation;
    GetCapsuleComponent()->SetCapsuleSize(34.0f, 96.0f);

    GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;
    GetCharacterMovement()->AirControl = 0.5f;

    AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
    AttributeSet = CreateDefaultSubobject<UAS_GuMasterAttributeSet>(TEXT("AttributeSet"));
}

UAbilitySystemComponent* AGu_Daoist_MasterCharacter::GetAbilitySystemComponent() const
{
    return AbilitySystemComponent;
}

void AGu_Daoist_MasterCharacter::ActivateTestGu(const FInputActionValue& Value)
{
    if (!AbilitySystemComponent || !TestGuAbilityHandle.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("Test Gu ability handle is invalid"));
        return;
    }

    const bool bActivated = AbilitySystemComponent->TryActivateAbility(TestGuAbilityHandle);
    UE_LOG(LogTemp, Warning, TEXT("TryActivateAbility returned: %s"), bActivated ? TEXT("true") : TEXT("false"));
}

void AGu_Daoist_MasterCharacter::PossessedBy(AController* NewController)
{
    Super::PossessedBy(NewController);

    if (AbilitySystemComponent)
    {
        AbilitySystemComponent->InitAbilityActorInfo(this, this);
    }

    if (!HasAuthority() || !AbilitySystemComponent || !GuAbilityClass || !TestGuDefinition)
    {
        return;
    }

    UObject* AbilitySourceObject = TestGuDefinition.Get();
    UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
    UGuDefinitionRegistrySubsystem* Registry = GameInstance
        ? GameInstance->GetSubsystem<UGuDefinitionRegistrySubsystem>()
        : nullptr;
    UGuEntitySubsystem* Entities = GameInstance
        ? GameInstance->GetSubsystem<UGuEntitySubsystem>()
        : nullptr;

    if (Registry && Entities)
    {
        FString RegisterError;
        if (Registry->RegisterDefinitionAsset(TestGuDefinition, RegisterError, true))
        {
            const FName DefinitionId = UGuDefinitionRegistrySubsystem::DefinitionIdForAsset(TestGuDefinition);
            AGuPlayerState* DomainPlayerState = GetPlayerState<AGuPlayerState>();
            const APlayerState* FallbackPlayerState = NewController ? NewController->GetPlayerState<APlayerState>() : nullptr;
            const FString FallbackOwnerId = FString::Printf(TEXT("player:%d"), FallbackPlayerState ? FallbackPlayerState->GetPlayerId() : 0);
            if (DomainPlayerState && DomainPlayerState->DomainCharacterId.IsEmpty())
            {
                DomainPlayerState->SetDomainCharacterId(FallbackOwnerId);
            }
            const FString OwnerId = DomainPlayerState && !DomainPlayerState->DomainCharacterId.IsEmpty()
                ? DomainPlayerState->DomainCharacterId
                : FallbackOwnerId;

            if (!Entities->FindOwnedGuInstance(
                    DefinitionId,
                    OwnerId,
                    EGuContainer::Aperture,
                    TestGuEntityId,
                    true))
            {
                TestGuEntityId = Entities->CreateGuInstance(DefinitionId, OwnerId, EGuContainer::Aperture);
            }

            if (TestGuEntityId.IsValid())
            {
                TestGuInstanceObject = NewObject<UGuInstanceObject>(this);
                TestGuInstanceObject->Initialize(TestGuEntityId, TestGuDefinition);
                AbilitySourceObject = TestGuInstanceObject;

                UE_LOG(
                    LogTemp,
                    Log,
                    TEXT("ECS Gu bound: %s -> %s"),
                    *TestGuDefinition->Name.ToString(),
                    *TestGuEntityId.ToString());
            }
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("Could not register %s with Gu ECS: %s"), *GetNameSafe(TestGuDefinition), *RegisterError);
        }
    }

    FGameplayAbilitySpec AbilitySpec(GuAbilityClass, 1, INDEX_NONE, AbilitySourceObject);
    TestGuAbilityHandle = AbilitySystemComponent->GiveAbility(AbilitySpec);

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("Gu setup - Authority: %s, Ability: %s, Definition: %s, Entity: %s"),
        HasAuthority() ? TEXT("true") : TEXT("false"),
        *GetNameSafe(GuAbilityClass),
        *GetNameSafe(TestGuDefinition),
        TestGuEntityId.IsValid() ? *TestGuEntityId.ToString() : TEXT("legacy/no ECS entity"));
}

void AGu_Daoist_MasterCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
    {
        EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &AGu_Daoist_MasterCharacter::DoJumpStart);
        EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &AGu_Daoist_MasterCharacter::DoJumpEnd);
        EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AGu_Daoist_MasterCharacter::MoveInput);
        EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AGu_Daoist_MasterCharacter::LookInput);
        EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &AGu_Daoist_MasterCharacter::LookInput);

        if (ActivateGuAction)
        {
            EnhancedInputComponent->BindAction(
                ActivateGuAction,
                ETriggerEvent::Started,
                this,
                &AGu_Daoist_MasterCharacter::ActivateTestGu);
        }
    }
    else
    {
        UE_LOG(
            LogGu_Daoist_Master,
            Error,
            TEXT("'%s' Failed to find an Enhanced Input Component!"),
            *GetNameSafe(this));
    }
}

void AGu_Daoist_MasterCharacter::MoveInput(const FInputActionValue& Value)
{
    const FVector2D MovementVector = Value.Get<FVector2D>();
    DoMove(MovementVector.X, MovementVector.Y);
}

void AGu_Daoist_MasterCharacter::LookInput(const FInputActionValue& Value)
{
    const FVector2D LookAxisVector = Value.Get<FVector2D>();
    DoAim(LookAxisVector.X, LookAxisVector.Y);
}

void AGu_Daoist_MasterCharacter::DoAim(float Yaw, float Pitch)
{
    if (GetController())
    {
        AddControllerYawInput(Yaw);
        AddControllerPitchInput(Pitch);
    }
}

void AGu_Daoist_MasterCharacter::DoMove(float Right, float Forward)
{
    if (GetController())
    {
        AddMovementInput(GetActorRightVector(), Right);
        AddMovementInput(GetActorForwardVector(), Forward);
    }
}

void AGu_Daoist_MasterCharacter::DoJumpStart()
{
    Jump();
}

void AGu_Daoist_MasterCharacter::DoJumpEnd()
{
    StopJumping();
}
