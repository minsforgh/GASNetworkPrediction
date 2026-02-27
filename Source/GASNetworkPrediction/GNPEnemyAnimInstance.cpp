// Fill out your copyright notice in the Description page of Project Settings.

#include "GNPEnemyAnimInstance.h"
#include "GNPEnemy.h"

void UGNPEnemyAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	AGNPEnemy* Enemy = Cast<AGNPEnemy>(GetOwningActor());
	if (!Enemy) return;

	Speed = Enemy->GetVelocity().Size2D();
	bIsDead = Enemy->bIsDead;
}
