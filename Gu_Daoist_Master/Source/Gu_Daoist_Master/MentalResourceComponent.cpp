#include "MentalResourceComponent.h"
#include "GuPlayerState.h"
#include "GuEntitySubsystem.h"
#include "GuRulesLibrary.h"
#include "Net/UnrealNetwork.h"

UMentalResourceComponent::UMentalResourceComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
}

void UMentalResourceComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UMentalResourceComponent, DomainOwnerId);
    DOREPLIFETIME(UMentalResourceComponent, MentalFoundation);
    DOREPLIFETIME(UMentalResourceComponent, MentalPracticeXp);
    DOREPLIFETIME(UMentalResourceComponent, FocusControlLevel);
    DOREPLIFETIME(UMentalResourceComponent, FocusPracticeXp);
    DOREPLIFETIME(UMentalResourceComponent, MultitaskingNaturalCapacity);
    DOREPLIFETIME(UMentalResourceComponent, MultitaskingPracticeXp);
}

int64 UMentalResourceComponent::NowUnixMs()
{
    return FDateTime::UtcNow().ToUnixTimestamp() * 1000LL;
}

int32 UMentalResourceComponent::GetRank() const
{
    if (const AGuPlayerState* PlayerState = Cast<AGuPlayerState>(GetOwner()))
    {
        return FMath::Max(1, PlayerState->CultivationRank);
    }
    return 1;
}

int32 UMentalResourceComponent::GetMentalFoundationCap() const
{
    return UGuRulesLibrary::MentalFoundationCap(GetRank());
}

int32 UMentalResourceComponent::GetFocusBranchCap() const
{
    return UGuRulesLibrary::FocusBranchCap(MentalFoundation, GetRank());
}

int32 UMentalResourceComponent::GetFocusCapacity() const
{
    return UGuRulesLibrary::MentalFocusCapacity(MentalFoundation, FocusControlLevel);
}

int32 UMentalResourceComponent::GetMultitaskingNaturalCap() const
{
    return UGuRulesLibrary::MultitaskingNaturalCap(GetRank());
}

int32 UMentalResourceComponent::GetActiveGuBoostSlots() const
{
    const UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
    const UGuEntitySubsystem* Entities = GI ? GI->GetSubsystem<UGuEntitySubsystem>() : nullptr;
    return Entities ? Entities->GetActiveMultitaskingBoostSlots(DomainOwnerId) : 0;
}

int32 UMentalResourceComponent::GetAttentionCapacity() const
{
    int32 Temporary = 0;
    for (const TPair<FName, int32>& Pair : TemporaryGrants) Temporary += FMath::Max(0, Pair.Value);
    return FMath::Max(1, MultitaskingNaturalCapacity) + GetActiveGuBoostSlots() + Temporary;
}

float UMentalResourceComponent::GetAttentionUsed(const FName ExcludeKey) const
{
    float Used = 0.0f;
    for (const TPair<FName, FAttentionReservation>& Pair : Reservations)
    {
        if (Pair.Key != ExcludeKey) Used += FMath::Max(0.0f, Pair.Value.Cost);
    }
    return Used;
}

float UMentalResourceComponent::GetAttentionAvailable(const FName ExcludeKey) const
{
    return FMath::Max(0.0f, static_cast<float>(GetAttentionCapacity()) - GetAttentionUsed(ExcludeKey));
}

bool UMentalResourceComponent::ReserveAttention(const FName Key, const float Cost, const FString& Label)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || Key.IsNone()) return false;
    const float Normalized = FMath::Max(0.01f, Cost);
    if (GetAttentionUsed(Key) + Normalized > static_cast<float>(GetAttentionCapacity()) + KINDA_SMALL_NUMBER) return false;

    FAttentionReservation& Reservation = Reservations.FindOrAdd(Key);
    Reservation.Key = Key;
    Reservation.Cost = Normalized;
    Reservation.Label = Label.IsEmpty() ? Key.ToString() : Label;
    if (Reservation.StartedAtUnixMs == 0) Reservation.StartedAtUnixMs = NowUnixMs();
    return true;
}

bool UMentalResourceComponent::ReleaseAttention(const FName Key)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return false;
    return Reservations.Remove(Key) > 0;
}

bool UMentalResourceComponent::HasAttentionReservation(const FName Key) const
{
    return Reservations.Contains(Key);
}

void UMentalResourceComponent::SetTemporaryAttentionGrant(const FName Key, const int32 Slots)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || Key.IsNone()) return;
    const int32 Normalized = FMath::Max(0, Slots);
    if (Normalized == 0) TemporaryGrants.Remove(Key);
    else TemporaryGrants.Add(Key, Normalized);
}

FAttentionSnapshot UMentalResourceComponent::GetAttentionSnapshot() const
{
    FAttentionSnapshot Snapshot;
    Snapshot.Natural = MultitaskingNaturalCapacity;
    Snapshot.NaturalCap = GetMultitaskingNaturalCap();
    Snapshot.Boost = GetActiveGuBoostSlots();
    for (const TPair<FName, int32>& Pair : TemporaryGrants) Snapshot.TemporaryBoost += FMath::Max(0, Pair.Value);
    Snapshot.Capacity = GetAttentionCapacity();
    Snapshot.Used = GetAttentionUsed();
    Snapshot.Available = GetAttentionAvailable();
    Snapshot.MentalFoundation = MentalFoundation;
    Snapshot.MentalCap = GetMentalFoundationCap();
    Snapshot.FocusControlLevel = FocusControlLevel;
    Snapshot.FocusCapacity = GetFocusCapacity();
    Reservations.GenerateValueArray(Snapshot.Processes);
    return Snapshot;
}

float UMentalResourceComponent::MentalPracticeRequirement(const int32 Level) const
{
    const int32 Current = FMath::Max(1, Level);
    return FMath::RoundToFloat(24.0f * FMath::Square(static_cast<float>(Current)));
}

float UMentalResourceComponent::FocusPracticeRequirement(const int32 Level) const
{
    const int32 Current = FMath::Max(1, Level);
    return FMath::RoundToFloat(18.0f * FMath::Square(static_cast<float>(Current)));
}

float UMentalResourceComponent::MultitaskingPracticeRequirement(const int32 Capacity) const
{
    const int32 Current = FMath::Max(1, Capacity);
    const float MentalEase = 1.0f + FMath::Max(0, MentalFoundation - 1) * 0.025f;
    return FMath::Max(1.0f, FMath::RoundToFloat(10.0f * FMath::Square(static_cast<float>(Current)) / MentalEase));
}

void UMentalResourceComponent::ApplyMentalPractice(const float SharedAmount, const float FocusAmount)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    MentalPracticeXp = FMath::Max(0.0f, MentalPracticeXp + FMath::Max(0.0f, SharedAmount));
    while (MentalFoundation < GetMentalFoundationCap() && MentalPracticeXp + KINDA_SMALL_NUMBER >= MentalPracticeRequirement(MentalFoundation))
    {
        MentalPracticeXp -= MentalPracticeRequirement(MentalFoundation);
        ++MentalFoundation;
    }

    FocusPracticeXp = FMath::Max(0.0f, FocusPracticeXp + FMath::Max(0.0f, FocusAmount));
    while (FocusControlLevel < GetFocusBranchCap() && FocusPracticeXp + KINDA_SMALL_NUMBER >= FocusPracticeRequirement(FocusControlLevel))
    {
        FocusPracticeXp -= FocusPracticeRequirement(FocusControlLevel);
        ++FocusControlLevel;
    }
}

void UMentalResourceComponent::RecordRefinementFocusUse(const float FocusSpent)
{
    const float Amount = FMath::Max(0.0f, FocusSpent);
    if (Amount <= 0.0f) return;
    // v7.9.25 parity: refinement practice contributes 0.28x to common mental
    // foundation and 0.55x to the focused-control branch.
    ApplyMentalPractice(Amount * 0.28f, Amount * 0.55f);
}

bool UMentalResourceComponent::ApplyMultitaskingPracticeGain(const float PracticeGain)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || PracticeGain <= 0.0f) return false;
    if (MultitaskingNaturalCapacity >= GetMultitaskingNaturalCap()) return false;

    MultitaskingPracticeXp += PracticeGain;
    ApplyMentalPractice(PracticeGain * 0.42f, 0.0f);
    bool bImproved = false;
    while (MultitaskingNaturalCapacity < GetMultitaskingNaturalCap())
    {
        const float Need = MultitaskingPracticeRequirement(MultitaskingNaturalCapacity);
        if (MultitaskingPracticeXp + KINDA_SMALL_NUMBER < Need) break;
        MultitaskingPracticeXp -= Need;
        ++MultitaskingNaturalCapacity;
        bImproved = true;
    }
    return bImproved;
}

FMentalResourceSnapshot UMentalResourceComponent::ExportSnapshot() const
{
    FMentalResourceSnapshot Snapshot;
    Snapshot.MentalFoundation = MentalFoundation;
    Snapshot.MentalPracticeXp = MentalPracticeXp;
    Snapshot.FocusControlLevel = FocusControlLevel;
    Snapshot.FocusPracticeXp = FocusPracticeXp;
    Snapshot.MultitaskingNaturalCapacity = MultitaskingNaturalCapacity;
    Snapshot.MultitaskingPracticeXp = MultitaskingPracticeXp;
    return Snapshot;
}

void UMentalResourceComponent::RestoreSnapshot(const FMentalResourceSnapshot& Snapshot)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    MentalFoundation = FMath::Clamp(Snapshot.MentalFoundation, 1, GetMentalFoundationCap());
    MentalPracticeXp = FMath::Max(0.0f, Snapshot.MentalPracticeXp);
    FocusControlLevel = FMath::Clamp(Snapshot.FocusControlLevel, 1, GetFocusBranchCap());
    FocusPracticeXp = FMath::Max(0.0f, Snapshot.FocusPracticeXp);
    MultitaskingNaturalCapacity = FMath::Clamp(Snapshot.MultitaskingNaturalCapacity, 1, GetMultitaskingNaturalCap());
    MultitaskingPracticeXp = FMath::Max(0.0f, Snapshot.MultitaskingPracticeXp);
    Reservations.Reset();
    TemporaryGrants.Reset();
}
