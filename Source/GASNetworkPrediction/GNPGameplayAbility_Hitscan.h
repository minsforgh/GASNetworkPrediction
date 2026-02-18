#pragma once

#include "CoreMinimal.h"
#include "GNPGameplayAbility.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "GNPGameplayAbility_Hitscan.generated.h"

class UGameplayEffect;

/**
 * 커스텀 TargetData: 히트스캔 Rewind 정보
 * 클라이언트 타임스탬프, 트레이스 정보를 서버로 전달
 */
USTRUCT()
struct FGameplayAbilityTargetData_HitscanRewind : public FGameplayAbilityTargetData
{
	GENERATED_USTRUCT_BODY()

	// 클라이언트가 발사한 시점의 서버 시간
	UPROPERTY()
	float ClientTimestamp = 0.0f;

	// 레이캐스트 시작/끝
	UPROPERTY()
	FVector_NetQuantize TraceStart;

	UPROPERTY()
	FVector_NetQuantize TraceEnd;

	// 클라이언트 히트 결과
	UPROPERTY()
	FHitResult ClientHitResult;

	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FGameplayAbilityTargetData_HitscanRewind::StaticStruct();
	}

	bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess);
};

template<>
struct TStructOpsTypeTraits<FGameplayAbilityTargetData_HitscanRewind> : public TStructOpsTypeTraitsBase2<FGameplayAbilityTargetData_HitscanRewind>
{
	enum
	{
		WithNetSerializer = true,
	};
};

/**
 * Server Rewind 기반 히트스캔 어빌리티
 * 클라이언트: 즉시 레이캐스트 + 이펙트 표시
 * 서버: Rewind 후 레이캐스트 검증 + 데미지 적용
 */
UCLASS()
class GASNETWORKPREDICTION_API UGNPGameplayAbility_Hitscan : public UGNPGameplayAbility
{
	GENERATED_BODY()

public:
	UGNPGameplayAbility_Hitscan();

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	// 클라이언트: 즉시 레이캐스트 + TargetData를 서버로 전송
	void PerformClientTrace();

	// Listen Server 호스트: Rewind 없이 즉시 서버 트레이스
	void PerformServerDirectTrace();

	// 서버: TargetData 수신 -> Rewind 검증 -> 데미지
	void OnTargetDataReceived(
		const FGameplayAbilityTargetDataHandle& Data,
		FGameplayTag ActivationTag);

	// 서버: Rewind 검증 후 데미지 적용
	void ServerValidateAndApplyDamage(
		float ClientTimestamp,
		FVector TraceStart,
		FVector TraceEnd);

	// BP 확장: 이펙트 표시 (클라이언트에서 호출)
	UFUNCTION(BlueprintImplementableEvent, Category = "GNP|Hitscan")
	void OnHitscanFired(FVector TraceStart, FVector TraceEnd, FHitResult HitResult, bool bDidHit);

	// ============ 설정 ============

	// 트레이스 범위
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GNP|Hitscan")
	float TraceRange = 10000.0f;

	// 데미지 GE
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GNP|Hitscan")
	TSubclassOf<UGameplayEffect> DamageGameplayEffect;

	// Rewind 허용 최대 시간 (치팅 방지)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GNP|Hitscan|Rewind")
	float MaxRewindTime = 0.5f;
	
};
