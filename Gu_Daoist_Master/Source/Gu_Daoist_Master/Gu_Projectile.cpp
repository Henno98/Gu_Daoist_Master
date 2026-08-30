// Fill out your copyright notice in the Description page of Project Settings.


#include "Gu_Projectile.h"

#include "UGuDefinition.h"
#include "Components/SphereComponent.h"
#include "UGuDefinition.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "GameplayEffect.h"
#include "AbilitySystemComponent.h"
#include "GuExecutionLibrary.h"
#include "Components/SphereComponent.h"

#include "GameFramework/ProjectileMovementComponent.h"

// Sets default values
AGu_Projectile::AGu_Projectile()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	Collision = CreateDefaultSubobject<USphereComponent>(
		TEXT("Collision")
	);

	SetRootComponent(Collision);

	ProjectileMovement =
		CreateDefaultSubobject<UProjectileMovementComponent>(
			TEXT("ProjectileMovement")
		);

	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Collision->SetCollisionObjectType(ECC_WorldDynamic);

	Collision->SetCollisionResponseToAllChannels(ECR_Ignore);
	Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	Collision->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);

	Collision->SetGenerateOverlapEvents(false);

	Collision->OnComponentHit.AddDynamic(
		this,
		&AGu_Projectile::OnProjectileHit
	);

	ProjectileMovement->UpdatedComponent = Collision;
	ProjectileMovement->bSweepCollision = true;

	ProjectileMovement->InitialSpeed = 1500.0f;
	ProjectileMovement->MaxSpeed = 1500.0f;
	ProjectileMovement->bRotationFollowsVelocity = true;

	// We want impact, not Unreal pinball.
	ProjectileMovement->bShouldBounce = false;

	ProjectileMovement->ProjectileGravityScale = 0.0f;
}

void AGu_Projectile::InitializeProjectile(const FGuProjectileMechanic& ProjectileData, UGuDefinition* InGuDefinition,
	UAbilitySystemComponent* InSourceASC)
{
	if (!InGuDefinition)
	{
		UE_LOG(LogTemp, Error,
			TEXT("InitializeProjectile: InGuDefinition is null"));

		Destroy();
		return;
	}

	if (!InSourceASC)
	{
		UE_LOG(LogTemp, Error,
			TEXT("InitializeProjectile: InSourceASC is null"));

		Destroy();
		return;
	}

	GuDefinition = InGuDefinition;
	SourceASC = InSourceASC;

	MaxRange = ProjectileData.MaxRange;
	SpawnLocation = GetActorLocation();

	// Enforce runtime collision settings.
	// This prevents Blueprint defaults from overriding our native setup.
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Collision->SetCollisionObjectType(ECC_WorldDynamic);

	Collision->SetCollisionResponseToAllChannels(ECR_Ignore);
	Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	Collision->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);

	Collision->SetGenerateOverlapEvents(false);

	Collision->SetSphereRadius(
		ProjectileData.Radius
	);

	ProjectileMovement->UpdatedComponent = Collision;

	ProjectileMovement->InitialSpeed =
		ProjectileData.Speed;

	ProjectileMovement->MaxSpeed =
		ProjectileData.Speed;

	ProjectileMovement->Velocity =
		GetActorForwardVector() * ProjectileData.Speed;

	ProjectileMovement->ProjectileGravityScale =
		ProjectileData.GravityScale;

	ProjectileMovement->bSweepCollision = true;
	ProjectileMovement->bShouldBounce = false;

	if (ProjectileData.Speed > 0.0f &&
		ProjectileData.MaxRange > 0.0f)
	{
		const float Lifetime =
			ProjectileData.MaxRange /
			ProjectileData.Speed;

		SetLifeSpan(Lifetime);
	}

	UE_LOG(
		LogTemp,
		Warning,
		TEXT(
			"Projectile initialized — Speed: %.1f, Range: %.1f, "
			"Lifetime: %.2f, PawnResponse: %d"
		),
		ProjectileData.Speed,
		ProjectileData.MaxRange,
		GetLifeSpan(),
		static_cast<int32>(
			Collision->GetCollisionResponseToChannel(ECC_Pawn)
			)
	);
}



void AGu_Projectile::OnProjectileHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (!OtherActor)
	{
		Destroy();
		return;
	}

	if (OtherActor == this || OtherActor == GetOwner())
	{
		return;
	}

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("Projectile hit fired — Actor: %s, Component: %s"),
		*GetNameSafe(OtherActor),
		*GetNameSafe(OtherComp)
	);

	if (!GuDefinition)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("%s has no GuDefinition"),
			*GetName()
		);

		Destroy();
		return;
	}

	if (!SourceASC)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("%s has no SourceASC"),
			*GetName()
		);

		Destroy();
		return;
	}

	UGuExecutionLibrary::ExecuteImpact(
		GuDefinition,
		SourceASC,
		OtherActor
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

