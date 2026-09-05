// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "AS_GuMasterAttributeSet.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "GuProceduralGeneratorSubsystem.h"
#include "Gu_Daoist_MasterCharacter.generated.h"

class UInputComponent;
class USkeletalMeshComponent;
class UCameraComponent;
class UInputAction;
struct FInputActionValue;
class UGameplayEffect;
class UGameplayAbility;
class UGuDefinition;
class UGuInstanceObject;
class UGuWildGuCaptureComponent;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

UCLASS(abstract)
class AGu_Daoist_MasterCharacter : public ACharacter, public IAbilitySystemInterface
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
    USkeletalMeshComponent* FirstPersonMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
    UCameraComponent* FirstPersonCameraComponent;

    /** Physical wild-Gu capture / will-refinement network facade. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Gu|Capture", meta=(AllowPrivateAccess="true"))
    TObjectPtr<UGuWildGuCaptureComponent> WildGuCaptureComponent;

protected:
    virtual void BeginPlay() override;

    UPROPERTY(EditAnywhere, Category="Input")
    UInputAction* JumpAction;

    UPROPERTY(EditAnywhere, Category="Input")
    UInputAction* MoveAction;

    UPROPERTY(EditAnywhere, Category="Input")
    UInputAction* LookAction;

    UPROPERTY(EditAnywhere, Category="Input")
    UInputAction* MouseLookAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input")
    TObjectPtr<UInputAction> ActivateGuAction;

    /** DEVELOPMENT TEST: compile/register/create one procedural Gu and put it in this character's aperture. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input|Gu|Debug")
    TObjectPtr<UInputAction> DebugGenerateProceduralGuAction;

    /** DEVELOPMENT TEST: physically capture the nearest wild Gu inside the capture component's range. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input|Gu|Debug")
    TObjectPtr<UInputAction> DebugCaptureNearestWildGuAction;

    /** DEVELOPMENT TEST: instantly refine the last captured Gu's will and place that same FGuid in the aperture. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input|Gu|Debug")
    TObjectPtr<UInputAction> DebugInstantRefineLastCapturedGuAction;

    /** Request used by DebugGenerateProceduralGuAction. Leave PrimaryPath empty to test deterministic Auto Path selection. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input|Gu|Debug")
    FProceduralGuGenerationRequest DebugProceduralGuRequest;

    FGameplayAbilitySpecHandle TestGuAbilityHandle;

public:
    AGu_Daoist_MasterCharacter();

    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
    void ActivateTestGu(const FInputActionValue& Value);
    virtual void PossessedBy(AController* NewController) override;

    /** Grants the generic Gu GAS ability for one physical ECS Gu entity. No per-species ability class is required. */
    bool GrantGuAbilityForEntity(FGuid EntityId, UGuDefinition* Definition, FGameplayAbilitySpecHandle& OutHandle, FString& OutError);

    /** Server-side activation entry point used by generated/refined Gu and future aperture UI. */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Gu|ECS")
    bool ActivateGuEntity(FGuid EntityId);

    /**
     * Binds a physical Gu that entered this character's aperture at runtime to the generic
     * GAS bridge and refreshes the replicated active-Gu inventory projection.
     * Use this after refinement, capture-will completion, trade, or any other runtime move
     * that places an already-existing physical Gu into the aperture.
     */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Gu|ECS")
    bool SynchronizeOwnedApertureGu(FGuid EntityId, FString& OutError);

    /** Client-safe request to change the ordinary active Gu. The server validates aperture ownership. */
    UFUNCTION(BlueprintCallable, Category="Gu|ECS")
    void RequestSetActiveGuEntity(FGuid EntityId);

    /** Client-safe activation of the currently selected physical Gu. */
    UFUNCTION(BlueprintCallable, Category="Gu|ECS")
    void RequestActivateActiveGu();

    /**
     * Client-safe procedural species request. The owning client sends the request to
     * the authoritative Character, which compiles/registers the species, creates the
     * physical ECS Gu in the aperture, binds GAS, and refreshes Active Gu state.
     */
    UFUNCTION(BlueprintCallable, Category="Gu|Generation")
    void RequestGenerateProceduralGu(FProceduralGuGenerationRequest Request);

    UFUNCTION(BlueprintImplementableEvent, Category="Gu|Generation", meta=(DisplayName="Procedural Gu Generated"))
    void K2_OnProceduralGuGenerated(FGuid EntityId, FName DefinitionId, const FString& GuName, const FString& Summary);

    UFUNCTION(BlueprintImplementableEvent, Category="Gu|Generation", meta=(DisplayName="Procedural Gu Generation Failed"))
    void K2_OnProceduralGuGenerationFailed(const FString& Error);

    UFUNCTION(BlueprintPure, Category="Gu|ECS")
    FGuid GetActiveGuEntityId() const;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Abilities")
    TSubclassOf<UGameplayEffect> InitialAttributesEffect;

    UPROPERTY()
    TObjectPtr<UAS_GuMasterAttributeSet> AttributeSet;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Gu")
    TSubclassOf<UGameplayAbility> GuAbilityClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Gu")
    TObjectPtr<UGuDefinition> TestGuDefinition;

    /**
     * Additional authored Gu initially owned in the aperture. These are physicalized
     * into ECS for inventory/refinement/killer moves and receive the same generic
     * GAS ability bridge as every other physical Gu. The HUD chooses which one is active.
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Gu|Starting Inventory")
    TArray<TObjectPtr<UGuDefinition>> StartingGuDefinitions;

    UPROPERTY(BlueprintReadOnly, Category="Gu|ECS")
    TArray<FGuid> StartingGuEntityIds;

    /** Physical ECS entity backing the current test Gu ability. */
    UPROPERTY(BlueprintReadOnly, Category="Gu|ECS")
    FGuid TestGuEntityId;

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Abilities")
    TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

    /** Keeps the GAS SourceObject bridge alive for this granted ability. */
    UPROPERTY(Transient)
    TObjectPtr<UGuInstanceObject> TestGuInstanceObject;

    /** Strong references for runtime/generated Gu SourceObjects used by GAS ability specs. */
    UPROPERTY(Transient)
    TArray<TObjectPtr<UGuInstanceObject>> RuntimeGuInstanceObjects;

    /** Physical Gu entity -> generic GAS ability spec. Server-authoritative runtime index. */
    TMap<FGuid, FGameplayAbilitySpecHandle> RuntimeGuAbilityHandles;

    /** Rebinds persisted physical Gu entities to the generic GAS ability after possession/load. */
    void BindPersistedGuAbilities();

    /** Rebuilds the owner-only network projection from authoritative server ECS state. */
    void RefreshOwnedGuPublicState();

    bool SetActiveGuEntityAuthoritative(FGuid EntityId, FString& OutError);
    bool ActivateActiveGuAuthoritative(FString& OutError);

    UFUNCTION(Server, Reliable)
    void ServerSetActiveGuEntity(FGuid EntityId);

    UFUNCTION(Server, Reliable)
    void ServerActivateActiveGu();

    UFUNCTION(Server, Reliable)
    void ServerGenerateProceduralGu(const FProceduralGuGenerationRequest& Request);

    UFUNCTION(Client, Reliable)
    void ClientProceduralGuGenerated(FGuid EntityId, FName DefinitionId, const FString& GuName, const FString& Summary);

    UFUNCTION(Client, Reliable)
    void ClientProceduralGuGenerationFailed(const FString& Error);

    bool GenerateProceduralGuAuthoritative(
        const FProceduralGuGenerationRequest& Request,
        FProceduralGuGenerationResult& OutResult,
        FString& OutError);

    void MoveInput(const FInputActionValue& Value);
    void LookInput(const FInputActionValue& Value);

    void DebugGenerateProceduralGuInput(const FInputActionValue& Value);
    void DebugCaptureNearestWildGuInput(const FInputActionValue& Value);
    void DebugInstantRefineLastCapturedGuInput(const FInputActionValue& Value);

    UFUNCTION(BlueprintCallable, Category="Input")
    virtual void DoAim(float Yaw, float Pitch);

    UFUNCTION(BlueprintCallable, Category="Input")
    virtual void DoMove(float Right, float Forward);

    UFUNCTION(BlueprintCallable, Category="Input")
    virtual void DoJumpStart();

    UFUNCTION(BlueprintCallable, Category="Input")
    virtual void DoJumpEnd();

    virtual void SetupPlayerInputComponent(UInputComponent* InputComponent) override;

public:
    USkeletalMeshComponent* GetFirstPersonMesh() const { return FirstPersonMesh; }
    UCameraComponent* GetFirstPersonCameraComponent() const { return FirstPersonCameraComponent; }
};
