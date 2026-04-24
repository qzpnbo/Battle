// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BattleTypes.generated.h"

// 移动方向枚举
UENUM(BlueprintType)
enum class EMovementDirection : uint8
{
    Forward,
    Backward,
    Left,
    Right
};

// 战斗状态枚举（统一管理角色的战斗状态，替代多个 bool 标记）
// 状态优先级从低到高：Idle < Attacking < HeavyAttacking < FallingAttacking < Dodging < Staggered < Dead
UENUM(BlueprintType)
enum class ECombatState : uint8
{
    Idle UMETA(DisplayName = "空闲/移动"),
    Attacking UMETA(DisplayName = "轻攻击中"),
    HeavyAttacking UMETA(DisplayName = "重攻击中"),
    FallingAttacking UMETA(DisplayName = "下落攻击中"),
    Dodging UMETA(DisplayName = "翻滚中"),
    Staggered UMETA(DisplayName = "受击硬直"),
    Dead UMETA(DisplayName = "死亡")
};

// 可缓存的输入动作类型（用于跨动作预输入系统）
UENUM(BlueprintType)
enum class EBufferedInputAction : uint8
{
    None,
    Attack,
    HeavyAttack,
    Dodge
};

// 攻击段内的阶段枚举（替代多个 bool 标记，清晰表达每段攻击的时间线）
// 每段攻击的时间线：Startup → Combo → Buffer
//   Startup: 起手阶段，攻击输入无效
//   Combo:   连击窗口，攻击输入触发连击跳转到下一段
//   Buffer:  预输入窗口，攻击输入被缓存，蒙太奇结束后从 S0 重新开始
UENUM(BlueprintType)
enum class EAttackPhase : uint8
{
    None     UMETA(DisplayName = "非攻击中"),
    Startup  UMETA(DisplayName = "起手阶段"),
    Combo    UMETA(DisplayName = "连击窗口"),
    Buffer   UMETA(DisplayName = "预输入窗口")
};
