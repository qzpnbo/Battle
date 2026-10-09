// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "BattleBGMSubsystem.generated.h"

class USoundBase;
class UAudioComponent;

/**
 * 全局背景音乐管理（World Subsystem，随关卡自动创建）
 *
 * 功能：播放/停止/切换 BGM，支持淡入淡出，适用于 Boss 战切歌等场景
 *
 * 用法（蓝图 / C++）：
 *   GetWorld()->GetSubsystem<UBattleBGMSubsystem>()->PlayBGM(Music);
 */
UCLASS()
class BATTLE_API UBattleBGMSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** 播放背景音乐（若已有 BGM 则先淡出再切换） */
	UFUNCTION(BlueprintCallable, Category = "Audio|BGM")
	void PlayBGM(USoundBase* Music, float Volume = 0.4f, float FadeInDuration = 2.0f);

	/** 停止当前 BGM（淡出） */
	UFUNCTION(BlueprintCallable, Category = "Audio|BGM")
	void StopBGM(float FadeOutDuration = 2.0f);

	/** 暂停 / 恢复 BGM */
	UFUNCTION(BlueprintCallable, Category = "Audio|BGM")
	void SetBGMPaused(bool bPaused);

	/** 调节 BGM 音量（立即生效） */
	UFUNCTION(BlueprintCallable, Category = "Audio|BGM")
	void SetBGMVolume(float Volume);

	/** 当前是否正在播放 BGM */
	UFUNCTION(BlueprintPure, Category = "Audio|BGM")
	bool IsBGMPlaying() const;

	/** 探索 BGM 资产路径（关卡开始时自动播放） */
	UPROPERTY(BlueprintReadWrite, Category = "Audio|BGM")
	FSoftObjectPath ExplorationBGMPath = FSoftObjectPath(TEXT("/Game/Audio/BGM_Exploration.BGM_Exploration"));

	/** Boss 战 BGM 资产路径 */
	UPROPERTY(BlueprintReadWrite, Category = "Audio|BGM")
	FSoftObjectPath BossBGMPath = FSoftObjectPath(TEXT("/Game/Audio/BGM_Boss.BGM_Boss"));

	/** 默认音量 */
	UPROPERTY(BlueprintReadWrite, Category = "Audio|BGM", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DefaultVolume = 0.4f;

	/** 是否在关卡开始时自动播放探索 BGM */
	UPROPERTY(BlueprintReadWrite, Category = "Audio|BGM")
	bool bAutoPlayOnStart = true;

	/** 切换到 Boss 战 BGM（淡出探索曲 → 淡入 Boss 曲） */
	UFUNCTION(BlueprintCallable, Category = "Audio|BGM")
	void PlayBossBGM(float Volume = 0.5f, float FadeInDuration = 1.0f);

	/** 切回探索 BGM（Boss 击败后调用） */
	UFUNCTION(BlueprintCallable, Category = "Audio|BGM")
	void PlayExplorationBGM(float Volume = 0.4f, float FadeInDuration = 3.0f);

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> BGMComponent;

	/** 切换 BGM 时，等待淡出完成后播放新曲目 */
	UPROPERTY(Transient)
	TObjectPtr<USoundBase> PendingMusic;

	float PendingVolume = 0.4f;
	float PendingFadeInDuration = 2.0f;
	FTimerHandle FadeOutTimerHandle;

	void OnFadeOutFinished();
	UAudioComponent* CreateBGMComponent(USoundBase* Music, float Volume);
};
