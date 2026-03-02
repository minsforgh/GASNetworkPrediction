#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "GNPCharacter.generated.h"

class UAbilitySystemComponent;
class UGNPAttributeSet;
class UInputMappingContext;
class UInputAction;

UCLASS()
class GASNETWORKPREDICTION_API AGNPCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AGNPCharacter();

	// IAbilitySystemInterface 구현
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	// AttributeSet 접근자
	UGNPAttributeSet* GetAttributeSet() const { return AttributeSet; }

	// 체력바 바인딩용 Getter
	UFUNCTION(BlueprintCallable, Category = "GAS|Attributes")
	float GetHealth() const;

	UFUNCTION(BlueprintCallable, Category = "GAS|Attributes")
	float GetMaxHealth() const;

	UFUNCTION(BlueprintCallable, Category = "GAS|Attributes")
	float GetHealthPercent() const;

protected:
	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	// 서버에서 Ability 부여
	void InitializeAbilities();

	// 입력 핸들러
	void Attack();
	void StopAttack();
	void Meteor();
	void Hitscan();
	void MoveForward(const struct FInputActionValue& Value);
	void MoveRight(const struct FInputActionValue& Value);
	void Look(const struct FInputActionValue& Value);
	void Cancel();

	// ASC 초기화 ('서버'에서 컨트롤러가 빙의할 때/'클라이언트'에서 PlayterState 복제되었을 때)
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;

	// AbilitySystemComponent
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	// AttributeSet (ASC가 소유, 여기선 캐시용)
	UPROPERTY()
	TObjectPtr<UGNPAttributeSet> AttributeSet;

	// 기본으로 부여할 Ability 목록
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS")
	TArray<TSubclassOf<class UGameplayAbility>> DefaultAbilities;

	// 초기 적용할 GameplayEffect (스탯 초기화 등)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS")
	TArray<TSubclassOf<class UGameplayEffect>> DefaultEffects;

	// ============ Enhanced Input ============
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> PrimaryAttackAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> MoveForwardAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> MoveRightAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> JumpAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> CancelAction;

	// Primary Attack에 사용할 Ability
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS|Abilities")
	TSubclassOf<class UGameplayAbility> PrimaryAttackAbility;

	// ============ Meteor 스킬 ============
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> MeteorAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS|Abilities")
	TSubclassOf<class UGameplayAbility> MeteorAbility;

	// ============ Hitscan 스킬 ============
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> HitscanAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS|Abilities")
	TSubclassOf<class UGameplayAbility> HitscanAbility;

public:
	
	// 투사체 ID 생성 (클라이언트에서 호출, 예측-서버 매칭용)
	UFUNCTION(BlueprintCallable, Category = "GAS|Projectile")
	int32 GenerateProjectileID();

private:
	bool bAbilitiesInitialized = false;

	// 투사체 ID 카운터 (클라이언트별로 고유)
	int32 ProjectileIDCounter = 0;
};
