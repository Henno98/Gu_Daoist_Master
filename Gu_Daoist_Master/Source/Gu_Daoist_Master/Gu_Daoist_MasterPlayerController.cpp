// Copyright Epic Games, Inc. All Rights Reserved.

#include "Gu_Daoist_MasterPlayerController.h"

#include "Blueprint/UserWidget.h"
#include "Components/InputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "GuDefinitionRegistrySubsystem.h"
#include "GuEntitySubsystem.h"
#include "GuHUDTabsWidget.h"
#include "GuSemanticsHUDWidget.h"
#include "GuProceduralGeneratorSubsystem.h"
#include "Gu_Daoist_MasterCharacter.h"
#include "GuPlayerState.h"
#include "Gu_Daoist_Master.h"
#include "Gu_Daoist_MasterCameraManager.h"
#include "InputMappingContext.h"
#include "InputCoreTypes.h"
#include "KillerMoveDefinition.h"
#include "KillerMoveHUDWidget.h"
#include "KillerMoveSubsystem.h"
#include "RefinementHUDWidget.h"
#include "RefinementSubsystem.h"
#include "Widgets/Input/SVirtualJoystick.h"

AGu_Daoist_MasterPlayerController::AGu_Daoist_MasterPlayerController()
{
    PlayerCameraManagerClass = AGu_Daoist_MasterCameraManager::StaticClass();
}

void AGu_Daoist_MasterPlayerController::BeginPlay()
{
    Super::BeginPlay();

    if (ShouldUseTouchControls() && IsLocalPlayerController())
    {
        MobileControlsWidget = CreateWidget<UUserWidget>(this, MobileControlsWidgetClass);
        if (MobileControlsWidget)
        {
            MobileControlsWidget->AddToPlayerScreen(0);
        }
        else
        {
            UE_LOG(LogGu_Daoist_Master, Error, TEXT("Could not spawn mobile controls widget."));
        }
    }

    if (IsLocalPlayerController())
    {
        GuHUDTabsWidget = CreateWidget<UGuHUDTabsWidget>(this, UGuHUDTabsWidget::StaticClass());
        if (GuHUDTabsWidget)
        {
            GuHUDTabsWidget->AddToPlayerScreen(20);
            GuHUDTabsWidget->SetPositionInViewport(FVector2D(15.0f, 12.0f), false);
            GuHUDTabsWidget->SetDesiredSizeInViewport(FVector2D(460.0f, 44.0f));
        }

        GuSemanticsHUDWidget = CreateWidget<UGuSemanticsHUDWidget>(this, UGuSemanticsHUDWidget::StaticClass());
        if (GuSemanticsHUDWidget)
        {
            GuSemanticsHUDWidget->AddToPlayerScreen(5);
        }

        RefinementHUDWidget = CreateWidget<URefinementHUDWidget>(this, URefinementHUDWidget::StaticClass());
        if (RefinementHUDWidget)
        {
            RefinementHUDWidget->AddToPlayerScreen(6);
        }

        KillerMoveHUDWidget = CreateWidget<UKillerMoveHUDWidget>(this, UKillerMoveHUDWidget::StaticClass());
        if (KillerMoveHUDWidget)
        {
            KillerMoveHUDWidget->AddToPlayerScreen(7);
            KillerMoveHUDWidget->SetPositionInViewport(FVector2D(15.0f, 96.0f), false);
            KillerMoveHUDWidget->SetDesiredSizeInViewport(FVector2D(720.0f, 330.0f));
        }

        ActiveGuHUDTab = EGuHUDTab::None;
        ApplyGuHUDTabVisibility();

        bShowMouseCursor = true;
        bEnableClickEvents = true;
        bEnableMouseOverEvents = true;

        FInputModeGameAndUI InputMode;
        InputMode.SetHideCursorDuringCapture(false);
        SetInputMode(InputMode);
    }
}

void AGu_Daoist_MasterPlayerController::ToggleGuHUDTab(const EGuHUDTab Tab)
{
    if (!IsLocalPlayerController()) return;
    ActiveGuHUDTab = ActiveGuHUDTab == Tab ? EGuHUDTab::None : Tab;
    ApplyGuHUDTabVisibility();
}

void AGu_Daoist_MasterPlayerController::ApplyGuHUDTabVisibility()
{
    if (GuSemanticsHUDWidget)
    {
        GuSemanticsHUDWidget->SetVisibility(ActiveGuHUDTab == EGuHUDTab::Gu ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    }
    if (RefinementHUDWidget)
    {
        RefinementHUDWidget->SetVisibility(ActiveGuHUDTab == EGuHUDTab::Refinement ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    }
    if (KillerMoveHUDWidget)
    {
        KillerMoveHUDWidget->SetVisibility(ActiveGuHUDTab == EGuHUDTab::KillerMove ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    }
}

void AGu_Daoist_MasterPlayerController::GuGenerate(FString PathTag, const int32 Rank, const int32 Seed, FString RoleText)
{
    if (HasAuthority()) ExecuteGuGenerate(PathTag, Rank, Seed, RoleText);
    else ServerGuGenerate(PathTag, Rank, Seed, RoleText);
}

void AGu_Daoist_MasterPlayerController::ServerGuGenerate_Implementation(const FString& PathTag, const int32 Rank, const int32 Seed, const FString& RoleText)
{
    ExecuteGuGenerate(PathTag, Rank, Seed, RoleText);
}

void AGu_Daoist_MasterPlayerController::ExecuteGuGenerate(const FString& PathTag, const int32 Rank, const int32 Seed, const FString& RoleText)
{
    AGu_Daoist_MasterCharacter* PlayerCharacter = Cast<AGu_Daoist_MasterCharacter>(GetPawn());
    UGameInstance* GI = GetGameInstance();
    UGuProceduralGeneratorSubsystem* Generator = GI ? GI->GetSubsystem<UGuProceduralGeneratorSubsystem>() : nullptr;
    if (!PlayerCharacter || !Generator)
    {
        ClientMessage(TEXT("GuGenerate failed: playable character or procedural Gu subsystem is unavailable."));
        return;
    }

    const FGameplayTag RequestedPath = FGameplayTag::RequestGameplayTag(FName(*PathTag), false);
    if (!RequestedPath.IsValid())
    {
        ClientMessage(FString::Printf(TEXT("GuGenerate failed: '%s' is not a registered Gameplay Tag. Expected Data.Paths.*."), *PathTag));
        return;
    }

    EProceduralGuRole RequestedRole = EProceduralGuRole::Auto;
    if (!UGuProceduralGeneratorSubsystem::TryParseRole(RoleText, RequestedRole))
    {
        ClientMessage(FString::Printf(TEXT("GuGenerate failed: unknown role '%s'. Use Auto, Offense, Defense, Movement, Healing, Control, Investigation, Concealment, Resource, Refinement or Support."), *RoleText));
        return;
    }

    FProceduralGuGenerationRequest Request;
    Request.PrimaryPath = RequestedPath;
    Request.Rank = Rank;
    Request.Seed = Seed;
    Request.Role = RequestedRole;

    FProceduralGuGenerationResult Result;
    FString Error;
    if (!Generator->GenerateAndGrantGu(PlayerCharacter, Request, Result, Error))
    {
        ClientMessage(FString::Printf(TEXT("GuGenerate failed: %s"), *Error));
        return;
    }

    LastGeneratedGuEntityId = Result.EntityId;
    const FString Message = FString::Printf(
        TEXT("Generated %s | %s | seed=%d | id=%s | entity=%s"),
        *Result.Name,
        *Result.Summary,
        Result.Seed,
        *Result.DefinitionId.ToString(),
        *Result.EntityId.ToString());
    ClientMessage(Message);
    UE_LOG(LogTemp, Log, TEXT("%s"), *Message);
}

void AGu_Daoist_MasterPlayerController::GuUseLastGenerated()
{
    if (HasAuthority()) ExecuteUseLastGenerated();
    else ServerUseLastGenerated();
}

void AGu_Daoist_MasterPlayerController::ServerUseLastGenerated_Implementation()
{
    ExecuteUseLastGenerated();
}

void AGu_Daoist_MasterPlayerController::ExecuteUseLastGenerated()
{
    AGu_Daoist_MasterCharacter* PlayerCharacter = Cast<AGu_Daoist_MasterCharacter>(GetPawn());
    if (!PlayerCharacter || !LastGeneratedGuEntityId.IsValid())
    {
        ClientMessage(TEXT("GuUseLastGenerated: no generated Gu is currently selected."));
        return;
    }

    const bool bActivated = PlayerCharacter->ActivateGuEntity(LastGeneratedGuEntityId);
    ClientMessage(FString::Printf(
        TEXT("GuUseLastGenerated: %s (%s)"),
        bActivated ? TEXT("activated") : TEXT("activation failed"),
        *LastGeneratedGuEntityId.ToString()));
}

void AGu_Daoist_MasterPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();

    if (IsLocalPlayerController())
    {
        if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
        {
            for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
            {
                Subsystem->AddMappingContext(CurrentContext, 0);
            }
            if (!ShouldUseTouchControls())
            {
                for (UInputMappingContext* CurrentContext : MobileExcludedMappingContexts)
                {
                    Subsystem->AddMappingContext(CurrentContext, 0);
                }
            }
        }
    }

    if (InputComponent)
    {
        InputComponent->BindKey(EKeys::K, IE_Pressed, this, &AGu_Daoist_MasterPlayerController::StartKillerMove);
        InputComponent->BindKey(EKeys::One, IE_Pressed, this, &AGu_Daoist_MasterPlayerController::KillerSlot1Pressed);
        InputComponent->BindKey(EKeys::One, IE_Released, this, &AGu_Daoist_MasterPlayerController::KillerSlot1Released);
        InputComponent->BindKey(EKeys::Two, IE_Pressed, this, &AGu_Daoist_MasterPlayerController::KillerSlot2Pressed);
        InputComponent->BindKey(EKeys::Two, IE_Released, this, &AGu_Daoist_MasterPlayerController::KillerSlot2Released);
        InputComponent->BindKey(EKeys::Three, IE_Pressed, this, &AGu_Daoist_MasterPlayerController::KillerSlot3Pressed);
        InputComponent->BindKey(EKeys::Three, IE_Released, this, &AGu_Daoist_MasterPlayerController::KillerSlot3Released);
        InputComponent->BindKey(EKeys::Four, IE_Pressed, this, &AGu_Daoist_MasterPlayerController::KillerSlot4Pressed);
        InputComponent->BindKey(EKeys::Four, IE_Released, this, &AGu_Daoist_MasterPlayerController::KillerSlot4Released);
    }
}

bool AGu_Daoist_MasterPlayerController::ShouldUseTouchControls() const
{
    return SVirtualJoystick::ShouldDisplayTouchInterface() || bForceTouchControls;
}

void AGu_Daoist_MasterPlayerController::ReportRefinementMessage(const FString& Message)
{
    if (!Message.IsEmpty()) ClientMessage(Message);
}

void AGu_Daoist_MasterPlayerController::RequestRefinementVerb(const ERefinementVerb Verb)
{
    if (HasAuthority()) ExecuteRefinementVerb(Verb);
    else ServerUseRefinementVerb(Verb);
}

void AGu_Daoist_MasterPlayerController::ExecuteRefinementVerb(const ERefinementVerb Verb)
{
    AGuPlayerState* PS = GetPlayerState<AGuPlayerState>();
    URefinementSubsystem* Refinement = GetGameInstance() ? GetGameInstance()->GetSubsystem<URefinementSubsystem>() : nullptr;
    if (!PS || !Refinement)
    {
        ReportRefinementMessage(TEXT("Refinement domain is unavailable. Check that the GameMode uses GuPlayerState."));
        return;
    }

    FString Error;
    if (!Refinement->UseBasicRefinementAction(PS, Verb, Error)) ReportRefinementMessage(Error);
}

void AGu_Daoist_MasterPlayerController::RefineProcess() { RequestRefinementVerb(ERefinementVerb::Process); }
void AGu_Daoist_MasterPlayerController::RefineHeat() { RequestRefinementVerb(ERefinementVerb::Heat); }
void AGu_Daoist_MasterPlayerController::RefineCool() { RequestRefinementVerb(ERefinementVerb::Cool); }
void AGu_Daoist_MasterPlayerController::RefineMerge() { RequestRefinementVerb(ERefinementVerb::Merge); }
void AGu_Daoist_MasterPlayerController::RefinePurify() { RequestRefinementVerb(ERefinementVerb::Purify); }
void AGu_Daoist_MasterPlayerController::RefineControl() { RequestRefinementVerb(ERefinementVerb::Control); }
void AGu_Daoist_MasterPlayerController::RefineCondense() { RequestRefinementVerb(ERefinementVerb::Condense); }

void AGu_Daoist_MasterPlayerController::RefineAbort()
{
    if (HasAuthority()) ExecuteAbortRefinement();
    else ServerAbortRefinement();
}

void AGu_Daoist_MasterPlayerController::ExecuteAbortRefinement()
{
    AGuPlayerState* PS = GetPlayerState<AGuPlayerState>();
    URefinementSubsystem* Refinement = GetGameInstance() ? GetGameInstance()->GetSubsystem<URefinementSubsystem>() : nullptr;
    if (!PS || !Refinement) return;
    FString Error;
    if (!Refinement->AbortRefinementSession(PS, Error)) ReportRefinementMessage(Error);
}

void AGu_Daoist_MasterPlayerController::RefineDebugStart()
{
#if !UE_BUILD_SHIPPING
    if (HasAuthority()) ExecuteDebugStart(); else ServerRefineDebugStart();
#endif
}

void AGu_Daoist_MasterPlayerController::ExecuteDebugStart()
{
#if !UE_BUILD_SHIPPING
    AGuPlayerState* PS = GetPlayerState<AGuPlayerState>();
    UGameInstance* GI = GetGameInstance();
    URefinementSubsystem* Refinement = GI ? GI->GetSubsystem<URefinementSubsystem>() : nullptr;
    UGuEntitySubsystem* Entities = GI ? GI->GetSubsystem<UGuEntitySubsystem>() : nullptr;
    UGuDefinitionRegistrySubsystem* Registry = GI ? GI->GetSubsystem<UGuDefinitionRegistrySubsystem>() : nullptr;
    if (!PS || !Refinement || !Entities || !Registry) return;
    if (Refinement->HasActiveSession(PS->DomainCharacterId))
    {
        ReportRefinementMessage(TEXT("A refinement is already active."));
        return;
    }

    FRefinementSemanticProfile Foundation;
    Foundation.Paths = {{TEXT("Fire"), 1.65f}};
    Foundation.Properties = {{TEXT("heat"), 1.25f}, {TEXT("expansion"), .52f}, {TEXT("precision"), .34f}};
    Foundation.Attributes = {{TEXT("range"), .82f}, {TEXT("precision"), .6f}};
    Foundation.Templates = {{TEXT("projectile"), 1.0f}};
    Foundation.DaoMass = 1.45f;

    FRefinementSemanticProfile Catalyst;
    Catalyst.Paths = {{TEXT("Fire"), .78f}};
    Catalyst.Properties = {{TEXT("motion"), .72f}, {TEXT("flow"), .3f}};
    Catalyst.Attributes = {{TEXT("speed"), .55f}};
    Catalyst.DaoMass = .82f;

    const FGuid A = Entities->CreateMaterialLot(Foundation, TEXT("Debug Fire Foundation"), 3, TEXT("debug"), FGuid());
    const FGuid B = Entities->CreateMaterialLot(Catalyst, TEXT("Debug Swift Catalyst"), 2, TEXT("debug"), FGuid());
    FRefinementInputSelection FoundationSelection; FoundationSelection.EntityId = A; FoundationSelection.Quantity = 2;
    FRefinementInputSelection CatalystSelection; CatalystSelection.EntityId = B; CatalystSelection.Quantity = 1;

    FString Error;
    if (!Refinement->BeginRefinementSessionWithQuantities(PS, {FoundationSelection, CatalystSelection}, NAME_None, false, Error))
    {
        Entities->DestroyEntity(A);
        Entities->DestroyEntity(B);
        ReportRefinementMessage(Error);
        return;
    }

    const FName GuideId(TEXT("debug_cauldron_guide_gu"));
    if (!Registry->HasDefinition(GuideId))
    {
        FGuDefinitionRecord Guide;
        Guide.Id = GuideId;
        Guide.Name = TEXT("Debug Cauldron Guide Gu");
        Guide.Rank = 1;
        Guide.Path = TEXT("Refinement");
        Guide.Category = TEXT("Refinement");
        Guide.FunctionalRoles = {TEXT("Refinement")};
        Guide.Description = TEXT("Development-only Gu used to exercise native refinement assistance.");
        FGuMechanicSpec Mechanic; Mechanic.Type = TEXT("refinement_assistance"); Guide.Mechanics.Add(Mechanic);
        Guide.RefinementAssistance.bEnabled = true;
        Guide.RefinementAssistance.ProgressPercent = 18.0f;
        Guide.RefinementAssistance.StabilityPerAction = 3.0f;
        Guide.RefinementAssistance.ImpurityReductionPerAction = 2.0f;
        Guide.RefinementAssistance.QualityBonus = 3.0f;
        Guide.RefinementAssistance.ActionUses = 3;
        Guide.RefinementAssistance.Processes = {TEXT("Process"), TEXT("Purify"), TEXT("Control"), TEXT("Condense")};
        Guide.RefinementProfile.Paths = {{TEXT("Refinement"), 1.0f}};
        Guide.RefinementProfile.Properties = {{TEXT("precision"), .8f}, {TEXT("stability"), .7f}, {TEXT("adhesion"), .45f}};
        Guide.RefinementProfile.Attributes = {{TEXT("precision"), .7f}, {TEXT("stability"), .55f}};
        Guide.RefinementProfile.Templates = {{TEXT("refinement"), 1.0f}};
        Guide.RefinementProfile.DaoMass = .4f;
        FString RegisterError;
        Registry->RegisterDefinition(Guide, RegisterError, false);
    }

    const FGuid GuideEntity = Entities->CreateGuInstance(GuideId, PS->DomainCharacterId, EGuContainer::Aperture);
    if (GuideEntity.IsValid())
    {
        FString AttachError;
        if (!Refinement->AttachGuRefinementAssistant(PS, GuideEntity, AttachError)) ReportRefinementMessage(AttachError);
    }
#endif
}

void AGu_Daoist_MasterPlayerController::RefineDebugAuto()
{
#if !UE_BUILD_SHIPPING
    if (HasAuthority()) ExecuteDebugAuto(); else ServerRefineDebugAuto();
#endif
}

void AGu_Daoist_MasterPlayerController::ExecuteDebugAuto()
{
#if !UE_BUILD_SHIPPING
    AGuPlayerState* PS = GetPlayerState<AGuPlayerState>();
    URefinementSubsystem* Refinement = GetGameInstance() ? GetGameInstance()->GetSubsystem<URefinementSubsystem>() : nullptr;
    if (!PS || !Refinement) return;

    for (int32 Guard = 0; Guard < 160 && Refinement->HasActiveSession(PS->DomainCharacterId); ++Guard)
    {
        FRefinementSessionState State;
        if (!Refinement->GetDebugSessionState(PS->DomainCharacterId, State) || !State.HiddenProcedure.IsValidIndex(State.StepIndex)) break;
        const FRefinementProcedureStep& Step = State.HiddenProcedure[State.StepIndex];
        ERefinementVerb Verb = Step.PrimaryProcess;
        if (State.Impurities > Step.MaxImpurities - 2.0f) Verb = ERefinementVerb::Purify;
        else if (State.Temperature < Step.TargetTemperature.X) Verb = ERefinementVerb::Heat;
        else if (State.Temperature > Step.TargetTemperature.Y) Verb = ERefinementVerb::Cool;
        else if (State.Stability < State.MaxStability * .55f) Verb = ERefinementVerb::Control;

        FString Error;
        if (!Refinement->UseBasicRefinementAction(PS, Verb, Error))
        {
            ReportRefinementMessage(Error);
            break;
        }
    }
#endif
}

void AGu_Daoist_MasterPlayerController::RefineDebugTechnique()
{
#if !UE_BUILD_SHIPPING
    if (HasAuthority()) ExecuteDebugTechnique(); else ServerRefineDebugTechnique();
#endif
}

void AGu_Daoist_MasterPlayerController::ExecuteDebugTechnique()
{
#if !UE_BUILD_SHIPPING
    AGuPlayerState* PS = GetPlayerState<AGuPlayerState>();
    URefinementSubsystem* Refinement = GetGameInstance() ? GetGameInstance()->GetSubsystem<URefinementSubsystem>() : nullptr;
    if (!PS || !Refinement) return;

    FRefinementSessionState State;
    if (!Refinement->GetDebugSessionState(PS->DomainCharacterId, State))
    {
        ReportRefinementMessage(TEXT("No refinement session."));
        return;
    }

    for (const FRefinementAssistanceContribution& Assistance : State.Assistance)
    {
        if (Assistance.UsesRemaining <= 0) continue;
        FString Error;
        if (!Refinement->UseGuRefinementAssistant(PS, Assistance.SourceEntityId, Error)) ReportRefinementMessage(Error);
        return;
    }
    ReportRefinementMessage(TEXT("No prepared refinement-Gu technique remains."));
#endif
}

void AGu_Daoist_MasterPlayerController::ServerUseRefinementVerb_Implementation(ERefinementVerb Verb) { ExecuteRefinementVerb(Verb); }
void AGu_Daoist_MasterPlayerController::ServerAbortRefinement_Implementation() { ExecuteAbortRefinement(); }
void AGu_Daoist_MasterPlayerController::ServerRefineDebugStart_Implementation() { ExecuteDebugStart(); }
void AGu_Daoist_MasterPlayerController::ServerRefineDebugAuto_Implementation() { ExecuteDebugAuto(); }
void AGu_Daoist_MasterPlayerController::ServerRefineDebugTechnique_Implementation() { ExecuteDebugTechnique(); }


void AGu_Daoist_MasterPlayerController::ReportKillerMoveMessage(const FString& Message)
{
    if (!Message.IsEmpty()) ClientMessage(Message);
}

void AGu_Daoist_MasterPlayerController::StartKillerMove()
{
    if (HasAuthority()) ExecuteStartKillerMove();
    else ServerStartKillerMove();
}

void AGu_Daoist_MasterPlayerController::ExecuteStartKillerMove()
{
    AGuPlayerState* PS = GetPlayerState<AGuPlayerState>();
    UKillerMoveSubsystem* KillerMoves = GetGameInstance() ? GetGameInstance()->GetSubsystem<UKillerMoveSubsystem>() : nullptr;
    if (!PS || !KillerMoves)
    {
        ReportKillerMoveMessage(TEXT("Killer-move domain is unavailable."));
        return;
    }

    FString Error;
    if (TestKillerMoveDefinition)
    {
        if (!KillerMoves->BeginKillerMoveAsset(PS, TestKillerMoveDefinition, Error)) ReportKillerMoveMessage(Error);
        return;
    }

#if !UE_BUILD_SHIPPING
    if (!KillerMoves->BeginDebugKillerMove(PS, Error)) ReportKillerMoveMessage(Error);
#else
    ReportKillerMoveMessage(TEXT("No killer move is configured."));
#endif
}

void AGu_Daoist_MasterPlayerController::StartDebugKillerMove()
{
#if !UE_BUILD_SHIPPING
    if (HasAuthority()) ExecuteStartDebugKillerMove();
    else ServerStartDebugKillerMove();
#endif
}

void AGu_Daoist_MasterPlayerController::ExecuteStartDebugKillerMove()
{
#if !UE_BUILD_SHIPPING
    AGuPlayerState* PS = GetPlayerState<AGuPlayerState>();
    UKillerMoveSubsystem* KillerMoves = GetGameInstance() ? GetGameInstance()->GetSubsystem<UKillerMoveSubsystem>() : nullptr;
    if (!PS || !KillerMoves)
    {
        ReportKillerMoveMessage(TEXT("Killer-move domain is unavailable."));
        return;
    }
    FString Error;
    if (!KillerMoves->BeginDebugKillerMove(PS, Error)) ReportKillerMoveMessage(Error);
#endif
}

void AGu_Daoist_MasterPlayerController::KillerMoveSlotInput(const int32 SlotIndex, const bool bPressed)
{
    const EKillerMoveInputEvent Event = bPressed ? EKillerMoveInputEvent::Pressed : EKillerMoveInputEvent::Released;
    if (HasAuthority()) ExecuteKillerMoveSlotInput(SlotIndex, Event);
    else ServerKillerMoveSlotInput(SlotIndex, Event);
}

void AGu_Daoist_MasterPlayerController::ExecuteKillerMoveSlotInput(const int32 SlotIndex, const EKillerMoveInputEvent Event)
{
    AGuPlayerState* PS = GetPlayerState<AGuPlayerState>();
    UKillerMoveSubsystem* KillerMoves = GetGameInstance() ? GetGameInstance()->GetSubsystem<UKillerMoveSubsystem>() : nullptr;
    if (!PS || !KillerMoves) return;
    FString Error;
    if (!KillerMoves->SubmitInput(PS, SlotIndex, Event, Error) && !Error.IsEmpty()) ReportKillerMoveMessage(Error);
}

void AGu_Daoist_MasterPlayerController::CancelKillerMove()
{
    if (HasAuthority()) ExecuteCancelKillerMove();
    else ServerCancelKillerMove();
}

void AGu_Daoist_MasterPlayerController::ExecuteCancelKillerMove()
{
    AGuPlayerState* PS = GetPlayerState<AGuPlayerState>();
    UKillerMoveSubsystem* KillerMoves = GetGameInstance() ? GetGameInstance()->GetSubsystem<UKillerMoveSubsystem>() : nullptr;
    if (!PS || !KillerMoves) return;
    FString Error;
    if (!KillerMoves->CancelKillerMove(PS, Error) && !Error.IsEmpty()) ReportKillerMoveMessage(Error);
}

void AGu_Daoist_MasterPlayerController::KillerSlot1Pressed() { KillerMoveSlotInput(0, true); }
void AGu_Daoist_MasterPlayerController::KillerSlot1Released() { KillerMoveSlotInput(0, false); }
void AGu_Daoist_MasterPlayerController::KillerSlot2Pressed() { KillerMoveSlotInput(1, true); }
void AGu_Daoist_MasterPlayerController::KillerSlot2Released() { KillerMoveSlotInput(1, false); }
void AGu_Daoist_MasterPlayerController::KillerSlot3Pressed() { KillerMoveSlotInput(2, true); }
void AGu_Daoist_MasterPlayerController::KillerSlot3Released() { KillerMoveSlotInput(2, false); }
void AGu_Daoist_MasterPlayerController::KillerSlot4Pressed() { KillerMoveSlotInput(3, true); }
void AGu_Daoist_MasterPlayerController::KillerSlot4Released() { KillerMoveSlotInput(3, false); }

void AGu_Daoist_MasterPlayerController::ServerStartKillerMove_Implementation() { ExecuteStartKillerMove(); }
void AGu_Daoist_MasterPlayerController::ServerStartDebugKillerMove_Implementation() { ExecuteStartDebugKillerMove(); }
void AGu_Daoist_MasterPlayerController::ServerKillerMoveSlotInput_Implementation(const int32 SlotIndex, const EKillerMoveInputEvent Event) { ExecuteKillerMoveSlotInput(SlotIndex, Event); }
void AGu_Daoist_MasterPlayerController::ServerCancelKillerMove_Implementation() { ExecuteCancelKillerMove(); }
