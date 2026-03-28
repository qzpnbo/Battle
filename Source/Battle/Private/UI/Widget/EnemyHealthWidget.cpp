// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Widget/EnemyHealthWidget.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Enemy/Enemy.h"

void UEnemyHealthWidget::InitWithOwner(AEnemy* InOwner)
{
	if (!InOwner)
	{
		UE_LOG(LogTemp, Error, TEXT("[EnemyHealthWidget] InitWithOwner: InOwner 为空！"));
		return;
	}

	OwnerEnemy = InOwner;

	UE_LOG(LogTemp, Warning, TEXT("[EnemyHealthWidget] 成功绑定到 Enemy: %s"), *InOwner->GetName());

	// 绑定委托：血量变化时自动更新血条
	InOwner->OnHealthChanged.AddDynamic(this, &UEnemyHealthWidget::HandleEnemyHealthChanged);

	// 初始化血条显示
	UpdateHealthUI(InOwner->GetHealth(), InOwner->GetMaxHealth());
}

void UEnemyHealthWidget::HandleEnemyHealthChanged(float CurrentHealth, float MaxHealth)
{
	UpdateHealthUI(CurrentHealth, MaxHealth);
}

void UEnemyHealthWidget::UpdateHealthUI(float CurrentHealth, float MaxHealth)
{
	if (HealthBar)
	{
		const float HealthPercent = MaxHealth > 0.0f ? CurrentHealth / MaxHealth : 0.0f;
		HealthBar->SetPercent(HealthPercent);
	}

	if (HealthText)
	{
		// 显示格式: "80 / 100"
		const FString HealthString = FString::Printf(TEXT("%.0f / %.0f"), CurrentHealth, MaxHealth);
		HealthText->SetText(FText::FromString(HealthString));
	}
}
