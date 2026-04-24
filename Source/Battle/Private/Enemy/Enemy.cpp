// Fill out your copyright notice in the Description page of Project Settings.

#include "Enemy/Enemy.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/WidgetComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Component/CombatComponent.h"
#include "UI/Widget/EnemyHealthWidget.h"

// Sets default values
AEnemy::AEnemy()
{
	// 敌人不需要每帧 Tick，关闭以提升性能
	PrimaryActorTick.bCanEverTick = false;

	// 创建头顶血条 Widget 组件
	HealthWidgetComp = CreateDefaultSubobject<UWidgetComponent>(TEXT("HealthWidget"));
	HealthWidgetComp->SetupAttachment(RootComponent);
	HealthWidgetComp->SetWidgetSpace(EWidgetSpace::Screen); // 屏幕空间，始终面向摄像机
	HealthWidgetComp->SetDrawAtDesiredSize(true);
	HealthWidgetComp->SetRelativeLocation(FVector(0.0f, 0.0f, 100.0f)); // 头顶偏移

	// 创建武器组件，挂载到 Mesh
	SwordMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SwordMesh"));
	SwordMesh->SetupAttachment(GetMesh(), FName(TEXT("hand_r_Socket")));

	// 创建武器碰撞体，挂载到 SwordMesh 下（形状/大小可在蓝图中可视化调整）
	SwordCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("SwordCollision"));
	SwordCollision->SetupAttachment(SwordMesh);
	SwordCollision->SetGenerateOverlapEvents(true);

	// 创建战斗组件（不是 SceneComponent，不需要 SetupAttachment）
	CombatComponent = CreateDefaultSubobject<UCombatComponent>(TEXT("CombatComponent"));
}

// Called when the game starts or when spawned
void AEnemy::BeginPlay()
{
	Super::BeginPlay();

	Health = MaxHealth;

	// 将武器碰撞体引用传递给战斗组件，并初始化 Overlap 回调
	if (CombatComponent && SwordCollision)
	{
		CombatComponent->SwordCollisionRef = SwordCollision;
		CombatComponent->InitSwordCollision();
	}

	// 无论 Widget 是通过代码 InitWidget 创建还是蓝图预设创建，都尝试绑定
	UEnemyHealthWidget* HealthWidget = Cast<UEnemyHealthWidget>(HealthWidgetComp->GetWidget());
	if (HealthWidget)
	{
		HealthWidget->InitWithOwner(this);
	}
}

// 敌人不需要每帧 Tick，已关闭
void AEnemy::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

float AEnemy::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	// 已死亡则不再受伤
	if (bIsDead)
	{
		return 0.0f;
	}

	// 通过战斗组件处理受击（检查无敌帧、中断当前动作、播放受击动画等）
	// 传入 DamageCauser 用于计算受击方向，选择对应的方向性受击蒙太奇
	if (CombatComponent && !CombatComponent->HandleTakeDamage(DamageCauser))
	{
		return 0.0f;
	}

	float ActualDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

	// 扣血
	Health = FMath::Clamp(Health - ActualDamage, 0.0f, MaxHealth);

	// 广播血量变化（更新头顶血条）
	OnHealthChanged.Broadcast(Health, MaxHealth);

	// 死亡判定
	if (Health <= 0.0f)
	{
		Die();
		return ActualDamage;
	}

	return ActualDamage;
}

void AEnemy::Die()
{
	if (bIsDead)
	{
		return;
	}

	bIsDead = true;

	// 通知战斗组件进入死亡状态（清理锁定目标、定时器、停止Tick等）
	if (CombatComponent)
	{
		CombatComponent->SetCombatState(ECombatState::Dead);
	}

	// 禁用移动
	GetCharacterMovement()->DisableMovement();

	// 禁用碰撞
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// 开启布娃娃物理
	GetMesh()->SetSimulatePhysics(true);

	// 隐藏头顶血条
	if (HealthWidgetComp)
	{
		HealthWidgetComp->SetVisibility(false);
	}
}
