#include "GuProceduralGeneratorSubsystem.h"

#include "AbilitySystemComponent.h"
#include "Engine/GameInstance.h"
#include "GuDefinitionRegistrySubsystem.h"
#include "GuEntitySubsystem.h"
#include "GuPlayerState.h"
#include "GuPersistenceSubsystem.h"
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

    bool ProcGuFindProjectileTemplate(UGuDefinitionRegistrySubsystem* Registry, FGuProjectileMechanic& OutProjectile)
    {
        if (!Registry) return false;
        for (const FGuDefinitionRecord& Record : Registry->GetAllDefinitions())
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
        Appearance.SchemaVersion = 1;
        Appearance.Transform.Scale = 0.85f + static_cast<float>(FMath::Abs(Seed % 37)) / 100.0f;
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

    if (!ProcGuIsCanonicalPathTag(Request.PrimaryPath))
    {
        OutError = TEXT("Procedural Gu require a canonical Gameplay Tag path such as Data.Paths.Moon.");
        return false;
    }

    const int32 Rank = FMath::Clamp(Request.Rank, 1, 9);
    OutEffectiveSeed = Request.Seed != 0 ? Request.Seed : FMath::RandRange(1, MAX_int32);
    const FName PrimaryPath = ProcGuPathLeaf(Request.PrimaryPath);
    const FProcGuPathAffinity Affinity = ProcGuCombinedAffinity(Request.PrimaryPath, Request.SecondaryPaths);
    FRandomStream RoleRandom(ProcGuSubSeed(OutEffectiveSeed, TEXT("role")));
    OutRole = Request.Role == EProceduralGuRole::Auto ? ProcGuPickWeightedRole(Affinity, RoleRandom) : Request.Role;
    const int32 Complexity = Request.Complexity > 0
        ? FMath::Clamp(Request.Complexity, 1, 5)
        : FMath::Clamp((Rank - 1) / 2, 0, 4);

    TArray<FString> SecondaryPathNames;
    for (const FGameplayTag& SecondaryPath : Request.SecondaryPaths.GetGameplayTagArray())
    {
        if (ProcGuIsCanonicalPathTag(SecondaryPath) && SecondaryPath != Request.PrimaryPath)
        {
            SecondaryPathNames.AddUnique(SecondaryPath.ToString());
        }
    }
    SecondaryPathNames.Sort();

    const FString SignatureSource = FString::Printf(
        TEXT("proc-v1|%s|secondary=%s|rank=%d|seed=%d|role=%s|complexity=%d|consumable=%d"),
        *Request.PrimaryPath.ToString(),
        *FString::Join(SecondaryPathNames, TEXT(",")),
        Rank,
        OutEffectiveSeed,
        *RoleToString(OutRole),
        Complexity,
        Request.bAllowConsumable ? 1 : 0);
    const uint32 Signature = ProcGuFnv1a32(SignatureSource);
    const FString StableIdText = FString::Printf(TEXT("proc_%s_r%d_%08x"), *PrimaryPath.ToString().ToLower(), Rank, Signature);
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
    Definition->Path = Request.PrimaryPath;
    Definition->SecondaryPaths.Reset();
    for (const FGameplayTag& SecondaryPath : Request.SecondaryPaths.GetGameplayTagArray())
    {
        if (ProcGuIsCanonicalPathTag(SecondaryPath) && SecondaryPath != Request.PrimaryPath)
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

    FRandomStream NameRandom(ProcGuSubSeed(OutEffectiveSeed, TEXT("name")));
    FString CandidateName = FString::Printf(TEXT("%s %s Gu"), *PrimaryPath.ToString(), ProcGuRoleNoun(OutRole, NameRandom));
    if (Registry->HasDefinition(FName(*CandidateName)))
    {
        CandidateName = FString::Printf(TEXT("%s %04X Gu"), *CandidateName.LeftChop(3), Signature & 0xffffu);
    }
    Definition->Name = FText::FromString(CandidateName);

    ProcGuApplyAppearance(PrimaryPath, OutRole, OutEffectiveSeed, Definition->Appearance);
    FRandomStream MechanicsRandom(ProcGuSubSeed(OutEffectiveSeed, TEXT("mechanics")));
    ProcGuBuildMechanicComposition(Definition, Registry, Affinity, OutRole, Rank, Complexity, MechanicsRandom);

    FGuDefinitionRecord Record;
    if (!UGuDefinitionRegistrySubsystem::BuildRecordFromAsset(Definition, Record, OutError)) return false;

    Record.Id = StableId;
    Record.Name = CandidateName;
    Record.Path = PrimaryPath;
    Record.SecondaryPaths.Reset();
    for (const FGameplayTag& Secondary : Request.SecondaryPaths.GetGameplayTagArray())
    {
        const FName Leaf = ProcGuPathLeaf(Secondary);
        if (!Leaf.IsNone() && Leaf != PrimaryPath) Record.SecondaryPaths.AddUnique(Leaf);
    }
    Record.PathRelation = Record.SecondaryPaths.IsEmpty() ? TEXT("Pure-path Gu") : TEXT("Multi-path Gu");
    Record.Category = FName(*ProcGuRoleCategory(OutRole));
    Record.FunctionalRoles = {Record.Category};
    Record.Description = FString::Printf(
        TEXT("Procedurally generated Rank %d %s-path Gu. Seed %d compiled into reusable Gu mechanics rather than a bespoke ability class."),
        Rank, *PrimaryPath.ToString(), OutEffectiveSeed);
    Record.PowerProfile.BaseBudget = ProcGuRankPowerBudget(Rank);
    Record.PowerProfile.EffectiveBudget = ProcGuRankPowerBudget(Rank);
    Record.PowerProfile.ConstraintMultiplier = Definition->Lifecycle.bConsumable ? 1.15f : 1.0f;
    Record.Source = FString::Printf(TEXT("Procedural Gu generator v1; seed=%d; role=%s"), OutEffectiveSeed, *RoleToString(OutRole));
    Record.Tags.AddUnique(TEXT("procedural"));
    Record.Tags.AddUnique(TEXT("generator:v1"));
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

    FRandomStream MechanicsRandom(ProcGuSubSeed(Request.Seed, TEXT("mechanics")));
    const FProcGuPathAffinity Affinity = ProcGuCombinedAffinity(Definition->Path, Definition->SecondaryPaths);
    ProcGuBuildMechanicComposition(Definition, GetGameInstance()->GetSubsystem<UGuDefinitionRegistrySubsystem>(), Affinity, Request.Role, Definition->Rank, Request.Complexity, MechanicsRandom);

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

