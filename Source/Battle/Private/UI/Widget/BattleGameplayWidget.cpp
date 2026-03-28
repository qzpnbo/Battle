// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/Widget/BattleGameplayWidget.h"
#include "Character/BattleCharacter.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Kismet/GameplayStatics.h"

void UBattleGameplayWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 获取玩家角色并绑定 OnHealthChanged 委托
	ABattleCharacter* PlayerCharacter = Cast<ABattleCharacter>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0));
	if (PlayerCharacter)
	{
		PlayerCharacter->OnHealthChanged.AddDynamic(this, &UBattleGameplayWidget::HandleHealthChanged);

		// 初始化血条显示（使用角色当前血量）
		UpdateHealthUI(PlayerCharacter->GetHealth(), PlayerCharacter->GetMaxHealth());
	}
}

void UBattleGameplayWidget::HandleHealthChanged(float CurrentHealth, float MaxHealth)
{
	UpdateHealthUI(CurrentHealth, MaxHealth);
}

void UBattleGameplayWidget::UpdateHealthUI(float CurrentHealth, float MaxHealth)
{
	if (HealthBar)
	{
		// 计算血量百分比 (0.0 ~ 1.0)
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
