#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MentalResourceComponent.generated.h"

USTRUCT(BlueprintType)
struct FAttentionReservation
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FName Key;

    UPROPERTY(BlueprintReadOnly)
    float Cost = 1.0f;

    UPROPERTY(BlueprintReadOnly)
    FString Label;

    UPROPERTY(BlueprintReadOnly)
    int64 StartedAtUnixMs = 0;
};

USTRUCT(BlueprintType)
struct FMentalResourceSnapshot
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    int32 MentalFoundation = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    float MentalPracticeXp = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    int32 FocusControlLevel = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    float FocusPracticeXp = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    int32 MultitaskingNaturalCapacity = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    float MultitaskingPracticeXp = 0.0f;
};

USTRUCT(BlueprintType)
struct FAttentionSnapshot
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    int32 Natural = 1;
    UPROPERTY(BlueprintReadOnly)
    int32 NaturalCap = 15;
    UPROPERTY(BlueprintReadOnly)
    int32 Boost = 0;
    UPROPERTY(BlueprintReadOnly)
    int32 TemporaryBoost = 0;
    UPROPERTY(BlueprintReadOnly)
    int32 Capacity = 1;
    UPROPERTY(BlueprintReadOnly)
    float Used = 0.0f;
    UPROPERTY(BlueprintReadOnly)
    float Available = 1.0f;
    UPROPERTY(BlueprintReadOnly)
    int32 MentalFoundation = 1;
    UPROPERTY(BlueprintReadOnly)
    int32 MentalCap = 20;
    UPROPERTY(BlueprintReadOnly)
    int32 FocusControlLevel = 1;
    UPROPERTY(BlueprintReadOnly)
    int32 FocusCapacity = 100;
    UPROPERTY(BlueprintReadOnly)
    TArray<FAttentionReservation> Processes;
};

UCLASS(ClassGroup=(Gu), meta=(BlueprintSpawnableComponent))
class GU_DAOIST_MASTER_API UMentalResourceComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UMentalResourceComponent();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, SaveGame, Category="Mental")
    FString DomainOwnerId = TEXT("player");

    UPROPERTY(BlueprintReadOnly, Replicated, SaveGame, Category="Mental")
    int32 MentalFoundation = 1;
    UPROPERTY(BlueprintReadOnly, Replicated, SaveGame, Category="Mental")
    float MentalPracticeXp = 0.0f;
    UPROPERTY(BlueprintReadOnly, Replicated, SaveGame, Category="Mental")
    int32 FocusControlLevel = 1;
    UPROPERTY(BlueprintReadOnly, Replicated, SaveGame, Category="Mental")
    float FocusPracticeXp = 0.0f;
    UPROPERTY(BlueprintReadOnly, Replicated, SaveGame, Category="Mental")
    int32 MultitaskingNaturalCapacity = 1;
    UPROPERTY(BlueprintReadOnly, Replicated, SaveGame, Category="Mental")
    float MultitaskingPracticeXp = 0.0f;

    UFUNCTION(BlueprintPure, Category="Mental")
    int32 GetRank() const;

    UFUNCTION(BlueprintPure, Category="Mental")
    int32 GetMentalFoundationCap() const;

    UFUNCTION(BlueprintPure, Category="Mental")
    int32 GetFocusBranchCap() const;

    UFUNCTION(BlueprintPure, Category="Mental")
    int32 GetFocusCapacity() const;

    UFUNCTION(BlueprintPure, Category="Mental")
    int32 GetMultitaskingNaturalCap() const;

    UFUNCTION(BlueprintPure, Category="Mental")
    int32 GetActiveGuBoostSlots() const;

    UFUNCTION(BlueprintPure, Category="Mental")
    int32 GetAttentionCapacity() const;

    UFUNCTION(BlueprintPure, Category="Mental")
    float GetAttentionUsed(FName ExcludeKey = NAME_None) const;

    UFUNCTION(BlueprintPure, Category="Mental")
    float GetAttentionAvailable(FName ExcludeKey = NAME_None) const;

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Mental")
    bool ReserveAttention(FName Key, float Cost, const FString& Label);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Mental")
    bool ReleaseAttention(FName Key);

    UFUNCTION(BlueprintPure, Category="Mental")
    bool HasAttentionReservation(FName Key) const;

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Mental")
    void SetTemporaryAttentionGrant(FName Key, int32 Slots);

    UFUNCTION(BlueprintPure, Category="Mental")
    FAttentionSnapshot GetAttentionSnapshot() const;

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Mental")
    void ApplyMentalPractice(float SharedAmount, float FocusAmount);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Mental")
    void RecordRefinementFocusUse(float FocusSpent);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Mental")
    bool ApplyMultitaskingPracticeGain(float PracticeGain);

    FMentalResourceSnapshot ExportSnapshot() const;
    void RestoreSnapshot(const FMentalResourceSnapshot& Snapshot);

private:
    static int64 NowUnixMs();
    float MentalPracticeRequirement(int32 Level) const;
    float FocusPracticeRequirement(int32 Level) const;
    float MultitaskingPracticeRequirement(int32 Capacity) const;

    UPROPERTY(Transient)
    TMap<FName, FAttentionReservation> Reservations;

    UPROPERTY(Transient)
    TMap<FName, int32> TemporaryGrants;
};
