#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GuWorldDaoEcologyTypes.generated.h"

UENUM(BlueprintType)
enum class EGuDaoEventGeometryType : uint8
{
    Circle,
    Ellipse,
    Polyline
};

UENUM(BlueprintType)
enum class EGuDaoSuccessionStage : uint8
{
    Trace,
    Influenced,
    Established,
    Aligned,
    Transformed,
    PathDomain
};

USTRUCT(BlueprintType)
struct FGuDaoEventGeometry
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gu|Dao Ecology")
    EGuDaoEventGeometryType Type = EGuDaoEventGeometryType::Circle;

    /** World-space XY center in Unreal centimetres. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gu|Dao Ecology")
    FVector2D Center = FVector2D::ZeroVector;

    /** Circle radius, in Unreal centimetres. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gu|Dao Ecology", meta = (ClampMin = "1.0"))
    float Radius = 9000.0f;

    /** Ellipse radii, in Unreal centimetres. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gu|Dao Ecology")
    FVector2D Radii = FVector2D(9000.0f, 9000.0f);

    /** Polyline world-space XY points. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gu|Dao Ecology")
    TArray<FVector2D> Points;

    /** Polyline influence half-width, in Unreal centimetres. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gu|Dao Ecology", meta = (ClampMin = "1.0"))
    float Width = 9000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gu|Dao Ecology", meta = (ClampMin = "0.1"))
    float FalloffPower = 1.65f;
};

USTRUCT(BlueprintType)
struct FGuWorldDaoEvent
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gu|Dao Ecology")
    FGuid Id;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gu|Dao Ecology")
    FName Kind = NAME_None;

    /** Optional source Gu/event identifier for diagnostics and persistence. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gu|Dao Ecology")
    FName SourceId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gu|Dao Ecology", meta = (ClampMin = "1", ClampMax = "9"))
    int32 SourceRank = 1;

    /** Years elapsed since the event was deposited. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gu|Dao Ecology", meta = (ClampMin = "0.0"))
    float AgeYears = 0.0f;

    /** Permanent/engraved events do not undergo loose-residue turnover. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gu|Dao Ecology")
    bool bPermanent = false;

    /** Optional explicit half-life. <= 0 uses the source-rank Gu trace half-life. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gu|Dao Ecology")
    float HalfLifeYears = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gu|Dao Ecology")
    FGuDaoEventGeometry Geometry;

    /** Signed environmental Dao-density deposits by Data.Paths.* tag. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gu|Dao Ecology")
    TMap<FGameplayTag, float> DaoDeposit;
};

USTRUCT(BlueprintType)
struct FGuDaoRegionField
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gu|Dao Ecology")
    FName Id = NAME_None;

    /** Physical substrate semantic, e.g. bamboo, mountain, still-water, rock-face. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gu|Dao Ecology")
    FName Substrate = TEXT("wild-grass");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gu|Dao Ecology")
    FVector2D Center = FVector2D::ZeroVector;

    /** Elliptical field radii in Unreal centimetres. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gu|Dao Ecology")
    FVector2D Radii = FVector2D(10000.0f, 10000.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gu|Dao Ecology", meta = (ClampMin = "0.1"))
    float FalloffPower = 1.35f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gu|Dao Ecology", meta = (ClampMin = "0.0"))
    float MaturityYears = 500.0f;

    /** Additional authored/natural Dao density layered over the substrate baseline. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gu|Dao Ecology")
    TMap<FGameplayTag, float> DaoMarks;
};


USTRUCT(BlueprintType)
struct FGuDaoActivityUse
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gu|Dao Ecology")
    FGameplayTag Path;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gu|Dao Ecology", meta = (ClampMin = "1", ClampMax = "9"))
    int32 Rank = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gu|Dao Ecology", meta = (ClampMin = "0.0"))
    float ActivationsPerDay = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gu|Dao Ecology", meta = (ClampMin = "0.0"))
    float Retention = 1.0f;
};

USTRUCT(BlueprintType)
struct FGuDaoActivityField
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gu|Dao Ecology")
    FName Id = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gu|Dao Ecology")
    FVector2D Center = FVector2D::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gu|Dao Ecology")
    FVector2D Radii = FVector2D(10000.0f, 10000.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gu|Dao Ecology", meta = (ClampMin = "0.1"))
    float FalloffPower = 1.35f;

    /** Duration of continuous historical/current activity. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gu|Dao Ecology", meta = (ClampMin = "0.0"))
    float ActiveYears = 0.0f;

    /** Years since the activity source ceased; zero means it remains active/current. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gu|Dao Ecology", meta = (ClampMin = "0.0"))
    float InactiveYears = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gu|Dao Ecology")
    TArray<FGuDaoActivityUse> Usages;
};

USTRUCT(BlueprintType)
struct FGuDaoConflictRule
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gu|Dao Ecology")
    FGameplayTag PathA;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gu|Dao Ecology")
    FGameplayTag PathB;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gu|Dao Ecology", meta = (ClampMin = "0.0"))
    float Strength = 0.0f;
};

USTRUCT(BlueprintType)
struct FGuDaoSuccessionState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Gu|Dao Ecology")
    FGameplayTag Path;

    UPROPERTY(BlueprintReadOnly, Category = "Gu|Dao Ecology")
    float Density = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Gu|Dao Ecology")
    float Share = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Gu|Dao Ecology")
    float MaturityYears = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Gu|Dao Ecology")
    EGuDaoSuccessionStage Stage = EGuDaoSuccessionStage::Trace;
};

USTRUCT(BlueprintType)
struct FGuWildGuHabitatRule
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gu|Dao Ecology")
    FName DefinitionId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gu|Dao Ecology")
    FText Name;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gu|Dao Ecology")
    FGameplayTag Path;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gu|Dao Ecology", meta = (ClampMin = "1", ClampMax = "9"))
    int32 Rank = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gu|Dao Ecology")
    float DensityRequired = 45.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gu|Dao Ecology")
    float ShareRequired = 0.195f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gu|Dao Ecology")
    float MaturityYears = 3.0f;
};

USTRUCT(BlueprintType)
struct FGuSubstrateResourceRule
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gu|Dao Ecology")
    FName Id = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gu|Dao Ecology")
    FText Name;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gu|Dao Ecology")
    FGameplayTag Path;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gu|Dao Ecology")
    int32 Rank = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gu|Dao Ecology")
    TArray<FName> Substrates;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gu|Dao Ecology")
    float DensityRequired = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gu|Dao Ecology")
    float ShareRequired = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gu|Dao Ecology")
    float MaturityYears = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gu|Dao Ecology")
    FName Kind = NAME_None;
};

USTRUCT(BlueprintType)
struct FGuBeastSuccessionRule
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gu|Dao Ecology")
    FName Id = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gu|Dao Ecology")
    FText Name;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gu|Dao Ecology")
    FGameplayTag Path;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gu|Dao Ecology")
    int32 Rank = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gu|Dao Ecology")
    TArray<FName> Substrates;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gu|Dao Ecology")
    float DensityRequired = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gu|Dao Ecology")
    float ShareRequired = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gu|Dao Ecology")
    float MaturityYears = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gu|Dao Ecology")
    FName Mode = NAME_None;
};

USTRUCT(BlueprintType)
struct FGuDaoEcologyCandidate
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Gu|Dao Ecology")
    FName Id = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category = "Gu|Dao Ecology")
    FText Name;

    UPROPERTY(BlueprintReadOnly, Category = "Gu|Dao Ecology")
    FGameplayTag Path;

    UPROPERTY(BlueprintReadOnly, Category = "Gu|Dao Ecology")
    int32 Rank = 1;

    UPROPERTY(BlueprintReadOnly, Category = "Gu|Dao Ecology")
    FName Kind = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category = "Gu|Dao Ecology")
    FName Substrate = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category = "Gu|Dao Ecology")
    float Intensity = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Gu|Dao Ecology")
    float LocalDensity = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Gu|Dao Ecology")
    float PathShare = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Gu|Dao Ecology")
    float MaturityYears = 0.0f;
};

USTRUCT(BlueprintType)
struct FGuDaoPathAttraction
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Gu|Dao Ecology")
    FGameplayTag Path;

    UPROPERTY(BlueprintReadOnly, Category = "Gu|Dao Ecology")
    float HabitatMultiplier = 1.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Gu|Dao Ecology")
    float Density = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Gu|Dao Ecology")
    float Share = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Gu|Dao Ecology")
    float MaturityYears = 0.0f;
};

USTRUCT(BlueprintType)
struct FGuDaoLandscapeTrait
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Gu|Dao Ecology")
    FGameplayTag Path;

    UPROPERTY(BlueprintReadOnly, Category = "Gu|Dao Ecology")
    EGuDaoSuccessionStage Stage = EGuDaoSuccessionStage::Trace;

    UPROPERTY(BlueprintReadOnly, Category = "Gu|Dao Ecology")
    float Intensity = 0.0f;
};

USTRUCT(BlueprintType)
struct FGuDaoEcologyProfile
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Gu|Dao Ecology")
    FName Substrate = TEXT("wild-grass");

    UPROPERTY(BlueprintReadOnly, Category = "Gu|Dao Ecology")
    TMap<FGameplayTag, float> Marks;

    UPROPERTY(BlueprintReadOnly, Category = "Gu|Dao Ecology")
    TMap<FGameplayTag, float> PathMaturityYears;

    UPROPERTY(BlueprintReadOnly, Category = "Gu|Dao Ecology")
    float TotalDensity = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Gu|Dao Ecology")
    FGameplayTag DominantPath;

    UPROPERTY(BlueprintReadOnly, Category = "Gu|Dao Ecology")
    float DominantDensity = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Gu|Dao Ecology")
    FGameplayTag SecondaryPath;

    UPROPERTY(BlueprintReadOnly, Category = "Gu|Dao Ecology")
    float Coherence = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Gu|Dao Ecology")
    float Conflict = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Gu|Dao Ecology")
    float InteractionPressure = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Gu|Dao Ecology")
    TMap<FGameplayTag, float> ConflictLosses;

    UPROPERTY(BlueprintReadOnly, Category = "Gu|Dao Ecology")
    FGuDaoSuccessionState DominantSuccession;

    UPROPERTY(BlueprintReadOnly, Category = "Gu|Dao Ecology")
    TArray<FGuDaoEcologyCandidate> WildGuCandidates;

    UPROPERTY(BlueprintReadOnly, Category = "Gu|Dao Ecology")
    TArray<FGuDaoEcologyCandidate> ResourceCandidates;

    UPROPERTY(BlueprintReadOnly, Category = "Gu|Dao Ecology")
    TArray<FGuDaoEcologyCandidate> BeastCandidates;

    UPROPERTY(BlueprintReadOnly, Category = "Gu|Dao Ecology")
    TArray<FGuDaoPathAttraction> BeastPathAttraction;

    UPROPERTY(BlueprintReadOnly, Category = "Gu|Dao Ecology")
    TArray<FGuDaoLandscapeTrait> LandscapeTraits;
};
