#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GuTownHonorParityLibrary.generated.h"

USTRUCT(BlueprintType)
struct FGuHonorMissionProgressView
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 CultivationRank = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 OwnedLivingGuCount = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 BattleVictories = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 GardenPlotCount = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 AuctionWins = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bHasBoundBeast = false;

    /** NPC name -> standing/likeability. Friend threshold is 20 in the browser baseline. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TMap<FName, int32> NpcStanding;
};

USTRUCT(BlueprintType)
struct FGuHonorMissionDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName Id;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FText Title;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FText Description;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName Npc;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 BaseStoneReward = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 BaseHonorReward = 0;
};

USTRUCT(BlueprintType)
struct FGuHonorReward
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 Stones = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 Honor = 0;

    /** Browser behavior: NPC-linked mission claims also grant +5 standing with that NPC. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 NpcStandingDelta = 0;
};

UENUM(BlueprintType)
enum class EGuHonorShopGrant : uint8
{
    RankMatchedRelic,
    SpiritStones
};

USTRUCT(BlueprintType)
struct FGuHonorShopItemDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName Key;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FText Label;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FText Description;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 HonorCost = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    EGuHonorShopGrant Grant = EGuHonorShopGrant::SpiritStones;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 StoneGrant = 0;
};

/**
 * Stateless browser-parity catalog for Honor missions/rewards.
 * The authoritative town/economy subsystem remains responsible for claimed state,
 * currency mutations, Relic Gu creation and persistence.
 */
UCLASS()
class GU_DAOIST_MASTER_API UGuTownHonorParityLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintPure, Category="Gu|Town|Honor")
    static TArray<FGuHonorMissionDefinition> GetHonorMissions();

    UFUNCTION(BlueprintPure, Category="Gu|Town|Honor")
    static bool IsHonorMissionComplete(FName MissionId, const FGuHonorMissionProgressView& Progress);

    UFUNCTION(BlueprintPure, Category="Gu|Town|Honor")
    static bool ComputeHonorMissionReward(FName MissionId, float DifficultyRewardMultiplier, FGuHonorReward& OutReward);

    UFUNCTION(BlueprintPure, Category="Gu|Town|Honor")
    static TArray<FGuHonorShopItemDefinition> GetHonorShopItems();

    static const FGuHonorMissionDefinition* FindMission(FName MissionId);
};
