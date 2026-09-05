#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GuRuntimeHUDWidget.generated.h"

class UBorder;
class UProgressBar;
class UTextBlock;

/**
 * Functionality-first native HUD.
 *
 * It is deliberately a VIEW of authoritative state. It owns no health, essence,
 * mental-resource, Gu, ecology or capture state.
 */
UCLASS()
class GU_DAOIST_MASTER_API UGuRuntimeHUDWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
    void BuildWidgetTree();
    void RefreshRuntimeState();

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> HeaderText;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> HealthText;

    UPROPERTY(Transient)
    TObjectPtr<UProgressBar> HealthBar;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> EssenceText;

    UPROPERTY(Transient)
    TObjectPtr<UProgressBar> EssenceBar;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> StaminaText;

    UPROPERTY(Transient)
    TObjectPtr<UProgressBar> StaminaBar;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> CultivationText;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> MentalText;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> PressureText;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> StatusText;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> WorldTargetText;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> CrosshairText;

    float RefreshAccumulator = 0.0f;
};
