// Copyright Epic Games, Inc. All Rights Reserved.

#include "Gu_Daoist_MasterCharacter.h"
#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "InputActionValue.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AbilitySystemInterface.h"
#include "GameplayAbilitySpec.h"
#include "UGuDefinition.h"
#include "AbilitySystemComponent.h"
#include "Gu_Daoist_Master.h"

AGu_Daoist_MasterCharacter::AGu_Daoist_MasterCharacter()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(55.f, 96.0f);
	
	// Create the first person mesh that will be viewed only by this character's owner
	FirstPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("First Person Mesh"));

	FirstPersonMesh->SetupAttachment(GetMesh());
	FirstPersonMesh->SetOnlyOwnerSee(true);
	FirstPersonMesh->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;
	FirstPersonMesh->SetCollisionProfileName(FName("NoCollision"));

	// Create the Camera Component	
	FirstPersonCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("First Person Camera"));
	FirstPersonCameraComponent->SetupAttachment(FirstPersonMesh, FName("head"));
	FirstPersonCameraComponent->SetRelativeLocationAndRotation(FVector(-2.8f, 5.89f, 0.0f), FRotator(0.0f, 90.0f, -90.0f));
	FirstPersonCameraComponent->bUsePawnControlRotation = true;
	FirstPersonCameraComponent->bEnableFirstPersonFieldOfView = true;
	FirstPersonCameraComponent->bEnableFirstPersonScale = true;
	FirstPersonCameraComponent->FirstPersonFieldOfView = 70.0f;
	FirstPersonCameraComponent->FirstPersonScale = 0.6f;

	// configure the character comps
	GetMesh()->SetOwnerNoSee(true);
	GetMesh()->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::WorldSpaceRepresentation;

	GetCapsuleComponent()->SetCapsuleSize(34.0f, 96.0f);

	// Configure character movement
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;
	GetCharacterMovement()->AirControl = 0.5f;

	AbilitySystemComponent =
		CreateDefaultSubobject<UAbilitySystemComponent>(
			TEXT("AbilitySystemComponent")
		);

	AttributeSet =
		CreateDefaultSubobject<UAS_GuMasterAttributeSet>(
			TEXT("AttributeSet")
		);
}

UAbilitySystemComponent* AGu_Daoist_MasterCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void AGu_Daoist_MasterCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);
	}


	if (InitialAttributesEffect)
	{
		FGameplayEffectContextHandle EffectContext =
			AbilitySystemComponent->MakeEffectContext();

		EffectContext.AddSourceObject(this);

		FGameplayEffectSpecHandle SpecHandle =
			AbilitySystemComponent->MakeOutgoingSpec(
				InitialAttributesEffect,
				1.0f,
				EffectContext
			);

		if (SpecHandle.IsValid())
		{
			AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(
				*SpecHandle.Data.Get()
			);
		}
	}
	UE_LOG(
		LogTemp,
		Warning,
		TEXT("Primeval Essence: %.1f / %.1f"),
		AttributeSet->GetPrimevalEssence(),
		AttributeSet->GetMaxPrimevalEssence()
	);
	UE_LOG(
		LogTemp,
		Warning,
		TEXT("Gu setup — Authority: %s, Ability: %s, Definition: %s"),
		HasAuthority() ? TEXT("true") : TEXT("false"),
		*GetNameSafe(GuAbilityClass),
		*GetNameSafe(TestGuDefinition)
	);

	if (HasAuthority() && GuAbilityClass && TestGuDefinition)
	{
		FGameplayAbilitySpec AbilitySpec(
			GuAbilityClass,
			1,
			INDEX_NONE,
			TestGuDefinition
		);

		const FGameplayAbilitySpecHandle AbilityHandle =
			AbilitySystemComponent->GiveAbility(AbilitySpec);

		const bool bActivated =
			AbilitySystemComponent->TryActivateAbility(AbilityHandle);

		UE_LOG(
			LogTemp,
			Warning,
			TEXT("TryActivateAbility returned: %s"),
			bActivated ? TEXT("true") : TEXT("false")
		);
	}
}


void AGu_Daoist_MasterCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{	
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &AGu_Daoist_MasterCharacter::DoJumpStart);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &AGu_Daoist_MasterCharacter::DoJumpEnd);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AGu_Daoist_MasterCharacter::MoveInput);

		// Looking/Aiming
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AGu_Daoist_MasterCharacter::LookInput);
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &AGu_Daoist_MasterCharacter::LookInput);
	}
	else
	{
		UE_LOG(LogGu_Daoist_Master, Error, TEXT("'%s' Failed to find an Enhanced Input Component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}


void AGu_Daoist_MasterCharacter::MoveInput(const FInputActionValue& Value)
{
	// get the Vector2D move axis
	FVector2D MovementVector = Value.Get<FVector2D>();

	// pass the axis values to the move input
	DoMove(MovementVector.X, MovementVector.Y);

}

void AGu_Daoist_MasterCharacter::LookInput(const FInputActionValue& Value)
{
	// get the Vector2D look axis
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	// pass the axis values to the aim input
	DoAim(LookAxisVector.X, LookAxisVector.Y);

}

void AGu_Daoist_MasterCharacter::DoAim(float Yaw, float Pitch)
{
	if (GetController())
	{
		// pass the rotation inputs
		AddControllerYawInput(Yaw);
		AddControllerPitchInput(Pitch);
	}
}

void AGu_Daoist_MasterCharacter::DoMove(float Right, float Forward)
{
	if (GetController())
	{
		// pass the move inputs
		AddMovementInput(GetActorRightVector(), Right);
		AddMovementInput(GetActorForwardVector(), Forward);
	}
}

void AGu_Daoist_MasterCharacter::DoJumpStart()
{
	// pass Jump to the character
	Jump();
}

void AGu_Daoist_MasterCharacter::DoJumpEnd()
{
	// pass StopJumping to the character
	StopJumping();
}
