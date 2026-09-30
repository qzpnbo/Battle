// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/Widget/BattleGameplayWidget.h"
#include "Component/AttributeComponent.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Blueprint/WidgetTree.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"

void UBattleGameplayWidget::NativeConstruct()
{
	Super::NativeConstruct();

	EnsureStaminaBar();

	APlayerController* PC = GetOwningPlayer();
	if (!PC)
	{
		PC = UGameplayStatics::GetPlayerController(this, 0);
	}

	if (PC)
	{
		BoundController = PC;
		PC->OnPossessedPawnChanged.AddUniqueDynamic(this, &UBattleGameplayWidget::HandlePossessedPawnChanged);
		BindToPawn(PC->GetPawn());
	}
}

void UBattleGameplayWidget::NativeDestruct()
{
	UnbindFromAttributes();

	if (APlayerController* PC = BoundController.Get())
	{
		PC->OnPossessedPawnChanged.RemoveDynamic(this, &UBattleGameplayWidget::HandlePossessedPawnChanged);
	}

	Super::NativeDestruct();
}

void UBattleGameplayWidget::HandlePossessedPawnChanged(APawn* OldPawn, APawn* NewPawn)
{
	BindToPawn(NewPawn);
}

void UBattleGameplayWidget::BindToPawn(APawn* Pawn)
{
	UnbindFromAttributes();

	UAttributeComponent* Attributes = Pawn ? Pawn->FindComponentByClass<UAttributeComponent>() : nullptr;
	if (!Attributes)
	{
		return;
	}

	BoundAttributes = Attributes;
	Attributes->OnHealthChanged.AddUniqueDynamic(this, &UBattleGameplayWidget::HandleHealthChanged);
	Attributes->OnStaminaChanged.AddUniqueDynamic(this, &UBattleGameplayWidget::HandleStaminaChanged);

	// 立即刷新一次（不依赖委托广播时序）
	UpdateHealthUI(Attributes->GetHealth(), Attributes->GetMaxHealth());
	HandleStaminaChanged(Attributes->GetStamina(), Attributes->GetMaxStamina());
}

void UBattleGameplayWidget::UnbindFromAttributes()
{
	if (UAttributeComponent* Attributes = BoundAttributes.Get())
	{
		Attributes->OnHealthChanged.RemoveDynamic(this, &UBattleGameplayWidget::HandleHealthChanged);
		Attributes->OnStaminaChanged.RemoveDynamic(this, &UBattleGameplayWidget::HandleStaminaChanged);
	}
	BoundAttributes.Reset();
}

void UBattleGameplayWidget::HandleHealthChanged(float CurrentHealth, float MaxHealth)
{
	UpdateHealthUI(CurrentHealth, MaxHealth);
}

void UBattleGameplayWidget::HandleStaminaChanged(float CurrentStamina, float MaxStamina)
{
	if (StaminaBar)
	{
		StaminaBar->SetPercent(MaxStamina > 0.0f ? CurrentStamina / MaxStamina : 0.0f);
	}
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

void UBattleGameplayWidget::EnsureStaminaBar()
{
	if (StaminaBar || !HealthBar || !WidgetTree)
	{
		return;
	}

	UPanelWidget* Parent = HealthBar->GetParent();
	if (!Parent)
	{
		return;
	}

	UProgressBar* NewBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("StaminaBar_Auto"));
	NewBar->SetFillColorAndOpacity(FLinearColor(0.2f, 0.75f, 0.3f, 1.0f));
	NewBar->SetPercent(1.0f);

	UCanvasPanel* Canvas = Cast<UCanvasPanel>(Parent);
	UCanvasPanelSlot* HealthSlot = Cast<UCanvasPanelSlot>(HealthBar->Slot);
	if (Canvas && HealthSlot)
	{
		UCanvasPanelSlot* NewSlot = Canvas->AddChildToCanvas(NewBar);
		NewSlot->SetAnchors(HealthSlot->GetAnchors());
		NewSlot->SetAlignment(HealthSlot->GetAlignment());
		NewSlot->SetZOrder(HealthSlot->GetZOrder());

		// 非纵向拉伸时：Offsets.Top = Y 位置，Offsets.Bottom = 高度；耐力条放在血条正下方，高度为血条的 60%
		FMargin Offsets = HealthSlot->GetOffsets();
		if (!HealthSlot->GetAnchors().IsStretchedVertical())
		{
			const float Gap = 6.0f;
			const float HealthHeight = Offsets.Bottom;
			Offsets.Top += HealthHeight + Gap;
			Offsets.Bottom = FMath::Max(HealthHeight * 0.6f, 8.0f);
		}
		NewSlot->SetOffsets(Offsets);
	}
	else
	{
		// 其他容器（VerticalBox 等）直接追加在末尾
		Parent->AddChild(NewBar);
	}

	StaminaBar = NewBar;
}
