// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "GNPCharacterAnimInstance.generated.h"

UCLASS()
class GASNETWORKPREDICTION_API UGNPCharacterAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	// 이동 속도 (Idle <-> Run 전환)
	UPROPERTY(BlueprintReadOnly, Category = "Animation")
	float Speed = 0.f;

	// 공중 여부 (Jump 스테이트 전환)
	UPROPERTY(BlueprintReadOnly, Category = "Animation")
	bool bIsInAir = false;

	// 이동 방향 (-180~180, Run Direction 2D 블렌드 스페이스 X축)
	UPROPERTY(BlueprintReadOnly, Category = "Animation")
	float MovementDirection = 0.f;
};
