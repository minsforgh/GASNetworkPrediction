#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbilityTargetActor.h"
#include "GNPMeteorFunctionLibrary.h"
#include "MeteorTargetActor.generated.h"

/**
 * 
 */
UCLASS()
class GASNETWORKPREDICTION_API AMeteorTargetActor : public AGameplayAbilityTargetActor
{
	GENERATED_BODY()

public:
    AMeteorTargetActor();

    virtual void Tick(float DeltaSeconds) override;

    // Ȯ�� �� (������ ���� �� ����)
    virtual void ConfirmTargetingAndContinue() override;

    // Ȯ��/��� �߰� �ٸ�
    virtual void ConfirmTargeting() override;
    virtual void CancelTargeting() override;

protected:

    // Ability Task Ȱ��ȭ �� 
    virtual void StartTargeting(UGameplayAbility* Ability) override;

    UPROPERTY()
    TObjectPtr<AGameplayAbilityWorldReticle> ReticleActor;

    UPROPERTY()
    TObjectPtr<UMaterialInstanceDynamic> ReticleDynamicMaterial;

    // ���� ��Ÿ�
    UPROPERTY(EditAnywhere, Category = "Targeting")
    float TraceRange = 10000.f;

    // ���� ��Ÿ�
    UPROPERTY(EditAnywhere, Category = "Targeting")
    float MaxRange = 1500.0f;

    // ��Ÿ� ��/�� ����
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting")
    FLinearColor ValidColor = FLinearColor::Blue;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting")
    FLinearColor InvalidColor = FLinearColor::Red;

    void SpawnReticleActor(UGameplayAbility* Ability);
	
};
