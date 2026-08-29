// Fill out your copyright notice in the Description page of Project Settings.


#include "Gu_Projectile.h"

#include "UGuDefinition.h"
#include "Components/SphereComponent.h"
#include "UGuDefinition.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "GameplayEffect.h"
#include "AbilitySystemComponent.h"
#include "Components/SphereComponent.h"

#include "GameFramework/ProjectileMovementComponent.h"

// Sets default values
AGu_Projectile::AGu_Projectile()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;


	Collision = CreateDefaultSubobject<USphereComponent>(
		TEXT("Collision")
	);

	SetRootComponent(Collision);

	ProjectileMovement =
		CreateDefaultSubobject<UProjectileMovementComponent>(
			TEXT("ProjectileMovement")
		);

	Collision->SetCollisionEnabled(
		ECollisionEnabled::QueryOnly
	);

	Collision->SetCollisionResponseToAllChannels(
		ECR_Ignore
	);

	Collision->SetCollisionResponseToChannel(
		ECC_Pawn,
		ECR_Overlap
	);

	Collision->OnComponentBeginOverlap.AddDynamic(
		this,
		&AGu_Projectile::OnProjectileOverlap
	);

	ProjectileMovement->InitialSpeed = 1500.0f;
	ProjectileMovement->MaxSpeed = 1500.0f;
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->bShouldBounce = true;
}

void AGu_Projectile::InitializeProjectile(const FGuProjectileMechanic& ProjectileData, UGuDefinition* InGuDefinition,
	UAbilitySystemComponent* InSourceASC)
{
	GuDefinition = InGuDefinition;
	SourceASC = InSourceASC;

	MaxRange = ProjectileData.MaxRange;
	SpawnLocation = GetActorLocation();

	Collision->SetSphereRadius(
		ProjectileData.Radius
	);

	ProjectileMovement->InitialSpeed =
		ProjectileData.Speed;

	ProjectileMovement->MaxSpeed =
		ProjectileData.Speed;

	ProjectileMovement->Velocity =
		GetActorForwardVector() * ProjectileData.Speed;

	if (ProjectileData.Speed > 0.0f &&
		ProjectileData.MaxRange > 0.0f)
	{
		const float Lifetime =
			ProjectileData.MaxRange / ProjectileData.Speed;

		SetLifeSpan(15);
	}

}

void AGu_Projectile::OnProjectileOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{

		if (!OtherActor ||
			OtherActor == GetOwner() ||
			!GuDefinition ||
			!SourceASC)
		{
			return;
		}

		UAbilitySystemComponent* TargetASC =
			UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(
				OtherActor
			);

		if (!TargetASC)
		{
			return;
		}

		const FGuDamageMechanic* DamageMechanic = nullptr;

		for (const TInstancedStruct<FGuMechanic>& Mechanic :
			GuDefinition->Mechanics)
		{
			if (const FGuDamageMechanic* DamageData =
				Mechanic.GetPtr<FGuDamageMechanic>())
			{
				DamageMechanic = DamageData;
				break;
			}
		}

		if (!DamageMechanic || !DamageEffect)
		{
			return;
		}

		FGameplayEffectContextHandle EffectContext =
			SourceASC->MakeEffectContext();

		EffectContext.AddSourceObject(this);

		FGameplayEffectSpecHandle DamageSpec =
			SourceASC->MakeOutgoingSpec(
				DamageEffect,
				1.0f,
				EffectContext
			);

		if (!DamageSpec.IsValid())
		{
			return;
		}

		const FGameplayTag DamageTag =
			FGameplayTag::RequestGameplayTag(
				FName("Data.Gu.Damage")
			);

		DamageSpec.Data->SetSetByCallerMagnitude(
			DamageTag,
			-DamageMechanic->Damage
		);

		SourceASC->ApplyGameplayEffectSpecToTarget(
			*DamageSpec.Data.Get(),
			TargetASC
		);

		Destroy();
	
}

// Called when the game starts or when spawned
void AGu_Projectile::BeginPlay()
{
	Super::BeginPlay();
	UE_LOG(
		LogTemp,
		Warning,
		TEXT("Projectile BeginPlay — Location: %s, Velocity: %s, Lifespan: %.2f"),
		*GetActorLocation().ToString(),
		*ProjectileMovement->Velocity.ToString(),
		GetLifeSpan()
	);
}

// Called every frame
void AGu_Projectile::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

