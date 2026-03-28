// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BattleGameplayWidget.generated.h"

class UProgressBar;
class UTextBlock;

/**
 * 战斗玩法主界面 Widget（血条、技能栏、小地图等）
 */
UCLASS()
class BATTLE_API UBattleGameplayWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 血条进度条（需在蓝图中绑定同名控件）
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> HealthBar;

	// 血量文本显示（需在蓝图中绑定同名控件）
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> HealthText;

protected:
	virtual void NativeConstruct() override;

	// 监听 FOnHealthChanged 委托的回调函数
	UFUNCTION()
	void HandleHealthChanged(float CurrentHealth, float MaxHealth);

private:
	// 更新血条 UI 显示
	void UpdateHealthUI(float CurrentHealth, float MaxHealth);
};
