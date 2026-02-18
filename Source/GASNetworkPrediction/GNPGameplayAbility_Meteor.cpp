#include "GNPGameplayAbility_Meteor.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "GameFramework/Actor.h"
#include "Kismet/KismetSystemLibrary.h"
#include "TimerManager.h"
#include "DrawDebugHelpers.h"

UGNPGameplayAbility_Meteor::UGNPGameplayAbility_Meteor()
{
	// 기본값은 부모 클래스에서 설정됨
}

float UGNPGameplayAbility_Meteor::ExecuteMeteorDamage(int32 Seed, FVector CenterLocation)
{
	// 서버에서만 데미지 판정
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!AvatarActor || !AvatarActor->HasAuthority())
	{
		return 0.0f;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return 0.0f;
	}

	// Seed 기반 메테오 위치 계산 (물리 기반 ImpactDelay)
	TArray<FMeteorImpactData> ImpactDataArray = UGNPMeteorFunctionLibrary::CalculateMeteorImpacts(
		Seed,
		CenterLocation,
		MinRadius,
		MaxRadius,
		MeteorCount,
		MinStartHeight,
		MaxStartHeight,
		FallSpeed,
		this
	);

	// 기존 타이머 정리
	for (FTimerHandle& Handle : ActiveTimerHandles)
	{
		World->GetTimerManager().ClearTimer(Handle);
	}
	ActiveTimerHandles.Empty();
	ActiveTimerHandles.Reserve(ImpactDataArray.Num());

	// MaxImpactDelay 계산
	float MaxImpactDelay = UGNPMeteorFunctionLibrary::GetMaxImpactDelay(ImpactDataArray);

	// 각 메테오별 개별 타이머 설정
	for (const FMeteorImpactData& ImpactData : ImpactDataArray)
	{
		FTimerHandle TimerHandle;
		FTimerDelegate TimerDelegate;

		// 람다로 위치 캡처하여 콜백 설정
		TimerDelegate.BindLambda([this, Location = ImpactData.Location]()
		{
			ApplyMeteorDamageAtLocation(Location);
		});

		World->GetTimerManager().SetTimer(
			TimerHandle,
			TimerDelegate,
			ImpactData.ImpactDelay,
			false  // 반복 안함
		);

		ActiveTimerHandles.Add(TimerHandle);
	}

	return MaxImpactDelay;
}

void UGNPGameplayAbility_Meteor::ApplyMeteorDamageAtLocation(FVector Location)
{
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!AvatarActor)
	{
		return;
	}

	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
	if (!SourceASC || !DamageGameplayEffect)
	{
		return;
	}

#if ENABLE_DRAW_DEBUG
	// 디버그: 판정 범위 시각화 (빨간색, 2초 유지)
	if (UWorld* World = GetWorld())
	{
		DrawDebugSphere(World, Location, DamageRadius, 16, FColor::Red, false, 2.0f);
	}
#endif

	// SphereOverlap으로 범위 내 액터 검색
	TArray<AActor*> OverlappedActors;
	TArray<AActor*> ActorsToIgnore;
	ActorsToIgnore.Add(AvatarActor);  // 시전자 제외

	UKismetSystemLibrary::SphereOverlapActors(
		this,
		Location,
		DamageRadius,
		ObjectTypesToDetect,
		AActor::StaticClass(),
		ActorsToIgnore,
		OverlappedActors
	);

	// GE Spec 생성
	FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(
		DamageGameplayEffect,
		GetAbilityLevel(),
		SourceASC->MakeEffectContext()
	);

	if (!SpecHandle.IsValid())
	{
		return;
	}

	// 범위 내 모든 타겟에 데미지 적용
	for (AActor* TargetActor : OverlappedActors)
	{
		if (!TargetActor)
		{
			continue;
		}

		// 타겟의 ASC 찾기
		UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);
		if (TargetASC)
		{
			SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
		}
	}
}

void UGNPGameplayAbility_Meteor::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	// 활성 타이머 모두 정리
	UWorld* World = GetWorld();
	if (World)
	{
		for (FTimerHandle& TimerHandle : ActiveTimerHandles)
		{
			World->GetTimerManager().ClearTimer(TimerHandle);
		}
	}
	ActiveTimerHandles.Empty();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
