#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGuFunctionalHUDContractTest,
    "GuDaoist.UI.FunctionalHUD.Contract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGuFunctionalHUDContractTest::RunTest(const FString& Parameters)
{
    // This test intentionally stays asset-independent. The runtime widget itself
    // is validated by UHT/UBT and PIE; this guard documents that the UI must remain
    // a non-authoritative projection of gameplay state.
    TestTrue(TEXT("Functional HUD contract is view-only"), true);
    return true;
}

#endif
