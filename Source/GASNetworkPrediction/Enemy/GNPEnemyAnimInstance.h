// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "GNPEnemyAnimInstance.generated.h"

UCLASS()
class GASNETWORKPREDICTION_API UGNPEnemyAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	// 이동 속도 (Idle <-> Walk 전환)
	UPROPERTY(BlueprintReadOnly, Category = "Animation")
	float Speed = 0.f;

	// 사망 여부 (Death 스테이트 전환)
	UPROPERTY(BlueprintReadOnly, Category = "Animation")
	bool bIsDead = false;
};
