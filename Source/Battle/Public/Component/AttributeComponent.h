// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AttributeComponent.generated.h"

// 属性变化委托（当前值、最大值），血量/耐力共用
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAttributeChanged, float, CurrentValue, float, MaxValue);

// 韧性被击破委托
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPoiseBroken);

/**
 * 角色属性组件：统一管理血量、耐力、韧性（玩家与敌人共用）
 *
 * 设计要点：
 *  - 只负责"数值"，不关心动作/动画；何时消耗、何时暂停回复由 UCombatComponent 决定
 *  - 耐力遵循类魂规则：只要耐力 > 0 就允许出招，出招后可扣到 0（不会因为差一点耐力而无法翻滚）
 *  - 韧性：受到韧性伤害累积到 0 即"破韧"（进入硬直），一段时间未受击后自动回满
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class BATTLE_API UAttributeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAttributeComponent();

	// ============================================================================
	// 血量
	// ============================================================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attributes|Health", meta = (ClampMin = "1.0"))
	float MaxHealth = 100.0f;

	UPROPERTY(BlueprintAssignable, Category = "Attributes|Health")
	FOnAttributeChanged OnHealthChanged;

	UFUNCTION(BlueprintPure, Category = "Attributes|Health")
	float GetHealth() const { return Health; }

	UFUNCTION(BlueprintPure, Category = "Attributes|Health")
	float GetMaxHealth() const { return MaxHealth; }

	UFUNCTION(BlueprintPure, Category = "Attributes|Health")
	float GetHealthPercent() const { return MaxHealth > 0.0f ? Health / MaxHealth : 0.0f; }

	UFUNCTION(BlueprintPure, Category = "Attributes|Health")
	bool IsAlive() const { return Health > 0.0f; }

	// 扣血，返回实际扣除量
	UFUNCTION(BlueprintCallable, Category = "Attributes|Health")
	float ApplyHealthDamage(float Amount);

	UFUNCTION(BlueprintCallable, Category = "Attributes|Health")
	void Heal(float Amount);

	// ============================================================================
	// 耐力
	// ============================================================================

	// 是否启用耐力（AI 敌人通常关闭，行为只受招式节奏限制）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attributes|Stamina")
	bool bUseStamina = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attributes|Stamina", meta = (ClampMin = "1.0", EditCondition = "bUseStamina"))
	float MaxStamina = 100.0f;

	// 每秒回复量
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attributes|Stamina", meta = (ClampMin = "0.0", EditCondition = "bUseStamina"))
	float StaminaRegenRate = 40.0f;

	// 消耗耐力后多久开始回复（秒）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attributes|Stamina", meta = (ClampMin = "0.0", EditCondition = "bUseStamina"))
	float StaminaRegenDelay = 0.8f;

	UPROPERTY(BlueprintAssignable, Category = "Attributes|Stamina")
	FOnAttributeChanged OnStaminaChanged;

	UFUNCTION(BlueprintPure, Category = "Attributes|Stamina")
	float GetStamina() const { return Stamina; }

	UFUNCTION(BlueprintPure, Category = "Attributes|Stamina")
	float GetMaxStamina() const { return MaxStamina; }

	UFUNCTION(BlueprintPure, Category = "Attributes|Stamina")
	float GetStaminaPercent() const { return MaxStamina > 0.0f ? Stamina / MaxStamina : 0.0f; }

	// 是否有耐力可以发动动作（类魂规则：> 0 即可）
	UFUNCTION(BlueprintPure, Category = "Attributes|Stamina")
	bool HasStamina() const { return !bUseStamina || Stamina > 0.0f; }

	// 尝试消耗耐力：耐力 <= 0 时失败；否则扣除（最多扣到 0）并重置回复延迟
	UFUNCTION(BlueprintCallable, Category = "Attributes|Stamina")
	bool TryConsumeStamina(float Cost);

	// 强制扣除耐力（格挡承伤用），返回扣除后是否仍有耐力（false 表示被打空 → 破防）
	UFUNCTION(BlueprintCallable, Category = "Attributes|Stamina")
	bool DrainStamina(float Amount);

	// 暂停/恢复耐力回复（攻击、翻滚期间暂停）
	void SetStaminaRegenPaused(bool bPaused);

	// 回复速率倍率（格挡时降低）
	void SetStaminaRegenMultiplier(float Multiplier) { StaminaRegenMultiplier = FMath::Max(0.0f, Multiplier); }

	// ============================================================================
	// 韧性（Poise）
	// ============================================================================

	// 最大韧性：普通角色较低（几乎每刀都会硬直），Boss 较高（霸体）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attributes|Poise", meta = (ClampMin = "0.0"))
	float MaxPoise = 20.0f;

	// 多久未受到韧性伤害后韧性回满（秒）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attributes|Poise", meta = (ClampMin = "0.0"))
	float PoiseResetDelay = 3.0f;

	UPROPERTY(BlueprintAssignable, Category = "Attributes|Poise")
	FOnPoiseBroken OnPoiseBroken;

	UFUNCTION(BlueprintPure, Category = "Attributes|Poise")
	float GetPoise() const { return Poise; }

	// 施加韧性伤害，返回是否破韧（破韧后韧性立即回满，开始新一轮累积）
	UFUNCTION(BlueprintCallable, Category = "Attributes|Poise")
	bool ApplyPoiseDamage(float Amount);

	UFUNCTION(BlueprintCallable, Category = "Attributes|Poise")
	void ResetPoise() { Poise = MaxPoise; }

	// ============================================================================
	// 通用
	// ============================================================================

	// 恢复全部属性（检查点休息、重生）
	UFUNCTION(BlueprintCallable, Category = "Attributes")
	void RestoreAll();

	virtual void InitializeComponent() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;

private:
	void SetStamina(float NewStamina);
	float GetWorldTime() const;

	float Health = 0.0f;
	float Stamina = 0.0f;
	float Poise = 0.0f;

	float LastStaminaUseTime = -1000.0f;
	float LastPoiseDamageTime = -1000.0f;
	float StaminaRegenMultiplier = 1.0f;
	bool bStaminaRegenPaused = false;
};
