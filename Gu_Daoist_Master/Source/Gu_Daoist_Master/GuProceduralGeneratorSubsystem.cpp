#include "GuProceduralGeneratorSubsystem.h"

#include "AbilitySystemComponent.h"
#include "Engine/GameInstance.h"
#include "GuDefinitionRegistrySubsystem.h"
#include "GuEntitySubsystem.h"
#include "GuPlayerState.h"
#include "GuPersistenceSubsystem.h"
#include "GuPathTags.h"
#include "GuRulesLibrary.h"
#include "Gu_Daoist_MasterCharacter.h"
#include "UGuDefinition.h"

namespace
{
    struct FProcGuPathAffinity
    {
        float Offense = 0.5f;
        float Defense = 0.5f;
        float Movement = 0.5f;
        float Healing = 0.35f;
        float Control = 0.4f;
        float Investigation = 0.3f;
        float Concealment = 0.25f;
        float Resource = 0.3f;
        float Refinement = 0.15f;
        float Persistent = 0.35f;
        float Area = 0.35f;
        float Link = 0.25f;
    };

    template<typename TMechanic>
    void ProcGuAddMechanic(UGuDefinition* Definition, const TMechanic& Value)
    {
        if (!Definition) return;
        TInstancedStruct<FGuMechanic> Entry;
        Entry.InitializeAs<TMechanic>(Value);
        Definition->Mechanics.Add(MoveTemp(Entry));
    }

    FName ProcGuPathLeaf(const FGameplayTag& Tag)
    {
        if (!Tag.IsValid()) return NAME_None;
        FString Raw = Tag.ToString();
        int32 DotIndex = INDEX_NONE;
        if (Raw.FindLastChar(TEXT('.'), DotIndex) && DotIndex + 1 < Raw.Len()) Raw = Raw.Mid(DotIndex + 1);
        Raw.TrimStartAndEndInline();
        return Raw.IsEmpty() ? NAME_None : FName(*Raw);
    }

    bool ProcGuIsCanonicalPathTag(const FGameplayTag& Tag)
    {
        return Tag.IsValid() && Tag.ToString().StartsWith(TEXT("Data.Paths."), ESearchCase::CaseSensitive);
    }

    FGameplayTag ProcGuPathTagFromLeaf(const FName Leaf)
    {
        if (Leaf.IsNone()) return FGameplayTag();
        FString Raw = Leaf.ToString();
        if (!Raw.StartsWith(TEXT("Data.Paths."), ESearchCase::CaseSensitive)) Raw = TEXT("Data.Paths.") + Raw;
        return FGameplayTag::RequestGameplayTag(FName(*Raw), false);
    }

    uint32 ProcGuFnv1a32(const FString& Text)
    {
        uint32 Hash = 2166136261u;
        FTCHARToUTF8 Utf8(*Text);
        for (int32 Index = 0; Index < Utf8.Length(); ++Index)
        {
            Hash ^= static_cast<uint8>(Utf8.Get()[Index]);
            Hash *= 16777619u;
        }
        return Hash;
    }

    int32 ProcGuSubSeed(const int32 BaseSeed, const TCHAR* Domain)
    {
        const uint32 Hash = ProcGuFnv1a32(FString::Printf(TEXT("%d|%s"), BaseSeed, Domain ? Domain : TEXT("default")));
        int32 Result = static_cast<int32>(Hash & 0x7fffffffu);
        if (Result == 0) Result = 1;
        return Result;
    }

    bool ProcGuTryReadTaggedInt(const FGuDefinitionRecord& Record, const TCHAR* Prefix, int32& OutValue)
    {
        const FString PrefixString(Prefix);
        for (const FName Tag : Record.Tags)
        {
            const FString Text = Tag.ToString();
            if (!Text.StartsWith(PrefixString, ESearchCase::IgnoreCase)) continue;
            const FString Number = Text.Mid(PrefixString.Len());
            if (Number.IsNumeric())
            {
                OutValue = FCString::Atoi(*Number);
                return true;
            }
        }
        return false;
    }

    bool ProcGuTryReadTaggedString(const FGuDefinitionRecord& Record, const TCHAR* Prefix, FString& OutValue)
    {
        const FString PrefixString(Prefix);
        for (const FName Tag : Record.Tags)
        {
            const FString Text = Tag.ToString();
            if (!Text.StartsWith(PrefixString, ESearchCase::IgnoreCase)) continue;
            OutValue = Text.Mid(PrefixString.Len());
            return !OutValue.IsEmpty();
        }
        return false;
    }

    float ProcGuRankPowerBudget(const int32 Rank)
    {
        static const float MortalAndImmortal[] =
        {
            0.0f,
            22.5f,
            112.5f,
            600.0f,
            3000.0f,
            12000.0f,
            48000.0f,
            192000.0f,
            768000.0f,
            3072000.0f
        };
        return MortalAndImmortal[FMath::Clamp(Rank, 1, 9)];
    }

    FProcGuPathAffinity ProcGuAffinityForPath(const FName Path)
    {
        FProcGuPathAffinity A;
        const FString P = Path.ToString().ToLower();

        if (P == TEXT("fire"))          { A.Offense=1.0f; A.Area=.8f; A.Persistent=.75f; A.Control=.35f; A.Healing=.05f; }
        else if (P == TEXT("water"))    { A.Healing=.85f; A.Control=.65f; A.Movement=.55f; A.Area=.7f; A.Persistent=.7f; }
        else if (P == TEXT("wind"))     { A.Movement=1.0f; A.Offense=.65f; A.Control=.55f; A.Area=.6f; }
        else if (P == TEXT("earth"))    { A.Defense=1.0f; A.Control=.8f; A.Area=.75f; A.Persistent=.65f; A.Movement=.15f; }
        else if (P == TEXT("metal"))    { A.Offense=.9f; A.Defense=.8f; A.Control=.45f; A.Area=.25f; }
        else if (P == TEXT("wood"))     { A.Healing=.9f; A.Control=.65f; A.Defense=.55f; A.Persistent=.8f; A.Resource=.45f; }
        else if (P == TEXT("lightning")){ A.Offense=1.0f; A.Movement=.8f; A.Link=.95f; A.Control=.55f; }
        else if (P == TEXT("ice"))      { A.Control=1.0f; A.Defense=.75f; A.Offense=.65f; A.Persistent=.6f; }
        else if (P == TEXT("light"))    { A.Offense=.7f; A.Investigation=.85f; A.Healing=.4f; A.Concealment=.05f; }
        else if (P == TEXT("dark"))     { A.Concealment=.95f; A.Control=.7f; A.Offense=.55f; A.Investigation=.4f; }
        else if (P == TEXT("shadow"))   { A.Concealment=1.0f; A.Movement=.65f; A.Control=.6f; A.Offense=.55f; }
        else if (P == TEXT("moon"))     { A.Offense=.75f; A.Investigation=.55f; A.Concealment=.5f; A.Control=.45f; A.Link=.4f; }
        else if (P == TEXT("star"))     { A.Offense=.7f; A.Investigation=.75f; A.Link=.8f; A.Area=.55f; }
        else if (P == TEXT("cloud"))    { A.Movement=.75f; A.Concealment=.65f; A.Area=.7f; A.Persistent=.7f; }
        else if (P == TEXT("sound"))    { A.Area=.85f; A.Investigation=.65f; A.Control=.7f; A.Offense=.55f; }
        else if (P == TEXT("strength")) { A.Offense=1.0f; A.Defense=.65f; A.Movement=.45f; A.Area=.2f; }
        else if (P == TEXT("qi"))       { A.Offense=.7f; A.Defense=.65f; A.Resource=.85f; A.Area=.65f; }
        else if (P == TEXT("blood"))    { A.Offense=.85f; A.Healing=.6f; A.Resource=.75f; A.Persistent=.85f; }
        else if (P == TEXT("bone"))     { A.Defense=.85f; A.Offense=.7f; A.Control=.4f; }
        else if (P == TEXT("poison"))   { A.Offense=.85f; A.Control=.85f; A.Persistent=1.0f; A.Area=.75f; A.Healing=.05f; }
        else if (P == TEXT("soul"))     { A.Offense=.65f; A.Control=.9f; A.Investigation=.6f; A.Link=.7f; A.Persistent=.65f; }
        else if (P == TEXT("enslavement")){ A.Control=.9f; A.Resource=.7f; A.Link=.85f; A.Persistent=.7f; A.Offense=.35f; }
        else if (P == TEXT("food"))     { A.Healing=.75f; A.Resource=1.0f; A.Defense=.45f; A.Persistent=.6f; }
        else if (P == TEXT("dream"))    { A.Control=.95f; A.Concealment=.85f; A.Investigation=.6f; A.Persistent=.7f; }
        else if (P == TEXT("time"))     { A.Control=.75f; A.Movement=.7f; A.Persistent=1.0f; A.Resource=.5f; }
        else if (P == TEXT("space"))    { A.Movement=1.0f; A.Area=.8f; A.Link=.8f; A.Control=.55f; }
        else if (P == TEXT("information")){ A.Investigation=1.0f; A.Link=1.0f; A.Control=.6f; A.Concealment=.45f; }
        else if (P == TEXT("wisdom"))   { A.Investigation=.95f; A.Control=.85f; A.Link=.8f; A.Resource=.5f; }
        else if (P == TEXT("refinement")){ A.Refinement=1.0f; A.Resource=.65f; A.Control=.6f; A.Investigation=.45f; A.Offense=.1f; }
        else if (P == TEXT("formation")){ A.Area=.95f; A.Persistent=1.0f; A.Defense=.7f; A.Control=.8f; A.Refinement=.45f; }
        else if (P == TEXT("human"))    { A.Healing=.7f; A.Resource=.75f; A.Defense=.6f; A.Investigation=.45f; }
        else if (P == TEXT("heaven"))   { A.Area=.75f; A.Resource=.75f; A.Control=.65f; A.Investigation=.65f; A.Defense=.6f; }
        else if (P == TEXT("theft"))    { A.Concealment=.85f; A.Resource=.85f; A.Movement=.7f; A.Control=.65f; A.Link=.55f; }
        else if (P == TEXT("luck"))     { A.Resource=.75f; A.Defense=.55f; A.Investigation=.55f; A.Control=.35f; }
        else if (P == TEXT("transformation")){ A.Movement=.7f; A.Defense=.7f; A.Offense=.7f; A.Healing=.45f; }
        else if (P == TEXT("sword") || P == TEXT("blade") || P == TEXT("weapon")){ A.Offense=1.0f; A.Movement=.45f; A.Control=.35f; A.Area=.25f; }
        else if (P == TEXT("phantom"))  { A.Concealment=.9f; A.Movement=.8f; A.Control=.55f; }
        else if (P == TEXT("painting")) { A.Control=.65f; A.Area=.65f; A.Persistent=.7f; A.Investigation=.5f; }
        else if (P == TEXT("pill"))     { A.Healing=.9f; A.Resource=.85f; A.Refinement=.55f; A.Offense=.15f; }
        else if (P == TEXT("rule"))     { A.Control=.9f; A.Defense=.55f; A.Investigation=.55f; A.Persistent=.6f; }
        else if (P == TEXT("killing"))  { A.Offense=1.0f; A.Control=.55f; A.Persistent=.65f; A.Healing=.05f; A.Defense=.2f; }
        else if (P == TEXT("emotion"))  { A.Control=.85f; A.Concealment=.65f; A.Investigation=.7f; A.Healing=.45f; A.Link=.7f; }
        else if (P == TEXT("restriction")){ A.Control=1.0f; A.Defense=.55f; A.Persistent=.8f; A.Area=.65f; A.Offense=.35f; }

        return A;
    }

    FProcGuPathAffinity ProcGuCombinedAffinity(const FGameplayTag& PrimaryPath, const FGameplayTagContainer& SecondaryPaths)
    {
        FProcGuPathAffinity Primary = ProcGuAffinityForPath(ProcGuPathLeaf(PrimaryPath));
        TArray<FProcGuPathAffinity> SecondaryAffinities;
        for (const FGameplayTag& SecondaryPath : SecondaryPaths.GetGameplayTagArray())
        {
            if (!ProcGuIsCanonicalPathTag(SecondaryPath) || SecondaryPath == PrimaryPath) continue;
            SecondaryAffinities.Add(ProcGuAffinityForPath(ProcGuPathLeaf(SecondaryPath)));
        }
        if (SecondaryAffinities.IsEmpty()) return Primary;

        FProcGuPathAffinity Average;
        Average.Offense = Average.Defense = Average.Movement = Average.Healing = 0.0f;
        Average.Control = Average.Investigation = Average.Concealment = Average.Resource = 0.0f;
        Average.Refinement = Average.Persistent = Average.Area = Average.Link = 0.0f;
        for (const FProcGuPathAffinity& Secondary : SecondaryAffinities)
        {
            Average.Offense += Secondary.Offense; Average.Defense += Secondary.Defense;
            Average.Movement += Secondary.Movement; Average.Healing += Secondary.Healing;
            Average.Control += Secondary.Control; Average.Investigation += Secondary.Investigation;
            Average.Concealment += Secondary.Concealment; Average.Resource += Secondary.Resource;
            Average.Refinement += Secondary.Refinement; Average.Persistent += Secondary.Persistent;
            Average.Area += Secondary.Area; Average.Link += Secondary.Link;
        }
        const float InvCount = 1.0f / static_cast<float>(SecondaryAffinities.Num());
        Average.Offense *= InvCount; Average.Defense *= InvCount; Average.Movement *= InvCount; Average.Healing *= InvCount;
        Average.Control *= InvCount; Average.Investigation *= InvCount; Average.Concealment *= InvCount; Average.Resource *= InvCount;
        Average.Refinement *= InvCount; Average.Persistent *= InvCount; Average.Area *= InvCount; Average.Link *= InvCount;

        // Primary path is the dominant Dao identity. Secondary paths bend the generated behavior without replacing it.
        constexpr float PrimaryWeight = 0.72f;
        constexpr float SecondaryWeight = 1.0f - PrimaryWeight;
        Primary.Offense = Primary.Offense * PrimaryWeight + Average.Offense * SecondaryWeight;
        Primary.Defense = Primary.Defense * PrimaryWeight + Average.Defense * SecondaryWeight;
        Primary.Movement = Primary.Movement * PrimaryWeight + Average.Movement * SecondaryWeight;
        Primary.Healing = Primary.Healing * PrimaryWeight + Average.Healing * SecondaryWeight;
        Primary.Control = Primary.Control * PrimaryWeight + Average.Control * SecondaryWeight;
        Primary.Investigation = Primary.Investigation * PrimaryWeight + Average.Investigation * SecondaryWeight;
        Primary.Concealment = Primary.Concealment * PrimaryWeight + Average.Concealment * SecondaryWeight;
        Primary.Resource = Primary.Resource * PrimaryWeight + Average.Resource * SecondaryWeight;
        Primary.Refinement = Primary.Refinement * PrimaryWeight + Average.Refinement * SecondaryWeight;
        Primary.Persistent = Primary.Persistent * PrimaryWeight + Average.Persistent * SecondaryWeight;
        Primary.Area = Primary.Area * PrimaryWeight + Average.Area * SecondaryWeight;
        Primary.Link = Primary.Link * PrimaryWeight + Average.Link * SecondaryWeight;
        return Primary;
    }

    EProceduralGuRole ProcGuPickWeightedRole(const FProcGuPathAffinity& A, FRandomStream& Random)
    {
        struct FChoice { EProceduralGuRole Role; float Weight; };
        const FChoice Choices[] =
        {
            {EProceduralGuRole::Offense, A.Offense},
            {EProceduralGuRole::Defense, A.Defense},
            {EProceduralGuRole::Movement, A.Movement},
            {EProceduralGuRole::Healing, A.Healing},
            {EProceduralGuRole::Control, A.Control},
            {EProceduralGuRole::Investigation, A.Investigation},
            {EProceduralGuRole::Concealment, A.Concealment},
            {EProceduralGuRole::Resource, A.Resource},
            {EProceduralGuRole::Refinement, A.Refinement},
            {EProceduralGuRole::Support, (A.Defense + A.Healing + A.Resource) / 3.0f}
        };

        float Total = 0.0f;
        for (const FChoice& Choice : Choices) Total += FMath::Max(0.01f, Choice.Weight);
        float Cursor = Random.FRandRange(0.0f, Total);
        for (const FChoice& Choice : Choices)
        {
            Cursor -= FMath::Max(0.01f, Choice.Weight);
            if (Cursor <= 0.0f) return Choice.Role;
        }
        return EProceduralGuRole::Support;
    }

    FString ProcGuRoleCategory(const EProceduralGuRole Role)
    {
        switch (Role)
        {
        case EProceduralGuRole::Offense: return TEXT("Attack");
        case EProceduralGuRole::Defense: return TEXT("Defense");
        case EProceduralGuRole::Movement: return TEXT("Movement");
        case EProceduralGuRole::Healing: return TEXT("Healing");
        case EProceduralGuRole::Control: return TEXT("Control");
        case EProceduralGuRole::Investigation: return TEXT("Investigation");
        case EProceduralGuRole::Concealment: return TEXT("Concealment");
        case EProceduralGuRole::Resource: return TEXT("Support");
        case EProceduralGuRole::Refinement: return TEXT("Refinement");
        case EProceduralGuRole::Support: return TEXT("Support");
        default: return TEXT("Utility");
        }
    }

    EProceduralGuRole ProcGuRoleFromRecord(const FGuDefinitionRecord& Record)
    {
        const FString Template = Record.KillerMove.Template.ToString().ToLower();
        const FString Category = Record.Category.ToString().ToLower();

        FName DominantTemplate = NAME_None;
        float DominantTemplateScore = -1.0f;
        for (const TPair<FName, float>& Pair : Record.RefinementProfile.Templates)
        {
            if (Pair.Value > DominantTemplateScore)
            {
                DominantTemplate = Pair.Key;
                DominantTemplateScore = Pair.Value;
            }
        }

        const FString RefinementTemplate = DominantTemplate.ToString().ToLower();
        const FString Key = !RefinementTemplate.IsEmpty() ? RefinementTemplate : Template;

        if (Key.Contains(TEXT("projectile")) || Key.Contains(TEXT("melee")) || Key.Contains(TEXT("attack"))) return EProceduralGuRole::Offense;
        if (Key.Contains(TEXT("area")) && Category.Contains(TEXT("attack"))) return EProceduralGuRole::Offense;
        if (Key.Contains(TEXT("shield")) || Category.Contains(TEXT("defense"))) return EProceduralGuRole::Defense;
        if (Key.Contains(TEXT("move")) || Category.Contains(TEXT("movement"))) return EProceduralGuRole::Movement;
        if (Key.Contains(TEXT("heal")) || Category.Contains(TEXT("healing"))) return EProceduralGuRole::Healing;
        if (Key.Contains(TEXT("restrict")) || Category.Contains(TEXT("control"))) return EProceduralGuRole::Control;
        if (Key.Contains(TEXT("reveal")) || Category.Contains(TEXT("investigation"))) return EProceduralGuRole::Investigation;
        if (Key.Contains(TEXT("conceal")) || Category.Contains(TEXT("conceal"))) return EProceduralGuRole::Concealment;
        if (Key.Contains(TEXT("refinement")) || Category.Contains(TEXT("refinement"))) return EProceduralGuRole::Refinement;
        return EProceduralGuRole::Support;
    }

    const TCHAR* ProcGuRoleNoun(const EProceduralGuRole Role, FRandomStream& Random)
    {
        static const TCHAR* Offense[] = {TEXT("Edge"),TEXT("Fang"),TEXT("Bolt"),TEXT("Lance"),TEXT("Needle"),TEXT("Claw"),TEXT("Burst")};
        static const TCHAR* Defense[] = {TEXT("Shell"),TEXT("Ward"),TEXT("Armor"),TEXT("Curtain"),TEXT("Mantle"),TEXT("Bulwark")};
        static const TCHAR* Movement[] = {TEXT("Step"),TEXT("Wing"),TEXT("Drift"),TEXT("Passage"),TEXT("Current"),TEXT("Stride")};
        static const TCHAR* Healing[] = {TEXT("Dew"),TEXT("Breath"),TEXT("Spring"),TEXT("Salve"),TEXT("Mending"),TEXT("Pulse")};
        static const TCHAR* Control[] = {TEXT("Chain"),TEXT("Lock"),TEXT("Net"),TEXT("Binding"),TEXT("Cage"),TEXT("Seal")};
        static const TCHAR* Investigation[] = {TEXT("Eye"),TEXT("Sense"),TEXT("Mirror"),TEXT("Beacon"),TEXT("Trace"),TEXT("Lens")};
        static const TCHAR* Concealment[] = {TEXT("Veil"),TEXT("Mist"),TEXT("Cloak"),TEXT("Hush"),TEXT("Shade"),TEXT("Mask")};
        static const TCHAR* Resource[] = {TEXT("Well"),TEXT("Furnace"),TEXT("Reservoir"),TEXT("Breath"),TEXT("Pool"),TEXT("Heart")};
        static const TCHAR* Refinement[] = {TEXT("Cauldron"),TEXT("Temper"),TEXT("Crucible"),TEXT("Hand"),TEXT("Bellows"),TEXT("Seal")};
        static const TCHAR* Support[] = {TEXT("Heart"),TEXT("Bell"),TEXT("Thread"),TEXT("Crown"),TEXT("Mantle"),TEXT("Ring")};

        auto Pick = [&Random](const TCHAR* const* Values, const int32 Count) { return Values[Random.RandRange(0, Count - 1)]; };
        switch (Role)
        {
        case EProceduralGuRole::Offense: return Pick(Offense, UE_ARRAY_COUNT(Offense));
        case EProceduralGuRole::Defense: return Pick(Defense, UE_ARRAY_COUNT(Defense));
        case EProceduralGuRole::Movement: return Pick(Movement, UE_ARRAY_COUNT(Movement));
        case EProceduralGuRole::Healing: return Pick(Healing, UE_ARRAY_COUNT(Healing));
        case EProceduralGuRole::Control: return Pick(Control, UE_ARRAY_COUNT(Control));
        case EProceduralGuRole::Investigation: return Pick(Investigation, UE_ARRAY_COUNT(Investigation));
        case EProceduralGuRole::Concealment: return Pick(Concealment, UE_ARRAY_COUNT(Concealment));
        case EProceduralGuRole::Resource: return Pick(Resource, UE_ARRAY_COUNT(Resource));
        case EProceduralGuRole::Refinement: return Pick(Refinement, UE_ARRAY_COUNT(Refinement));
        default: return Pick(Support, UE_ARRAY_COUNT(Support));
        }
    }

    float ProcGuSemanticScore(const FRefinementSemanticProfile& Profile, const TCHAR* KeyText)
    {
        const FName Key(KeyText);
        return FMath::Max(
            FMath::Max(Profile.Attributes.FindRef(Key), Profile.Properties.FindRef(Key)),
            FMath::Max(Profile.Traits.FindRef(Key), Profile.Templates.FindRef(Key)));
    }

    void ProcGuApplySemanticAffinity(FProcGuPathAffinity& A, const FRefinementSemanticProfile& Profile)
    {
        const float Area = ProcGuSemanticScore(Profile, TEXT("area"));
        const float Persistence = FMath::Max(ProcGuSemanticScore(Profile, TEXT("persistence")), ProcGuSemanticScore(Profile, TEXT("duration")));
        const float Speed = ProcGuSemanticScore(Profile, TEXT("speed"));
        const float Recovery = ProcGuSemanticScore(Profile, TEXT("recovery"));
        const float Suppression = ProcGuSemanticScore(Profile, TEXT("suppression"));
        const float Precision = FMath::Max(ProcGuSemanticScore(Profile, TEXT("precision")), ProcGuSemanticScore(Profile, TEXT("tracking")));
        const float Concealment = ProcGuSemanticScore(Profile, TEXT("concealment"));
        const float Efficiency = ProcGuSemanticScore(Profile, TEXT("efficiency"));
        const float Link = ProcGuSemanticScore(Profile, TEXT("link"));
        const float Stability = ProcGuSemanticScore(Profile, TEXT("stability"));
        const float Amplification = ProcGuSemanticScore(Profile, TEXT("amplification"));
        const float Penetration = ProcGuSemanticScore(Profile, TEXT("penetration"));
        const float Poison = ProcGuSemanticScore(Profile, TEXT("poison"));
        const float Bleed = ProcGuSemanticScore(Profile, TEXT("bleed"));

        A.Area = FMath::Clamp(A.Area + Area * .55f, .01f, 1.5f);
        A.Persistent = FMath::Clamp(A.Persistent + Persistence * .55f, .01f, 1.5f);
        A.Movement = FMath::Clamp(A.Movement + Speed * .45f, .01f, 1.5f);
        A.Healing = FMath::Clamp(A.Healing + Recovery * .6f, .01f, 1.5f);
        A.Control = FMath::Clamp(A.Control + Suppression * .6f, .01f, 1.5f);
        A.Investigation = FMath::Clamp(A.Investigation + Precision * .5f, .01f, 1.5f);
        A.Concealment = FMath::Clamp(A.Concealment + Concealment * .6f, .01f, 1.5f);
        A.Resource = FMath::Clamp(A.Resource + Efficiency * .5f, .01f, 1.5f);
        A.Link = FMath::Clamp(A.Link + Link * .7f, .01f, 1.5f);
        A.Defense = FMath::Clamp(A.Defense + Stability * .45f, .01f, 1.5f);
        A.Refinement = FMath::Clamp(A.Refinement + (Stability + Precision) * .25f, .01f, 1.5f);
        A.Offense = FMath::Clamp(A.Offense + FMath::Max(FMath::Max(Amplification, Penetration), FMath::Max(Poison, Bleed)) * .5f, .01f, 1.5f);
    }

    FString ProcGuPathMotif(const FName Path, FRandomStream& Random)
    {
        const FString P = Path.ToString().ToLower();
        auto Pick = [&Random](const TCHAR* const* Values, const int32 Count) { return Values[Random.RandRange(0, Count - 1)]; };

        static const TCHAR* Moon[] = {TEXT("Crescent"),TEXT("Silver"),TEXT("Moonshadow"),TEXT("Pale"),TEXT("Nightglow"),TEXT("Lunar"),TEXT("Cold Moon"),TEXT("Moonbeam")};
        static const TCHAR* Wind[] = {TEXT("Gale"),TEXT("Breeze"),TEXT("Whistling"),TEXT("Cloudwind"),TEXT("Drifting"),TEXT("Sky"),TEXT("Restless"),TEXT("Featherwind")};
        static const TCHAR* Fire[] = {TEXT("Cinder"),TEXT("Ember"),TEXT("Scarlet"),TEXT("Ash"),TEXT("Blazing"),TEXT("Flame"),TEXT("Smoldering"),TEXT("Fireheart")};
        static const TCHAR* Water[] = {TEXT("Ripple"),TEXT("Deepwater"),TEXT("Clear Spring"),TEXT("Tide"),TEXT("Rain"),TEXT("Blue Current"),TEXT("Mistwater"),TEXT("River")};
        static const TCHAR* Wood[] = {TEXT("Greenleaf"),TEXT("Bamboo"),TEXT("Vine"),TEXT("Old Root"),TEXT("Sap"),TEXT("Verdant"),TEXT("Thornwood"),TEXT("Young Shoot")};
        static const TCHAR* Blood[] = {TEXT("Crimson"),TEXT("Bloodmist"),TEXT("Red Vein"),TEXT("Heartblood"),TEXT("Sanguine"),TEXT("Bloodmoon"),TEXT("Scarlet"),TEXT("Red Tide")};
        static const TCHAR* Lightning[] = {TEXT("Thunder"),TEXT("Lightning"),TEXT("Violet Spark"),TEXT("Storm"),TEXT("Heavenspark"),TEXT("Thunderclap"),TEXT("White Bolt"),TEXT("Stormflash")};
        static const TCHAR* Ice[] = {TEXT("Frost"),TEXT("Cold"),TEXT("Snow"),TEXT("Iceglass"),TEXT("Frozen"),TEXT("White Frost"),TEXT("Rime"),TEXT("Winter")};
        static const TCHAR* Shadow[] = {TEXT("Shadow"),TEXT("Dusk"),TEXT("Black Mist"),TEXT("Night"),TEXT("Hidden Shadow"),TEXT("Dark"),TEXT("Umbral"),TEXT("Silent Shade")};
        static const TCHAR* Soul[] = {TEXT("Soulfire"),TEXT("Ghost"),TEXT("Spirit"),TEXT("Pale Soul"),TEXT("Wandering Soul"),TEXT("Soulmist"),TEXT("Phantom"),TEXT("Soul Echo")};
        static const TCHAR* Poison[] = {TEXT("Venom"),TEXT("Miasma"),TEXT("Green Poison"),TEXT("Rot"),TEXT("Bitter"),TEXT("Toxic Mist"),TEXT("Poisonfang"),TEXT("Corroding")};
        static const TCHAR* Star[] = {TEXT("Starlight"),TEXT("Falling Star"),TEXT("Astral"),TEXT("Starry"),TEXT("Seven-Star"),TEXT("Cold Star"),TEXT("Starshadow"),TEXT("Night Star")};
        static const TCHAR* Generic[] = {TEXT("Jade"),TEXT("Iron"),TEXT("Hollow"),TEXT("Quiet"),TEXT("Flowing"),TEXT("Hidden"),TEXT("Coiling"),TEXT("Wandering"),TEXT("Falling"),TEXT("Rising"),TEXT("Returning"),TEXT("Clear")};

        if (P == TEXT("moon")) return Pick(Moon, UE_ARRAY_COUNT(Moon));
        if (P == TEXT("wind")) return Pick(Wind, UE_ARRAY_COUNT(Wind));
        if (P == TEXT("fire")) return Pick(Fire, UE_ARRAY_COUNT(Fire));
        if (P == TEXT("water")) return Pick(Water, UE_ARRAY_COUNT(Water));
        if (P == TEXT("wood")) return Pick(Wood, UE_ARRAY_COUNT(Wood));
        if (P == TEXT("blood")) return Pick(Blood, UE_ARRAY_COUNT(Blood));
        if (P == TEXT("lightning")) return Pick(Lightning, UE_ARRAY_COUNT(Lightning));
        if (P == TEXT("ice")) return Pick(Ice, UE_ARRAY_COUNT(Ice));
        if (P == TEXT("dark") || P == TEXT("shadow") || P == TEXT("phantom")) return Pick(Shadow, UE_ARRAY_COUNT(Shadow));
        if (P == TEXT("soul")) return Pick(Soul, UE_ARRAY_COUNT(Soul));
        if (P == TEXT("poison")) return Pick(Poison, UE_ARRAY_COUNT(Poison));
        if (P == TEXT("star")) return Pick(Star, UE_ARRAY_COUNT(Star));

        const FString GenericWord = Pick(Generic, UE_ARRAY_COUNT(Generic));
        if (!Path.IsNone() && Random.FRand() < .72f)
        {
            return Random.FRand() < .55f
                ? FString::Printf(TEXT("%s %s"), *Path.ToString(), *GenericWord)
                : Path.ToString();
        }
        return GenericWord;
    }

    FString ProcGuBuildGeneratedName(const FName Path, const EProceduralGuRole Role, const int32 Seed)
    {
        FRandomStream Random(ProcGuSubSeed(Seed, TEXT("name-v2")));
        const FString Motif = ProcGuPathMotif(Path, Random);
        const FString Noun = ProcGuRoleNoun(Role, Random);
        static const TCHAR* Qualifiers[] = {TEXT("Quiet"),TEXT("Swift"),TEXT("Hidden"),TEXT("Coiling"),TEXT("Falling"),TEXT("Rising"),TEXT("Wandering"),TEXT("Still"),TEXT("Piercing"),TEXT("Soft"),TEXT("Hollow"),TEXT("Flowing"),TEXT("Returning"),TEXT("Broken")};
        const FString Qualifier = Qualifiers[Random.RandRange(0, UE_ARRAY_COUNT(Qualifiers) - 1)];

        switch (Random.RandRange(0, 3))
        {
        case 0: return FString::Printf(TEXT("%s %s Gu"), *Motif, *Noun);
        case 1: return FString::Printf(TEXT("%s %s Gu"), *Qualifier, *Noun);
        case 2: return FString::Printf(TEXT("%s %s Gu"), *Qualifier, *Motif);
        default: return FString::Printf(TEXT("%s %s %s Gu"), *Motif, *Qualifier, *Noun);
        }
    }

    bool ProcGuHasMechanicStruct(const UGuDefinition* Definition, const UScriptStruct* Type);

    bool ProcGuHasImpactCarrier(const UGuDefinition* Definition)
    {
        return ProcGuHasMechanicStruct(Definition, FGuProjectileMechanic::StaticStruct())
            || ProcGuHasMechanicStruct(Definition, FGuMeleeMechanic::StaticStruct())
            || ProcGuHasMechanicStruct(Definition, FGuAreaMechanic::StaticStruct())
            || ProcGuHasMechanicStruct(Definition, FGuFieldMechanic::StaticStruct());
    }

    bool ProcGuFindProjectileTemplate(UGuDefinitionRegistrySubsystem* Registry, FGuProjectileMechanic& OutProjectile)
    {
        if (!Registry) return false;
        TArray<FGuDefinitionRecord> Records = Registry->GetAllDefinitions();
        Records.Sort([](const FGuDefinitionRecord& A, const FGuDefinitionRecord& B)
        {
            return A.Id.ToString() < B.Id.ToString();
        });
        for (const FGuDefinitionRecord& Record : Records)
        {
            const UGuDefinition* Definition = Registry->FindDefinitionAsset(Record.Id);
            if (!Definition) continue;
            for (const TInstancedStruct<FGuMechanic>& Entry : Definition->Mechanics)
            {
                if (const FGuProjectileMechanic* Projectile = Entry.GetPtr<FGuProjectileMechanic>(); Projectile && Projectile->ProjectileClass)
                {
                    OutProjectile = *Projectile;
                    return true;
                }
            }
        }
        return false;
    }

    bool ProcGuHasMechanicStruct(const UGuDefinition* Definition, const UScriptStruct* Type)
    {
        if (!Definition || !Type) return false;
        for (const TInstancedStruct<FGuMechanic>& Entry : Definition->Mechanics)
        {
            if (Entry.GetScriptStruct() == Type) return true;
        }
        return false;
    }

    void ProcGuApplyAppearance(const FName Path, const EProceduralGuRole Role, const int32 Seed, FGuAppearanceSpec& Appearance)
    {
        Appearance.Seed = Seed;
        Appearance.SchemaVersion = 2;

        FRandomStream VisualRandom(ProcGuSubSeed(Seed, TEXT("appearance-v2")));
        static const FName Archetypes[] = {TEXT("worm"),TEXT("beetle"),TEXT("moth"),TEXT("cicada"),TEXT("orb"),TEXT("seed"),TEXT("needle"),TEXT("shell")};
        static const FName Silhouettes[] = {TEXT("pearl"),TEXT("slender"),TEXT("crescent"),TEXT("winged"),TEXT("segmented"),TEXT("spined"),TEXT("leaf"),TEXT("drop")};
        static const FName Materials[] = {TEXT("jade"),TEXT("chitin"),TEXT("crystal"),TEXT("bone"),TEXT("mist"),TEXT("metal"),TEXT("wood"),TEXT("glass")};
        Appearance.Archetype = Archetypes[VisualRandom.RandRange(0, UE_ARRAY_COUNT(Archetypes) - 1)];
        Appearance.Silhouette = Silhouettes[VisualRandom.RandRange(0, UE_ARRAY_COUNT(Silhouettes) - 1)];
        Appearance.Material = Materials[VisualRandom.RandRange(0, UE_ARRAY_COUNT(Materials) - 1)];
        Appearance.Transform.Scale = 0.72f + VisualRandom.FRandRange(0.0f, 0.48f);
        Appearance.Anatomy.Segments = 2 + FMath::Abs(Seed % 7);
        Appearance.Anatomy.EyeCount = Role == EProceduralGuRole::Investigation ? 1 + FMath::Abs((Seed / 7) % 4) : FMath::Abs((Seed / 13) % 2);
        Appearance.Anatomy.LegPairs = Role == EProceduralGuRole::Movement ? 2 + FMath::Abs((Seed / 17) % 3) : FMath::Abs((Seed / 19) % 2);
        Appearance.Surface.Glow = (Role == EProceduralGuRole::Investigation || Role == EProceduralGuRole::Resource) ? .45f : .18f;
        Appearance.Surface.Opacity = Role == EProceduralGuRole::Concealment ? .65f : 1.0f;
        Appearance.Animation.Idle = Role == EProceduralGuRole::Movement ? TEXT("drift") : TEXT("float");
        Appearance.Animation.Activation = Role == EProceduralGuRole::Offense ? TEXT("flare") : TEXT("pulse");

        const FString P = Path.ToString().ToLower();
        if (P == TEXT("moon")) { Appearance.Palette.Primary=TEXT("#d6e4ff"); Appearance.Palette.Secondary=TEXT("#8fb7ff"); Appearance.Palette.Highlight=TEXT("#ffffff"); Appearance.Palette.Shadow=TEXT("#243650"); }
        else if (P == TEXT("fire")) { Appearance.Palette.Primary=TEXT("#b83a26"); Appearance.Palette.Secondary=TEXT("#f58b32"); Appearance.Palette.Highlight=TEXT("#ffd28a"); Appearance.Palette.Shadow=TEXT("#3b1712"); }
        else if (P == TEXT("wind")) { Appearance.Palette.Primary=TEXT("#bfe6d2"); Appearance.Palette.Secondary=TEXT("#73c8a5"); Appearance.Palette.Highlight=TEXT("#ecfff7"); Appearance.Palette.Shadow=TEXT("#244238"); }
        else if (P == TEXT("water")) { Appearance.Palette.Primary=TEXT("#65a9d9"); Appearance.Palette.Secondary=TEXT("#9ed8f2"); Appearance.Palette.Highlight=TEXT("#e8fbff"); Appearance.Palette.Shadow=TEXT("#19364d"); }
        else if (P == TEXT("blood")) { Appearance.Palette.Primary=TEXT("#7d1021"); Appearance.Palette.Secondary=TEXT("#c72b42"); Appearance.Palette.Highlight=TEXT("#f38a95"); Appearance.Palette.Shadow=TEXT("#26060b"); }
        else if (P == TEXT("wood")) { Appearance.Palette.Primary=TEXT("#477a42"); Appearance.Palette.Secondary=TEXT("#82b85c"); Appearance.Palette.Highlight=TEXT("#d6e8a2"); Appearance.Palette.Shadow=TEXT("#1d3520"); }
        else if (P == TEXT("lightning")) { Appearance.Palette.Primary=TEXT("#7a73d8"); Appearance.Palette.Secondary=TEXT("#c1b8ff"); Appearance.Palette.Highlight=TEXT("#f6f2ff"); Appearance.Palette.Shadow=TEXT("#27264d"); }
        else if (P == TEXT("dark") || P == TEXT("shadow")) { Appearance.Palette.Primary=TEXT("#26243a"); Appearance.Palette.Secondary=TEXT("#4d466f"); Appearance.Palette.Highlight=TEXT("#8b80bd"); Appearance.Palette.Shadow=TEXT("#0d0c14"); }
        else if (P == TEXT("refinement")) { Appearance.Palette.Primary=TEXT("#9a6843"); Appearance.Palette.Secondary=TEXT("#d0a15e"); Appearance.Palette.Highlight=TEXT("#f5dda4"); Appearance.Palette.Shadow=TEXT("#392419"); }
    }

    void ProcGuAddOffenseComposition(UGuDefinition* Definition, UGuDefinitionRegistrySubsystem* Registry, const FProcGuPathAffinity& A, const int32 Rank, const int32 Complexity, const float Budget, FRandomStream& Random)
    {
        const float CarrierRoll = Random.FRand();
        FGuProjectileMechanic ProjectileTemplate;
        const bool bCanProjectile = ProcGuFindProjectileTemplate(Registry, ProjectileTemplate);
        if (bCanProjectile && CarrierRoll < 0.52f)
        {
            ProjectileTemplate.Speed = 1050.0f + Rank * 115.0f + A.Movement * 360.0f;
            ProjectileTemplate.MaxRange = 650.0f + Rank * 170.0f + A.Investigation * 260.0f;
            ProjectileTemplate.GravityScale = 0.0f;
            ProcGuAddMechanic(Definition, ProjectileTemplate);
        }
        else if (CarrierRoll < 0.78f)
        {
            FGuMeleeMechanic Melee;
            Melee.Range = 125.0f + Rank * 18.0f + A.Movement * 35.0f;
            Melee.Radius = 35.0f + Rank * 4.0f;
            Melee.ArcDegrees = 70.0f + A.Area * 80.0f;
            Melee.MaxTargets = A.Area > .7f ? 0 : FMath::Max(1, Rank / 2);
            ProcGuAddMechanic(Definition, Melee);
        }
        else
        {
            FGuAreaMechanic Area;
            Area.Radius = 160.0f + Rank * 42.0f + A.Area * 100.0f;
            Area.ForwardOffset = 100.0f + Rank * 20.0f;
            ProcGuAddMechanic(Definition, Area);
        }

        FGuDamageMechanic Damage;
        Damage.Damage = Budget * Random.FRandRange(.68f, .98f);
        ProcGuAddMechanic(Definition, Damage);

        for (int32 Extra = 0; Extra < Complexity; ++Extra)
        {
            const float Roll = Random.FRand();
            if (A.Persistent > .55f && Roll < .24f && !ProcGuHasMechanicStruct(Definition, FGuDamageOverTimeMechanic::StaticStruct()))
            {
                FGuDamageOverTimeMechanic Dot;
                Dot.Duration = 2.5f + Rank * .65f;
                Dot.TickInterval = .5f;
                Dot.DamagePerTick = Budget * .30f / FMath::Max(1.0f, Dot.Duration / Dot.TickInterval);
                ProcGuAddMechanic(Definition, Dot);
            }
            else if (A.Link > .55f && Roll < .42f && !ProcGuHasMechanicStruct(Definition, FGuChainMechanic::StaticStruct()))
            {
                FGuChainMechanic Chain;
                Chain.JumpRadius = 300.0f + Rank * 55.0f;
                Chain.MaxAdditionalTargets = FMath::Clamp(1 + Rank / 2, 1, 6);
                Chain.MagnitudeFalloff = FMath::Clamp(.68f + Rank * .03f, .68f, .92f);
                ProcGuAddMechanic(Definition, Chain);
            }
            else if (A.Control > .55f && Roll < .67f && !ProcGuHasMechanicStruct(Definition, FGuRestrictionMechanic::StaticStruct()))
            {
                FGuRestrictionMechanic Restrict;
                Restrict.MovementMultiplier = FMath::Clamp(.78f - Rank * .055f, .25f, .78f);
                Restrict.Duration = 1.25f + Rank * .35f;
                ProcGuAddMechanic(Definition, Restrict);
            }
            else if (!ProcGuHasMechanicStruct(Definition, FGuDisplacementMechanic::StaticStruct()))
            {
                FGuDisplacementMechanic Displacement;
                Displacement.Mode = Random.FRand() < .72f ? EGuDisplacementMode::AwayFromSource : EGuDisplacementMode::Upward;
                Displacement.Strength = 280.0f + Rank * 95.0f;
                Displacement.VerticalStrength = Displacement.Mode == EGuDisplacementMode::Upward ? 180.0f + Rank * 45.0f : 0.0f;
                ProcGuAddMechanic(Definition, Displacement);
            }
        }
    }

    void ProcGuAddDefenseComposition(UGuDefinition* Definition, const FProcGuPathAffinity& A, const int32 Rank, const int32 Complexity, const float Budget, FRandomStream& Random)
    {
        FGuShieldMechanic Shield;
        Shield.Amount = Budget * Random.FRandRange(.78f, 1.12f);
        Shield.Duration = Random.FRand() < .45f ? 0.0f : 4.0f + Rank * 1.2f;
        ProcGuAddMechanic(Definition, Shield);

        for (int32 Extra = 0; Extra < Complexity; ++Extra)
        {
            const float Roll = Random.FRand();
            if (A.Healing > .45f && Roll < .38f && !ProcGuHasMechanicStruct(Definition, FGuHealOverTimeMechanic::StaticStruct()))
            {
                FGuHealOverTimeMechanic Hot;
                Hot.HealPerTick = Budget * .22f;
                Hot.TickInterval = 1.0f;
                Hot.Duration = 3.0f + Rank;
                ProcGuAddMechanic(Definition, Hot);
            }
            else if (Roll < .67f && !ProcGuHasMechanicStruct(Definition, FGuCleanseMechanic::StaticStruct()))
            {
                ProcGuAddMechanic(Definition, FGuCleanseMechanic());
            }
            else if (!ProcGuHasMechanicStruct(Definition, FGuRestrictionMechanic::StaticStruct()))
            {
                FGuRestrictionMechanic Restrict;
                Restrict.MovementMultiplier = FMath::Clamp(.85f - Rank * .04f, .45f, .85f);
                Restrict.Duration = 1.5f + Rank * .25f;
                ProcGuAddMechanic(Definition, Restrict);
            }
        }
    }

    void ProcGuAddMovementComposition(UGuDefinition* Definition, const FProcGuPathAffinity& A, const int32 Rank, const int32 Complexity, FRandomStream& Random)
    {
        FGuMovementMechanic Movement;
        const float Roll = Random.FRand();
        if (Roll < .55f)
        {
            Movement.Mode = EGuMovementMode::SpeedMultiplier;
            Movement.SpeedMultiplier = 1.15f + Rank * .055f + A.Movement * .10f;
            Movement.Duration = 3.0f + Rank * .8f;
        }
        else if (Roll < .83f)
        {
            Movement.Mode = EGuMovementMode::Dash;
            Movement.DashSpeed = 850.0f + Rank * 160.0f;
            Movement.VerticalSpeed = 0.0f;
        }
        else
        {
            Movement.Mode = EGuMovementMode::Blink;
            Movement.BlinkDistance = 280.0f + Rank * 85.0f;
        }
        ProcGuAddMechanic(Definition, Movement);

        for (int32 Extra = 0; Extra < Complexity; ++Extra)
        {
            if (A.Concealment > .45f && Random.FRand() < .5f && !ProcGuHasMechanicStruct(Definition, FGuConcealmentMechanic::StaticStruct()))
            {
                FGuConcealmentMechanic Conceal;
                Conceal.Duration = 2.5f + Rank * .8f;
                Conceal.DetectionResistance = FMath::Clamp(.14f + Rank * .065f, .14f, .75f);
                Conceal.Opacity = FMath::Clamp(.42f - Rank * .035f, .08f, .42f);
                ProcGuAddMechanic(Definition, Conceal);
            }
            else if (!ProcGuHasMechanicStruct(Definition, FGuAttentionBoostMechanic::StaticStruct()))
            {
                FGuAttentionBoostMechanic Attention;
                Attention.SlotsGranted = FMath::Clamp(1 + Rank / 3, 1, 4);
                Attention.Duration = 4.0f + Rank;
                ProcGuAddMechanic(Definition, Attention);
            }
        }
    }

    void ProcGuAddHealingComposition(UGuDefinition* Definition, const int32 Rank, const int32 Complexity, const float Budget, FRandomStream& Random)
    {
        if (Random.FRand() < .58f)
        {
            FGuHealMechanic Heal;
            Heal.Amount = Budget * Random.FRandRange(.5f, .78f);
            ProcGuAddMechanic(Definition, Heal);
        }
        else
        {
            FGuHealOverTimeMechanic Hot;
            Hot.HealPerTick = Budget * .16f;
            Hot.TickInterval = .8f;
            Hot.Duration = 4.0f + Rank * .8f;
            ProcGuAddMechanic(Definition, Hot);
        }

        for (int32 Extra = 0; Extra < Complexity; ++Extra)
        {
            if (Random.FRand() < .55f && !ProcGuHasMechanicStruct(Definition, FGuCleanseMechanic::StaticStruct()))
            {
                ProcGuAddMechanic(Definition, FGuCleanseMechanic());
            }
            else if (!ProcGuHasMechanicStruct(Definition, FGuShieldMechanic::StaticStruct()))
            {
                FGuShieldMechanic Shield;
                Shield.Amount = Budget * .35f;
                Shield.Duration = 4.0f + Rank;
                ProcGuAddMechanic(Definition, Shield);
            }
        }
    }

    void ProcGuAddControlComposition(UGuDefinition* Definition, UGuDefinitionRegistrySubsystem* Registry, const FProcGuPathAffinity& A, const int32 Rank, const int32 Complexity, FRandomStream& Random)
    {
        if (A.Area + A.Persistent > 1.25f)
        {
            FGuFieldMechanic Field;
            Field.Radius = 190.0f + Rank * 45.0f;
            Field.ForwardOffset = 120.0f;
            Field.TickInterval = .6f;
            Field.Duration = 3.0f + Rank * .7f;
            ProcGuAddMechanic(Definition, Field);
        }
        else
        {
            FGuProjectileMechanic ProjectileTemplate;
            if (ProcGuFindProjectileTemplate(Registry, ProjectileTemplate) && Random.FRand() < .5f)
            {
                ProjectileTemplate.Speed = 900.0f + Rank * 95.0f;
                ProjectileTemplate.MaxRange = 650.0f + Rank * 150.0f;
                ProcGuAddMechanic(Definition, ProjectileTemplate);
            }
            else
            {
                FGuAreaMechanic Area;
                Area.Radius = 150.0f + Rank * 35.0f;
                Area.ForwardOffset = 90.0f;
                ProcGuAddMechanic(Definition, Area);
            }
        }

        FGuRestrictionMechanic Restriction;
        Restriction.MovementMultiplier = FMath::Clamp(.72f - Rank * .06f, .18f, .72f);
        Restriction.Duration = 1.6f + Rank * .42f;
        ProcGuAddMechanic(Definition, Restriction);

        for (int32 Extra = 0; Extra < Complexity; ++Extra)
        {
            const float Roll = Random.FRand();
            if (Rank >= 2 && Roll < .28f && !ProcGuHasMechanicStruct(Definition, FGuGuSuppressionMechanic::StaticStruct()))
            {
                FGuGuSuppressionMechanic Suppress;
                Suppress.Duration = .8f + Rank * .28f;
                ProcGuAddMechanic(Definition, Suppress);
            }
            else if (Roll < .55f && !ProcGuHasMechanicStruct(Definition, FGuDisplacementMechanic::StaticStruct()))
            {
                FGuDisplacementMechanic Displacement;
                Displacement.Mode = Random.FRand() < .5f ? EGuDisplacementMode::TowardSource : EGuDisplacementMode::AwayFromSource;
                Displacement.Strength = 250.0f + Rank * 85.0f;
                ProcGuAddMechanic(Definition, Displacement);
            }
            else if (!ProcGuHasMechanicStruct(Definition, FGuMarkMechanic::StaticStruct()))
            {
                FGuMarkMechanic Mark;
                Mark.MarkId = FName(TEXT("ProceduralControlMark"));
                Mark.Strength = 1.0f + Rank * .12f;
                Mark.Duration = 3.0f + Rank;
                ProcGuAddMechanic(Definition, Mark);
            }
        }
    }

    void ProcGuAddInvestigationComposition(UGuDefinition* Definition, const FProcGuPathAffinity& A, const int32 Rank, const int32 Complexity, FRandomStream& Random)
    {
        FGuRevealMechanic Reveal;
        Reveal.Range = 650.0f + Rank * 260.0f + A.Investigation * 280.0f;
        Reveal.Duration = 2.0f + Rank * .65f;
        Reveal.Strength = .8f + Rank * .18f;
        ProcGuAddMechanic(Definition, Reveal);

        for (int32 Extra = 0; Extra < Complexity; ++Extra)
        {
            if (Random.FRand() < .65f && !ProcGuHasMechanicStruct(Definition, FGuMarkMechanic::StaticStruct()))
            {
                FGuMarkMechanic Mark;
                Mark.MarkId = FName(TEXT("ProceduralTrace"));
                Mark.Strength = .8f + Rank * .16f;
                Mark.Duration = 4.0f + Rank * 1.2f;
                ProcGuAddMechanic(Definition, Mark);
            }
            else if (!ProcGuHasMechanicStruct(Definition, FGuAttentionBoostMechanic::StaticStruct()))
            {
                FGuAttentionBoostMechanic Attention;
                Attention.SlotsGranted = FMath::Clamp(1 + Rank / 4, 1, 3);
                Attention.Duration = 4.0f + Rank;
                ProcGuAddMechanic(Definition, Attention);
            }
        }
    }

    void ProcGuAddConcealmentComposition(UGuDefinition* Definition, const FProcGuPathAffinity& A, const int32 Rank, const int32 Complexity, FRandomStream& Random)
    {
        FGuConcealmentMechanic Conceal;
        Conceal.Duration = 3.0f + Rank * 1.1f;
        Conceal.DetectionResistance = FMath::Clamp(.16f + Rank * .075f + A.Concealment * .08f, .16f, .82f);
        Conceal.Opacity = FMath::Clamp(.38f - Rank * .035f, .06f, .38f);
        Conceal.bBreakOnAttack = Random.FRand() > .18f;
        ProcGuAddMechanic(Definition, Conceal);

        for (int32 Extra = 0; Extra < Complexity; ++Extra)
        {
            if (A.Movement > .45f && Random.FRand() < .6f && !ProcGuHasMechanicStruct(Definition, FGuMovementMechanic::StaticStruct()))
            {
                FGuMovementMechanic Movement;
                Movement.Mode = EGuMovementMode::SpeedMultiplier;
                Movement.SpeedMultiplier = 1.12f + Rank * .045f;
                Movement.Duration = Conceal.Duration;
                ProcGuAddMechanic(Definition, Movement);
            }
            else if (!ProcGuHasMechanicStruct(Definition, FGuMarkMechanic::StaticStruct()))
            {
                FGuMarkMechanic Mark;
                Mark.MarkId = FName(TEXT("HiddenLink"));
                Mark.Strength = 1.0f;
                Mark.Duration = Conceal.Duration;
                ProcGuAddMechanic(Definition, Mark);
            }
        }
    }

    void ProcGuAddResourceComposition(UGuDefinition* Definition, const int32 Rank, const int32 Complexity, FRandomStream& Random)
    {
        if (Random.FRand() < .48f)
        {
            FGuEssenceChangeMechanic Essence;
            Essence.Mode = EGuEssenceChangeMode::Restore;
            Essence.Amount = 3.0f + Rank * 1.5f;
            Essence.bPercentOfMaximum = true;
            ProcGuAddMechanic(Definition, Essence);
        }
        else
        {
            FGuEssenceRegenerationMechanic Regen;
            Regen.PercentOfMaximumPerSecond = .3f + Rank * .18f;
            Regen.Duration = 4.0f + Rank * 1.2f;
            ProcGuAddMechanic(Definition, Regen);
        }

        for (int32 Extra = 0; Extra < Complexity; ++Extra)
        {
            if (Random.FRand() < .5f && !ProcGuHasMechanicStruct(Definition, FGuAttentionBoostMechanic::StaticStruct()))
            {
                FGuAttentionBoostMechanic Attention;
                Attention.SlotsGranted = FMath::Clamp(1 + Rank / 3, 1, 4);
                Attention.Duration = 5.0f + Rank;
                ProcGuAddMechanic(Definition, Attention);
            }
            else if (!ProcGuHasMechanicStruct(Definition, FGuCleanseMechanic::StaticStruct()))
            {
                ProcGuAddMechanic(Definition, FGuCleanseMechanic());
            }
        }
    }

    void ProcGuAddRefinementComposition(UGuDefinition* Definition, const int32 Rank, const int32 Complexity, FRandomStream& Random)
    {
        FGuRefinementAssistMechanic Assist;
        Assist.ProgressPercent = 4.0f + Rank * 3.0f;
        Assist.StabilityPerAction = .8f + Rank * .75f;
        Assist.ImpurityReductionPerAction = .5f + Rank * .55f;
        Assist.QualityBonus = Rank >= 3 ? Rank * .5f : 0.0f;
        Assist.ActionUses = FMath::Clamp(2 + Rank, 3, 12);
        Assist.Processes = {TEXT("Process"), TEXT("Merge"), TEXT("Purify"), TEXT("Control"), TEXT("Condense")};
        ProcGuAddMechanic(Definition, Assist);

        Definition->RefinementAssistance.bEnabled = true;
        Definition->RefinementAssistance.ProgressPercent = Assist.ProgressPercent;
        Definition->RefinementAssistance.StabilityPerAction = Assist.StabilityPerAction;
        Definition->RefinementAssistance.ImpurityReductionPerAction = Assist.ImpurityReductionPerAction;
        Definition->RefinementAssistance.QualityBonus = Assist.QualityBonus;
        Definition->RefinementAssistance.ActionUses = Assist.ActionUses;
        Definition->RefinementAssistance.Processes = Assist.Processes;

        if (Complexity > 0 && Random.FRand() < .7f)
        {
            FGuAttentionBoostMechanic Attention;
            Attention.SlotsGranted = FMath::Clamp(1 + Rank / 3, 1, 4);
            Attention.Duration = 6.0f + Rank;
            ProcGuAddMechanic(Definition, Attention);
        }
    }

    void ProcGuAddSupportComposition(UGuDefinition* Definition, const int32 Rank, const int32 Complexity, const float Budget, FRandomStream& Random)
    {
        if (Random.FRand() < .5f)
        {
            FGuShieldMechanic Shield;
            Shield.Amount = Budget * .55f;
            Shield.Duration = 4.0f + Rank;
            ProcGuAddMechanic(Definition, Shield);
        }
        else
        {
            FGuAttentionBoostMechanic Attention;
            Attention.SlotsGranted = FMath::Clamp(1 + Rank / 3, 1, 4);
            Attention.Duration = 5.0f + Rank;
            ProcGuAddMechanic(Definition, Attention);
        }

        for (int32 Extra = 0; Extra < Complexity; ++Extra)
        {
            if (Random.FRand() < .5f && !ProcGuHasMechanicStruct(Definition, FGuHealMechanic::StaticStruct()))
            {
                FGuHealMechanic Heal;
                Heal.Amount = Budget * .32f;
                ProcGuAddMechanic(Definition, Heal);
            }
            else if (!ProcGuHasMechanicStruct(Definition, FGuRevealMechanic::StaticStruct()))
            {
                FGuRevealMechanic Reveal;
                Reveal.Range = 500.0f + Rank * 180.0f;
                Reveal.Duration = 2.0f + Rank * .5f;
                ProcGuAddMechanic(Definition, Reveal);
            }
        }
    }

    enum class EProcGuDiversityModule : uint8
    {
        Chain,
        Periodic,
        Restriction,
        Displacement,
        Suppression,
        Mark,
        Area,
        Field,
        Cleanse,
        Dispel,
        Attention,
        Shield,
        Heal,
        Concealment,
        Reveal,
        EssenceDrain
    };

    bool ProcGuTryAddDiversityModule(
        UGuDefinition* Definition,
        const EProcGuDiversityModule Module,
        const EProceduralGuRole Role,
        const int32 Rank,
        const float Budget,
        FRandomStream& Random)
    {
        if (!Definition) return false;
        const bool bImpactCarrier = ProcGuHasImpactCarrier(Definition);

        switch (Module)
        {
        case EProcGuDiversityModule::Chain:
            if (!bImpactCarrier || ProcGuHasMechanicStruct(Definition, FGuChainMechanic::StaticStruct())) return false;
            {
                FGuChainMechanic Value;
                Value.JumpRadius = 260.0f + Rank * 55.0f + Random.FRandRange(0.0f, 180.0f);
                Value.MaxAdditionalTargets = FMath::Clamp(1 + Rank / 2 + Random.RandRange(0, 2), 1, 8);
                Value.MagnitudeFalloff = Random.FRandRange(.62f, .9f);
                ProcGuAddMechanic(Definition, Value);
                return true;
            }

        case EProcGuDiversityModule::Periodic:
            if (Role == EProceduralGuRole::Offense && bImpactCarrier && !ProcGuHasMechanicStruct(Definition, FGuDamageOverTimeMechanic::StaticStruct()))
            {
                FGuDamageOverTimeMechanic Value;
                Value.DamagePerTick = Budget * Random.FRandRange(.055f, .11f);
                Value.TickInterval = Random.FRandRange(.45f, 1.1f);
                Value.Duration = 2.0f + Rank * Random.FRandRange(.45f, .9f);
                Value.Recipient = EGuMechanicRecipient::ImpactTarget;
                ProcGuAddMechanic(Definition, Value);
                return true;
            }
            if ((Role == EProceduralGuRole::Healing || Role == EProceduralGuRole::Support || Role == EProceduralGuRole::Defense)
                && !ProcGuHasMechanicStruct(Definition, FGuHealOverTimeMechanic::StaticStruct()))
            {
                FGuHealOverTimeMechanic Value;
                Value.HealPerTick = Budget * Random.FRandRange(.04f, .08f);
                Value.TickInterval = Random.FRandRange(.6f, 1.2f);
                Value.Duration = 2.5f + Rank * Random.FRandRange(.45f, .85f);
                Value.Recipient = EGuMechanicRecipient::Self;
                ProcGuAddMechanic(Definition, Value);
                return true;
            }
            return false;

        case EProcGuDiversityModule::Restriction:
            if (!bImpactCarrier || ProcGuHasMechanicStruct(Definition, FGuRestrictionMechanic::StaticStruct())) return false;
            {
                FGuRestrictionMechanic Value;
                Value.MovementMultiplier = FMath::Clamp(Random.FRandRange(.25f, .72f) - Rank * .025f, .08f, .8f);
                Value.Duration = Random.FRandRange(1.0f, 2.0f) + Rank * .3f;
                ProcGuAddMechanic(Definition, Value);
                return true;
            }

        case EProcGuDiversityModule::Displacement:
            if (!bImpactCarrier || ProcGuHasMechanicStruct(Definition, FGuDisplacementMechanic::StaticStruct())) return false;
            {
                FGuDisplacementMechanic Value;
                const int32 Mode = Random.RandRange(0, 3);
                Value.Mode = static_cast<EGuDisplacementMode>(Mode);
                Value.Strength = 220.0f + Rank * 75.0f + Random.FRandRange(0.0f, 240.0f);
                Value.VerticalStrength = Mode >= 2 ? Random.FRandRange(80.0f, 340.0f) : Random.FRandRange(-40.0f, 120.0f);
                ProcGuAddMechanic(Definition, Value);
                return true;
            }

        case EProcGuDiversityModule::Suppression:
            if (!bImpactCarrier || ProcGuHasMechanicStruct(Definition, FGuGuSuppressionMechanic::StaticStruct())) return false;
            {
                FGuGuSuppressionMechanic Value;
                Value.Duration = .55f + Rank * Random.FRandRange(.16f, .34f);
                ProcGuAddMechanic(Definition, Value);
                return true;
            }

        case EProcGuDiversityModule::Mark:
            if (!bImpactCarrier || ProcGuHasMechanicStruct(Definition, FGuMarkMechanic::StaticStruct())) return false;
            {
                FGuMarkMechanic Value;
                Value.MarkId = FName(*FString::Printf(TEXT("ProceduralMark_%d"), FMath::Abs(Random.RandHelper(99991))));
                Value.Strength = .7f + Rank * .12f + Random.FRandRange(0.0f, .35f);
                Value.Duration = 2.0f + Rank * Random.FRandRange(.55f, 1.1f);
                ProcGuAddMechanic(Definition, Value);
                return true;
            }

        case EProcGuDiversityModule::Area:
            if (ProcGuHasMechanicStruct(Definition, FGuAreaMechanic::StaticStruct()) || ProcGuHasMechanicStruct(Definition, FGuFieldMechanic::StaticStruct())) return false;
            {
                FGuAreaMechanic Value;
                Value.Radius = 115.0f + Rank * 32.0f + Random.FRandRange(0.0f, 120.0f);
                Value.ForwardOffset = Random.FRandRange(40.0f, 160.0f);
                Value.MaxTargets = Random.FRand() < .55f ? 0 : FMath::Clamp(1 + Rank / 2, 1, 8);
                ProcGuAddMechanic(Definition, Value);
                return true;
            }

        case EProcGuDiversityModule::Field:
            if (ProcGuHasMechanicStruct(Definition, FGuFieldMechanic::StaticStruct())
                || ProcGuHasMechanicStruct(Definition, FGuAreaMechanic::StaticStruct())) return false;
            {
                FGuFieldMechanic Value;
                Value.Radius = 140.0f + Rank * 38.0f + Random.FRandRange(0.0f, 150.0f);
                Value.ForwardOffset = Random.FRandRange(60.0f, 180.0f);
                Value.TickInterval = Random.FRandRange(.4f, 1.0f);
                Value.Duration = 2.0f + Rank * Random.FRandRange(.45f, .9f);
                ProcGuAddMechanic(Definition, Value);
                return true;
            }

        case EProcGuDiversityModule::Cleanse:
            if (ProcGuHasMechanicStruct(Definition, FGuCleanseMechanic::StaticStruct())) return false;
            ProcGuAddMechanic(Definition, FGuCleanseMechanic());
            return true;

        case EProcGuDiversityModule::Dispel:
            if (!bImpactCarrier || ProcGuHasMechanicStruct(Definition, FGuDispelMechanic::StaticStruct())) return false;
            ProcGuAddMechanic(Definition, FGuDispelMechanic());
            return true;

        case EProcGuDiversityModule::Attention:
            if (ProcGuHasMechanicStruct(Definition, FGuAttentionBoostMechanic::StaticStruct())) return false;
            {
                FGuAttentionBoostMechanic Value;
                Value.SlotsGranted = FMath::Clamp(1 + Rank / 3 + Random.RandRange(0, 1), 1, 5);
                Value.Duration = 3.0f + Rank * Random.FRandRange(.7f, 1.2f);
                ProcGuAddMechanic(Definition, Value);
                return true;
            }

        case EProcGuDiversityModule::Shield:
            if (ProcGuHasMechanicStruct(Definition, FGuShieldMechanic::StaticStruct())) return false;
            {
                FGuShieldMechanic Value;
                Value.Amount = Budget * Random.FRandRange(.18f, .38f);
                Value.Duration = 2.5f + Rank * Random.FRandRange(.45f, .85f);
                ProcGuAddMechanic(Definition, Value);
                return true;
            }

        case EProcGuDiversityModule::Heal:
            if (ProcGuHasMechanicStruct(Definition, FGuHealMechanic::StaticStruct())) return false;
            {
                FGuHealMechanic Value;
                Value.Amount = Budget * Random.FRandRange(.15f, .32f);
                Value.Recipient = EGuMechanicRecipient::Self;
                ProcGuAddMechanic(Definition, Value);
                return true;
            }

        case EProcGuDiversityModule::Concealment:
            if (ProcGuHasMechanicStruct(Definition, FGuConcealmentMechanic::StaticStruct())) return false;
            {
                FGuConcealmentMechanic Value;
                Value.Opacity = Random.FRandRange(.08f, .3f);
                Value.DetectionResistance = FMath::Clamp(.18f + Rank * .055f + Random.FRandRange(0.0f, .18f), .18f, .85f);
                Value.Duration = 2.5f + Rank * Random.FRandRange(.55f, 1.05f);
                ProcGuAddMechanic(Definition, Value);
                return true;
            }

        case EProcGuDiversityModule::Reveal:
            if (ProcGuHasMechanicStruct(Definition, FGuRevealMechanic::StaticStruct())) return false;
            {
                FGuRevealMechanic Value;
                Value.Range = 450.0f + Rank * 170.0f + Random.FRandRange(0.0f, 350.0f);
                Value.Duration = 1.5f + Rank * Random.FRandRange(.4f, .8f);
                Value.Strength = .6f + Rank * .12f + Random.FRandRange(0.0f, .35f);
                ProcGuAddMechanic(Definition, Value);
                return true;
            }

        case EProcGuDiversityModule::EssenceDrain:
            if (!bImpactCarrier || ProcGuHasMechanicStruct(Definition, FGuEssenceChangeMechanic::StaticStruct())) return false;
            {
                FGuEssenceChangeMechanic Value;
                Value.Mode = EGuEssenceChangeMode::Drain;
                Value.Amount = FMath::Clamp(1.5f + Rank * Random.FRandRange(.7f, 1.6f), 1.0f, 22.0f);
                Value.bPercentOfMaximum = true;
                Value.Recipient = EGuMechanicRecipient::ImpactTarget;
                Value.bTransferDrainedEssenceToSource = Random.FRand() < .6f;
                ProcGuAddMechanic(Definition, Value);
                return true;
            }
        }
        return false;
    }

    void ProcGuApplyStructuralDiversity(
        UGuDefinition* Definition,
        const FProcGuPathAffinity& A,
        const FRefinementSemanticProfile* SemanticProfile,
        const EProceduralGuRole Role,
        const int32 Rank,
        const int32 Complexity,
        FRandomStream& Random)
    {
        if (!Definition) return;
        const float Budget = ProcGuRankPowerBudget(Rank);
        auto S = [SemanticProfile](const TCHAR* Key)
        {
            return SemanticProfile ? ProcGuSemanticScore(*SemanticProfile, Key) : 0.0f;
        };

        const float Link = FMath::Max(A.Link, S(TEXT("link")));
        const float Persistence = FMath::Max(A.Persistent, FMath::Max(S(TEXT("persistence")), S(TEXT("duration"))));
        const float Area = FMath::Max(A.Area, S(TEXT("area")));
        const float Control = FMath::Max(A.Control, S(TEXT("suppression")));
        const float Precision = FMath::Max(A.Investigation, FMath::Max(S(TEXT("precision")), S(TEXT("tracking"))));
        const float Recovery = FMath::Max(A.Healing, S(TEXT("recovery")));
        const float Conceal = FMath::Max(A.Concealment, S(TEXT("concealment")));
        const float Resource = FMath::Max(A.Resource, S(TEXT("efficiency")));

        struct FCandidate
        {
            EProcGuDiversityModule Module;
            float Weight;
        };
        TArray<FCandidate> Candidates;
        auto Add = [&Candidates](const EProcGuDiversityModule Module, const float Weight)
        {
            Candidates.Add({Module, FMath::Max(.01f, Weight)});
        };

        switch (Role)
        {
        case EProceduralGuRole::Offense:
            Add(EProcGuDiversityModule::Chain, .15f + Link * .8f);
            Add(EProcGuDiversityModule::Periodic, .18f + Persistence * .75f + FMath::Max(S(TEXT("poison")), S(TEXT("bleed"))) * .5f);
            Add(EProcGuDiversityModule::Restriction, .12f + Control * .55f);
            Add(EProcGuDiversityModule::Displacement, .14f + (Control + A.Movement) * .32f);
            Add(EProcGuDiversityModule::Mark, .1f + (Link + Precision) * .35f);
            Add(EProcGuDiversityModule::Area, .1f + Area * .55f);
            Add(EProcGuDiversityModule::EssenceDrain, .05f + Resource * .25f);
            break;
        case EProceduralGuRole::Defense:
            Add(EProcGuDiversityModule::Cleanse, .2f + Recovery * .4f);
            Add(EProcGuDiversityModule::Heal, .15f + Recovery * .45f);
            Add(EProcGuDiversityModule::Attention, .12f + Resource * .3f);
            Add(EProcGuDiversityModule::Concealment, .08f + Conceal * .3f);
            break;
        case EProceduralGuRole::Movement:
            Add(EProcGuDiversityModule::Concealment, .2f + Conceal * .55f);
            Add(EProcGuDiversityModule::Shield, .14f + A.Defense * .3f);
            Add(EProcGuDiversityModule::Attention, .12f + Resource * .3f);
            Add(EProcGuDiversityModule::Reveal, .08f + Precision * .22f);
            break;
        case EProceduralGuRole::Healing:
            Add(EProcGuDiversityModule::Periodic, .25f + Persistence * .6f);
            Add(EProcGuDiversityModule::Cleanse, .22f + Recovery * .45f);
            Add(EProcGuDiversityModule::Shield, .17f + A.Defense * .35f);
            Add(EProcGuDiversityModule::Attention, .08f + Resource * .25f);
            break;
        case EProceduralGuRole::Control:
            Add(EProcGuDiversityModule::Field, .15f + (Area + Persistence) * .45f);
            Add(EProcGuDiversityModule::Suppression, .22f + Control * .55f);
            Add(EProcGuDiversityModule::Displacement, .18f + (Control + A.Movement) * .3f);
            Add(EProcGuDiversityModule::Mark, .12f + Link * .45f);
            Add(EProcGuDiversityModule::Chain, .08f + Link * .35f);
            Add(EProcGuDiversityModule::Dispel, .08f + Control * .28f);
            break;
        case EProceduralGuRole::Investigation:
            Add(EProcGuDiversityModule::Attention, .2f + Precision * .5f);
            Add(EProcGuDiversityModule::Concealment, .1f + Conceal * .35f);
            Add(EProcGuDiversityModule::Shield, .08f + A.Defense * .2f);
            break;
        case EProceduralGuRole::Concealment:
            Add(EProcGuDiversityModule::Attention, .12f + Precision * .25f);
            Add(EProcGuDiversityModule::Reveal, .08f + Precision * .25f);
            Add(EProcGuDiversityModule::Shield, .08f + A.Defense * .2f);
            break;
        case EProceduralGuRole::Resource:
            Add(EProcGuDiversityModule::Attention, .22f + Resource * .45f);
            Add(EProcGuDiversityModule::Heal, .12f + Recovery * .3f);
            Add(EProcGuDiversityModule::Shield, .1f + A.Defense * .25f);
            Add(EProcGuDiversityModule::Cleanse, .1f + Recovery * .25f);
            break;
        case EProceduralGuRole::Refinement:
            Add(EProcGuDiversityModule::Attention, .25f + Precision * .4f);
            Add(EProcGuDiversityModule::Cleanse, .14f + Recovery * .25f);
            Add(EProcGuDiversityModule::Reveal, .1f + Precision * .25f);
            break;
        default:
            Add(EProcGuDiversityModule::Heal, .14f + Recovery * .3f);
            Add(EProcGuDiversityModule::Shield, .14f + A.Defense * .3f);
            Add(EProcGuDiversityModule::Reveal, .1f + Precision * .25f);
            Add(EProcGuDiversityModule::Attention, .12f + Resource * .25f);
            Add(EProcGuDiversityModule::Cleanse, .08f + Recovery * .2f);
            break;
        }

        const int32 DesiredAdds = FMath::Clamp(1 + (Complexity >= 3 ? 1 : 0) + (Rank >= 5 && Complexity >= 4 ? 1 : 0), 1, 3);
        for (int32 Added = 0, Attempts = 0; Added < DesiredAdds && Attempts < DesiredAdds * 8 && !Candidates.IsEmpty(); ++Attempts)
        {
            float TotalWeight = 0.0f;
            for (const FCandidate& Candidate : Candidates) TotalWeight += Candidate.Weight;
            float Cursor = Random.FRandRange(0.0f, FMath::Max(.01f, TotalWeight));
            int32 PickedIndex = Candidates.Num() - 1;
            for (int32 Index = 0; Index < Candidates.Num(); ++Index)
            {
                Cursor -= Candidates[Index].Weight;
                if (Cursor <= 0.0f)
                {
                    PickedIndex = Index;
                    break;
                }
            }

            const EProcGuDiversityModule Picked = Candidates[PickedIndex].Module;
            Candidates.RemoveAtSwap(PickedIndex);
            if (ProcGuTryAddDiversityModule(Definition, Picked, Role, Rank, Budget, Random)) ++Added;
        }
    }

    void ProcGuBuildMechanicComposition(UGuDefinition* Definition, UGuDefinitionRegistrySubsystem* Registry, const FProcGuPathAffinity& Affinity, const EProceduralGuRole Role, const int32 Rank, const int32 Complexity, FRandomStream& Random)
    {
        const float Budget = ProcGuRankPowerBudget(Rank);

        FGuEssenceCostMechanic Cost;
        Cost.Cost = FMath::Clamp(5.0f + Rank * 2.0f + Complexity * 1.25f + Random.FRandRange(-1.0f, 2.0f), 3.0f, 35.0f);
        ProcGuAddMechanic(Definition, Cost);

        switch (Role)
        {
        case EProceduralGuRole::Offense: ProcGuAddOffenseComposition(Definition, Registry, Affinity, Rank, Complexity, Budget, Random); break;
        case EProceduralGuRole::Defense: ProcGuAddDefenseComposition(Definition, Affinity, Rank, Complexity, Budget, Random); break;
        case EProceduralGuRole::Movement: ProcGuAddMovementComposition(Definition, Affinity, Rank, Complexity, Random); break;
        case EProceduralGuRole::Healing: ProcGuAddHealingComposition(Definition, Rank, Complexity, Budget, Random); break;
        case EProceduralGuRole::Control: ProcGuAddControlComposition(Definition, Registry, Affinity, Rank, Complexity, Random); break;
        case EProceduralGuRole::Investigation: ProcGuAddInvestigationComposition(Definition, Affinity, Rank, Complexity, Random); break;
        case EProceduralGuRole::Concealment: ProcGuAddConcealmentComposition(Definition, Affinity, Rank, Complexity, Random); break;
        case EProceduralGuRole::Resource: ProcGuAddResourceComposition(Definition, Rank, Complexity, Random); break;
        case EProceduralGuRole::Refinement: ProcGuAddRefinementComposition(Definition, Rank, Complexity, Random); break;
        case EProceduralGuRole::Support: ProcGuAddSupportComposition(Definition, Rank, Complexity, Budget, Random); break;
        default: ProcGuAddSupportComposition(Definition, Rank, Complexity, Budget, Random); break;
        }
    }

    FString ProcGuBuildSummary(const UGuDefinition* Definition)
    {
        if (!Definition) return TEXT("No definition.");
        TArray<FString> Parts;
        for (const TInstancedStruct<FGuMechanic>& Entry : Definition->Mechanics)
        {
            const UScriptStruct* Struct = Entry.GetScriptStruct();
            if (!Struct || Struct == FGuEssenceCostMechanic::StaticStruct()) continue;
            FString Name = Struct->GetName();
            Name.RemoveFromStart(TEXT("Gu"));
            Name.RemoveFromStart(TEXT("FGu"));
            Name.RemoveFromEnd(TEXT("Mechanic"));
            Parts.Add(Name);
        }
        return Parts.IsEmpty() ? TEXT("semantic-only") : FString::Join(Parts, TEXT(" + "));
    }

    void ProcGuPopulateStructureResult(const UGuDefinition* Definition, FProceduralGuGenerationResult& OutResult)
    {
        OutResult.MechanicTypes.Reset();
        if (!Definition)
        {
            OutResult.StructureSignature = NAME_None;
            return;
        }

        TArray<FString> SignatureParts;
        for (const TInstancedStruct<FGuMechanic>& Entry : Definition->Mechanics)
        {
            const UScriptStruct* Struct = Entry.GetScriptStruct();
            if (!Struct || Struct == FGuEssenceCostMechanic::StaticStruct()) continue;

            FString Name = Struct->GetName();
            Name.RemoveFromStart(TEXT("FGu"));
            Name.RemoveFromStart(TEXT("Gu"));
            Name.RemoveFromEnd(TEXT("Mechanic"));
            OutResult.MechanicTypes.AddUnique(FName(*Name));

            FString Part = Name.ToLower();
            if (const FGuMovementMechanic* Movement = Entry.GetPtr<FGuMovementMechanic>())
            {
                Part += FString::Printf(TEXT(":mode=%d"), static_cast<int32>(Movement->Mode));
            }
            else if (const FGuDisplacementMechanic* Displacement = Entry.GetPtr<FGuDisplacementMechanic>())
            {
                Part += FString::Printf(TEXT(":mode=%d"), static_cast<int32>(Displacement->Mode));
            }
            else if (const FGuEssenceChangeMechanic* Essence = Entry.GetPtr<FGuEssenceChangeMechanic>())
            {
                Part += FString::Printf(TEXT(":mode=%d:recipient=%d"), static_cast<int32>(Essence->Mode), static_cast<int32>(Essence->Recipient));
            }
            else if (const FGuHealMechanic* Heal = Entry.GetPtr<FGuHealMechanic>())
            {
                Part += FString::Printf(TEXT(":recipient=%d"), static_cast<int32>(Heal->Recipient));
            }
            else if (const FGuShieldMechanic* Shield = Entry.GetPtr<FGuShieldMechanic>())
            {
                Part += FString::Printf(TEXT(":recipient=%d"), static_cast<int32>(Shield->Recipient));
            }
            SignatureParts.Add(MoveTemp(Part));
        }

        OutResult.MechanicTypes.Sort([](const FName& A, const FName& B) { return A.ToString() < B.ToString(); });
        SignatureParts.Sort();
        const FString Canonical = FString::Join(SignatureParts, TEXT("|"));
        OutResult.StructureSignature = FName(*FString::Printf(TEXT("structure_%08x"), ProcGuFnv1a32(Canonical)));
    }
}

FString UGuProceduralGeneratorSubsystem::RoleToString(const EProceduralGuRole Role)
{
    switch (Role)
    {
    case EProceduralGuRole::Auto: return TEXT("Auto");
    case EProceduralGuRole::Offense: return TEXT("Offense");
    case EProceduralGuRole::Defense: return TEXT("Defense");
    case EProceduralGuRole::Movement: return TEXT("Movement");
    case EProceduralGuRole::Healing: return TEXT("Healing");
    case EProceduralGuRole::Control: return TEXT("Control");
    case EProceduralGuRole::Investigation: return TEXT("Investigation");
    case EProceduralGuRole::Concealment: return TEXT("Concealment");
    case EProceduralGuRole::Resource: return TEXT("Resource");
    case EProceduralGuRole::Refinement: return TEXT("Refinement");
    case EProceduralGuRole::Support: return TEXT("Support");
    default: return TEXT("Unknown");
    }
}

bool UGuProceduralGeneratorSubsystem::TryParseRole(const FString& Text, EProceduralGuRole& OutRole)
{
    const FString Key = Text.TrimStartAndEnd().ToLower();
    if (Key.IsEmpty() || Key == TEXT("auto")) OutRole = EProceduralGuRole::Auto;
    else if (Key == TEXT("offense") || Key == TEXT("attack")) OutRole = EProceduralGuRole::Offense;
    else if (Key == TEXT("defense") || Key == TEXT("defence")) OutRole = EProceduralGuRole::Defense;
    else if (Key == TEXT("movement")) OutRole = EProceduralGuRole::Movement;
    else if (Key == TEXT("healing") || Key == TEXT("heal")) OutRole = EProceduralGuRole::Healing;
    else if (Key == TEXT("control") || Key == TEXT("restriction")) OutRole = EProceduralGuRole::Control;
    else if (Key == TEXT("investigation") || Key == TEXT("reveal")) OutRole = EProceduralGuRole::Investigation;
    else if (Key == TEXT("concealment") || Key == TEXT("conceal")) OutRole = EProceduralGuRole::Concealment;
    else if (Key == TEXT("resource") || Key == TEXT("essence")) OutRole = EProceduralGuRole::Resource;
    else if (Key == TEXT("refinement")) OutRole = EProceduralGuRole::Refinement;
    else if (Key == TEXT("support")) OutRole = EProceduralGuRole::Support;
    else return false;
    return true;
}

bool UGuProceduralGeneratorSubsystem::BuildGeneratedDefinition(
    const FProceduralGuGenerationRequest& Request,
    UGuDefinition*& OutDefinition,
    FGuDefinitionRecord& OutRecord,
    int32& OutEffectiveSeed,
    EProceduralGuRole& OutRole,
    FString& OutError)
{
    OutDefinition = nullptr;
    OutRecord = FGuDefinitionRecord();

    const int32 Rank = FMath::Clamp(Request.Rank, 1, 9);
    OutEffectiveSeed = Request.Seed != 0 ? Request.Seed : FMath::RandRange(1, MAX_int32);

    FGameplayTag ResolvedPrimaryPath;
    if (Request.PrimaryPath.IsValid())
    {
        // Accept canonical Data.Paths.X and tolerate old/singular Data.Path.X-style authored values
        // by resolving the leaf against the native path vocabulary.
        ResolvedPrimaryPath = GuPathTags::FindByLeaf(ProcGuPathLeaf(Request.PrimaryPath));
        if (!ResolvedPrimaryPath.IsValid())
        {
            OutError = FString::Printf(
                TEXT("Unknown procedural Gu Path '%s'. Choose a registered Data.Paths.* tag or leave PrimaryPath empty for Auto Path."),
                *Request.PrimaryPath.ToString());
            return false;
        }
    }
    else
    {
        const TArray<FGameplayTag>& RegisteredPaths = GuPathTags::GetAll();
        if (RegisteredPaths.IsEmpty())
        {
            OutError = TEXT("No native Gu Path Gameplay Tags are registered.");
            return false;
        }
        FRandomStream PathRandom(ProcGuSubSeed(OutEffectiveSeed, TEXT("primary-path")));
        ResolvedPrimaryPath = RegisteredPaths[PathRandom.RandRange(0, RegisteredPaths.Num() - 1)];
    }

    FGameplayTagContainer ResolvedSecondaryPaths;
    for (const FGameplayTag& SecondaryPath : ResolvedSecondaryPaths.GetGameplayTagArray())
    {
        const FGameplayTag ResolvedSecondary = GuPathTags::FindByLeaf(ProcGuPathLeaf(SecondaryPath));
        if (ResolvedSecondary.IsValid() && ResolvedSecondary != ResolvedPrimaryPath)
        {
            ResolvedSecondaryPaths.AddTag(ResolvedSecondary);
        }
    }

    const FName PrimaryPath = ProcGuPathLeaf(ResolvedPrimaryPath);
    const FProcGuPathAffinity Affinity = ProcGuCombinedAffinity(ResolvedPrimaryPath, ResolvedSecondaryPaths);
    FRandomStream RoleRandom(ProcGuSubSeed(OutEffectiveSeed, TEXT("role")));
    OutRole = Request.Role == EProceduralGuRole::Auto ? ProcGuPickWeightedRole(Affinity, RoleRandom) : Request.Role;
    const int32 Complexity = Request.Complexity > 0
        ? FMath::Clamp(Request.Complexity, 1, 5)
        : FMath::Clamp((Rank - 1) / 2, 0, 4);

    TArray<FString> SecondaryPathNames;
    for (const FGameplayTag& SecondaryPath : ResolvedSecondaryPaths.GetGameplayTagArray())
    {
        SecondaryPathNames.AddUnique(SecondaryPath.ToString());
    }
    SecondaryPathNames.Sort();

    const FString SignatureSource = FString::Printf(
        TEXT("proc-v2|%s|secondary=%s|rank=%d|seed=%d|role=%s|complexity=%d|consumable=%d"),
        *ResolvedPrimaryPath.ToString(),
        *FString::Join(SecondaryPathNames, TEXT(",")),
        Rank,
        OutEffectiveSeed,
        *RoleToString(OutRole),
        Complexity,
        Request.bAllowConsumable ? 1 : 0);
    const uint32 Signature = ProcGuFnv1a32(SignatureSource);
    const FString StableIdText = FString::Printf(TEXT("proc2_%s_r%d_%08x"), *PrimaryPath.ToString().ToLower(), Rank, Signature);
    const FName StableId(*StableIdText);

    UGuDefinitionRegistrySubsystem* Registry = GetGameInstance() ? GetGameInstance()->GetSubsystem<UGuDefinitionRegistrySubsystem>() : nullptr;
    if (!Registry)
    {
        OutError = TEXT("Gu definition registry is unavailable.");
        return false;
    }

    if (const FGuDefinitionRecord* Existing = Registry->FindDefinition(StableId))
    {
        const UGuDefinition* ExistingAsset = Registry->FindDefinitionAsset(StableId);
        if (ExistingAsset)
        {
            OutDefinition = const_cast<UGuDefinition*>(ExistingAsset);
            OutRecord = *Existing;
            OutError.Reset();
            return true;
        }
    }

    UGuDefinition* Definition = NewObject<UGuDefinition>(this, NAME_None, RF_Transient);
    if (!Definition)
    {
        OutError = TEXT("Could not allocate transient Gu definition.");
        return false;
    }

    Definition->StableDefinitionId = StableId;
    Definition->Rank = Rank;
    Definition->Path = ResolvedPrimaryPath;
    Definition->SecondaryPaths.Reset();
    for (const FGameplayTag& SecondaryPath : ResolvedSecondaryPaths.GetGameplayTagArray())
    {
        if (SecondaryPath.IsValid() && SecondaryPath != ResolvedPrimaryPath)
        {
            Definition->SecondaryPaths.AddTag(SecondaryPath);
        }
    }
    Definition->ActivationModel = EGuActivationModel::Instant;
    Definition->Feeding.FoodKey = TEXT("food");
    Definition->Feeding.IntervalHours = FMath::Max(4.0f, 28.0f - Rank * 2.0f);
    FRandomStream LifecycleRandom(ProcGuSubSeed(OutEffectiveSeed, TEXT("lifecycle")));
    Definition->Lifecycle.bConsumable = Request.bAllowConsumable && LifecycleRandom.FRand() < .06f;
    Definition->Lifecycle.ConsumeOn = EGuConsumeOn::SuccessfulActivation;
    Definition->Lifecycle.Charges = Definition->Lifecycle.bConsumable ? FMath::Clamp(1 + Rank / 3, 1, 4) : 1;

    FString CandidateName = ProcGuBuildGeneratedName(PrimaryPath, OutRole, OutEffectiveSeed);
    if (Registry->HasDefinition(FName(*CandidateName)))
    {
        CandidateName = FString::Printf(TEXT("%s %04X Gu"), *CandidateName.LeftChop(3), Signature & 0xffffu);
    }
    Definition->Name = FText::FromString(CandidateName);

    ProcGuApplyAppearance(PrimaryPath, OutRole, OutEffectiveSeed, Definition->Appearance);
    FRandomStream MechanicsRandom(ProcGuSubSeed(OutEffectiveSeed, TEXT("mechanics")));
    ProcGuBuildMechanicComposition(Definition, Registry, Affinity, OutRole, Rank, Complexity, MechanicsRandom);
    FRandomStream DiversityRandom(ProcGuSubSeed(OutEffectiveSeed, TEXT("diversity-v2")));
    ProcGuApplyStructuralDiversity(Definition, Affinity, nullptr, OutRole, Rank, Complexity, DiversityRandom);

    FGuDefinitionRecord Record;
    if (!UGuDefinitionRegistrySubsystem::BuildRecordFromAsset(Definition, Record, OutError)) return false;

    Record.Id = StableId;
    Record.Name = CandidateName;
    Record.Path = PrimaryPath;
    Record.SecondaryPaths.Reset();
    for (const FGameplayTag& Secondary : ResolvedSecondaryPaths.GetGameplayTagArray())
    {
        const FName Leaf = ProcGuPathLeaf(Secondary);
        if (!Leaf.IsNone() && Leaf != PrimaryPath) Record.SecondaryPaths.AddUnique(Leaf);
    }
    Record.PathRelation = Record.SecondaryPaths.IsEmpty() ? TEXT("Pure-path Gu") : TEXT("Multi-path Gu");
    Record.Category = FName(*ProcGuRoleCategory(OutRole));
    Record.FunctionalRoles = {Record.Category};
    Record.Description = FString::Printf(
        TEXT("Procedurally compiled Rank %d %s-path Gu. Seed %d selects a deterministic composition of reusable carrier, payload, modifier, lifecycle, semantic, and appearance modules."),
        Rank, *PrimaryPath.ToString(), OutEffectiveSeed);
    Record.PowerProfile.BaseBudget = ProcGuRankPowerBudget(Rank);
    Record.PowerProfile.EffectiveBudget = ProcGuRankPowerBudget(Rank);
    Record.PowerProfile.ConstraintMultiplier = Definition->Lifecycle.bConsumable ? 1.15f : 1.0f;
    Record.Source = FString::Printf(TEXT("Procedural Gu species compiler v2; seed=%d; role=%s"), OutEffectiveSeed, *RoleToString(OutRole));
    Record.Tags.AddUnique(TEXT("procedural"));
    Record.Tags.AddUnique(TEXT("generator:v2"));
    Record.Tags.AddUnique(TEXT("origin:procedural"));
    Record.Tags.AddUnique(FName(*FString::Printf(TEXT("seed:%d"), OutEffectiveSeed)));
    Record.Tags.AddUnique(FName(*FString::Printf(TEXT("complexity:%d"), Complexity)));
    Record.Tags.AddUnique(FName(*FString::Printf(TEXT("role:%s"), *RoleToString(OutRole).ToLower())));
    Record.bCustom = true;

    Definition->RefinementProfile = Record.RefinementProfile;
    Definition->bRefinementSemanticsMaterialized = true;

    OutDefinition = Definition;
    OutRecord = MoveTemp(Record);
    OutError.Reset();
    return true;
}

bool UGuProceduralGeneratorSubsystem::GenerateAndRegisterGu(
    const FProceduralGuGenerationRequest& Request,
    FProceduralGuGenerationResult& OutResult,
    FString& OutError)
{
    OutResult = FProceduralGuGenerationResult();

    UGameInstance* GI = GetGameInstance();
    UGuPersistenceSubsystem* Persistence = GI ? GI->GetSubsystem<UGuPersistenceSubsystem>() : nullptr;
    if (Persistence && !Persistence->IsLoading())
    {
        if (!Persistence->EnsureLoaded(OutError)) return false;
    }

    UGuDefinition* Definition = nullptr;
    FGuDefinitionRecord Record;
    int32 EffectiveSeed = 0;
    EProceduralGuRole ResolvedRole = EProceduralGuRole::Auto;
    if (!BuildGeneratedDefinition(Request, Definition, Record, EffectiveSeed, ResolvedRole, OutError)) return false;

    UGuDefinitionRegistrySubsystem* Registry = GI ? GI->GetSubsystem<UGuDefinitionRegistrySubsystem>() : nullptr;
    if (!Registry)
    {
        OutError = TEXT("Gu definition registry is unavailable.");
        return false;
    }

    FName EquivalentId;
    if (Registry->FindEquivalentRuntimeDefinition(Record, EquivalentId))
    {
        const FGuDefinitionRecord* ExistingRecord = Registry->FindDefinition(EquivalentId);
        const UGuDefinition* ExistingAsset = Registry->FindDefinitionAsset(EquivalentId);
        if (ExistingRecord && ExistingAsset)
        {
            OutResult.DefinitionId = EquivalentId;
            OutResult.Name = ExistingRecord->Name;
            OutResult.Seed = EffectiveSeed;
            OutResult.Role = ResolvedRole;
            OutResult.Definition = const_cast<UGuDefinition*>(ExistingAsset);
            OutResult.Summary = ProcGuBuildSummary(OutResult.Definition);
            ProcGuPopulateStructureResult(OutResult.Definition, OutResult);
            OutResult.bReusedExistingSpecies = true;
            OutError.Reset();
            return true;
        }
    }

    const bool bAlreadyRegistered = Registry->HasDefinition(Record.Id) && Registry->FindDefinitionAsset(Record.Id);
    if (!bAlreadyRegistered)
    {
        if (!Registry->RegisterRuntimeDefinitionAsset(Record, Definition, OutError, true)) return false;
    }

    const FGuDefinitionRecord* CanonicalRecord = Registry->FindDefinition(Record.Id);
    const UGuDefinition* CanonicalAsset = Registry->FindDefinitionAsset(Record.Id);
    OutResult.DefinitionId = CanonicalRecord ? CanonicalRecord->Id : Record.Id;
    OutResult.Name = CanonicalRecord ? CanonicalRecord->Name : Record.Name;
    OutResult.Seed = EffectiveSeed;
    OutResult.Role = ResolvedRole;
    OutResult.Definition = CanonicalAsset ? const_cast<UGuDefinition*>(CanonicalAsset) : Definition;
    OutResult.Summary = ProcGuBuildSummary(OutResult.Definition);
    ProcGuPopulateStructureResult(OutResult.Definition, OutResult);
    OutResult.bReusedExistingSpecies = bAlreadyRegistered;
    OutError.Reset();

    if (Persistence) Persistence->RequestAutosave();
    return true;
}

bool UGuProceduralGeneratorSubsystem::GenerateAndGrantGu(
    AGu_Daoist_MasterCharacter* Character,
    const FProceduralGuGenerationRequest& Request,
    FProceduralGuGenerationResult& OutResult,
    FString& OutError)
{
    if (!IsValid(Character) || !Character->HasAuthority())
    {
        OutError = TEXT("Procedural Gu creation is server-authoritative and requires a valid character.");
        return false;
    }

    if (!GenerateAndRegisterGu(Request, OutResult, OutError)) return false;

    UGameInstance* GI = GetGameInstance();
    UGuEntitySubsystem* Entities = GI ? GI->GetSubsystem<UGuEntitySubsystem>() : nullptr;
    UGuDefinitionRegistrySubsystem* Registry = GI ? GI->GetSubsystem<UGuDefinitionRegistrySubsystem>() : nullptr;
    UGuPersistenceSubsystem* Persistence = GI ? GI->GetSubsystem<UGuPersistenceSubsystem>() : nullptr;
    if (!Entities || !Registry)
    {
        OutError = TEXT("Gu ECS or definition registry is unavailable.");
        return false;
    }

    AGuPlayerState* DomainPS = Character->GetPlayerState<AGuPlayerState>();
    FString OwnerId;
    if (DomainPS)
    {
        if (DomainPS->DomainCharacterId.IsEmpty())
        {
            DomainPS->SetDomainCharacterId(FString::Printf(TEXT("player:%d"), DomainPS->GetPlayerId()));
        }
        OwnerId = DomainPS->DomainCharacterId;
    }
    if (OwnerId.IsEmpty())
    {
        OwnerId = FString::Printf(TEXT("player:%s"), *Character->GetName());
    }

    if (const FGuDefinitionRecord* Record = Registry->FindDefinition(OutResult.DefinitionId))
    {
        if (Record->bUnique)
        {
            FGuid ExistingUniqueEntity;
            if (Entities->FindOwnedGuInstance(OutResult.DefinitionId, OwnerId, EGuContainer::Aperture, ExistingUniqueEntity, true))
            {
                OutResult.EntityId = ExistingUniqueEntity;
                FGameplayAbilitySpecHandle ExistingHandle;
                Character->GrantGuAbilityForEntity(ExistingUniqueEntity, OutResult.Definition, ExistingHandle, OutError);
                if (Persistence) Persistence->RequestAutosave();
                return OutError.IsEmpty();
            }
        }
    }

    const FGuid EntityId = Entities->CreateGuInstance(OutResult.DefinitionId, OwnerId, EGuContainer::Aperture);
    if (!EntityId.IsValid())
    {
        OutError = TEXT("The generated definition registered, but its physical ECS Gu could not be created.");
        return false;
    }

    FGameplayAbilitySpecHandle AbilityHandle;
    if (!Character->GrantGuAbilityForEntity(EntityId, OutResult.Definition, AbilityHandle, OutError))
    {
        Entities->DestroyEntity(EntityId);
        return false;
    }

    OutResult.EntityId = EntityId;
    OutError.Reset();
    if (Persistence) Persistence->RequestAutosave();
    return true;
}

bool UGuProceduralGeneratorSubsystem::GenerateSpeciesBatch(
    const FProceduralGuGenerationRequest& BaseRequest,
    const int32 Count,
    const int32 BatchSeed,
    TArray<FProceduralGuGenerationResult>& OutResults,
    FString& OutError)
{
    OutResults.Reset();
    const int32 TargetCount = FMath::Clamp(Count, 1, 5000);
    const int32 EffectiveBatchSeed = BatchSeed != 0 ? BatchSeed : FMath::RandRange(1, MAX_int32);
    TSet<FName> SeenDefinitions;

    const int32 MaxAttempts = TargetCount * 12;
    for (int32 Attempt = 0; Attempt < MaxAttempts && OutResults.Num() < TargetCount; ++Attempt)
    {
        FProceduralGuGenerationRequest Request = BaseRequest;
        Request.Seed = ProcGuSubSeed(EffectiveBatchSeed, *FString::Printf(TEXT("batch-species-%d"), Attempt));

        FProceduralGuGenerationResult Result;
        FString GenerateError;
        if (!GenerateAndRegisterGu(Request, Result, GenerateError))
        {
            OutError = FString::Printf(TEXT("Batch generation failed at attempt %d: %s"), Attempt, *GenerateError);
            return false;
        }

        if (Result.DefinitionId.IsNone() || SeenDefinitions.Contains(Result.DefinitionId)) continue;
        SeenDefinitions.Add(Result.DefinitionId);
        OutResults.Add(MoveTemp(Result));
    }

    if (OutResults.Num() != TargetCount)
    {
        OutError = FString::Printf(
            TEXT("Requested %d distinct procedural species but only %d unique canonical species were produced after %d deterministic attempts."),
            TargetCount,
            OutResults.Num(),
            MaxAttempts);
        return false;
    }

    OutError.Reset();
    return true;
}

bool UGuProceduralGeneratorSubsystem::BuildDefinitionFromRuntimeRecord(
    const FGuDefinitionRecord& SourceRecord,
    UGuDefinition*& OutDefinition,
    FGuDefinitionRecord& OutRecord,
    FString& OutError)
{
    OutDefinition = nullptr;
    OutRecord = SourceRecord;

    const FGameplayTag PrimaryPathTag = ProcGuPathTagFromLeaf(SourceRecord.Path);
    if (!PrimaryPathTag.IsValid())
    {
        OutError = FString::Printf(TEXT("Runtime Gu '%s' uses path '%s', but Data.Paths.%s is not registered as a Gameplay Tag."), *SourceRecord.Name, *SourceRecord.Path.ToString(), *SourceRecord.Path.ToString());
        return false;
    }

    FProceduralGuGenerationRequest Request;
    Request.PrimaryPath = PrimaryPathTag;
    Request.Rank = SourceRecord.Rank;
    Request.Role = ProcGuRoleFromRecord(SourceRecord);

    FString TaggedRole;
    EProceduralGuRole ParsedRole = EProceduralGuRole::Auto;
    if (ProcGuTryReadTaggedString(SourceRecord, TEXT("role:"), TaggedRole) && TryParseRole(TaggedRole, ParsedRole))
    {
        Request.Role = ParsedRole;
    }

    if (!ProcGuTryReadTaggedInt(SourceRecord, TEXT("seed:"), Request.Seed) || Request.Seed == 0)
    {
        const uint32 RuntimeHash = ProcGuFnv1a32(SourceRecord.Id.ToString() + TEXT("|") + SourceRecord.RefinementOrigin.SourceSignature);
        Request.Seed = static_cast<int32>(RuntimeHash & 0x7fffffffu);
        if (Request.Seed == 0) Request.Seed = 1;
    }

    int32 TaggedComplexity = 0;
    Request.Complexity = ProcGuTryReadTaggedInt(SourceRecord, TEXT("complexity:"), TaggedComplexity)
        ? FMath::Clamp(TaggedComplexity, 0, 5)
        : FMath::Clamp((SourceRecord.Rank - 1) / 2, 0, 4);
    Request.bAllowConsumable = SourceRecord.Lifecycle.bConsumable;
    for (const FName SecondaryPath : SourceRecord.SecondaryPaths)
    {
        const FGameplayTag SecondaryTag = ProcGuPathTagFromLeaf(SecondaryPath);
        if (SecondaryTag.IsValid()) Request.SecondaryPaths.AddTag(SecondaryTag);
    }

    UGuDefinition* Definition = NewObject<UGuDefinition>(this, NAME_None, RF_Transient);
    if (!Definition)
    {
        OutError = TEXT("Could not allocate executable runtime Gu definition.");
        return false;
    }

    Definition->StableDefinitionId = SourceRecord.Id;
    Definition->Name = FText::FromString(SourceRecord.Name);
    Definition->Rank = FMath::Clamp(SourceRecord.Rank, 1, 9);
    Definition->Path = PrimaryPathTag;
    Definition->SecondaryPaths = Request.SecondaryPaths;
    Definition->ActivationModel = SourceRecord.ActivationModel;
    Definition->Lifecycle = SourceRecord.Lifecycle;
    Definition->Feeding = SourceRecord.Feeding;
    Definition->RefinementTraits = SourceRecord.RefinementTraits;
    Definition->RefinementProfile = SourceRecord.RefinementProfile;
    Definition->bRefinementSemanticsMaterialized = true;
    Definition->RefinementAssistance = SourceRecord.RefinementAssistance;
    Definition->Appearance = SourceRecord.Appearance;

    auto HasCompilerTag = [&SourceRecord](const TCHAR* Wanted)
    {
        for (const FName Tag : SourceRecord.Tags)
        {
            if (Tag.ToString().Equals(Wanted, ESearchCase::IgnoreCase)) return true;
        }
        return false;
    };

    const bool bProceduralV2 = HasCompilerTag(TEXT("generator:v2"));
    const bool bSemanticCompilerV2 = HasCompilerTag(TEXT("compiler:semantic-v2"));

    FProcGuPathAffinity Affinity = ProcGuCombinedAffinity(Definition->Path, Definition->SecondaryPaths);
    if (bSemanticCompilerV2) ProcGuApplySemanticAffinity(Affinity, SourceRecord.RefinementProfile);

    FRandomStream MechanicsRandom(ProcGuSubSeed(Request.Seed, TEXT("mechanics")));
    ProcGuBuildMechanicComposition(Definition, GetGameInstance()->GetSubsystem<UGuDefinitionRegistrySubsystem>(), Affinity, Request.Role, Definition->Rank, Request.Complexity, MechanicsRandom);

    if (bProceduralV2 || bSemanticCompilerV2)
    {
        FRandomStream DiversityRandom(ProcGuSubSeed(Request.Seed, TEXT("diversity-v2")));
        ProcGuApplyStructuralDiversity(
            Definition,
            Affinity,
            bSemanticCompilerV2 ? &SourceRecord.RefinementProfile : nullptr,
            Request.Role,
            Definition->Rank,
            Request.Complexity,
            DiversityRandom);
    }

    // Runtime/refinement records own their agreed activation cost. The generated typed
    // mechanic must match that persisted contract instead of silently rerolling it.
    if (SourceRecord.EssenceCost >= 0.0f)
    {
        for (TInstancedStruct<FGuMechanic>& Entry : Definition->Mechanics)
        {
            if (FGuEssenceCostMechanic* Cost = Entry.GetMutablePtr<FGuEssenceCostMechanic>())
            {
                Cost->Cost = SourceRecord.EssenceCost;
                break;
            }
        }
    }

    // Preserve a refinement-specific assistant exactly when the outcome record already authored one.
    if (SourceRecord.RefinementAssistance.bEnabled && !ProcGuHasMechanicStruct(Definition, FGuRefinementAssistMechanic::StaticStruct()))
    {
        FGuRefinementAssistMechanic Assist;
        Assist.ProgressPercent = SourceRecord.RefinementAssistance.ProgressPercent;
        Assist.StabilityPerAction = SourceRecord.RefinementAssistance.StabilityPerAction;
        Assist.ImpurityReductionPerAction = SourceRecord.RefinementAssistance.ImpurityReductionPerAction;
        Assist.QualityBonus = SourceRecord.RefinementAssistance.QualityBonus;
        Assist.ActionUses = SourceRecord.RefinementAssistance.ActionUses;
        Assist.Processes = SourceRecord.RefinementAssistance.Processes;
        ProcGuAddMechanic(Definition, Assist);
    }

    // Keep identity/history/semantics from refinement, but rebuild the executable mechanic/effect vocabulary from the typed definition.
    FGuDefinitionRecord Compiled;
    if (!UGuDefinitionRegistrySubsystem::BuildRecordFromAsset(Definition, Compiled, OutError)) return false;
    Compiled.Id = SourceRecord.Id;
    Compiled.Name = SourceRecord.Name;
    Compiled.Path = SourceRecord.Path;
    Compiled.SecondaryPaths = SourceRecord.SecondaryPaths;
    Compiled.PathRelation = SourceRecord.PathRelation;
    Compiled.Category = SourceRecord.Category;
    Compiled.Description = SourceRecord.Description;
    Compiled.FunctionalRoles = SourceRecord.FunctionalRoles;
    Compiled.ActivationModel = SourceRecord.ActivationModel;
    Compiled.EssenceCostMode = SourceRecord.EssenceCostMode;
    Compiled.EssenceCost = SourceRecord.EssenceCost;
    Compiled.bUnique = SourceRecord.bUnique;
    Compiled.Feeding = SourceRecord.Feeding;
    Compiled.Lifecycle = SourceRecord.Lifecycle;
    Compiled.RefinementTraits = SourceRecord.RefinementTraits;
    Compiled.IntrinsicConstraints = SourceRecord.IntrinsicConstraints;
    Compiled.PowerProfile = SourceRecord.PowerProfile;
    Compiled.RefinementProfile = SourceRecord.RefinementProfile;
    Compiled.RefinementAssistance = SourceRecord.RefinementAssistance;
    Compiled.Appearance = SourceRecord.Appearance;
    Compiled.Tags = SourceRecord.Tags;
    Compiled.Source = SourceRecord.Source;
    Compiled.KnownSynergies = SourceRecord.KnownSynergies;
    Compiled.RefinementOrigin = SourceRecord.RefinementOrigin;
    Compiled.bHasRefinementOrigin = SourceRecord.bHasRefinementOrigin;
    Compiled.bCustom = true;

    OutDefinition = Definition;
    OutRecord = MoveTemp(Compiled);
    OutError.Reset();
    return true;
}

bool UGuProceduralGeneratorSubsystem::CompileAndRegisterRuntimeRecord(
    const FGuDefinitionRecord& SourceRecord,
    UGuDefinition*& OutDefinition,
    FString& OutError,
    const bool bReplaceExisting,
    FName* OutCanonicalDefinitionId)
{
    if (OutCanonicalDefinitionId) *OutCanonicalDefinitionId = SourceRecord.Id;

    UGuDefinitionRegistrySubsystem* Registry = GetGameInstance() ? GetGameInstance()->GetSubsystem<UGuDefinitionRegistrySubsystem>() : nullptr;
    if (!Registry)
    {
        OutError = TEXT("Gu definition registry is unavailable.");
        return false;
    }

    // Exact ID already restored/compiled in this GameInstance.
    if (const UGuDefinition* ExistingById = Registry->FindDefinitionAsset(SourceRecord.Id))
    {
        OutDefinition = const_cast<UGuDefinition*>(ExistingById);
        if (OutCanonicalDefinitionId) *OutCanonicalDefinitionId = SourceRecord.Id;
        OutError.Reset();
        return true;
    }

    FGuDefinitionRecord CompiledRecord;
    if (!BuildDefinitionFromRuntimeRecord(SourceRecord, OutDefinition, CompiledRecord, OutError)) return false;

    FName EquivalentId;
    if (Registry->FindEquivalentRuntimeDefinition(CompiledRecord, EquivalentId))
    {
        const UGuDefinition* ExistingAsset = Registry->FindDefinitionAsset(EquivalentId);
        if (ExistingAsset)
        {
            OutDefinition = const_cast<UGuDefinition*>(ExistingAsset);
            if (OutCanonicalDefinitionId) *OutCanonicalDefinitionId = EquivalentId;
            OutError.Reset();
            return true;
        }
    }

    if (!Registry->RegisterRuntimeDefinitionAsset(CompiledRecord, OutDefinition, OutError, bReplaceExisting)) return false;
    if (OutCanonicalDefinitionId) *OutCanonicalDefinitionId = CompiledRecord.Id;

    if (UGuPersistenceSubsystem* Persistence = GetGameInstance()->GetSubsystem<UGuPersistenceSubsystem>())
    {
        Persistence->RequestAutosave();
    }
    return true;
}

