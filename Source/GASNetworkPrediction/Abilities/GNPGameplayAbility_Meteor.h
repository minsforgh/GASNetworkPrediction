#pragma once

#include "CoreMinimal.h"
#include "GAS/GNPGameplayAbility.h"
#include "GNPMeteorFunctionLibrary.h"
#include "GNPGameplayAbility_Meteor.generated.h"

class UGameplayEffect;

/**
 * 메테오 스킬 전용 Ability
 * FTimerManager로 각 메테오의 순차적 판정을 처리
 */
UCLASS()
class GASNETWORKPREDICTION_API UGNPGameplayAbility_Meteor : public UGNPGameplayAbility
{
	GENERATED_BODY()

public:
	UGNPGameplayAbility_Meteor();

	/**
	 * 메테오 데미지 판정 실행 (BP에서 호출)
	 * 서버에서만 실행됨 (HasAuthority 체크 포함)
	 *
	 * @param Seed 메테오 위치 계산용 시드
	 * @param CenterLocation 메테오 범위 중심점
	 * @return MaxImpactDelay (End Ability 타이밍용)
	 */
	UFUNCTION(BlueprintCallable, Category = "GNP|Meteor")
	float ExecuteMeteorDamage(int32 Seed, FVector CenterLocation);

protected:
	// Ability 종료 시 타이머 정리
	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

	// 개별 메테오 데미지 적용 (타이머 콜백)
	void ApplyMeteorDamageAtLocation(FVector Location);

	// 메테오 데미지 GameplayEffect (BP에서 설정)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GNP|Meteor")
	TSubclassOf<UGameplayEffect> DamageGameplayEffect;

	// AoE 데미지 반경
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GNP|Meteor")
	float DamageRadius = 300.0f;

	// 메테오 범위 설정
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GNP|Meteor")
	float MinRadius = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GNP|Meteor")
	float MaxRadius = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GNP|Meteor")
	int32 MeteorCount = 25;

	// 탐지할 오브젝트 타입 (비어있으면 모든 타입)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GNP|Meteor")
	TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypesToDetect;

	// 메테오 낙하 설정 (물리 기반 ImpactDelay 계산용)
	// 시작 높이 = CenterLocation.Z + RandomRange(MinStartHeight, MaxStartHeight)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GNP|Meteor|Fall")
	float MinStartHeight = 1000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GNP|Meteor|Fall")
	float MaxStartHeight = 2000.0f;

	// 낙하 속도 (units/sec)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GNP|Meteor|Fall")
	float FallSpeed = 1000.0f;

private:
	// 활성 타이머 핸들 (정리용)
	TArray<FTimerHandle> ActiveTimerHandles;
};
