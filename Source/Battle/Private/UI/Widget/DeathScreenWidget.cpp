// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Widget/DeathScreenWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/BorderSlot.h"
#include "Components/TextBlock.h"

UDeathScreenWidget::UDeathScreenWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Message = NSLOCTEXT("Battle", "YouDied", "YOU DIED");
}

bool UDeathScreenWidget::Initialize()
{
	const bool bJustInitialized = Super::Initialize();

	// 纯 C++ 类创建时没有设计器布局，用代码搭建一套默认布局
	if (bJustInitialized && WidgetTree && !WidgetTree->RootWidget)
	{
		BuildDefaultLayout();
	}
	return bJustInitialized;
}

void UDeathScreenWidget::BuildDefaultLayout()
{
	// 全屏半透明黑底 + 居中暗红色大字
	UBorder* Background = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Background"));
	Background->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.55f));
	Background->SetHorizontalAlignment(HAlign_Center);
	Background->SetVerticalAlignment(VAlign_Center);
	WidgetTree->RootWidget = Background;

	MessageText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("MessageText"));
	FSlateFontInfo Font = MessageText->GetFont();
	Font.Size = 96;
	Font.LetterSpacing = 200;
	MessageText->SetFont(Font);
	MessageText->SetColorAndOpacity(FSlateColor(FLinearColor(0.55f, 0.04f, 0.04f)));
	MessageText->SetJustification(ETextJustify::Center);
	Background->SetContent(MessageText);
}

void UDeathScreenWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (MessageText)
	{
		MessageText->SetText(Message);
	}

	Elapsed = 0.0f;
	SetRenderOpacity(FadeInDuration > 0.0f ? 0.0f : 1.0f);
}

void UDeathScreenWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (FadeInDuration > 0.0f && Elapsed < FadeInDuration)
	{
		Elapsed += InDeltaTime;
		SetRenderOpacity(FMath::Clamp(Elapsed / FadeInDuration, 0.0f, 1.0f));
	}
}
