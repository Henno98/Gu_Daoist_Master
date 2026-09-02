// Fill out your copyright notice in the Description page of Project Settings.
#include "UGuDefinition.h"

#include "GuDefinitionRegistrySubsystem.h"

void UGuDefinition::PostLoad()
{
    Super::PostLoad();

    if (HasAnyFlags(RF_ClassDefaultObject) || bRefinementSemanticsMaterialized)
    {
        return;
    }

    FGuDefinitionRecord EffectiveRecord;
    FString Error;
    if (UGuDefinitionRegistrySubsystem::BuildRecordFromAsset(this, EffectiveRecord, Error))
    {
        RefinementProfile = MoveTemp(EffectiveRecord.RefinementProfile);
        bRefinementSemanticsMaterialized = true;

#if WITH_EDITOR
        // This is a one-time legacy DataAsset migration. Marking the package dirty
        // lets Save All persist the copied semantic data instead of recomputing it
        // forever in memory.
        MarkPackageDirty();
#endif
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("Could not materialize refinement semantics for %s: %s"), *GetPathName(), *Error);
    }
}

void UGuDefinition::RebuildRefinementSemantics()
{
#if WITH_EDITOR
    Modify();
#endif

    // Rebuild means exactly that: discard the currently materialized snapshot and
    // derive it again from the authored Path, activation model and GAS mechanics.
    RefinementProfile = FRefinementSemanticProfile();
    bRefinementSemanticsMaterialized = false;

    FGuDefinitionRecord EffectiveRecord;
    FString Error;
    if (!UGuDefinitionRegistrySubsystem::BuildRecordFromAsset(this, EffectiveRecord, Error))
    {
        UE_LOG(LogTemp, Warning, TEXT("Could not rebuild refinement semantics for %s: %s"), *GetPathName(), *Error);
        return;
    }

    RefinementProfile = MoveTemp(EffectiveRecord.RefinementProfile);
    bRefinementSemanticsMaterialized = true;

#if WITH_EDITOR
    MarkPackageDirty();
#endif
}
