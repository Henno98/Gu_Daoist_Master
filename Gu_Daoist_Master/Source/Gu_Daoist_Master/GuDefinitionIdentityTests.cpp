#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/GameInstance.h"
#include "GuDefinitionRegistrySubsystem.h"
#include "UGuDefinition.h"

namespace GuDefinitionIdentityTests
{
    static FGuDefinitionRecord MakeCanonical(const FName Id, const TCHAR* Name)
    {
        FGuDefinitionRecord Record;
        Record.Id = Id;
        Record.Name = Name;
        Record.Rank = 1;
        Record.Path = FName(TEXT("Moon"));
        return Record;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGuDefinitionNamedAssetCanonicalizationTest,
    "GuDaoist.Definitions.Identity.NamedAssetCanonicalization",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGuDefinitionNamedAssetCanonicalizationTest::RunTest(const FString& Parameters)
{
    UGameInstance* GI = NewObject<UGameInstance>();
    UGuDefinitionRegistrySubsystem* Registry = NewObject<UGuDefinitionRegistrySubsystem>(GI);
    FString Error;

    TestTrue(TEXT("Canonical species registers"), Registry->RegisterDefinition(
        GuDefinitionIdentityTests::MakeCanonical(TEXT("moonlight_gu"), TEXT("Moonlight Gu")), Error, false));

    UGuDefinition* Asset = NewObject<UGuDefinition>(GI, TEXT("Moonlight"));
    Asset->Name = FText::FromString(TEXT("Moonlight Gu"));
    Asset->StableDefinitionId = NAME_None;

    TestTrue(TEXT("Named DataAsset binds to existing canonical species without replacing it"), Registry->RegisterDefinitionAsset(Asset, Error, false));
    TestEqual(TEXT("Asset receives canonical runtime ID"), Asset->StableDefinitionId, FName(TEXT("moonlight_gu")));
    TestNotNull(TEXT("Object-name alias resolves"), Registry->FindDefinition(TEXT("Moonlight")));
    TestEqual(TEXT("Alias resolves canonical ID"), Registry->FindDefinition(TEXT("Moonlight"))->Id, FName(TEXT("moonlight_gu")));
    TestTrue(TEXT("Executable bridge is kept on canonical ID"), Registry->FindDefinitionAsset(TEXT("moonlight_gu")) == Asset);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGuDefinitionReverseRegistrationOrderTest,
    "GuDaoist.Definitions.Identity.ReverseRegistrationOrder",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGuDefinitionReverseRegistrationOrderTest::RunTest(const FString& Parameters)
{
    UGameInstance* GI = NewObject<UGameInstance>();
    UGuDefinitionRegistrySubsystem* Registry = NewObject<UGuDefinitionRegistrySubsystem>(GI);
    FString Error;

    UGuDefinition* Asset = NewObject<UGuDefinition>(GI, TEXT("Windburst"));
    Asset->Name = FText::FromString(TEXT("Windburst Gu"));
    TestTrue(TEXT("Asset can register first"), Registry->RegisterDefinitionAsset(Asset, Error, true));
    TestEqual(TEXT("First registration uses object ID"), Asset->StableDefinitionId, FName(TEXT("Windburst")));

    TestTrue(TEXT("Later canonical record can replace identity by name"), Registry->RegisterDefinition(
        GuDefinitionIdentityTests::MakeCanonical(TEXT("windburst_gu"), TEXT("Windburst Gu")), Error, true));

    TestEqual(TEXT("Existing asset bridge retargets to canonical ID"), Asset->StableDefinitionId, FName(TEXT("windburst_gu")));
    TestNotNull(TEXT("Old object ID remains an alias"), Registry->FindDefinition(TEXT("Windburst")));
    TestEqual(TEXT("Old object ID resolves to canonical species"), Registry->FindDefinition(TEXT("Windburst"))->Id, FName(TEXT("windburst_gu")));
    TestTrue(TEXT("Asset bridge moved with the identity"), Registry->FindDefinitionAsset(TEXT("windburst_gu")) == Asset);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGuDefinitionDAFallbackTest,
    "GuDaoist.Definitions.Identity.DAFallback",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGuDefinitionDAFallbackTest::RunTest(const FString& Parameters)
{
    UGameInstance* GI = NewObject<UGameInstance>();
    UGuDefinitionRegistrySubsystem* Registry = NewObject<UGuDefinitionRegistrySubsystem>(GI);
    FString Error;

    UGuDefinition* Asset = NewObject<UGuDefinition>(GI, TEXT("DA_SkyCanopy"));
    Asset->Name = FText::GetEmpty();
    TestTrue(TEXT("Unnamed DA asset still registers"), Registry->RegisterDefinitionAsset(Asset, Error, true));
    TestEqual(TEXT("DA object name remains its stable fallback ID"), Asset->StableDefinitionId, FName(TEXT("DA_SkyCanopy")));
    TestNotNull(TEXT("DA fallback remains resolvable"), Registry->FindDefinition(TEXT("DA_SkyCanopy")));
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
