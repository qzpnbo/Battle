// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/BattleHUD.h"

void ABattleHUD::BeginPlay()
{
    Super::BeginPlay();

    if (GameplayWidgetClass)
    {
        // 以玩家控制器为 Owner 创建，Widget 内可通过 GetOwningPlayer 监听 Pawn 切换（重生后自动重新绑定）
        APlayerController* OwningPC = GetOwningPlayerController();
        GameplayWidgetInstance = OwningPC
            ? CreateWidget<UUserWidget>(OwningPC, GameplayWidgetClass)
            : CreateWidget<UUserWidget>(GetWorld(), GameplayWidgetClass);
        if (GameplayWidgetInstance)
        {
            GameplayWidgetInstance->AddToViewport();
        }
    }
}
