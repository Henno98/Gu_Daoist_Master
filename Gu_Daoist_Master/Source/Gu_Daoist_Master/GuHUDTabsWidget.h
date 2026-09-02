#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GuHUDTypes.h"
#include "GuHUDTabsWidget.generated.h"

class UButton;
class UTextBlock;
class UHorizontalBox;
class AGu_Daoist_MasterPlayerController;

/** Small persistent tab strip. Clicking the active tab closes its panel. */
UCLASS()
class GU_DAOIST_MASTER_API UGuHUDTabsWidget : public UUserWidget
{
    GENERATED_BODY()

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
    UPROPERTY(Transient) TObjectPtr<UTextBlock> GuLabel;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> RefinementLabel;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> KillerMoveLabel;

    void BuildNativeLayout();
    void RefreshLabels();
    UButton* AddTabButton(UHorizontalBox* Parent, const FString& Label, UTextBlock*& OutLabel);
    AGu_Daoist_MasterPlayerController* Controller() const;

    UFUNCTION() void OnGuClicked();
    UFUNCTION() void OnRefinementClicked();
    UFUNCTION() void OnKillerMoveClicked();
};
