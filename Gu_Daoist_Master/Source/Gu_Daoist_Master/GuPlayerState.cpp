#include "GuPlayerState.h"
#include "Net/UnrealNetwork.h"

AGuPlayerState::AGuPlayerState()
{
    MentalResources = CreateDefaultSubobject<UMentalResourceComponent>(TEXT("MentalResources"));
}

void AGuPlayerState::BeginPlay()
{
    Super::BeginPlay();

    if (HasAuthority() && DomainCharacterId.IsEmpty())
    {
        DomainCharacterId = FString::Printf(TEXT("player:%d"), GetPlayerId());
    }
    PropagateDomainCharacterId();
}

void AGuPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AGuPlayerState, CultivationRank);
    DOREPLIFETIME(AGuPlayerState, DomainCharacterId);
    DOREPLIFETIME(AGuPlayerState, RefinementPublicState);
}

void AGuPlayerState::SetDomainCharacterId(const FString& NewCharacterId)
{
    if (!HasAuthority()) return;
    DomainCharacterId = NewCharacterId.TrimStartAndEnd();
    PropagateDomainCharacterId();
    ForceNetUpdate();
}

void AGuPlayerState::OnRep_DomainCharacterId()
{
    PropagateDomainCharacterId();
}

void AGuPlayerState::SetRefinementPublicState(const FRefinementPublicState& NewState)
{
    if (!HasAuthority()) return;
    RefinementPublicState = NewState;
    ForceNetUpdate();
}

void AGuPlayerState::ClearRefinementPublicState()
{
    if (!HasAuthority()) return;
    RefinementPublicState = FRefinementPublicState();
    ForceNetUpdate();
}

void AGuPlayerState::OnRep_RefinementPublicState()
{
    // UMG reads the replicated public projection directly.
}

void AGuPlayerState::PropagateDomainCharacterId()
{
    if (MentalResources)
    {
        MentalResources->DomainOwnerId = DomainCharacterId;
    }
}
