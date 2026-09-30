// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Character/BattleCharacterBase.h"
#include "BattleCharacter.generated.h"

// 前向声明（减少头文件包含，加快编译速度）
class UUserWidget;
class USpringArmComponent;
class UCameraComponent;
class USoundBase;
class UInputMappingContext;
class UInputAction;
struct FInputActionValue;
enum class EMovementDirection : uint8;

/**
 * 玩家角色：摄像机、输入处理、死亡后通知重生系统
 */
UCLASS()
class BATTLE_API ABattleCharacter : public ABattleCharacterBase
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	ABattleCharacter();

	// 基础组件声明
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	class USpringArmComponent* CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	class UCameraComponent* FollowCamera;

	// 死亡界面类（为空时使用 C++ 默认的 UDeathScreenWidget）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Death")
	TSubclassOf<UUserWidget> DeathScreenWidgetClass;

	// 死亡后多久重生（秒）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Death", meta = (ClampMin = "0.5"))
	float RespawnDelay = 4.0f;

protected:
	// Mapping Context
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	class UInputMappingContext* IMC_Default;

	// 视角输入动作
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	class UInputAction* IA_Look;

	// 移动输入动作
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	class UInputAction* IA_Move;

	// 跳跃输入动作
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	class UInputAction* IA_Jump;

	// 锁定敌人输入动作
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	class UInputAction* IA_Lock;

	// 攻击输入动作
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	class UInputAction* IA_Attack;

	// 翻滚输入动作
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	class UInputAction* IA_Dodge;

	// 格挡输入动作（可选）：为空时运行时自动创建，并默认映射到 鼠标右键 / 手柄 LB
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	class UInputAction* IA_Block;

	// 跳跃音效
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound")
	USoundBase* JumpSound;

	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	// 被控制器占有时添加输入映射（重生后新 Pawn 的 BeginPlay 早于 Possess，必须在这里添加）
	virtual void NotifyControllerChanged() override;

	// 失去控制器时移除本 Pawn 运行时创建的输入映射
	virtual void UnPossessed() override;

	// 视角输入处理（Camera Input）
	void Look(const FInputActionValue& Value);

	// 移动输入处理（Movement Input）
	void Move(const FInputActionValue& Value);

	// 移动输入结束（松开按键/摇杆回中），重置移动方向
	void StopMove(const FInputActionValue& Value);

	// 锁定敌人输入处理（Lock Input）
	void LockTarget();

	// 攻击输入处理（Attack Input）
	void Attack();

	// 重攻击输入处理（Heavy Attack Input）
	void HeavyAttack();

	// 翻滚输入处理（Dodge Input）
	void Dodge();

	// 格挡输入处理（按下开始格挡，松开结束格挡）
	void StartBlock();
	void StopBlock();

	// 根据输入值计算移动方向
	EMovementDirection GetMovementDirection(const FInputActionValue& Value);

	// 跳跃（重写基类 ACharacter::Jump）
	virtual void Jump() override;

	// 停止跳跃（重写基类 ACharacter::StopJumping）
	virtual void StopJumping() override;

	// 跳跃成功后的回调（播放音效）
	virtual void OnJumped_Implementation() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

private:
	// 将输入映射上下文添加到本地玩家
	void AddInputMappingContexts();

	// 运行时创建的输入映射上下文（仅包含格挡等未配置资产的输入）
	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> RuntimeMappingContext;
};
