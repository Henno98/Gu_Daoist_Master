#include "GuEffectField.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "CollisionQueryParams.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GuExecutionLibrary.h"
#include "UGuDefinition.h"

AGuEffectField::AGuEffectField()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;
    bReplicates = true;
    SetReplicateMovement(false);
}

void AGuEffectField::InitializeField(
    const FGuFieldMechanic& FieldMechanic,
    UGuDefinition* InDefinition,
    UAbilitySystemComponent* InSourceASC,
    AActor* InSourceActor)
{
    Definition = InDefinition;
    SourceASC = InSourceASC;
    SourceActor = InSourceActor;
    Radius = FMath::Max(1.0f, FieldMechanic.Radius);
    TickInterval = FMath::Max(0.02f, FieldMechanic.TickInterval);
    MaxTargetsPerPulse = FMath::Max(0, FieldMechanic.MaxTargetsPerPulse);
    bIncludeSelf = FieldMechanic.bIncludeSelf;
    NextPulseAt = GetWorld() ? static_cast<double>(GetWorld()->GetTimeSeconds()) : 0.0;
    SetLifeSpan(FMath::Max(0.02f, FieldMechanic.Duration));
}

void AGuEffectField::Tick(const float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    UWorld* World = GetWorld();
    if (!World || !Definition || !SourceASC.IsValid()) return;

    const double Now = static_cast<double>(World->GetTimeSeconds());
    if (Now + KINDA_SMALL_NUMBER < NextPulseAt) return;

    Pulse();
    NextPulseAt = Now + TickInterval;
}

void AGuEffectField::Pulse()
{
    UWorld* World = GetWorld();
    AActor* Source = SourceActor.Get();
    UAbilitySystemComponent* SourceAbilitySystem = SourceASC.Get();
    if (!World || !Definition || !SourceAbilitySystem) return;

    FCollisionObjectQueryParams Objects;
    Objects.AddObjectTypesToQuery(ECC_Pawn);
    FCollisionQueryParams Query(SCENE_QUERY_STAT(GuPersistentField), false, bIncludeSelf ? nullptr : Source);
    TArray<FOverlapResult> Overlaps;
    World->OverlapMultiByObjectType(
        Overlaps,
        GetActorLocation(),
        FQuat::Identity,
        Objects,
        FCollisionShape::MakeSphere(Radius),
        Query);

    TArray<AActor*> Targets;
    for (const FOverlapResult& Overlap : Overlaps)
    {
        AActor* Actor = Overlap.GetActor();
        if (!IsValid(Actor) || (!bIncludeSelf && Actor == Source)) continue;
        if (UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Actor)) Targets.AddUnique(Actor);
    }
    if (bIncludeSelf && IsValid(Source) && UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Source))
    {
        Targets.AddUnique(Source);
    }

    const FVector Center = GetActorLocation();
    Targets.Sort([Center](const AActor& A, const AActor& B)
    {
        return FVector::DistSquared(A.GetActorLocation(), Center) < FVector::DistSquared(B.GetActorLocation(), Center);
    });

    const int32 Limit = MaxTargetsPerPulse > 0 ? FMath::Min(MaxTargetsPerPulse, Targets.Num()) : Targets.Num();
    for (int32 Index = 0; Index < Limit; ++Index)
    {
        FHitResult Hit;
        Hit.TraceStart = Center;
        Hit.TraceEnd = Targets[Index]->GetActorLocation();
        Hit.ImpactPoint = Targets[Index]->GetActorLocation();
        UGuExecutionLibrary::ExecuteImpact(Definition, SourceAbilitySystem, Targets[Index], Hit);
    }

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
    DrawDebugSphere(World, Center, Radius, 24, FColor::Cyan, false, FMath::Min(TickInterval, 0.12f), 0, 1.25f);
#endif
}
