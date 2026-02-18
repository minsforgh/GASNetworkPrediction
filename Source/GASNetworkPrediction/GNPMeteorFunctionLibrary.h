#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GNPMeteorFunctionLibrary.generated.h"

// 메테오 착탄 정보 구조체
USTRUCT(BlueprintType)
struct FMeteorImpactData
{
	GENERATED_BODY()

	// 착탄 위치 (지면)
	UPROPERTY(BlueprintReadOnly, Category = "Meteor")
	FVector Location = FVector::ZeroVector;

	// 지면 법선 (데칼 회전용)
	UPROPERTY(BlueprintReadOnly, Category = "Meteor")
	FVector Normal = FVector::UpVector;

	// 착탄 지연 시간 (순차 낙하용)
	UPROPERTY(BlueprintReadOnly, Category = "Meteor")
	float ImpactDelay = 0.0f;
};

UCLASS()
class GASNETWORKPREDICTION_API UGNPMeteorFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Seed 기반 결정론적 메테오 착탄 위치 계산 (물리 기반)
	 * 서버/클라이언트가 같은 Seed로 동일한 결과를 얻음
	 * ImpactDelay = (랜덤 시작 높이 + 지형 높이 차이) / 낙하 속도
	 *
	 * @param Seed 랜덤 시드 (서버에서 생성, 클라이언트로 전달)
	 * @param CenterLocation 메테오 범위 중심점
	 * @param MinRadius 최소 낙하 반경
	 * @param MaxRadius 최대 낙하 반경
	 * @param MeteorCount 메테오 개수
	 * @param MinStartHeight 최소 시작 높이 (CenterLocation.Z 기준)
	 * @param MaxStartHeight 최대 시작 높이 (CenterLocation.Z 기준)
	 * @param FallSpeed 낙하 속도 (units/sec)
	 * @param WorldContext Line Trace용 월드 컨텍스트
	 * @return 각 메테오의 착탄 정보 배열
	 */
	UFUNCTION(BlueprintCallable, Category = "GNP|Meteor", meta = (WorldContext = "WorldContextObject"))
	static TArray<FMeteorImpactData> CalculateMeteorImpacts(
		int32 Seed,
		FVector CenterLocation,
		float MinRadius,
		float MaxRadius,
		int32 MeteorCount,
		float MinStartHeight,
		float MaxStartHeight,
		float FallSpeed,
		const UObject* WorldContextObject
	);

	/**
	 * 랜덤 Seed 생성 (서버에서만 호출)
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "GNP|Meteor")
	static int32 GenerateMeteorSeed();

	/**
	 * ImpactData 배열에서 가장 긴 ImpactDelay 반환
	 * End Ability 타이밍 계산용
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "GNP|Meteor")
	static float GetMaxImpactDelay(const TArray<FMeteorImpactData>& ImpactDataArray);

	/**
	 * ImpactData 배열에서 Location 배열만 추출
	 * 단일 나이아가라 시스템에 배열 파라미터로 전달용
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "GNP|Meteor")
	static TArray<FVector> ExtractImpactLocations(const TArray<FMeteorImpactData>& ImpactDataArray);

	/**
	 * ImpactData 배열에서 ImpactDelay 배열만 추출
	 * 단일 나이아가라 시스템에 배열 파라미터로 전달용
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "GNP|Meteor")
	static TArray<float> ExtractImpactDelays(const TArray<FMeteorImpactData>& ImpactDataArray);
};
