#include "GNPGameplayAbility_Hitscan.h"
#include "AbilitySystemComponent.h"
#include "Network/GNPRewindSubSystem.h"
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

// Rewind 방식 선택
// 0: Ping 기반 (기본, 안정적) - PlayerState Ping으로 풀 RTT 계산
// 1: 타임스탬프 기반 (실험) - 클라이언트가 보낸 GameState 서버 시간 직접 사용
static TAutoConsoleVariable<int32> CVarRewindMode(
	TEXT("GNP.RewindMode"),
	1,
	TEXT("0: Ping 기반  1: 타임스탬프 기반 (기본)"),
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

	const float ServerTimeNow = World->GetTimeSeconds();
	float RewindToTime = 0.0f;
	float RewindSeconds = 0.0f;
	const int32 RewindMode = CVarRewindMode.GetValueOnGameThread();

	if (RewindMode == 1)
	{
		// 타임스탬프 방식: 클라이언트가 보낸 GameState 서버 시간을 Rewind 목표로 직접 사용
		// 클라이언트의 GetServerWorldTimeSeconds()는 이미 리플리케이션 지연만큼 뒤처진 값이므로
		// 별도 Ping 계산 없이 그 시점으로 바로 되감기
		const float TimeDiff = ServerTimeNow - ClientTimestamp;

		// 타임스탬프가 미래이거나(조작 의심) 너무 오래된 경우 거부
		if (TimeDiff < 0.0f || TimeDiff > MaxRewindTime)
		{
			return;
		}

		RewindSeconds = TimeDiff;
		RewindToTime = ClientTimestamp;
	}
	else
	{
		// Ping 기반 방식 (기본, 안정적)
		// 풀 RTT만큼 되감기: 클라이언트 뷰 지연(HalfRTT) + 패킷 전송(HalfRTT) = FullRTT
		float PingMs = 0.0f;
		APlayerController* PC = Cast<APlayerController>(GetActorInfo().PlayerController.Get());
		if (PC)
		{
			if (APlayerState* PS = PC->GetPlayerState<APlayerState>())
			{
				PingMs = static_cast<float>(PS->GetPingInMilliseconds());
			}
		}

		RewindSeconds = PingMs / 1000.0f;

		// 치팅 방지: 되감기 시간이 최대값 초과 시 거부
		if (RewindSeconds > MaxRewindTime)
		{
			return;
		}

		RewindToTime = ServerTimeNow - RewindSeconds;
	}

	// Rewind 서브시스템으로 되감기 + 트레이스 + 복원
	UGNPRewindSubSystem* RewindSys = World->GetSubsystem<UGNPRewindSubSystem>();
	if (!RewindSys)
	{
		return;
	}

	FHitResult ServerHitResult = RewindSys->ValidateHitscanHit(
		RewindToTime, TraceStart, TraceEnd, AvatarActor);

	// 디버그 레벨 2: Rewind 정보 화면 표시
	if (CVarShowHitscanDebug.GetValueOnGameThread() >= 2)
	{
		const TCHAR* ModeText = (RewindMode == 1) ? TEXT("TIMESTAMP") : TEXT("PING");
		const FString DebugText = FString::Printf(
			TEXT("[%s] Rewind: %.0fms  Hit: %s"),
			ModeText, RewindSeconds * 1000.0f,
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
