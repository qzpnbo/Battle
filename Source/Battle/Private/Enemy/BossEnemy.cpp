// Fill out your copyright notice in the Description page of Project Settings.

#include "Enemy/BossEnemy.h"
#include "Component/CombatComponent.h"
#include "Component/AttributeComponent.h"
#include "UI/Widget/BossHealthWidget.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Game/BattleBGMSubsystem.h"

ABossEnemy::ABossEnemy()
{
	BossName = NSLOCTEXT("Battle", "DefaultBossName", "Boss");

	// Boss 击杀后永久死亡，尸体保留
	bRespawnAfterDeath = false;
	CorpseLifeSpan = 0.0f;

	// 高血量 + 高韧性 + 出招霸体（出招期间只承受 30% 削韧）
	AttributeComponent->MaxHealth = 500.0f;
	AttributeComponent->MaxPoise = 80.0f;
	CombatComponent->AttackingPoiseDamageScale = 0.3f;
}

void ABossEnemy::BeginPlay()
{
	Super::BeginPlay();

	// Boss 使用屏幕底部血条，隐藏头顶血条
	if (HealthWidgetComp)
	{
		HealthWidgetComp->SetVisibility(false);
	}

	if (AttributeComponent)
	{
		AttributeComponent->OnHealthChanged.AddUniqueDynamic(this, &ABossEnemy::HandleHealthChanged);
	}

	GetWorldTimerManager().SetTimer(EngageTimerHandle, this, &ABossEnemy::CheckEngage, 0.25f, true);
}

void ABossEnemy::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(EngageTimerHandle);
	GetWorldTimerManager().ClearTimer(RemoveBarTimerHandle);
	RemoveHealthBar();

	Super::EndPlay(EndPlayReason);
}

void ABossEnemy::CheckEngage()
{
	if (bBossFightActive || IsDead())
	{
		return;
	}

	const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (PlayerPawn && FVector::DistSquared(PlayerPawn->GetActorLocation(), GetActorLocation()) <= FMath::Square(EngageRadius))
	{
		StartBossFight();
	}
}

void ABossEnemy::StartBossFight()
{
	if (bBossFightActive || IsDead())
	{
		return;
	}

	bBossFightActive = true;
	GetWorldTimerManager().ClearTimer(EngageTimerHandle);

	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC)
	{
		return;
	}

	TSubclassOf<UBossHealthWidget> WidgetClass = BossHealthWidgetClass ? BossHealthWidgetClass : TSubclassOf<UBossHealthWidget>(UBossHealthWidget::StaticClass());
	BossHealthWidget = CreateWidget<UBossHealthWidget>(PC, WidgetClass);
	if (BossHealthWidget)
	{
		BossHealthWidget->InitWithBoss(this);
		BossHealthWidget->AddToViewport(5);
	}

	GetWorld()->GetSubsystem<UBattleBGMSubsystem>()->PlayBossBGM();
}

void ABossEnemy::HandleHealthChanged(float CurrentHealth, float MaxHealth)
{
	// 受到攻击也视为开战（例如玩家远距离偷袭）
	if (!bBossFightActive && CurrentHealth < MaxHealth)
	{
		StartBossFight();
	}

	if (CurrentPhase == 1 && CurrentHealth > 0.0f && MaxHealth > 0.0f && CurrentHealth / MaxHealth <= Phase2HealthThreshold)
	{
		EnterPhase(2);
	}
}

void ABossEnemy::EnterPhase(int32 NewPhase)
{
	if (NewPhase == CurrentPhase)
	{
		return;
	}

	CurrentPhase = NewPhase;

	if (NewPhase >= 2)
	{
		if (CombatComponent)
		{
			CombatComponent->ActionPlayRate = Phase2ActionPlayRate;

			// 转阶段动作：强制打断当前动作并全程无敌（HandleHealthChanged 在受击结算中触发，
			// 这里打断会覆盖本次受击硬直，表现为 Boss 硬吃一刀后直接咆哮）
			if (Phase2TransitionMontage)
			{
				CombatComponent->PlayForcedActionMontage(Phase2TransitionMontage, true);
			}
		}

		if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
		{
			MoveComp->MaxWalkSpeed *= Phase2MoveSpeedMultiplier;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("Boss %s entered phase %d"), *GetName(), NewPhase);
	OnPhaseChanged.Broadcast(NewPhase);
}

void ABossEnemy::Die(AActor* Killer)
{
	Super::Die(Killer);

	GetWorldTimerManager().ClearTimer(EngageTimerHandle);

	// 血条停留一会儿再移除
	if (BossHealthWidget)
	{
		GetWorldTimerManager().SetTimer(RemoveBarTimerHandle, this, &ABossEnemy::RemoveHealthBar, FMath::Max(HealthBarLingerTime, 0.01f), false);
	}

	// 切回探索音乐
	GetWorld()->GetSubsystem<UBattleBGMSubsystem>()->PlayExplorationBGM();
}

void ABossEnemy::RemoveHealthBar()
{
	if (BossHealthWidget)
	{
		BossHealthWidget->RemoveFromParent();
		BossHealthWidget = nullptr;
	}
}
