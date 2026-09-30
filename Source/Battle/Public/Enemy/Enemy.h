// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Character/BattleCharacterBase.h"
#include "Enemy.generated.h"

class UWidgetComponent;

/**
 * 普通敌人：头顶血条、死亡时停止 AI、尸体延迟销毁，并注册到重生系统（玩家死亡后重置）
 */
UCLASS()
class BATTLE_API AEnemy : public ABattleCharacterBase
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	AEnemy();

	// --- 头顶血条 Widget 组件 ---
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI")
	class UWidgetComponent* HealthWidgetComp;

	// 死亡后尸体保留时间（秒），到时自动销毁；<= 0 表示永久保留
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float CorpseLifeSpan = 10.0f;

	// 玩家死亡重生时，已被击杀的该敌人是否复活（普通敌人复活，Boss 击杀后永久死亡）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Respawn")
	bool bRespawnAfterDeath = true;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	virtual void Die(AActor* Killer) override;
};
