#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GuEntityTypes.h"
#include "GuEntitySubsystem.generated.h"

UCLASS()
class GU_DAOIST_MASTER_API UGuEntitySubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Gu|ECS")
    FGuid CreateRefinableEntity(const FRefinementSemanticProfile& Profile, ERefinableKind Kind, FName SourceId, FName DefinitionId);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Gu|ECS")
    FGuid CreateMaterialLot(const FRefinementSemanticProfile& Profile, FName Item, int32 Quantity, FName SourceKind, FGuid SourceEntityId);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Gu|ECS")
    FGuid CreateGuInstance(FName DefinitionId, const FString& OwnerId, EGuContainer Container = EGuContainer::Aperture);

    bool CreateGuInstanceWithId(FName DefinitionId, const FString& OwnerId, EGuContainer Container, const FGuid& RequestedId, FGuid& OutEntityId, FString& OutError);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Gu|ECS")
    bool DestroyEntity(FGuid EntityId);

    UFUNCTION(BlueprintPure, Category="Gu|ECS")
    bool HasEntity(FGuid EntityId) const { return Entities.Contains(EntityId); }

    UFUNCTION(BlueprintCallable, Category="Gu|ECS")
    bool GetRefinementSemanticSnapshot(FGuid EntityId, FRefinementSemanticSnapshot& OutSnapshot) const;

    UFUNCTION(BlueprintPure, Category="Gu|ECS")
    TArray<FGuid> QueryRefinableEntities() const;

    UFUNCTION(BlueprintPure, Category="Gu|ECS")
    TArray<FGuid> QueryGuEntities() const;

    TArray<FGuid> QueryGuEntitiesForOwner(const FString& OwnerId, EGuContainer Container, bool bLivingOnly = true) const;

    /** Finds one physical Gu instance of this species already owned by the character. */
    bool FindOwnedGuInstance(FName DefinitionId, const FString& OwnerId, EGuContainer Container, FGuid& OutEntityId, bool bLivingOnly = true) const;

    /** Checks the physical instance state before GAS is allowed to execute it. */
    bool CanUseGu(FGuid EntityId, FString& OutError) const;

    /** Applies per-instance post-activation state and consumable lifecycle rules. */
    bool NotifySuccessfulGuActivation(FGuid EntityId, FString& OutError);

    const FMaterialLotComponent* GetMaterialLot(FGuid EntityId) const { return MaterialLots.Find(EntityId); }
    FMaterialLotComponent* GetMutableMaterialLot(FGuid EntityId) { return MaterialLots.Find(EntityId); }

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Gu|ECS")
    bool ConsumeMaterialQuantity(FGuid EntityId, int32 Quantity, FString& OutError);

    const FGuInstanceComponent* GetGuInstance(FGuid EntityId) const { return GuInstances.Find(EntityId); }
    const FGuConditionComponent* GetGuCondition(FGuid EntityId) const { return GuConditions.Find(EntityId); }
    FGuConditionComponent* GetMutableGuCondition(FGuid EntityId) { return GuConditions.Find(EntityId); }
    const FGuNourishmentComponent* GetGuNourishment(FGuid EntityId) const { return GuNourishment.Find(EntityId); }
    FGuNourishmentComponent* GetMutableGuNourishment(FGuid EntityId) { return GuNourishment.Find(EntityId); }
    const FGuVisualStateComponent* GetGuVisualState(FGuid EntityId) const { return GuVisualStates.Find(EntityId); }
    FGuVisualStateComponent* GetMutableGuVisualState(FGuid EntityId) { return GuVisualStates.Find(EntityId); }
    const FGuPlacementComponent* GetGuPlacement(FGuid EntityId) const { return GuPlacements.Find(EntityId); }
    FGuPlacementComponent* GetMutableGuPlacement(FGuid EntityId) { return GuPlacements.Find(EntityId); }
    const FOwnedByComponent* GetOwnedBy(FGuid EntityId) const { return Owners.Find(EntityId); }
    const FGuChargesComponent* GetGuCharges(FGuid EntityId) const { return GuCharges.Find(EntityId); }
    FGuChargesComponent* GetMutableGuCharges(FGuid EntityId) { return GuCharges.Find(EntityId); }
    const FRefinementAssistantComponent* GetRefinementAssistant(FGuid EntityId) const { return RefinementAssistants.Find(EntityId); }

    void SetContamination(FGuid EntityId, const FDaoContaminationComponent& Contamination);
    void SetMultitaskingBoost(FGuid EntityId, const FMultitaskingBoostComponent& Boost);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Gu|ECS")
    bool ConsumeGuCharge(FGuid EntityId, FString Reason);

    UFUNCTION(BlueprintPure, Category="Gu|ECS")
    int32 GetActiveMultitaskingBoostSlots(const FString& OwnerId) const;

    TArray<FGuEntitySnapshot> ExportSnapshots() const;
    bool RestoreSnapshots(const TArray<FGuEntitySnapshot>& Snapshots, FString& OutError);
    void ResetAllEntities();

private:
    static int64 NowUnixMs();
    bool HasDomainAuthority() const;
    FGuid AllocateEntityId(const FGuid* RequestedId = nullptr) const;
    void AttachRefinementSemantics(FGuid EntityId, const FRefinementSemanticProfile& Profile, ERefinableKind Kind, FName SourceId, FName DefinitionId);

    TSet<FGuid> Entities;
    TMap<FGuid, FRefinableEntityComponent> Refinables;
    TMap<FGuid, FMaterialLotComponent> MaterialLots;
    TMap<FGuid, FDaoMarkProfileComponent> DaoMarks;
    TMap<FGuid, FRefinementPropertiesComponent> RefinementProperties;
    TMap<FGuid, FRefinementAttributesComponent> RefinementAttributes;
    TMap<FGuid, FRefinementTraitsComponent> RefinementTraits;
    TMap<FGuid, FRefinementTemplatesComponent> RefinementTemplates;
    TMap<FGuid, FDaoContaminationComponent> DaoContamination;

    TMap<FGuid, FGuInstanceComponent> GuInstances;
    TMap<FGuid, FGuConditionComponent> GuConditions;
    TMap<FGuid, FGuVisualStateComponent> GuVisualStates;
    TMap<FGuid, FGuNourishmentComponent> GuNourishment;
    TMap<FGuid, FOwnedByComponent> Owners;
    TMap<FGuid, FGuStatusComponent> GuStatus;
    TMap<FGuid, FGuLifecycleComponent> GuLifecycles;
    TMap<FGuid, FGuChargesComponent> GuCharges;
    TMap<FGuid, FGuPlacementComponent> GuPlacements;
    TMap<FGuid, FGuEnslavementControllerComponent> EnslavementControllers;
    TMap<FGuid, FMultitaskingBoostComponent> MultitaskingBoosts;
    TMap<FGuid, FRefinementAssistantComponent> RefinementAssistants;
};
