#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayEffectTypes.h"
#include "GNPProjectile.generated.h"

class USphereComponent;
class UProjectileMovementComponent;
class UStaticMeshComponent;
class UGameplayEffect;
class UParticleSystem;

// 투사체 예측 상태 
UENUM(BlueprintType)
enum class EProjectilePredictionState : uint8
{
	None,        // 서버 권위적 (기본)
	Predicted,   // 클라이언트 예측 (로컬)
	Confirmed    // 서버 확정
};

UCLASS()
class GASNETWORKPREDICTION_API AGNPProjectile : public AActor
{
	GENERATED_BODY()

public:
	AGNPProjectile();

	// 초기화 - 서버 투사체 (블루프린트에서 호출)
	UFUNCTION(BlueprintCallable, Category = "Projectile")
	void InitProjectile(AActor* InInstigator, const FGameplayEffectSpecHandle& InDamageEffectSpec, int32 InProjectileID = -1);

	// 초기화 - 예측 투사체 (클라이언트 로컬)
	UFUNCTION(BlueprintCallable, Category = "Projectile|Prediction")
	void InitAsPredicted(AActor* InInstigator, int32 InProjectileID);

	// Getter
	UFUNCTION(BlueprintCallable, Category = "Projectile|Prediction")
	EProjectilePredictionState GetPredictionState() const { return PredictionState; }

	UFUNCTION(BlueprintCallable, Category = "Projectile|Prediction")
	int32 GetProjectileID() const { return ProjectileID; }

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 충돌 처리
	UFUNCTION()
	void OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	UFUNCTION()
	void OnOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
		bool bFromSweep, const FHitResult& SweepResult);

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USphereComponent> CollisionComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UProjectileMovementComponent> MovementComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> MeshComp;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile")
	float Speed = 2000.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile")
	float LifeSpan = 5.f;

	// 충돌 시 이펙트 (Cascade Particle)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effects")
	TObjectPtr<UParticleSystem> ImpactEffect;

protected:
	// 데미지 Effect Spec (발사자가 설정)
	FGameplayEffectSpecHandle DamageEffectSpec;

	// 이미 피격된 액터 (중복 피격 방지)
	UPROPERTY()
	TArray<TObjectPtr<AActor>> HitActors;

	// === 예측 시스템 (단순화) ===

	// 현재 예측 상태
	UPROPERTY(BlueprintReadOnly, Category = "Projectile|Prediction")
	EProjectilePredictionState PredictionState = EProjectilePredictionState::None;

	// 투사체 고유 ID (예측-서버 매칭용, 서버에서 리플리케이트)
	UPROPERTY(ReplicatedUsing = OnRep_ProjectileID)
	int32 ProjectileID = -1;

	UFUNCTION()
	void OnRep_ProjectileID();

	// 클라이언트에서 예측 투사체 찾기
	AGNPProjectile* FindPredictedProjectile(int32 InProjectileID) const;

	// 디버그용 이전 틱 위치 (경로 선 그리기)
	FVector LastTickLocation = FVector::ZeroVector;
};
