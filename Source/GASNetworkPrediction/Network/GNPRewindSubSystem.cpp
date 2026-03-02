#include "GNPRewindSubSystem.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"

// 디버그 CVar
static TAutoConsoleVariable<int32> CVarShowRewindDebug(
	TEXT("GNP.ShowRewindDebug"),
	0,
	TEXT("0: Off, 1: Rewind positions, 2: Full history"),
	ECVF_Cheat
);

TStatId UGNPRewindSubSystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UGNPRewindSubSystem, STATGROUP_Tickables);
}

void UGNPRewindSubSystem::Tick(float DeltaTime)
{
	// 서버에서만 위치 기록
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client)
	{
		return;
	}

	// 30Hz 간격으로 기록
	TimeSinceLastRecord += DeltaTime;
	if (TimeSinceLastRecord < RecordInterval)
	{
		return;
	}
	TimeSinceLastRecord = 0.0f;

	const float CurrentTime = World->GetTimeSeconds();

	// 등록된 모든 액터의 위치 기록
	for (auto It = PositionHistories.CreateIterator(); It; ++It)
	{
		AActor* Actor = It->Key.Get();
		if (!Actor)
		{
			// 유효하지 않은 액터 제거
			It.RemoveCurrent();
			continue;
		}

		FActorPositionHistory& History = It->Value;

		// 스냅샷 추가
		FPositionSnapshot Snapshot;
		Snapshot.ServerTime = CurrentTime;
		Snapshot.Location = Actor->GetActorLocation();
		Snapshot.Rotation = Actor->GetActorRotation();
		History.Snapshots.Add(Snapshot);

		// 오래된 스냅샷 정리
		TrimHistory(History, CurrentTime);
	}

	// 디버그 레벨 2: 전체 히스토리 시각화
	if (CVarShowRewindDebug.GetValueOnGameThread() >= 2)
	{
		for (const auto& Pair : PositionHistories)
		{
			const AActor* Actor = Pair.Key.Get();
			if (!Actor) continue;

			const FActorPositionHistory& History = Pair.Value;
			for (int32 i = 0; i < History.Snapshots.Num(); ++i)
			{
				const FPositionSnapshot& Snap = History.Snapshots[i];
				// 흰색 점으로 히스토리 표시
				DrawDebugPoint(World, Snap.Location, 5.0f, FColor::White, false, RecordInterval * 2.0f);

				// 스냅샷 간 연결선
				if (i > 0)
				{
					DrawDebugLine(World, History.Snapshots[i - 1].Location, Snap.Location,
						FColor(128, 128, 128), false, RecordInterval * 2.0f);
				}
			}
		}
	}
}

void UGNPRewindSubSystem::RegisterActor(AActor* Actor)
{
	if (!Actor)
	{
		return;
	}

	if (!PositionHistories.Contains(Actor))
	{
		PositionHistories.Add(Actor, FActorPositionHistory());
		UE_LOG(LogTemp, Log, TEXT("[Rewind] Actor registered: %s"), *Actor->GetName());
	}
}

void UGNPRewindSubSystem::UnregisterActor(AActor* Actor)
{
	if (Actor && PositionHistories.Remove(Actor) > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[Rewind] Actor unregistered: %s"), *Actor->GetName());
	}
}

void UGNPRewindSubSystem::RewindTo(float ServerTime)
{
	if (bIsRewound)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Rewind] Already rewound! Call RestorePositions() first."));
		return;
	}

	for (auto& Pair : PositionHistories)
	{
		AActor* Actor = Pair.Key.Get();
		if (!Actor) continue;

		FActorPositionHistory& History = Pair.Value;
		const TArray<FPositionSnapshot>& Snapshots = History.Snapshots;

		if (Snapshots.Num() == 0) continue;

		// 현재 위치 저장 (복원용)
		History.SavedLocation = Actor->GetActorLocation();
		History.SavedRotation = Actor->GetActorRotation();

		// 요청 시간이 히스토리 범위 밖이면 가장 가까운 스냅샷 사용
		if (ServerTime <= Snapshots[0].ServerTime)
		{
			Actor->SetActorLocationAndRotation(
				Snapshots[0].Location,
				Snapshots[0].Rotation,
				false, nullptr, ETeleportType::TeleportPhysics
			);
			continue;
		}

		if (ServerTime >= Snapshots.Last().ServerTime)
		{
			Actor->SetActorLocationAndRotation(
				Snapshots.Last().Location,
				Snapshots.Last().Rotation,
				false, nullptr, ETeleportType::TeleportPhysics
			);
			continue;
		}

		// 두 스냅샷 사이 보간
		for (int32 i = 0; i < Snapshots.Num() - 1; ++i)
		{
			if (ServerTime >= Snapshots[i].ServerTime && ServerTime <= Snapshots[i + 1].ServerTime)
			{
				FPositionSnapshot Interpolated = InterpolateSnapshots(
					Snapshots[i], Snapshots[i + 1], ServerTime);

				Actor->SetActorLocationAndRotation(
					Interpolated.Location,
					Interpolated.Rotation,
					false, nullptr, ETeleportType::TeleportPhysics
				);
				break;
			}
		}
	}

	bIsRewound = true;
}

void UGNPRewindSubSystem::RestorePositions()
{
	if (!bIsRewound)
	{
		return;
	}

	for (auto& Pair : PositionHistories)
	{
		AActor* Actor = Pair.Key.Get();
		if (!Actor) continue;

		const FActorPositionHistory& History = Pair.Value;

		Actor->SetActorLocationAndRotation(
			History.SavedLocation,
			History.SavedRotation,
			false, nullptr, ETeleportType::TeleportPhysics
		);
	}

	bIsRewound = false;
}

bool UGNPRewindSubSystem::IsRegisteredActor(const AActor* Actor) const
{
	if (!Actor) return false;

	for (const auto& Pair : PositionHistories)
	{
		if (Pair.Key.Get() == Actor)
		{
			return true;
		}
	}
	return false;
}

FHitResult UGNPRewindSubSystem::ValidateHitscanHit(
	float ClientTimestamp,
	FVector TraceStart,
	FVector TraceEnd,
	AActor* IgnoreActor)
{
	FHitResult HitResult;
	UWorld* World = GetWorld();
	if (!World) return HitResult;

	// 되감기
	RewindTo(ClientTimestamp);

	// SweepTrace (되감긴 위치 기준, 보간 오차 보정)
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(IgnoreActor);
	Params.bTraceComplex = false;

	const FCollisionShape SweepShape = FCollisionShape::MakeSphere(ServerTraceRadius);
	World->SweepSingleByChannel(
		HitResult,
		TraceStart,
		TraceEnd,
		FQuat::Identity,
		ECC_Visibility,
		SweepShape,
		Params
	);

	// 히트 대상이 등록된 Rewind 액터인지 확인
	const bool bHitRewindActor = HitResult.bBlockingHit && IsRegisteredActor(HitResult.GetActor());

	// 디버그 시각화 (등록 액터 히트 시에만 캡슐 표시)
	if (CVarShowRewindDebug.GetValueOnGameThread() >= 1)
	{
		DrawRewindDebug(ClientTimestamp, TraceStart, TraceEnd, HitResult, bHitRewindActor);
	}

	// 원래 위치로 복원
	RestorePositions();

	return HitResult;
}

void UGNPRewindSubSystem::DrawRewindDebug(
	float ServerTime,
	FVector TraceStart,
	FVector TraceEnd,
	const FHitResult& HitResult,
	bool bHitRewindActor,
	float Duration)
{
	UWorld* World = GetWorld();
	if (!World) return;

	const float CurrentTime = World->GetTimeSeconds();
	const float RewindDelta = CurrentTime - ServerTime;

	// 등록 액터 히트 시에만 캡슐 시각화 (바닥/벽 히트 시 불필요)
	if (bHitRewindActor)
	{
		for (const auto& Pair : PositionHistories)
		{
			const AActor* Actor = Pair.Key.Get();
			if (!Actor) continue;

			const FActorPositionHistory& History = Pair.Value;

			// 빨강 캡슐: 현재 위치 (되감기 전 저장된 위치)
			DrawDebugCapsule(World, History.SavedLocation, 88.0f, 34.0f,
				FQuat(History.SavedRotation), FColor::Red, false, Duration, 0, 1.5f);

			// 파랑 캡슐: 되감긴 위치 (현재 액터 위치 = 되감긴 상태)
			DrawDebugCapsule(World, Actor->GetActorLocation(), 88.0f, 34.0f,
				FQuat(Actor->GetActorRotation()), FColor::Blue, false, Duration, 0, 1.5f);

			// 되감기 시간 텍스트
			FString TimeText = FString::Printf(TEXT("Rewind: -%.0fms"), RewindDelta * 1000.0f);
			DrawDebugString(World, Actor->GetActorLocation() + FVector(0, 0, 120),
				TimeText, nullptr, FColor::Yellow, Duration, true);
		}

		// 초록: 적 히트 트레이스
		DrawDebugLine(World, TraceStart, HitResult.ImpactPoint, FColor::Green, false, Duration, 0, 1.5f);
		DrawDebugSphere(World, HitResult.ImpactPoint, 10.0f, 8, FColor::Green, false, Duration, 0, 1.5f);
		DrawDebugString(World, HitResult.ImpactPoint + FVector(0, 0, 30),
			TEXT("HIT CONFIRMED"), nullptr, FColor::Green, Duration, true);
	}
}

FPositionSnapshot UGNPRewindSubSystem::InterpolateSnapshots(
	const FPositionSnapshot& A,
	const FPositionSnapshot& B,
	float TargetTime) const
{
	const float TimeDiff = B.ServerTime - A.ServerTime;
	const float Alpha = (TimeDiff > KINDA_SMALL_NUMBER)
		? FMath::Clamp((TargetTime - A.ServerTime) / TimeDiff, 0.0f, 1.0f)
		: 0.0f;

	FPositionSnapshot Result;
	Result.ServerTime = TargetTime;
	Result.Location = FMath::Lerp(A.Location, B.Location, Alpha);
	Result.Rotation = FMath::Lerp(A.Rotation, B.Rotation, Alpha);
	return Result;
}

void UGNPRewindSubSystem::TrimHistory(FActorPositionHistory& History, float CurrentTime)
{
	const float OldestAllowed = CurrentTime - MaxHistoryDuration;

	// 오래된 스냅샷 제거 (최소 1개 유지)
	while (History.Snapshots.Num() > 1 && History.Snapshots[0].ServerTime < OldestAllowed)
	{
		History.Snapshots.RemoveAt(0);
	}
}
