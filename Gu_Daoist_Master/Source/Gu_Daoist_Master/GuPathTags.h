#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "NativeGameplayTags.h"

/**
 * Canonical native Gameplay Tags for Reverend Insanity Gu Paths.
 *
 * Runtime/authored code should store paths as Data.Paths.<PathName> tags.
 * These native registrations make the path vocabulary available in cooked builds,
 * Blueprint tag pickers, procedural generation, refinement, ecology, and GAS without
 * requiring editor-created GameplayTag config entries.
 */
namespace GuPathTags
{
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Paths);

    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Paths_Blade);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Paths_Blood);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Paths_Bone);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Paths_Cloud);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Paths_Dark);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Paths_Dream);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Paths_Earth);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Paths_Emotion);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Paths_Enchantment);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Paths_Enslavement);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Paths_Fire);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Paths_Food);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Paths_Formation);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Paths_Heaven);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Paths_Human);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Paths_Ice);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Paths_Illusion);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Paths_Information);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Paths_Light);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Paths_Lightning);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Paths_Luck);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Paths_Metal);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Paths_Moon);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Paths_Painting);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Paths_Phantom);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Paths_Pill);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Paths_Poison);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Paths_Qi);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Paths_Refinement);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Paths_Restriction);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Paths_Rule);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Paths_Snow);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Paths_Soul);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Paths_Sound);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Paths_Space);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Paths_Star);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Paths_Strength);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Paths_Sword);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Paths_Theft);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Paths_Time);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Paths_Transformation);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Paths_Water);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Paths_Weapon);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Paths_Wind);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Paths_Wisdom);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Paths_Wood);

    /** All concrete Gu Path tags. Does not include the Data.Paths parent. */
    GU_DAOIST_MASTER_API const TArray<FGameplayTag>& GetAll();

    /** Resolve Moon or Data.Path.Moon/Data.Paths.Moon-style input to the canonical registered tag. */
    GU_DAOIST_MASTER_API FGameplayTag FindByLeaf(FName LeafOrTagName);
}
