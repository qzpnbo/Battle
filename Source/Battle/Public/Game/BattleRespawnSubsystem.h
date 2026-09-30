// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "BattleRespawnSubsystem.generated.h"

class AEnemy;
class ABattleCharacter;
class ABattleCharacterBase;
class ACheckpoint;
class UUserWidget;

// 敌人出生记录（USTRUCT + UPROPERTY：保证敌人类在所有实例销毁后仍被 GC 引用）
USTRUCT()
struct FEnemySpawnRecord
{
	GENERATED_BODY()

	UPROPERTY()
	TSubclassOf<AEnemy> EnemyClass;

	UPROPERTY()
	FTransform SpawnTransform;

	UPROPERTY()
	TWeakObjectPtr<AEnemy> Instance;

	UPROPERTY()
	bool bRespawnAfterDeath = true;

	UPROPERTY()
	bool bPermanentlyDefeated = false;
};

/**
 * 死亡与重生系统（World Subsystem，无需任何配置，随关卡自动创建）
 *
 * 流程：玩家死亡 → 显示 "YOU DIED" → 延迟 → 重置敌人 → 在最近激活的检查点（没有则 PlayerStart）重生
 *
 * 敌人重置：记录每个敌人首次 BeginPlay 时的类与出生点，重置时销毁现有实例并重新生成，
 *          行为树/血量/位置全部回到初始状态；bRespawnAfterDeath = false 的敌人（Boss）被击杀后不再复活。
 */
UCLASS()
class BATTLE_API UBattleRespawnSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// 敌人 BeginPlay 时注册（记录出生信息）
	void RegisterEnemy(AEnemy* Enemy);

	// 玩家 BeginPlay 时注册（监听死亡）
	void RegisterPlayer(ABattleCharacter* Player);

	UFUNCTION(BlueprintCallable, Category = "Respawn")
	void SetActiveCheckpoint(ACheckpoint* Checkpoint);

	UFUNCTION(BlueprintPure, Category = "Respawn")
	ACheckpoint* GetActiveCheckpoint() const { return ActiveCheckpoint.Get(); }

	// 把所有敌人重置到出生状态（已被永久击杀的除外）
	UFUNCTION(BlueprintCallable, Category = "Respawn")
	void ResetEnemies();

	virtual void Deinitialize() override;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	UFUNCTION()
	void HandleEnemyDied(ABattleCharacterBase* DeadCharacter, AActor* Killer);

	UFUNCTION()
	void HandlePlayerDied(ABattleCharacterBase* DeadCharacter, AActor* Killer);

	void RespawnPlayer();
	void RemoveDeathScreen();

	UPROPERTY(Transient)
	TArray<FEnemySpawnRecord> EnemyRecords;

	TWeakObjectPtr<ACheckpoint> ActiveCheckpoint;
	TWeakObjectPtr<APlayerController> PendingRespawnController;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> DeathScreen;

	FTimerHandle RespawnTimerHandle;

	// 重置敌人期间生成的敌人不重复注册
	bool bIsResettingEnemies = false;
};
