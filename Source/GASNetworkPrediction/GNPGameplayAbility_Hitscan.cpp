#include "GNPGameplayAbility_Hitscan.h"
#include "AbilitySystemComponent.h"
#include "GNPRewindSubSystem.h"
#include "GameFramework/PlayerController.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"

// 디버그 CVar
static TAutoConsoleVariable<int32> CVarShowHitscanDebug(
	TEXT("GNP.ShowHitscanDebug"),
	0,
	TEXT("0: Off, 1: Trace line, 2: + Rewind info"),
	ECVF_Cheat
);

// ============ NetSerialize ============

bool FGameplayAbilityTargetData_HitscanRewind::NetSerialize(
	FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	Ar << ClientTimestamp;
	TraceStart.NetSerialize(Ar, Map, bOutSuccess);
	TraceEnd.NetSerialize(Ar, Map, bOutSuccess);
	ClientHitResult.NetSerialize(Ar, Map, bOutSuccess);

	bOutSuccess = true;
	return true;
}

// ============ GNPGameplayAbility_Hitscan ============

UGNPGameplayAbility_Hitscan::UGNPGameplayAbility_Hitscan()
{
	// 기본값은 부모(GNPGameplayAbility)에서 설정됨
	// LocalPredicted, InstancedPerActor, ServerOnlyTermination
}

void UGNPGameplayAbility_Hitscan::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (ActorInfo->IsLocallyControlled())
	{
		if (ActorInfo->IsNetAuthority())
		{
			// Listen Server 호스트: Rewind 불필요, 즉시 서버 트레이스
			PerformServerDirectTrace();
		}
		else
		{
			// 클라이언트: 즉시 비주얼 + 서버로 TargetData 전송
			PerformClientTrace();
		}
	}
	else if (ActorInfo->IsNetAuthority())
	{
		// 서버 (원격 클라이언트의 어빌리티): TargetData 대기
		UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
		if (ASC)
		{
			ASC->AbilityTargetDataSetDelegate(
				Handle,
				ActivationInfo.GetActivationPredictionKey()
			).AddUObject(this, &UGNPGameplayAbility_Hitscan::OnTargetDataReceived);
		}
	}
}

void UGNPGameplayAbility_Hitscan::PerformClientTrace()
{
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!AvatarActor) return;

	APlayerController* PC = Cast<APlayerController>(GetActorInfo().PlayerController.Get());
	if (!PC) return;

	// 카메라 시점에서 레이캐스트
	FVector ViewLoc;
	FRotator ViewRot;
	PC->GetPlayerViewPoint(ViewLoc, ViewRot);

	const FVector TraceStart = ViewLoc;
	const FVector TraceEnd = ViewLoc + ViewRot.Vector() * TraceRange;

	FHitResult HitResult;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(AvatarActor);

	const bool bHit = GetWorld()->LineTraceSingleByChannel(
		HitResult, TraceStart, TraceEnd, ECC_Visibility, Params);

	// 즉시 BP 이펙트 표시
	OnHitscanFired(TraceStart, bHit ? HitResult.ImpactPoint : TraceEnd, HitResult, bHit);

	// 클라이언트 디버그
	if (CVarShowHitscanDebug.GetValueOnGameThread() >= 1)
	{
		const FColor TraceColor = bHit ? FColor::Green : FColor::Yellow;
		const FVector EndPoint = bHit ? HitResult.ImpactPoint : TraceEnd;
		DrawDebugLine(GetWorld(), TraceStart, EndPoint, TraceColor, false, 3.0f, 0, 1.0f);
		if (bHit)
		{
			DrawDebugSphere(GetWorld(), HitResult.ImpactPoint, 8.0f, 8, FColor::Red, false, 3.0f);
		}
	}

	// 서버 시간 타임스탬프 (GameState에서 리플리케이트된 서버 시간)
	const AGameStateBase* GameState = GetWorld()->GetGameState<AGameStateBase>();
	const float Timestamp = GameState ? GameState->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();

	// TargetData 구성 + 서버 전송
	FGameplayAbilityTargetData_HitscanRewind* RewindData = new FGameplayAbilityTargetData_HitscanRewind();
	RewindData->ClientTimestamp = Timestamp;
	RewindData->TraceStart = TraceStart;
	RewindData->TraceEnd = TraceEnd;
	RewindData->ClientHitResult = HitResult;

	FGameplayAbilityTargetDataHandle DataHandle;
	DataHandle.Add(RewindData);

	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (ASC)
	{
		ASC->ServerSetReplicatedTargetData(
			GetCurrentAbilitySpecHandle(),
			GetCurrentActivationInfo().GetActivationPredictionKey(),
			DataHandle,
			FGameplayTag(),
			ASC->ScopedPredictionKey
		);
	}

	// 클라이언트에서는 바로 종료
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(),
		GetCurrentActivationInfo(), true, false);
}

void UGNPGameplayAbility_Hitscan::PerformServerDirectTrace()
{
	// Listen Server 호스트: Rewind 없이 즉시 판정
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!AvatarActor) return;

	APlayerController* PC = Cast<APlayerController>(
		GetActorInfo().PlayerController.Get());
	if (!PC) return;

	FVector ViewLoc;
	FRotator ViewRot;
	PC->GetPlayerViewPoint(ViewLoc, ViewRot);

	const FVector TraceStart = ViewLoc;
	const FVector TraceEnd = ViewLoc + ViewRot.Vector() * TraceRange;

	FHitResult HitResult;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(AvatarActor);

	const bool bHit = GetWorld()->LineTraceSingleByChannel(
		HitResult, TraceStart, TraceEnd, ECC_Visibility, Params);

	// BP 이펙트 표시
	OnHitscanFired(TraceStart, bHit ? HitResult.ImpactPoint : TraceEnd, HitResult, bHit);

	// 호스트 디버그
	if (CVarShowHitscanDebug.GetValueOnGameThread() >= 1)
	{
		const FColor TraceColor = bHit ? FColor::Green : FColor::Yellow;
		const FVector EndPoint = bHit ? HitResult.ImpactPoint : TraceEnd;
		DrawDebugLine(GetWorld(), TraceStart, EndPoint, TraceColor, false, 3.0f, 0, 1.0f);
		if (bHit)
		{
			DrawDebugSphere(GetWorld(), HitResult.ImpactPoint, 8.0f, 8, FColor::Green, false, 3.0f);
			DrawDebugString(GetWorld(), HitResult.ImpactPoint + FVector(0, 0, 30),
				TEXT("HOST HIT (No Rewind)"), nullptr, FColor::Cyan, 3.0f, true);
		}
	}

	// 히트 시 데미지 적용 (서버 직접)
	if (bHit && HitResult.GetActor())
	{
		if (UAbilitySystemComponent* TargetASC =
			HitResult.GetActor()->FindComponentByClass<UAbilitySystemComponent>())
		{
			if (DamageGameplayEffect)
			{
				FGameplayEffectSpecHandle Spec = MakeOutgoingGameplayEffectSpec(
					GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(),
					GetCurrentActivationInfo(), DamageGameplayEffect, GetAbilityLevel());

				if (Spec.IsValid())
				{
					TargetASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
				}
			}
		}
	}

	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(),
		GetCurrentActivationInfo(), true, false);
}

void UGNPGameplayAbility_Hitscan::OnTargetDataReceived(
	const FGameplayAbilityTargetDataHandle& Data,
	FGameplayTag ActivationTag)
{
	// Delegate 해제
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (ASC)
	{
		ASC->AbilityTargetDataSetDelegate(
			GetCurrentAbilitySpecHandle(),
			GetCurrentActivationInfo().GetActivationPredictionKey()
		).RemoveAll(this);
	}

	// TargetData에서 Rewind 정보 추출
	if (Data.Num() > 0)
	{
		const FGameplayAbilityTargetData_HitscanRewind* RewindData =
			static_cast<const FGameplayAbilityTargetData_HitscanRewind*>(Data.Get(0));

		if (RewindData)
		{
			ServerValidateAndApplyDamage(
				RewindData->ClientTimestamp,
				FVector(RewindData->TraceStart),
				FVector(RewindData->TraceEnd)
			);
		}
	}

	// 서버에서 종료
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
}

void UGNPGameplayAbility_Hitscan::ServerValidateAndApplyDamage(
	float ClientTimestamp,
	FVector TraceStart,
	FVector TraceEnd)
{
	UWorld* World = GetWorld();
	if (!World) return;

	AActor* AvatarActor = GetAvatarActorFromActorInfo();

	// Ping 기반 Rewind 시간 계산 (서버 시계만 사용, 클라이언트 시계 의존 없음)
	// 풀 RTT만큼 되감기: 클라이언트는 HalfRTT 전의 세상을 보고 + 발사 패킷이 HalfRTT 걸려 도착
	const float ServerTimeNow = World->GetTimeSeconds();
	float PingMs = 0.0f;

	APlayerController* PC = Cast<APlayerController>(GetActorInfo().PlayerController.Get());
	if (PC)
	{
		if (APlayerState* PS = PC->GetPlayerState<APlayerState>())
		{
			PingMs = static_cast<float>(PS->GetPingInMilliseconds());
		}
	}

	const float RewindSeconds = PingMs / 1000.0f;

	// 치팅 방지: 되감기 시간이 최대값 초과 시 거부
	if (RewindSeconds > MaxRewindTime)
	{
		return;
	}

	const float RewindToTime = ServerTimeNow - RewindSeconds;

	// Rewind 서브시스템으로 되감기 + 트레이스 + 복원
	UGNPRewindSubSystem* RewindSys = World->GetSubsystem<UGNPRewindSubSystem>();
	if (!RewindSys)
	{
		return;
	}

	FHitResult ServerHitResult = RewindSys->ValidateHitscanHit(
		RewindToTime, TraceStart, TraceEnd, AvatarActor);

	// 디버그 레벨 2: Ping/Rewind 정보를 화면에 표시
	if (CVarShowHitscanDebug.GetValueOnGameThread() >= 2)
	{
		const FString DebugText = FString::Printf(TEXT("Ping: %.0fms  Rewind: %.0fms  Hit: %s"),
			PingMs, PingMs,
			ServerHitResult.bBlockingHit ? TEXT("YES") : TEXT("NO"));
		DrawDebugString(World, TraceStart + FVector(0, 0, 50),
			DebugText, nullptr, FColor::Yellow, 3.0f, true);
	}

	// 히트 시 데미지 적용 + 서버 이펙트 표시
	if (ServerHitResult.bBlockingHit && ServerHitResult.GetActor())
	{
		if (UAbilitySystemComponent* TargetASC =
			ServerHitResult.GetActor()->FindComponentByClass<UAbilitySystemComponent>())
		{
			// 서버에서 히트 이펙트 표시 (호스트 화면에서 보이도록)
			OnHitscanFired(TraceStart, ServerHitResult.ImpactPoint, ServerHitResult, true);

			if (DamageGameplayEffect)
			{
				FGameplayEffectSpecHandle Spec = MakeOutgoingGameplayEffectSpec(
					GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(),
					GetCurrentActivationInfo(), DamageGameplayEffect, GetAbilityLevel());

				if (Spec.IsValid())
				{
					TargetASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
				}
			}
		}
	}
}
