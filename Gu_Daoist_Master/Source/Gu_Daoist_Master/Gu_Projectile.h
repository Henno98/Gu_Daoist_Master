// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Gu_Projectile.generated.h"

class UAbilitySystemComponent;
class UGuDefinition;
struct FGuProjectileMechanic;
class UProjectileMovementComponent;
class USphereComponent;
class UGameplayEffect;

UCLASS(Blueprintable)
class GU_DAOIST_MASTER_API AGu_Projectile : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AGu_Projectile();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TObjectPtr<USphereComponent> Collision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	UFUNCTION()
	void InitializeProjectile(
		const FGuProjectileMechanic& ProjectileData,
		UGuDefinition* InGuDefinition,
		UAbilitySystemComponent* InSourceASC
	);

	UFUNCTION()
	void OnProjectileOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult
	);

	UPROPERTY()
	TObjectPtr<UGuDefinition> GuDefinition;

	UPROPERTY()
	TObjectPtr<UAbilitySystemComponent> SourceASC;

	float MaxRange = 0.0f;
	FVector SpawnLocation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gu|Effects")
	TSubclassOf<UGameplayEffect> DamageEffect;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

};
