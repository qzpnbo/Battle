// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/Tasks/BTTask_BlackboardBase.h"
#include "BTTask_BossAttack.generated.h"

class UAnimMontage;

// Boss 招式配置
USTRUCT(BlueprintType)
struct FBossAttackOption
{
	GENERATED_BODY()

	// 招式蒙太奇（需包含 Damage Trace 通知窗口）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack")
	TObjectPtr<UAnimMontage> Montage = nullptr;

	// 可用距离区间（与目标的水平距离）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack", meta = (ClampMin = "0.0"))
	float MinDistance = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack", meta = (ClampMin = "0.0"))
	float MaxDistance = 300.0f;

	// 选择权重（越大越常用）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack", meta = (ClampMin = "0.0"))
	float Weight = 1.0f;

	// 可用阶段区间（例如二阶段专属大招：MinPhase = 2）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack", meta = (ClampMin = "1"))
	int32 MinPhase = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack", meta = (ClampMin = "1"))
	int32 MaxPhase = 99;

	// 伤害倍率（× 武器基础伤害）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack", meta = (ClampMin = "0.0"))
	float DamageMultiplier = 1.0f;

	// 削韧值
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack", meta = (ClampMin = "0.0"))
	float PoiseDamage = 30.0f;

	// 能否被玩家弹反（大招通常不可弹反，只能翻滚躲避）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack")
	bool bCanBeParried = true;
};

/**
 * Boss 攻击任务：按"距离 + 阶段"过滤招式，再按权重随机选择
 *  - 刚用过的招式权重减半，避免同一招连续重复
 *  - 出招前若干秒持续转向目标（类魂 Boss 的"追踪"），之后锁定方向给玩家留出闪避空间
 *  - 任务持续到 Boss 回到 Idle（包括被打出硬直、被弹反的情况），期间行为树不会让 Boss 移动
 *  - 没有配置任何招式时，退回使用 CombatComponent 的普通连击/重攻击
 */
UCLASS(meta = (DisplayName = "Boss Attack"))
class BATTLE_API UBTTask_BossAttack : public UBTTask_BlackboardBase
{
	GENERATED_BODY()

public:
	UBTTask_BossAttack();

	UPROPERTY(EditAnywhere, Category = "Boss Attack")
	TArray<FBossAttackOption> AttackOptions;

	// 出招后持续转向目标的时长（秒）
	UPROPERTY(EditAnywhere, Category = "Boss Attack", meta = (ClampMin = "0.0"))
	float TrackingDuration = 0.35f;

	// 转向插值速度
	UPROPERTY(EditAnywhere, Category = "Boss Attack", meta = (ClampMin = "0.0"))
	float TrackingInterpSpeed = 8.0f;

	// 刚使用过的招式的权重倍率
	UPROPERTY(EditAnywhere, Category = "Boss Attack", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float RepeatWeightScale = 0.5f;

	// 安全超时（秒），防止动画配置错误导致任务永远不结束
	UPROPERTY(EditAnywhere, Category = "Boss Attack", meta = (ClampMin = "1.0"))
	float MaxTaskDuration = 8.0f;

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual uint16 GetInstanceMemorySize() const override;
	virtual void InitializeMemory(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTMemoryInit::Type InitType) const override;
	virtual FString GetStaticDescription() const override;

private:
	struct FBTBossAttackMemory
	{
		float Elapsed = 0.0f;
		int32 LastOptionIndex = INDEX_NONE;
	};

	// 按距离/阶段/权重选出一个招式索引，找不到返回 INDEX_NONE
	int32 SelectAttackOption(float Distance, int32 Phase, int32 LastOptionIndex) const;
};
