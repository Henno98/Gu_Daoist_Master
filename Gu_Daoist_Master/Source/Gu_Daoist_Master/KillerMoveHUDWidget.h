#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "KillerMoveHUDWidget.generated.h"

class UButton;
class UTextBlock;
class UVerticalBox;
class AGu_Daoist_MasterPlayerController;

/** Stock native UMG surface for the timed-input killer-move runtime. */
UCLASS()
class GU_DAOIST_MASTER_API UKillerMoveHUDWidget : public UUserWidget
{
    GENERATED_BODY()

protected:
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> TitleText;
    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> PromptText;
    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> StateText;
    UPROPERTY(Transient)
    TObjectPtr<UButton> StartButton;
    UPROPERTY(Transient)
    TObjectPtr<UButton> CancelButton;
    UPROPERTY(Transient)
    TArray<TObjectPtr<UButton>> SlotButtons;
    UPROPERTY(Transient)
    TArray<TObjectPtr<UTextBlock>> SlotLabels;

    AGu_Daoist_MasterPlayerController* Controller() const;
    void BuildNativeLayout();
    void RefreshState();

    UFUNCTION() void OnStartClicked();
    UFUNCTION() void OnCancelClicked();
    UFUNCTION() void OnSlot1Pressed();
    UFUNCTION() void OnSlot1Released();
    UFUNCTION() void OnSlot2Pressed();
    UFUNCTION() void OnSlot2Released();
    UFUNCTION() void OnSlot3Pressed();
    UFUNCTION() void OnSlot3Released();
    UFUNCTION() void OnSlot4Pressed();
    UFUNCTION() void OnSlot4Released();
    void SendSlot(int32 SlotIndex, bool bPressed);
};
