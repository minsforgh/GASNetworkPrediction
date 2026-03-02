// Fill out your copyright notice in the Description page of Project Settings.

#include "GNPCharacterAnimInstance.h"
#include "GNPCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetMathLibrary.h"

void UGNPCharacterAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	AGNPCharacter* Character = Cast<AGNPCharacter>(GetOwningActor());
	if (!Character) return;

	UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	if (!Movement) return;

	// XY 평면 속도 (수직 이동 제외)
	Speed = Character->GetVelocity().Size2D();
	bIsInAir = Movement->IsFalling();

	// 이동 방향: 속도 벡터 방향 - 캐릭터 전방 방향 = 상대 각도
	// 정지 중에는 0 유지 (블렌드 스페이스 중앙 = 전방)
	if (Speed > 1.f)
	{
		FRotator MovementRot = Character->GetVelocity().Rotation();
		FRotator ActorRot = Character->GetActorRotation();
		MovementDirection = UKismetMathLibrary::NormalizedDeltaRotator(MovementRot, ActorRot).Yaw;
	}
	else
	{
		MovementDirection = 0.f;
	}
}
