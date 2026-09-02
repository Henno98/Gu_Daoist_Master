// Fill out your copyright notice in the Description page of Project Settings.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "AttributeSet.h"
#include "GameplayEffectTypes.h"
#include "StructUtils/InstancedStruct.h"
#include "GuDefinitionTypes.h"
#include "UGuDefinition.generated.h"

class AGu_Projectile;
class UGameplayEffect;
class UStaticMesh;
class UMaterialInterface;

USTRUCT(BlueprintType)
struct FGuMechanic
{
    GENERATED_BODY()
};

USTRUCT(BlueprintType)
struct FGuEssenceCostMechanic : public FGuMechanic
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gu")
    float Cost = 0.0f;
};

USTRUCT(BlueprintType)
struct FGuKnockbackMechanic : public FGuMechanic
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Knockback")
    float Strength = 500.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Knockback")
    float VerticalStrength = 0.0f;
};

UENUM(BlueprintType)
enum class EGuProjectileCollisionType : uint8
{
    Sphere,
    Box,
    Capsule,
    Mesh
};

USTRUCT(BlueprintType)
struct FGuProjectileMechanic : public FGuMechanic
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TSubclassOf<AGu_Projectile> ProjectileClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile")
    float Speed = 1500.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile")
    float MaxRange = 1000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile")
    float Radius = 10.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile")
    float GravityScale = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Visual")
    TObjectPtr<UStaticMesh> Mesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Visual")
    TObjectPtr<UMaterialInterface> Material = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Visual")
    FVector MeshScale = FVector(1.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Collision")
    EGuProjectileCollisionType CollisionType = EGuProjectileCollisionType::Sphere;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Projectile|Collision",
        meta = (EditCondition = "CollisionType == EGuProjectileCollisionType::Sphere", EditConditionHides))
    float SphereRadius = 10.0f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Projectile|Collision",
        meta = (EditCondition = "CollisionType == EGuProjectileCollisionType::Box", EditConditionHides))
    FVector BoxExtent = FVector(10.0f);

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Projectile|Collision",
        meta = (EditCondition = "CollisionType == EGuProjectileCollisionType::Capsule", EditConditionHides))
    float CapsuleRadius = 5.0f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Projectile|Collision",
        meta = (EditCondition = "CollisionType == EGuProjectileCollisionType::Capsule", EditConditionHides))
    float CapsuleHalfHeight = 20.0f;
};

USTRUCT(BlueprintType)
struct FGuDamageMechanic : public FGuMechanic
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
    float Damage = 10.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
    TSubclassOf<UGameplayEffect> EffectClass;
};

USTRUCT(BlueprintType)
struct FGuBuffMechanic : public FGuMechanic
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Buff")
    FGameplayAttribute Attribute;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Buff")
    TEnumAsByte<EGameplayModOp::Type> Operation = EGameplayModOp::Additive;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Buff")
    float Magnitude = 10.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Buff")
    float Duration = 5.0f;
};

/**
 * Authored Gu species definition.
 *
 * Existing GAS mechanics remain here. The added Domain/Refinement fields are
 * immutable species data copied into each physical ECS Gu instance when it is
 * created. Mutable condition, nourishment, charges, contamination and ownership
 * live on the ECS entity, never on this shared DataAsset.
 */
UCLASS(BlueprintType)
class GU_DAOIST_MASTER_API UGuDefinition : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    virtual void PostLoad() override;

    /**
     * Rebuild the shared ECS/refinement semantics from this Gu's Path and GAS
     * mechanics. Intended for legacy assets that predate the semantic fields.
     */
    UFUNCTION(CallInEditor, Category = "Gu|Refinement")
    void RebuildRefinementSemantics();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gu")
    FText Name;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gu")
    int32 Rank = 1;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gu")
    FGameplayTag Path;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gu")
    FGameplayTagContainer Tags;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gu|Mechanics")
    TArray<TInstancedStruct<FGuMechanic>> Mechanics;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gu")
    TSubclassOf<UGameplayEffect> PrimevalEssenceCostEffect;

    /** Optional stable override. If empty the DataAsset object name is used. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gu|Domain")
    FName StableDefinitionId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gu|Domain")
    EGuActivationModel ActivationModel = EGuActivationModel::Instant;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gu|Domain|Lifecycle")
    FGuLifecycleSpec Lifecycle;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gu|Domain|Feeding")
    FGuFeedingSpec Feeding;

    /** Extra semantic traits such as maintained, prepared, charged or contact. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gu|Refinement")
    TArray<FName> RefinementTraits;

    /**
     * Shared physical/refinement semantics. Leave empty to derive a reasonable
     * profile from Path + existing GAS mechanics; author it explicitly when the
     * Gu's canon effect needs more precise semantics.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gu|Refinement")
    FRefinementSemanticProfile RefinementProfile;

    /**
     * Migration marker. Legacy assets are upgraded once on load so their
     * effective semantic profile is visible directly on UGuDefinition rather
     * than existing only inside the runtime registry.
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gu|Refinement")
    bool bRefinementSemanticsMaterialized = false;

    /** Enables this physical Gu to contribute a limited-use refinement technique. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gu|Refinement")
    FGuRefinementAssistanceSpec RefinementAssistance;

    /** Asset-independent appearance metadata used by future ECS renderers/HUD. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gu|Appearance")
    FGuAppearanceSpec Appearance;
};
