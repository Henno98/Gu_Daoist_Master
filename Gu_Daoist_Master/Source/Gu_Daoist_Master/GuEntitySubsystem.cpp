#include "GuEntitySubsystem.h"
#include "GuDefinitionRegistrySubsystem.h"
#include "GuRulesLibrary.h"
#include "GuPersistenceSubsystem.h"

int64 UGuEntitySubsystem::NowUnixMs()
{
    return FDateTime::UtcNow().ToUnixTimestamp() * 1000LL;
}

bool UGuEntitySubsystem::HasDomainAuthority() const
{
    const UWorld* World = GetWorld();
    return !World || World->GetNetMode() != NM_Client;
}

void UGuEntitySubsystem::RequestPersistentSave()
{
    if (UGameInstance* GI = GetGameInstance())
    {
        if (UGuPersistenceSubsystem* Persistence = GI->GetSubsystem<UGuPersistenceSubsystem>())
        {
            Persistence->RequestAutosave();
        }
    }
}

FGuid UGuEntitySubsystem::AllocateEntityId(const FGuid* RequestedId) const
{
    if (RequestedId && RequestedId->IsValid() && !Entities.Contains(*RequestedId)) return *RequestedId;
    FGuid Id;
    do { Id = FGuid::NewGuid(); } while (Entities.Contains(Id));
    return Id;
}

void UGuEntitySubsystem::AttachRefinementSemantics(
    const FGuid EntityId,
    const FRefinementSemanticProfile& InProfile,
    const ERefinableKind Kind,
    const FName SourceId,
    const FName DefinitionId)
{
    FRefinementSemanticProfile Profile = InProfile;
    UGuRulesLibrary::NormalizeSemanticProfile(Profile);

    FRefinableEntityComponent Refinable;
    Refinable.Kind = Kind;
    Refinable.SourceId = SourceId;
    Refinable.DefinitionId = DefinitionId;
    Refinables.Add(EntityId, Refinable);

    FDaoMarkProfileComponent Dao;
    Dao.Paths = Profile.Paths;
    Dao.DaoMass = Profile.DaoMass;
    DaoMarks.Add(EntityId, MoveTemp(Dao));

    FRefinementPropertiesComponent Props;
    Props.Scores = Profile.Properties;
    RefinementProperties.Add(EntityId, MoveTemp(Props));

    FRefinementAttributesComponent Attrs;
    Attrs.Scores = Profile.Attributes;
    RefinementAttributes.Add(EntityId, MoveTemp(Attrs));

    FRefinementTraitsComponent Traits;
    Traits.Scores = Profile.Traits;
    RefinementTraits.Add(EntityId, MoveTemp(Traits));

    FRefinementTemplatesComponent Templates;
    Templates.Scores = Profile.Templates;
    RefinementTemplates.Add(EntityId, MoveTemp(Templates));
}

FGuid UGuEntitySubsystem::CreateRefinableEntity(
    const FRefinementSemanticProfile& Profile,
    const ERefinableKind Kind,
    const FName SourceId,
    const FName DefinitionId)
{
    if (!HasDomainAuthority()) return FGuid();
    const FGuid EntityId = AllocateEntityId();
    Entities.Add(EntityId);
    AttachRefinementSemantics(EntityId, Profile, Kind, SourceId, DefinitionId);
    RequestPersistentSave();
    return EntityId;
}

FGuid UGuEntitySubsystem::CreateMaterialLot(
    const FRefinementSemanticProfile& Profile,
    const FName Item,
    const int32 Quantity,
    const FName SourceKind,
    const FGuid SourceEntityId)
{
    if (!HasDomainAuthority() || Item.IsNone() || Quantity <= 0) return FGuid();
    const FGuid EntityId = AllocateEntityId();
    Entities.Add(EntityId);
    AttachRefinementSemantics(EntityId, Profile, ERefinableKind::Material, Item, Item);

    FMaterialLotComponent Lot;
    Lot.LotId = FString::Printf(TEXT("lot_%s"), *EntityId.ToString(EGuidFormats::Digits));
    Lot.Item = Item;
    Lot.Quantity = Quantity;
    Lot.SourceEntityId = SourceEntityId;
    Lot.SourceKind = SourceKind;
    Lot.CreatedAtUnixMs = NowUnixMs();
    MaterialLots.Add(EntityId, MoveTemp(Lot));
    RequestPersistentSave();
    return EntityId;
}

bool UGuEntitySubsystem::ConsumeMaterialQuantity(const FGuid EntityId, const int32 Quantity, FString& OutError)
{
    if (!HasDomainAuthority()) { OutError=TEXT("Material mutations are server-authoritative."); return false; }
    FMaterialLotComponent* Lot=MaterialLots.Find(EntityId);
    if (!Lot) { OutError=TEXT("The selected entity is not a material lot."); return false; }
    const int32 Requested=FMath::Max(0,Quantity);
    if (Requested<=0) { OutError.Reset(); return true; }
    if (Lot->Quantity<Requested)
    {
        OutError=FString::Printf(TEXT("Material lot contains %d units but %d were requested."),Lot->Quantity,Requested);
        return false;
    }
    Lot->Quantity-=Requested;
    if (Lot->Quantity<=0) DestroyEntity(EntityId);
    else RequestPersistentSave();
    OutError.Reset();
    return true;
}

FGuid UGuEntitySubsystem::CreateGuInstance(const FName DefinitionId, const FString& OwnerId, const EGuContainer Container)
{
    if (!HasDomainAuthority()) return FGuid();
    FGuid EntityId;
    FString Error;
    if (!CreateGuInstanceWithId(DefinitionId, OwnerId, Container, FGuid(), EntityId, Error))
    {
        UE_LOG(LogTemp, Error, TEXT("CreateGuInstance failed: %s"), *Error);
        return FGuid();
    }
    return EntityId;
}

bool UGuEntitySubsystem::CreateGuInstanceWithId(
    const FName DefinitionId,
    const FString& OwnerId,
    const EGuContainer Container,
    const FGuid& RequestedId,
    FGuid& OutEntityId,
    FString& OutError)
{
    if (!HasDomainAuthority())
    {
        OutError = TEXT("Gu domain mutations are server-authoritative.");
        return false;
    }

    const UGameInstance* GI = GetGameInstance();
    const UGuDefinitionRegistrySubsystem* Registry = GI ? GI->GetSubsystem<UGuDefinitionRegistrySubsystem>() : nullptr;
    const FGuDefinitionRecord* Definition = Registry ? Registry->FindDefinition(DefinitionId) : nullptr;
    if (!Definition)
    {
        OutError = FString::Printf(TEXT("Unknown Gu definition '%s'."), *DefinitionId.ToString());
        return false;
    }

    if (Definition->bUnique)
    {
        for (const TPair<FGuid, FGuInstanceComponent>& Pair : GuInstances)
        {
            const FGuConditionComponent* ExistingCondition = GuConditions.Find(Pair.Key);
            if (Pair.Value.DefinitionId == Definition->Id && ExistingCondition && ExistingCondition->bAlive)
            {
                OutError = FString::Printf(TEXT("%s is unique and already has a living instance."), *Definition->Name);
                return false;
            }
        }
    }

    OutEntityId = AllocateEntityId(RequestedId.IsValid() ? &RequestedId : nullptr);
    Entities.Add(OutEntityId);

    FGuInstanceComponent Instance;
    Instance.DefinitionId = Definition->Id;
    Instance.CreatedAtUnixMs = NowUnixMs();
    GuInstances.Add(OutEntityId, Instance);

    GuConditions.Add(OutEntityId, FGuConditionComponent());

    FGuVisualStateComponent VisualState;
    VisualState.Animation = Definition->Appearance.Animation.Idle;
    VisualState.Condition = FName(TEXT("healthy"));
    GuVisualStates.Add(OutEntityId, VisualState);

    FGuNourishmentComponent Nourishment;
    Nourishment.LastUpdateUnixMs = NowUnixMs();
    Nourishment.FoodKey = Definition->Feeding.FoodKey;
    Nourishment.IntervalHours = Definition->Feeding.IntervalHours;
    GuNourishment.Add(OutEntityId, Nourishment);

    FOwnedByComponent Owner;
    Owner.OwnerId = OwnerId;
    Owners.Add(OutEntityId, Owner);

    FGuStatusComponent Status;
    Status.States = { FName(TEXT("Refined / owned")), FName(TEXT("Active")) };
    Status.HolderId = OwnerId;
    GuStatus.Add(OutEntityId, MoveTemp(Status));

    FGuLifecycleComponent Lifecycle;
    Lifecycle.bConsumable = Definition->Lifecycle.bConsumable;
    Lifecycle.ConsumeOn = Definition->Lifecycle.ConsumeOn;
    Lifecycle.MaxCharges = Definition->Lifecycle.Charges;
    Lifecycle.ConsumedForm = Definition->Lifecycle.ConsumedForm;
    GuLifecycles.Add(OutEntityId, MoveTemp(Lifecycle));

    FGuChargesComponent Charges;
    Charges.Remaining = Definition->Lifecycle.Charges;
    GuCharges.Add(OutEntityId, Charges);

    FGuPlacementComponent Placement;
    Placement.Container = Container;
    GuPlacements.Add(OutEntityId, Placement);

    AttachRefinementSemantics(
        OutEntityId,
        Definition->RefinementProfile,
        ERefinableKind::Gu,
        FName(*Definition->Name),
        Definition->Id);

    if (Definition->RefinementAssistance.bEnabled)
    {
        FRefinementAssistantComponent Assistant;
        Assistant.ProgressPercent = Definition->RefinementAssistance.ProgressPercent;
        Assistant.StabilityPerAction = Definition->RefinementAssistance.StabilityPerAction;
        Assistant.ImpurityReductionPerAction = Definition->RefinementAssistance.ImpurityReductionPerAction;
        Assistant.QualityBonus = Definition->RefinementAssistance.QualityBonus;
        Assistant.ActionUses = Definition->RefinementAssistance.ActionUses;
        Assistant.Processes = Definition->RefinementAssistance.Processes;
        RefinementAssistants.Add(OutEntityId, MoveTemp(Assistant));
    }

    OutError.Reset();
    RequestPersistentSave();
    return true;
}

bool UGuEntitySubsystem::DestroyEntity(const FGuid EntityId)
{
    if (!HasDomainAuthority()) return false;
    if (!Entities.Remove(EntityId)) return false;
    Refinables.Remove(EntityId);
    MaterialLots.Remove(EntityId);
    DaoMarks.Remove(EntityId);
    RefinementProperties.Remove(EntityId);
    RefinementAttributes.Remove(EntityId);
    RefinementTraits.Remove(EntityId);
    RefinementTemplates.Remove(EntityId);
    DaoContamination.Remove(EntityId);
    GuInstances.Remove(EntityId);
    GuConditions.Remove(EntityId);
    GuVisualStates.Remove(EntityId);
    GuNourishment.Remove(EntityId);
    Owners.Remove(EntityId);
    GuStatus.Remove(EntityId);
    GuLifecycles.Remove(EntityId);
    GuCharges.Remove(EntityId);
    GuPlacements.Remove(EntityId);
    EnslavementControllers.Remove(EntityId);
    MultitaskingBoosts.Remove(EntityId);
    RefinementAssistants.Remove(EntityId);
    RequestPersistentSave();
    return true;
}

bool UGuEntitySubsystem::GetRefinementSemanticSnapshot(const FGuid EntityId, FRefinementSemanticSnapshot& OutSnapshot) const
{
    if (!Entities.Contains(EntityId)) return false;
    const FRefinableEntityComponent* Refinable = Refinables.Find(EntityId);
    const FDaoMarkProfileComponent* Dao = DaoMarks.Find(EntityId);
    if (!Refinable || !Dao) return false;

    OutSnapshot = FRefinementSemanticSnapshot();
    OutSnapshot.EntityId = EntityId;
    OutSnapshot.Kind = Refinable->Kind;
    OutSnapshot.SourceId = Refinable->SourceId;
    OutSnapshot.DefinitionId = Refinable->DefinitionId;
    OutSnapshot.Semantic.Paths = Dao->Paths;
    OutSnapshot.Semantic.DaoMass = Dao->DaoMass;
    if (const FRefinementPropertiesComponent* Found = RefinementProperties.Find(EntityId)) OutSnapshot.Semantic.Properties = Found->Scores;
    if (const FRefinementAttributesComponent* Found = RefinementAttributes.Find(EntityId)) OutSnapshot.Semantic.Attributes = Found->Scores;
    if (const FRefinementTraitsComponent* Found = RefinementTraits.Find(EntityId)) OutSnapshot.Semantic.Traits = Found->Scores;
    if (const FRefinementTemplatesComponent* Found = RefinementTemplates.Find(EntityId)) OutSnapshot.Semantic.Templates = Found->Scores;
    if (const FDaoContaminationComponent* Found = DaoContamination.Find(EntityId)) OutSnapshot.Contamination = *Found;
    return true;
}

TArray<FGuid> UGuEntitySubsystem::QueryRefinableEntities() const
{
    TArray<FGuid> Result;
    Refinables.GenerateKeyArray(Result);
    return Result;
}

TArray<FGuid> UGuEntitySubsystem::QueryGuEntities() const
{
    TArray<FGuid> Result;
    GuInstances.GenerateKeyArray(Result);
    return Result;
}

TArray<FGuid> UGuEntitySubsystem::QueryGuEntitiesForOwner(const FString& OwnerId, const EGuContainer Container, const bool bLivingOnly) const
{
    TArray<FGuid> Result;
    for (const TPair<FGuid, FGuInstanceComponent>& Pair : GuInstances)
    {
        const FOwnedByComponent* Owner = Owners.Find(Pair.Key);
        const FGuPlacementComponent* Placement = GuPlacements.Find(Pair.Key);
        const FGuConditionComponent* Condition = GuConditions.Find(Pair.Key);
        if (!Owner || !Placement || Owner->OwnerId != OwnerId || Placement->Container != Container) continue;
        if (bLivingOnly && (!Condition || !Condition->bAlive)) continue;
        Result.Add(Pair.Key);
    }
    return Result;
}

bool UGuEntitySubsystem::FindOwnedGuInstance(
    const FName DefinitionId,
    const FString& OwnerId,
    const EGuContainer Container,
    FGuid& OutEntityId,
    const bool bLivingOnly) const
{
    for (const TPair<FGuid, FGuInstanceComponent>& Pair : GuInstances)
    {
        if (Pair.Value.DefinitionId != DefinitionId) continue;
        const FOwnedByComponent* Owner = Owners.Find(Pair.Key);
        const FGuPlacementComponent* Placement = GuPlacements.Find(Pair.Key);
        const FGuConditionComponent* Condition = GuConditions.Find(Pair.Key);
        if (!Owner || !Placement || Owner->OwnerId != OwnerId || Placement->Container != Container) continue;
        if (bLivingOnly && (!Condition || !Condition->bAlive)) continue;
        OutEntityId = Pair.Key;
        return true;
    }
    OutEntityId.Invalidate();
    return false;
}

bool UGuEntitySubsystem::CanUseGu(const FGuid EntityId, FString& OutError) const
{
    const FGuInstanceComponent* Instance = GuInstances.Find(EntityId);
    const FGuConditionComponent* Condition = GuConditions.Find(EntityId);
    const FGuPlacementComponent* Placement = GuPlacements.Find(EntityId);
    const FGuLifecycleComponent* Lifecycle = GuLifecycles.Find(EntityId);
    const FGuChargesComponent* Charges = GuCharges.Find(EntityId);

    if (!Instance || !Condition || !Placement)
    {
        OutError = TEXT("The GameplayAbility is not bound to a valid physical Gu entity.");
        return false;
    }
    if (!Condition->bAlive || Placement->Container == EGuContainer::Consumed)
    {
        OutError = TEXT("That Gu no longer exists in a usable state.");
        return false;
    }
    if (Lifecycle && Lifecycle->bConsumable && (!Charges || Charges->Remaining <= 0))
    {
        OutError = TEXT("That consumable Gu has no charges remaining.");
        return false;
    }

    OutError.Reset();
    return true;
}

bool UGuEntitySubsystem::NotifySuccessfulGuActivation(const FGuid EntityId, FString& OutError)
{
    if (!HasDomainAuthority())
    {
        OutError = TEXT("Gu lifecycle mutations are server-authoritative.");
        return false;
    }
    if (!CanUseGu(EntityId, OutError)) return false;

    if (FGuConditionComponent* Condition = GuConditions.Find(EntityId))
    {
        ++Condition->ActivationCount;
    }
    if (FGuVisualStateComponent* Visual = GuVisualStates.Find(EntityId))
    {
        Visual->Animation = TEXT("activation");
        Visual->ActivationUntilUnixMs = NowUnixMs() + 350;
    }

    const FGuLifecycleComponent* Lifecycle = GuLifecycles.Find(EntityId);
    if (Lifecycle && Lifecycle->bConsumable && Lifecycle->ConsumeOn == EGuConsumeOn::SuccessfulActivation)
    {
        if (!ConsumeGuCharge(EntityId, TEXT("successful activation")))
        {
            OutError = TEXT("The Gu activated, but its consumable lifecycle could not be settled.");
            return false;
        }
    }

    OutError.Reset();
    RequestPersistentSave();
    return true;
}

void UGuEntitySubsystem::SetContamination(const FGuid EntityId, const FDaoContaminationComponent& InContamination)
{
    if (!HasDomainAuthority() || !Entities.Contains(EntityId)) return;
    FDaoContaminationComponent Clean = InContamination;
    UGuRulesLibrary::NormalizeScoreMap(Clean.Paths);
    UGuRulesLibrary::NormalizeScoreMap(Clean.Attributes);
    UGuRulesLibrary::NormalizeScoreMap(Clean.Traits);
    Clean.Total = FMath::Max(0.0f, Clean.Total);
    DaoContamination.Add(EntityId, MoveTemp(Clean));
    RequestPersistentSave();
}

void UGuEntitySubsystem::SetMultitaskingBoost(const FGuid EntityId, const FMultitaskingBoostComponent& Boost)
{
    if (!HasDomainAuthority() || !Entities.Contains(EntityId)) return;
    FMultitaskingBoostComponent Clean = Boost;
    Clean.SlotsGranted = FMath::Max(0, Clean.SlotsGranted);
    if (Clean.SlotsGranted == 0) MultitaskingBoosts.Remove(EntityId);
    else MultitaskingBoosts.Add(EntityId, MoveTemp(Clean));
    RequestPersistentSave();
}

bool UGuEntitySubsystem::ConsumeGuCharge(const FGuid EntityId, FString Reason)
{
    if (!HasDomainAuthority()) return false;
    FGuConditionComponent* Condition = GuConditions.Find(EntityId);
    FGuChargesComponent* Charges = GuCharges.Find(EntityId);
    FGuLifecycleComponent* Lifecycle = GuLifecycles.Find(EntityId);
    FGuPlacementComponent* Placement = GuPlacements.Find(EntityId);
    if (!Condition || !Charges || !Lifecycle || !Placement || !Condition->bAlive) return false;

    Charges->Remaining = FMath::Max(0, Charges->Remaining - 1);
    if (Lifecycle->bConsumable && Charges->Remaining <= 0)
    {
        Condition->bAlive = false;
        Placement->Container = EGuContainer::Consumed;
        if (FGuVisualStateComponent* Visual = GuVisualStates.Find(EntityId))
        {
            Visual->Condition = FName(TEXT("dead"));
            Visual->ActivationUntilUnixMs = 0;
        }
        if (FGuStatusComponent* Status = GuStatus.Find(EntityId))
        {
            Status->States.Remove(FName(TEXT("Active")));
            Status->States.AddUnique(FName(TEXT("Dead/destroyed")));
        }
        UE_LOG(LogTemp, Log, TEXT("Consumed Gu entity %s: %s"), *EntityId.ToString(), *Reason);
    }
    RequestPersistentSave();
    return true;
}

int32 UGuEntitySubsystem::GetActiveMultitaskingBoostSlots(const FString& OwnerId) const
{
    int32 Slots = 0;
    for (const TPair<FGuid, FMultitaskingBoostComponent>& Pair : MultitaskingBoosts)
    {
        const FGuConditionComponent* Condition = GuConditions.Find(Pair.Key);
        const FGuPlacementComponent* Placement = GuPlacements.Find(Pair.Key);
        if (Pair.Value.OwnerId == OwnerId && Condition && Condition->bAlive && Placement && Placement->Container == EGuContainer::Aperture)
        {
            Slots += FMath::Max(0, Pair.Value.SlotsGranted);
        }
    }
    return Slots;
}

TArray<FGuEntitySnapshot> UGuEntitySubsystem::ExportSnapshots() const
{
    TArray<FGuEntitySnapshot> Out;
    Out.Reserve(Entities.Num());
    for (const FGuid EntityId : Entities)
    {
        FGuEntitySnapshot S;
        S.EntityId = EntityId;
        if (const FRefinableEntityComponent* Found = Refinables.Find(EntityId)) { S.bHasRefinable = true; S.Refinable = *Found; }
        if (const FMaterialLotComponent* Found = MaterialLots.Find(EntityId)) { S.bHasMaterialLot = true; S.MaterialLot = *Found; }
        if (const FDaoMarkProfileComponent* Found = DaoMarks.Find(EntityId)) { S.bHasDaoMarks = true; S.DaoMarks = *Found; }
        if (const FRefinementPropertiesComponent* Found = RefinementProperties.Find(EntityId)) S.RefinementProperties = *Found;
        if (const FRefinementAttributesComponent* Found = RefinementAttributes.Find(EntityId)) S.RefinementAttributes = *Found;
        if (const FRefinementTraitsComponent* Found = RefinementTraits.Find(EntityId)) S.RefinementTraits = *Found;
        if (const FRefinementTemplatesComponent* Found = RefinementTemplates.Find(EntityId)) S.RefinementTemplates = *Found;
        if (const FDaoContaminationComponent* Found = DaoContamination.Find(EntityId)) { S.bHasContamination = true; S.Contamination = *Found; }
        if (const FGuInstanceComponent* Found = GuInstances.Find(EntityId))
        {
            S.bHasGuInstance = true;
            S.GuInstance = *Found;
            if (const FGuConditionComponent* C = GuConditions.Find(EntityId)) S.GuCondition = *C;
            if (const FGuVisualStateComponent* C = GuVisualStates.Find(EntityId)) S.GuVisualState = *C;
            if (const FGuNourishmentComponent* C = GuNourishment.Find(EntityId)) S.GuNourishment = *C;
            if (const FOwnedByComponent* C = Owners.Find(EntityId)) S.OwnedBy = *C;
            if (const FGuStatusComponent* C = GuStatus.Find(EntityId)) S.GuStatus = *C;
            if (const FGuLifecycleComponent* C = GuLifecycles.Find(EntityId)) S.GuLifecycle = *C;
            if (const FGuChargesComponent* C = GuCharges.Find(EntityId)) S.GuCharges = *C;
            if (const FGuPlacementComponent* C = GuPlacements.Find(EntityId)) S.GuPlacement = *C;
        }
        if (const FGuEnslavementControllerComponent* Found = EnslavementControllers.Find(EntityId)) { S.bHasEnslavementController = true; S.EnslavementController = *Found; }
        if (const FMultitaskingBoostComponent* Found = MultitaskingBoosts.Find(EntityId)) { S.bHasMultitaskingBoost = true; S.MultitaskingBoost = *Found; }
        if (const FRefinementAssistantComponent* Found = RefinementAssistants.Find(EntityId)) { S.bHasRefinementAssistant = true; S.RefinementAssistant = *Found; }
        Out.Add(MoveTemp(S));
    }
    return Out;
}

bool UGuEntitySubsystem::RestoreSnapshots(const TArray<FGuEntitySnapshot>& Snapshots, FString& OutError)
{
    if (!HasDomainAuthority())
    {
        OutError = TEXT("Save restoration is server-authoritative.");
        return false;
    }
    ResetAllEntities();
    for (const FGuEntitySnapshot& S : Snapshots)
    {
        if (!S.EntityId.IsValid() || Entities.Contains(S.EntityId))
        {
            OutError = TEXT("Save contains an invalid or duplicate entity ID.");
            ResetAllEntities();
            return false;
        }
        Entities.Add(S.EntityId);
        if (S.bHasRefinable) Refinables.Add(S.EntityId, S.Refinable);
        if (S.bHasMaterialLot) MaterialLots.Add(S.EntityId, S.MaterialLot);
        if (S.bHasDaoMarks) DaoMarks.Add(S.EntityId, S.DaoMarks);
        if (S.bHasRefinable)
        {
            RefinementProperties.Add(S.EntityId, S.RefinementProperties);
            RefinementAttributes.Add(S.EntityId, S.RefinementAttributes);
            RefinementTraits.Add(S.EntityId, S.RefinementTraits);
            RefinementTemplates.Add(S.EntityId, S.RefinementTemplates);
        }
        if (S.bHasContamination) DaoContamination.Add(S.EntityId, S.Contamination);
        if (S.bHasGuInstance)
        {
            GuInstances.Add(S.EntityId, S.GuInstance);
            GuConditions.Add(S.EntityId, S.GuCondition);
            GuVisualStates.Add(S.EntityId, S.GuVisualState);
            GuNourishment.Add(S.EntityId, S.GuNourishment);
            Owners.Add(S.EntityId, S.OwnedBy);
            GuStatus.Add(S.EntityId, S.GuStatus);
            GuLifecycles.Add(S.EntityId, S.GuLifecycle);
            GuCharges.Add(S.EntityId, S.GuCharges);
            GuPlacements.Add(S.EntityId, S.GuPlacement);
        }
        if (S.bHasEnslavementController) EnslavementControllers.Add(S.EntityId, S.EnslavementController);
        if (S.bHasMultitaskingBoost) MultitaskingBoosts.Add(S.EntityId, S.MultitaskingBoost);
        if (S.bHasRefinementAssistant) RefinementAssistants.Add(S.EntityId, S.RefinementAssistant);
    }
    OutError.Reset();
    return true;
}

void UGuEntitySubsystem::ResetAllEntities()
{
    Entities.Reset();
    Refinables.Reset();
    MaterialLots.Reset();
    DaoMarks.Reset();
    RefinementProperties.Reset();
    RefinementAttributes.Reset();
    RefinementTraits.Reset();
    RefinementTemplates.Reset();
    DaoContamination.Reset();
    GuInstances.Reset();
    GuConditions.Reset();
    GuVisualStates.Reset();
    GuNourishment.Reset();
    Owners.Reset();
    GuStatus.Reset();
    GuLifecycles.Reset();
    GuCharges.Reset();
    GuPlacements.Reset();
    EnslavementControllers.Reset();
    MultitaskingBoosts.Reset();
    RefinementAssistants.Reset();
}

bool UGuEntitySubsystem::TransferGuOwnershipAndPlacement(
    const FGuid EntityId,
    const FString& NewOwnerId,
    const EGuContainer NewContainer,
    FString& OutError)
{
    if (!HasDomainAuthority())
    {
        OutError = TEXT("Gu ownership/placement mutations are server-authoritative.");
        return false;
    }

    if (!GuInstances.Contains(EntityId))
    {
        OutError = FString::Printf(TEXT("Entity %s is not a Gu."), *EntityId.ToString());
        return false;
    }

    FGuConditionComponent* Condition = GuConditions.Find(EntityId);
    if (!Condition || !Condition->bAlive)
    {
        OutError = TEXT("A dead or missing Gu cannot be transferred.");
        return false;
    }

    if (NewContainer == EGuContainer::Consumed)
    {
        OutError = TEXT("Transfer cannot place a living Gu directly into Consumed.");
        return false;
    }

    FOwnedByComponent& Owner = Owners.FindOrAdd(EntityId);
    FGuPlacementComponent& Placement = GuPlacements.FindOrAdd(EntityId);
    FGuStatusComponent& Status = GuStatus.FindOrAdd(EntityId);

    Owner.OwnerId = NewOwnerId;
    Placement.Container = NewContainer;
    Status.HolderId = NewOwnerId;

    Status.States.Remove(TEXT("Wild"));
    Status.States.Remove(TEXT("Refined / owned"));

    if (NewContainer == EGuContainer::World)
    {
        Status.States.AddUnique(TEXT("Wild"));
        Status.Visibility = TEXT("Public");
    }
    else
    {
        Status.States.AddUnique(TEXT("Refined / owned"));
        Status.Visibility = TEXT("Secret");
    }
    Status.States.AddUnique(TEXT("Active"));

    OutError.Reset();
    return true;
}
