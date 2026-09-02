#include "RefinementHUDWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "GuDefinitionRegistrySubsystem.h"
#include "GuEntitySubsystem.h"
#include "GuPlayerState.h"
#include "Gu_Daoist_MasterPlayerController.h"
#include "RefinementSubsystem.h"

namespace
{
    void AddRefinementVertical(UVerticalBox* Box, UWidget* Widget, const FMargin Padding = FMargin(0.0f, 2.0f))
    {
        if (!Box || !Widget) return;
        if (UVerticalBoxSlot* VerticalSlot = Box->AddChildToVerticalBox(Widget))
        {
            VerticalSlot->SetPadding(Padding);
        }
    }

    void SetButtonEnabled(UButton* Button, const bool bEnabled)
    {
        if (Button) Button->SetIsEnabled(bEnabled);
    }
}

void URefinementHUDWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    BuildWidgetTree();
    RefreshState();
}

void URefinementHUDWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    RefreshState();
}

UTextBlock* URefinementHUDWidget::MakeText(const FString& InitialText, const bool bWrap)
{
    if (!WidgetTree) return nullptr;
    UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>();
    Text->SetText(FText::FromString(InitialText));
    Text->SetColorAndOpacity(FSlateColor(FLinearColor(0.90f, 0.91f, 0.94f, 1.0f)));
    Text->SetAutoWrapText(bWrap);
    return Text;
}

UButton* URefinementHUDWidget::MakeButton(UHorizontalBox* Parent, const FString& Label)
{
    if (!WidgetTree || !Parent) return nullptr;

    UButton* Button = WidgetTree->ConstructWidget<UButton>();
    UTextBlock* Text = MakeText(Label);
    Text->SetJustification(ETextJustify::Center);
    Button->AddChild(Text);

    if (UHorizontalBoxSlot* HorizontalSlot = Parent->AddChildToHorizontalBox(Button))
    {
        HorizontalSlot->SetPadding(FMargin(2.0f));
    }
    return Button;
}

void URefinementHUDWidget::BuildWidgetTree()
{
    if (!WidgetTree || WidgetTree->RootWidget) return;

    UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>();
    WidgetTree->RootWidget = Root;
    Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

    UBorder* Panel = WidgetTree->ConstructWidget<UBorder>();
    Panel->SetPadding(FMargin(12.0f));
    Panel->SetBrushColor(FLinearColor(0.018f, 0.021f, 0.028f, 0.92f));

    UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(Panel);
    PanelSlot->SetPosition(FVector2D(15.0f, 96.0f));
    PanelSlot->SetSize(FVector2D(720.0f, 610.0f));
    PanelSlot->SetAutoSize(false);

    UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>();
    Panel->SetContent(Content);

    UTextBlock* Title = MakeText(TEXT("REFINEMENT CAULDRON"));
    Title->SetColorAndOpacity(FSlateColor(FLinearColor(0.94f, 0.86f, 0.58f, 1.0f)));
    AddRefinementVertical(Content, Title, FMargin(0.0f, 0.0f, 0.0f, 5.0f));

    OwnedGuText = MakeText(TEXT("Aperture Gu: none registered"), true);
    SessionText = MakeText(TEXT("No active refinement."));
    FormText = MakeText(TEXT("Form: Unformed"));
    ResponseText = MakeText(TEXT("Response: Unreadable"));
    ConditionText = MakeText(TEXT("Condition: Stable"));
    AddRefinementVertical(Content, OwnedGuText, FMargin(0.0f, 0.0f, 0.0f, 6.0f));
    AddRefinementVertical(Content, SessionText);
    AddRefinementVertical(Content, FormText);
    AddRefinementVertical(Content, ResponseText);
    AddRefinementVertical(Content, ConditionText);

    FocusText = MakeText(TEXT("Focus 0 / 100"));
    AddRefinementVertical(Content, FocusText, FMargin(0.0f, 7.0f, 0.0f, 1.0f));

    FocusBar = WidgetTree->ConstructWidget<UProgressBar>();
    FocusBar->SetPercent(0.0f);
    FocusBar->SetFillColorAndOpacity(FLinearColor(0.52f, 0.28f, 0.68f, 1.0f));
    AddRefinementVertical(Content, FocusBar, FMargin(0.0f, 0.0f, 0.0f, 5.0f));

    AssistanceText = MakeText(TEXT("No active refinement Gu assistance."), true);
    ObservationText = MakeText(TEXT(""), true);
    ObservationText->SetColorAndOpacity(FSlateColor(FLinearColor(0.72f, 0.75f, 0.80f, 1.0f)));
    ResultText = MakeText(TEXT(""), true);
    DiagnosticsText = MakeText(TEXT(""), true);
    DiagnosticsText->SetColorAndOpacity(FSlateColor(FLinearColor(0.65f, 0.77f, 0.92f, 1.0f)));
    DiagnosticsText->SetVisibility(ESlateVisibility::Collapsed);
    AddRefinementVertical(Content, AssistanceText);
    AddRefinementVertical(Content, ObservationText, FMargin(0.0f, 4.0f));
    AddRefinementVertical(Content, ResultText);
    AddRefinementVertical(Content, DiagnosticsText, FMargin(0.0f, 4.0f));

    UTextBlock* ActionsLabel = MakeText(TEXT("Refinement actions"));
    ActionsLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.94f, 0.86f, 0.58f, 1.0f)));
    AddRefinementVertical(Content, ActionsLabel, FMargin(0.0f, 8.0f, 0.0f, 2.0f));

    UHorizontalBox* RowA = WidgetTree->ConstructWidget<UHorizontalBox>();
    ProcessButton = MakeButton(RowA, TEXT("Process"));
    HeatButton = MakeButton(RowA, TEXT("Heat"));
    CoolButton = MakeButton(RowA, TEXT("Cool"));
    MergeButton = MakeButton(RowA, TEXT("Merge"));
    AddRefinementVertical(Content, RowA);

    UHorizontalBox* RowB = WidgetTree->ConstructWidget<UHorizontalBox>();
    PurifyButton = MakeButton(RowB, TEXT("Purify"));
    ControlButton = MakeButton(RowB, TEXT("Control"));
    CondenseButton = MakeButton(RowB, TEXT("Condense"));
    AbortButton = MakeButton(RowB, TEXT("Abort"));
    AddRefinementVertical(Content, RowB);

    DebugControls = WidgetTree->ConstructWidget<UHorizontalBox>();
    DebugStartButton = MakeButton(DebugControls, TEXT("Start Test"));
    DebugTechniqueButton = MakeButton(DebugControls, TEXT("Use Assistant"));
    DebugAutoButton = MakeButton(DebugControls, TEXT("Auto Complete"));
    DebugDiagnosticsButton = MakeButton(DebugControls, TEXT("Diagnostics"));
    AddRefinementVertical(Content, DebugControls, FMargin(0.0f, 8.0f, 0.0f, 0.0f));

#if UE_BUILD_SHIPPING
    DebugControls->SetVisibility(ESlateVisibility::Collapsed);
#endif

    if (ProcessButton) ProcessButton->OnClicked.AddDynamic(this, &URefinementHUDWidget::OnProcessClicked);
    if (HeatButton) HeatButton->OnClicked.AddDynamic(this, &URefinementHUDWidget::OnHeatClicked);
    if (CoolButton) CoolButton->OnClicked.AddDynamic(this, &URefinementHUDWidget::OnCoolClicked);
    if (MergeButton) MergeButton->OnClicked.AddDynamic(this, &URefinementHUDWidget::OnMergeClicked);
    if (PurifyButton) PurifyButton->OnClicked.AddDynamic(this, &URefinementHUDWidget::OnPurifyClicked);
    if (ControlButton) ControlButton->OnClicked.AddDynamic(this, &URefinementHUDWidget::OnControlClicked);
    if (CondenseButton) CondenseButton->OnClicked.AddDynamic(this, &URefinementHUDWidget::OnCondenseClicked);
    if (AbortButton) AbortButton->OnClicked.AddDynamic(this, &URefinementHUDWidget::OnAbortClicked);
    if (DebugStartButton) DebugStartButton->OnClicked.AddDynamic(this, &URefinementHUDWidget::OnDebugStartClicked);
    if (DebugTechniqueButton) DebugTechniqueButton->OnClicked.AddDynamic(this, &URefinementHUDWidget::OnDebugTechniqueClicked);
    if (DebugAutoButton) DebugAutoButton->OnClicked.AddDynamic(this, &URefinementHUDWidget::OnDebugAutoClicked);
    if (DebugDiagnosticsButton) DebugDiagnosticsButton->OnClicked.AddDynamic(this, &URefinementHUDWidget::OnDebugDiagnosticsClicked);
}

void URefinementHUDWidget::RefreshState()
{
    AGu_Daoist_MasterPlayerController* PC = GetOwningPlayer<AGu_Daoist_MasterPlayerController>();
    const AGuPlayerState* PS = PC ? PC->GetPlayerState<AGuPlayerState>() : nullptr;
    if (!PS)
    {
        if (SessionText) SessionText->SetText(FText::FromString(TEXT("Refinement unavailable: GameMode is not using GuPlayerState.")));
        return;
    }

    UGameInstance* GI = PC && PC->GetWorld() ? PC->GetWorld()->GetGameInstance() : nullptr;
    const UGuEntitySubsystem* Entities = GI ? GI->GetSubsystem<UGuEntitySubsystem>() : nullptr;
    const UGuDefinitionRegistrySubsystem* Registry = GI ? GI->GetSubsystem<UGuDefinitionRegistrySubsystem>() : nullptr;
    URefinementSubsystem* Refinement = GI ? GI->GetSubsystem<URefinementSubsystem>() : nullptr;

    if (OwnedGuText && Entities)
    {
        TArray<FString> Names;
        for (const FGuid EntityId : Entities->QueryGuEntitiesForOwner(PS->DomainCharacterId, EGuContainer::Aperture, true))
        {
            const FGuInstanceComponent* Instance = Entities->GetGuInstance(EntityId);
            if (!Instance) continue;
            const FGuDefinitionRecord* Definition = Registry ? Registry->FindDefinition(Instance->DefinitionId) : nullptr;
            const FString Name = Definition ? Definition->Name : Instance->DefinitionId.ToString();
            const FGuChargesComponent* Charges = Entities->GetGuCharges(EntityId);
            if (Charges && Definition && Definition->Lifecycle.bConsumable)
            {
                Names.Add(FString::Printf(TEXT("%s [%d]"), *Name, Charges->Remaining));
            }
            else
            {
                Names.Add(Name);
            }
        }
        OwnedGuText->SetText(FText::FromString(Names.IsEmpty()
            ? TEXT("Aperture Gu: none registered")
            : FString::Printf(TEXT("Aperture Gu: %s"), *FString::Join(Names, TEXT(" | ")))));
    }

    const FRefinementPublicState& State = PS->RefinementPublicState;
    const bool bCanAct = State.bActive;

    if (SessionText)
    {
        FString SessionLabel;
        if (State.bActive)
        {
            SessionLabel = State.PhaseCount > 0
                ? FString::Printf(TEXT("Step %d / %d"), State.PhaseNumber, State.PhaseCount)
                : TEXT("Experimental formation in progress");
        }
        else if (State.bFinished)
        {
            SessionLabel = State.bSucceeded ? TEXT("Formation settled successfully") : TEXT("Formation collapsed");
        }
        else SessionLabel = TEXT("No active refinement.");
        SessionText->SetText(FText::FromString(SessionLabel));
    }

    if (FormText) FormText->SetText(FText::FromString(FString::Printf(TEXT("Form: %s"), *State.Form)));
    if (ResponseText) ResponseText->SetText(FText::FromString(FString::Printf(TEXT("Response: %s"), *State.Response)));
    if (ConditionText) ConditionText->SetText(FText::FromString(FString::Printf(TEXT("Condition: %s"), *State.Condition)));
    if (FocusText) FocusText->SetText(FText::FromString(FString::Printf(TEXT("Focus %.0f / %.0f"), State.Focus, State.MaxFocus)));
    if (FocusBar) FocusBar->SetPercent(FMath::Clamp(State.Focus / FMath::Max(1.0f, State.MaxFocus), 0.0f, 1.0f));

    if (AssistanceText)
    {
        AssistanceText->SetText(FText::FromString(State.ActiveAssistance.IsEmpty()
            ? TEXT("No active refinement Gu assistance.")
            : FString::Printf(TEXT("Assistance: %s"), *FString::Join(State.ActiveAssistance, TEXT(" | ")))));
    }

    if (ObservationText)
    {
        ObservationText->SetText(FText::FromString(State.LastObservation));
        ObservationText->SetVisibility(State.LastObservation.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
    }

    if (ResultText)
    {
        const bool bDistinctResult = State.bFinished && !State.ResultText.IsEmpty() && State.ResultText != State.LastObservation;
        ResultText->SetText(FText::FromString(bDistinctResult ? State.ResultText : TEXT("")));
        ResultText->SetColorAndOpacity(FSlateColor(State.bSucceeded
            ? FLinearColor(0.55f, 0.82f, 0.58f, 1.0f)
            : FLinearColor(0.90f, 0.46f, 0.44f, 1.0f)));
        ResultText->SetVisibility(bDistinctResult ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    }

#if !UE_BUILD_SHIPPING
    if (DiagnosticsText)
    {
        if (bShowDiagnostics && Refinement && PC->HasAuthority())
        {
            FRefinementSessionState DebugState;
            if (Refinement->GetDebugSessionState(PS->DomainCharacterId, DebugState))
            {
                const FString Path = DebugState.InitialAnalysis.PrimaryPath.IsNone()
                    ? TEXT("none")
                    : DebugState.InitialAnalysis.PrimaryPath.ToString();
                DiagnosticsText->SetText(FText::FromString(FString::Printf(
                    TEXT("DEV: temperature %.2f | impurities %.2f | stability %.2f / %.2f | stage progress %.2f | accepted %d | off-method %d | nascent path %s | contamination %.2f"),
                    DebugState.Temperature,
                    DebugState.Impurities,
                    DebugState.Stability,
                    DebugState.MaxStability,
                    DebugState.StepProgress,
                    DebugState.AcceptedActionCount,
                    DebugState.OffMethodActionCount,
                    *Path,
                    [&DebugState]()
                    {
                        float Total = 0.0f;
                        for (const TPair<FName, float>& Pair : DebugState.ContaminationPaths) Total += FMath::Max(0.0f, Pair.Value);
                        for (const TPair<FName, float>& Pair : DebugState.ContaminationAttributes) Total += FMath::Max(0.0f, Pair.Value);
                        for (const TPair<FName, float>& Pair : DebugState.ContaminationTraits) Total += FMath::Max(0.0f, Pair.Value);
                        return Total;
                    }())));
                DiagnosticsText->SetVisibility(ESlateVisibility::HitTestInvisible);
            }
            else
            {
                DiagnosticsText->SetText(FText::FromString(TEXT("DEV: no active hidden session state.")));
                DiagnosticsText->SetVisibility(ESlateVisibility::HitTestInvisible);
            }
        }
        else
        {
            DiagnosticsText->SetVisibility(ESlateVisibility::Collapsed);
        }
    }
#endif

    SetButtonEnabled(ProcessButton, bCanAct);
    SetButtonEnabled(HeatButton, bCanAct);
    SetButtonEnabled(CoolButton, bCanAct);
    SetButtonEnabled(MergeButton, bCanAct);
    SetButtonEnabled(PurifyButton, bCanAct);
    SetButtonEnabled(ControlButton, bCanAct);
    SetButtonEnabled(CondenseButton, bCanAct);
    SetButtonEnabled(AbortButton, bCanAct);
    SetButtonEnabled(DebugTechniqueButton, bCanAct);
    SetButtonEnabled(DebugAutoButton, bCanAct);
    SetButtonEnabled(DebugStartButton, !bCanAct);
}

#define WITH_GU_PC(Action) \
    if (AGu_Daoist_MasterPlayerController* PC = GetOwningPlayer<AGu_Daoist_MasterPlayerController>()) { PC->Action(); }

void URefinementHUDWidget::OnProcessClicked() { WITH_GU_PC(RefineProcess); }
void URefinementHUDWidget::OnHeatClicked() { WITH_GU_PC(RefineHeat); }
void URefinementHUDWidget::OnCoolClicked() { WITH_GU_PC(RefineCool); }
void URefinementHUDWidget::OnMergeClicked() { WITH_GU_PC(RefineMerge); }
void URefinementHUDWidget::OnPurifyClicked() { WITH_GU_PC(RefinePurify); }
void URefinementHUDWidget::OnControlClicked() { WITH_GU_PC(RefineControl); }
void URefinementHUDWidget::OnCondenseClicked() { WITH_GU_PC(RefineCondense); }
void URefinementHUDWidget::OnAbortClicked() { WITH_GU_PC(RefineAbort); }
void URefinementHUDWidget::OnDebugStartClicked() { WITH_GU_PC(RefineDebugStart); }
void URefinementHUDWidget::OnDebugTechniqueClicked() { WITH_GU_PC(RefineDebugTechnique); }
void URefinementHUDWidget::OnDebugAutoClicked() { WITH_GU_PC(RefineDebugAuto); }
void URefinementHUDWidget::OnDebugDiagnosticsClicked() { bShowDiagnostics = !bShowDiagnostics; }

#undef WITH_GU_PC
