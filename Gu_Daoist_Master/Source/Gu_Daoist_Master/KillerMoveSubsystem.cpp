#include "KillerMoveSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GuDefinitionRegistrySubsystem.h"
#include "GuEntitySubsystem.h"
#include "GuPlayerState.h"
#include "KillerMoveDefinition.h"
#include "MentalResourceComponent.h"
#include "TimerManager.h"

namespace
{
    FString RoleLabel(const EKillerMoveRole Role)
    {
        if (const UEnum* Enum = StaticEnum<EKillerMoveRole>())
        {
            return Enum->GetDisplayNameTextByValue(static_cast<int64>(Role)).ToString();
        }
        return TEXT("Control");
    }

    bool IsDeliveryTargetRole(const EKillerMoveRole Role)
    {
        return Role == EKillerMoveRole::Targeting
            || Role == EKillerMoveRole::InvestigationSensor
            || Role == EKillerMoveRole::RecognitionValidation
            || Role == EKillerMoveRole::Routing
            || Role == EKillerMoveRole::Boundary
            || Role == EKillerMoveRole::Anchor
            || Role == EKillerMoveRole::Trigger
            || Role == EKillerMoveRole::Storage
            || Role == EKillerMoveRole::Termination
            || Role == EKillerMoveRole::Switching;
    }
}

void UKillerMoveSubsystem::Deinitialize()
{
    if (UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr)
    {
        for (TPair<FString, FRuntimeSession>& Pair : Sessions)
        {
            World->GetTimerManager().ClearTimer(Pair.Value.DeadlineTimer);
            ReleaseAllAttention(Pair.Value);
        }
    }
    Sessions.Reset();
    Super::Deinitialize();
}

float UKillerMoveSubsystem::ServerWorldTime() const
{
    const UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
    if (!World) return 0.0f;
    if (const AGameStateBase* GS = World->GetGameState()) return GS->GetServerWorldTimeSeconds();
    return World->GetTimeSeconds();
}

float UKillerMoveSubsystem::EffectiveWindow(const AGuPlayerState* PlayerState, const FKillerMoveInputStep& Step) const
{
    const int32 FocusLevel = PlayerState && PlayerState->MentalResources
        ? FMath::Max(1, PlayerState->MentalResources->FocusControlLevel)
        : 1;
    const float FocusBonus = FMath::Min(0.20f, static_cast<float>(FocusLevel - 1) * 0.015f);
    return FMath::Clamp(Step.TimingWindow + FocusBonus, 0.03f, 2.0f);
}

FName UKillerMoveSubsystem::AttentionKey(const FRuntimeSession& Session, const int32 SlotIndex) const
{
    return FName(*FString::Printf(TEXT("killer-move:%s:%s:%d"), *Session.OwnerId, *Session.SessionId.ToString(EGuidFormats::Digits), SlotIndex));
}

FName UKillerMoveSubsystem::BranchForRole(const EKillerMoveRole Role)
{
    switch (Role)
    {
    case EKillerMoveRole::Core:
    case EKillerMoveRole::Output:
    case EKillerMoveRole::Amplification:
    case EKillerMoveRole::Suppression:
    case EKillerMoveRole::Concealment:
        return TEXT("phenomenon");
    case EKillerMoveRole::Medium:
    case EKillerMoveRole::Link:
    case EKillerMoveRole::Routing:
    case EKillerMoveRole::Boundary:
    case EKillerMoveRole::Anchor:
    case EKillerMoveRole::Conversion:
    case EKillerMoveRole::Switching:
        return TEXT("delivery");
    case EKillerMoveRole::Targeting:
    case EKillerMoveRole::InvestigationSensor:
    case EKillerMoveRole::RecognitionValidation:
        return TEXT("targeting");
    case EKillerMoveRole::Timing:
    case EKillerMoveRole::Trigger:
    case EKillerMoveRole::Storage:
    case EKillerMoveRole::Termination:
        return TEXT("timing");
    case EKillerMoveRole::Stabilization:
    case EKillerMoveRole::Safety:
    case EKillerMoveRole::Buffer:
    case EKillerMoveRole::Recovery:
        return TEXT("structure");
    case EKillerMoveRole::Fuel:
        return TEXT("resource");
    default:
        return TEXT("control");
    }
}

FName UKillerMoveSubsystem::RelationForRole(const EKillerMoveRole Role)
{
    switch (Role)
    {
    case EKillerMoveRole::Output: return TEXT("reinforces");
    case EKillerMoveRole::Amplification: return TEXT("amplifies");
    case EKillerMoveRole::Medium: return TEXT("carries");
    case EKillerMoveRole::Targeting: return TEXT("guides");
    case EKillerMoveRole::InvestigationSensor: return TEXT("detects");
    case EKillerMoveRole::Control: return TEXT("controls");
    case EKillerMoveRole::Timing: return TEXT("schedules");
    case EKillerMoveRole::Trigger: return TEXT("triggers");
    case EKillerMoveRole::Boundary: return TEXT("bounds");
    case EKillerMoveRole::Stabilization: return TEXT("stabilizes");
    case EKillerMoveRole::Safety: return TEXT("safeguards");
    case EKillerMoveRole::Buffer: return TEXT("buffers");
    case EKillerMoveRole::Subordinate: return TEXT("commands");
    case EKillerMoveRole::Conversion: return TEXT("converts");
    case EKillerMoveRole::Fuel: return TEXT("fuels");
    case EKillerMoveRole::Storage: return TEXT("stores");
    case EKillerMoveRole::Routing: return TEXT("routes");
    case EKillerMoveRole::Anchor: return TEXT("anchors");
    case EKillerMoveRole::Link: return TEXT("binds");
    case EKillerMoveRole::RecognitionValidation: return TEXT("validates");
    case EKillerMoveRole::Concealment: return TEXT("conceals");
    case EKillerMoveRole::Suppression: return TEXT("suppresses");
    case EKillerMoveRole::Recovery: return TEXT("recovers");
    case EKillerMoveRole::Termination: return TEXT("terminates");
    case EKillerMoveRole::Switching: return TEXT("switches");
    default: return TEXT("modifies");
    }
}

bool UKillerMoveSubsystem::BuildEffectGraph(FKillerMoveDefinitionRecord& InOutDefinition, FString& OutError) const
{
    OutError.Reset();
    InOutDefinition.EffectGraph = FKillerMoveEffectGraph();
    if (InOutDefinition.GuSlots.Num() < 1)
    {
        OutError = TEXT("A killer move requires at least one Gu slot.");
        return false;
    }

    UGuDefinitionRegistrySubsystem* Registry = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UGuDefinitionRegistrySubsystem>()
        : nullptr;
    if (!Registry)
    {
        OutError = TEXT("Gu definition registry is unavailable.");
        return false;
    }

    int32 CoreIndex = INDEX_NONE;
    int32 FirstMediumIndex = INDEX_NONE;
    for (int32 Index = 0; Index < InOutDefinition.GuSlots.Num(); ++Index)
    {
        const FKillerMoveGuSlot& Slot = InOutDefinition.GuSlots[Index];
        if (Slot.SlotId.IsNone())
        {
            OutError = FString::Printf(TEXT("Killer-move slot %d has no SlotId."), Index + 1);
            return false;
        }
        if (Slot.GuDefinitionId.IsNone())
        {
            OutError = FString::Printf(TEXT("Killer-move slot '%s' has no Gu definition."), *Slot.SlotId.ToString());
            return false;
        }
        FGuDefinitionRecord GuDefinition;
        if (!Registry->GetDefinition(Slot.GuDefinitionId, GuDefinition))
        {
            OutError = FString::Printf(TEXT("Unknown Gu definition '%s'."), *Slot.GuDefinitionId.ToString());
            return false;
        }

        FKillerMoveEffectNode Node;
        Node.NodeId = FName(*FString::Printf(TEXT("component:%s"), *Slot.SlotId.ToString()));
        Node.SlotId = Slot.SlotId;
        Node.GuDefinitionId = GuDefinition.Id;
        Node.Role = Index == 0 ? EKillerMoveRole::Core : Slot.Role;
        Node.Branch = BranchForRole(Node.Role);
        Node.Path = GuDefinition.Path;
        InOutDefinition.EffectGraph.Nodes.Add(Node);
        if (Node.Role == EKillerMoveRole::Core && CoreIndex == INDEX_NONE) CoreIndex = Index;
        if (Node.Role == EKillerMoveRole::Medium && FirstMediumIndex == INDEX_NONE) FirstMediumIndex = Index;
    }

    if (CoreIndex == INDEX_NONE) CoreIndex = 0;
    InOutDefinition.EffectGraph.Nodes[CoreIndex].Role = EKillerMoveRole::Core;
    InOutDefinition.EffectGraph.Nodes[CoreIndex].Branch = BranchForRole(EKillerMoveRole::Core);
    InOutDefinition.EffectGraph.RootNodeId = InOutDefinition.EffectGraph.Nodes[CoreIndex].NodeId;

    const FKillerMoveEffectNode& Core = InOutDefinition.EffectGraph.Nodes[CoreIndex];
    const int32 DeliveryTargetIndex = FirstMediumIndex != INDEX_NONE ? FirstMediumIndex : CoreIndex;

    for (int32 Index = 0; Index < InOutDefinition.EffectGraph.Nodes.Num(); ++Index)
    {
        if (Index == CoreIndex) continue;
        const FKillerMoveEffectNode& Node = InOutDefinition.EffectGraph.Nodes[Index];
        int32 TargetIndex = CoreIndex;
        if (Node.Role == EKillerMoveRole::Link || IsDeliveryTargetRole(Node.Role)) TargetIndex = DeliveryTargetIndex;
        if (TargetIndex == Index) TargetIndex = CoreIndex;
        const FKillerMoveEffectNode& Target = InOutDefinition.EffectGraph.Nodes[TargetIndex];

        FKillerMoveEffectEdge Edge;
        Edge.Relation = RelationForRole(Node.Role);
        if (Node.Role == EKillerMoveRole::Medium)
        {
            // Browser parity: the core phenomenon flows into the carrier/medium.
            Edge.FromNodeId = Core.NodeId;
            Edge.ToNodeId = Node.NodeId;
            Edge.Label = FString::Printf(TEXT("%s is carried by %s."), *Core.SlotId.ToString(), *Node.SlotId.ToString());
        }
        else
        {
            Edge.FromNodeId = Node.NodeId;
            Edge.ToNodeId = Target.NodeId;
            Edge.Label = FString::Printf(TEXT("%s %s %s."), *Node.SlotId.ToString(), *Edge.Relation.ToString(), *Target.SlotId.ToString());
        }
        InOutDefinition.EffectGraph.Edges.Add(Edge);
    }
    return true;
}

bool UKillerMoveSubsystem::ResolvePhysicalGu(FRuntimeSession& Session, FString& OutError)
{
    UGuEntitySubsystem* Entities = GetGameInstance() ? GetGameInstance()->GetSubsystem<UGuEntitySubsystem>() : nullptr;
    if (!Entities)
    {
        OutError = TEXT("Gu ECS is unavailable.");
        return false;
    }

    Session.BoundGuEntities.Reset();
    TSet<FGuid> UsedEntities;
    for (const FKillerMoveGuSlot& Slot : Session.Definition.GuSlots)
    {
        FGuid EntityId = Slot.PreferredEntityId;
        if (EntityId.IsValid())
        {
            const FOwnedByComponent* Owner = Entities->GetOwnedBy(EntityId);
            const FGuInstanceComponent* Instance = Entities->GetGuInstance(EntityId);
            const FGuPlacementComponent* Placement = Entities->GetGuPlacement(EntityId);
            if (!Owner || Owner->OwnerId != Session.OwnerId || !Instance || Instance->DefinitionId != Slot.GuDefinitionId
                || !Placement || Placement->Container != EGuContainer::Aperture)
            {
                EntityId = FGuid();
            }
        }
        if (!EntityId.IsValid())
        {
            Entities->FindOwnedGuInstance(Slot.GuDefinitionId, Session.OwnerId, EGuContainer::Aperture, EntityId, true);
        }
        if (!EntityId.IsValid())
        {
            if (Slot.bRequired)
            {
                OutError = FString::Printf(TEXT("Required Gu '%s' is not alive in the aperture."), *Slot.GuDefinitionId.ToString());
                return false;
            }
            Session.BoundGuEntities.Add(FGuid());
            continue;
        }
        if (UsedEntities.Contains(EntityId))
        {
            OutError = FString::Printf(TEXT("One physical Gu cannot fill multiple killer-move slots (%s)."), *Slot.SlotId.ToString());
            return false;
        }
        FString CanUseError;
        if (!Entities->CanUseGu(EntityId, CanUseError))
        {
            OutError = FString::Printf(TEXT("%s cannot participate: %s"), *Slot.SlotId.ToString(), *CanUseError);
            return false;
        }
        UsedEntities.Add(EntityId);
        Session.BoundGuEntities.Add(EntityId);
    }
    return true;
}

bool UKillerMoveSubsystem::BeginKillerMoveAsset(AGuPlayerState* PlayerState, const UKillerMoveDefinition* Definition, FString& OutError)
{
    if (!Definition)
    {
        OutError = TEXT("No killer-move definition supplied.");
        return false;
    }
    return BeginKillerMove(PlayerState, Definition->Definition, OutError);
}

bool UKillerMoveSubsystem::BeginKillerMove(AGuPlayerState* PlayerState, const FKillerMoveDefinitionRecord& Definition, FString& OutError)
{
    OutError.Reset();
    if (!PlayerState || !PlayerState->HasAuthority())
    {
        OutError = TEXT("Killer moves must begin on the authority.");
        return false;
    }
    if (PlayerState->DomainCharacterId.IsEmpty())
    {
        OutError = TEXT("Player has no domain character ID.");
        return false;
    }
    if (HasActiveKillerMove(PlayerState->DomainCharacterId))
    {
        OutError = TEXT("A killer move is already being formed.");
        return false;
    }

    FKillerMoveDefinitionRecord Compiled = Definition;
    if (Compiled.Id.IsNone()) Compiled.Id = TEXT("runtime_killer_move");
    if (Compiled.Name.IsEmpty()) Compiled.Name = FText::FromName(Compiled.Id);
    if (Compiled.GuSlots.Num() < 1 || Compiled.Choreography.Num() < 1)
    {
        OutError = TEXT("A killer move needs Gu slots and an activation choreography.");
        return false;
    }
    Compiled.GuSlots[0].Role = EKillerMoveRole::Core;
    if (!BuildEffectGraph(Compiled, OutError)) return false;

    Compiled.Choreography.Sort([](const FKillerMoveInputStep& A, const FKillerMoveInputStep& B)
    {
        return A.TargetTime < B.TargetTime;
    });

    FRuntimeSession Session;
    Session.SessionId = FGuid::NewGuid();
    Session.PlayerState = PlayerState;
    Session.OwnerId = PlayerState->DomainCharacterId;
    Session.Definition = Compiled;
    Session.StartedServerWorldTime = ServerWorldTime();
    if (!ResolvePhysicalGu(Session, OutError)) return false;

    const FString OwnerId = Session.OwnerId;
    Sessions.Add(OwnerId, MoveTemp(Session));
    FRuntimeSession& Stored = Sessions.FindChecked(OwnerId);
    PushPublicState(Stored, TEXT("Killer move forming. Follow the timed Gu inputs."));
    ScheduleDeadline(Stored);
    return true;
}

bool UKillerMoveSubsystem::HasActiveKillerMove(const FString& OwnerId) const
{
    const FRuntimeSession* Session = Sessions.Find(OwnerId);
    return Session && Session->StepIndex < Session->Definition.Choreography.Num();
}

void UKillerMoveSubsystem::ScheduleDeadline(FRuntimeSession& Session)
{
    UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
    if (!World || !Session.Definition.Choreography.IsValidIndex(Session.StepIndex)) return;

    World->GetTimerManager().ClearTimer(Session.DeadlineTimer);
    const FKillerMoveInputStep& Step = Session.Definition.Choreography[Session.StepIndex];
    const float Window = EffectiveWindow(Session.PlayerState.Get(), Step);
    const float Deadline = Session.StartedServerWorldTime + Step.TargetTime + Window;
    const float Delay = FMath::Max(0.01f, Deadline - ServerWorldTime());
    FTimerDelegate Delegate;
    Delegate.BindUObject(this, &UKillerMoveSubsystem::HandleDeadline, Session.OwnerId);
    World->GetTimerManager().SetTimer(Session.DeadlineTimer, Delegate, Delay, false);
}

void UKillerMoveSubsystem::HandleDeadline(FString OwnerId)
{
    FRuntimeSession* Session = Sessions.Find(OwnerId);
    if (!Session) return;
    FString Error;
    AdvancePastMissedStep(*Session, Error);
}

bool UKillerMoveSubsystem::AdvancePastMissedStep(FRuntimeSession& Session, FString& OutError)
{
    if (!Session.Definition.Choreography.IsValidIndex(Session.StepIndex)) return false;
    const FKillerMoveInputStep& Step = Session.Definition.Choreography[Session.StepIndex];
    if (Step.bCritical)
    {
        OutError = FString::Printf(TEXT("Missed critical timing: %s %s."),
            Step.Event == EKillerMoveInputEvent::Pressed ? TEXT("press") : TEXT("release"), *Step.SlotId.ToString());
        FinishSession(Session, EKillerMoveRunState::Failed, OutError);
        return false;
    }

    Session.Stability = FMath::Max(0.0f, Session.Stability - 30.0f);
    if (Step.Event == EKillerMoveInputEvent::Released)
    {
        const int32 SlotIndex = Session.Definition.GuSlots.IndexOfByPredicate([&Step](const FKillerMoveGuSlot& Slot)
        {
            return Slot.SlotId == Step.SlotId;
        });
        if (SlotIndex != INDEX_NONE && Session.HeldSlotIndices.Contains(SlotIndex))
        {
            if (AGuPlayerState* PS = Session.PlayerState.Get(); PS && PS->MentalResources)
            {
                PS->MentalResources->ReleaseAttention(AttentionKey(Session, SlotIndex));
            }
            Session.HeldSlotIndices.Remove(SlotIndex);
        }
    }
    ++Session.SkippedSteps;
    ++Session.StepIndex;
    if (Session.StepIndex >= Session.Definition.Choreography.Num())
    {
        CompleteSession(Session);
        return true;
    }
    PushPublicState(Session, TEXT("A non-critical Gu timing was missed; the formation continues imperfectly."));
    ScheduleDeadline(Session);
    return true;
}

bool UKillerMoveSubsystem::SubmitInput(AGuPlayerState* PlayerState, const int32 SlotIndex, const EKillerMoveInputEvent Event, FString& OutError)
{
    OutError.Reset();
    if (!PlayerState || !PlayerState->HasAuthority())
    {
        OutError = TEXT("Killer-move input must be resolved on the authority.");
        return false;
    }
    FRuntimeSession* Session = Sessions.Find(PlayerState->DomainCharacterId);
    if (!Session || !Session->Definition.Choreography.IsValidIndex(Session->StepIndex))
    {
        OutError = TEXT("No killer move is currently forming.");
        return false;
    }
    if (!Session->Definition.GuSlots.IsValidIndex(SlotIndex))
    {
        OutError = TEXT("Invalid killer-move Gu slot.");
        return false;
    }

    const FKillerMoveInputStep& Step = Session->Definition.Choreography[Session->StepIndex];
    const int32 ExpectedSlotIndex = Session->Definition.GuSlots.IndexOfByPredicate([&Step](const FKillerMoveGuSlot& Slot)
    {
        return Slot.SlotId == Step.SlotId;
    });
    const float Now = ServerWorldTime();
    const float Elapsed = Now - Session->StartedServerWorldTime;
    const float Window = EffectiveWindow(PlayerState, Step);
    const float Offset = Elapsed - Step.TargetTime;

    if (SlotIndex != ExpectedSlotIndex || Event != Step.Event)
    {
        Session->Stability = FMath::Max(0.0f, Session->Stability - 15.0f);
        if (Session->Stability <= 0.0f)
        {
            FinishSession(*Session, EKillerMoveRunState::Failed, TEXT("The killer move collapsed under conflicting Gu operations."));
        }
        else
        {
            PushPublicState(*Session, TEXT("Wrong Gu operation. Formation destabilized."));
        }
        OutError = TEXT("Wrong killer-move input.");
        return false;
    }

    if (Offset < -Window)
    {
        if (Event == EKillerMoveInputEvent::Released && Session->HeldSlotIndices.Contains(SlotIndex))
        {
            if (PlayerState->MentalResources) PlayerState->MentalResources->ReleaseAttention(AttentionKey(*Session, SlotIndex));
            Session->HeldSlotIndices.Remove(SlotIndex);
            Session->Stability = FMath::Max(0.0f, Session->Stability - 22.0f);
            const FString Message = FString::Printf(TEXT("%s was released too early."), *Step.SlotId.ToString());
            if (Step.bCritical) FinishSession(*Session, EKillerMoveRunState::Failed, Message);
            else
            {
                ++Session->SkippedSteps;
                ++Session->StepIndex;
                if (Session->StepIndex >= Session->Definition.Choreography.Num()) CompleteSession(*Session);
                else
                {
                    PushPublicState(*Session, Message);
                    ScheduleDeadline(*Session);
                }
            }
            OutError = Message;
            return false;
        }
        Session->Stability = FMath::Max(0.0f, Session->Stability - 8.0f);
        PushPublicState(*Session, TEXT("Too early. The Gu did not synchronize."));
        OutError = TEXT("Input was too early.");
        return false;
    }
    if (Offset > Window)
    {
        return AdvancePastMissedStep(*Session, OutError);
    }

    UMentalResourceComponent* Mental = PlayerState->MentalResources;
    const FKillerMoveGuSlot& Slot = Session->Definition.GuSlots[SlotIndex];
    const FName ReservationKey = AttentionKey(*Session, SlotIndex);

    if (Event == EKillerMoveInputEvent::Pressed)
    {
        if (!Mental || !Mental->ReserveAttention(ReservationKey, Slot.AttentionCost, FString::Printf(TEXT("Killer move: %s"), *Slot.SlotId.ToString())))
        {
            Session->Stability = FMath::Max(0.0f, Session->Stability - 25.0f);
            const FString Message = FString::Printf(TEXT("Insufficient attention to control %s."), *Slot.SlotId.ToString());
            if (Step.bCritical) FinishSession(*Session, EKillerMoveRunState::Failed, Message);
            else PushPublicState(*Session, Message);
            OutError = Message;
            return false;
        }
        if (Step.bHoldAttention) Session->HeldSlotIndices.Add(SlotIndex);
        else Mental->ReleaseAttention(ReservationKey);
    }
    else
    {
        if (Mental) Mental->ReleaseAttention(ReservationKey);
        Session->HeldSlotIndices.Remove(SlotIndex);
    }

    const float Accuracy = FMath::Clamp(1.0f - FMath::Abs(Offset) / FMath::Max(Window, KINDA_SMALL_NUMBER), 0.0f, 1.0f);
    Session->QualitySum += Accuracy;
    ++Session->AcceptedSteps;
    Session->Stability = FMath::Max(0.0f, Session->Stability - (1.0f - Accuracy) * 8.0f);
    ++Session->StepIndex;

    if (Session->StepIndex >= Session->Definition.Choreography.Num())
    {
        CompleteSession(*Session);
        return true;
    }

    PushPublicState(*Session, FString::Printf(TEXT("Input accepted (%+.0f ms)."), Offset * 1000.0f));
    if (AGuPlayerState* PS = Session->PlayerState.Get())
    {
        FKillerMovePublicState Public = PS->KillerMovePublicState;
        Public.LastInputOffsetMs = Offset * 1000.0f;
        PS->SetKillerMovePublicState(Public);
    }
    ScheduleDeadline(*Session);
    return true;
}

void UKillerMoveSubsystem::PushPublicState(FRuntimeSession& Session, const FString& StatusText)
{
    AGuPlayerState* PS = Session.PlayerState.Get();
    if (!PS) return;

    FKillerMovePublicState Public;
    Public.State = EKillerMoveRunState::Forming;
    Public.KillerMoveId = Session.Definition.Id;
    Public.Name = Session.Definition.Name.ToString();
    Public.CurrentStep = FMath::Min(Session.StepIndex + 1, Session.Definition.Choreography.Num());
    Public.TotalSteps = Session.Definition.Choreography.Num();
    Public.Stability = Session.Stability;
    Public.ExecutionQuality = Session.AcceptedSteps > 0 ? Session.QualitySum / static_cast<float>(Session.AcceptedSteps) : 0.0f;
    Public.StatusText = StatusText;

    UGuDefinitionRegistrySubsystem* Registry = GetGameInstance() ? GetGameInstance()->GetSubsystem<UGuDefinitionRegistrySubsystem>() : nullptr;
    for (const FKillerMoveGuSlot& Slot : Session.Definition.GuSlots)
    {
        FKillerMovePublicSlot PublicSlot;
        PublicSlot.SlotId = Slot.SlotId;
        PublicSlot.Role = Slot.Role;
        FGuDefinitionRecord Definition;
        PublicSlot.GuName = Registry && Registry->GetDefinition(Slot.GuDefinitionId, Definition) ? Definition.Name : Slot.GuDefinitionId.ToString();
        Public.Slots.Add(PublicSlot);
    }

    if (Session.Definition.Choreography.IsValidIndex(Session.StepIndex))
    {
        const FKillerMoveInputStep& Step = Session.Definition.Choreography[Session.StepIndex];
        Public.ExpectedSlotIndex = Session.Definition.GuSlots.IndexOfByPredicate([&Step](const FKillerMoveGuSlot& Slot)
        {
            return Slot.SlotId == Step.SlotId;
        });
        Public.ExpectedEvent = Step.Event;
        Public.TimingWindow = EffectiveWindow(PS, Step);
        Public.ExpectedServerWorldTime = Session.StartedServerWorldTime + Step.TargetTime;
    }
    PS->SetKillerMovePublicState(Public);
}

void UKillerMoveSubsystem::ReleaseAllAttention(FRuntimeSession& Session)
{
    AGuPlayerState* PS = Session.PlayerState.Get();
    if (!PS || !PS->MentalResources) return;
    for (int32 SlotIndex = 0; SlotIndex < Session.Definition.GuSlots.Num(); ++SlotIndex)
    {
        PS->MentalResources->ReleaseAttention(AttentionKey(Session, SlotIndex));
    }
    Session.HeldSlotIndices.Reset();
}

void UKillerMoveSubsystem::FinishSession(FRuntimeSession& Session, const EKillerMoveRunState EndState, const FString& Message)
{
    UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
    if (World) World->GetTimerManager().ClearTimer(Session.DeadlineTimer);
    ReleaseAllAttention(Session);

    if (AGuPlayerState* PS = Session.PlayerState.Get())
    {
        FKillerMovePublicState Public = PS->KillerMovePublicState;
        Public.State = EndState;
        Public.CurrentStep = Public.TotalSteps;
        Public.ExpectedSlotIndex = INDEX_NONE;
        Public.ExpectedServerWorldTime = 0.0f;
        Public.TimingWindow = 0.0f;
        Public.Stability = Session.Stability;
        Public.ExecutionQuality = Session.Definition.Choreography.Num() > 0
            ? Session.QualitySum / static_cast<float>(Session.Definition.Choreography.Num())
            : 0.0f;
        Public.StatusText = Message;
        PS->SetKillerMovePublicState(Public);
    }

    const FString OwnerId = Session.OwnerId;
    Sessions.Remove(OwnerId);
}

void UKillerMoveSubsystem::CompleteSession(FRuntimeSession& Session)
{
    const float Quality = Session.Definition.Choreography.Num() > 0
        ? Session.QualitySum / static_cast<float>(Session.Definition.Choreography.Num())
        : 0.0f;
    const FString Message = FString::Printf(
        TEXT("Killer move formed. Execution quality %.0f%%. Effect graph ready for resolution."),
        Quality * 100.0f);
    FinishSession(Session, EKillerMoveRunState::Completed, Message);
}

bool UKillerMoveSubsystem::CancelKillerMove(AGuPlayerState* PlayerState, FString& OutError)
{
    OutError.Reset();
    if (!PlayerState || !PlayerState->HasAuthority())
    {
        OutError = TEXT("Killer-move cancellation must run on the authority.");
        return false;
    }
    FRuntimeSession* Session = Sessions.Find(PlayerState->DomainCharacterId);
    if (!Session)
    {
        OutError = TEXT("No killer move is currently forming.");
        return false;
    }
    FinishSession(*Session, EKillerMoveRunState::Cancelled, TEXT("Killer move cancelled; attention released."));
    return true;
}

bool UKillerMoveSubsystem::BeginDebugKillerMove(AGuPlayerState* PlayerState, FString& OutError)
{
#if UE_BUILD_SHIPPING
    OutError = TEXT("Debug killer moves are unavailable in Shipping builds.");
    return false;
#else
    if (!PlayerState)
    {
        OutError = TEXT("No GuPlayerState.");
        return false;
    }
    UGuEntitySubsystem* Entities = GetGameInstance() ? GetGameInstance()->GetSubsystem<UGuEntitySubsystem>() : nullptr;
    UGuDefinitionRegistrySubsystem* Registry = GetGameInstance() ? GetGameInstance()->GetSubsystem<UGuDefinitionRegistrySubsystem>() : nullptr;
    if (!Entities || !Registry)
    {
        OutError = TEXT("Killer-move domain is unavailable.");
        return false;
    }

    TArray<FGuid> Owned = Entities->QueryGuEntitiesForOwner(PlayerState->DomainCharacterId, EGuContainer::Aperture, true);
    if (Owned.Num() < 1)
    {
        OutError = TEXT("No living Gu are present in the aperture.");
        return false;
    }
    Owned.SetNum(FMath::Min(2, Owned.Num()));

    FKillerMoveDefinitionRecord Definition;
    Definition.Id = TEXT("debug_timed_killer_move");
    Definition.Name = FText::FromString(TEXT("Debug Timed Killer Move"));
    Definition.Rank = 1;

    for (int32 Index = 0; Index < Owned.Num(); ++Index)
    {
        const FGuInstanceComponent* Instance = Entities->GetGuInstance(Owned[Index]);
        if (!Instance) continue;
        FKillerMoveGuSlot Slot;
        Slot.SlotId = Index == 0 ? TEXT("Core") : TEXT("Support");
        Slot.GuDefinitionId = Instance->DefinitionId;
        Slot.PreferredEntityId = Owned[Index];
        Slot.Role = Index == 0 ? EKillerMoveRole::Core : EKillerMoveRole::Amplification;
        Slot.AttentionCost = 1.0f;
        Definition.GuSlots.Add(Slot);
    }

    if (Definition.GuSlots.Num() < 1)
    {
        OutError = TEXT("Owned Gu could not be resolved.");
        return false;
    }

    const bool bCanOverlap = Definition.GuSlots.Num() >= 2
        && PlayerState->MentalResources
        && PlayerState->MentalResources->GetAttentionCapacity() >= 2;

    auto AddStep = [&Definition](const FName SlotId, const EKillerMoveInputEvent Event, const float TargetTime,
        const float Window, const bool bCritical, const bool bHoldAttention)
    {
        FKillerMoveInputStep Step;
        Step.SlotId = SlotId;
        Step.Event = Event;
        Step.TargetTime = TargetTime;
        Step.TimingWindow = Window;
        Step.bCritical = bCritical;
        Step.bHoldAttention = bHoldAttention;
        Definition.Choreography.Add(Step);
    };

    if (bCanOverlap)
    {
        AddStep(TEXT("Core"), EKillerMoveInputEvent::Pressed, 0.80f, 0.22f, true, true);
        AddStep(TEXT("Support"), EKillerMoveInputEvent::Pressed, 1.25f, 0.20f, false, true);
        AddStep(TEXT("Support"), EKillerMoveInputEvent::Released, 1.55f, 0.20f, false, false);
        AddStep(TEXT("Core"), EKillerMoveInputEvent::Released, 1.95f, 0.22f, true, false);
    }
    else if (Definition.GuSlots.Num() >= 2)
    {
        AddStep(TEXT("Core"), EKillerMoveInputEvent::Pressed, 0.70f, 0.24f, true, true);
        AddStep(TEXT("Core"), EKillerMoveInputEvent::Released, 1.00f, 0.22f, true, false);
        AddStep(TEXT("Support"), EKillerMoveInputEvent::Pressed, 1.35f, 0.22f, false, true);
        AddStep(TEXT("Support"), EKillerMoveInputEvent::Released, 1.65f, 0.22f, false, false);
    }
    else
    {
        AddStep(TEXT("Core"), EKillerMoveInputEvent::Pressed, 0.75f, 0.24f, true, true);
        AddStep(TEXT("Core"), EKillerMoveInputEvent::Released, 1.20f, 0.22f, true, false);
        AddStep(TEXT("Core"), EKillerMoveInputEvent::Pressed, 1.60f, 0.20f, false, true);
        AddStep(TEXT("Core"), EKillerMoveInputEvent::Released, 1.82f, 0.20f, false, false);
    }
    return BeginKillerMove(PlayerState, Definition, OutError);
#endif
}
