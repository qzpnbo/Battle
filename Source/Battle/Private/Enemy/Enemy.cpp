// Fill out your copyright notice in the Description page of Project Settings.

#include "Enemy/Enemy.h"
#include "Component/CombatComponent.h"
#include "Component/AttributeComponent.h"
#include "Game/BattleRespawnSubsystem.h"
#include "Types/BattleTypes.h"
#include "Components/WidgetComponent.h"
#include "UI/Widget/EnemyHealthWidget.h"
#include "AIController.h"
#include "BrainComponent.h"

// Sets default values
AEnemy::AEnemy()
{
	// 敌人不需要每帧 Tick，关闭以提升性能
	PrimaryActorTick.bCanEverTick = false;

	// 重生系统重置敌人时通过 SpawnActor 生成，需要自动创建 AI 控制器
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	// 创建头顶血条 Widget 组件
	HealthWidgetComp = CreateDefaultSubobject<UWidgetComponent>(TEXT("HealthWidget"));
	HealthWidgetComp->SetupAttachment(RootComponent);
	HealthWidgetComp->SetWidgetSpace(EWidgetSpace::Screen); // 屏幕空间，始终面向摄像机
	HealthWidgetComp->SetDrawAtDesiredSize(true);
	HealthWidgetComp->SetRelativeLocation(FVector(0.0f, 0.0f, 100.0f)); // 头顶偏移

	CombatComponent->Team = ECombatTeam::Enemy;

	// AI 的出招节奏由行为树控制，不使用耐力
	AttributeComponent->bUseStamina = false;
}

// Called when the game starts or when spawned
void AEnemy::BeginPlay()
{
	Super::BeginPlay();

	// 无论 Widget 是通过代码 InitWidget 创建还是蓝图预设创建，都尝试绑定
	UEnemyHealthWidget* HealthWidget = Cast<UEnemyHealthWidget>(HealthWidgetComp->GetWidget());
	if (HealthWidget)
	{
		HealthWidget->InitWithOwner(this);
	}

	// 注册到重生系统：记录出生点，玩家死亡后重置
	if (UBattleRespawnSubsystem* RespawnSubsystem = GetWorld()->GetSubsystem<UBattleRespawnSubsystem>())
	{
		RespawnSubsystem->RegisterEnemy(this);
	}
}

void AEnemy::Die(AActor* Killer)
{
	if (bIsDead)
	{
		return;
	}

	// 停止 AI 逻辑（行为树），否则死亡后行为树仍会继续执行追击/攻击任务
	if (AAIController* AIController = Cast<AAIController>(GetController()))
	{
		if (UBrainComponent* Brain = AIController->GetBrainComponent())
		{
			Brain->StopLogic(TEXT("Dead"));
		}
		AIController->StopMovement();
	}

	Super::Die(Killer);

	// 隐藏头顶血条
	if (HealthWidgetComp)
	{
		HealthWidgetComp->SetVisibility(false);
	}

	// 一段时间后自动销毁尸体（<= 0 表示永久保留）
	if (CorpseLifeSpan > 0.0f)
	{
		SetLifeSpan(CorpseLifeSpan);
	}
}
