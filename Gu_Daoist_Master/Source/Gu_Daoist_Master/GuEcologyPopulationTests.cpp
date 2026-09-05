#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "GuEcologyPopulationSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGuEcologyPopulationAbundanceTest,
    "GuDaoist.World.EcologyPopulation.Abundance",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGuEcologyPopulationAbundanceTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("No habitat means no spawn chance"),
        UGuEcologyPopulationSubsystem::SpawnChanceForIntensity(0.0f), 0.0f);

    const float Weak = UGuEcologyPopulationSubsystem::SpawnChanceForIntensity(1.0f);
    const float Strong = UGuEcologyPopulationSubsystem::SpawnChanceForIntensity(6.0f);

    TestTrue(TEXT("Eligible weak habitat can support residents"), Weak > 0.0f && Weak < 1.0f);
    TestTrue(TEXT("Stronger habitat increases abundance"), Strong > Weak);
    TestTrue(TEXT("Population chance remains bounded"), Strong <= 0.92f + KINDA_SMALL_NUMBER);
    return true;
}

#endif
