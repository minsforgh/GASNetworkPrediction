#include "GNPGameplayAbility.h"
#include "AbilitySystemComponent.h"

UGNPGameplayAbility::UGNPGameplayAbility()
{
	// 인스턴스 생성 방식: 액터당 하나의 인스턴스
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	// 네트워크: 클라이언트 예측 + 서버 검증
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	// 보안: 서버만 Ability 종료 가능
	NetSecurityPolicy = EGameplayAbilityNetSecurityPolicy::ServerOnlyTermination;
}

bool UGNPGameplayAbility::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	return CheckActivateCondition();
}

void UGNPGameplayAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	// Cost + Cooldown 한번에 처리
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	OnActivated();
}

void UGNPGameplayAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	OnEnded(bWasCancelled);

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

// BlueprintNativeEvent 기본 구현 
bool UGNPGameplayAbility::CheckActivateCondition_Implementation() const
{
	return true;
}

void UGNPGameplayAbility::OnActivated_Implementation()
{
	// BP에서 오버라이드
}

void UGNPGameplayAbility::OnEnded_Implementation(bool bWasCancelled)
{
	// BP에서 오버라이드
}
