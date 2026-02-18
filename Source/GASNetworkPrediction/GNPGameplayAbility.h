#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GNPGameplayAbility.generated.h"

// Ability 활성화 정책
UENUM(BlueprintType)
enum class EGNPAbilityActivationPolicy : uint8
{
	OnInputTriggered,  // 입력 시 즉시
	WhileInputActive,  // 입력 유지 중
	OnSpawn            // 스폰 시 자동
};

UCLASS(Abstract, Blueprintable)
class GASNETWORKPREDICTION_API UGNPGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGNPGameplayAbility();

	EGNPAbilityActivationPolicy GetActivationPolicy() const { return ActivationPolicy; }

protected:
	
	// 활성화 가능 체크
	virtual bool CanActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	// Ability 시작
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	// Ability 종료
	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

	// 블루프린트 확장 포인트 (BlueprintNativeEvent)
	UFUNCTION(BlueprintNativeEvent, Category = "GNP|Ability")
	bool CheckActivateCondition() const;
	virtual bool CheckActivateCondition_Implementation() const;

	UFUNCTION(BlueprintNativeEvent, Category = "GNP|Ability")
	void OnActivated();
	virtual void OnActivated_Implementation();

	UFUNCTION(BlueprintNativeEvent, Category = "GNP|Ability")
	void OnEnded(bool bWasCancelled);
	virtual void OnEnded_Implementation(bool bWasCancelled);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GNP|Activation")
	EGNPAbilityActivationPolicy ActivationPolicy = EGNPAbilityActivationPolicy::OnInputTriggered;
};
