// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Widget/BossHealthWidget.h"
#include "Enemy/BossEnemy.h"
#include "Component/AttributeComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

bool UBossHealthWidget::Initialize()
{
	const bool bJustInitialized = Super::Initialize();

	// 纯 C++ 类创建时没有设计器布局，用代码搭建一套默认布局
	if (bJustInitialized && WidgetTree && !WidgetTree->RootWidget)
	{
		BuildDefaultLayout();
	}
	return bJustInitialized;
}

void UBossHealthWidget::BuildDefaultLayout()
{
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("BossBox"));
	UCanvasPanelSlot* BoxSlot = Root->AddChildToCanvas(Box);
	BoxSlot->SetAnchors(FAnchors(0.5f, 1.0f));      // 底部居中
	BoxSlot->SetAlignment(FVector2D(0.5f, 1.0f));
	BoxSlot->SetPosition(FVector2D(0.0f, -70.0f));
	BoxSlot->SetSize(FVector2D(900.0f, 60.0f));

	// Boss 名字
	BossNameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("BossNameText"));
	FSlateFontInfo Font = BossNameText->GetFont();
	Font.Size = 20;
	BossNameText->SetFont(Font);
	BossNameText->SetColorAndOpacity(FSlateColor(FLinearColor(0.92f, 0.88f, 0.78f)));
	BossNameText->SetShadowOffset(FVector2D(1.0f, 1.0f));
	if (UVerticalBoxSlot* NameSlot = Box->AddChildToVerticalBox(BossNameText))
	{
		NameSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));
	}

	// 血条：延迟条在下层，红色血条在上层（背景透明）
	USizeBox* BarSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("BarSize"));
	BarSize->SetHeightOverride(14.0f);
	Box->AddChildToVerticalBox(BarSize);

	UOverlay* BarOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("BarOverlay"));
	BarSize->AddChild(BarOverlay);

	HealthTrailBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("HealthTrailBar"));
	HealthTrailBar->SetFillColorAndOpacity(FLinearColor(0.85f, 0.65f, 0.15f));
	HealthTrailBar->SetPercent(1.0f);
	if (UOverlaySlot* TrailSlot = BarOverlay->AddChildToOverlay(HealthTrailBar))
	{
		TrailSlot->SetHorizontalAlignment(HAlign_Fill);
		TrailSlot->SetVerticalAlignment(VAlign_Fill);
	}

	HealthBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("HealthBar"));
	HealthBar->SetFillColorAndOpacity(FLinearColor(0.55f, 0.05f, 0.05f));
	HealthBar->SetPercent(1.0f);
	FProgressBarStyle TopStyle = HealthBar->GetWidgetStyle();
	TopStyle.BackgroundImage.TintColor = FSlateColor(FLinearColor::Transparent);
	HealthBar->SetWidgetStyle(TopStyle);
	if (UOverlaySlot* HealthSlot = BarOverlay->AddChildToOverlay(HealthBar))
	{
		HealthSlot->SetHorizontalAlignment(HAlign_Fill);
		HealthSlot->SetVerticalAlignment(VAlign_Fill);
	}
}

void UBossHealthWidget::InitWithBoss(ABossEnemy* InBoss)
{
	if (!InBoss || !InBoss->AttributeComponent)
	{
		return;
	}

	Boss = InBoss;

	if (BossNameText)
	{
		BossNameText->SetText(InBoss->BossName);
	}

	InBoss->AttributeComponent->OnHealthChanged.AddUniqueDynamic(this, &UBossHealthWidget::HandleHealthChanged);

	TargetPercent = InBoss->AttributeComponent->GetHealthPercent();
	TrailPercent = TargetPercent;
	TimeSinceDamage = TrailDelay;
	if (HealthBar)
	{
		HealthBar->SetPercent(TargetPercent);
	}
	if (HealthTrailBar)
	{
		HealthTrailBar->SetPercent(TrailPercent);
	}
}

void UBossHealthWidget::HandleHealthChanged(float CurrentHealth, float MaxHealth)
{
	const float NewPercent = MaxHealth > 0.0f ? CurrentHealth / MaxHealth : 0.0f;
	if (NewPercent < TargetPercent)
	{
		TimeSinceDamage = 0.0f;
	}
	TargetPercent = NewPercent;

	if (HealthBar)
	{
		HealthBar->SetPercent(TargetPercent);
	}

	// 回血时延迟条直接跟上
	if (TrailPercent < TargetPercent)
	{
		TrailPercent = TargetPercent;
	}
}

void UBossHealthWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	TimeSinceDamage += InDeltaTime;
	if (TrailPercent > TargetPercent && TimeSinceDamage >= TrailDelay)
	{
		TrailPercent = FMath::Max(TargetPercent, TrailPercent - TrailSpeed * InDeltaTime);
	}

	if (HealthTrailBar)
	{
		HealthTrailBar->SetPercent(TrailPercent);
	}
}

void UBossHealthWidget::NativeDestruct()
{
	if (ABossEnemy* BossPtr = Boss.Get())
	{
		if (BossPtr->AttributeComponent)
		{
			BossPtr->AttributeComponent->OnHealthChanged.RemoveDynamic(this, &UBossHealthWidget::HandleHealthChanged);
		}
	}

	Super::NativeDestruct();
}
