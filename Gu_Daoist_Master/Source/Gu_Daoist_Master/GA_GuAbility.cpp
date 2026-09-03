#include "GA_GuAbility.h"

#include "AS_GuMasterAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Pawn.h"
#include "GuExecutionLibrary.h"
#include "GuEntitySubsystem.h"
#include "GuInstanceObject.h"
#include "GuRuntimeEffectComponent.h"
#include "Gu_Projectile.h"
#include "UGuDefinition.h"

UGA_GuAbility::UGA_GuAbility()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

UGuInstanceObject* UGA_GuAbility::GetGuInstanceObject(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo) const
{
    if (!ActorInfo || !ActorInfo->AbilitySystemComponent.IsValid()) return nullptr;
    const FGameplayAbilitySpec* Spec = ActorInfo->AbilitySystemComponent->FindAbilitySpecFromHandle(Handle);
    return Spec ? Cast<UGuInstanceObject>(Spec->SourceObject.Get()) : nullptr;
}

UGuDefinition* UGA_GuAbility::GetGuDefinition(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo) const
{
    if (!ActorInfo || !ActorInfo->AbilitySystemComponent.IsValid()) return nullptr;
    const FGameplayAbilitySpec* Spec = ActorInfo->AbilitySystemComponent->FindAbilitySpecFromHandle(Handle);
    if (!Spec) return nullptr;

    // New path: one GameplayAbility spec is tied to one physical ECS Gu.
    if (UGuInstanceObject* Instance = Cast<UGuInstanceObject>(Spec->SourceObject.Get()))
    {
        return Instance->Definition;
    }

    // Legacy fallback keeps old Blueprint/spec setup functional during migration.
    return Cast<UGuDefinition>(Spec->SourceObject.Get());
}

void UGA_GuAbility::ActivateAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    UGuDefinition* GuDefinition = GetGuDefinition(Handle, ActorInfo);
    if (!GuDefinition)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    if (AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr)
    {
        if (const UGuRuntimeEffectComponent* Runtime = Avatar->FindComponentByClass<UGuRuntimeEffectComponent>(); Runtime && Runtime->IsGuSuppressed())
        {
            UE_LOG(LogTemp, Warning, TEXT("Cannot activate %s: Gu activation is suppressed."), *GuDefinition->Name.ToString());
            EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
            return;
        }
    }

    UGuInstanceObject* PhysicalGu = GetGuInstanceObject(Handle, ActorInfo);
    UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
    UGuEntitySubsystem* Entities = GameInstance
        ? GameInstance->GetSubsystem<UGuEntitySubsystem>()
        : nullptr;

    if (PhysicalGu && Entities)
    {
        FString DomainError;
        if (!Entities->CanUseGu(PhysicalGu->EntityId, DomainError))
        {
            UE_LOG(LogTemp, Warning, TEXT("Cannot activate %s: %s"), *GuDefinition->Name.ToString(), *DomainError);
            EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
            return;
        }
    }

    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    bool bExecuted = UGuExecutionLibrary::ExecuteActivation(
        GuDefinition,
        ActorInfo->AbilitySystemComponent.Get(),
        ActorInfo->AvatarActor.Get(),
        GuSystemConfig);

    // Projectile is an optional carrier, not a requirement for every Gu.
    const FGuProjectileMechanic* ProjectileMechanic = nullptr;
    for (const TInstancedStruct<FGuMechanic>& Mechanic : GuDefinition->Mechanics)
    {
        if (const FGuProjectileMechanic* Projectile = Mechanic.GetPtr<FGuProjectileMechanic>())
        {
            ProjectileMechanic = Projectile;
            break;
        }
    }

    if (ProjectileMechanic)
    {
        AActor* Avatar = ActorInfo->AvatarActor.Get();
        if (!Avatar || !ProjectileMechanic->ProjectileClass)
        {
            UE_LOG(LogTemp, Error, TEXT("%s has an invalid projectile mechanic."), *GuDefinition->Name.ToString());
            EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
            return;
        }

        const FVector SpawnLocation = Avatar->GetActorLocation() + Avatar->GetActorForwardVector() * 100.0f;
        const FTransform SpawnTransform(Avatar->GetActorRotation(), SpawnLocation);
        AGu_Projectile* SpawnedProjectile = GetWorld()->SpawnActorDeferred<AGu_Projectile>(
            ProjectileMechanic->ProjectileClass,
            SpawnTransform,
            Avatar,
            Cast<APawn>(Avatar),
            ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

        if (!SpawnedProjectile)
        {
            UE_LOG(LogTemp, Error, TEXT("SpawnActorDeferred failed for %s"), *GuDefinition->Name.ToString());
            EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
            return;
        }

        SpawnedProjectile->FinishSpawning(SpawnTransform);
        SpawnedProjectile->InitializeProjectile(
            *ProjectileMechanic,
            GuDefinition,
            ActorInfo->AbilitySystemComponent.Get());
        bExecuted = true;
    }

    if (!bExecuted)
    {
        UE_LOG(LogTemp, Warning, TEXT("%s activated but no activation/carrier mechanic executed."), *GuDefinition->Name.ToString());
    }

    if (PhysicalGu && Entities)
    {
        FString LifecycleError;
        if (!Entities->NotifySuccessfulGuActivation(PhysicalGu->EntityId, LifecycleError) && !LifecycleError.IsEmpty())
        {
            UE_LOG(LogTemp, Warning, TEXT("%s lifecycle settlement: %s"), *GuDefinition->Name.ToString(), *LifecycleError);
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("Activated Gu: %s"), *GuDefinition->Name.ToString());
    EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

bool UGA_GuAbility::CheckCost(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    FGameplayTagContainer* OptionalRelevantTags) const
{
    if (!Super::CheckCost(Handle, ActorInfo, OptionalRelevantTags)) return false;

    if (AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr)
    {
        if (const UGuRuntimeEffectComponent* Runtime = Avatar->FindComponentByClass<UGuRuntimeEffectComponent>(); Runtime && Runtime->IsGuSuppressed())
        {
            return false;
        }
    }

    if (const UGuInstanceObject* PhysicalGu = GetGuInstanceObject(Handle, ActorInfo))
    {
        const UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
        const UGuEntitySubsystem* Entities = GI ? GI->GetSubsystem<UGuEntitySubsystem>() : nullptr;
        FString DomainError;
        if (Entities && !Entities->CanUseGu(PhysicalGu->EntityId, DomainError)) return false;
    }

    const FGuEssenceCostMechanic* EssenceCost = GetEssenceCostMechanic(Handle, ActorInfo);
    if (!EssenceCost) return true;

    const UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
    if (!ASC) return false;

    return ASC->GetNumericAttribute(UAS_GuMasterAttributeSet::GetPrimevalEssenceAttribute()) >= EssenceCost->Cost;
}

void UGA_GuAbility::ApplyCost(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo) const
{
    Super::ApplyCost(Handle, ActorInfo, ActivationInfo);

    const FGuEssenceCostMechanic* EssenceCost = GetEssenceCostMechanic(Handle, ActorInfo);
    if (!EssenceCost || !PrimevalEssenceCostEffect) return;

    FGameplayEffectSpecHandle CostSpec = MakeOutgoingGameplayEffectSpec(
        PrimevalEssenceCostEffect,
        GetAbilityLevel(Handle, ActorInfo));
    if (!CostSpec.IsValid()) return;

    const FGameplayTag EssenceCostTag = FGameplayTag::RequestGameplayTag(FName("Data.Gu.EssenceCost"));
    CostSpec.Data->SetSetByCallerMagnitude(EssenceCostTag, -EssenceCost->Cost);
    ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, CostSpec);
}

const FGuEssenceCostMechanic* UGA_GuAbility::GetEssenceCostMechanic(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo) const
{
    const UGuDefinition* GuDefinition = GetGuDefinition(Handle, ActorInfo);
    if (!GuDefinition) return nullptr;

    for (const TInstancedStruct<FGuMechanic>& Mechanic : GuDefinition->Mechanics)
    {
        if (const FGuEssenceCostMechanic* Cost = Mechanic.GetPtr<FGuEssenceCostMechanic>()) return Cost;
    }
    return nullptr;
}
