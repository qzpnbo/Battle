// Fill out your copyright notice in the Description page of Project Settings.

#include "Game/BattleRespawnSubsystem.h"
#include "Game/Checkpoint.h"
#include "Character/BattleCharacter.h"
#include "Enemy/Enemy.h"
#include "UI/Widget/DeathScreenWidget.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include <Game/BattleBGMSubsystem.h>

bool UBattleRespawnSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	// 只在游戏/PIE 世界中创建（编辑器预览世界等不需要）
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UBattleRespawnSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RespawnTimerHandle);
	}
	RemoveDeathScreen();
	EnemyRecords.Reset();

	Super::Deinitialize();
}

// ============================================================================
// 注册
// ============================================================================

void UBattleRespawnSubsystem::RegisterEnemy(AEnemy* Enemy)
{
	// 重置流程中生成的敌人由 ResetEnemies 自己记录
	if (!Enemy || bIsResettingEnemies)
	{
		return;
	}

	for (const FEnemySpawnRecord& Record : EnemyRecords)
	{
		if (Record.Instance.Get() == Enemy)
		{
			return;
		}
	}

	FEnemySpawnRecord& Record = EnemyRecords.AddDefaulted_GetRef();
	Record.EnemyClass = Enemy->GetClass();
	Record.SpawnTransform = Enemy->GetActorTransform();
	Record.Instance = Enemy;
	Record.bRespawnAfterDeath = Enemy->bRespawnAfterDeath;

	Enemy->OnDied.AddUniqueDynamic(this, &UBattleRespawnSubsystem::HandleEnemyDied);
}

void UBattleRespawnSubsystem::RegisterPlayer(ABattleCharacter* Player)
{
	if (Player)
	{
		Player->OnDied.AddUniqueDynamic(this, &UBattleRespawnSubsystem::HandlePlayerDied);
	}
}

void UBattleRespawnSubsystem::SetActiveCheckpoint(ACheckpoint* Checkpoint)
{
	if (!Checkpoint || ActiveCheckpoint.Get() == Checkpoint)
	{
		return;
	}

	if (ACheckpoint* Previous = ActiveCheckpoint.Get())
	{
		Previous->SetActivated(false);
	}

	ActiveCheckpoint = Checkpoint;
	Checkpoint->SetActivated(true);

	UE_LOG(LogTemp, Log, TEXT("Checkpoint activated: %s"), *Checkpoint->GetName());
}

// ============================================================================
// 死亡回调
// ============================================================================

void UBattleRespawnSubsystem::HandleEnemyDied(ABattleCharacterBase* DeadCharacter, AActor* Killer)
{
	for (FEnemySpawnRecord& Record : EnemyRecords)
	{
		if (Record.Instance.Get() == DeadCharacter)
		{
			// Boss 等不复活的敌人：记为永久击杀
			Record.bPermanentlyDefeated = !Record.bRespawnAfterDeath;
			return;
		}
	}
}

void UBattleRespawnSubsystem::HandlePlayerDied(ABattleCharacterBase* DeadCharacter, AActor* Killer)
{
	ABattleCharacter* Player = Cast<ABattleCharacter>(DeadCharacter);
	APlayerController* PC = Player ? Cast<APlayerController>(Player->GetController()) : nullptr;
	UWorld* World = GetWorld();
	if (!PC || !World)
	{
		return;
	}

	PendingRespawnController = PC;

	// 显示死亡界面
	TSubclassOf<UUserWidget> WidgetClass = Player->DeathScreenWidgetClass ? Player->DeathScreenWidgetClass : TSubclassOf<UUserWidget>(UDeathScreenWidget::StaticClass());
	RemoveDeathScreen();
	DeathScreen = CreateWidget<UUserWidget>(PC, WidgetClass);
	if (DeathScreen)
	{
		DeathScreen->AddToViewport(100);
	}

	// 重生倒计时
	World->GetTimerManager().SetTimer(RespawnTimerHandle, this, &UBattleRespawnSubsystem::RespawnPlayer, Player->RespawnDelay, false);
	
	// 恢复探索音乐
	World->GetSubsystem<UBattleBGMSubsystem>()->PlayExplorationBGM();
}

// ============================================================================
// 重生
// ============================================================================

void UBattleRespawnSubsystem::RespawnPlayer()
{
	RemoveDeathScreen();

	UWorld* World = GetWorld();
	APlayerController* PC = PendingRespawnController.Get();
	PendingRespawnController.Reset();
	if (!World || !PC)
	{
		return;
	}

	AGameModeBase* GameMode = World->GetAuthGameMode();
	if (!GameMode)
	{
		return;
	}

	// 先重置敌人，再生成玩家，避免玩家一出生就被旧的敌人追击
	ResetEnemies();

	// 重生位置：激活的检查点 > PlayerStart
	FTransform SpawnTransform = FTransform::Identity;
	if (const ACheckpoint* Checkpoint = ActiveCheckpoint.Get())
	{
		SpawnTransform = Checkpoint->GetRespawnTransform();
	}
	else if (const AActor* PlayerStart = GameMode->FindPlayerStart(PC))
	{
		SpawnTransform = FTransform(FRotator(0.0f, PlayerStart->GetActorRotation().Yaw, 0.0f), PlayerStart->GetActorLocation());
	}

	// 销毁尸体并重新生成 Pawn（RestartPlayerAtTransform 要求控制器当前没有 Pawn）
	if (APawn* OldPawn = PC->GetPawn())
	{
		PC->UnPossess();
		OldPawn->Destroy();
	}

	GameMode->RestartPlayerAtTransform(PC, SpawnTransform);
}

void UBattleRespawnSubsystem::ResetEnemies()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	bIsResettingEnemies = true;

	for (FEnemySpawnRecord& Record : EnemyRecords)
	{
		if (Record.bPermanentlyDefeated || !Record.EnemyClass)
		{
			continue;
		}

		// 销毁现有实例（存活的或尚未消失的尸体）
		if (AEnemy* Existing = Record.Instance.Get())
		{
			Existing->Destroy();
		}

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

		AEnemy* NewEnemy = World->SpawnActor<AEnemy>(Record.EnemyClass, Record.SpawnTransform, Params);
		Record.Instance = NewEnemy;
		if (!NewEnemy)
		{
			continue;
		}

		// 兜底：蓝图若把 AutoPossessAI 改成了 PlacedInWorld，生成后不会自动创建 AI 控制器
		if (!NewEnemy->GetController())
		{
			NewEnemy->SpawnDefaultController();
		}

		NewEnemy->OnDied.AddUniqueDynamic(this, &UBattleRespawnSubsystem::HandleEnemyDied);
	}

	bIsResettingEnemies = false;
}

void UBattleRespawnSubsystem::RemoveDeathScreen()
{
	if (DeathScreen)
	{
		DeathScreen->RemoveFromParent();
		DeathScreen = nullptr;
	}
}
