// Copyright Epic Games, Inc. All Rights Reserved.

#include "Gu_Daoist_MasterCharacter.h"

#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "Engine/GameInstance.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerState.h"
#include "GameplayAbilitySpec.h"
#include "GuDefinitionRegistrySubsystem.h"
#include "GuEntitySubsystem.h"
#include "GuInstanceObject.h"
#include "GuPlayerState.h"
#include "GuPersistenceSubsystem.h"
#include "GuWildGuCaptureComponent.h"
#include "Gu_Daoist_Master.h"
#include "InputActionValue.h"
#include "UGuDefinition.h"

namespace
{
    FString FormatGuInventorySemanticMap(const FString& Heading, const TMap<FName, float>& Values)
    {
        FString Out = Heading + TEXT("\n");
        if (Values.IsEmpty()) return Out + TEXT("  none\n");

        TArray<FName> Keys;
        Values.GenerateKeyArray(Keys);
        Keys.Sort([](const FName& A, const FName& B) { return A.ToString() < B.ToString(); });
        for (const FName Key : Keys)
        {
            Out += FString::Printf(TEXT("  %-22s %.3f\n"), *Key.ToString(), Values.FindRef(Key));
        }
        return Out;
    }

    FString BuildGuInventorySemanticsSummary(const FRefinementSemanticProfile& Semantic, const FDaoContaminationComponent* Contamination)
    {
        FString Body = FString::Printf(
            TEXT("Dao mass: %.3f%s\n\n"),
            Semantic.DaoMass,
            Semantic.bDerivedPropertySnapshot ? TEXT("  [derived property snapshot]") : TEXT(""));
        Body += FormatGuInventorySemanticMap(TEXT("PATHS"), Semantic.Paths) + TEXT("\n");
        Body += FormatGuInventorySemanticMap(TEXT("PROPERTIES"), Semantic.Properties) + TEXT("\n");
        Body += FormatGuInventorySemanticMap(TEXT("ATTRIBUTES"), Semantic.Attributes) + TEXT("\n");
        Body += FormatGuInventorySemanticMap(TEXT("TRAITS"), Semantic.Traits) + TEXT("\n");
        Body += FormatGuInventorySemanticMap(TEXT("TEMPLATES"), Semantic.Templates);

        if (Contamination)
        {
            Body += FString::Printf(TEXT("\nCONTAMINATION TOTAL\n  %.3f\n"), Contamination->Total);
        }
        return Body;
    }
}

void AGu_Daoist_MasterCharacter::BeginPlay()
{
    Super::BeginPlay();

    if (AbilitySystemComponent)
    {
        AbilitySystemComponent->InitAbilityActorInfo(this, this);
    }

    if (HasAuthority() && InitialAttributesEffect && AbilitySystemComponent)
    {
        FGameplayEffectContextHandle EffectContext = AbilitySystemComponent->MakeEffectContext();
        EffectContext.AddSourceObject(this);
        FGameplayEffectSpecHandle SpecHandle = AbilitySystemComponent->MakeOutgoingSpec(
            InitialAttributesEffect,
            1.0f,
            EffectContext);

        if (SpecHandle.IsValid())
        {
            AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
        }
    }

    if (AttributeSet)
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("Primeval Essence: %.1f / %.1f"),
            AttributeSet->GetPrimevalEssence(),
            AttributeSet->GetMaxPrimevalEssence());
    }
}

AGu_Daoist_MasterCharacter::AGu_Daoist_MasterCharacter()
{
    GetCapsuleComponent()->InitCapsuleSize(55.f, 96.0f);

    FirstPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("First Person Mesh"));
    FirstPersonMesh->SetupAttachment(GetMesh());
    FirstPersonMesh->SetOnlyOwnerSee(true);
    FirstPersonMesh->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;
    FirstPersonMesh->SetCollisionProfileName(FName("NoCollision"));

    FirstPersonCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("First Person Camera"));
    FirstPersonCameraComponent->SetupAttachment(FirstPersonMesh, FName("head"));
    FirstPersonCameraComponent->SetRelativeLocationAndRotation(
        FVector(-2.8f, 5.89f, 0.0f),
        FRotator(0.0f, 90.0f, -90.0f));
    FirstPersonCameraComponent->bUsePawnControlRotation = true;
    FirstPersonCameraComponent->bEnableFirstPersonFieldOfView = true;
    FirstPersonCameraComponent->bEnableFirstPersonScale = true;
    FirstPersonCameraComponent->FirstPersonFieldOfView = 70.0f;
    FirstPersonCameraComponent->FirstPersonScale = 0.6f;

    GetMesh()->SetOwnerNoSee(true);
    GetMesh()->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::WorldSpaceRepresentation;
    GetCapsuleComponent()->SetCapsuleSize(34.0f, 96.0f);

    GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;
    GetCharacterMovement()->AirControl = 0.5f;

    AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
    AbilitySystemComponent->SetIsReplicated(true);
    AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
    AttributeSet = CreateDefaultSubobject<UAS_GuMasterAttributeSet>(TEXT("AttributeSet"));

    WildGuCaptureComponent = CreateDefaultSubobject<UGuWildGuCaptureComponent>(TEXT("WildGuCaptureComponent"));
}

UAbilitySystemComponent* AGu_Daoist_MasterCharacter::GetAbilitySystemComponent() const
{
    return AbilitySystemComponent;
}

void AGu_Daoist_MasterCharacter::ActivateTestGu(const FInputActionValue& Value)
{
    RequestActivateActiveGu();
}

void AGu_Daoist_MasterCharacter::DebugGenerateProceduralGuInput(const FInputActionValue& Value)
{
#if UE_BUILD_SHIPPING
    return;
#else
    FProceduralGuGenerationRequest Request = DebugProceduralGuRequest;
    // Empty PrimaryPath deliberately means Auto Path. The procedural compiler resolves
    // a deterministic registered Data.Paths.* tag from the generation seed.

    UE_LOG(
        LogTemp,
        Log,
        TEXT("Debug input: request procedural Gu. Path=%s Rank=%d Role=%d Seed=%d Complexity=%d"),
        Request.PrimaryPath.IsValid() ? *Request.PrimaryPath.ToString() : TEXT("<Auto>"),
        Request.Rank,
        static_cast<int32>(Request.Role),
        Request.Seed,
        Request.Complexity);

    RequestGenerateProceduralGu(Request);
#endif
}

void AGu_Daoist_MasterCharacter::DebugCaptureNearestWildGuInput(const FInputActionValue& Value)
{
#if UE_BUILD_SHIPPING
    return;
#else
    if (!WildGuCaptureComponent)
    {
        UE_LOG(LogTemp, Warning, TEXT("Debug capture input ignored: WildGuCaptureComponent is unavailable."));
        return;
    }

    WildGuCaptureComponent->RequestCaptureNearestWildGu();
#endif
}

void AGu_Daoist_MasterCharacter::DebugInstantRefineLastCapturedGuInput(const FInputActionValue& Value)
{
#if UE_BUILD_SHIPPING
    return;
#else
    if (!WildGuCaptureComponent)
    {
        UE_LOG(LogTemp, Warning, TEXT("Debug will-refinement input ignored: WildGuCaptureComponent is unavailable."));
        return;
    }

    WildGuCaptureComponent->RequestDebugInstantRefineLastCapturedGu(EGuContainer::Aperture);
#endif
}

bool AGu_Daoist_MasterCharacter::GrantGuAbilityForEntity(
    const FGuid EntityId,
    UGuDefinition* Definition,
    FGameplayAbilitySpecHandle& OutHandle,
    FString& OutError)
{
    OutHandle = FGameplayAbilitySpecHandle();

    if (!HasAuthority())
    {
        OutError = TEXT("Gu ability grants are server-authoritative.");
        return false;
    }
    if (!AbilitySystemComponent || !GuAbilityClass)
    {
        OutError = TEXT("The character has no generic Gu ability class configured.");
        return false;
    }
    if (!EntityId.IsValid() || !IsValid(Definition))
    {
        OutError = TEXT("A valid physical Gu entity and executable Gu definition are required.");
        return false;
    }

    if (const FGameplayAbilitySpecHandle* ExistingHandle = RuntimeGuAbilityHandles.Find(EntityId))
    {
        OutHandle = *ExistingHandle;
        OutError.Reset();
        return OutHandle.IsValid();
    }

    UGuInstanceObject* InstanceBridge = NewObject<UGuInstanceObject>(this);
    if (!InstanceBridge)
    {
        OutError = TEXT("Could not allocate the Gu ECS/GAS instance bridge.");
        return false;
    }
    InstanceBridge->Initialize(EntityId, Definition);

    FGameplayAbilitySpec AbilitySpec(GuAbilityClass, 1, INDEX_NONE, InstanceBridge);
    const FGameplayAbilitySpecHandle NewHandle = AbilitySystemComponent->GiveAbility(AbilitySpec);
    if (!NewHandle.IsValid())
    {
        OutError = TEXT("GAS refused to grant the generic Gu ability.");
        return false;
    }

    RuntimeGuInstanceObjects.Add(InstanceBridge);
    RuntimeGuAbilityHandles.Add(EntityId, NewHandle);
    OutHandle = NewHandle;
    OutError.Reset();

    UE_LOG(
        LogTemp,
        Log,
        TEXT("Granted runtime Gu ability: %s -> entity %s"),
        *Definition->Name.ToString(),
        *EntityId.ToString());

    RefreshOwnedGuPublicState();
    return true;
}

bool AGu_Daoist_MasterCharacter::SynchronizeOwnedApertureGu(
    const FGuid EntityId,
    FString& OutError)
{
    if (!HasAuthority())
    {
        OutError = TEXT("Runtime Gu synchronization is server-authoritative.");
        return false;
    }
    if (!EntityId.IsValid())
    {
        OutError = TEXT("A valid physical Gu entity is required for runtime synchronization.");
        return false;
    }

    AGuPlayerState* DomainPlayerState = GetPlayerState<AGuPlayerState>();
    UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
    UGuDefinitionRegistrySubsystem* Registry = GI ? GI->GetSubsystem<UGuDefinitionRegistrySubsystem>() : nullptr;
    UGuEntitySubsystem* Entities = GI ? GI->GetSubsystem<UGuEntitySubsystem>() : nullptr;
    if (!DomainPlayerState || !Registry || !Entities || DomainPlayerState->DomainCharacterId.IsEmpty())
    {
        OutError = TEXT("Character Gu runtime state is not ready for synchronization.");
        return false;
    }

    const FOwnedByComponent* Ownership = Entities->GetOwnedBy(EntityId);
    const FGuPlacementComponent* Placement = Entities->GetGuPlacement(EntityId);
    const FGuInstanceComponent* Instance = Entities->GetGuInstance(EntityId);
    if (!Ownership || Ownership->OwnerId != DomainPlayerState->DomainCharacterId)
    {
        OutError = TEXT("The physical Gu is not owned by this character.");
        return false;
    }
    if (!Placement || Placement->Container != EGuContainer::Aperture)
    {
        OutError = TEXT("Only a Gu currently inside this character's aperture can be synchronized as an active Gu.");
        return false;
    }
    if (!Instance)
    {
        OutError = TEXT("The physical Gu has no Gu instance component.");
        return false;
    }

    UGuDefinition* ExecutableDefinition = const_cast<UGuDefinition*>(Registry->FindDefinitionAsset(Instance->DefinitionId));
    if (!ExecutableDefinition)
    {
        OutError = FString::Printf(
            TEXT("Gu definition '%s' has no executable UGuDefinition asset."),
            *Instance->DefinitionId.ToString());
        return false;
    }

    FGameplayAbilitySpecHandle Handle;
    if (!GrantGuAbilityForEntity(EntityId, ExecutableDefinition, Handle, OutError))
    {
        return false;
    }

    // GrantGuAbilityForEntity refreshes when it creates a new bridge. Refresh again here
    // deliberately so an already-bound entity that just re-entered the aperture is also projected.
    RefreshOwnedGuPublicState();

    UE_LOG(
        LogTemp,
        Log,
        TEXT("Synchronized runtime aperture Gu %s (%s) with GAS and active-Gu inventory."),
        *EntityId.ToString(),
        *Instance->DefinitionId.ToString());

    OutError.Reset();
    return true;
}

bool AGu_Daoist_MasterCharacter::ActivateGuEntity(const FGuid EntityId)
{
    if (!HasAuthority() || !AbilitySystemComponent || !EntityId.IsValid()) return false;

    AGuPlayerState* DomainPlayerState = GetPlayerState<AGuPlayerState>();
    UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
    UGuEntitySubsystem* Entities = GI ? GI->GetSubsystem<UGuEntitySubsystem>() : nullptr;
    if (!DomainPlayerState || !Entities || DomainPlayerState->DomainCharacterId.IsEmpty()) return false;

    const FOwnedByComponent* OwnerComponent = Entities->GetOwnedBy(EntityId);
    const FGuPlacementComponent* Placement = Entities->GetGuPlacement(EntityId);
    if (!OwnerComponent || OwnerComponent->OwnerId != DomainPlayerState->DomainCharacterId ||
        !Placement || Placement->Container != EGuContainer::Aperture)
    {
        UE_LOG(LogTemp, Warning, TEXT("Rejected Gu activation for non-owned/non-aperture entity %s."), *EntityId.ToString());
        return false;
    }

    FString UseError;
    if (!Entities->CanUseGu(EntityId, UseError))
    {
        UE_LOG(LogTemp, Warning, TEXT("Gu %s cannot be activated: %s"), *EntityId.ToString(), *UseError);
        return false;
    }

    const FGameplayAbilitySpecHandle* Handle = RuntimeGuAbilityHandles.Find(EntityId);
    if ((!Handle || !Handle->IsValid()) && EntityId == TestGuEntityId && TestGuAbilityHandle.IsValid())
    {
        Handle = &TestGuAbilityHandle;
    }
    if (!Handle || !Handle->IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("No runtime Gu ability is bound to entity %s."), *EntityId.ToString());
        return false;
    }

    const bool bActivated = AbilitySystemComponent->TryActivateAbility(*Handle);
    RefreshOwnedGuPublicState();
    return bActivated;
}

void AGu_Daoist_MasterCharacter::RequestSetActiveGuEntity(const FGuid EntityId)
{
    if (HasAuthority())
    {
        FString Error;
        if (!SetActiveGuEntityAuthoritative(EntityId, Error) && !Error.IsEmpty())
        {
            UE_LOG(LogTemp, Warning, TEXT("Could not select active Gu: %s"), *Error);
        }
        return;
    }

    ServerSetActiveGuEntity(EntityId);
}

void AGu_Daoist_MasterCharacter::RequestActivateActiveGu()
{
    if (HasAuthority())
    {
        FString Error;
        if (!ActivateActiveGuAuthoritative(Error) && !Error.IsEmpty())
        {
            UE_LOG(LogTemp, Warning, TEXT("Could not activate active Gu: %s"), *Error);
        }
        return;
    }

    ServerActivateActiveGu();
}

void AGu_Daoist_MasterCharacter::RequestGenerateProceduralGu(
    FProceduralGuGenerationRequest Request)
{
    if (HasAuthority())
    {
        FProceduralGuGenerationResult Result;
        FString Error;
        if (!GenerateProceduralGuAuthoritative(Request, Result, Error))
        {
            ClientProceduralGuGenerationFailed(Error);
            return;
        }

        ClientProceduralGuGenerated(
            Result.EntityId,
            Result.DefinitionId,
            Result.Name,
            Result.Summary);
        return;
    }

    ServerGenerateProceduralGu(Request);
}

FGuid AGu_Daoist_MasterCharacter::GetActiveGuEntityId() const
{
    const AGuPlayerState* DomainPlayerState = GetPlayerState<AGuPlayerState>();
    return DomainPlayerState ? DomainPlayerState->ActiveGuEntityId : FGuid();
}

void AGu_Daoist_MasterCharacter::ServerSetActiveGuEntity_Implementation(const FGuid EntityId)
{
    FString Error;
    if (!SetActiveGuEntityAuthoritative(EntityId, Error) && !Error.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("Server rejected active Gu selection: %s"), *Error);
    }
}

void AGu_Daoist_MasterCharacter::ServerActivateActiveGu_Implementation()
{
    FString Error;
    if (!ActivateActiveGuAuthoritative(Error) && !Error.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("Server rejected active Gu activation: %s"), *Error);
    }
}

void AGu_Daoist_MasterCharacter::ServerGenerateProceduralGu_Implementation(
    const FProceduralGuGenerationRequest& Request)
{
    FProceduralGuGenerationResult Result;
    FString Error;
    if (!GenerateProceduralGuAuthoritative(Request, Result, Error))
    {
        ClientProceduralGuGenerationFailed(Error);
        return;
    }

    ClientProceduralGuGenerated(
        Result.EntityId,
        Result.DefinitionId,
        Result.Name,
        Result.Summary);
}

void AGu_Daoist_MasterCharacter::ClientProceduralGuGenerated_Implementation(
    const FGuid EntityId,
    const FName DefinitionId,
    const FString& GuName,
    const FString& Summary)
{
    K2_OnProceduralGuGenerated(EntityId, DefinitionId, GuName, Summary);
}

void AGu_Daoist_MasterCharacter::ClientProceduralGuGenerationFailed_Implementation(
    const FString& Error)
{
    K2_OnProceduralGuGenerationFailed(Error);
}

bool AGu_Daoist_MasterCharacter::GenerateProceduralGuAuthoritative(
    const FProceduralGuGenerationRequest& Request,
    FProceduralGuGenerationResult& OutResult,
    FString& OutError)
{
    OutResult = FProceduralGuGenerationResult();
    OutError.Reset();

    if (!HasAuthority())
    {
        OutError = TEXT("Procedural Gu generation must execute on server authority.");
        return false;
    }

    UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
    UGuProceduralGeneratorSubsystem* Generator = GI
        ? GI->GetSubsystem<UGuProceduralGeneratorSubsystem>()
        : nullptr;
    if (!Generator)
    {
        OutError = TEXT("Procedural Gu generator subsystem is unavailable.");
        return false;
    }

    if (!Generator->GenerateAndGrantGu(this, Request, OutResult, OutError))
    {
        return false;
    }

#if !UE_BUILD_SHIPPING
    TArray<FString> MechanicNames;
    MechanicNames.Reserve(OutResult.MechanicTypes.Num());
    for (const FName MechanicType : OutResult.MechanicTypes)
    {
        MechanicNames.Add(MechanicType.ToString());
    }

    const FString GeneratedPath = OutResult.Definition && OutResult.Definition->Path.IsValid()
        ? OutResult.Definition->Path.ToString()
        : TEXT("<none>");

    UE_LOG(
        LogTemp,
        Log,
        TEXT("Procedural Gu generated: Name='%s' Definition=%s Entity=%s Path=%s Role=%d Seed=%d Structure=%s Mechanics=[%s] Summary='%s'"),
        *OutResult.Name,
        *OutResult.DefinitionId.ToString(),
        OutResult.EntityId.IsValid() ? *OutResult.EntityId.ToString() : TEXT("none"),
        *GeneratedPath,
        static_cast<int32>(OutResult.Role),
        OutResult.Seed,
        *OutResult.StructureSignature.ToString(),
        *FString::Join(MechanicNames, TEXT(", ")),
        *OutResult.Summary);
#else
    UE_LOG(
        LogTemp,
        Log,
        TEXT("Procedural Gu generated and granted: %s [%s] entity %s seed %d"),
        *OutResult.Name,
        *OutResult.DefinitionId.ToString(),
        OutResult.EntityId.IsValid() ? *OutResult.EntityId.ToString() : TEXT("none"),
        OutResult.Seed);
#endif

    OutError.Reset();
    return true;
}

bool AGu_Daoist_MasterCharacter::SetActiveGuEntityAuthoritative(const FGuid EntityId, FString& OutError)
{
    OutError.Reset();
    if (!HasAuthority())
    {
        OutError = TEXT("Active Gu selection is server-authoritative.");
        return false;
    }

    AGuPlayerState* DomainPlayerState = GetPlayerState<AGuPlayerState>();
    UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
    UGuEntitySubsystem* Entities = GI ? GI->GetSubsystem<UGuEntitySubsystem>() : nullptr;
    if (!DomainPlayerState || !Entities || DomainPlayerState->DomainCharacterId.IsEmpty())
    {
        OutError = TEXT("The player's Gu domain is not ready.");
        return false;
    }

    const FOwnedByComponent* OwnerComponent = Entities->GetOwnedBy(EntityId);
    const FGuPlacementComponent* Placement = Entities->GetGuPlacement(EntityId);
    if (!EntityId.IsValid() || !OwnerComponent || OwnerComponent->OwnerId != DomainPlayerState->DomainCharacterId ||
        !Placement || Placement->Container != EGuContainer::Aperture)
    {
        OutError = TEXT("That Gu is not in this player's aperture.");
        return false;
    }

    DomainPlayerState->SetActiveGuEntityId(EntityId);
    return true;
}

bool AGu_Daoist_MasterCharacter::ActivateActiveGuAuthoritative(FString& OutError)
{
    OutError.Reset();
    AGuPlayerState* DomainPlayerState = GetPlayerState<AGuPlayerState>();
    if (!DomainPlayerState || !DomainPlayerState->ActiveGuEntityId.IsValid())
    {
        OutError = TEXT("No active Gu is selected.");
        return false;
    }

    if (!ActivateGuEntity(DomainPlayerState->ActiveGuEntityId))
    {
        OutError = TEXT("The active Gu could not be activated.");
        return false;
    }
    return true;
}

void AGu_Daoist_MasterCharacter::PossessedBy(AController* NewController)
{
    Super::PossessedBy(NewController);

    if (AbilitySystemComponent)
    {
        AbilitySystemComponent->InitAbilityActorInfo(this, this);
    }

    if (!HasAuthority() || !AbilitySystemComponent || !GuAbilityClass)
    {
        return;
    }

    UObject* AbilitySourceObject = TestGuDefinition.Get();
    UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
    UGuDefinitionRegistrySubsystem* Registry = GameInstance
        ? GameInstance->GetSubsystem<UGuDefinitionRegistrySubsystem>()
        : nullptr;
    UGuEntitySubsystem* Entities = GameInstance
        ? GameInstance->GetSubsystem<UGuEntitySubsystem>()
        : nullptr;

    // Register authored species before loading runtime species. Procedural v1 can borrow
    // generic projectile/world carriers from authored Gu while reconstructing saved definitions.
    if (Registry)
    {
        FString PreRegisterError;
        if (TestGuDefinition)
        {
            Registry->RegisterDefinitionAsset(TestGuDefinition, PreRegisterError, true);
        }
        for (UGuDefinition* StartingDefinition : StartingGuDefinitions)
        {
            if (!StartingDefinition || StartingDefinition == TestGuDefinition) continue;
            PreRegisterError.Reset();
            Registry->RegisterDefinitionAsset(StartingDefinition, PreRegisterError, true);
        }
    }

    if (UGuPersistenceSubsystem* Persistence = GameInstance
            ? GameInstance->GetSubsystem<UGuPersistenceSubsystem>()
            : nullptr)
    {
        FString LoadError;
        if (!Persistence->EnsureLoaded(LoadError))
        {
            UE_LOG(LogTemp, Error, TEXT("Persistent Gu domain failed to load: %s"), *LoadError);
        }
    }

    if (Registry && Entities && TestGuDefinition)
    {
        FString RegisterError;
        if (Registry->RegisterDefinitionAsset(TestGuDefinition, RegisterError, true))
        {
            const FName DefinitionId = UGuDefinitionRegistrySubsystem::DefinitionIdForAsset(TestGuDefinition);
            AGuPlayerState* DomainPlayerState = GetPlayerState<AGuPlayerState>();
            const APlayerState* FallbackPlayerState = NewController ? NewController->GetPlayerState<APlayerState>() : nullptr;
            const FString FallbackOwnerId = FString::Printf(TEXT("player:%d"), FallbackPlayerState ? FallbackPlayerState->GetPlayerId() : 0);
            if (DomainPlayerState && DomainPlayerState->DomainCharacterId.IsEmpty())
            {
                DomainPlayerState->SetDomainCharacterId(FallbackOwnerId);
            }
            const FString OwnerId = DomainPlayerState && !DomainPlayerState->DomainCharacterId.IsEmpty()
                ? DomainPlayerState->DomainCharacterId
                : FallbackOwnerId;

            if (!Entities->FindOwnedGuInstance(
                    DefinitionId,
                    OwnerId,
                    EGuContainer::Aperture,
                    TestGuEntityId,
                    true))
            {
                TestGuEntityId = Entities->CreateGuInstance(DefinitionId, OwnerId, EGuContainer::Aperture);
            }

            if (TestGuEntityId.IsValid())
            {
                TestGuInstanceObject = NewObject<UGuInstanceObject>(this);
                TestGuInstanceObject->Initialize(TestGuEntityId, TestGuDefinition);
                AbilitySourceObject = TestGuInstanceObject;

                UE_LOG(
                    LogTemp,
                    Log,
                    TEXT("ECS Gu bound: %s -> %s"),
                    *TestGuDefinition->Name.ToString(),
                    *TestGuEntityId.ToString());
            }
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("Could not register %s with Gu ECS: %s"), *GetNameSafe(TestGuDefinition), *RegisterError);
        }
    }

    if (Registry && Entities && StartingGuDefinitions.Num() > 0)
    {
        AGuPlayerState* DomainPlayerState = GetPlayerState<AGuPlayerState>();
        const APlayerState* FallbackPlayerState = NewController ? NewController->GetPlayerState<APlayerState>() : nullptr;
        const FString FallbackOwnerId = FString::Printf(TEXT("player:%d"), FallbackPlayerState ? FallbackPlayerState->GetPlayerId() : 0);
        const FString OwnerId = DomainPlayerState && !DomainPlayerState->DomainCharacterId.IsEmpty()
            ? DomainPlayerState->DomainCharacterId
            : FallbackOwnerId;

        StartingGuEntityIds.Reset();
        if (TestGuEntityId.IsValid()) StartingGuEntityIds.Add(TestGuEntityId);

        for (UGuDefinition* StartingDefinition : StartingGuDefinitions)
        {
            if (!StartingDefinition || StartingDefinition == TestGuDefinition) continue;

            FString RegisterError;
            if (!Registry->RegisterDefinitionAsset(StartingDefinition, RegisterError, true))
            {
                UE_LOG(LogTemp, Error, TEXT("Could not register starting Gu %s: %s"), *GetNameSafe(StartingDefinition), *RegisterError);
                continue;
            }

            const FName DefinitionId = UGuDefinitionRegistrySubsystem::DefinitionIdForAsset(StartingDefinition);
            FGuid EntityId;
            if (!Entities->FindOwnedGuInstance(DefinitionId, OwnerId, EGuContainer::Aperture, EntityId, true))
            {
                EntityId = Entities->CreateGuInstance(DefinitionId, OwnerId, EGuContainer::Aperture);
            }
            if (EntityId.IsValid()) StartingGuEntityIds.AddUnique(EntityId);
        }

        UE_LOG(LogTemp, Log, TEXT("Starting aperture ECS Gu: %d"), StartingGuEntityIds.Num());
    }

    if (TestGuDefinition)
    {
        FGameplayAbilitySpec AbilitySpec(GuAbilityClass, 1, INDEX_NONE, AbilitySourceObject);
        TestGuAbilityHandle = AbilitySystemComponent->GiveAbility(AbilitySpec);
        if (TestGuEntityId.IsValid() && TestGuAbilityHandle.IsValid())
        {
            RuntimeGuAbilityHandles.Add(TestGuEntityId, TestGuAbilityHandle);
        }
    }

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("Gu setup - Authority: %s, Ability: %s, TestDefinition: %s, TestEntity: %s, StartingDefinitions: %d"),
        HasAuthority() ? TEXT("true") : TEXT("false"),
        *GetNameSafe(GuAbilityClass),
        *GetNameSafe(TestGuDefinition),
        TestGuEntityId.IsValid() ? *TestGuEntityId.ToString() : TEXT("none"),
        StartingGuDefinitions.Num());

    BindPersistedGuAbilities();
    RefreshOwnedGuPublicState();
}

void AGu_Daoist_MasterCharacter::BindPersistedGuAbilities()
{
    if (!HasAuthority() || !AbilitySystemComponent || !GuAbilityClass) return;

    UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
    UGuDefinitionRegistrySubsystem* Registry = GI ? GI->GetSubsystem<UGuDefinitionRegistrySubsystem>() : nullptr;
    UGuEntitySubsystem* Entities = GI ? GI->GetSubsystem<UGuEntitySubsystem>() : nullptr;
    if (!Registry || !Entities) return;

    AGuPlayerState* DomainPlayerState = GetPlayerState<AGuPlayerState>();
    const FString OwnerId = DomainPlayerState ? DomainPlayerState->DomainCharacterId : FString();
    if (OwnerId.IsEmpty()) return;

    int32 BoundCount = 0;
    for (const FGuid EntityId : Entities->QueryGuEntitiesForOwner(OwnerId, EGuContainer::Aperture, true))
    {
        if (!EntityId.IsValid() || EntityId == TestGuEntityId) continue;
        const FGuInstanceComponent* Instance = Entities->GetGuInstance(EntityId);
        if (!Instance) continue;

        UGuDefinition* ExecutableDefinition = const_cast<UGuDefinition*>(Registry->FindDefinitionAsset(Instance->DefinitionId));
        if (!ExecutableDefinition)
        {
            UE_LOG(
                LogTemp,
                Warning,
                TEXT("Persisted Gu entity %s references definition %s with no executable UGuDefinition."),
                *EntityId.ToString(),
                *Instance->DefinitionId.ToString());
            continue;
        }

        FGameplayAbilitySpecHandle Handle;
        FString GrantError;
        if (GrantGuAbilityForEntity(EntityId, ExecutableDefinition, Handle, GrantError))
        {
            ++BoundCount;
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("Could not bind persisted Gu %s: %s"), *EntityId.ToString(), *GrantError);
        }
    }

    UE_LOG(LogTemp, Log, TEXT("Bound %d persisted/aperture Gu entities to generic GAS abilities."), BoundCount);
}

void AGu_Daoist_MasterCharacter::RefreshOwnedGuPublicState()
{
    if (!HasAuthority()) return;

    AGuPlayerState* DomainPlayerState = GetPlayerState<AGuPlayerState>();
    UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
    UGuDefinitionRegistrySubsystem* Registry = GI ? GI->GetSubsystem<UGuDefinitionRegistrySubsystem>() : nullptr;
    UGuEntitySubsystem* Entities = GI ? GI->GetSubsystem<UGuEntitySubsystem>() : nullptr;
    if (!DomainPlayerState || !Registry || !Entities || DomainPlayerState->DomainCharacterId.IsEmpty()) return;

    const TArray<FGuid> OwnedIds = Entities->QueryGuEntitiesForOwner(
        DomainPlayerState->DomainCharacterId,
        EGuContainer::Aperture,
        false);

    TArray<FGuPublicInventoryEntry> Projection;
    Projection.Reserve(OwnedIds.Num());

    for (const FGuid EntityId : OwnedIds)
    {
        const FGuInstanceComponent* Instance = Entities->GetGuInstance(EntityId);
        if (!Instance) continue;

        const FGuDefinitionRecord* Definition = Registry->FindDefinition(Instance->DefinitionId);
        if (!Definition) continue;

        FGuPublicInventoryEntry Entry;
        Entry.EntityId = EntityId;
        Entry.DefinitionId = Instance->DefinitionId;
        Entry.Name = Definition->Name;
        Entry.Rank = Definition->Rank;
        Entry.Path = Definition->Path;

        if (const FGuConditionComponent* Condition = Entities->GetGuCondition(EntityId))
        {
            Entry.bAlive = Condition->bAlive;
            Entry.Durability = Condition->Durability;
            Entry.Quality = Condition->Quality;
            Entry.ActivationCount = Condition->ActivationCount;
        }
        if (const FGuNourishmentComponent* Nourishment = Entities->GetGuNourishment(EntityId))
        {
            Entry.Hunger = Nourishment->Hunger;
            Entry.FoodKey = Nourishment->FoodKey;
            Entry.FeedingIntervalHours = Nourishment->IntervalHours;
        }
        if (const FGuChargesComponent* Charges = Entities->GetGuCharges(EntityId))
        {
            Entry.RemainingCharges = Charges->Remaining;
        }

        FRefinementSemanticSnapshot Snapshot;
        if (Entities->GetRefinementSemanticSnapshot(EntityId, Snapshot) && Snapshot.EntityId.IsValid())
        {
            Entry.SemanticsSummary = BuildGuInventorySemanticsSummary(Snapshot.Semantic, &Snapshot.Contamination);
        }
        else
        {
            Entry.SemanticsSummary = BuildGuInventorySemanticsSummary(Definition->RefinementProfile, nullptr);
        }

        Projection.Add(MoveTemp(Entry));
    }

    Projection.Sort([](const FGuPublicInventoryEntry& A, const FGuPublicInventoryEntry& B)
    {
        const int32 NameCompare = A.Name.Compare(B.Name, ESearchCase::IgnoreCase);
        return NameCompare == 0 ? A.EntityId.ToString() < B.EntityId.ToString() : NameCompare < 0;
    });

    DomainPlayerState->SetOwnedGuInventory(Projection);

    const bool bActiveStillOwned = Projection.ContainsByPredicate([DomainPlayerState](const FGuPublicInventoryEntry& Entry)
    {
        return Entry.EntityId == DomainPlayerState->ActiveGuEntityId;
    });

    if (!bActiveStillOwned)
    {
        FGuid DefaultActive;
        if (TestGuEntityId.IsValid() && Projection.ContainsByPredicate([this](const FGuPublicInventoryEntry& Entry)
            { return Entry.EntityId == TestGuEntityId; }))
        {
            DefaultActive = TestGuEntityId;
        }
        else if (!Projection.IsEmpty())
        {
            DefaultActive = Projection[0].EntityId;
        }
        DomainPlayerState->SetActiveGuEntityId(DefaultActive);
    }
}

void AGu_Daoist_MasterCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
    {
        EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &AGu_Daoist_MasterCharacter::DoJumpStart);
        EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &AGu_Daoist_MasterCharacter::DoJumpEnd);
        EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AGu_Daoist_MasterCharacter::MoveInput);
        EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AGu_Daoist_MasterCharacter::LookInput);
        EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &AGu_Daoist_MasterCharacter::LookInput);

        if (ActivateGuAction)
        {
            EnhancedInputComponent->BindAction(
                ActivateGuAction,
                ETriggerEvent::Started,
                this,
                &AGu_Daoist_MasterCharacter::ActivateTestGu);
        }

        if (DebugGenerateProceduralGuAction)
        {
            EnhancedInputComponent->BindAction(
                DebugGenerateProceduralGuAction,
                ETriggerEvent::Started,
                this,
                &AGu_Daoist_MasterCharacter::DebugGenerateProceduralGuInput);
        }

        if (DebugCaptureNearestWildGuAction)
        {
            EnhancedInputComponent->BindAction(
                DebugCaptureNearestWildGuAction,
                ETriggerEvent::Started,
                this,
                &AGu_Daoist_MasterCharacter::DebugCaptureNearestWildGuInput);
        }

        if (DebugInstantRefineLastCapturedGuAction)
        {
            EnhancedInputComponent->BindAction(
                DebugInstantRefineLastCapturedGuAction,
                ETriggerEvent::Started,
                this,
                &AGu_Daoist_MasterCharacter::DebugInstantRefineLastCapturedGuInput);
        }
    }
    else
    {
        UE_LOG(
            LogGu_Daoist_Master,
            Error,
            TEXT("'%s' Failed to find an Enhanced Input Component!"),
            *GetNameSafe(this));
    }
}

void AGu_Daoist_MasterCharacter::MoveInput(const FInputActionValue& Value)
{
    const FVector2D MovementVector = Value.Get<FVector2D>();
    DoMove(MovementVector.X, MovementVector.Y);
}

void AGu_Daoist_MasterCharacter::LookInput(const FInputActionValue& Value)
{
    const FVector2D LookAxisVector = Value.Get<FVector2D>();
    DoAim(LookAxisVector.X, LookAxisVector.Y);
}

void AGu_Daoist_MasterCharacter::DoAim(float Yaw, float Pitch)
{
    if (GetController())
    {
        AddControllerYawInput(Yaw);
        AddControllerPitchInput(Pitch);
    }
}

void AGu_Daoist_MasterCharacter::DoMove(float Right, float Forward)
{
    if (GetController())
    {
        AddMovementInput(GetActorRightVector(), Right);
        AddMovementInput(GetActorForwardVector(), Forward);
    }
}

void AGu_Daoist_MasterCharacter::DoJumpStart()
{
    Jump();
}

void AGu_Daoist_MasterCharacter::DoJumpEnd()
{
    StopJumping();
}
