// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Character.h" // 获取角色引用
#include "GameFramework/CharacterMovementComponent.h" // 操作移动组件
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

    // --- 目标锁定 ---
    UPROPERTY(BlueprintReadWrite, Category = "Targeting")
    AActor* TargetLockActor;

    UPROPERTY(BlueprintReadWrite, Category = "Targeting")
    AActor* TargetLockWidget;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting")
    TSubclassOf<AActor> TargetLockWidgetBP;

    // 对应 Socket Name 变量
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting")
    FName TargetSocketName = TEXT("spine_05_Socket");

    // 锁定目标时的旋转插值速度
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting")
    float TargetInterpSpeed = 10.0f;

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

    UFUNCTION(BlueprintCallable, Category = "Combat")
    void LockTarget();

    UFUNCTION(BlueprintCallable, Category = "Combat")
    void UnlockTarget();

    // --- 翻滚 ---
    // 是否正在翻滚中
    UPROPERTY(BlueprintReadWrite, Category = "Dodge")
    bool bIsDodging = false;

    // 翻滚预输入标记（翻滚后摇期间再次按下翻滚键，结束后自动触发下一次翻滚）
    UPROPERTY(BlueprintReadWrite, Category = "Dodge")
    bool bDodgeBuffered = false;

    // 是否处于可接受预输入的窗口（由蒙太奇中的 DodgeBufferWindow 通知开启）
    UPROPERTY(BlueprintReadWrite, Category = "Dodge")
    bool bCanBufferDodge = false;

    // 四方向翻滚蒙太奇（前/左/后/右）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dodge")
    class UAnimMontage* DodgeMontage_F;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dodge")
    class UAnimMontage* DodgeMontage_L;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dodge")
    class UAnimMontage* DodgeMontage_B;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dodge")
    class UAnimMontage* DodgeMontage_R;

    // 翻滚函数，由输入动作调用
    UFUNCTION(BlueprintCallable, Category = "Dodge")
    void Dodge();

    // 当前移动方向（由角色在 Move 时通过 SetMovementDirection 设置）
    UPROPERTY(BlueprintReadWrite, Category = "Movement")
    EMovementDirection MovementDirection = EMovementDirection::Forward;

    // 设置移动方向（供角色调用，保持组件与角色解耦）
    UFUNCTION(BlueprintCallable, Category = "Movement")
    void SetMovementDirection(EMovementDirection NewDirection) { MovementDirection = NewDirection; }

    // --- 战斗状态 ---
    // 是否正在攻击中
    UPROPERTY(BlueprintReadWrite, Category = "Combat")
    bool bIsAttacking = false;

    // 是否有连击输入（攻击期间再次按下攻击键）
    UPROPERTY(BlueprintReadWrite, Category = "Combat")
    bool bCombo = false;

    // 攻击动画蒙太奇
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
    class UAnimMontage* AttackMontage;

    // 攻击函数，由输入动作调用
    UFUNCTION(BlueprintCallable, Category = "Combat")
    void Attack();

	// --- 武器碰撞伤害系统 ---

    // 武器碰撞体引用（由 BattleCharacter 在 BeginPlay 中设置）
    UPROPERTY(BlueprintReadWrite, Category = "Weapon")
    UShapeComponent* SwordCollisionRef = nullptr;

    // 武器伤害值
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
    float SwordDamage = 10.0f;

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

    // 锁定敌人时每帧让玩家控制器面向敌人
    void HandleFaceTarget();

    // 每帧球形检测锁定目标是否仍在范围内，超出范围则自动解锁
    void CheckTargetInRange();

    // 检查玩家与锁定目标之间是否有物体遮挡，有则取消锁定
    void CheckTargetOcclusion();

    // 遮挡延迟解锁的定时器句柄
    FTimerHandle OcclusionTimerHandle;

    // 延迟解锁回调函数
    void OnOcclusionTimerExpired();

private:
    // 蒙太奇播放结束回调（On Completed / On Blend Out）
    UFUNCTION()
    void OnAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted);

    // 翻滚蒙太奇播放结束回调
    UFUNCTION()
    void OnDodgeMontageEnded(UAnimMontage* Montage, bool bInterrupted);

    // 翻滚蒙太奇通知回调，用于开启预输入窗口
    UFUNCTION()
    void OnDodgeMontageNotifyBegin(FName NotifyName, const FBranchingPointNotifyPayload& BranchingPointPayload);

    // 蒙太奇通知回调（On Notify Begin），用于连击判定
    UFUNCTION()
    void OnAttackMontageNotifyBegin(FName NotifyName, const FBranchingPointNotifyPayload& BranchingPointPayload);

    // 缓存Owner角色的骨骼网格体组件，避免每次获取
    USkeletalMeshComponent* GetOwnerMesh() const;

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

};
