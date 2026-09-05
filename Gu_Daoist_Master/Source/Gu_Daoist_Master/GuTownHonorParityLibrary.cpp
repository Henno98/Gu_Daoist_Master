#include "GuTownHonorParityLibrary.h"

namespace
{
    const TArray<FGuHonorMissionDefinition>& HonorMissionCatalog()
    {
        static const TArray<FGuHonorMissionDefinition> Missions = []
        {
            TArray<FGuHonorMissionDefinition> Out;
            auto Add = [&Out](const TCHAR* Id, const TCHAR* Title, const TCHAR* Description, const TCHAR* Npc, int32 Stones, int32 Honor)
            {
                FGuHonorMissionDefinition M;
                M.Id = FName(Id);
                M.Title = FText::FromString(FString(Title));
                M.Description = FText::FromString(FString(Description));
                M.Npc = Npc && *Npc ? FName(Npc) : NAME_None;
                M.BaseStoneReward = Stones;
                M.BaseHonorReward = Honor;
                Out.Add(MoveTemp(M));
            };

            Add(TEXT("m1"),  TEXT("First Steps"),          TEXT("Reach Rank 2."),                                      TEXT(""),          200, 10);
            Add(TEXT("m2"),  TEXT("Chen Yu's Favor"),     TEXT("Raise your standing with Chen Yu to Friend."),        TEXT("Chen Yu"),   150, 15);
            Add(TEXT("m3"),  TEXT("A Beast at Heel"),     TEXT("Have a beast bound to an Enslavement Gu."),           TEXT(""),          250, 15);
            Add(TEXT("m4"),  TEXT("Proven in Battle"),    TEXT("Win 5 battles."),                                     TEXT(""),          300, 20);
            Add(TEXT("m5"),  TEXT("Tending the Soil"),    TEXT("Expand the Herb Garden to 6 plots."),                 TEXT(""),          200, 15);
            Add(TEXT("m6"),  TEXT("Zhao Feng's Trust"),  TEXT("Raise your standing with Zhao Feng to Friend."),      TEXT("Zhao Feng"), 150, 15);
            Add(TEXT("m7"),  TEXT("A Name at Auction"),   TEXT("Win an item at the Auction House."),                   TEXT(""),          250, 20);
            Add(TEXT("m8"),  TEXT("Deepening Cultivation"),TEXT("Reach Rank 3."),                                     TEXT(""),          400, 30);
            Add(TEXT("m9"),  TEXT("A Full Aperture"),     TEXT("Own 8 distinct Gu."),                                 TEXT(""),          300, 25);
            Add(TEXT("m10"), TEXT("Elder Song's Regard"),TEXT("Raise your standing with Elder Song to Friend."),     TEXT("Elder Song"),300, 35);
            return Out;
        }();
        return Missions;
    }

    int32 StandingFor(const FGuHonorMissionProgressView& Progress, const TCHAR* Npc)
    {
        return Progress.NpcStanding.FindRef(FName(Npc));
    }
}

TArray<FGuHonorMissionDefinition> UGuTownHonorParityLibrary::GetHonorMissions()
{
    return HonorMissionCatalog();
}

const FGuHonorMissionDefinition* UGuTownHonorParityLibrary::FindMission(const FName MissionId)
{
    return HonorMissionCatalog().FindByPredicate([MissionId](const FGuHonorMissionDefinition& M)
    {
        return M.Id == MissionId;
    });
}

bool UGuTownHonorParityLibrary::IsHonorMissionComplete(
    const FName MissionId,
    const FGuHonorMissionProgressView& Progress)
{
    if (MissionId == FName(TEXT("m1")))  return Progress.CultivationRank >= 2;
    if (MissionId == FName(TEXT("m2")))  return StandingFor(Progress, TEXT("Chen Yu")) >= 20;
    if (MissionId == FName(TEXT("m3")))  return Progress.bHasBoundBeast;
    if (MissionId == FName(TEXT("m4")))  return Progress.BattleVictories >= 5;
    if (MissionId == FName(TEXT("m5")))  return Progress.GardenPlotCount >= 6;
    if (MissionId == FName(TEXT("m6")))  return StandingFor(Progress, TEXT("Zhao Feng")) >= 20;
    if (MissionId == FName(TEXT("m7")))  return Progress.AuctionWins >= 1;
    if (MissionId == FName(TEXT("m8")))  return Progress.CultivationRank >= 3;
    if (MissionId == FName(TEXT("m9")))  return Progress.OwnedLivingGuCount >= 8;
    if (MissionId == FName(TEXT("m10"))) return StandingFor(Progress, TEXT("Elder Song")) >= 20;
    return false;
}

bool UGuTownHonorParityLibrary::ComputeHonorMissionReward(
    const FName MissionId,
    const float DifficultyRewardMultiplier,
    FGuHonorReward& OutReward)
{
    const FGuHonorMissionDefinition* Mission = FindMission(MissionId);
    if (!Mission)
    {
        OutReward = FGuHonorReward();
        return false;
    }

    const float Multiplier = FMath::Max(0.0f, DifficultyRewardMultiplier);
    OutReward.Stones = FMath::RoundToInt(static_cast<float>(Mission->BaseStoneReward) * Multiplier);
    OutReward.Honor = FMath::RoundToInt(static_cast<float>(Mission->BaseHonorReward) * Multiplier);
    OutReward.NpcStandingDelta = Mission->Npc.IsNone() ? 0 : 5;
    return true;
}

TArray<FGuHonorShopItemDefinition> UGuTownHonorParityLibrary::GetHonorShopItems()
{
    TArray<FGuHonorShopItemDefinition> Out;

    FGuHonorShopItemDefinition Relic;
    Relic.Key = FName(TEXT("honor_relic"));
    Relic.Label = FText::FromString(TEXT("Attuned Relic Gu"));
    Relic.Description = FText::FromString(TEXT("A Relic Gu matched to your current rank, free of charge."));
    Relic.HonorCost = 40;
    Relic.Grant = EGuHonorShopGrant::RankMatchedRelic;
    Out.Add(MoveTemp(Relic));

    FGuHonorShopItemDefinition Stipend;
    Stipend.Key = FName(TEXT("honor_stones"));
    Stipend.Label = FText::FromString(TEXT("Sect Stipend"));
    Stipend.Description = FText::FromString(TEXT("A one-time gift of 500 spirit stones."));
    Stipend.HonorCost = 25;
    Stipend.Grant = EGuHonorShopGrant::SpiritStones;
    Stipend.StoneGrant = 500;
    Out.Add(MoveTemp(Stipend));

    return Out;
}
