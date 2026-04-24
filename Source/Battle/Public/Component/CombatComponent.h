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

    // 重攻击起跳的垂直冲量大小
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|HeavyAttack")
    float HeavyAttackLaunchForce = 800.0f;

    // 重攻击浮空高度检测阈值：超过此高度中断蒙太奇切换下落动画
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|HeavyAttack")
    float HeavyAttackMaxAirborneHeight = 300.0f;

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

    // 处理受击逻辑（由角色类的 TakeDamage 调用）
    // @param DamageCauser 造成伤害的 Actor（用于计算受击方向）
    // 返回 true 表示正常受击，返回 false 表示无敌帧期间免疫伤害
    UFUNCTION(BlueprintCallable, Category = "Combat")
    bool HandleTakeDamage(AActor* DamageCauser = nullptr);

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

    // 设置移动方向（供角色调用，保持组件与角色解耦）
    UFUNCTION(BlueprintCallable, Category = "Movement")
    void SetMovementDirection(EMovementDirection NewDirection) { MovementDirection = NewDirection; }

    // ============================================================================
    // 攻击系统
    // ============================================================================
    
    // 攻击函数，由输入动作调用
    UFUNCTION(BlueprintCallable, Category = "Combat")
    void Attack();

    // 重攻击函数，由按住Shift+攻击键调用
    UFUNCTION(BlueprintCallable, Category = "Combat")
    void HeavyAttack();

    // 重攻击起跳冲量（由蒙太奇通知 HeavyAttackLaunch 触发）
    UFUNCTION(BlueprintCallable, Category = "Combat")
    void HeavyAttackLaunch();

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

    // Hit Lag 定时器句柄
    // Hit Lag 定时器句柄（使用真实时间定时器，不受全局时间膨胀影响）
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

    // 重攻击蒙太奇通知回调，用于触发起跳冲量
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

    // 向下射线检测的最大距离（应大于 MaxAirborneHeight，确保能检测到地面）
    float HeavyAttackTraceDistance = 500.0f;

    // 标记是否已经执行过起跳冲量（防止重复触发）
    bool bHeavyAttackLaunched = false;

    // 每帧检测重攻击期间的浮空状态
    void CheckHeavyAttackAirborne(float DeltaTime);

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

};
