#include "GNPProjectile.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystem.h"
#include "Net/UnrealNetwork.h"
#include "EngineUtils.h"
#include "DrawDebugHelpers.h"

// 투사체 디버그 CVar
// 1: 예측 경로(초록) + 서버 확정 경로(파랑) + 스냅 지점(주황) 표시
static TAutoConsoleVariable<int32> CVarShowProjectileDebug(
	TEXT("GNP.ShowProjectileDebug"),
	0,
	TEXT("0: Off  1: 예측(초록)/서버(파랑) 경로 + 스냅 지점 표시"),
	ECVF_Cheat
);

AGNPProjectile::AGNPProjectile()
{
	PrimaryActorTick.bCanEverTick = true;

	// Replication
	bReplicates = true;
	SetReplicateMovement(true);

	// 충돌 컴포넌트
	CollisionComp = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComp"));
	CollisionComp->InitSphereRadius(15.f);
	CollisionComp->SetCollisionProfileName(TEXT("Projectile"));
	CollisionComp->SetGenerateOverlapEvents(true);
	RootComponent = CollisionComp;

	// 충돌 이벤트 바인딩
	CollisionComp->OnComponentHit.AddDynamic(this, &AGNPProjectile::OnHit);
	CollisionComp->OnComponentBeginOverlap.AddDynamic(this, &AGNPProjectile::OnOverlap);

	// 메시 컴포넌트
	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
	MeshComp->SetupAttachment(RootComponent);
	MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// 이동 컴포넌트
	MovementComp = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("MovementComp"));
	MovementComp->SetUpdatedComponent(CollisionComp);
	MovementComp->InitialSpeed = Speed;
	MovementComp->MaxSpeed = Speed;
	MovementComp->bRotationFollowsVelocity = true;
	MovementComp->bShouldBounce = false;
	MovementComp->ProjectileGravityScale = 0.f; // 중력 없음
}

void AGNPProjectile::BeginPlay()
{
	Super::BeginPlay();

	SetLifeSpan(LifeSpan);
	LastTickLocation = GetActorLocation();
}

void AGNPProjectile::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (CVarShowProjectileDebug.GetValueOnGameThread() < 1) return;

	const FVector CurrentLocation = GetActorLocation();

	if (!LastTickLocation.IsZero() && !LastTickLocation.Equals(CurrentLocation))
	{
		FColor LineColor;
		switch (PredictionState)
		{
		case EProjectilePredictionState::Predicted:
			LineColor = FColor::Green;   // 예측 투사체: 초록
			break;
		case EProjectilePredictionState::Confirmed:
			LineColor = FColor::Blue;    // 서버 확정 투사체: 파랑
			break;
		default:
			LineColor = FColor::White;   // 다른 클라이언트 투사체: 흰색
			break;
		}

		DrawDebugLine(GetWorld(), LastTickLocation, CurrentLocation,
			LineColor, false, 5.0f, 0, 1.5f);
	}

	LastTickLocation = CurrentLocation;
}

void AGNPProjectile::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AGNPProjectile, ProjectileID);
}

void AGNPProjectile::InitProjectile(AActor* InInstigator, const FGameplayEffectSpecHandle& InDamageEffectSpec, int32 InProjectileID)
{
	// 엔진 기본 Instigator 사용 (자동 리플리케이트)
	SetInstigator(Cast<APawn>(InInstigator));
	DamageEffectSpec = InDamageEffectSpec;

	// 예측 시스템용 ID 설정 (리플리케이트됨)
	if (InProjectileID >= 0)
	{
		ProjectileID = InProjectileID;
	}

	// 발사자 무시
	if (InInstigator)
	{
		CollisionComp->IgnoreActorWhenMoving(InInstigator, true);
	}

	// 서버에서 스폰된 투사체는 Confirmed 상태
	if (HasAuthority())
	{
		PredictionState = EProjectilePredictionState::Confirmed;
	}
}

void AGNPProjectile::InitAsPredicted(AActor* InInstigator, int32 InProjectileID)
{
	// 예측 투사체로 초기화 (클라이언트 로컬)
	SetInstigator(Cast<APawn>(InInstigator));
	ProjectileID = InProjectileID;
	PredictionState = EProjectilePredictionState::Predicted;

	// 발사자 무시
	if (InInstigator)
	{
		CollisionComp->IgnoreActorWhenMoving(InInstigator, true);
	}

	// 예측 투사체는 충돌 비활성화 (시각적 전용)
	// 실제 데미지는 서버 투사체가 처리
	CollisionComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Green,
			FString::Printf(TEXT("[Predicted] Projectile ID: %d"), ProjectileID));
	}
}

void AGNPProjectile::OnRep_ProjectileID()
{
	// 서버 투사체가 클라이언트에 리플리케이트됨
	if (ProjectileID >= 0 && PredictionState == EProjectilePredictionState::None)
	{
		PredictionState = EProjectilePredictionState::Confirmed;

		AGNPProjectile* PredictedProj = FindPredictedProjectile(ProjectileID);
		if (PredictedProj)
		{
			// 스냅: 서버 투사체를 예측 투사체 위치로 이동
			const FVector ServerLocation = GetActorLocation();   // 스냅 전 서버 위치
			const FVector PredictedLocation = PredictedProj->GetActorLocation();
			const FRotator PredictedRotation = PredictedProj->GetActorRotation();
			const float LocationDiff = FVector::Dist(ServerLocation, PredictedLocation);

			// 스냅 지점 시각화: 서버 위치(주황) → 예측 위치(노랑) 차이 표시
			if (CVarShowProjectileDebug.GetValueOnGameThread() >= 1)
			{
				DrawDebugSphere(GetWorld(), ServerLocation, 8.f, 8, FColor::Orange, false, 5.0f);
				DrawDebugSphere(GetWorld(), PredictedLocation, 8.f, 8, FColor::Yellow, false, 5.0f);
				DrawDebugLine(GetWorld(), ServerLocation, PredictedLocation,
					FColor::Orange, false, 5.0f, 0, 2.0f);
			}

			SetActorLocationAndRotation(PredictedLocation, PredictedRotation);

			// 예측 투사체의 속도/방향 이어받기 (궤적 연속성 유지)
			if (MovementComp && PredictedProj->MovementComp)
			{
				MovementComp->Velocity = PredictedProj->MovementComp->Velocity;
			}

			// 스냅 후 서버가 위치를 다시 덮어쓰지 못하도록 차단
			// 클라이언트에서만 적용, 서버 권위적 충돌 판정에는 영향 없음
			SetReplicateMovement(false);

			// 예측 투사체 삭제
			PredictedProj->Destroy();

			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Blue,
					FString::Printf(TEXT("[Snap] ID: %d  Diff: %.1fcm"), ProjectileID, LocationDiff));
			}
		}
	}
}

AGNPProjectile* AGNPProjectile::FindPredictedProjectile(int32 InProjectileID) const
{
	// 월드에서 같은 ID + 같은 발사자의 예측 투사체 찾기
	for (TActorIterator<AGNPProjectile> It(GetWorld()); It; ++It)
	{
		AGNPProjectile* Proj = *It;
		if (Proj && Proj != this &&
			Proj->GetProjectileID() == InProjectileID &&
			Proj->GetInstigator() == GetInstigator() &&  // 같은 발사자인지 확인 (다중 클라이언트 대응)
			Proj->GetPredictionState() == EProjectilePredictionState::Predicted)
		{
			return Proj;
		}
	}
	return nullptr;
}

// 충돌 시
void AGNPProjectile::OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	// 발사자 제외
	if (OtherActor == GetInstigator())
	{
		return;
	}

	// 충돌 이펙트 스폰 (모든 클라이언트에서 시각적으로 표시)
	if (ImpactEffect)
	{
		UGameplayStatics::SpawnEmitterAtLocation(
			GetWorld(),
			ImpactEffect,
			Hit.ImpactPoint,
			Hit.ImpactNormal.Rotation(),
			true // 자동 파괴
		);
	}

	// 서버에서만 파괴 처리
	if (HasAuthority())
	{
		Destroy();
	}
}

void AGNPProjectile::OnOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
	bool bFromSweep, const FHitResult& SweepResult)
{
	// 초기화 전이면 무시 (InitProjectile 호출 전)
	if (GetInstigator() == nullptr)
	{
		return;
	}

	// 발사자 제외
	if (OtherActor == GetInstigator() || OtherActor == nullptr)
	{
		return;
	}

	// 중복 피격 방지 (서버/클라이언트 모두에서 체크)
	if (HitActors.Contains(OtherActor))
	{
		return;
	}

	// 충돌 이펙트 스폰 (모든 클라이언트에서 시각적으로 표시)
	if (ImpactEffect)
	{
		// Overlap은 정확한 충돌점이 없으므로 타겟 위치 사용
		FVector ImpactLocation = bFromSweep ? FVector(SweepResult.ImpactPoint) : OtherActor->GetActorLocation();
		FRotator ImpactRotation = (GetActorLocation() - ImpactLocation).Rotation();

		UGameplayStatics::SpawnEmitterAtLocation(
			GetWorld(),
			ImpactEffect,
			ImpactLocation,
			ImpactRotation,
			true // 자동 파괴
		);
	}

	// 서버에서만 데미지 처리
	if (!HasAuthority())
	{
		return;
	}

	// ASC 확인
	UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OtherActor);
	if (!TargetASC)
	{
		return;
	}

	// 데미지 적용
	if (DamageEffectSpec.IsValid())
	{
		TargetASC->ApplyGameplayEffectSpecToSelf(*DamageEffectSpec.Data.Get());
		HitActors.Add(OtherActor);

		// 디버그 출력
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Red,
				FString::Printf(TEXT("Hit: %s"), *OtherActor->GetName()));
		}

		// 단일 타겟 투사체면 파괴
		Destroy();
	}
}
