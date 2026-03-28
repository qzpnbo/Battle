// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/BattleHUD.h"

void ABattleHUD::BeginPlay()
{
    Super::BeginPlay();

    if (GameplayWidgetClass)
    {
        GameplayWidgetInstance = CreateWidget<UUserWidget>(GetWorld(), GameplayWidgetClass);
        if (GameplayWidgetInstance)
        {
            GameplayWidgetInstance->AddToViewport();
        }
    }
}
