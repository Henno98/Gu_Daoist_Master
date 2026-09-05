#include "GuRuntimeHUDSubsystem.h"

#include "Blueprint/UserWidget.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GuRuntimeHUDWidget.h"

void UGuRuntimeHUDSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    TryCreateHUD();
}

void UGuRuntimeHUDSubsystem::Deinitialize()
{
    if (RuntimeHUD)
    {
        RuntimeHUD->RemoveFromParent();
        RuntimeHUD = nullptr;
    }

    Super::Deinitialize();
}

void UGuRuntimeHUDSubsystem::Tick(const float DeltaTime)
{
    if (!RuntimeHUD)
    {
        TryCreateHUD();
    }
}

TStatId UGuRuntimeHUDSubsystem::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(UGuRuntimeHUDSubsystem, STATGROUP_Tickables);
}

bool UGuRuntimeHUDSubsystem::IsTickable() const
{
    return !IsTemplate();
}

void UGuRuntimeHUDSubsystem::TryCreateHUD()
{
    ULocalPlayer* LocalPlayer = GetLocalPlayer();
    UWorld* World = LocalPlayer ? LocalPlayer->GetWorld() : nullptr;
    APlayerController* PC = LocalPlayer && World ? LocalPlayer->GetPlayerController(World) : nullptr;

    if (!PC || !PC->IsLocalController())
    {
        return;
    }

    RuntimeHUD = CreateWidget<UGuRuntimeHUDWidget>(
        PC,
        UGuRuntimeHUDWidget::StaticClass(),
        TEXT("GuRuntimeHUD"));

    if (RuntimeHUD)
    {
        RuntimeHUD->AddToPlayerScreen(5);
    }
}
