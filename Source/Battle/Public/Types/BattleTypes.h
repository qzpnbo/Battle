// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DamageEvents.h"
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

// 战斗阵营枚举（用于伤害与锁定的敌我判断，同阵营之间互不伤害、不可锁定）
UENUM(BlueprintType)
enum class ECombatTeam : uint8
{
    Neutral UMETA(DisplayName = "中立"),
    Player  UMETA(DisplayName = "玩家"),
    Enemy   UMETA(DisplayName = "敌人")
};

// 战斗状态枚举（统一管理角色的战斗状态，替代多个 bool 标记）
// 新状态一律追加在末尾，避免已序列化的枚举值错位
UENUM(BlueprintType)
enum class ECombatState : uint8
{
    Idle UMETA(DisplayName = "空闲/移动"),
    Attacking UMETA(DisplayName = "轻攻击中"),
    HeavyAttacking UMETA(DisplayName = "重攻击中"),
    FallingAttacking UMETA(DisplayName = "下落攻击中"),
    Dodging UMETA(DisplayName = "翻滚中"),
    Staggered UMETA(DisplayName = "受击硬直"),
    Dead UMETA(DisplayName = "死亡"),
    Blocking UMETA(DisplayName = "格挡中"),
    SpecialAttacking UMETA(DisplayName = "特殊动作中（AI 招式/阶段转换）")
};

// 一次受击的最终判定结果（由 UCombatComponent::ResolveIncomingHit 返回）
UENUM(BlueprintType)
enum class EHitResponse : uint8
{
    Ignored     UMETA(DisplayName = "忽略（已死亡）"),
    Dodged      UMETA(DisplayName = "无敌帧闪避"),
    Parried     UMETA(DisplayName = "被弹反"),
    Blocked     UMETA(DisplayName = "被格挡"),
    GuardBroken UMETA(DisplayName = "破防"),
    Hit         UMETA(DisplayName = "命中（霸体未硬直）"),
    Staggered   UMETA(DisplayName = "命中并硬直")
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

// 近战伤害事件：在引擎 FDamageEvent 基础上携带韧性伤害、可否格挡/弹反等战斗信息
// 用法与 FPointDamageEvent 相同：TakeDamage 中通过 IsOfType(ClassID) 判断后 static_cast
struct FBattleDamageEvent : public FDamageEvent
{
    // 韧性（削韧）伤害
    float PoiseDamage = 0.0f;

    // 是否可被弹反（重攻击、下落攻击通常不可弹反）
    bool bCanBeParried = true;

    // 是否可被格挡
    bool bCanBeBlocked = true;

    static const int32 ClassID = 1001;

    virtual int32 GetTypeID() const override { return FBattleDamageEvent::ClassID; }
    virtual bool IsOfType(int32 InID) const override { return FBattleDamageEvent::ClassID == InID || FDamageEvent::IsOfType(InID); }
};

// 受击判定的输入/输出数据（ResolveIncomingHit 会就地修改 Damage，例如格挡减伤、弹反硬直增伤）
struct FIncomingHit
{
    float Damage = 0.0f;
    float PoiseDamage = 0.0f;
    bool bCanBeParried = true;
    bool bCanBeBlocked = true;
};
