#include "KillerMoveHUDWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "GameFramework/GameStateBase.h"
#include "GuPlayerState.h"
#include "Gu_Daoist_MasterPlayerController.h"
#include "KillerMoveTypes.h"

namespace
{
    UTextBlock* AddButtonLabel(UWidgetTree* Tree, UButton* Button, const FString& Text)
    {
        UTextBlock* Label = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        Label->SetText(FText::FromString(Text));
        Button->AddChild(Label);
        return Label;
    }

    FString InputEventText(const EKillerMoveInputEvent Event)
    {
        return Event == EKillerMoveInputEvent::Pressed ? TEXT("PRESS") : TEXT("RELEASE");
    }
}

void UKillerMoveHUDWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    if (WidgetTree && !WidgetTree->RootWidget)
    {
        BuildNativeLayout();
    }
}

void UKillerMoveHUDWidget::NativeConstruct()
{
    Super::NativeConstruct();
    RefreshState();
}

void UKillerMoveHUDWidget::BuildNativeLayout()
{
    UBorder* Background = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    Background->SetPadding(FMargin(12.0f));
    Background->SetBrushColor(FLinearColor(0.018f, 0.021f, 0.028f, 0.94f));
    WidgetTree->RootWidget = Background;

    UVerticalBox* Root = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    Background->SetContent(Root);

    TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    TitleText->SetText(FText::FromString(TEXT("KILLER MOVE")));
    TitleText->SetColorAndOpacity(FSlateColor(FLinearColor(0.94f, 0.86f, 0.58f, 1.0f)));
    Root->AddChildToVerticalBox(TitleText);

    PromptText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    PromptText->SetText(FText::FromString(TEXT("Press Killer Move to begin.")));
    Root->AddChildToVerticalBox(PromptText);

    StateText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    Root->AddChildToVerticalBox(StateText);

    StartButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
    AddButtonLabel(WidgetTree, StartButton, TEXT("KILLER MOVE"));
    StartButton->OnClicked.AddDynamic(this, &UKillerMoveHUDWidget::OnStartClicked);
    Root->AddChildToVerticalBox(StartButton);

    UHorizontalBox* SlotRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
    Root->AddChildToVerticalBox(SlotRow);

    for (int32 Index = 0; Index < 4; ++Index)
    {
        UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
        UTextBlock* Label = AddButtonLabel(WidgetTree, Button, FString::Printf(TEXT("Gu %d"), Index + 1));
        SlotButtons.Add(Button);
        SlotLabels.Add(Label);
        if (UHorizontalBoxSlot* HorizontalSlot = SlotRow->AddChildToHorizontalBox(Button))
        {
            HorizontalSlot->SetPadding(FMargin(2.0f));
        }
    }

    SlotButtons[0]->OnPressed.AddDynamic(this, &UKillerMoveHUDWidget::OnSlot1Pressed);
    SlotButtons[0]->OnReleased.AddDynamic(this, &UKillerMoveHUDWidget::OnSlot1Released);
    SlotButtons[1]->OnPressed.AddDynamic(this, &UKillerMoveHUDWidget::OnSlot2Pressed);
    SlotButtons[1]->OnReleased.AddDynamic(this, &UKillerMoveHUDWidget::OnSlot2Released);
    SlotButtons[2]->OnPressed.AddDynamic(this, &UKillerMoveHUDWidget::OnSlot3Pressed);
    SlotButtons[2]->OnReleased.AddDynamic(this, &UKillerMoveHUDWidget::OnSlot3Released);
    SlotButtons[3]->OnPressed.AddDynamic(this, &UKillerMoveHUDWidget::OnSlot4Pressed);
    SlotButtons[3]->OnReleased.AddDynamic(this, &UKillerMoveHUDWidget::OnSlot4Released);

    CancelButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
    AddButtonLabel(WidgetTree, CancelButton, TEXT("Cancel"));
    CancelButton->OnClicked.AddDynamic(this, &UKillerMoveHUDWidget::OnCancelClicked);
    Root->AddChildToVerticalBox(CancelButton);

    RefreshState();
}

AGu_Daoist_MasterPlayerController* UKillerMoveHUDWidget::Controller() const
{
    return Cast<AGu_Daoist_MasterPlayerController>(GetOwningPlayer());
}

void UKillerMoveHUDWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    RefreshState();
}

void UKillerMoveHUDWidget::RefreshState()
{
    const AGu_Daoist_MasterPlayerController* PC = Controller();
    const AGuPlayerState* PS = PC ? PC->GetPlayerState<AGuPlayerState>() : nullptr;
    if (!PS || !PromptText || !StateText) return;

    const FKillerMovePublicState& State = PS->KillerMovePublicState;
    TitleText->SetText(FText::FromString(State.Name.IsEmpty() ? TEXT("Killer Move") : State.Name));

    if (State.State == EKillerMoveRunState::Forming && State.ExpectedSlotIndex != INDEX_NONE)
    {
        float ServerNow = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
        if (const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr)
        {
            ServerNow = GS->GetServerWorldTimeSeconds();
        }
        const float Remaining = State.ExpectedServerWorldTime - ServerNow;
        const FString SlotName = State.Slots.IsValidIndex(State.ExpectedSlotIndex)
            ? State.Slots[State.ExpectedSlotIndex].SlotId.ToString()
            : FString::Printf(TEXT("Gu %d"), State.ExpectedSlotIndex + 1);
        PromptText->SetText(FText::FromString(FString::Printf(
            TEXT("%s %s  %+.2fs  window +/-%.2fs"),
            *InputEventText(State.ExpectedEvent), *SlotName, Remaining, State.TimingWindow)));
    }
    else
    {
        PromptText->SetText(FText::FromString(State.StatusText.IsEmpty() ? TEXT("Press Killer Move to begin.") : State.StatusText));
    }

    StateText->SetText(FText::FromString(FString::Printf(
        TEXT("Step %d/%d | Stability %.0f | Quality %.0f%%"),
        State.CurrentStep, State.TotalSteps, State.Stability, State.ExecutionQuality * 100.0f)));

    for (int32 Index = 0; Index < SlotButtons.Num(); ++Index)
    {
        const bool bHasSlot = State.Slots.IsValidIndex(Index);
        SlotButtons[Index]->SetVisibility(bHasSlot ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
        if (bHasSlot)
        {
            SlotLabels[Index]->SetText(FText::FromString(FString::Printf(
                TEXT("%d: %s\n%s"), Index + 1, *State.Slots[Index].SlotId.ToString(), *State.Slots[Index].GuName)));
        }
    }
    CancelButton->SetVisibility(State.State == EKillerMoveRunState::Forming ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

void UKillerMoveHUDWidget::OnStartClicked() { if (AGu_Daoist_MasterPlayerController* PC = Controller()) PC->StartKillerMove(); }
void UKillerMoveHUDWidget::OnCancelClicked() { if (AGu_Daoist_MasterPlayerController* PC = Controller()) PC->CancelKillerMove(); }
void UKillerMoveHUDWidget::SendSlot(const int32 SlotIndex, const bool bPressed)
{
    if (AGu_Daoist_MasterPlayerController* PC = Controller()) PC->KillerMoveSlotInput(SlotIndex, bPressed);
}
void UKillerMoveHUDWidget::OnSlot1Pressed() { SendSlot(0, true); }
void UKillerMoveHUDWidget::OnSlot1Released() { SendSlot(0, false); }
void UKillerMoveHUDWidget::OnSlot2Pressed() { SendSlot(1, true); }
void UKillerMoveHUDWidget::OnSlot2Released() { SendSlot(1, false); }
void UKillerMoveHUDWidget::OnSlot3Pressed() { SendSlot(2, true); }
void UKillerMoveHUDWidget::OnSlot3Released() { SendSlot(2, false); }
void UKillerMoveHUDWidget::OnSlot4Pressed() { SendSlot(3, true); }
void UKillerMoveHUDWidget::OnSlot4Released() { SendSlot(3, false); }
