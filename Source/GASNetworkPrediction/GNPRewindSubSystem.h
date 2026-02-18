#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GNPRewindSubSystem.generated.h"

// 위치 스냅샷 (특정 시점의 액터 상태)
USTRUCT()
struct FPositionSnapshot
{
	GENERATED_BODY()

	float ServerTime = 0.0f;
	FVector Location = FVector::ZeroVector;
	FRotator Rotation = FRotator::ZeroRotator;
};

// 액터별 위치 히스토리
USTRUCT()
struct FActorPositionHistory
{
	GENERATED_BODY()

	// 시간순 정렬된 스냅샷 배열 (오래된 것 -> 최신)
	TArray<FPositionSnapshot> Snapshots;

	// 되감기 전 원래 위치 (복원용)
	FVector SavedLocation = FVector::ZeroVector;
	FRotator SavedRotation = FRotator::ZeroRotator;
};

/**
 * Server Rewind 서브시스템
 * 매 틱 추적 대상 액터의 위치를 기록하고,
 * 특정 서버 시점으로 되감기/복원하여 히트스캔 판정을 수행
 */
UCLASS()
class GASNETWORKPREDICTION_API UGNPRewindSubSystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	// UTickableWorldSubsystem 인터페이스
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	// 추적 대상 등록/해제
	void RegisterActor(AActor* Actor);
	void UnregisterActor(AActor* Actor);

	// 특정 서버 시간으로 모든 추적 액터 되감기
	void RewindTo(float ServerTime);

	// 되감기 전 원래 위치로 복원
	void RestorePositions();

	/**
	 * 되감기 + 레이캐스트 + 복원을 한번에 수행
	 * @param ClientTimestamp 클라이언트가 발사한 시점의 서버 시간
	 * @param TraceStart 레이캐스트 시작점
	 * @param TraceEnd 레이캐스트 끝점
	 * @param IgnoreActor 트레이스에서 무시할 액터 (발사자)
	 * @return 히트 결과
	 */
	FHitResult ValidateHitscanHit(
		float ClientTimestamp,
		FVector TraceStart,
		FVector TraceEnd,
		AActor* IgnoreActor
	);

	// 히트된 액터가 Rewind 등록 액터인지 확인
	bool IsRegisteredActor(const AActor* Actor) const;

	// 디버그: 되감긴 위치 시각화 (bHitRewindActor: 히트된 대상이 등록 액터인지)
	void DrawRewindDebug(float ServerTime, FVector TraceStart, FVector TraceEnd, const FHitResult& HitResult, bool bHitRewindActor, float Duration = 3.0f);

	// 서버 검증 트레이스 반지름 (LineTrace 대신 SweepTrace, 되감기 보간 오차 보정)
	float ServerTraceRadius = 12.0f;

private:
	// 액터별 히스토리
	TMap<TWeakObjectPtr<AActor>, FActorPositionHistory> PositionHistories;

	// 기록 설정
	float RecordInterval = 0.03333f;     // 30Hz (~33ms 간격)
	float MaxHistoryDuration = 1.0f;     // 최대 1초 히스토리
	float TimeSinceLastRecord = 0.0f;

	// 되감기 상태 (중첩 방지)
	bool bIsRewound = false;

	// 두 스냅샷 사이 보간
	FPositionSnapshot InterpolateSnapshots(
		const FPositionSnapshot& A,
		const FPositionSnapshot& B,
		float TargetTime
	) const;

	// 오래된 스냅샷 정리
	void TrimHistory(FActorPositionHistory& History, float CurrentTime);
};
