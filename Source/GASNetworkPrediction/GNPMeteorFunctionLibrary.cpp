#include "GNPMeteorFunctionLibrary.h"
#include "Engine/World.h"

TArray<FMeteorImpactData> UGNPMeteorFunctionLibrary::CalculateMeteorImpacts(
	int32 Seed,
	FVector CenterLocation,
	float MinRadius,
	float MaxRadius,
	int32 MeteorCount,
	float MinStartHeight,
	float MaxStartHeight,
	float FallSpeed,
	const UObject* WorldContextObject)
{
	TArray<FMeteorImpactData> Results;
	Results.Reserve(MeteorCount);

	// Seed 기반 랜덤 스트림 (서버/클라이언트 동일 결과)
	FRandomStream RandomStream(Seed);

	UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;

	// 0으로 나누기 방지
	if (FallSpeed <= 0.0f)
	{
		FallSpeed = 1000.0f;
	}

	for (int32 i = 0; i < MeteorCount; ++i)
	{
		FMeteorImpactData ImpactData;

		// 랜덤 방향 (XY 평면)
		float Angle = RandomStream.FRandRange(0.0f, 2.0f * PI);
		float Radius = RandomStream.FRandRange(MinRadius, MaxRadius);

		FVector Offset(
			FMath::Cos(Angle) * Radius,
			FMath::Sin(Angle) * Radius,
			0.0f
		);

		FVector TargetLocation = CenterLocation + Offset;

		// 랜덤 시작 높이 (CenterLocation.Z 기준 상대 높이)
		float RandomStartHeight = RandomStream.FRandRange(MinStartHeight, MaxStartHeight);
		float StartAbsoluteZ = CenterLocation.Z + RandomStartHeight;

		// Line Trace로 지면 찾기
		if (World)
		{
			FHitResult HitResult;
			FVector TraceStart = TargetLocation + FVector(0.0f, 0.0f, 5000.0f);
			FVector TraceEnd = TargetLocation - FVector(0.0f, 0.0f, 5000.0f);

			FCollisionQueryParams QueryParams;
			QueryParams.bTraceComplex = false;

			if (World->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_Visibility, QueryParams))
			{
				ImpactData.Location = HitResult.ImpactPoint;
				ImpactData.Normal = HitResult.ImpactNormal;
			}
			else
			{
				// 지면 못 찾으면 CenterLocation의 Z 사용
				ImpactData.Location = TargetLocation;
				ImpactData.Location.Z = CenterLocation.Z;
				ImpactData.Normal = FVector::UpVector;
			}
		}
		else
		{
			// World 없으면 그냥 위치만
			ImpactData.Location = TargetLocation;
			ImpactData.Location.Z = CenterLocation.Z;
			ImpactData.Normal = FVector::UpVector;
		}

		// 물리 기반 ImpactDelay 계산
		// 낙하 거리 = 시작 높이 - 착탄 위치 Z
		float FallDistance = StartAbsoluteZ - ImpactData.Location.Z;
		ImpactData.ImpactDelay = FMath::Max(0.0f, FallDistance / FallSpeed);

		Results.Add(ImpactData);
	}

	return Results;
}

int32 UGNPMeteorFunctionLibrary::GenerateMeteorSeed()
{
	// 현재 시간 기반 Seed 생성
	return FMath::Rand();
}

float UGNPMeteorFunctionLibrary::GetMaxImpactDelay(const TArray<FMeteorImpactData>& ImpactDataArray)
{
	float MaxDelay = 0.0f;

	for (const FMeteorImpactData& Data : ImpactDataArray)
	{
		if (Data.ImpactDelay > MaxDelay)
		{
			MaxDelay = Data.ImpactDelay;
		}
	}

	return MaxDelay;
}

TArray<FVector> UGNPMeteorFunctionLibrary::ExtractImpactLocations(const TArray<FMeteorImpactData>& ImpactDataArray)
{
	TArray<FVector> Locations;
	Locations.Reserve(ImpactDataArray.Num());

	for (const FMeteorImpactData& Data : ImpactDataArray)
	{
		Locations.Add(Data.Location);
	}

	return Locations;
}

TArray<float> UGNPMeteorFunctionLibrary::ExtractImpactDelays(const TArray<FMeteorImpactData>& ImpactDataArray)
{
	TArray<float> Delays;
	Delays.Reserve(ImpactDataArray.Num());

	for (const FMeteorImpactData& Data : ImpactDataArray)
	{
		Delays.Add(Data.ImpactDelay);
	}

	return Delays;
}
