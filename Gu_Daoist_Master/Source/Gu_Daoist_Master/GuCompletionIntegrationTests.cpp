#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/GameInstance.h"
#include "GuDefinitionTypes.h"
#include "GuEntitySubsystem.h"
#include "GuEntityTypes.h"
#include "GuTownHonorParityLibrary.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGuContainerSerializationStabilityTest,
    "GuDaoist.Completion.Persistence.ContainerValues",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGuContainerSerializationStabilityTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("Aperture keeps v1 numeric value"), static_cast<uint8>(EGuContainer::Aperture), static_cast<uint8>(0));
    TestEqual(TEXT("Storage keeps v1 numeric value"), static_cast<uint8>(EGuContainer::Storage), static_cast<uint8>(1));
    TestEqual(TEXT("House keeps v1 numeric value"), static_cast<uint8>(EGuContainer::House), static_cast<uint8>(2));
    TestEqual(TEXT("World keeps v1 numeric value"), static_cast<uint8>(EGuContainer::World), static_cast<uint8>(3));
    TestEqual(TEXT("Consumed keeps v1 numeric value"), static_cast<uint8>(EGuContainer::Consumed), static_cast<uint8>(4));
    TestEqual(TEXT("Escrow is appended rather than inserted"), static_cast<uint8>(EGuContainer::Escrow), static_cast<uint8>(5));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGuPhysicalMaterialEscrowTest,
    "GuDaoist.Completion.ECS.PhysicalMaterialEscrow",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGuPhysicalMaterialEscrowTest::RunTest(const FString& Parameters)
{
    UGameInstance* GameInstance = NewObject<UGameInstance>();
    UGuEntitySubsystem* Entities = NewObject<UGuEntitySubsystem>(GameInstance);
    TestNotNull(TEXT("Entity subsystem can be allocated for a pure domain test"), Entities);
    if (!Entities) return false;

    FRefinementSemanticProfile Semantic;
    Semantic.DaoMass = 3.0f;
    Semantic.Paths.Add(FName(TEXT("Data.Paths.Wood")), 1.0f);

    const FGuid Source = Entities->CreateOwnedMaterialLot(
        Semantic,
        FName(TEXT("BambooSap")),
        10,
        FName(TEXT("Gathering")),
        FGuid(),
        TEXT("player:test"),
        EGuContainer::Storage);

    TestTrue(TEXT("Created material has a physical FGuid"), Source.IsValid());
    TestNotNull(TEXT("Created material has ownership"), Entities->GetOwnedBy(Source));
    TestNotNull(TEXT("Created material has placement"), Entities->GetEntityPlacement(Source));

    FGuid EscrowSplit;
    FString Error;
    TestTrue(
        TEXT("Partial escrow split succeeds"),
        Entities->MoveOrSplitMaterialLot(
            Source,
            4,
            TEXT("player:test"),
            TEXT("escrow:listing-1"),
            EGuContainer::Escrow,
            EscrowSplit,
            Error));
    TestTrue(TEXT("Partial split creates a distinct physical FGuid"), EscrowSplit.IsValid() && EscrowSplit != Source);
    TestEqual(TEXT("Source keeps six units"), Entities->GetMaterialLot(Source)->Quantity, 6);
    TestEqual(TEXT("Escrow lot has four units"), Entities->GetMaterialLot(EscrowSplit)->Quantity, 4);
    TestEqual(TEXT("Escrow owner is physical"), Entities->GetOwnedBy(EscrowSplit)->OwnerId, FString(TEXT("escrow:listing-1")));
    TestEqual(TEXT("Escrow placement is physical"), static_cast<uint8>(Entities->GetEntityPlacement(EscrowSplit)->Container), static_cast<uint8>(EGuContainer::Escrow));

    const TArray<FGuEntitySnapshot> Saved = Entities->ExportSnapshots();
    UGameInstance* RestoreGameInstance = NewObject<UGameInstance>();
    UGuEntitySubsystem* Restored = NewObject<UGuEntitySubsystem>(RestoreGameInstance);
    TestNotNull(TEXT("Restore subsystem allocated"), Restored);
    if (!Restored) return false;
    TestTrue(TEXT("Physical ECS snapshot restores"), Restored->RestoreSnapshots(Saved, Error));
    TestEqual(TEXT("Restored escrow quantity"), Restored->GetMaterialLot(EscrowSplit)->Quantity, 4);
    TestEqual(TEXT("Restored escrow owner"), Restored->GetOwnedBy(EscrowSplit)->OwnerId, FString(TEXT("escrow:listing-1")));
    TestEqual(TEXT("Restored escrow placement"), static_cast<uint8>(Restored->GetEntityPlacement(EscrowSplit)->Container), static_cast<uint8>(EGuContainer::Escrow));

    FGuid WholeMove;
    TestTrue(
        TEXT("Whole-lot handoff succeeds"),
        Restored->MoveOrSplitMaterialLot(
            EscrowSplit,
            4,
            TEXT("escrow:listing-1"),
            TEXT("player:buyer"),
            EGuContainer::Storage,
            WholeMove,
            Error));
    TestEqual(TEXT("Whole-lot handoff preserves the physical FGuid"), WholeMove, EscrowSplit);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGuV1MaterialOwnershipMigrationShapeTest,
    "GuDaoist.Completion.Persistence.V1MaterialOwnershipMigration",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGuV1MaterialOwnershipMigrationShapeTest::RunTest(const FString& Parameters)
{
    FGuEntitySnapshot Legacy;
    Legacy.EntityId = FGuid::NewGuid();
    Legacy.bHasMaterialLot = true;
    Legacy.MaterialLot.Item = FName(TEXT("LegacyMaterial"));
    Legacy.MaterialLot.Quantity = 2;
    Legacy.bHasOwner = false;
    Legacy.bHasPlacement = false;

    UGameInstance* MigrationGameInstance = NewObject<UGameInstance>();
    UGuEntitySubsystem* Restored = NewObject<UGuEntitySubsystem>(MigrationGameInstance);
    FString Error;
    TArray<FGuEntitySnapshot> LegacySnapshots;
    LegacySnapshots.Add(Legacy);
    TestTrue(TEXT("v1-shaped material snapshot restores"), Restored->RestoreSnapshots(LegacySnapshots, Error));
    TestNotNull(TEXT("v1 material receives migrated ownership"), Restored->GetOwnedBy(Legacy.EntityId));
    TestNotNull(TEXT("v1 material receives migrated placement"), Restored->GetEntityPlacement(Legacy.EntityId));
    if (const FOwnedByComponent* Owner = Restored->GetOwnedBy(Legacy.EntityId))
    {
        TestEqual(TEXT("v1 material migrates to legacy player owner"), Owner->OwnerId, FString(TEXT("player")));
    }
    if (const FGuPlacementComponent* Placement = Restored->GetEntityPlacement(Legacy.EntityId))
    {
        TestEqual(TEXT("v1 material migrates to Storage"), static_cast<uint8>(Placement->Container), static_cast<uint8>(EGuContainer::Storage));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGuHonorParityCatalogTest,
    "GuDaoist.Completion.Economy.HonorParity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGuHonorParityCatalogTest::RunTest(const FString& Parameters)
{
    const TArray<FGuHonorMissionDefinition> Missions = UGuTownHonorParityLibrary::GetHonorMissions();
    TestEqual(TEXT("Browser parity has ten Honor missions"), Missions.Num(), 10);

    FGuHonorMissionProgressView Progress;
    Progress.CultivationRank = 3;
    Progress.OwnedLivingGuCount = 8;
    Progress.BattleVictories = 5;
    Progress.GardenPlotCount = 6;
    Progress.AuctionWins = 1;
    Progress.bHasBoundBeast = true;
    Progress.NpcStanding.Add(FName(TEXT("Chen Yu")), 20);
    Progress.NpcStanding.Add(FName(TEXT("Zhao Feng")), 20);
    Progress.NpcStanding.Add(FName(TEXT("Elder Song")), 20);

    for (const FGuHonorMissionDefinition& Mission : Missions)
    {
        TestTrue(*FString::Printf(TEXT("%s completion predicate"), *Mission.Id.ToString()),
            UGuTownHonorParityLibrary::IsHonorMissionComplete(Mission.Id, Progress));
    }

    FGuHonorReward Reward;
    TestTrue(TEXT("m10 reward resolves"), UGuTownHonorParityLibrary::ComputeHonorMissionReward(FName(TEXT("m10")), 1.0f, Reward));
    TestEqual(TEXT("m10 stones"), Reward.Stones, 300);
    TestEqual(TEXT("m10 Honor"), Reward.Honor, 35);
    TestEqual(TEXT("NPC-linked mission adds standing"), Reward.NpcStandingDelta, 5);

    const TArray<FGuHonorShopItemDefinition> Shop = UGuTownHonorParityLibrary::GetHonorShopItems();
    TestEqual(TEXT("Browser parity has two Honor shop entries"), Shop.Num(), 2);
    TestEqual(TEXT("Relic costs 40 Honor"), Shop[0].HonorCost, 40);
    TestEqual(TEXT("Stipend costs 25 Honor"), Shop[1].HonorCost, 25);
    TestEqual(TEXT("Stipend grants 500 stones"), Shop[1].StoneGrant, 500);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
