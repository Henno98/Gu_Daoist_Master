#include "WildGuWorldActor.h"

#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "Net/UnrealNetwork.h"

AWildGuWorldActor::AWildGuWorldActor()
{
    PrimaryActorTick.bCanEverTick = false;

    bReplicates = true;
    SetReplicateMovement(true);

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);

    InteractionBounds = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionBounds"));
    InteractionBounds->SetupAttachment(SceneRoot);
    InteractionBounds->InitSphereRadius(60.0f);
    InteractionBounds->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    InteractionBounds->SetCollisionResponseToAllChannels(ECR_Ignore);
    InteractionBounds->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
}

void AWildGuWorldActor::InitializeWorldGu(
    const FGuid& InEntityId,
    FName InDefinitionId,
    FName InRegionId,
    int32 InSpawnSeed)
{
    GuEntityId = InEntityId;
    DefinitionId = InDefinitionId;
    RegionId = InRegionId;
    SpawnSeed = InSpawnSeed;

    K2_OnWorldGuBindingChanged();
}

void AWildGuWorldActor::OnRep_GuBinding()
{
    K2_OnWorldGuBindingChanged();
}

void AWildGuWorldActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(AWildGuWorldActor, GuEntityId);
    DOREPLIFETIME(AWildGuWorldActor, DefinitionId);
    DOREPLIFETIME(AWildGuWorldActor, RegionId);
    DOREPLIFETIME(AWildGuWorldActor, SpawnSeed);
}
