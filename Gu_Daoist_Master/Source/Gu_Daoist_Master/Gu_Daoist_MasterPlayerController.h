// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "RefinementTypes.h"
#include "KillerMoveTypes.h"
#include "GuHUDTypes.h"
#include "Gu_Daoist_MasterPlayerController.generated.h"

class UInputMappingContext;
class UUserWidget;
class URefinementHUDWidget;
class UKillerMoveHUDWidget;
class UKillerMoveDefinition;
class UGuHUDTabsWidget;
class UGuSemanticsHUDWidget;

UCLASS(abstract, config="Game")
class GU_DAOIST_MASTER_API AGu_Daoist_MasterPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    AGu_Daoist_MasterPlayerController();

    UFUNCTION(BlueprintCallable, Category="Gu|Refinement") void RefineProcess();
    UFUNCTION(BlueprintCallable, Category="Gu|Refinement") void RefineHeat();
    UFUNCTION(BlueprintCallable, Category="Gu|Refinement") void RefineCool();
    UFUNCTION(BlueprintCallable, Category="Gu|Refinement") void RefineMerge();
    UFUNCTION(BlueprintCallable, Category="Gu|Refinement") void RefinePurify();
    UFUNCTION(BlueprintCallable, Category="Gu|Refinement") void RefineControl();
    UFUNCTION(BlueprintCallable, Category="Gu|Refinement") void RefineCondense();
    UFUNCTION(BlueprintCallable, Category="Gu|Refinement") void RefineAbort();

    /** Development harness used by the stock native HUD until real ingredient selection lands. */
    UFUNCTION(BlueprintCallable, Category="Gu|Refinement|Debug") void RefineDebugStart();
    UFUNCTION(BlueprintCallable, Category="Gu|Refinement|Debug") void RefineDebugAuto();
    UFUNCTION(BlueprintCallable, Category="Gu|Refinement|Debug") void RefineDebugTechnique();

    /** Starts the configured authored move, or the development move when none is configured. */
    UFUNCTION(BlueprintCallable, Category="Gu|Killer Move") void StartKillerMove();

    /** Starts the development choreography using owned aperture Gu. */
    UFUNCTION(BlueprintCallable, Category="Gu|Killer Move|Debug") void StartDebugKillerMove();
    UFUNCTION(BlueprintCallable, Category="Gu|Killer Move") void KillerMoveSlotInput(int32 SlotIndex, bool bPressed);
    UFUNCTION(BlueprintCallable, Category="Gu|Killer Move") void CancelKillerMove();

    /** Opens a HUD page, or closes it when the same tab is clicked again. */
    UFUNCTION(BlueprintCallable, Category="Gu|HUD") void ToggleGuHUDTab(EGuHUDTab Tab);
    UFUNCTION(BlueprintPure, Category="Gu|HUD") EGuHUDTab GetActiveGuHUDTab() const { return ActiveGuHUDTab; }

    /** Console harness: GuGenerate Data.Paths.Moon 1 12345 Offense */
    UFUNCTION(Exec)
    void GuGenerate(FString PathTag, int32 Rank, int32 Seed, FString RoleText);

    /** Activates the most recently generated physical Gu. */
    UFUNCTION(Exec)
    void GuUseLastGenerated();

protected:
    UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
    TArray<UInputMappingContext*> DefaultMappingContexts;

    UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
    TArray<UInputMappingContext*> MobileExcludedMappingContexts;

    UPROPERTY(EditAnywhere, Category="Input|Touch Controls")
    TSubclassOf<UUserWidget> MobileControlsWidgetClass;

    UPROPERTY()
    TObjectPtr<UUserWidget> MobileControlsWidget;

    UPROPERTY(EditAnywhere, Config, Category="Input|Touch Controls")
    bool bForceTouchControls = false;

    UPROPERTY(Transient)
    TObjectPtr<URefinementHUDWidget> RefinementHUDWidget;

    UPROPERTY(Transient)
    TObjectPtr<UKillerMoveHUDWidget> KillerMoveHUDWidget;

    UPROPERTY(Transient)
    TObjectPtr<UGuHUDTabsWidget> GuHUDTabsWidget;

    UPROPERTY(Transient)
    TObjectPtr<UGuSemanticsHUDWidget> GuSemanticsHUDWidget;

    /** Optional authored move used by the current Killer Move button until the learned-move selector lands. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Gu|Killer Move")
    TObjectPtr<UKillerMoveDefinition> TestKillerMoveDefinition;

    virtual void BeginPlay() override;
    virtual void SetupInputComponent() override;
    bool ShouldUseTouchControls() const;

private:
    EGuHUDTab ActiveGuHUDTab = EGuHUDTab::None;
    FGuid LastGeneratedGuEntityId;
    void ApplyGuHUDTabVisibility();
    void ExecuteGuGenerate(const FString& PathTag, int32 Rank, int32 Seed, const FString& RoleText);
    void ExecuteUseLastGenerated();

    void RequestRefinementVerb(ERefinementVerb Verb);
    void ExecuteRefinementVerb(ERefinementVerb Verb);
    void ExecuteAbortRefinement();
    void ExecuteDebugStart();
    void ExecuteDebugAuto();
    void ExecuteDebugTechnique();
    void ReportRefinementMessage(const FString& Message);

    void ExecuteStartKillerMove();
    void ExecuteStartDebugKillerMove();
    void ExecuteKillerMoveSlotInput(int32 SlotIndex, EKillerMoveInputEvent Event);
    void ExecuteCancelKillerMove();
    void ReportKillerMoveMessage(const FString& Message);

    void KillerSlot1Pressed();
    void KillerSlot1Released();
    void KillerSlot2Pressed();
    void KillerSlot2Released();
    void KillerSlot3Pressed();
    void KillerSlot3Released();
    void KillerSlot4Pressed();
    void KillerSlot4Released();

    UFUNCTION(Server, Reliable)
    void ServerUseRefinementVerb(ERefinementVerb Verb);

    UFUNCTION(Server, Reliable)
    void ServerAbortRefinement();

    UFUNCTION(Server, Reliable)
    void ServerRefineDebugStart();

    UFUNCTION(Server, Reliable)
    void ServerRefineDebugAuto();

    UFUNCTION(Server, Reliable)
    void ServerRefineDebugTechnique();

    UFUNCTION(Server, Reliable)
    void ServerStartKillerMove();

    UFUNCTION(Server, Reliable)
    void ServerStartDebugKillerMove();

    UFUNCTION(Server, Reliable)
    void ServerKillerMoveSlotInput(int32 SlotIndex, EKillerMoveInputEvent Event);

    UFUNCTION(Server, Reliable)
    void ServerCancelKillerMove();

    UFUNCTION(Server, Reliable)
    void ServerGuGenerate(const FString& PathTag, int32 Rank, int32 Seed, const FString& RoleText);

    UFUNCTION(Server, Reliable)
    void ServerUseLastGenerated();
};
