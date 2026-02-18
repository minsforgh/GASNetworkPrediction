#include "GNPEnemy.h"
#include "AbilitySystemComponent.h"
#include "GNPAttributeSet.h"
#include "GNPRewindSubSystem.h"

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

void AGNPEnemy::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 서버에서만 이동 (리플리케이션으로 클라이언트 동기화)
	if (!HasAuthority() || !bPatrolEnabled)
	{
		return;
	}

	const FVector CurrentLoc = GetActorLocation();
	const FVector Direction = (CurrentPatrolTarget - CurrentLoc).GetSafeNormal();
	const FVector NewLoc = CurrentLoc + Direction * PatrolSpeed * DeltaTime;

	SetActorLocation(NewLoc);

	// 목표 지점 도달 시 반대편으로 전환
	if (FVector::Dist(NewLoc, CurrentPatrolTarget) < 30.0f)
	{
		CurrentPatrolTarget = (CurrentPatrolTarget.Equals(PatrolPointA, 1.0f)) ? PatrolPointB : PatrolPointA;
	}
}

void AGNPEnemy::BeginPlay()
{
	Super::BeginPlay();

	// 왕복 이동 초기화
	PatrolPointA = GetActorLocation();
	PatrolPointB = PatrolPointA + PatrolOffset;
	CurrentPatrolTarget = PatrolPointB;

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

