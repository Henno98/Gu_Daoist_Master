// Fill out your copyright notice in the Description page of Project Settings.


#include "Gu_Projectile.h"

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

	ProjectileMovement->InitialSpeed = 1500.0f;
	ProjectileMovement->MaxSpeed = 1500.0f;
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->bShouldBounce = false;
}

// Called when the game starts or when spawned
void AGu_Projectile::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AGu_Projectile::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

