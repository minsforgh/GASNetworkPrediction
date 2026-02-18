#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "GNPAttributeSet.generated.h"

// 매크로
#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

UCLASS()
class GASNETWORKPREDICTION_API UGNPAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UGNPAttributeSet();

	// Replication 설정
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// Attribute 값 변경 직전 호출
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;

	// GameplayEffect 적용 후 호출
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

	// ============ Health ============
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Health", ReplicatedUsing = OnRep_Health)
	FGameplayAttributeData Health;
	ATTRIBUTE_ACCESSORS(UGNPAttributeSet, Health)

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Health", ReplicatedUsing = OnRep_MaxHealth)
	FGameplayAttributeData MaxHealth;
	ATTRIBUTE_ACCESSORS(UGNPAttributeSet, MaxHealth)

	// ============ Mana ============
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Mana", ReplicatedUsing = OnRep_Mana)
	FGameplayAttributeData Mana;
	ATTRIBUTE_ACCESSORS(UGNPAttributeSet, Mana) 

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Mana", ReplicatedUsing = OnRep_MaxMana)
	FGameplayAttributeData MaxMana;
	ATTRIBUTE_ACCESSORS(UGNPAttributeSet, MaxMana)

protected:
	// RepNotify 함수들
	UFUNCTION()
	virtual void OnRep_Health(const FGameplayAttributeData& OldHealth);

	UFUNCTION()
	virtual void OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth);

	UFUNCTION()
	virtual void OnRep_Mana(const FGameplayAttributeData& OldMana);

	UFUNCTION()
	virtual void OnRep_MaxMana(const FGameplayAttributeData& OldMaxMana);
};
