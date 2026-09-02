#include "GuHUDTabsWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Gu_Daoist_MasterPlayerController.h"

void UGuHUDTabsWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    if (WidgetTree && !WidgetTree->RootWidget)
    {
        BuildNativeLayout();
    }
}

void UGuHUDTabsWidget::NativeConstruct()
{
    Super::NativeConstruct();
    RefreshLabels();
}

void UGuHUDTabsWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    RefreshLabels();
}

AGu_Daoist_MasterPlayerController* UGuHUDTabsWidget::Controller() const
{
    return Cast<AGu_Daoist_MasterPlayerController>(GetOwningPlayer());
}

UButton* UGuHUDTabsWidget::AddTabButton(UHorizontalBox* Parent, const FString& Label, UTextBlock*& OutLabel)
{
    if (!WidgetTree || !Parent) return nullptr;

    UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
    OutLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    OutLabel->SetText(FText::FromString(Label));
    OutLabel->SetJustification(ETextJustify::Center);
    OutLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.92f, 0.93f, 0.96f, 1.0f)));
    Button->AddChild(OutLabel);

    if (UHorizontalBoxSlot* HorizontalSlot = Parent->AddChildToHorizontalBox(Button))
    {
        HorizontalSlot->SetPadding(FMargin(2.0f));
    }
    return Button;
}

void UGuHUDTabsWidget::BuildNativeLayout()
{
    UBorder* Background = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    Background->SetPadding(FMargin(6.0f));
    Background->SetBrushColor(FLinearColor(0.018f, 0.021f, 0.028f, 0.94f));
    WidgetTree->RootWidget = Background;

    UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
    Background->SetContent(Row);

    UTextBlock* GuLabelRaw = nullptr;
    UTextBlock* RefinementLabelRaw = nullptr;
    UTextBlock* KillerMoveLabelRaw = nullptr;

    UButton* GuButton = AddTabButton(Row, TEXT("GU"), GuLabelRaw);
    UButton* RefinementButton = AddTabButton(Row, TEXT("REFINEMENT"), RefinementLabelRaw);
    UButton* KillerMoveButton = AddTabButton(Row, TEXT("KILLER MOVE"), KillerMoveLabelRaw);

    GuLabel = GuLabelRaw;
    RefinementLabel = RefinementLabelRaw;
    KillerMoveLabel = KillerMoveLabelRaw;

    if (GuButton) GuButton->OnClicked.AddDynamic(this, &UGuHUDTabsWidget::OnGuClicked);
    if (RefinementButton) RefinementButton->OnClicked.AddDynamic(this, &UGuHUDTabsWidget::OnRefinementClicked);
    if (KillerMoveButton) KillerMoveButton->OnClicked.AddDynamic(this, &UGuHUDTabsWidget::OnKillerMoveClicked);
}

void UGuHUDTabsWidget::RefreshLabels()
{
    const AGu_Daoist_MasterPlayerController* PC = Controller();
    const EGuHUDTab Active = PC ? PC->GetActiveGuHUDTab() : EGuHUDTab::None;

    if (GuLabel) GuLabel->SetText(FText::FromString(Active == EGuHUDTab::Gu ? TEXT("[ GU ]") : TEXT("GU")));
    if (RefinementLabel) RefinementLabel->SetText(FText::FromString(Active == EGuHUDTab::Refinement ? TEXT("[ REFINEMENT ]") : TEXT("REFINEMENT")));
    if (KillerMoveLabel) KillerMoveLabel->SetText(FText::FromString(Active == EGuHUDTab::KillerMove ? TEXT("[ KILLER MOVE ]") : TEXT("KILLER MOVE")));
}

void UGuHUDTabsWidget::OnGuClicked()
{
    if (AGu_Daoist_MasterPlayerController* PC = Controller()) PC->ToggleGuHUDTab(EGuHUDTab::Gu);
}

void UGuHUDTabsWidget::OnRefinementClicked()
{
    if (AGu_Daoist_MasterPlayerController* PC = Controller()) PC->ToggleGuHUDTab(EGuHUDTab::Refinement);
}

void UGuHUDTabsWidget::OnKillerMoveClicked()
{
    if (AGu_Daoist_MasterPlayerController* PC = Controller()) PC->ToggleGuHUDTab(EGuHUDTab::KillerMove);
}
