#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "GNPEnemy.generated.h"

class UAbilitySystemComponent;
class UGNPAttributeSet;

UCLASS()
class GASNETWORKPREDICTION_API AGNPEnemy : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AGNPEnemy();

	// IAbilitySystemInterface 구현
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	// AttributeSet 접근자
	UGNPAttributeSet* GetAttributeSet() const { return AttributeSet; }

	// 체력바 바인딩용 Getter
	UFUNCTION(BlueprintCallable, Category = "GAS|Attributes")
	float GetHealth() const;

	UFUNCTION(BlueprintCallable, Category = "GAS|Attributes")
	float GetMaxHealth() const;

	// 체력 비율 (0~1, 체력바 위젯에서 사용)
	UFUNCTION(BlueprintCallable, Category = "GAS|Attributes")
	float GetHealthPercent() const;

	virtual void Tick(float DeltaTime) override;

	// 사망 처리 (AttributeSet에서 호출 → 애니메이션 재생 후 Destroy)
	void HandleDeath();

	// 사망 여부 (리플리케이션 → 클라이언트 AnimBP에서 읽음)
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "State")
	bool bIsDead = false;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// AI Controller 빙의 시 ASC 초기화
	virtual void PossessedBy(AController* NewController) override;

	// Attribute 초기화 (서버에서만)
	void InitializeAttributes();

	// AbilitySystemComponent
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	// AttributeSet (ASC가 소유, 여기선 캐시용)
	UPROPERTY()
	TObjectPtr<UGNPAttributeSet> AttributeSet;

	// 초기 적용할 GameplayEffect (스탯 초기화)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS")
	TArray<TSubclassOf<class UGameplayEffect>> DefaultEffects;

	// --- 왕복 이동 (Server Rewind 테스트용) ---

	// 왕복 이동 활성화
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Patrol")
	bool bPatrolEnabled = false;

	// 스폰 지점 기준 이동 오프셋 (예: (500,0,0) → X축 500 왕복)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Patrol", meta = (EditCondition = "bPatrolEnabled"))
	FVector PatrolOffset = FVector(500.0f, 0.0f, 0.0f);

	// 이동 속도
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Patrol", meta = (EditCondition = "bPatrolEnabled"))
	float PatrolSpeed = 200.0f;

	// GetLifetimeReplicatedProps (bIsDead 리플리케이션 등록)
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	bool bAttributesInitialized = false;

	// 왕복 이동 내부 상태
	FVector PatrolPointA;
	FVector PatrolPointB;
	FVector CurrentPatrolTarget;
	bool bMovingToB = true;

	// 사망 후 Destroy 타이머
	FTimerHandle DeathTimerHandle;
};
