// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Enemy.generated.h"

class UWidgetComponent;
class UStaticMeshComponent;
class UBoxComponent;
class UCombatComponent;

// 敌人血量变化委托（供血条 Widget 绑定）
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEnemyHealthChanged, float, CurrentHealth, float, MaxHealth);

UCLASS()
class BATTLE_API AEnemy : public ACharacter
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	AEnemy();

	// --- 头顶血条 Widget 组件 ---
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI")
	class UWidgetComponent* HealthWidgetComp;

	// --- 血量系统 ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float MaxHealth = 100.0f;

	UFUNCTION(BlueprintCallable, Category = "Stats")
	float GetHealth() const { return Health; }

	UFUNCTION(BlueprintCallable, Category = "Stats")
	float GetMaxHealth() const { return MaxHealth; }

	// 血量变化委托
	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnEnemyHealthChanged OnHealthChanged;

	// --- 战斗组件 ---
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Logic")
	UCombatComponent* CombatComponent;

	// --- 武器组件 ---
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	UStaticMeshComponent* SwordMesh;

	// 武器碰撞体组件（挂载在 SwordMesh 下，形状/大小可在蓝图中调整）
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	UBoxComponent* SwordCollision;

	// 受伤处理
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser) override;

protected:
	float Health;

	// 是否已死亡
	bool bIsDead = false;

	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	// 死亡处理
	void Die();

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;
};
