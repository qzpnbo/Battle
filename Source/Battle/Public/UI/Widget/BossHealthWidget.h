// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BossHealthWidget.generated.h"

class UProgressBar;
class UTextBlock;
class ABossEnemy;

/**
 * 屏幕底部 Boss 血条（类魂风格：Boss 名 + 红色血条 + 黄色延迟掉血条）
 *
 * 可以直接用 C++ 类创建（会自动搭建默认布局），也可以派生蓝图自定义外观：
 * 蓝图中放置同名控件（HealthBar / HealthTrailBar / BossNameText）即可替换默认布局。
 */
UCLASS()
class BATTLE_API UBossHealthWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> HealthBar;

	// 延迟掉血条（受击后停顿一下再缓慢追上当前血量）
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> HealthTrailBar;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> BossNameText;

	// 受击后延迟条开始追赶的等待时间（秒）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss UI")
	float TrailDelay = 0.6f;

	// 延迟条追赶速度（每秒百分比）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss UI")
	float TrailSpeed = 0.5f;

	UFUNCTION(BlueprintCallable, Category = "Boss UI")
	void InitWithBoss(ABossEnemy* InBoss);

	virtual bool Initialize() override;

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeDestruct() override;

	UFUNCTION()
	void HandleHealthChanged(float CurrentHealth, float MaxHealth);

private:
	// 蓝图未提供布局时，用代码搭建默认布局
	void BuildDefaultLayout();

	TWeakObjectPtr<ABossEnemy> Boss;

	float TargetPercent = 1.0f;
	float TrailPercent = 1.0f;
	float TimeSinceDamage = 0.0f;
};
