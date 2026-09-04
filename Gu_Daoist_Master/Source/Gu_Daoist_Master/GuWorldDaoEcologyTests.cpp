#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "GuWorldDaoEcologySubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGuDaoEcologyTraceScalingTest,
    "GuDaoist.World.DaoEcology.TraceScaling",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGuDaoEcologyTraceScalingTest::RunTest(const FString& Parameters)
{
    TestTrue(TEXT("Rank 1 activation is tiny"),
        FMath::IsNearlyEqual(UGuWorldDaoEcologySubsystem::TraceUnitsForRank(1), 0.00025f, 0.0000001f));
    TestTrue(TEXT("Rank 5 activation is much larger than Rank 1"),
        UGuWorldDaoEcologySubsystem::TraceUnitsForRank(5) > UGuWorldDaoEcologySubsystem::TraceUnitsForRank(1) * 100.0f);
    TestTrue(TEXT("Rank 9 activation is 850 environmental density units"),
        FMath::IsNearlyEqual(UGuWorldDaoEcologySubsystem::TraceUnitsForRank(9), 850.0f));
    TestTrue(TEXT("Ranks clamp high"),
        FMath::IsNearlyEqual(UGuWorldDaoEcologySubsystem::TraceUnitsForRank(99), 850.0f));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGuDaoEcologyTurnoverTest,
    "GuDaoist.World.DaoEcology.ExponentialTurnover",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGuDaoEcologyTurnoverTest::RunTest(const FString& Parameters)
{
    const float HalfLife = UGuWorldDaoEcologySubsystem::TraceHalfLifeYearsForRank(1);
    TestTrue(TEXT("One half-life retains half the loose residue"),
        FMath::IsNearlyEqual(UGuWorldDaoEcologySubsystem::DaoRetentionFraction(HalfLife, HalfLife), 0.5f, 0.0001f));

    const float Rate = 1.0f;
    const float Stock80 = UGuWorldDaoEcologySubsystem::ContinuousDaoStock(Rate, 80.0f, HalfLife);
    const float Stock160 = UGuWorldDaoEcologySubsystem::ContinuousDaoStock(Rate, 160.0f, HalfLife);
    const float Stock10000 = UGuWorldDaoEcologySubsystem::ContinuousDaoStock(Rate, 10000.0f, HalfLife);
    const float Equilibrium = Rate / (FMath::Loge(2.0f) / HalfLife);

    TestTrue(TEXT("Continuous input grows with time"), Stock160 > Stock80);
    TestTrue(TEXT("Continuous input is bounded by exponential-turnover equilibrium"), Stock10000 <= Equilibrium * 1.001f);
    TestTrue(TEXT("Inactive stock decays"),
        UGuWorldDaoEcologySubsystem::DecayDaoStock(100.0f, HalfLife, HalfLife) < 100.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGuDaoEcologySuccessionTest,
    "GuDaoist.World.DaoEcology.SuccessionThresholds",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGuDaoEcologySuccessionTest::RunTest(const FString& Parameters)
{
    const FGameplayTag DummyPath;

    TestTrue(TEXT("Low density stays trace"),
        UGuWorldDaoEcologySubsystem::EvaluateSuccession(DummyPath, 20.0f, 100.0f, 200.0f).Stage == EGuDaoSuccessionStage::Trace);

    TestTrue(TEXT("Influenced threshold"),
        UGuWorldDaoEcologySubsystem::EvaluateSuccession(DummyPath, 40.0f, 200.0f, 0.0f).Stage == EGuDaoSuccessionStage::Influenced);

    TestTrue(TEXT("Established requires maturity"),
        UGuWorldDaoEcologySubsystem::EvaluateSuccession(DummyPath, 80.0f, 300.0f, 8.0f).Stage == EGuDaoSuccessionStage::Established);

    TestTrue(TEXT("Aligned threshold"),
        UGuWorldDaoEcologySubsystem::EvaluateSuccession(DummyPath, 120.0f, 300.0f, 25.0f).Stage == EGuDaoSuccessionStage::Aligned);

    TestTrue(TEXT("Transformed threshold"),
        UGuWorldDaoEcologySubsystem::EvaluateSuccession(DummyPath, 220.0f, 500.0f, 60.0f).Stage == EGuDaoSuccessionStage::Transformed);

    TestTrue(TEXT("Path-domain threshold"),
        UGuWorldDaoEcologySubsystem::EvaluateSuccession(DummyPath, 450.0f, 800.0f, 160.0f).Stage == EGuDaoSuccessionStage::PathDomain);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGuDaoEcologyWildHabitatThresholdTest,
    "GuDaoist.World.DaoEcology.WildGuRankThresholds",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGuDaoEcologyWildHabitatThresholdTest::RunTest(const FString& Parameters)
{
    TestTrue(TEXT("Wild Rank 2 needs denser ecology than Rank 1"),
        UGuWorldDaoEcologySubsystem::WildGuDensityForRank(2) > UGuWorldDaoEcologySubsystem::WildGuDensityForRank(1));
    TestTrue(TEXT("Wild Rank 6 needs much older habitat than Rank 3"),
        UGuWorldDaoEcologySubsystem::WildGuMaturityYearsForRank(6) > UGuWorldDaoEcologySubsystem::WildGuMaturityYearsForRank(3) * 10.0f);
    TestTrue(TEXT("Rank 9 habitat maturity mirrors v7.9.84"),
        FMath::IsNearlyEqual(UGuWorldDaoEcologySubsystem::WildGuMaturityYearsForRank(9), 12000.0f));
    return true;
}

#endif
