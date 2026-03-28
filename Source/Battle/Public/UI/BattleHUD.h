// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "Blueprint/UserWidget.h"
#include "BattleHUD.generated.h"

/**
 * 
 */
UCLASS()
class BATTLE_API ABattleHUD : public AHUD
{
	GENERATED_BODY()
	
protected:
    virtual void BeginPlay() override;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
    TSubclassOf<UUserWidget> GameplayWidgetClass;

    UPROPERTY()
    UUserWidget* GameplayWidgetInstance;

};
