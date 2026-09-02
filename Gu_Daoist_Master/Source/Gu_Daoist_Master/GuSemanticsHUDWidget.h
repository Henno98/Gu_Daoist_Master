#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GuSemanticsHUDWidget.generated.h"

class UButton;
class UTextBlock;
class UProgressBar;

/** Inspection page for owned physical Gu and the shared semantics used by ECS/refinement/killer moves. */
UCLASS()
class GU_DAOIST_MASTER_API UGuSemanticsHUDWidget : public UUserWidget
{
    GENERATED_BODY()

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
    UPROPERTY(Transient) TObjectPtr<UTextBlock> GuCountText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> NameText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> InstanceText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> NourishmentText;
    UPROPERTY(Transient) TObjectPtr<UProgressBar> NourishmentBar;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> SemanticsText;
    UPROPERTY(Transient) TObjectPtr<UButton> PreviousButton;
    UPROPERTY(Transient) TObjectPtr<UButton> NextButton;

    FGuid SelectedGuId;

    void BuildNativeLayout();
    void RefreshState();
    TArray<FGuid> GetOwnedGuIds();
    void EnsureValidSelection(const TArray<FGuid>& OwnedGuIds);
    FString GuDisplayName(FGuid EntityId);

    static FString FormatSemanticMap(const FString& Heading, const TMap<FName, float>& Values);

    UFUNCTION() void OnPreviousClicked();
    UFUNCTION() void OnNextClicked();
};
