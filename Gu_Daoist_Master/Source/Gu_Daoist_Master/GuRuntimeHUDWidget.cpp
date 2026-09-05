#include "GuRuntimeHUDWidget.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "AttributeSet.h"
#include "Blueprint/WidgetTree.h"
#include "Components/ActorComponent.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"
#include "Styling/CoreStyle.h"
#include "UObject/UnrealType.h"
#include "WildGuWorldActor.h"

#if __has_include("MentalResourceComponent.h")
#include "MentalResourceComponent.h"
#define GU_RUNTIME_HUD_HAS_MENTAL_RESOURCE 1
#else
#define GU_RUNTIME_HUD_HAS_MENTAL_RESOURCE 0
#endif

namespace
{
    static TAutoConsoleVariable<int32> CVarGuFunctionalHUD(
        TEXT("gu.HUD.Functional"),
        1,
        TEXT("Show the functionality-first native Gu runtime HUD."),
        ECVF_Default);

    constexpr float HudRefreshInterval = 0.10f;

    bool ReadNumericProperty(const UObject* Object, const FName PropertyName, double& OutValue)
    {
        if (!Object || PropertyName.IsNone()) return false;

        const FProperty* Property = Object->GetClass()->FindPropertyByName(PropertyName);
        const FNumericProperty* Numeric = CastField<FNumericProperty>(Property);
        if (!Numeric) return false;

        const void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Object);
        if (!ValuePtr) return false;

        if (Numeric->IsFloatingPoint())
        {
            OutValue = Numeric->GetFloatingPointPropertyValue(ValuePtr);
            return true;
        }

        if (Numeric->IsInteger())
        {
            OutValue = static_cast<double>(Numeric->GetSignedIntPropertyValue(ValuePtr));
            return true;
        }

        return false;
    }

    bool ReadNumericFromObjectSet(
        const TArray<UObject*>& Objects,
        const TArray<FName>& Names,
        double& OutValue)
    {
        for (UObject* Object : Objects)
        {
            for (const FName Name : Names)
            {
                if (ReadNumericProperty(Object, Name, OutValue)) return true;
            }
        }
        return false;
    }

    void AddActorObjects(AActor* Actor, TArray<UObject*>& Objects)
    {
        if (!Actor) return;
        Objects.Add(Actor);

        TArray<UActorComponent*> Components;
        Actor->GetComponents(Components);
        for (UActorComponent* Component : Components)
        {
            if (Component) Objects.Add(Component);
        }
    }

    bool ReadGameplayAttribute(
        const UAbilitySystemComponent* ASC,
        const TArray<FName>& Names,
        float& OutValue)
    {
        if (!ASC) return false;

        for (const UAttributeSet* AttributeSet : ASC->GetSpawnedAttributes())
        {
            if (!AttributeSet) continue;

            for (const FName Name : Names)
            {
                FProperty* Property = AttributeSet->GetClass()->FindPropertyByName(Name);
                if (!Property) continue;

                const FStructProperty* StructProperty = CastField<FStructProperty>(Property);
                if (!StructProperty || StructProperty->Struct != FGameplayAttributeData::StaticStruct())
                {
                    continue;
                }

                const FGameplayAttribute Attribute(Property);
                if (!Attribute.IsValid()) continue;

                OutValue = ASC->GetNumericAttribute(Attribute);
                return true;
            }
        }

        return false;
    }

    UAbilitySystemComponent* ResolveASC(APawn* Pawn)
    {
        if (!Pawn) return nullptr;
        if (IAbilitySystemInterface* AbilityOwner = Cast<IAbilitySystemInterface>(Pawn))
        {
            return AbilityOwner->GetAbilitySystemComponent();
        }
        return nullptr;
    }

    FString CleanDefinitionLabel(FName DefinitionId)
    {
        FString Label = DefinitionId.ToString();
        Label.ReplaceInline(TEXT("_"), TEXT(" "));
        if (Label.StartsWith(TEXT("gu."), ESearchCase::IgnoreCase))
        {
            Label.RightChopInline(3);
        }
        return Label;
    }

    AWildGuWorldActor* FindViewedWildGu(const APlayerController* PC)
    {
        if (!PC || !PC->GetWorld()) return nullptr;

        FVector ViewLocation;
        FRotator ViewRotation;
        PC->GetPlayerViewPoint(ViewLocation, ViewRotation);
        const FVector Forward = ViewRotation.Vector();

        AWildGuWorldActor* Best = nullptr;
        float BestScore = -BIG_NUMBER;

        for (TActorIterator<AWildGuWorldActor> It(PC->GetWorld()); It; ++It)
        {
            AWildGuWorldActor* Candidate = *It;
            if (!Candidate || Candidate->IsActorBeingDestroyed()) continue;

            const FVector Delta = Candidate->GetActorLocation() - ViewLocation;
            const float Distance = Delta.Size();
            if (Distance > 650.0f || Distance < KINDA_SMALL_NUMBER) continue;

            const float Facing = FVector::DotProduct(Forward, Delta / Distance);
            if (Facing < 0.82f) continue;

            // Favor centered targets strongly, then distance.
            const float Score = Facing * 4.0f - Distance / 650.0f;
            if (Score > BestScore)
            {
                BestScore = Score;
                Best = Candidate;
            }
        }

        return Best;
    }

    void SetTextVisibility(UTextBlock* TextBlock, const bool bVisible)
    {
        if (TextBlock)
        {
            TextBlock->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
        }
    }

    void SetBarVisibility(UProgressBar* Bar, const bool bVisible)
    {
        if (Bar)
        {
            Bar->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
        }
    }
}

void UGuRuntimeHUDWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (!WidgetTree->RootWidget)
    {
        BuildWidgetTree();
    }

    SetIsFocusable(false);
    RefreshRuntimeState();
}

void UGuRuntimeHUDWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    const bool bEnabled = CVarGuFunctionalHUD.GetValueOnGameThread() != 0;
    SetVisibility(bEnabled ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    if (!bEnabled) return;

    RefreshAccumulator += FMath::Max(0.0f, InDeltaTime);
    if (RefreshAccumulator >= HudRefreshInterval)
    {
        RefreshAccumulator = 0.0f;
        RefreshRuntimeState();
    }
}

void UGuRuntimeHUDWidget::BuildWidgetTree()
{
    UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RuntimeHudRoot"));
    WidgetTree->RootWidget = Root;

    USizeBox* PanelSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("RuntimeHudPanelSize"));
    PanelSize->SetWidthOverride(365.0f);

    UCanvasPanelSlot* PanelCanvasSlot = Root->AddChildToCanvas(PanelSize);
    PanelCanvasSlot->SetAnchors(FAnchors(0.0f, 0.0f));
    PanelCanvasSlot->SetPosition(FVector2D(24.0f, 24.0f));
    PanelCanvasSlot->SetAutoSize(true);

    UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("RuntimeHudPanel"));
    Panel->SetPadding(FMargin(12.0f, 9.0f));
    Panel->SetBrushColor(FLinearColor(0.015f, 0.018f, 0.022f, 0.78f));
    PanelSize->SetContent(Panel);

    UVerticalBox* Stack = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("RuntimeHudStack"));
    Panel->SetContent(Stack);

    const auto AddText = [&](const TCHAR* Name, const FString& Initial, const float Size) -> UTextBlock*
    {
        UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), FName(Name));
        Text->SetText(FText::FromString(Initial));
        Text->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), FMath::RoundToInt(Size)));
        Text->SetColorAndOpacity(FSlateColor(FLinearColor(0.92f, 0.94f, 0.96f, 1.0f)));
        UVerticalBoxSlot* Slot = Stack->AddChildToVerticalBox(Text);
        Slot->SetPadding(FMargin(0.0f, 1.0f, 0.0f, 1.0f));
        return Text;
    };

    const auto AddBar = [&](const TCHAR* TextName, const TCHAR* BarName, TObjectPtr<UTextBlock>& OutText, TObjectPtr<UProgressBar>& OutBar)
    {
        OutText = AddText(TextName, TEXT(""), 12.0f);

        USizeBox* BarSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), FName(*FString::Printf(TEXT("%sSize"), BarName)));
        BarSize->SetHeightOverride(10.0f);

        OutBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), FName(BarName));
        OutBar->SetPercent(0.0f);
        BarSize->SetContent(OutBar);

        UVerticalBoxSlot* Slot = Stack->AddChildToVerticalBox(BarSize);
        Slot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 3.0f));
    };

    HeaderText = AddText(TEXT("Header"), TEXT("GU DAOIST"), 14.0f);
    AddBar(TEXT("HealthText"), TEXT("HealthBar"), HealthText, HealthBar);
    AddBar(TEXT("EssenceText"), TEXT("EssenceBar"), EssenceText, EssenceBar);
    AddBar(TEXT("StaminaText"), TEXT("StaminaBar"), StaminaText, StaminaBar);

    CultivationText = AddText(TEXT("CultivationText"), TEXT(""), 11.0f);
    MentalText = AddText(TEXT("MentalText"), TEXT(""), 11.0f);
    PressureText = AddText(TEXT("PressureText"), TEXT(""), 11.0f);
    StatusText = AddText(TEXT("StatusText"), TEXT(""), 10.0f);
    WorldTargetText = AddText(TEXT("WorldTargetText"), TEXT(""), 11.0f);

    CrosshairText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("RuntimeCrosshair"));
    CrosshairText->SetText(FText::FromString(TEXT("+")));
    CrosshairText->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 15));
    CrosshairText->SetColorAndOpacity(FSlateColor(FLinearColor(0.92f, 0.92f, 0.92f, 0.75f)));

    UCanvasPanelSlot* CrosshairSlot = Root->AddChildToCanvas(CrosshairText);
    CrosshairSlot->SetAnchors(FAnchors(0.5f, 0.5f));
    CrosshairSlot->SetAlignment(FVector2D(0.5f, 0.5f));
    CrosshairSlot->SetPosition(FVector2D::ZeroVector);
    CrosshairSlot->SetAutoSize(true);
}

void UGuRuntimeHUDWidget::RefreshRuntimeState()
{
    APlayerController* PC = GetOwningPlayer();
    APawn* Pawn = PC ? PC->GetPawn() : nullptr;
    APlayerState* PlayerState = PC ? PC->GetPlayerState<APlayerState>() : nullptr;
    if (!PC || !Pawn) return;

    UAbilitySystemComponent* ASC = ResolveASC(Pawn);

    // ---------------------------
    // Health
    // ---------------------------
    float Health = 0.0f;
    float MaxHealth = 0.0f;
    const bool bHasHealth =
        ReadGameplayAttribute(ASC, {TEXT("Health"), TEXT("Life"), TEXT("CurrentHealth")}, Health) &&
        ReadGameplayAttribute(ASC, {TEXT("MaxHealth"), TEXT("MaxLife")}, MaxHealth) &&
        MaxHealth > KINDA_SMALL_NUMBER;

    SetTextVisibility(HealthText, bHasHealth);
    SetBarVisibility(HealthBar, bHasHealth);
    if (bHasHealth)
    {
        HealthText->SetText(FText::FromString(FString::Printf(TEXT("Health  %.0f / %.0f"), Health, MaxHealth)));
        HealthBar->SetPercent(FMath::Clamp(Health / MaxHealth, 0.0f, 1.0f));
        HealthBar->SetFillColorAndOpacity(FLinearColor(0.62f, 0.16f, 0.16f, 1.0f));
    }

    // ---------------------------
    // Primeval essence
    // ---------------------------
    float Essence = 0.0f;
    float MaxEssence = 0.0f;
    const bool bHasEssence =
        ReadGameplayAttribute(ASC, {TEXT("PrimevalEssence"), TEXT("Essence")}, Essence) &&
        ReadGameplayAttribute(ASC, {TEXT("MaxPrimevalEssence"), TEXT("MaxEssence")}, MaxEssence) &&
        MaxEssence > KINDA_SMALL_NUMBER;

    SetTextVisibility(EssenceText, bHasEssence);
    SetBarVisibility(EssenceBar, bHasEssence);
    if (bHasEssence)
    {
        EssenceText->SetText(FText::FromString(FString::Printf(TEXT("Primeval Essence  %.0f / %.0f"), Essence, MaxEssence)));
        EssenceBar->SetPercent(FMath::Clamp(Essence / MaxEssence, 0.0f, 1.0f));
        EssenceBar->SetFillColorAndOpacity(FLinearColor(0.13f, 0.48f, 0.72f, 1.0f));
    }

    // ---------------------------
    // Generic reflected runtime values
    // ---------------------------
    TArray<UObject*> Objects;
    AddActorObjects(Pawn, Objects);
    AddActorObjects(PlayerState, Objects);

    double Stamina = 0.0;
    double MaxStamina = 0.0;
    bool bHasStamina =
        ReadNumericFromObjectSet(Objects, {TEXT("Stamina"), TEXT("CurrentStamina")}, Stamina) &&
        ReadNumericFromObjectSet(Objects, {TEXT("MaxStamina"), TEXT("StaminaCapacity")}, MaxStamina) &&
        MaxStamina > KINDA_SMALL_NUMBER;

    // GAS may own stamina in newer builds.
    if (!bHasStamina)
    {
        float GasStamina = 0.0f;
        float GasMaxStamina = 0.0f;
        bHasStamina =
            ReadGameplayAttribute(ASC, {TEXT("Stamina"), TEXT("CurrentStamina")}, GasStamina) &&
            ReadGameplayAttribute(ASC, {TEXT("MaxStamina"), TEXT("StaminaCapacity")}, GasMaxStamina) &&
            GasMaxStamina > KINDA_SMALL_NUMBER;
        if (bHasStamina)
        {
            Stamina = GasStamina;
            MaxStamina = GasMaxStamina;
        }
    }

    SetTextVisibility(StaminaText, bHasStamina);
    SetBarVisibility(StaminaBar, bHasStamina);
    if (bHasStamina)
    {
        StaminaText->SetText(FText::FromString(FString::Printf(TEXT("Stamina  %.0f / %.0f"), Stamina, MaxStamina)));
        StaminaBar->SetPercent(FMath::Clamp(static_cast<float>(Stamina / MaxStamina), 0.0f, 1.0f));
        StaminaBar->SetFillColorAndOpacity(FLinearColor(0.62f, 0.54f, 0.16f, 1.0f));
    }

    double Rank = 0.0;
    double Stones = 0.0;
    const bool bHasRank = ReadNumericFromObjectSet(
        Objects,
        {TEXT("Rank"), TEXT("CultivationRank"), TEXT("PlayerRank")},
        Rank);
    const bool bHasStones = ReadNumericFromObjectSet(
        Objects,
        {TEXT("PrimevalStones"), TEXT("SpiritStones"), TEXT("StoneCount"), TEXT("Stones")},
        Stones);

    FString Cultivation;
    if (bHasRank) Cultivation += FString::Printf(TEXT("Rank %d"), FMath::Max(0, FMath::RoundToInt(Rank)));
    if (bHasStones)
    {
        if (!Cultivation.IsEmpty()) Cultivation += TEXT("   |   ");
        Cultivation += FString::Printf(TEXT("Stones %.0f"), Stones);
    }

    CultivationText->SetText(FText::FromString(Cultivation));
    SetTextVisibility(CultivationText, !Cultivation.IsEmpty());

    // ---------------------------
    // Shared mental-control budget
    // ---------------------------
    bool bHasMental = false;
    FString MentalLine;

#if GU_RUNTIME_HUD_HAS_MENTAL_RESOURCE
    UMentalResourceComponent* Mental = Pawn->FindComponentByClass<UMentalResourceComponent>();
    if (!Mental && PlayerState)
    {
        Mental = PlayerState->FindComponentByClass<UMentalResourceComponent>();
    }

    if (Mental)
    {
        const FAttentionSnapshot Attention = Mental->GetAttentionSnapshot();
        MentalLine = FString::Printf(
            TEXT("Control  %.1f / %d   |   Focus cap %d   |   Mental %d"),
            Attention.Used,
            Attention.Capacity,
            Attention.FocusCapacity,
            Attention.MentalFoundation);
        bHasMental = true;
    }
#endif

    MentalText->SetText(FText::FromString(MentalLine));
    SetTextVisibility(MentalText, bHasMental);

    // ---------------------------
    // Aperture pressure, reflection only.
    // No dependency on an obsolete aperture class.
    // ---------------------------
    double Pressure = 0.0;
    double PressureMax = 0.0;
    const bool bHasPressure =
        ReadNumericFromObjectSet(
            Objects,
            {TEXT("AperturePressure"), TEXT("CurrentAperturePressure"), TEXT("GuPressure")},
            Pressure) &&
        ReadNumericFromObjectSet(
            Objects,
            {TEXT("AperturePressureCapacity"), TEXT("MaxAperturePressure"), TEXT("GuPressureCapacity")},
            PressureMax) &&
        PressureMax > KINDA_SMALL_NUMBER;

    if (bHasPressure)
    {
        PressureText->SetText(FText::FromString(
            FString::Printf(TEXT("Aperture Pressure  %.1f / %.1f"), Pressure, PressureMax)));
    }
    SetTextVisibility(PressureText, bHasPressure);

    // ---------------------------
    // Gameplay status tags
    // ---------------------------
    FString StatusLine;
    if (ASC)
    {
        FGameplayTagContainer OwnedTags;
        ASC->GetOwnedGameplayTags(OwnedTags);

        TArray<FString> Visible;
        for (const FGameplayTag& Tag : OwnedTags)
        {
            const FString Name = Tag.ToString();
            const bool bStatusLike =
                Name.StartsWith(TEXT("State."), ESearchCase::IgnoreCase) ||
                Name.StartsWith(TEXT("Status."), ESearchCase::IgnoreCase) ||
                Name.StartsWith(TEXT("Effect."), ESearchCase::IgnoreCase) ||
                Name.StartsWith(TEXT("Buff."), ESearchCase::IgnoreCase) ||
                Name.StartsWith(TEXT("Debuff."), ESearchCase::IgnoreCase);

            if (!bStatusLike) continue;

            Visible.Add(Name);
            if (Visible.Num() >= 4) break;
        }

        if (!Visible.IsEmpty())
        {
            StatusLine = FString::Printf(TEXT("Status: %s"), *FString::Join(Visible, TEXT("  |  ")));
        }
    }

    StatusText->SetText(FText::FromString(StatusLine));
    SetTextVisibility(StatusText, !StatusLine.IsEmpty());

    // ---------------------------
    // World Gu awareness
    // ---------------------------
    AWildGuWorldActor* ViewedWildGu = FindViewedWildGu(PC);
    if (ViewedWildGu)
    {
        const FString Name = CleanDefinitionLabel(ViewedWildGu->GetGuDefinitionId());
        const float DistanceM = FVector::Dist(Pawn->GetActorLocation(), ViewedWildGu->GetActorLocation()) / 100.0f;

        WorldTargetText->SetText(FText::FromString(
            FString::Printf(
                TEXT("Wild Gu nearby: %s   %.1f m"),
                *Name,
                DistanceM)));

        WorldTargetText->SetColorAndOpacity(
            FSlateColor(FLinearColor(0.78f, 0.86f, 0.48f, 1.0f)));
        SetTextVisibility(WorldTargetText, true);
    }
    else
    {
        SetTextVisibility(WorldTargetText, false);
    }
}
