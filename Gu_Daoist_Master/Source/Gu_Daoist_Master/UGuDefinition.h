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
class AActor;
class UMaterialInterface;

USTRUCT(BlueprintType)
struct FGuMechanic
{
    GENERATED_BODY()
};

/** Where a payload mechanic applies when used directly or through a carrier. */
UENUM(BlueprintType)
enum class EGuMechanicRecipient : uint8
{
    Self,
    ImpactTarget
};

UENUM(BlueprintType)
enum class EGuMovementMode : uint8
{
    SpeedMultiplier,
    Dash,
    Blink
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

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Collision",
        meta = (EditCondition = "CollisionType == EGuProjectileCollisionType::Sphere", EditConditionHides))
    float SphereRadius = 10.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Collision",
        meta = (EditCondition = "CollisionType == EGuProjectileCollisionType::Box", EditConditionHides))
    FVector BoxExtent = FVector(10.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Collision",
        meta = (EditCondition = "CollisionType == EGuProjectileCollisionType::Capsule", EditConditionHides))
    float CapsuleRadius = 5.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Collision",
        meta = (EditCondition = "CollisionType == EGuProjectileCollisionType::Capsule", EditConditionHides))
    float CapsuleHalfHeight = 20.0f;
};

/** Close-range delivery carrier. Impact payloads on the same Gu are applied to actors inside the sweep. */
USTRUCT(BlueprintType)
struct FGuMeleeMechanic : public FGuMechanic
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee", meta = (ClampMin = "1.0"))
    float Range = 150.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee", meta = (ClampMin = "1.0"))
    float Radius = 45.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee", meta = (ClampMin = "1.0", ClampMax = "360.0"))
    float ArcDegrees = 100.0f;

    /** 0 means unlimited targets inside the sweep. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee", meta = (ClampMin = "0"))
    int32 MaxTargets = 1;
};

/** Instant radial delivery carrier. Payload mechanics on the Gu are applied to actors inside Radius. */
USTRUCT(BlueprintType)
struct FGuAreaMechanic : public FGuMechanic
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Area", meta = (ClampMin = "1.0"))
    float Radius = 250.0f;

    /** Moves the centre forward from the user. Zero centres the area on the user. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Area")
    float ForwardOffset = 0.0f;

    /** 0 means unlimited targets. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Area", meta = (ClampMin = "0"))
    int32 MaxTargets = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Area")
    bool bIncludeSelf = false;
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

/** Restores Health either to the user or to the actor reached through a carrier. */
USTRUCT(BlueprintType)
struct FGuHealMechanic : public FGuMechanic
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Healing", meta = (ClampMin = "0.0"))
    float Amount = 20.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Healing")
    EGuMechanicRecipient Recipient = EGuMechanicRecipient::Self;
};

/** Adds an independent shield layer. Duration 0 means the layer persists until depleted. */
USTRUCT(BlueprintType)
struct FGuShieldMechanic : public FGuMechanic
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shield", meta = (ClampMin = "0.0"))
    float Amount = 20.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shield", meta = (ClampMin = "0.0"))
    float Duration = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shield")
    EGuMechanicRecipient Recipient = EGuMechanicRecipient::Self;
};

/** Speed enhancement, dash impulse, or spatial blink. */
USTRUCT(BlueprintType)
struct FGuMovementMechanic : public FGuMechanic
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
    EGuMovementMode Mode = EGuMovementMode::SpeedMultiplier;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
    EGuMechanicRecipient Recipient = EGuMechanicRecipient::Self;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement",
        meta = (ClampMin = "0.05", EditCondition = "Mode == EGuMovementMode::SpeedMultiplier", EditConditionHides))
    float SpeedMultiplier = 1.4f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement",
        meta = (ClampMin = "0.0", EditCondition = "Mode == EGuMovementMode::SpeedMultiplier", EditConditionHides))
    float Duration = 5.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement",
        meta = (ClampMin = "0.0", EditCondition = "Mode == EGuMovementMode::Dash", EditConditionHides))
    float DashSpeed = 1200.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement",
        meta = (EditCondition = "Mode == EGuMovementMode::Dash", EditConditionHides))
    float VerticalSpeed = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement",
        meta = (ClampMin = "0.0", EditCondition = "Mode == EGuMovementMode::Blink", EditConditionHides))
    float BlinkDistance = 600.0f;
};

/** Slows or roots a delivered target for a duration. 0 multiplier is a full root. */
USTRUCT(BlueprintType)
struct FGuRestrictionMechanic : public FGuMechanic
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Restriction", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MovementMultiplier = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Restriction", meta = (ClampMin = "0.0"))
    float Duration = 3.0f;
};

/** Runtime concealment state. AI/targeting can query UGuRuntimeEffectComponent for resistance/opacity. */
USTRUCT(BlueprintType)
struct FGuConcealmentMechanic : public FGuMechanic
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Concealment", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float Opacity = 0.12f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Concealment", meta = (ClampMin = "0.0", ClampMax = "0.95"))
    float DetectionResistance = 0.35f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Concealment", meta = (ClampMin = "0.0"))
    float Duration = 8.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Concealment")
    bool bBreakOnAttack = true;
};

/** Investigation state carried by the user for a duration. */
USTRUCT(BlueprintType)
struct FGuRevealMechanic : public FGuMechanic
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Investigation", meta = (ClampMin = "1.0"))
    float Range = 1200.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Investigation", meta = (ClampMin = "0.0"))
    float Duration = 3.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Investigation", meta = (ClampMin = "0.0"))
    float Strength = 1.0f;
};


USTRUCT(BlueprintType)
struct FGuDamageOverTimeMechanic : public FGuMechanic
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage Over Time", meta = (ClampMin = "0.0"))
    float DamagePerTick = 5.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage Over Time", meta = (ClampMin = "0.02"))
    float TickInterval = 0.5f;

    /** Zero means the periodic effect persists until cleansed/removed. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage Over Time", meta = (ClampMin = "0.0"))
    float Duration = 4.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage Over Time")
    EGuMechanicRecipient Recipient = EGuMechanicRecipient::ImpactTarget;
};

USTRUCT(BlueprintType)
struct FGuHealOverTimeMechanic : public FGuMechanic
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Healing Over Time", meta = (ClampMin = "0.0"))
    float HealPerTick = 5.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Healing Over Time", meta = (ClampMin = "0.02"))
    float TickInterval = 0.5f;

    /** Zero means the periodic effect persists until dispelled/removed. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Healing Over Time", meta = (ClampMin = "0.0"))
    float Duration = 4.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Healing Over Time")
    EGuMechanicRecipient Recipient = EGuMechanicRecipient::Self;
};

UENUM(BlueprintType)
enum class EGuEssenceChangeMode : uint8
{
    Restore,
    Drain
};

/** Immediate primeval-essence restoration/drain. Drain can transfer what was removed to the source. */
USTRUCT(BlueprintType)
struct FGuEssenceChangeMechanic : public FGuMechanic
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Essence")
    EGuEssenceChangeMode Mode = EGuEssenceChangeMode::Restore;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Essence", meta = (ClampMin = "0.0"))
    float Amount = 10.0f;

    /** If true, Amount is interpreted as a percentage of the recipient's maximum essence. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Essence")
    bool bPercentOfMaximum = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Essence")
    EGuMechanicRecipient Recipient = EGuMechanicRecipient::Self;

    /** Only meaningful for Drain on an impact target. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Essence")
    bool bTransferDrainedEssenceToSource = false;
};

/** Continuous primeval-essence production. Duration 0 persists until explicitly removed. */
USTRUCT(BlueprintType)
struct FGuEssenceRegenerationMechanic : public FGuMechanic
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Essence Regeneration", meta = (ClampMin = "0.0"))
    float FlatPerSecond = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Essence Regeneration", meta = (ClampMin = "0.0", ClampMax = "100.0"))
    float PercentOfMaximumPerSecond = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Essence Regeneration", meta = (ClampMin = "0.0"))
    float Duration = 10.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Essence Regeneration")
    EGuMechanicRecipient Recipient = EGuMechanicRecipient::Self;
};

UENUM(BlueprintType)
enum class EGuDisplacementMode : uint8
{
    AwayFromSource,
    TowardSource,
    Upward,
    Downward
};

/** General push/pull/launch control. Knockback remains as the simple away-from-source variant. */
USTRUCT(BlueprintType)
struct FGuDisplacementMechanic : public FGuMechanic
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Displacement")
    EGuDisplacementMode Mode = EGuDisplacementMode::TowardSource;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Displacement", meta = (ClampMin = "0.0"))
    float Strength = 600.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Displacement")
    float VerticalStrength = 0.0f;
};

/** Prevents the target from activating Gu while the suppression remains. */
USTRUCT(BlueprintType)
struct FGuGuSuppressionMechanic : public FGuMechanic
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gu Suppression", meta = (ClampMin = "0.0"))
    float Duration = 2.0f;
};

/** Removes hostile transient Gu states from self or an impact target. */
USTRUCT(BlueprintType)
struct FGuCleanseMechanic : public FGuMechanic
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cleanse")
    EGuMechanicRecipient Recipient = EGuMechanicRecipient::Self;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cleanse") bool bRemoveDamageOverTime = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cleanse") bool bRemoveRestrictions = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cleanse") bool bRemoveGuSuppression = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cleanse") bool bRemoveMarks = true;
};

/** Removes beneficial transient Gu states from self or an impact target. */
USTRUCT(BlueprintType)
struct FGuDispelMechanic : public FGuMechanic
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dispel")
    EGuMechanicRecipient Recipient = EGuMechanicRecipient::ImpactTarget;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dispel") bool bRemoveShields = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dispel") bool bRemoveMovementBuffs = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dispel") bool bRemoveHealingOverTime = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dispel") bool bRemoveEssenceRegeneration = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dispel") bool bRemoveConcealment = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dispel") bool bRemoveReveal = true;
};

/** Persistent area carrier. Other impact mechanics on this Gu pulse through the field. */
USTRUCT(BlueprintType)
struct FGuFieldMechanic : public FGuMechanic
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Field", meta = (ClampMin = "1.0"))
    float Radius = 300.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Field")
    float ForwardOffset = 150.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Field", meta = (ClampMin = "0.02"))
    float TickInterval = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Field", meta = (ClampMin = "0.02"))
    float Duration = 5.0f;

    /** 0 means unlimited targets per pulse. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Field", meta = (ClampMin = "0"))
    int32 MaxTargetsPerPulse = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Field")
    bool bIncludeSelf = false;
};

/** On impact, repeats the payload onto nearby valid targets. */
USTRUCT(BlueprintType)
struct FGuChainMechanic : public FGuMechanic
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chain", meta = (ClampMin = "1.0"))
    float JumpRadius = 450.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chain", meta = (ClampMin = "1", ClampMax = "32"))
    int32 MaxAdditionalTargets = 3;

    /** Multiplies magnitude after each jump. 1 means no falloff. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chain", meta = (ClampMin = "0.0", ClampMax = "2.0"))
    float MagnitudeFalloff = 0.8f;
};

/** Attaches a named target link that future Gu/killer moves can query. */
USTRUCT(BlueprintType)
struct FGuMarkMechanic : public FGuMechanic
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mark")
    FName MarkId = TEXT("GuMark");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mark", meta = (ClampMin = "0.0"))
    float Strength = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mark", meta = (ClampMin = "0.0"))
    float Duration = 8.0f;
};

/** Temporarily increases simultaneous Gu-control capacity. */
USTRUCT(BlueprintType)
struct FGuAttentionBoostMechanic : public FGuMechanic
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mental", meta = (ClampMin = "1", ClampMax = "50"))
    int32 SlotsGranted = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mental", meta = (ClampMin = "0.1"))
    float Duration = 10.0f;
};

/** Authoring-side refinement assistance. Registry normalization converts this into the shared ECS assistant component. */
USTRUCT(BlueprintType)
struct FGuRefinementAssistMechanic : public FGuMechanic
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Refinement Assistance", meta = (ClampMin = "0.0"))
    float ProgressPercent = 10.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Refinement Assistance")
    float StabilityPerAction = 2.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Refinement Assistance", meta = (ClampMin = "0.0"))
    float ImpurityReductionPerAction = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Refinement Assistance")
    float QualityBonus = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Refinement Assistance", meta = (ClampMin = "1"))
    int32 ActionUses = 3;

    /** Valid values currently include Process, Heat, Cool, Merge, Purify, Control and Condense. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Refinement Assistance")
    TArray<FName> Processes;
};

/** Generic world manifestation for puppet, beast, construct, trap-proxy, clone, etc. ECS beast binding comes later. */
USTRUCT(BlueprintType)
struct FGuSummonMechanic : public FGuMechanic
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Summon")
    TSubclassOf<AActor> ActorClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Summon", meta = (ClampMin = "1", ClampMax = "64"))
    int32 Count = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Summon", meta = (ClampMin = "0.0"))
    float SpawnRadius = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Summon")
    float ForwardOffset = 150.0f;

    /** Zero leaves lifespan to the spawned actor. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Summon", meta = (ClampMin = "0.0"))
    float Lifetime = 0.0f;
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
 * Existing GAS mechanics remain here. Domain/refinement fields are immutable
 * species data copied into physical ECS Gu instances when they are created.
 */
UCLASS(BlueprintType)
class GU_DAOIST_MASTER_API UGuDefinition : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    virtual void PostLoad() override;

    UFUNCTION(CallInEditor, Category = "Gu|Refinement")
    void RebuildRefinementSemantics();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gu")
    FText Name;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gu")
    int32 Rank = 1;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gu|Path",
        meta = (Categories = "Data.Paths"))
    FGameplayTag PrimaryPath;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gu|Path",
        meta = (Categories = "Data.Paths"))
    FGameplayTagContainer SecondaryPaths;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gu")
    FGameplayTagContainer Tags;

    /** Composable effect/carrier vocabulary. Add any FGuMechanic-derived struct here. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gu|Mechanics", meta = (ExcludeBaseStruct))
    TArray<TInstancedStruct<FGuMechanic>> Mechanics;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gu")
    TSubclassOf<UGameplayEffect> PrimevalEssenceCostEffect;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gu|Domain")
    FName StableDefinitionId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gu|Domain")
    EGuActivationModel ActivationModel = EGuActivationModel::Instant;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gu|Domain|Lifecycle")
    FGuLifecycleSpec Lifecycle;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gu|Domain|Feeding")
    FGuFeedingSpec Feeding;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gu|Refinement")
    TArray<FName> RefinementTraits;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gu|Refinement")
    FRefinementSemanticProfile RefinementProfile;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gu|Refinement")
    bool bRefinementSemanticsMaterialized = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gu|Refinement")
    FGuRefinementAssistanceSpec RefinementAssistance;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gu|Appearance")
    FGuAppearanceSpec Appearance;
};
