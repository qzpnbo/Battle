// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Enemy/Enemy.h"
#include "BossEnemy.generated.h"

class UBossHealthWidget;
class UAnimMontage;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBossPhaseChanged, int32, NewPhase);

/**
 * Boss 敌人
 *  - 玩家进入交战半径（或 Boss 受到攻击）时开战，屏幕底部显示 Boss 血条
 *  - 血量低于阈值进入二阶段：播放转阶段动作（全程无敌），之后出招与移动加速
 *  - 击杀后永久死亡（玩家重生不会复活 Boss）
 *  - 招式选择见 UBTTask_BossAttack（按距离 / 阶段 / 权重随机）
 */
UCLASS()
class BATTLE_API ABossEnemy : public AEnemy
{
	GENERATED_BODY()

public:
	ABossEnemy();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss")
	FText BossName;

	// 玩家进入该半径时开战（显示 Boss 血条）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss", meta = (ClampMin = "0.0"))
	float EngageRadius = 1800.0f;

	// Boss 血条类（为空时使用 C++ 默认布局的 UBossHealthWidget）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|UI")
	TSubclassOf<UBossHealthWidget> BossHealthWidgetClass;

	// Boss 死亡后血条保留时间（秒）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|UI", meta = (ClampMin = "0.0"))
	float HealthBarLingerTime = 2.5f;

	// 血量百分比低于该值进入二阶段
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Phase", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Phase2HealthThreshold = 0.5f;

	// 转阶段动作（如咆哮），播放期间无敌；为空则直接进入二阶段
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Phase")
	UAnimMontage* Phase2TransitionMontage = nullptr;

	// 二阶段出招速率倍率
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Phase", meta = (ClampMin = "0.1", ClampMax = "3.0"))
	float Phase2ActionPlayRate = 1.2f;

	// 二阶段移动速度倍率
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Phase", meta = (ClampMin = "0.1", ClampMax = "3.0"))
	float Phase2MoveSpeedMultiplier = 1.25f;

	UPROPERTY(BlueprintAssignable, Category = "Boss|Phase")
	FOnBossPhaseChanged OnPhaseChanged;

	UFUNCTION(BlueprintPure, Category = "Boss")
	int32 GetCurrentPhase() const { return CurrentPhase; }

	UFUNCTION(BlueprintPure, Category = "Boss")
	bool IsBossFightActive() const { return bBossFightActive; }

	// 开战：创建并显示 Boss 血条
	UFUNCTION(BlueprintCallable, Category = "Boss")
	void StartBossFight();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Die(AActor* Killer) override;

private:
	UFUNCTION()
	void HandleHealthChanged(float CurrentHealth, float MaxHealth);

	// 定时检查玩家是否进入交战半径
	void CheckEngage();

	void EnterPhase(int32 NewPhase);

	void RemoveHealthBar();

	UPROPERTY(Transient)
	TObjectPtr<UBossHealthWidget> BossHealthWidget;

	FTimerHandle EngageTimerHandle;
	FTimerHandle RemoveBarTimerHandle;

	int32 CurrentPhase = 1;
	bool bBossFightActive = false;
};
