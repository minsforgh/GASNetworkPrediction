#include "GNPEnemy.h"
#include "AbilitySystemComponent.h"
#include "GAS/GNPAttributeSet.h"
#include "Network/GNPRewindSubSystem.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"

void AGNPEnemy::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AGNPEnemy, bIsDead);
}

AGNPEnemy::AGNPEnemy()
{
	PrimaryActorTick.bCanEverTick = true;

	// ASC 생성
	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);

	// AI는 Minimal로 충분 (클라이언트 예측 필요 없음)
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

	// AttributeSet 생성 (ASC가 자동으로 소유권 가짐)
	AttributeSet = CreateDefaultSubobject<UGNPAttributeSet>(TEXT("AttributeSet"));

	// 이동 방향으로 자동 회전 (왕복 이동 시 방향 전환)
	// bUseControllerRotationYaw 기본값 true가 bOrientRotationToMovement를 무시하므로 반드시 false 필요
	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;
}

UAbilitySystemComponent* AGNPEnemy::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

float AGNPEnemy::GetHealth() const
{
	if (AttributeSet)
	{
		return AttributeSet->GetHealth();
	}
	return 0.0f;
}

float AGNPEnemy::GetMaxHealth() const
{
	if (AttributeSet)
	{
		return AttributeSet->GetMaxHealth();
	}
	return 0.0f;
}

float AGNPEnemy::GetHealthPercent() const
{
	const float MaxHP = GetMaxHealth();
	if (MaxHP > 0.0f)
	{
		return GetHealth() / MaxHP;
	}
	return 0.0f;
}

void AGNPEnemy::HandleDeath()
{
	// 중복 호출 방지
	if (bIsDead) return;

	bIsDead = true;

	// 충돌 비활성화 (히트스캔 트레이스 제외)
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// 이동 중지
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();

	// 사망 애니메이션 재생 시간 후 Destroy 
	GetWorldTimerManager().SetTimer(DeathTimerHandle, [this]()
	{
		Destroy();
	}, 2.0f, false);
}

void AGNPEnemy::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 서버에서만 이동 (리플리케이션으로 클라이언트 동기화)
	if (!HasAuthority() || !bPatrolEnabled || bIsDead)
	{
		return;
	}

	const FVector CurrentLoc = GetActorLocation();

	// 목표 지점 도달 판정 (XY 평면 기준, Z 오차 무시)
	if (FVector::Dist2D(CurrentLoc, CurrentPatrolTarget) < 50.0f)
	{
		bMovingToB = !bMovingToB;
		CurrentPatrolTarget = bMovingToB ? PatrolPointB : PatrolPointA;
	}

	// XY 평면 이동 방향 계산 후 이동
	const FVector Direction = (CurrentPatrolTarget - CurrentLoc).GetSafeNormal2D();
	AddMovementInput(Direction, 1.0f);
}

void AGNPEnemy::BeginPlay()
{
	Super::BeginPlay();

	// 왕복 이동 초기화
	PatrolPointA = GetActorLocation();
	PatrolPointB = PatrolPointA + PatrolOffset;
	CurrentPatrolTarget = PatrolPointB;
	if (bPatrolEnabled)
	{
		GetCharacterMovement()->MaxWalkSpeed = PatrolSpeed;
	}

	// 클라이언트에서 ASC ActorInfo 초기화 (체력바 바인딩 등)
	if (!HasAuthority() && AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);
	}

	// Server Rewind 시스템에 등록 (서버에서만)
	if (HasAuthority())
	{
		if (UGNPRewindSubSystem* RewindSys = GetWorld()->GetSubsystem<UGNPRewindSubSystem>())
		{
			RewindSys->RegisterActor(this);
		}
	}
}

void AGNPEnemy::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Server Rewind 시스템에서 해제
	if (HasAuthority())
	{
		if (UGNPRewindSubSystem* RewindSys = GetWorld()->GetSubsystem<UGNPRewindSubSystem>())
		{
			RewindSys->UnregisterActor(this);
		}
	}

	Super::EndPlay(EndPlayReason);
}

void AGNPEnemy::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// 서버에서 ASC 초기화
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);
		InitializeAttributes();
	}
}

void AGNPEnemy::InitializeAttributes()
{
	// 서버에서만, 한 번만 실행
	if (!HasAuthority() || bAttributesInitialized)
	{
		return;
	}

	if (!AbilitySystemComponent)
	{
		return;
	}

	// 초기 Effect 적용 (체력 초기화 등)
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

	bAttributesInitialized = true;
}

