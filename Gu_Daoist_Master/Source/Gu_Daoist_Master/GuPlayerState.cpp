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
    DOREPLIFETIME_CONDITION(AGuPlayerState, OwnedGuInventory, COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(AGuPlayerState, ActiveGuEntityId, COND_OwnerOnly);
    DOREPLIFETIME(AGuPlayerState, RefinementPublicState);
    DOREPLIFETIME(AGuPlayerState, KillerMovePublicState);
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

void AGuPlayerState::SetOwnedGuInventory(const TArray<FGuPublicInventoryEntry>& NewInventory)
{
    if (!HasAuthority()) return;
    OwnedGuInventory = NewInventory;
    ForceNetUpdate();
}

void AGuPlayerState::SetActiveGuEntityId(const FGuid NewActiveGuEntityId)
{
    if (!HasAuthority()) return;
    ActiveGuEntityId = NewActiveGuEntityId;
    ForceNetUpdate();
}

const FGuPublicInventoryEntry* AGuPlayerState::FindPublicGu(const FGuid EntityId) const
{
    return OwnedGuInventory.FindByPredicate([EntityId](const FGuPublicInventoryEntry& Entry)
    {
        return Entry.EntityId == EntityId;
    });
}

void AGuPlayerState::OnRep_OwnedGuInventory()
{
    // Native Gu HUD reads the replicated projection directly.
}

void AGuPlayerState::OnRep_ActiveGuEntityId()
{
    // Native Gu HUD reads the replicated active selection directly.
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

void AGuPlayerState::SetKillerMovePublicState(const FKillerMovePublicState& NewState)
{
    if (!HasAuthority()) return;
    KillerMovePublicState = NewState;
    ForceNetUpdate();
}

void AGuPlayerState::ClearKillerMovePublicState()
{
    if (!HasAuthority()) return;
    KillerMovePublicState = FKillerMovePublicState();
    ForceNetUpdate();
}

void AGuPlayerState::OnRep_KillerMovePublicState()
{
    // Native killer-move UMG reads this replicated projection directly.
}

void AGuPlayerState::PropagateDomainCharacterId()
{
    if (MentalResources)
    {
        MentalResources->DomainOwnerId = DomainCharacterId;
    }
}
