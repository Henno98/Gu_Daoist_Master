#pragma once

#include "CoreMinimal.h"
#include "KillerMoveTypes.generated.h"

/** The semantic job performed by one Gu inside a killer move. */
UENUM(BlueprintType)
enum class EKillerMoveRole : uint8
{
    Core,
    Output,
    Amplification,
    Suppression,
    Concealment,
    Medium,
    Link,
    Routing,
    Boundary,
    Anchor,
    Conversion,
    Switching,
    Targeting,
    InvestigationSensor,
    RecognitionValidation,
    Timing,
    Trigger,
    Storage,
    Termination,
    Stabilization,
    Safety,
    Buffer,
    Recovery,
    Control,
    Subordinate,
    Fuel
};

/** Physical player input events. A pulse is simply a press/release pair in the choreography. */
UENUM(BlueprintType)
enum class EKillerMoveInputEvent : uint8
{
    Pressed,
    Released
};

UENUM(BlueprintType)
enum class EKillerMoveRunState : uint8
{
    Idle,
    Forming,
    Completed,
    Failed,
    Cancelled
};

/** One Gu requirement in the learned killer-move formula. */
USTRUCT(BlueprintType)
struct FKillerMoveGuSlot
{
    GENERATED_BODY()

    /** Stable name used by choreography steps, e.g. Core, Carrier, Guide. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName SlotId = NAME_None;

    /** Species requirement. The runtime resolves this to an owned physical ECS Gu. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName GuDefinitionId = NAME_None;

    /** Optional exact physical Gu binding for player-created killer moves. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FGuid PreferredEntityId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EKillerMoveRole Role = EKillerMoveRole::Amplification;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.01"))
    float AttentionCost = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bRequired = true;
};

/** One timed press/release in the activation choreography. */
USTRUCT(BlueprintType)
struct FKillerMoveInputStep
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName SlotId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EKillerMoveInputEvent Event = EKillerMoveInputEvent::Pressed;

    /** Seconds after the killer-move button is pressed. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.0"))
    float TargetTime = 0.5f;

    /** Base +/- timing window. Focus Control widens this at runtime. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.03", ClampMax="2.0"))
    float TimingWindow = 0.2f;

    /** If true, missing the window collapses the entire activation. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bCritical = true;

    /** Pressing this step keeps its Gu under attention until a later Release step. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bHoldAttention = false;
};

USTRUCT(BlueprintType)
struct FKillerMoveEffectNode
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FName NodeId = NAME_None;
    UPROPERTY(BlueprintReadOnly)
    FName SlotId = NAME_None;
    UPROPERTY(BlueprintReadOnly)
    FName GuDefinitionId = NAME_None;
    UPROPERTY(BlueprintReadOnly)
    EKillerMoveRole Role = EKillerMoveRole::Control;
    UPROPERTY(BlueprintReadOnly)
    FName Branch = NAME_None;
    UPROPERTY(BlueprintReadOnly)
    FName Path = NAME_None;
};

USTRUCT(BlueprintType)
struct FKillerMoveEffectEdge
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FName Relation = NAME_None;
    UPROPERTY(BlueprintReadOnly)
    FName FromNodeId = NAME_None;
    UPROPERTY(BlueprintReadOnly)
    FName ToNodeId = NAME_None;
    UPROPERTY(BlueprintReadOnly)
    FString Label;
};

/** Typed graph ported from the browser killer-move role model. */
USTRUCT(BlueprintType)
struct FKillerMoveEffectGraph
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    int32 Version = 1;
    UPROPERTY(BlueprintReadOnly)
    FName RootNodeId = NAME_None;
    UPROPERTY(BlueprintReadOnly)
    TArray<FKillerMoveEffectNode> Nodes;
    UPROPERTY(BlueprintReadOnly)
    TArray<FKillerMoveEffectEdge> Edges;
};

/** Serializable/authorable killer-move formula. Runtime binding happens against physical ECS Gu. */
USTRUCT(BlueprintType)
struct FKillerMoveDefinitionRecord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName Id = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FText Name;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="1", ClampMax="9"))
    int32 Rank = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FKillerMoveGuSlot> GuSlots;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FKillerMoveInputStep> Choreography;
    UPROPERTY(BlueprintReadOnly)
    FKillerMoveEffectGraph EffectGraph;
};

USTRUCT(BlueprintType)
struct FKillerMovePublicSlot
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FName SlotId = NAME_None;
    UPROPERTY(BlueprintReadOnly)
    FString GuName;
    UPROPERTY(BlueprintReadOnly)
    EKillerMoveRole Role = EKillerMoveRole::Control;
};

/** Replicated player-safe projection. Exact physical entity IDs stay server-side. */
USTRUCT(BlueprintType)
struct FKillerMovePublicState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    EKillerMoveRunState State = EKillerMoveRunState::Idle;
    UPROPERTY(BlueprintReadOnly)
    FName KillerMoveId = NAME_None;
    UPROPERTY(BlueprintReadOnly)
    FString Name;
    UPROPERTY(BlueprintReadOnly)
    int32 CurrentStep = 0;
    UPROPERTY(BlueprintReadOnly)
    int32 TotalSteps = 0;
    UPROPERTY(BlueprintReadOnly)
    int32 ExpectedSlotIndex = INDEX_NONE;
    UPROPERTY(BlueprintReadOnly)
    EKillerMoveInputEvent ExpectedEvent = EKillerMoveInputEvent::Pressed;
    UPROPERTY(BlueprintReadOnly)
    float ExpectedServerWorldTime = 0.0f;
    UPROPERTY(BlueprintReadOnly)
    float TimingWindow = 0.0f;
    UPROPERTY(BlueprintReadOnly)
    float Stability = 100.0f;
    UPROPERTY(BlueprintReadOnly)
    float ExecutionQuality = 0.0f;
    UPROPERTY(BlueprintReadOnly)
    float LastInputOffsetMs = 0.0f;
    UPROPERTY(BlueprintReadOnly)
    FString StatusText;
    UPROPERTY(BlueprintReadOnly)
    TArray<FKillerMovePublicSlot> Slots;
};
