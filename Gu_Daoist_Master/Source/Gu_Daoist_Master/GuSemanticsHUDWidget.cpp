#include "GuSemanticsHUDWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ProgressBar.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "GuDefinitionRegistrySubsystem.h"
#include "GuEntitySubsystem.h"
#include "GuPlayerState.h"
#include "Gu_Daoist_MasterPlayerController.h"
#include "Gu_Daoist_MasterCharacter.h"

namespace
{
    UTextBlock* MakeHUDText(UWidgetTree* Tree, const FString& InitialText, const bool bWrap = false)
    {
        if (!Tree) return nullptr;
        UTextBlock* Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        Text->SetText(FText::FromString(InitialText));
        Text->SetColorAndOpacity(FSlateColor(FLinearColor(0.90f, 0.91f, 0.94f, 1.0f)));
        Text->SetAutoWrapText(bWrap);
        return Text;
    }

    void AddSemanticsVertical(UVerticalBox* Box, UWidget* Widget, const FMargin Padding = FMargin(0.0f, 2.0f))
    {
        if (!Box || !Widget) return;
        if (UVerticalBoxSlot* VerticalSlot = Box->AddChildToVerticalBox(Widget))
        {
            VerticalSlot->SetPadding(Padding);
        }
    }

    UButton* MakeSmallButton(UWidgetTree* Tree, UHorizontalBox* Parent, const FString& Label)
    {
        if (!Tree || !Parent) return nullptr;
        UButton* Button = Tree->ConstructWidget<UButton>(UButton::StaticClass());
        UTextBlock* LabelText = MakeHUDText(Tree, Label);
        LabelText->SetJustification(ETextJustify::Center);
        Button->AddChild(LabelText);
        if (UHorizontalBoxSlot* HorizontalSlot = Parent->AddChildToHorizontalBox(Button))
        {
            HorizontalSlot->SetPadding(FMargin(2.0f));
        }
        return Button;
    }
}

void UGuSemanticsHUDWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    if (WidgetTree && !WidgetTree->RootWidget)
    {
        BuildNativeLayout();
    }
}

void UGuSemanticsHUDWidget::NativeConstruct()
{
    Super::NativeConstruct();
    RefreshState();
}

void UGuSemanticsHUDWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    RefreshState();
}

void UGuSemanticsHUDWidget::BuildNativeLayout()
{
    UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
    WidgetTree->RootWidget = Root;
    Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

    UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    Panel->SetPadding(FMargin(12.0f));
    Panel->SetBrushColor(FLinearColor(0.018f, 0.021f, 0.028f, 0.94f));

    if (UCanvasPanelSlot* CanvasPosition = Root->AddChildToCanvas(Panel))
    {
        CanvasPosition->SetPosition(FVector2D(15.0f, 96.0f));
        CanvasPosition->SetSize(FVector2D(720.0f, 670.0f));
        CanvasPosition->SetAutoSize(false);
    }

    UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    Panel->SetContent(Content);

    UTextBlock* Title = MakeHUDText(WidgetTree, TEXT("PHYSICAL GU / ECS"));
    Title->SetColorAndOpacity(FSlateColor(FLinearColor(0.94f, 0.86f, 0.58f, 1.0f)));
    AddSemanticsVertical(Content, Title, FMargin(0.0f, 0.0f, 0.0f, 4.0f));

    UHorizontalBox* SelectionRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
    PreviousButton = MakeSmallButton(WidgetTree, SelectionRow, TEXT("< Previous"));
    NextButton = MakeSmallButton(WidgetTree, SelectionRow, TEXT("Next >"));
    GuCountText = MakeHUDText(WidgetTree, TEXT("0 Gu"));
    if (UHorizontalBoxSlot* CountPosition = SelectionRow->AddChildToHorizontalBox(GuCountText))
    {
        CountPosition->SetPadding(FMargin(10.0f, 5.0f, 0.0f, 0.0f));
    }
    AddSemanticsVertical(Content, SelectionRow, FMargin(0.0f, 0.0f, 0.0f, 3.0f));

    ActiveText = MakeHUDText(WidgetTree, TEXT("Active Gu: none"), true);
    ActiveText->SetColorAndOpacity(FSlateColor(FLinearColor(0.62f, 0.86f, 0.70f, 1.0f)));
    AddSemanticsVertical(Content, ActiveText, FMargin(0.0f, 0.0f, 0.0f, 7.0f));

    NameText = MakeHUDText(WidgetTree, TEXT("No Gu in aperture"), true);
    NameText->SetColorAndOpacity(FSlateColor(FLinearColor(0.76f, 0.85f, 1.0f, 1.0f)));
    InstanceText = MakeHUDText(WidgetTree, TEXT(""), true);
    NourishmentText = MakeHUDText(WidgetTree, TEXT(""));
    NourishmentBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass());
    NourishmentBar->SetFillColorAndOpacity(FLinearColor(0.42f, 0.68f, 0.37f, 1.0f));

    AddSemanticsVertical(Content, NameText);
    AddSemanticsVertical(Content, InstanceText);
    AddSemanticsVertical(Content, NourishmentText, FMargin(0.0f, 5.0f, 0.0f, 1.0f));
    AddSemanticsVertical(Content, NourishmentBar, FMargin(0.0f, 0.0f, 0.0f, 7.0f));

    UTextBlock* SemanticHeading = MakeHUDText(WidgetTree, TEXT("SHARED REFINEMENT SEMANTICS"));
    SemanticHeading->SetColorAndOpacity(FSlateColor(FLinearColor(0.94f, 0.86f, 0.58f, 1.0f)));
    AddSemanticsVertical(Content, SemanticHeading, FMargin(0.0f, 4.0f, 0.0f, 3.0f));

    UScrollBox* SemanticScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
    SemanticsText = MakeHUDText(WidgetTree, TEXT("No semantic profile."), true);
    SemanticScroll->AddChild(SemanticsText);
    if (UVerticalBoxSlot* SemanticPosition = Content->AddChildToVerticalBox(SemanticScroll))
    {
        SemanticPosition->SetPadding(FMargin(0.0f, 2.0f));
        SemanticPosition->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    }

    if (PreviousButton) PreviousButton->OnClicked.AddDynamic(this, &UGuSemanticsHUDWidget::OnPreviousClicked);
    if (NextButton) NextButton->OnClicked.AddDynamic(this, &UGuSemanticsHUDWidget::OnNextClicked);
}

FString UGuSemanticsHUDWidget::GuDisplayName(const FGuid EntityId)
{
    const AGu_Daoist_MasterPlayerController* PC = GetOwningPlayer<AGu_Daoist_MasterPlayerController>();
    const AGuPlayerState* PS = PC ? PC->GetPlayerState<AGuPlayerState>() : nullptr;
    if (PS)
    {
        if (const FGuPublicInventoryEntry* PublicEntry = PS->FindPublicGu(EntityId))
        {
            return PublicEntry->Name;
        }
    }

    UGameInstance* GI = PC && PC->GetWorld() ? PC->GetWorld()->GetGameInstance() : nullptr;
    const UGuEntitySubsystem* Entities = GI ? GI->GetSubsystem<UGuEntitySubsystem>() : nullptr;
    const UGuDefinitionRegistrySubsystem* Registry = GI ? GI->GetSubsystem<UGuDefinitionRegistrySubsystem>() : nullptr;
    const FGuInstanceComponent* Instance = Entities ? Entities->GetGuInstance(EntityId) : nullptr;
    const FGuDefinitionRecord* Definition = Instance && Registry ? Registry->FindDefinition(Instance->DefinitionId) : nullptr;
    return Definition ? Definition->Name : (Instance ? Instance->DefinitionId.ToString() : EntityId.ToString());
}

TArray<FGuid> UGuSemanticsHUDWidget::GetOwnedGuIds()
{
    TArray<FGuid> Result;
    const AGu_Daoist_MasterPlayerController* PC = GetOwningPlayer<AGu_Daoist_MasterPlayerController>();
    const AGuPlayerState* PS = PC ? PC->GetPlayerState<AGuPlayerState>() : nullptr;
    if (!PS) return Result;

    // Remote listen clients do not own the authoritative server GameInstance ECS.
    // Use the owner-only replicated projection whenever it is available.
    if (!PS->OwnedGuInventory.IsEmpty())
    {
        Result.Reserve(PS->OwnedGuInventory.Num());
        for (const FGuPublicInventoryEntry& Entry : PS->OwnedGuInventory)
        {
            if (Entry.EntityId.IsValid()) Result.Add(Entry.EntityId);
        }
    }
    else
    {
        UGameInstance* GI = PC && PC->GetWorld() ? PC->GetWorld()->GetGameInstance() : nullptr;
        const UGuEntitySubsystem* Entities = GI ? GI->GetSubsystem<UGuEntitySubsystem>() : nullptr;
        if (Entities && !PS->DomainCharacterId.IsEmpty())
        {
            Result = Entities->QueryGuEntitiesForOwner(PS->DomainCharacterId, EGuContainer::Aperture, false);
        }
    }

    Result.Sort([this](const FGuid& A, const FGuid& B)
    {
        const FString NameA = GuDisplayName(A);
        const FString NameB = GuDisplayName(B);
        const int32 NameCompare = NameA.Compare(NameB, ESearchCase::IgnoreCase);
        return NameCompare == 0 ? A.ToString() < B.ToString() : NameCompare < 0;
    });
    return Result;
}

void UGuSemanticsHUDWidget::EnsureValidSelection(const TArray<FGuid>& OwnedGuIds)
{
    if (OwnedGuIds.IsEmpty())
    {
        SelectedGuId.Invalidate();
        return;
    }
    if (!SelectedGuId.IsValid() || !OwnedGuIds.Contains(SelectedGuId))
    {
        const AGu_Daoist_MasterPlayerController* PC = GetOwningPlayer<AGu_Daoist_MasterPlayerController>();
        const AGuPlayerState* PS = PC ? PC->GetPlayerState<AGuPlayerState>() : nullptr;
        SelectedGuId = PS && PS->ActiveGuEntityId.IsValid() && OwnedGuIds.Contains(PS->ActiveGuEntityId)
            ? PS->ActiveGuEntityId
            : OwnedGuIds[0];
    }
}

FString UGuSemanticsHUDWidget::FormatSemanticMap(const FString& Heading, const TMap<FName, float>& Values)
{
    FString Out = Heading + TEXT("\n");
    if (Values.IsEmpty()) return Out + TEXT("  none\n");

    TArray<FName> Keys;
    Values.GenerateKeyArray(Keys);
    Keys.Sort([](const FName& A, const FName& B)
    {
        return A.ToString() < B.ToString();
    });
    for (const FName Key : Keys)
    {
        Out += FString::Printf(TEXT("  %-22s %.3f\n"), *Key.ToString(), Values.FindRef(Key));
    }
    return Out;
}

void UGuSemanticsHUDWidget::RefreshState()
{
    const AGu_Daoist_MasterPlayerController* PC = GetOwningPlayer<AGu_Daoist_MasterPlayerController>();
    const AGuPlayerState* PS = PC ? PC->GetPlayerState<AGuPlayerState>() : nullptr;
    if (!PC || !PS) return;

    UGameInstance* GI = PC->GetWorld() ? PC->GetWorld()->GetGameInstance() : nullptr;
    const UGuEntitySubsystem* Entities = GI ? GI->GetSubsystem<UGuEntitySubsystem>() : nullptr;
    const UGuDefinitionRegistrySubsystem* Registry = GI ? GI->GetSubsystem<UGuDefinitionRegistrySubsystem>() : nullptr;

    const TArray<FGuid> OwnedGuIds = GetOwnedGuIds();
    EnsureValidSelection(OwnedGuIds);
    if (GuCountText)
    {
        const int32 SelectedIndex = SelectedGuId.IsValid() ? OwnedGuIds.IndexOfByKey(SelectedGuId) : INDEX_NONE;
        GuCountText->SetText(FText::FromString(SelectedIndex == INDEX_NONE
            ? FString::Printf(TEXT("%d Gu in aperture"), OwnedGuIds.Num())
            : FString::Printf(TEXT("Gu %d / %d"), SelectedIndex + 1, OwnedGuIds.Num())));
    }
    if (PreviousButton) PreviousButton->SetIsEnabled(OwnedGuIds.Num() > 1);
    if (NextButton) NextButton->SetIsEnabled(OwnedGuIds.Num() > 1);

    if (ActiveText)
    {
        ActiveText->SetText(FText::FromString(PS->ActiveGuEntityId.IsValid()
            ? FString::Printf(TEXT("Active Gu: %s"), *GuDisplayName(PS->ActiveGuEntityId))
            : TEXT("Active Gu: none")));
    }

    if (!SelectedGuId.IsValid())
    {
        if (NameText) NameText->SetText(FText::FromString(TEXT("No Gu in aperture")));
        if (InstanceText) InstanceText->SetText(FText::GetEmpty());
        if (NourishmentText) NourishmentText->SetText(FText::GetEmpty());
        if (NourishmentBar) NourishmentBar->SetPercent(0.0f);
        if (SemanticsText) SemanticsText->SetText(FText::FromString(TEXT("No semantic profile.")));
        return;
    }

    const FGuPublicInventoryEntry* PublicEntry = PS->FindPublicGu(SelectedGuId);
    const FGuInstanceComponent* Instance = Entities ? Entities->GetGuInstance(SelectedGuId) : nullptr;
    const FGuDefinitionRecord* Definition = Instance && Registry ? Registry->FindDefinition(Instance->DefinitionId) : nullptr;

    // Server/listen host can render directly from ECS. Remote owning clients use the replicated projection.
    if (!Instance || !Definition)
    {
        if (!PublicEntry) return;

        const bool bIsActive = PS->ActiveGuEntityId == SelectedGuId;
        if (NameText)
        {
            NameText->SetText(FText::FromString(FString::Printf(
                TEXT("%s%s | Rank %d | %s"),
                bIsActive ? TEXT("[ACTIVE] ") : TEXT(""),
                *PublicEntry->Name,
                PublicEntry->Rank,
                *PublicEntry->Path.ToString())));
        }
        if (InstanceText)
        {
            const FString ChargeSuffix = PublicEntry->RemainingCharges >= 0
                ? FString::Printf(TEXT(" | Charges %d"), PublicEntry->RemainingCharges)
                : FString();
            InstanceText->SetText(FText::FromString(FString::Printf(
                TEXT("Alive %s | Durability %.0f | Quality %.2f | Activations %d%s\nDefinition %s | Entity %s"),
                PublicEntry->bAlive ? TEXT("yes") : TEXT("no"),
                PublicEntry->Durability,
                PublicEntry->Quality,
                PublicEntry->ActivationCount,
                *ChargeSuffix,
                *PublicEntry->DefinitionId.ToString(),
                *SelectedGuId.ToString(EGuidFormats::DigitsWithHyphens))));
        }
        if (NourishmentText)
        {
            NourishmentText->SetText(FText::FromString(FString::Printf(
                TEXT("Nourishment %.0f / 100 | Food: %s | Feed interval %.1fh"),
                PublicEntry->Hunger,
                *PublicEntry->FoodKey.ToString(),
                PublicEntry->FeedingIntervalHours)));
        }
        if (NourishmentBar) NourishmentBar->SetPercent(FMath::Clamp(PublicEntry->Hunger / 100.0f, 0.0f, 1.0f));
        if (SemanticsText) SemanticsText->SetText(FText::FromString(PublicEntry->SemanticsSummary));
        return;
    }

    const FGuConditionComponent* Condition = Entities->GetGuCondition(SelectedGuId);
    const FGuVisualStateComponent* Visual = Entities->GetGuVisualState(SelectedGuId);
    const FGuNourishmentComponent* Nourishment = Entities->GetGuNourishment(SelectedGuId);
    const FGuChargesComponent* Charges = Entities->GetGuCharges(SelectedGuId);
    const bool bIsActive = PS->ActiveGuEntityId == SelectedGuId;

    if (NameText)
    {
        NameText->SetText(FText::FromString(FString::Printf(
            TEXT("%s%s | Rank %d | %s"),
            bIsActive ? TEXT("[ACTIVE] ") : TEXT(""),
            *Definition->Name,
            Definition->Rank,
            *Definition->Path.ToString())));
    }
    if (InstanceText)
    {
        const FString ChargeSuffix = Charges ? FString::Printf(TEXT(" | Charges %d"), Charges->Remaining) : FString();
        InstanceText->SetText(FText::FromString(FString::Printf(
            TEXT("Condition %s | Durability %.0f | Quality %.2f | Activations %d%s\nDefinition %s | Entity %s"),
            Visual ? *Visual->Condition.ToString() : TEXT("unknown"),
            Condition ? Condition->Durability : 0.0f,
            Condition ? Condition->Quality : 0.0f,
            Condition ? Condition->ActivationCount : 0,
            *ChargeSuffix,
            *Instance->DefinitionId.ToString(),
            *SelectedGuId.ToString(EGuidFormats::DigitsWithHyphens))));
    }
    if (NourishmentText)
    {
        NourishmentText->SetText(FText::FromString(Nourishment
            ? FString::Printf(TEXT("Nourishment %.0f / 100 | Food: %s | Feed interval %.1fh"), Nourishment->Hunger, *Nourishment->FoodKey.ToString(), Nourishment->IntervalHours)
            : TEXT("No nourishment component")));
    }
    if (NourishmentBar) NourishmentBar->SetPercent(Nourishment ? FMath::Clamp(Nourishment->Hunger / 100.0f, 0.0f, 1.0f) : 0.0f);

    FRefinementSemanticSnapshot Snapshot;
    Entities->GetRefinementSemanticSnapshot(SelectedGuId, Snapshot);
    FRefinementSemanticProfile Semantic = Snapshot.EntityId.IsValid() ? Snapshot.Semantic : Definition->RefinementProfile;
    if (Snapshot.EntityId.IsValid()) Semantic.bDerivedPropertySnapshot = Definition->RefinementProfile.bDerivedPropertySnapshot;

    FString Body;
    Body += FString::Printf(TEXT("Dao mass: %.3f%s\n\n"), Semantic.DaoMass, Semantic.bDerivedPropertySnapshot ? TEXT("  [derived property snapshot]") : TEXT(""));
    Body += FormatSemanticMap(TEXT("PATHS"), Semantic.Paths) + TEXT("\n");
    Body += FormatSemanticMap(TEXT("PROPERTIES"), Semantic.Properties) + TEXT("\n");
    Body += FormatSemanticMap(TEXT("ATTRIBUTES"), Semantic.Attributes) + TEXT("\n");
    Body += FormatSemanticMap(TEXT("TRAITS"), Semantic.Traits) + TEXT("\n");
    Body += FormatSemanticMap(TEXT("TEMPLATES"), Semantic.Templates) + TEXT("\n");

    if (Snapshot.EntityId.IsValid())
    {
        Body += FString::Printf(TEXT("CONTAMINATION TOTAL\n  %.3f\n\n"), Snapshot.Contamination.Total);
        Body += FormatSemanticMap(TEXT("CONTAMINATION PATHS"), Snapshot.Contamination.Paths) + TEXT("\n");
        Body += FormatSemanticMap(TEXT("CONTAMINATION ATTRIBUTES"), Snapshot.Contamination.Attributes) + TEXT("\n");
        Body += FormatSemanticMap(TEXT("CONTAMINATION TRAITS"), Snapshot.Contamination.Traits);
    }

    if (SemanticsText) SemanticsText->SetText(FText::FromString(Body));
}

void UGuSemanticsHUDWidget::CommitSelectionAsActive()
{
    if (!SelectedGuId.IsValid()) return;
    AGu_Daoist_MasterPlayerController* PC = GetOwningPlayer<AGu_Daoist_MasterPlayerController>();
    AGu_Daoist_MasterCharacter* PlayerCharacter = PC ? Cast<AGu_Daoist_MasterCharacter>(PC->GetPawn()) : nullptr;
    if (PlayerCharacter)
    {
        PlayerCharacter->RequestSetActiveGuEntity(SelectedGuId);
    }
}

void UGuSemanticsHUDWidget::OnPreviousClicked()
{
    const TArray<FGuid> OwnedGuIds = GetOwnedGuIds();
    EnsureValidSelection(OwnedGuIds);
    if (OwnedGuIds.Num() <= 1) return;
    const int32 Current = OwnedGuIds.IndexOfByKey(SelectedGuId);
    SelectedGuId = OwnedGuIds[(Current <= 0 ? OwnedGuIds.Num() : Current) - 1];
    CommitSelectionAsActive();
    RefreshState();
}

void UGuSemanticsHUDWidget::OnNextClicked()
{
    const TArray<FGuid> OwnedGuIds = GetOwnedGuIds();
    EnsureValidSelection(OwnedGuIds);
    if (OwnedGuIds.Num() <= 1) return;
    const int32 Current = OwnedGuIds.IndexOfByKey(SelectedGuId);
    SelectedGuId = OwnedGuIds[(Current + 1) % OwnedGuIds.Num()];
    CommitSelectionAsActive();
    RefreshState();
}
