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
	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	MaxHealth = 100.0f;
	CurrentHealth = MaxHealth;

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

	CurrentHealth = MaxHealth;

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

// Called every frame
void AEnemy::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

// Called to bind functionality to input
void AEnemy::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

float AEnemy::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	float ActualDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

	// 扣血
	CurrentHealth = FMath::Clamp(CurrentHealth - ActualDamage, 0.0f, MaxHealth);

	// 广播血量变化（更新头顶血条）
	OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);

	// 播放受击动画
	if (HitReactMontage)
	{
		PlayAnimMontage(HitReactMontage, 1.0f);
	}

	// 死亡判定
	if (CurrentHealth <= 0.0f)
	{
		Die();
	}

	return ActualDamage;
}

void AEnemy::Die()
{
	bIsDead = true;

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
