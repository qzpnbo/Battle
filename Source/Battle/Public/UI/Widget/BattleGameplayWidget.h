// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BattleGameplayWidget.generated.h"

class UProgressBar;
class UTextBlock;
class UAttributeComponent;
class APawn;

/**
 * 战斗玩法主界面 Widget（血条、耐力条）
 *
 * 绑定的是玩家当前 Pawn 的 AttributeComponent，并监听控制器的 Pawn 切换，
 * 玩家死亡重生（生成新 Pawn）后会自动重新绑定。
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

	// 耐力条（可选）：蓝图中没有同名控件时，运行时会在血条下方自动创建一个
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> StaminaBar;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// 监听属性组件血量变化
	UFUNCTION()
	void HandleHealthChanged(float CurrentHealth, float MaxHealth);

	// 监听属性组件耐力变化
	UFUNCTION()
	void HandleStaminaChanged(float CurrentStamina, float MaxStamina);

	// 玩家控制器切换 Pawn（重生）时重新绑定
	UFUNCTION()
	void HandlePossessedPawnChanged(APawn* OldPawn, APawn* NewPawn);

private:
	void BindToPawn(APawn* Pawn);
	void UnbindFromAttributes();

	// 蓝图中没有 StaminaBar 时，在 HealthBar 下方运行时创建
	void EnsureStaminaBar();

	// 更新血条 UI 显示
	void UpdateHealthUI(float CurrentHealth, float MaxHealth);

	TWeakObjectPtr<UAttributeComponent> BoundAttributes;
	TWeakObjectPtr<APlayerController> BoundController;
};
