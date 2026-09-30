// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "BattleCharacterBase.generated.h"

class UCombatComponent;
class UAttributeComponent;
class UStaticMeshComponent;
class UBoxComponent;
class ABattleCharacterBase;

// 角色死亡委托（死亡角色、击杀者）
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCharacterDied, ABattleCharacterBase*, DeadCharacter, AActor*, Killer);

/**
 * 战斗角色基类（玩家与敌人共用）
 *
 * 负责：战斗/属性组件、武器组件、统一的受伤管线与死亡流程。
 * 受伤管线：TakeDamage → CombatComponent::ResolveIncomingHit（无敌帧/弹反/格挡/韧性）→ AttributeComponent 扣血 → Die
 */
UCLASS(Abstract)
class BATTLE_API ABattleCharacterBase : public ACharacter
{
	GENERATED_BODY()

public:
	ABattleCharacterBase();

	// 战斗组件（状态机、攻击、翻滚、格挡、受击）
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Logic")
	TObjectPtr<UCombatComponent> CombatComponent;

	// 属性组件（血量、耐力、韧性）
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Logic")
	TObjectPtr<UAttributeComponent> AttributeComponent;

	// 武器网格体
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<UStaticMeshComponent> SwordMesh;

	// 武器碰撞体（挂载在 SwordMesh 下，形状/大小可在蓝图中调整）
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<UBoxComponent> SwordCollision;

	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnCharacterDied OnDied;

	UFUNCTION(BlueprintPure, Category = "Stats")
	bool IsDead() const { return bIsDead; }

	UFUNCTION(BlueprintCallable, Category = "Stats")
	float GetHealth() const;

	UFUNCTION(BlueprintCallable, Category = "Stats")
	float GetMaxHealth() const;

	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser) override;

	virtual void PostLoad() override;
	virtual void PostInitializeComponents() override;

protected:
	virtual void BeginPlay() override;

	// 死亡处理（子类可扩展：停止 AI、显示死亡界面等）
	virtual void Die(AActor* Killer);

	bool bIsDead = false;

	// 旧版本直接定义在角色上的最大血量，已迁移到 AttributeComponent->MaxHealth。
	// 保留该属性仅用于读取旧资产中的数值并自动迁移（反射名仍为 "MaxHealth"）
	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "已迁移到 AttributeComponent->MaxHealth"))
	float MaxHealth_DEPRECATED = 0.0f;

private:
	void MigrateDeprecatedProperties();
};
