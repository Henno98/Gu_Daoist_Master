#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "Tickable.h"
#include "GuRuntimeHUDSubsystem.generated.h"

class UGuRuntimeHUDWidget;

/**
 * Automatically installs the native functionality HUD for each local player.
 * No GameMode or Blueprint HUDClass replacement is required.
 */
UCLASS()
class GU_DAOIST_MASTER_API UGuRuntimeHUDSubsystem : public ULocalPlayerSubsystem, public FTickableGameObject
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override;
    virtual bool IsTickable() const override;

private:
    void TryCreateHUD();

    UPROPERTY(Transient)
    TObjectPtr<UGuRuntimeHUDWidget> RuntimeHUD;
};
