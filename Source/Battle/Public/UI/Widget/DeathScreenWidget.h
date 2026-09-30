// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DeathScreenWidget.generated.h"

class UTextBlock;
class UBorder;

/**
 * 死亡界面（"YOU DIED"），淡入显示
 * 可直接使用 C++ 类（自动搭建默认布局），也可派生蓝图并放置同名控件 MessageText 自定义外观
 */
UCLASS()
class BATTLE_API UDeathScreenWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MessageText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Death Screen")
	FText Message;

	// 淡入时长（秒）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Death Screen", meta = (ClampMin = "0.0"))
	float FadeInDuration = 1.2f;

	UDeathScreenWidget(const FObjectInitializer& ObjectInitializer);

	virtual bool Initialize() override;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void BuildDefaultLayout();

	float Elapsed = 0.0f;
};
