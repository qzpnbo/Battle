// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Blueprint/UserWidget.h"
#include "Component/CombatComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Sound/SoundBase.h"
#include "BattleCharacter.generated.h"

// 定义一个动态多播委托，参数为当前生命值和最大生命值
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHealthChanged, float, CurrentHealth, float, MaxHealth);

// 预声明组件，减少头文件包含，加快编译速度
class USpringArmComponent;
class UCameraComponent;
class UInputMappingContext;
class UInputAction;
struct FInputActionValue;
class UCombatComponent;

UCLASS()
class BATTLE_API ABattleCharacter : public ACharacter
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

public:
    // 委托实例（供 UI 等外部系统绑定）
    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnHealthChanged OnHealthChanged;

    // 血量访问函数
    UFUNCTION(BlueprintCallable, Category = "Stats")
    float GetHealth() const { return Health; }

    UFUNCTION(BlueprintCallable, Category = "Stats")
    float GetMaxHealth() const { return MaxHealth; }

protected:
    // 属性变量
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
    float Health;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
    float MaxHealth;

    // 战斗组件
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Logic")
	class UCombatComponent* CombatComponent;

	// 武器网格体组件
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	class UStaticMeshComponent* SwordMesh;

	// 武器碰撞体组件（挂载在 SwordMesh 下，形状/大小可在蓝图中调整）
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	class UBoxComponent* SwordCollision;

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

	// 原始最大移动速度（BeginPlay 时记录）
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movement")
	float OriginalMaxWalkSpeed;

	// 受击减速恢复定时器句柄
	FTimerHandle SpeedResetTimerHandle;

	// 蒙太奇动画
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	class UAnimMontage* HitReactMontage;

	// 跳跃音效
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound")
	USoundBase* JumpSound;

	// 保存创建出的HUD实例，方便后续操作
	UPROPERTY()
	UUserWidget* HUDWidgetInstance;

	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	// 视角输入处理（Camera Input）
	void Look(const FInputActionValue& Value);

	// 移动输入处理（Movement Input）
	void Move(const FInputActionValue& Value);

	// 锁定敌人输入处理（Lock Input）
	void LockTarget();

	// 攻击输入处理（Attack Input）
	void Attack();

	// 翻滚输入处理（Dodge Input）
	void Dodge();

	// 根据输入值计算移动方向
	EMovementDirection GetMovementDirection(const FInputActionValue& Value);

	// 跳跃（重写基类 ACharacter::Jump）
	virtual void Jump() override;

	// 停止跳跃（重写基类 ACharacter::StopJumping）
	virtual void StopJumping() override;

	// 跳跃成功后的回调（播放音效）
	virtual void OnJumped_Implementation() override;

	// 死亡
	void Die();

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser) override;

};
