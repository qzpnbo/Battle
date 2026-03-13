// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Character.h" // 获取角色引用
#include "GameFramework/CharacterMovementComponent.h" // 操作移动组件
#include "CombatComponent.generated.h"

// 对应蓝图中的 Movement Direction 枚举
UENUM(BlueprintType)
enum class EMovementDirection : uint8
{
    Forward, Backward, Left, Right
};

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class BATTLE_API UCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UCombatComponent();

    // --- 目标锁定 ---
    UPROPERTY(BlueprintReadWrite, Category = "Targeting")
    AActor* TargetLockActor; // 对应蓝图的 Target Lock Actor

    UPROPERTY(BlueprintReadWrite, Category = "Targeting")
    AActor* TargetLockWidget;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting")
    TSubclassOf<AActor> TargetLockWidgetBP;

    // 对应 Socket Name 变量
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting")
    FName TargetSocketName = TEXT("spine_05_Socket"); // 默认值可以改为你常用的插槽名

    // --- 函数 ---
    UFUNCTION(BlueprintCallable, Category = "Combat")
    void LockTarget();

    UFUNCTION(BlueprintCallable, Category = "Combat")
    void UnlockTarget();

    // 对应图中那些复杂的逻辑节点
    void UpdateMovementDirection();

	// --- 战斗状态 ---
    // 攻击动画蒙太奇 (在编辑器里指定)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
    class UAnimMontage* AttackMontage;

    // 状态变量
    UPROPERTY(BlueprintReadWrite, Category = "Combat")
    bool bIsAttacking = false;

    UPROPERTY(BlueprintReadWrite, Category = "Combat")
    bool bComboRequest = false; // 对应蓝图中中间那个 AND 判断的逻辑

    UPROPERTY(BlueprintReadWrite, Category = "Combat")
    int32 ComboCount = 0;

    UFUNCTION(BlueprintCallable, Category = "Combat")
    void Attack();

    // 蒙太奇结束的回调函数
    void OnAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted);

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

		
};
