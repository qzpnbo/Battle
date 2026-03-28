// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "EnemyHealthWidget.generated.h"

class UProgressBar;
class UTextBlock;
class AEnemy;

/**
 * 敌人头顶血条 Widget
 */
UCLASS()
class BATTLE_API UEnemyHealthWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 血条进度条（需在蓝图中绑定同名控件）
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> HealthBar;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> HealthText;

	// 由 AEnemy 调用，传入自身引用以完成初始化和委托绑定
	UFUNCTION(BlueprintCallable, Category = "UI")
	void InitWithOwner(AEnemy* InOwner);

	// 更新血条显示
	UFUNCTION(BlueprintCallable, Category = "UI")
	void UpdateHealthUI(float CurrentHealth, float MaxHealth);

protected:
	// 监听敌人血量变化的回调
	UFUNCTION()
	void HandleEnemyHealthChanged(float CurrentHealth, float MaxHealth);

private:
	// 拥有此血条的敌人引用
	TWeakObjectPtr<AEnemy> OwnerEnemy;
};
