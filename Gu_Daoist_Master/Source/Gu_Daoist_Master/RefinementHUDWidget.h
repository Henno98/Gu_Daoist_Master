#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RefinementHUDWidget.generated.h"

class UButton;
class UProgressBar;
class UTextBlock;
class UHorizontalBox;

/** Asset-free native UMG refinement interface for the original project. */
UCLASS()
class GU_DAOIST_MASTER_API URefinementHUDWidget : public UUserWidget
{
    GENERATED_BODY()

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
    UPROPERTY(Transient) TObjectPtr<UTextBlock> SessionText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> FormText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> ResponseText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> ConditionText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> FocusText;
    UPROPERTY(Transient) TObjectPtr<UProgressBar> FocusBar;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> OwnedGuText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> AssistanceText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> ObservationText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> ResultText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> DiagnosticsText;

    UPROPERTY(Transient) TObjectPtr<UButton> ProcessButton;
    UPROPERTY(Transient) TObjectPtr<UButton> HeatButton;
    UPROPERTY(Transient) TObjectPtr<UButton> CoolButton;
    UPROPERTY(Transient) TObjectPtr<UButton> MergeButton;
    UPROPERTY(Transient) TObjectPtr<UButton> PurifyButton;
    UPROPERTY(Transient) TObjectPtr<UButton> ControlButton;
    UPROPERTY(Transient) TObjectPtr<UButton> CondenseButton;
    UPROPERTY(Transient) TObjectPtr<UButton> AbortButton;
    UPROPERTY(Transient) TObjectPtr<UButton> DebugStartButton;
    UPROPERTY(Transient) TObjectPtr<UButton> DebugTechniqueButton;
    UPROPERTY(Transient) TObjectPtr<UButton> DebugAutoButton;
    UPROPERTY(Transient) TObjectPtr<UButton> DebugDiagnosticsButton;
    UPROPERTY(Transient) TObjectPtr<UHorizontalBox> DebugControls;

    bool bShowDiagnostics = false;

    void BuildWidgetTree();
    void RefreshState();
    UButton* MakeButton(UHorizontalBox* Parent, const FString& Label);
    UTextBlock* MakeText(const FString& InitialText, bool bWrap = false);

    UFUNCTION() void OnProcessClicked();
    UFUNCTION() void OnHeatClicked();
    UFUNCTION() void OnCoolClicked();
    UFUNCTION() void OnMergeClicked();
    UFUNCTION() void OnPurifyClicked();
    UFUNCTION() void OnControlClicked();
    UFUNCTION() void OnCondenseClicked();
    UFUNCTION() void OnAbortClicked();
    UFUNCTION() void OnDebugStartClicked();
    UFUNCTION() void OnDebugTechniqueClicked();
    UFUNCTION() void OnDebugAutoClicked();
    UFUNCTION() void OnDebugDiagnosticsClicked();
};
