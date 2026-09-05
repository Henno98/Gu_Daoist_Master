#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/GameInstance.h"
#include "GuDefinitionTypes.h"
#include "GuEntitySubsystem.h"
#include "GuEntityTypes.h"

namespace GuWildCaptureTests
{
    static FGuEntitySnapshot MakeLegacyGu(
        const FGuid EntityId,
        const FString& OwnerId,
        const EGuContainer Container)
    {
        FGuEntitySnapshot Snapshot;
        Snapshot.EntityId = EntityId;
        Snapshot.bHasGuInstance = true;
        Snapshot.GuInstance.DefinitionId = TEXT("test_wild_gu");
        Snapshot.GuCondition.bAlive = true;
        Snapshot.bHasOwner = true;
        Snapshot.OwnedBy.OwnerId = OwnerId;
        Snapshot.bHasPlacement = true;
        Snapshot.GuPlacement.Container = Container;
        Snapshot.GuStatus.HolderId = OwnerId;
        Snapshot.GuStatus.States.Add(Container == EGuContainer::World ? TEXT("Wild") : TEXT("Refined / owned"));
        return Snapshot;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGuCapturedContainerSerializationTest,
    "GuDaoist.World.WildGuCapture.ContainerValue",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGuCapturedContainerSerializationTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("Escrow retains completion-hardening value"), static_cast<uint8>(EGuContainer::Escrow), static_cast<uint8>(5));
    TestEqual(TEXT("Captured is append-only"), static_cast<uint8>(EGuContainer::Captured), static_cast<uint8>(6));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGuLegacyWillMigrationTest,
    "GuDaoist.World.WildGuCapture.LegacyWillMigration",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGuLegacyWillMigrationTest::RunTest(const FString& Parameters)
{
    UGameInstance* GI = NewObject<UGameInstance>();
    UGuEntitySubsystem* Entities = NewObject<UGuEntitySubsystem>(GI);
    FString Error;

    const FGuid WildId = FGuid::NewGuid();
    const FGuid OwnedId = FGuid::NewGuid();
    TArray<FGuEntitySnapshot> Snapshots;
    Snapshots.Add(GuWildCaptureTests::MakeLegacyGu(WildId, FString(), EGuContainer::World));
    Snapshots.Add(GuWildCaptureTests::MakeLegacyGu(OwnedId, TEXT("player:test"), EGuContainer::Storage));

    TestTrue(TEXT("Legacy Gu snapshots restore"), Entities->RestoreSnapshots(Snapshots, Error));

    FGuWillComponent WildWill;
    FGuWillComponent OwnedWill;
    TestTrue(TEXT("Legacy world Gu receives derived will state"), Entities->GetGuWillSnapshot(WildId, WildWill));
    TestTrue(TEXT("Legacy owned Gu receives derived will state"), Entities->GetGuWillSnapshot(OwnedId, OwnedWill));
    TestEqual(TEXT("Legacy world Gu migrates to Wild"), static_cast<uint8>(WildWill.State), static_cast<uint8>(EGuWillState::Wild));
    TestEqual(TEXT("Legacy owned Gu migrates to Refined"), static_cast<uint8>(OwnedWill.State), static_cast<uint8>(EGuWillState::Refined));
    TestEqual(TEXT("Legacy owned Gu master follows old owner"), OwnedWill.MasterId, FString(TEXT("player:test")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGuPhysicalCaptureAndWillRefinementTest,
    "GuDaoist.World.WildGuCapture.PhysicalThenRefine",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGuPhysicalCaptureAndWillRefinementTest::RunTest(const FString& Parameters)
{
    UGameInstance* GI = NewObject<UGameInstance>();
    UGuEntitySubsystem* Entities = NewObject<UGuEntitySubsystem>(GI);
    FString Error;

    const FGuid EntityId = FGuid::NewGuid();
    TArray<FGuEntitySnapshot> Snapshots;
    Snapshots.Add(GuWildCaptureTests::MakeLegacyGu(EntityId, FString(), EGuContainer::World));
    TestTrue(TEXT("Wild test Gu restores"), Entities->RestoreSnapshots(Snapshots, Error));

    TestTrue(TEXT("Physical capture succeeds"), Entities->MarkGuCaptured(EntityId, TEXT("player:test"), Error));
    TestTrue(TEXT("Physical identity is preserved"), Entities->HasEntity(EntityId));
    TestEqual(TEXT("Captor becomes physical holder"), Entities->GetOwnedBy(EntityId)->OwnerId, FString(TEXT("player:test")));
    TestEqual(TEXT("Captured Gu leaves ordinary inventory containers"), static_cast<uint8>(Entities->GetEntityPlacement(EntityId)->Container), static_cast<uint8>(EGuContainer::Captured));

    FGuWillComponent Will;
    TestTrue(TEXT("Captured Gu has will state"), Entities->GetGuWillSnapshot(EntityId, Will));
    TestEqual(TEXT("Will remains unrefined after physical capture"), static_cast<uint8>(Will.State), static_cast<uint8>(EGuWillState::Captured));
    TestEqual(TEXT("Will refinement starts at zero"), Will.RefinementProgress, 0.0f);

    TestFalse(TEXT("Captured unrefined Gu cannot be used"), Entities->CanUseGu(EntityId, Error));
    TestFalse(TEXT("Gu transfer API cannot bypass will refinement"), Entities->TransferGuOwnershipAndPlacement(EntityId, TEXT("player:test"), EGuContainer::Storage, Error));
    TestFalse(TEXT("Generic ownership API cannot bypass will refinement"), Entities->SetEntityOwnershipAndContainer(EntityId, TEXT("player:test"), EGuContainer::Storage, Error));
    TestTrue(TEXT("Will refinement can begin"), Entities->BeginGuWillRefinement(EntityId, TEXT("player:test"), Error));
    TestTrue(TEXT("Will refinement accepts steering progress"), Entities->AdvanceGuWillRefinement(EntityId, TEXT("player:test"), 55.0f, Error));
    TestTrue(TEXT("Will refinement can reach completion threshold"), Entities->AdvanceGuWillRefinement(EntityId, TEXT("player:test"), 45.0f, Error));
    TestTrue(TEXT("Completed will can be claimed into storage"), Entities->CompleteGuWillRefinement(EntityId, TEXT("player:test"), EGuContainer::Storage, Error));

    TestTrue(TEXT("Same FGuid survives will refinement"), Entities->HasEntity(EntityId));
    TestEqual(TEXT("Refined Gu enters requested normal container"), static_cast<uint8>(Entities->GetEntityPlacement(EntityId)->Container), static_cast<uint8>(EGuContainer::Storage));
    TestTrue(TEXT("Refined Gu becomes usable"), Entities->CanUseGu(EntityId, Error));

    TestTrue(TEXT("Final will state is readable"), Entities->GetGuWillSnapshot(EntityId, Will));
    TestEqual(TEXT("Final state is Refined"), static_cast<uint8>(Will.State), static_cast<uint8>(EGuWillState::Refined));
    TestEqual(TEXT("Master is the refiner"), Will.MasterId, FString(TEXT("player:test")));
    TestEqual(TEXT("Progress remains complete"), Will.RefinementProgress, 100.0f);

    const TArray<FGuEntitySnapshot> Saved = Entities->ExportSnapshots();
    UGameInstance* RestoreGI = NewObject<UGameInstance>();
    UGuEntitySubsystem* Restored = NewObject<UGuEntitySubsystem>(RestoreGI);
    TestTrue(TEXT("Capture/refinement state persists"), Restored->RestoreSnapshots(Saved, Error));
    TestTrue(TEXT("Restored will exists"), Restored->GetGuWillSnapshot(EntityId, Will));
    TestEqual(TEXT("Restored will remains refined"), static_cast<uint8>(Will.State), static_cast<uint8>(EGuWillState::Refined));
    TestEqual(TEXT("Restored FGuid is unchanged"), Restored->QueryGuEntities()[0], EntityId);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
