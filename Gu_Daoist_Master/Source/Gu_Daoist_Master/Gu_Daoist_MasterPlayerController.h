// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "RefinementTypes.h"
#include "Gu_Daoist_MasterPlayerController.generated.h"

class UInputMappingContext;
class UUserWidget;
class URefinementHUDWidget;

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

    virtual void BeginPlay() override;
    virtual void SetupInputComponent() override;
    bool ShouldUseTouchControls() const;

private:
    void RequestRefinementVerb(ERefinementVerb Verb);
    void ExecuteRefinementVerb(ERefinementVerb Verb);
    void ExecuteAbortRefinement();
    void ExecuteDebugStart();
    void ExecuteDebugAuto();
    void ExecuteDebugTechnique();
    void ReportRefinementMessage(const FString& Message);

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
};
