#include "GuWildGuCaptureComponent.h"

#include "EngineUtils.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GuEcologyPopulationSubsystem.h"
#include "GuEntitySubsystem.h"
#include "GuPlayerState.h"
#include "WildGuWorldActor.h"

UGuWildGuCaptureComponent::UGuWildGuCaptureComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
}

AGuPlayerState* UGuWildGuCaptureComponent::ResolveGuPlayerState() const
{
    const AActor* OwnerActor = GetOwner();
    if (!OwnerActor) return nullptr;

    if (const AController* Controller = Cast<AController>(OwnerActor))
    {
        return Controller->GetPlayerState<AGuPlayerState>();
    }

    if (const APawn* Pawn = Cast<APawn>(OwnerActor))
    {
        if (const AController* Controller = Pawn->GetController())
        {
            return Controller->GetPlayerState<AGuPlayerState>();
        }
    }

    return nullptr;
}

AWildGuWorldActor* UGuWildGuCaptureComponent::FindNearestCaptureTarget() const
{
    UWorld* World = GetWorld();
    const AActor* OwnerActor = GetOwner();
    if (!World || !OwnerActor) return nullptr;

    const FVector Origin = OwnerActor->GetActorLocation();
    const float MaxDistanceSq = FMath::Square(FMath::Max(10.0f, CaptureRangeCm));
    float BestDistanceSq = MaxDistanceSq;
    AWildGuWorldActor* Best = nullptr;

    for (TActorIterator<AWildGuWorldActor> It(World); It; ++It)
    {
        AWildGuWorldActor* Candidate = *It;
        if (!IsValid(Candidate) || !Candidate->GetGuEntityId().IsValid()) continue;

        const float DistanceSq = FVector::DistSquared(Origin, Candidate->GetActorLocation());
        if (DistanceSq <= BestDistanceSq)
        {
            BestDistanceSq = DistanceSq;
            Best = Candidate;
        }
    }

    return Best;
}

bool UGuWildGuCaptureComponent::CanPhysicallyCaptureWildGu_Implementation(
    AWildGuWorldActor* Target,
    FString& OutReason) const
{
    (void)Target;
    OutReason.Reset();
    return true;
}

bool UGuWildGuCaptureComponent::ValidateCaptureTarget(AWildGuWorldActor* Target, FString& OutError) const
{
    const AActor* OwnerActor = GetOwner();
    if (!OwnerActor)
    {
        OutError = TEXT("Capture component has no owning actor.");
        return false;
    }
    if (!IsValid(Target) || !Target->GetGuEntityId().IsValid())
    {
        OutError = TEXT("No valid wild Gu is targeted.");
        return false;
    }

    const float Distance = FVector::Dist(OwnerActor->GetActorLocation(), Target->GetActorLocation());
    if (Distance > FMath::Max(10.0f, CaptureRangeCm))
    {
        OutError = FString::Printf(
            TEXT("The wild Gu is too far away to capture (%.0f cm > %.0f cm)."),
            Distance,
            CaptureRangeCm);
        return false;
    }

    if (!CanPhysicallyCaptureWildGu(Target, OutError))
    {
        if (OutError.IsEmpty()) OutError = TEXT("This wild Gu cannot currently be physically captured.");
        return false;
    }

    OutError.Reset();
    return true;
}

bool UGuWildGuCaptureComponent::RequestCaptureWildGu(AWildGuWorldActor* Target)
{
    FString Error;
    if (!ValidateCaptureTarget(Target, Error))
    {
        K2_OnWildGuCaptureFailed(Error);
        return false;
    }

    if (GetOwner() && GetOwner()->HasAuthority())
    {
        if (!ExecuteCaptureOnAuthority(Target, Error))
        {
            K2_OnWildGuCaptureFailed(Error);
            return false;
        }
        return true;
    }

    ServerCaptureWildGu(Target);
    return true;
}

bool UGuWildGuCaptureComponent::RequestCaptureNearestWildGu()
{
    AWildGuWorldActor* Target = FindNearestCaptureTarget();
    if (!Target)
    {
        K2_OnWildGuCaptureFailed(TEXT("No wild Gu is within physical capture range."));
        return false;
    }
    return RequestCaptureWildGu(Target);
}

void UGuWildGuCaptureComponent::ServerCaptureWildGu_Implementation(AWildGuWorldActor* Target)
{
    FString Error;
    if (!ExecuteCaptureOnAuthority(Target, Error))
    {
        ClientCaptureFailed(Error);
    }
}

bool UGuWildGuCaptureComponent::ExecuteCaptureOnAuthority(AWildGuWorldActor* Target, FString& OutError)
{
    if (!GetOwner() || !GetOwner()->HasAuthority())
    {
        OutError = TEXT("Wild Gu capture must execute on server authority.");
        return false;
    }
    if (!ValidateCaptureTarget(Target, OutError)) return false;

    AGuPlayerState* PlayerState = ResolveGuPlayerState();
    if (!PlayerState || PlayerState->DomainCharacterId.IsEmpty())
    {
        OutError = TEXT("The captor has no persistent domain character identity.");
        return false;
    }

    UWorld* World = GetWorld();
    UGuEcologyPopulationSubsystem* Ecology = World ? World->GetSubsystem<UGuEcologyPopulationSubsystem>() : nullptr;
    if (!Ecology)
    {
        OutError = TEXT("Wild Gu ecology population is unavailable.");
        return false;
    }

    const FGuid EntityId = Target->GetGuEntityId();
    const FName DefinitionId = Target->GetGuDefinitionId();

    const FGuWorldCaptureResult Result = Ecology->PhysicallyCaptureWildGu(
        EntityId,
        PlayerState->DomainCharacterId);

    if (!Result.bSuccess)
    {
        OutError = Result.Error;
        return false;
    }

    LastCapturedEntityId = EntityId;
    ClientCaptureSucceeded(EntityId, DefinitionId);
    OutError.Reset();
    return true;
}

void UGuWildGuCaptureComponent::ClientCaptureSucceeded_Implementation(const FGuid EntityId, const FName DefinitionId)
{
    LastCapturedEntityId = EntityId;
    K2_OnWildGuCaptured(EntityId, DefinitionId);
}

void UGuWildGuCaptureComponent::ClientCaptureFailed_Implementation(const FString& Error)
{
    K2_OnWildGuCaptureFailed(Error);
}

bool UGuWildGuCaptureComponent::RequestDebugInstantRefineCapturedGu(
    const FGuid EntityId,
    const EGuContainer TargetContainer)
{
#if UE_BUILD_SHIPPING
    K2_OnWildGuWillRefinementFailed(TEXT("Instant will refinement is disabled in Shipping builds."));
    return false;
#else
    if (!EntityId.IsValid())
    {
        K2_OnWildGuWillRefinementFailed(TEXT("No captured Gu entity was supplied."));
        return false;
    }

    FString Error;
    if (GetOwner() && GetOwner()->HasAuthority())
    {
        if (!ExecuteInstantRefineOnAuthority(EntityId, TargetContainer, Error))
        {
            K2_OnWildGuWillRefinementFailed(Error);
            return false;
        }
        return true;
    }

    ServerDebugInstantRefineCapturedGu(EntityId, TargetContainer);
    return true;
#endif
}

bool UGuWildGuCaptureComponent::RequestDebugInstantRefineLastCapturedGu(const EGuContainer TargetContainer)
{
    return RequestDebugInstantRefineCapturedGu(LastCapturedEntityId, TargetContainer);
}

void UGuWildGuCaptureComponent::ServerDebugInstantRefineCapturedGu_Implementation(
    const FGuid EntityId,
    const EGuContainer TargetContainer)
{
#if UE_BUILD_SHIPPING
    ClientWillRefinementFailed(TEXT("Instant will refinement is disabled in Shipping builds."));
#else
    FString Error;
    if (!ExecuteInstantRefineOnAuthority(EntityId, TargetContainer, Error))
    {
        ClientWillRefinementFailed(Error);
    }
#endif
}

bool UGuWildGuCaptureComponent::ExecuteInstantRefineOnAuthority(
    const FGuid EntityId,
    const EGuContainer TargetContainer,
    FString& OutError)
{
#if UE_BUILD_SHIPPING
    OutError = TEXT("Instant will refinement is disabled in Shipping builds.");
    return false;
#else
    if (!GetOwner() || !GetOwner()->HasAuthority())
    {
        OutError = TEXT("Gu will refinement must execute on server authority.");
        return false;
    }

    AGuPlayerState* PlayerState = ResolveGuPlayerState();
    if (!PlayerState || PlayerState->DomainCharacterId.IsEmpty())
    {
        OutError = TEXT("The refiner has no persistent domain character identity.");
        return false;
    }

    UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
    UGuEntitySubsystem* Entities = GI ? GI->GetSubsystem<UGuEntitySubsystem>() : nullptr;
    if (!Entities)
    {
        OutError = TEXT("Gu entity subsystem is unavailable.");
        return false;
    }

    if (!Entities->BeginGuWillRefinement(EntityId, PlayerState->DomainCharacterId, OutError))
    {
        return false;
    }
    if (!Entities->AdvanceGuWillRefinement(EntityId, PlayerState->DomainCharacterId, 100.0f, OutError))
    {
        return false;
    }
    if (!Entities->CompleteGuWillRefinement(EntityId, PlayerState->DomainCharacterId, TargetContainer, OutError))
    {
        return false;
    }

    ClientWillRefinementSucceeded(EntityId);
    OutError.Reset();
    return true;
#endif
}

void UGuWildGuCaptureComponent::ClientWillRefinementSucceeded_Implementation(const FGuid EntityId)
{
    K2_OnWildGuWillRefined(EntityId);
}

void UGuWildGuCaptureComponent::ClientWillRefinementFailed_Implementation(const FString& Error)
{
    K2_OnWildGuWillRefinementFailed(Error);
}
