// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Animation/AnimMontage.h"
#include "Types/BattleTypes.h"
class UStaticMeshComponent;
class UBoxComponent;
class UShapeComponent;
class UAnimInstance;
class USoundBase;
class UAttributeComponent;

#include "CombatComponent.generated.h"

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class BATTLE_API UCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UCombatComponent();

    // ============================================================================
    // 目标锁定配置
    // ============================================================================

    // 锁定目标时显示的UI蓝图类（在构造函数中通过路径加载）
    UPROPERTY(BlueprintReadOnly, Category = "Targeting")
    TSubclassOf<AActor> TargetLockWidgetBP;

    // 锁定UI挂载的骨骼Socket名称
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting")
    FName TargetSocketName = TEXT("spine_05_Socket");

    // 锁定目标时的旋转插值速度
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting")
    float TargetInterpSpeed = 15.0f;

    // 锁定目标时的Pitch偏移量
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting")
    float TargetPitchOffset = -25.0f;

    // 锁定目标的球形检测半径
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting")
    float LockOnRadius = 1500.0f;

    // 锁定目标的最大允许角度（与摄像机前方向量的夹角，单位：度）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting")
    float LockOnMaxAngle = 60.0f;

    // 遮挡后延迟解锁的时间（秒），在此时间内重新获得视野则取消解锁
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting")
    float OcclusionUnlockDelay = 1.2f;

    // ============================================================================
    // 锁定目标切换配置
    // ============================================================================

    // 触发目标切换所需的水平 Look 输入累积阈值（累积值超过此值才触发切换）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting|Switch", meta = (ClampMin = "0.1", ClampMax = "50.0"))
    float TargetSwitchThreshold = 20.0f;

    // 两次切换之间的冷却时间（秒），防止鼠标抖动导致反复切换
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting|Switch", meta = (ClampMin = "0.05", ClampMax = "2.0"))
    float TargetSwitchCooldown = 0.35f;

    // ============================================================================
    // 翻滚蒙太奇配置
    // ============================================================================

    // 四方向翻滚蒙太奇（前/左/后/右）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dodge")
    class UAnimMontage* DodgeMontage_F = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dodge")
    class UAnimMontage* DodgeMontage_L = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dodge")
    class UAnimMontage* DodgeMontage_B = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dodge")
    class UAnimMontage* DodgeMontage_R = nullptr;

    // ============================================================================
    // 攻击蒙太奇配置
    // ============================================================================

    // 轻攻击动画蒙太奇
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack")
    class UAnimMontage* AttackMontage = nullptr;

    // 重攻击动画蒙太奇
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack")
    class UAnimMontage* HeavyAttackMontage = nullptr;

    // 下落攻击动画蒙太奇（空中按攻击键时播放）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack")
    class UAnimMontage* FallingAttackMontage = nullptr;

    // 连击蒙太奇 Section 名称列表（与蒙太奇中的 Section 名称一一对应）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack")
    TArray<FName> AttackComboSectionNames = { TEXT("S0"), TEXT("S1"), TEXT("S2") };

    // ============================================================================
    // 攻击数值配置
    // ============================================================================

    // 攻击开始后允许通过移动输入调整朝向的时间窗口（秒）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Rotation")
    float AttackRotationWindowDuration = 0.5f;

    // 攻击朝向调整的旋转插值速度
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Rotation")
    float AttackRotationInterpSpeed = 12.0f;

    // ============================================================================
    // 方向性攻击配置（Aim Offset）
    // ============================================================================

    // 当前瞄准的 Pitch 值（-60 ~ 60），供动画蓝图中的 Aim Offset 使用
    // 在攻击状态或锁定目标时生效，正值 = 向上看，负值 = 向下看
    UPROPERTY(BlueprintReadOnly, Category = "Attack|AimOffset")
    float AttackAimPitch = 0.0f;

    // 是否启用方向性攻击（根据摄像机俯仰角混合不同攻击姿态）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|AimOffset")
    bool bEnableDirectionalAttack = true;

    // 攻击 Aim Pitch 的插值速度（越大越灵敏，越小越平滑）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|AimOffset")
    float AttackAimPitchInterpSpeed = 10.0f;

    // 攻击 Aim Pitch 的最大角度限制（上下对称，例如60表示 -60 ~ 60）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|AimOffset")
    float AttackAimPitchClamp = 60.0f;

    // ============================================================================
    // 受击硬直配置（方向性受击蒙太奇，根据攻击来源方向播放不同动画）
    // ============================================================================

    // 前方受击（攻击者在受击者前方，角色向后仰）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitReact")
    class UAnimMontage* HitReactMontage_F = nullptr;

    // 后方受击（攻击者在受击者后方，角色向前弯）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitReact")
    class UAnimMontage* HitReactMontage_B = nullptr;

    // 左侧受击（攻击者在受击者左侧，角色向右歪）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitReact")
    class UAnimMontage* HitReactMontage_L = nullptr;

    // 右侧受击（攻击者在受击者右侧，角色向左歪）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitReact")
    class UAnimMontage* HitReactMontage_R = nullptr;

    // ============================================================================
    // 翻滚无敌帧配置
    // ============================================================================

    // 当前是否处于无敌帧中（翻滚期间由蒙太奇通知控制）
    UPROPERTY(BlueprintReadOnly, Category = "Dodge|IFrame")
    bool bIsInvincible = false;

    // ============================================================================
    // 武器伤害配置
    // ============================================================================

    // 武器基础伤害值（轻攻击伤害）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
    float SwordDamage = 10.0f;

    // 重攻击伤害倍率（最终伤害 = SwordDamage * 倍率）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
    float HeavyAttackDamageMultiplier = 2.0f;

    // 下落攻击伤害倍率（最终伤害 = SwordDamage * 倍率）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
    float FallingAttackDamageMultiplier = 1.5f;

    // ============================================================================
    // 命中反馈配置（Hit Lag + Camera Shake）
    // ============================================================================

    // 是否启用命中镜头震动
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitFeedback")
    bool bEnableHitCameraShake = true;

    // 轻攻击镜头震动类（在蓝图中指定具体的 CameraShakeBase 子类）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitFeedback")
    TSubclassOf<UCameraShakeBase> LightHitCameraShake;

    // 重攻击/下落攻击镜头震动类
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitFeedback")
    TSubclassOf<UCameraShakeBase> HeavyHitCameraShake;

    // ============================================================================
    // Hit Lag 配置（攻击者动画减速，替代全局时间膨胀的局部方案）
    // 命中时只降低攻击者的蒙太奇播放速率，受击者不受影响
    // ============================================================================

    // 是否启用 Hit Lag（攻击者动画减速）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitFeedback|HitLag")
    bool bEnableHitLag = true;

    // 轻攻击 Hit Lag 的动画播放速率（越小越慢，0.05 = 几乎暂停）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitFeedback|HitLag", meta = (ClampMin = "0.01", ClampMax = "1.0"))
    float LightHitLagRate = 0.05f;

    // 轻攻击 Hit Lag 持续时间（秒，真实时间）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitFeedback|HitLag", meta = (ClampMin = "0.01", ClampMax = "0.5"))
    float LightHitLagDuration = 0.08f;

    // 重攻击/下落攻击 Hit Lag 的动画播放速率
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitFeedback|HitLag", meta = (ClampMin = "0.01", ClampMax = "1.0"))
    float HeavyHitLagRate = 0.02f;

    // 重攻击/下落攻击 Hit Lag 持续时间（秒，真实时间）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitFeedback|HitLag", meta = (ClampMin = "0.01", ClampMax = "0.5"))
    float HeavyHitLagDuration = 0.12f;

    // ============================================================================
    // 耐力消耗配置（数值存储在 UAttributeComponent，这里只定义各动作的消耗）
    // ============================================================================

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stamina", meta = (ClampMin = "0.0"))
    float LightAttackStaminaCost = 12.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stamina", meta = (ClampMin = "0.0"))
    float HeavyAttackStaminaCost = 25.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stamina", meta = (ClampMin = "0.0"))
    float FallingAttackStaminaCost = 18.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stamina", meta = (ClampMin = "0.0"))
    float DodgeStaminaCost = 18.0f;

    // 格挡时每点（格挡前）伤害消耗的耐力
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stamina", meta = (ClampMin = "0.0"))
    float BlockStaminaCostPerDamage = 1.5f;

    // 举盾格挡期间的耐力回复倍率
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stamina", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float BlockStaminaRegenMultiplier = 0.3f;

    // ============================================================================
    // 韧性（削韧）配置
    // ============================================================================

    // 各攻击对目标造成的韧性伤害
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Poise", meta = (ClampMin = "0.0"))
    float LightAttackPoiseDamage = 20.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Poise", meta = (ClampMin = "0.0"))
    float HeavyAttackPoiseDamage = 45.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Poise", meta = (ClampMin = "0.0"))
    float FallingAttackPoiseDamage = 35.0f;

    // 自身出招期间受到的韧性伤害倍率（< 1 即"攻击霸体"，Boss 常用 0.3 左右）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Poise", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float AttackingPoiseDamageScale = 1.0f;

    // ============================================================================
    // 格挡 / 弹反配置
    // ============================================================================

    // 举盾循环蒙太奇（会自动把第一个 Section 设为循环，无需手动配置）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Block")
    UAnimMontage* BlockMontage = nullptr;

    // 格挡住攻击时的受击反馈蒙太奇（可选）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Block")
    UAnimMontage* BlockReactMontage = nullptr;

    // 耐力被打空破防时的蒙太奇（可选，为空时使用方向性受击蒙太奇）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Block")
    UAnimMontage* GuardBreakMontage = nullptr;

    // 弹反成功时自身播放的蒙太奇（可选，为空时使用格挡反馈蒙太奇）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Block|Parry")
    UAnimMontage* ParryMontage = nullptr;

    // 自己的攻击被弹反时播放的大硬直蒙太奇（可选，为空时使用前方受击蒙太奇）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Block|Parry")
    UAnimMontage* ParriedMontage = nullptr;

    // 格挡减伤比例（0.9 = 只承受 10% 伤害）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Block", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float BlockDamageReduction = 0.9f;

    // 可格挡的正面半角（度），来自身后/侧后方的攻击无法格挡
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Block", meta = (ClampMin = "0.0", ClampMax = "180.0"))
    float BlockHalfAngle = 70.0f;

    // 举盾期间的移动速度倍率
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Block", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float BlockWalkSpeedMultiplier = 0.45f;

    // 弹反窗口：按下格挡后多长时间内被击中判定为弹反（秒）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Block|Parry", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float ParryWindow = 0.2f;

    // 被弹反硬直期间受到伤害的倍率（弹反后的反击加成）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Block|Parry", meta = (ClampMin = "1.0"))
    float ParriedDamageMultiplier = 2.0f;

    // 弹反成功时的全局慢动作（仅玩家参与时触发）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Block|Parry", meta = (ClampMin = "0.05", ClampMax = "1.0"))
    float ParrySlowMoTimeDilation = 0.25f;

    // 慢动作持续时间（真实时间，秒）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Block|Parry", meta = (ClampMin = "0.0", ClampMax = "2.0"))
    float ParrySlowMoDuration = 0.3f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Block")
    USoundBase* BlockSound = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Block|Parry")
    USoundBase* ParrySound = nullptr;

    // ============================================================================
    // 动作速率（Boss 二阶段等场景下整体加快出招）
    // ============================================================================

    // 攻击类蒙太奇的播放速率倍率
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack", meta = (ClampMin = "0.1", ClampMax = "3.0"))
    float ActionPlayRate = 1.0f;

    // ============================================================================
    // 阵营
    // ============================================================================

    // 所属阵营（由拥有者在构造函数中设置默认值，可在蓝图中覆盖）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Team")
    ECombatTeam Team = ECombatTeam::Neutral;

    // 判断另一个 Actor 是否为敌对目标
    // 没有 CombatComponent 的 Actor（如可破坏物）视为可被攻击；同阵营互不敌对
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Team")
    bool IsHostileTo(const AActor* Other) const;

    // ============================================================================
    // 目标锁定系统
    // ============================================================================

    UPROPERTY(BlueprintReadWrite, Category = "Targeting")
    AActor* TargetLockActor = nullptr;

    UPROPERTY(BlueprintReadWrite, Category = "Targeting")
    AActor* TargetLockWidget = nullptr;

    UFUNCTION(BlueprintCallable, Category = "Combat")
    void LockTarget();

    UFUNCTION(BlueprintCallable, Category = "Combat")
    void UnlockTarget();

    // 切换锁定目标到相邻敌人（由角色 Look 输入驱动）
    // @param LookDeltaX 水平 Look 输入增量（正值 = 向右，负值 = 向左）
    UFUNCTION(BlueprintCallable, Category = "Combat")
    void HandleLockLookInput(float LookDeltaX);

    // 切换锁定目标到相邻敌人
    // @param bRight true=向右切换，false=向左切换
    // @return 是否成功切换到新目标
    UFUNCTION(BlueprintCallable, Category = "Combat")
    bool SwitchLockTarget(bool bRight);

    // ============================================================================
    // 受击硬直系统
    // ============================================================================

    // 受击判定（由角色 TakeDamage 调用），按优先级依次判断：
    //   死亡 → 无敌帧 → 被弹反增伤 → 格挡（弹反窗口 / 减伤 / 破防）→ 韧性（破韧才硬直）
    // 会就地修改 Hit.Damage（格挡减伤、被弹反增伤），并负责播放对应的受击/格挡动画
    EHitResponse ResolveIncomingHit(FIncomingHit& Hit, AActor* DamageCauser);

    // 自己的攻击被对方弹反：中断动作，进入大硬直，期间受到的伤害提高
    UFUNCTION(BlueprintCallable, Category = "Combat")
    void ReceiveParried(AActor* Parrier);

    // 是否处于被弹反后的硬直中
    UFUNCTION(BlueprintPure, Category = "Combat")
    bool IsParryStunned() const { return bIsParryStunned; }

    // ============================================================================
    // 格挡系统
    // ============================================================================

    // 按下格挡键（Idle 时立即举盾；攻击/翻滚结束后若仍按住会自动举盾）
    UFUNCTION(BlueprintCallable, Category = "Block")
    void StartBlock();

    // 松开格挡键
    UFUNCTION(BlueprintCallable, Category = "Block")
    void StopBlock();

    UFUNCTION(BlueprintPure, Category = "Block")
    bool IsBlocking() const { return CombatState == ECombatState::Blocking; }

    // ============================================================================
    // AI / 特殊动作接口
    // ============================================================================

    // 播放任意攻击蒙太奇（Boss 招式等），伤害/削韧/可否弹反由参数指定
    // @return 是否成功开始（非 Idle 或空中时失败）
    UFUNCTION(BlueprintCallable, Category = "Combat")
    bool PerformAttackMontage(UAnimMontage* Montage, float DamageMultiplier = 1.0f, float PoiseDamage = 30.0f, bool bCanBeParried = true);

    // 强制播放特殊动作（打断当前动作，如 Boss 转阶段咆哮），可选全程无敌
    UFUNCTION(BlueprintCallable, Category = "Combat")
    bool PlayForcedActionMontage(UAnimMontage* Montage, bool bInvincibleDuringAction = true);

    // ============================================================================
    // 战斗状态管理
    // ============================================================================

    // 当前战斗状态
    UPROPERTY(BlueprintReadWrite, Category = "Combat")
    ECombatState CombatState = ECombatState::Idle;

    // 获取当前战斗状态
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Combat")
    ECombatState GetCombatState() const { return CombatState; }

    // 设置战斗状态
    UFUNCTION(BlueprintCallable, Category = "Combat")
    void SetCombatState(ECombatState NewState);

    // 判断角色是否可以执行新动作
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Combat")
    bool CanPerformAction() const { return CombatState == ECombatState::Idle; }

    
    // 判断角色是否处于任何攻击状态
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Combat")
    bool IsInAnyAttackState() const;

    // ============================================================================
    // 通用输入缓存系统（跨动作类型预输入）
    // ============================================================================

    // 缓存的输入动作类型
    UPROPERTY(BlueprintReadWrite, Category = "InputBuffer")
    EBufferedInputAction BufferedAction = EBufferedInputAction::None;

    // 是否处于可接受跨动作预输入的窗口
    // 攻击蒙太奇中由 AttackPhase == Buffer 控制，其他蒙太奇（翻滚/重攻击）由此 bool 控制
    UPROPERTY(BlueprintReadWrite, Category = "InputBuffer")
    bool bCanBufferInput = false;

    // 尝试缓存一个输入动作（在动作被拒绝时调用）
    UFUNCTION(BlueprintCallable, Category = "InputBuffer")
    void BufferInput(EBufferedInputAction Action);

    // 消费并执行缓存的输入动作（在蒙太奇结束时调用）
    UFUNCTION(BlueprintCallable, Category = "InputBuffer")
    void ConsumeBufferedInput();

    // 清空缓存的输入
    UFUNCTION(BlueprintCallable, Category = "InputBuffer")
    void ClearBufferedInput();

    // ============================================================================
    // 翻滚系统
    // ============================================================================

    // 翻滚函数，由输入动作调用
    UFUNCTION(BlueprintCallable, Category = "Dodge")
    void Dodge();

    // 当前移动方向（由角色在 Move 时通过 SetMovementDirection 设置）
    UPROPERTY(BlueprintReadWrite, Category = "Movement")
    EMovementDirection MovementDirection = EMovementDirection::Forward;

    // 当前是否有移动输入（松开摇杆/按键时为 false，此时翻滚为原地后撤步）
    UPROPERTY(BlueprintReadOnly, Category = "Movement")
    bool bHasMovementInput = false;

    // 设置移动方向（供角色调用，保持组件与角色解耦）
    UFUNCTION(BlueprintCallable, Category = "Movement")
    void SetMovementDirection(EMovementDirection NewDirection)
    {
        MovementDirection = NewDirection;
        bHasMovementInput = true;
    }

    // 清除移动输入（由角色在移动输入结束时调用）
    UFUNCTION(BlueprintCallable, Category = "Movement")
    void ClearMovementInput()
    {
        bHasMovementInput = false;
        MovementDirection = EMovementDirection::Backward;
    }

    // ============================================================================
    // 攻击系统
    // ============================================================================
    
    // 攻击函数，由输入动作调用
    UFUNCTION(BlueprintCallable, Category = "Combat")
    void Attack();

    // 重攻击函数，由按住Shift+攻击键调用
    UFUNCTION(BlueprintCallable, Category = "Combat")
    float HeavyAttack();

    // 当前连击段索引（0=第一段, 1=第二段, 2=第三段）
    UPROPERTY(BlueprintReadWrite, Category = "Combat")
    int32 AttackComboIndex = 0;

    // 当前攻击段的阶段（替代 bInComboWindow / bCanBufferInput / bComboJustAdvanced 三个 bool）
    // 每段攻击的时间线：Startup → Combo → Buffer
    //   Startup: 起手阶段，攻击输入无效
    //   Combo:   连击窗口，攻击输入触发连击跳转到下一段
    //   Buffer:  预输入窗口，攻击输入被缓存，蒙太奇结束后从 S0 重新开始
    UPROPERTY(BlueprintReadWrite, Category = "Combat")
    EAttackPhase AttackPhase = EAttackPhase::None;

    // ============================================================================
    // 武器碰撞伤害系统
    // ============================================================================

    // 武器碰撞体引用（由 BattleCharacter 在 BeginPlay 中设置）
    UPROPERTY(BlueprintReadWrite, Category = "Weapon")
    UShapeComponent* SwordCollisionRef = nullptr;

    // 开始伤害检测
    UFUNCTION(BlueprintCallable, Category = "Weapon")
    void StartDamageTrace();

    // 结束伤害检测
    UFUNCTION(BlueprintCallable, Category = "Weapon")
    void EndDamageTrace();

    // 初始化武器碰撞体（绑定 Overlap 回调）
    void InitSwordCollision();

    // Overlap 回调：碰撞体与其他 Actor 重叠时触发
    UFUNCTION()
    void OnSwordOverlapBegin(
        UPrimitiveComponent* OverlappedComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComp,
        int32 OtherBodyIndex,
        bool bFromSweep,
        const FHitResult& SweepResult);

    // 记录本次挥砍中已命中的 Actor，防止重复伤害
    UPROPERTY()
    TSet<AActor*> HitActorsSet;

    // ============================================================================
    // 内部工具函数
    // ============================================================================

    // 锁定敌人时每帧让玩家控制器面向敌人
    void HandleFaceTarget(float DeltaTime);

    // 每帧球形检测锁定目标是否仍在范围内，超出范围则自动解锁
    void CheckTargetInRange();

    // 检查玩家与锁定目标之间是否有物体遮挡，有则取消锁定
    void CheckTargetOcclusion();

    // 遮挡延迟解锁的定时器句柄
    FTimerHandle OcclusionTimerHandle;

    // 延迟解锁回调函数
    void OnOcclusionTimerExpired();

    // 将锁定 UI Widget 附着到指定 Actor 的 Mesh（用于切换目标时更新 UI）
    void AttachLockWidgetToActor(AActor* TargetActor);

    // 目标切换冷却剩余时间（>0 表示冷却中，禁止再次切换）
    float TargetSwitchCooldownRemaining = 0.0f;

    // 水平 Look 输入累积值（用于判断是否达到切换阈值）
    float TargetSwitchLookAccumulator = 0.0f;

    // 命中反馈：触发 Hit Lag 和镜头震动
    void ApplyHitFeedback();

    // Hit Lag 定时器句柄（普通游戏时间定时器，本项目未使用全局时间膨胀）
    FTimerHandle HitLagTimerHandle;

    // 应用 Hit Lag（降低攻击者蒙太奇播放速率）
    void ApplyHitLag();

    // Hit Lag 恢复回调（恢复正常播放速率）
    void OnHitLagTimerExpired();

    // 获取当前正在播放的攻击蒙太奇（用于 Hit Lag 速率调整）
    UAnimMontage* GetCurrentAttackMontage() const;

private:
    // 蒙太奇播放结束回调（On Completed / On Blend Out）
    UFUNCTION()
    void OnAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted);

    // 重攻击蒙太奇播放结束回调
    UFUNCTION()
    void OnHeavyAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted);

    // 下落攻击蒙太奇播放结束回调
    UFUNCTION()
    void OnFallingAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted);

    // 翻滚蒙太奇播放结束回调
    UFUNCTION()
    void OnDodgeMontageEnded(UAnimMontage* Montage, bool bInterrupted);

    // 翻滚蒙太奇通知回调，用于开启预输入窗口
    UFUNCTION()
    void OnDodgeMontageNotifyBegin(FName NotifyName, const FBranchingPointNotifyPayload& BranchingPointPayload);

    // 蒙太奇通知回调（On Notify Begin），用于连击判定
    UFUNCTION()
    void OnAttackMontageNotifyBegin(FName NotifyName, const FBranchingPointNotifyPayload& BranchingPointPayload);

    // 尝试设置连击的 NextSection 链接（在 ComboWindow 期间调用）
    void TrySetComboNextSection();

    // 重攻击蒙太奇通知回调，用于开启预输入窗口
    UFUNCTION()
    void OnHeavyAttackMontageNotifyBegin(FName NotifyName, const FBranchingPointNotifyPayload& BranchingPointPayload);

    // 受击蒙太奇播放结束回调
    UFUNCTION()
    void OnHitReactMontageEnded(UAnimMontage* Montage, bool bInterrupted);

    // 受击蒙太奇通知回调，用于开启预输入窗口
    UFUNCTION()
    void OnHitReactMontageNotifyBegin(FName NotifyName, const FBranchingPointNotifyPayload& BranchingPointPayload);

    // 根据攻击来源方向选择对应的受击蒙太奇
    UAnimMontage* SelectDirectionalHitReact(AActor* DamageCauser) const;

    // 判断指定蒙太奇是否为受击蒙太奇之一（用于连续受击时的回调过滤）
    bool IsAnyHitReactMontage(UAnimMontage* Montage) const;

    // 中断当前正在播放的蒙太奇并清理相关状态
    void InterruptCurrentAction();

    // 死亡时的完整清理（解锁目标、清除定时器、中断动作、停止Tick）
    void HandleDeath();

    // 缓存Owner角色引用，避免每次Cast
    UPROPERTY()
    TWeakObjectPtr<ACharacter> CachedOwnerCharacter;

    // 缓存Owner角色的骨骼网格体组件
    UPROPERTY()
    TWeakObjectPtr<USkeletalMeshComponent> CachedOwnerMesh;

    // 设置攻击初始朝向（按下攻击键时调用，重置朝向调整计时器）
    void SetAttackRotation();

    // 攻击期间每帧更新朝向（类魂机制：动画前几帧可通过移动输入改变朝向）
    void UpdateAttackRotation(float DeltaTime);

    // 每帧更新瞄准 Pitch（从控制器/摄像机获取俯仰角，传递给动画蓝图）
    void UpdateAimPitch(float DeltaTime);

    // 攻击朝向调整已经过的时间（超过窗口时长后停止调整）
    float AttackRotationElapsed = 0.0f;

    // 当前正在播放的翻滚蒙太奇（四方向之一，用于通知过滤与精确停止）
    UPROPERTY()
    TObjectPtr<UAnimMontage> CurrentDodgeMontage = nullptr;

    // 是否处于锁定状态（独立于 TargetLockActor，目标被销毁/GC 置空后仍能正确执行解锁清理）
    bool bIsTargetLocked = false;

    // 在 [-1, 1] 内安全计算两个单位向量的夹角（度），避免浮点误差导致 Acos 返回 NaN
    static float SafeAngleDegrees(const FVector& A, const FVector& B);

    // ---------------------------------------------------------------------------
    // 状态流转辅助
    // ---------------------------------------------------------------------------

    // 动作结束统一出口：回到 Idle → 执行预输入 → 若仍按住格挡则自动举盾
    void ReturnToIdle();

    // 播放硬直类蒙太奇（受击 / 破防 / 被弹反），结束后自动回到 Idle
    void PlayStaggerMontage(UAnimMontage* Montage);

    // 当前播放中的硬直蒙太奇（用于过滤连续受击时旧实例的结束回调）
    UPROPERTY()
    TObjectPtr<UAnimMontage> CurrentStaggerMontage = nullptr;

    // 当前攻击的规格（开始攻击时写入，命中时读取）
    float CurrentAttackDamageMultiplier = 1.0f;
    float CurrentAttackPoiseDamage = 0.0f;
    bool bCurrentAttackCanBeParried = true;
    void SetCurrentAttackSpec(float DamageMultiplier, float PoiseDamage, bool bCanBeParried);

    // 当前播放中的特殊动作蒙太奇（PerformAttackMontage / PlayForcedActionMontage）
    UPROPERTY()
    TObjectPtr<UAnimMontage> CurrentSpecialMontage = nullptr;

    // 特殊动作是否开启了全程无敌（结束时需要关闭）
    bool bSpecialActionInvincible = false;

    UFUNCTION()
    void OnSpecialMontageEnded(UAnimMontage* Montage, bool bInterrupted);

    // 获取拥有者的 AnimInstance（失败返回 nullptr）
    UAnimInstance* GetOwnerAnimInstance() const;

    // ---------------------------------------------------------------------------
    // 耐力
    // ---------------------------------------------------------------------------

    UPROPERTY()
    TWeakObjectPtr<UAttributeComponent> CachedAttributes;

    // 是否有耐力发动动作（未挂属性组件或未启用耐力时总是 true）
    bool HasStaminaForAction() const;

    // 消耗耐力（前置条件：HasStaminaForAction 已通过）
    void ConsumeStamina(float Cost);

    // 根据状态切换更新耐力回复（出招时暂停、格挡时减缓）与格挡移速
    void OnCombatStateTransition(ECombatState OldState, ECombatState NewState);

    // ---------------------------------------------------------------------------
    // 格挡 / 弹反
    // ---------------------------------------------------------------------------

    // 格挡键是否按住（动作结束后用于自动恢复举盾）
    bool bBlockInputHeld = false;

    // 本次举盾开始时间（用于弹反窗口判定）
    float BlockStartTime = -1000.0f;

    // 进入格挡前的移动速度（退出时恢复）
    float SavedMaxWalkSpeed = 0.0f;

    // 是否处于被弹反的硬直中
    bool bIsParryStunned = false;

    void EnterBlock();

    // 离开格挡（停止举盾动画）；bReturnToIdle=false 时由调用方负责设置后续状态
    void ExitBlock(bool bReturnToIdle);

    // 播放举盾循环
    void PlayBlockLoop();

    // 格挡中播放一次性反馈蒙太奇（格挡受击 / 弹反成功），结束后回到举盾循环
    void PlayBlockReaction(UAnimMontage* Montage);

    UFUNCTION()
    void OnBlockReactionMontageEnded(UAnimMontage* Montage, bool bInterrupted);

    // 攻击者是否在自己的格挡角度内
    bool IsAttackerInBlockArc(const AActor* Attacker) const;

    // 弹反成功：让攻击者进入被弹反硬直，并播放反馈（慢动作、音效、镜头震动）
    void HandleParrySuccess(AActor* Attacker);

    // 弹反慢动作
    FTimerHandle ParrySlowMoTimerHandle;
    bool bParrySlowMoActive = false;
    void StartParrySlowMo();
    void StopParrySlowMo();

    // 在自身位置播放音效
    void PlayCombatSound(USoundBase* Sound) const;

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

};
