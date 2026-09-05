#include "GuPathTags.h"

namespace GuPathTags
{
    UE_DEFINE_GAMEPLAY_TAG(Data_Paths, "Data.Paths");

    UE_DEFINE_GAMEPLAY_TAG(Data_Paths_Blade, "Data.Paths.Blade");
    UE_DEFINE_GAMEPLAY_TAG(Data_Paths_Blood, "Data.Paths.Blood");
    UE_DEFINE_GAMEPLAY_TAG(Data_Paths_Bone, "Data.Paths.Bone");
    UE_DEFINE_GAMEPLAY_TAG(Data_Paths_Cloud, "Data.Paths.Cloud");
    UE_DEFINE_GAMEPLAY_TAG(Data_Paths_Dark, "Data.Paths.Dark");
    UE_DEFINE_GAMEPLAY_TAG(Data_Paths_Dream, "Data.Paths.Dream");
    UE_DEFINE_GAMEPLAY_TAG(Data_Paths_Earth, "Data.Paths.Earth");
    UE_DEFINE_GAMEPLAY_TAG(Data_Paths_Emotion, "Data.Paths.Emotion");
    UE_DEFINE_GAMEPLAY_TAG(Data_Paths_Enchantment, "Data.Paths.Enchantment");
    UE_DEFINE_GAMEPLAY_TAG(Data_Paths_Enslavement, "Data.Paths.Enslavement");
    UE_DEFINE_GAMEPLAY_TAG(Data_Paths_Fire, "Data.Paths.Fire");
    UE_DEFINE_GAMEPLAY_TAG(Data_Paths_Food, "Data.Paths.Food");
    UE_DEFINE_GAMEPLAY_TAG(Data_Paths_Formation, "Data.Paths.Formation");
    UE_DEFINE_GAMEPLAY_TAG(Data_Paths_Heaven, "Data.Paths.Heaven");
    UE_DEFINE_GAMEPLAY_TAG(Data_Paths_Human, "Data.Paths.Human");
    UE_DEFINE_GAMEPLAY_TAG(Data_Paths_Ice, "Data.Paths.Ice");
    UE_DEFINE_GAMEPLAY_TAG(Data_Paths_Illusion, "Data.Paths.Illusion");
    UE_DEFINE_GAMEPLAY_TAG(Data_Paths_Information, "Data.Paths.Information");
    UE_DEFINE_GAMEPLAY_TAG(Data_Paths_Light, "Data.Paths.Light");
    UE_DEFINE_GAMEPLAY_TAG(Data_Paths_Lightning, "Data.Paths.Lightning");
    UE_DEFINE_GAMEPLAY_TAG(Data_Paths_Luck, "Data.Paths.Luck");
    UE_DEFINE_GAMEPLAY_TAG(Data_Paths_Metal, "Data.Paths.Metal");
    UE_DEFINE_GAMEPLAY_TAG(Data_Paths_Moon, "Data.Paths.Moon");
    UE_DEFINE_GAMEPLAY_TAG(Data_Paths_Painting, "Data.Paths.Painting");
    UE_DEFINE_GAMEPLAY_TAG(Data_Paths_Phantom, "Data.Paths.Phantom");
    UE_DEFINE_GAMEPLAY_TAG(Data_Paths_Pill, "Data.Paths.Pill");
    UE_DEFINE_GAMEPLAY_TAG(Data_Paths_Poison, "Data.Paths.Poison");
    UE_DEFINE_GAMEPLAY_TAG(Data_Paths_Qi, "Data.Paths.Qi");
    UE_DEFINE_GAMEPLAY_TAG(Data_Paths_Refinement, "Data.Paths.Refinement");
    UE_DEFINE_GAMEPLAY_TAG(Data_Paths_Restriction, "Data.Paths.Restriction");
    UE_DEFINE_GAMEPLAY_TAG(Data_Paths_Rule, "Data.Paths.Rule");
    UE_DEFINE_GAMEPLAY_TAG(Data_Paths_Snow, "Data.Paths.Snow");
    UE_DEFINE_GAMEPLAY_TAG(Data_Paths_Soul, "Data.Paths.Soul");
    UE_DEFINE_GAMEPLAY_TAG(Data_Paths_Sound, "Data.Paths.Sound");
    UE_DEFINE_GAMEPLAY_TAG(Data_Paths_Space, "Data.Paths.Space");
    UE_DEFINE_GAMEPLAY_TAG(Data_Paths_Star, "Data.Paths.Star");
    UE_DEFINE_GAMEPLAY_TAG(Data_Paths_Strength, "Data.Paths.Strength");
    UE_DEFINE_GAMEPLAY_TAG(Data_Paths_Sword, "Data.Paths.Sword");
    UE_DEFINE_GAMEPLAY_TAG(Data_Paths_Theft, "Data.Paths.Theft");
    UE_DEFINE_GAMEPLAY_TAG(Data_Paths_Time, "Data.Paths.Time");
    UE_DEFINE_GAMEPLAY_TAG(Data_Paths_Transformation, "Data.Paths.Transformation");
    UE_DEFINE_GAMEPLAY_TAG(Data_Paths_Water, "Data.Paths.Water");
    UE_DEFINE_GAMEPLAY_TAG(Data_Paths_Weapon, "Data.Paths.Weapon");
    UE_DEFINE_GAMEPLAY_TAG(Data_Paths_Wind, "Data.Paths.Wind");
    UE_DEFINE_GAMEPLAY_TAG(Data_Paths_Wisdom, "Data.Paths.Wisdom");
    UE_DEFINE_GAMEPLAY_TAG(Data_Paths_Wood, "Data.Paths.Wood");

    const TArray<FGameplayTag>& GetAll()
    {
        static const TArray<FGameplayTag> Paths =
        {
            Data_Paths_Blade,
            Data_Paths_Blood,
            Data_Paths_Bone,
            Data_Paths_Cloud,
            Data_Paths_Dark,
            Data_Paths_Dream,
            Data_Paths_Earth,
            Data_Paths_Emotion,
            Data_Paths_Enchantment,
            Data_Paths_Enslavement,
            Data_Paths_Fire,
            Data_Paths_Food,
            Data_Paths_Formation,
            Data_Paths_Heaven,
            Data_Paths_Human,
            Data_Paths_Ice,
            Data_Paths_Illusion,
            Data_Paths_Information,
            Data_Paths_Light,
            Data_Paths_Lightning,
            Data_Paths_Luck,
            Data_Paths_Metal,
            Data_Paths_Moon,
            Data_Paths_Painting,
            Data_Paths_Phantom,
            Data_Paths_Pill,
            Data_Paths_Poison,
            Data_Paths_Qi,
            Data_Paths_Refinement,
            Data_Paths_Restriction,
            Data_Paths_Rule,
            Data_Paths_Snow,
            Data_Paths_Soul,
            Data_Paths_Sound,
            Data_Paths_Space,
            Data_Paths_Star,
            Data_Paths_Strength,
            Data_Paths_Sword,
            Data_Paths_Theft,
            Data_Paths_Time,
            Data_Paths_Transformation,
            Data_Paths_Water,
            Data_Paths_Weapon,
            Data_Paths_Wind,
            Data_Paths_Wisdom,
            Data_Paths_Wood
        };
        return Paths;
    }

    FGameplayTag FindByLeaf(const FName LeafOrTagName)
    {
        if (LeafOrTagName.IsNone()) return FGameplayTag();

        FString Wanted = LeafOrTagName.ToString();
        int32 LastDot = INDEX_NONE;
        if (Wanted.FindLastChar(TEXT('.'), LastDot) && LastDot + 1 < Wanted.Len())
        {
            Wanted = Wanted.Mid(LastDot + 1);
        }
        Wanted.TrimStartAndEndInline();
        if (Wanted.IsEmpty()) return FGameplayTag();

        for (const FGameplayTag& Path : GetAll())
        {
            FString Candidate = Path.ToString();
            int32 CandidateDot = INDEX_NONE;
            if (Candidate.FindLastChar(TEXT('.'), CandidateDot) && CandidateDot + 1 < Candidate.Len())
            {
                Candidate = Candidate.Mid(CandidateDot + 1);
            }
            if (Candidate.Equals(Wanted, ESearchCase::IgnoreCase)) return Path;
        }
        return FGameplayTag();
    }
}
