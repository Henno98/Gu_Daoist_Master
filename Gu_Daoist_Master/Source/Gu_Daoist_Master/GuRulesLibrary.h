#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GuDefinitionTypes.h"
#include "GuRulesLibrary.generated.h"

UCLASS()
class GU_DAOIST_MASTER_API UGuRulesLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    static constexpr float BaseEssenceCap = 70.0f;
    static constexpr float AperturePressureCapacity = 100.0f;
    static constexpr float AperturePressureSameRank = 5.0f;
    static constexpr float AperturePressureLowerRankDivisor = 12.0f;
    static constexpr float AperturePressureHigherRankMultiplier = 4.1f;

    UFUNCTION(BlueprintPure, Category="Gu|Rules")
    static float GameplayEssenceQualityRelativeToRank1(int32 Rank);

    UFUNCTION(BlueprintPure, Category="Gu|Rules")
    static float TheoreticalEssenceCap(int32 Rank);

    UFUNCTION(BlueprintPure, Category="Gu|Rules")
    static float PersonalEssenceCap(int32 Rank, float GradeFillPercent);

    UFUNCTION(BlueprintPure, Category="Gu|Rules")
    static float GuAperturePressure(int32 GuRank, int32 HolderRank);

    UFUNCTION(BlueprintPure, Category="Gu|Mental")
    static int32 MentalFoundationCap(int32 Rank);

    UFUNCTION(BlueprintPure, Category="Gu|Mental")
    static int32 FocusBranchCap(int32 MentalFoundation, int32 Rank);

    UFUNCTION(BlueprintPure, Category="Gu|Mental")
    static int32 MentalFocusCapacity(int32 MentalFoundation, int32 FocusControlLevel);

    UFUNCTION(BlueprintPure, Category="Gu|Mental")
    static int32 MultitaskingNaturalCap(int32 Rank);

    UFUNCTION(BlueprintPure, Category="Gu|Refinement")
    static float RefinementDaoMassRequiredForRank(int32 Rank);

    UFUNCTION(BlueprintPure, Category="Gu|Refinement")
    static int32 ExperimentalFormationRankFromRetainedDaoMass(float RetainedDaoMass);

    // Browser parity for guDefinitionRefinementProfile(). Used only when an
    // imported/authored definition does not already carry a canonical profile.
    UFUNCTION(BlueprintPure, Category="Gu|Refinement")
    static FRefinementSemanticProfile BuildDefaultGuRefinementProfile(const FGuDefinitionRecord& Definition);

    static void NormalizeScoreMap(TMap<FName, float>& Scores);
    static void NormalizeSemanticProfile(FRefinementSemanticProfile& Profile);
    static FName NormalizePath(FName Path);
};
