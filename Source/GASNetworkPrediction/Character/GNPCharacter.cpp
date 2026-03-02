#include "GNPCharacter.h"
#include "AbilitySystemComponent.h"
#include "GAS/GNPAttributeSet.h"
#include "GameplayAbilitySpec.h"
#include "GameplayTagContainer.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputActionValue.h"
#include "GameFramework/CharacterMovementComponent.h"

AGNPCharacter::AGNPCharacter()
{
	PrimaryActorTick.bCanEverTick = false;

	// ASC 생성
	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);

	// 예측 모드: Mixed = 싱글 + 멀티 모두 지원
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Full);

	// AttributeSet 생성 (ASC가 자동으로 소유권 가짐)
	AttributeSet = CreateDefaultSubobject<UGNPAttributeSet>(TEXT("AttributeSet"));
}

UAbilitySystemComponent* AGNPCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void AGNPCharacter::BeginPlay()
{
	Super::BeginPlay();

	// Input Mapping Context 등록
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			if (DefaultMappingContext)
			{
				Subsystem->AddMappingContext(DefaultMappingContext, 0);
			}
		}
	}
}

void AGNPCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// 서버
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);
		InitializeAbilities();
	}
}

void AGNPCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	// 클라이언트
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);
	}
}

void AGNPCharacter::InitializeAbilities()
{
	// 서버에서만, 한 번만 실행
	if (!HasAuthority() || bAbilitiesInitialized)
	{
		return;
	}

	if (!AbilitySystemComponent)
	{
		return;
	}

	// 초기 Ability 부여
	for (const TSubclassOf<UGameplayAbility>& AbilityClass : DefaultAbilities)
	{
		if (AbilityClass)
		{
			FGameplayAbilitySpec Spec(AbilityClass, 1, INDEX_NONE, this);
			AbilitySystemComponent->GiveAbility(Spec);
		}
	}

	// Primary Attack Ability 부여
	if (PrimaryAttackAbility)
	{
		FGameplayAbilitySpec Spec(PrimaryAttackAbility, 1, INDEX_NONE, this);
		AbilitySystemComponent->GiveAbility(Spec);
	}

	// Meteor Ability 부여
	if (MeteorAbility)
	{
		FGameplayAbilitySpec Spec(MeteorAbility, 1, INDEX_NONE, this);
		AbilitySystemComponent->GiveAbility(Spec);
	}

	// Hitscan Ability 부여
	if (HitscanAbility)
	{
		FGameplayAbilitySpec Spec(HitscanAbility, 1, INDEX_NONE, this);
		AbilitySystemComponent->GiveAbility(Spec);
	}

	// 초기 Effect 적용
	for (const TSubclassOf<UGameplayEffect>& EffectClass : DefaultEffects)
	{
		if (EffectClass)
		{
			FGameplayEffectContextHandle Context = AbilitySystemComponent->MakeEffectContext();
			Context.AddSourceObject(this);

			FGameplayEffectSpecHandle Spec = AbilitySystemComponent->MakeOutgoingSpec(EffectClass, 1, Context);
			if (Spec.IsValid())
			{
				AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
			}
		}
	}

	bAbilitiesInitialized = true;
}

void AGNPCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// 공격
		if (PrimaryAttackAction)
		{
			EnhancedInput->BindAction(PrimaryAttackAction, ETriggerEvent::Started, this, &AGNPCharacter::Attack);
			EnhancedInput->BindAction(PrimaryAttackAction, ETriggerEvent::Completed, this, &AGNPCharacter::StopAttack);
		}

		// 취소
		if (CancelAction)
		{
			EnhancedInput->BindAction(CancelAction, ETriggerEvent::Started, this, &AGNPCharacter::Cancel);
		}

		// 메테오
		if (MeteorAction)
		{
			EnhancedInput->BindAction(MeteorAction, ETriggerEvent::Started, this, &AGNPCharacter::Meteor);
		}

		// 히트스캔
		if (HitscanAction)
		{
			EnhancedInput->BindAction(HitscanAction, ETriggerEvent::Started, this, &AGNPCharacter::Hitscan);
		}

		// 이동
		if (MoveForwardAction)
		{
			EnhancedInput->BindAction(MoveForwardAction, ETriggerEvent::Triggered, this, &AGNPCharacter::MoveForward);
		}
		if (MoveRightAction)
		{
			EnhancedInput->BindAction(MoveRightAction, ETriggerEvent::Triggered, this, &AGNPCharacter::MoveRight);
		}
		if (LookAction)
		{
			EnhancedInput->BindAction(LookAction, ETriggerEvent::Triggered, this, &AGNPCharacter::Look);
		}

		// 점프 - ACharacter::Jump 직접 바인딩
		if (JumpAction)
		{
			EnhancedInput->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
			EnhancedInput->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
		}
	}
}

void AGNPCharacter::Attack()
{
	if (!AbilitySystemComponent)
	{
		return;
	}

	// 타겟팅 중이면 확정 처리 (메테오 등)
	FGameplayTag TargetingTag = FGameplayTag::RequestGameplayTag(FName("State.Targeting"));
	if (AbilitySystemComponent->HasMatchingGameplayTag(TargetingTag))
	{
		AbilitySystemComponent->InputConfirm();
		return;
	}

	// 기존 공격
	if (PrimaryAttackAbility)
	{
		AbilitySystemComponent->TryActivateAbilityByClass(PrimaryAttackAbility);
	}
}

void AGNPCharacter::StopAttack()
{
	// WhileInputActive 타입 Ability 취소용 (나중에 필요 시 구현)
}

void AGNPCharacter::Cancel()
{
	if (!AbilitySystemComponent)
	{
		return;
	}

	// 타겟팅 중이면 취소 처리
	FGameplayTag TargetingTag = FGameplayTag::RequestGameplayTag(FName("State.Targeting"));
	if (AbilitySystemComponent->HasMatchingGameplayTag(TargetingTag))
	{
		AbilitySystemComponent->InputCancel();
		return;
	}
}

void AGNPCharacter::Meteor()
{	
	UE_LOG(LogTemp, Warning, TEXT("Meteor Called"));

	if (!AbilitySystemComponent || !MeteorAbility)
	{
		return;
	}
	UE_LOG(LogTemp, Warning, TEXT("Try Activate"));
	AbilitySystemComponent->TryActivateAbilityByClass(MeteorAbility);
}

void AGNPCharacter::Hitscan()
{
	if (!AbilitySystemComponent || !HitscanAbility)
	{
		return;
	}
	AbilitySystemComponent->TryActivateAbilityByClass(HitscanAbility);
}

void AGNPCharacter::MoveForward(const FInputActionValue& Value)
{
	const float MoveValue = Value.Get<float>();
	if (Controller && MoveValue != 0.0f)
	{
		// 카메라 방향 기준 전진/후진
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);

		AddMovementInput(ForwardDirection, MoveValue);
	}
}

void AGNPCharacter::MoveRight(const FInputActionValue& Value)
{
	const float MoveValue = Value.Get<float>();
	if (Controller && MoveValue != 0.0f)
	{
		// 카메라 방향 기준 좌/우
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		AddMovementInput(RightDirection, MoveValue);
	}
}

void AGNPCharacter::Look(const FInputActionValue& Value)
{
	const FVector2D LookValue = Value.Get<FVector2D>();
	if (Controller)
	{
		AddControllerYawInput(LookValue.X);
		AddControllerPitchInput(-LookValue.Y);
	}
}

int32 AGNPCharacter::GenerateProjectileID()
{
	return ++ProjectileIDCounter;
}

float AGNPCharacter::GetHealth() const
{
	if (AttributeSet)
	{
		return AttributeSet->GetHealth();
	}
	return 0.0f;
}

float AGNPCharacter::GetMaxHealth() const
{
	if (AttributeSet)
	{
		return AttributeSet->GetMaxHealth();
	}
	return 0.0f;
}

float AGNPCharacter::GetHealthPercent() const
{
	const float MaxHP = GetMaxHealth();
	if (MaxHP > 0.0f)
	{
		return GetHealth() / MaxHP;
	}
	return 0.0f;
}
