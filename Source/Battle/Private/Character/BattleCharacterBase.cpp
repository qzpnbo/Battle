// Fill out your copyright notice in the Description page of Project Settings.

#include "Character/BattleCharacterBase.h"
#include "Component/CombatComponent.h"
#include "Component/AttributeComponent.h"
#include "Types/BattleTypes.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

ABattleCharacterBase::ABattleCharacterBase()
{
	PrimaryActorTick.bCanEverTick = false;

	// 武器，挂载到右手骨骼插槽
	SwordMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SwordMesh"));
	SwordMesh->SetupAttachment(GetMesh(), FName(TEXT("hand_r_Socket")));

	// 武器碰撞体：默认关闭，只在攻击判定窗口（AnimNotifyState_Damage）内开启
	SwordCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("SwordCollision"));
	SwordCollision->SetupAttachment(SwordMesh);
	SwordCollision->SetGenerateOverlapEvents(true);
	SwordCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// 逻辑组件（不是 SceneComponent，不需要 SetupAttachment）
	CombatComponent = CreateDefaultSubobject<UCombatComponent>(TEXT("CombatComponent"));
	AttributeComponent = CreateDefaultSubobject<UAttributeComponent>(TEXT("AttributeComponent"));
}

void ABattleCharacterBase::PostLoad()
{
	Super::PostLoad();
	MigrateDeprecatedProperties();
}

void ABattleCharacterBase::PostInitializeComponents()
{
	// 运行时兜底：若 PostLoad 阶段未能迁移（例如蓝图加载时被重新编译），在组件初始化前再迁移一次
	MigrateDeprecatedProperties();
	Super::PostInitializeComponents();
}

void ABattleCharacterBase::MigrateDeprecatedProperties()
{
	if (MaxHealth_DEPRECATED > 0.0f && AttributeComponent)
	{
		AttributeComponent->MaxHealth = MaxHealth_DEPRECATED;
		MaxHealth_DEPRECATED = 0.0f;
	}
}

void ABattleCharacterBase::BeginPlay()
{
	Super::BeginPlay();

	// 将武器碰撞体引用传递给战斗组件，并初始化 Overlap 回调
	if (CombatComponent && SwordCollision)
	{
		CombatComponent->SwordCollisionRef = SwordCollision;
		CombatComponent->InitSwordCollision();
	}
}

float ABattleCharacterBase::GetHealth() const
{
	return AttributeComponent ? AttributeComponent->GetHealth() : 0.0f;
}

float ABattleCharacterBase::GetMaxHealth() const
{
	return AttributeComponent ? AttributeComponent->GetMaxHealth() : 0.0f;
}

float ABattleCharacterBase::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (bIsDead || !AttributeComponent)
	{
		return 0.0f;
	}

	// 解析战斗信息：自定义近战伤害事件携带韧性伤害与格挡/弹反规则，其他伤害（如环境伤害）按默认规则处理
	FIncomingHit Hit;
	Hit.Damage = DamageAmount;
	Hit.PoiseDamage = DamageAmount;
	if (DamageEvent.IsOfType(FBattleDamageEvent::ClassID))
	{
		const FBattleDamageEvent& BattleEvent = static_cast<const FBattleDamageEvent&>(DamageEvent);
		Hit.PoiseDamage = BattleEvent.PoiseDamage;
		Hit.bCanBeParried = BattleEvent.bCanBeParried;
		Hit.bCanBeBlocked = BattleEvent.bCanBeBlocked;
	}

	// 战斗判定：无敌帧 / 弹反 / 格挡 / 韧性（会就地修改 Hit.Damage）
	const EHitResponse Response = CombatComponent ? CombatComponent->ResolveIncomingHit(Hit, DamageCauser) : EHitResponse::Hit;
	if (Response == EHitResponse::Ignored || Response == EHitResponse::Dodged || Response == EHitResponse::Parried)
	{
		return 0.0f;
	}

	const float ModifiedDamage = Super::TakeDamage(Hit.Damage, DamageEvent, EventInstigator, DamageCauser);
	const float AppliedDamage = AttributeComponent->ApplyHealthDamage(ModifiedDamage);

	if (!AttributeComponent->IsAlive())
	{
		Die(DamageCauser);
	}

	return AppliedDamage;
}

void ABattleCharacterBase::Die(AActor* Killer)
{
	if (bIsDead)
	{
		return;
	}

	bIsDead = true;

	// 通知战斗组件进入死亡状态（中断动作、解锁目标、清理定时器、停止 Tick）
	if (CombatComponent)
	{
		CombatComponent->SetCombatState(ECombatState::Dead);
	}

	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// 切换到 Ragdoll 碰撞预设再开启物理，否则 Mesh 默认预设不与地面发生物理碰撞，会穿地
	GetMesh()->SetCollisionProfileName(TEXT("Ragdoll"));
	GetMesh()->SetSimulatePhysics(true);

	OnDied.Broadcast(this, Killer);
}
