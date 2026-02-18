#include "MeteorTargetActor.h"
#include "Abilities/GameplayAbilityWorldReticle.h"
#include "Abilities/GameplayAbility.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "Components/DecalComponent.h"

AMeteorTargetActor::AMeteorTargetActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;
}

void AMeteorTargetActor::StartTargeting(UGameplayAbility* Ability)
{
	Super::StartTargeting(Ability);

	// SourceActor, PrimaryPC 초기화
	if (Ability)
	{
		SourceActor = Ability->GetAvatarActorFromActorInfo();

		if (!PrimaryPC)
		{
			PrimaryPC = Cast<APlayerController>(Ability->GetActorInfo().PlayerController);
		}
	}

	SpawnReticleActor(Ability);

}

void AMeteorTargetActor::SpawnReticleActor(UGameplayAbility* Ability)
{
	// ReticleClass는 부모 클래스에서 이미 정의된 멤버
	if (GetWorld() && ReticleClass)
	{
		ReticleActor = GetWorld()->SpawnActor<AGameplayAbilityWorldReticle>(ReticleClass, GetActorLocation(), GetActorRotation());

		if (ReticleActor)
		{
			ReticleActor->InitializeReticle(this, PrimaryPC, ReticleParams);
			ReticleActor->SetActorHiddenInGame(false);

		}
	}
}

void AMeteorTargetActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!SourceActor || !PrimaryPC || !PrimaryPC->IsLocalController())
	{
		return;
	}

	FHitResult Hit;
	FVector ViewLocation;
	FRotator ViewRotation;
	PrimaryPC->GetPlayerViewPoint(ViewLocation, ViewRotation);

	FVector TraceStart = ViewLocation;
	FVector TraceEnd = ViewLocation + (ViewRotation.Vector() * TraceRange);

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(SourceActor); // 소유자 제외
	Params.AddIgnoredActor(this);        // 본인 제외

	if (GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, Params))
	{
		// TargetActor Hit 위치로
		SetActorLocation(Hit.Location);

		if (ReticleActor)
		{
			// 시전자와 타겟 지점 사이의 거리 계산
			float Distance = FVector::Dist(SourceActor->GetActorLocation(), Hit.Location);

			// 사거리 내에 있는지 판단하여 색상 선택
			FLinearColor TargetColor = (Distance <= MaxRange) ? ValidColor : InvalidColor;

			if (UDecalComponent* DecalComp = ReticleActor->GetComponentByClass<UDecalComponent>())
			{
				// 처음에 한번만 생성
				if (!ReticleDynamicMaterial)
				{
					ReticleDynamicMaterial = DecalComp->CreateDynamicMaterialInstance();
				}

				// Color Parameter 사용해서 색 변경
				if (ReticleDynamicMaterial)
				{
					ReticleDynamicMaterial->SetVectorParameterValue(TEXT("Color"), TargetColor);
				}
			}

			ReticleActor->SetActorHiddenInGame(false);
			ReticleActor->SetActorLocation(Hit.Location);

			// 바닥 면에 딱 붙도록 Hit 결과의 법선 방향으로 회전 값 설정
			FRotator TargetRotation = FRotationMatrix::MakeFromZ(Hit.ImpactNormal).Rotator();
			ReticleActor->SetActorRotation(TargetRotation);
		}
	}
	else
	{
		// Hit 없을 시 Reticle 숨김
		if (ReticleActor)
		{
			ReticleActor->SetActorHiddenInGame(true);
		}
	}

}

void AMeteorTargetActor::ConfirmTargetingAndContinue()
{
	if (!SourceActor) return;
	float Distance = FVector::Dist(SourceActor->GetActorLocation(), GetActorLocation());

	// 사거리 안에 있을 경우에만
	if (Distance <= MaxRange)
	{
		// Target 지점의 위치 데이터를 담는 Handle
		FGameplayAbilityTargetDataHandle DataHandle;

		// 위치 데이터 저장용 구조체 생성
		FGameplayAbilityTargetData_LocationInfo* LocData = new FGameplayAbilityTargetData_LocationInfo();

		// TargetLocation: 메테오 중심점
		LocData->TargetLocation.LiteralTransform = GetActorTransform();
		LocData->TargetLocation.LocationType = EGameplayAbilityTargetingLocationType::LiteralTransform;

		// SourceLocation: Seed를 X좌표에 저장 (클라이언트가 생성)
		int32 Seed = UGNPMeteorFunctionLibrary::GenerateMeteorSeed();
		LocData->SourceLocation.LiteralTransform.SetLocation(FVector(static_cast<float>(Seed), 0.0f, 0.0f));
		LocData->SourceLocation.LocationType = EGameplayAbilityTargetingLocationType::LiteralTransform;

		DataHandle.Add(LocData);

		// WaitTargetData 태스크에게 데이터가 준비됐음을 알림
		TargetDataReadyDelegate.Broadcast(DataHandle);
	}
	else
	{
		// 사거리 밖이면 취소 (또는 알림)
		UE_LOG(LogTemp, Warning, TEXT("Target is out of range!"));
	}
}

void AMeteorTargetActor::ConfirmTargeting()
{
	if (ReticleActor)
	{
		ReticleActor->Destroy();
		ReticleActor = nullptr;
	}

	// 부모의 ConfirmTargeting 호출
	Super::ConfirmTargeting();
}

void AMeteorTargetActor::CancelTargeting()
{
	if (ReticleActor)
	{
		ReticleActor->Destroy();
		ReticleActor = nullptr;
	}

	// 부모의 CancelTargeting 호출
	Super::CancelTargeting();
}