// Fill out your copyright notice in the Description page of Project Settings.

#include "Game/BattleBGMSubsystem.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "TimerManager.h"

bool UBattleBGMSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	// 仅在游戏世界和 PIE（编辑器内运行）中启用
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UBattleBGMSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	if (bAutoPlayOnStart)
	{
		PlayExplorationBGM(DefaultVolume);
	}
}

void UBattleBGMSubsystem::Deinitialize()
{
	StopBGM(0.f);

	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(FadeOutTimerHandle);
	}

	Super::Deinitialize();
}

void UBattleBGMSubsystem::PlayBGM(USoundBase* Music, float Volume, float FadeInDuration)
{
	if (!Music)
	{
		return;
	}

	// 如果当前已有 BGM 在播放，先淡出再切换
	if (BGMComponent && BGMComponent->IsPlaying())
	{
		PendingMusic = Music;
		PendingVolume = Volume;
		PendingFadeInDuration = FadeInDuration;

		const float FadeOutDuration = 1.5f;
		BGMComponent->FadeOut(FadeOutDuration, 0.f);

		// 淡出完成后执行切换
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				FadeOutTimerHandle,
				this,
				&UBattleBGMSubsystem::OnFadeOutFinished,
				FadeOutDuration,
				false
			);
		}
		return;
	}

	// 直接播放新 BGM
	BGMComponent = CreateBGMComponent(Music, Volume);
	if (BGMComponent)
	{
		if (FadeInDuration > 0.f)
		{
			BGMComponent->FadeIn(FadeInDuration, Volume);
		}
		else
		{
			BGMComponent->Play();
		}
	}
}

void UBattleBGMSubsystem::StopBGM(float FadeOutDuration)
{
	PendingMusic = nullptr;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FadeOutTimerHandle);
	}

	if (BGMComponent)
	{
		if (FadeOutDuration > 0.f)
		{
			BGMComponent->FadeOut(FadeOutDuration, 0.f);
		}
		else
		{
			BGMComponent->Stop();
		}
	}
}

void UBattleBGMSubsystem::SetBGMPaused(bool bPaused)
{
	if (BGMComponent)
	{
		BGMComponent->SetPaused(bPaused);
	}
}

void UBattleBGMSubsystem::SetBGMVolume(float Volume)
{
	if (BGMComponent)
	{
		BGMComponent->SetVolumeMultiplier(FMath::Clamp(Volume, 0.f, 1.f));
	}
}

bool UBattleBGMSubsystem::IsBGMPlaying() const
{
	return BGMComponent && BGMComponent->IsPlaying();
}

void UBattleBGMSubsystem::OnFadeOutFinished()
{
	// 销毁旧的 BGM 组件
	if (BGMComponent)
	{
		BGMComponent->Stop();
		BGMComponent->DestroyComponent();
		BGMComponent = nullptr;
	}

	// 播放新曲目
	if (PendingMusic)
	{
		USoundBase* Music = PendingMusic;
		PendingMusic = nullptr;

		BGMComponent = CreateBGMComponent(Music, PendingVolume);
		if (BGMComponent)
		{
			if (PendingFadeInDuration > 0.f)
			{
				BGMComponent->FadeIn(PendingFadeInDuration, PendingVolume);
			}
			else
			{
				BGMComponent->Play();
			}
		}
	}
}

void UBattleBGMSubsystem::PlayBossBGM(float Volume, float FadeInDuration)
{
	if (BossBGMPath.IsValid())
	{
		USoundBase* Music = Cast<USoundBase>(BossBGMPath.TryLoad());
		if (Music)
		{
			PlayBGM(Music, Volume, FadeInDuration);
		}
	}
}

void UBattleBGMSubsystem::PlayExplorationBGM(float Volume, float FadeInDuration)
{
	if (ExplorationBGMPath.IsValid())
	{
		USoundBase* Music = Cast<USoundBase>(ExplorationBGMPath.TryLoad());
		if (Music)
		{
			PlayBGM(Music, Volume, FadeInDuration);
		}
	}
}

UAudioComponent* UBattleBGMSubsystem::CreateBGMComponent(USoundBase* Music, float Volume)
{
	UWorld* World = GetWorld();
	if (!World || !Music)
	{
		return nullptr;
	}

	// 使用 2D 音效（非空间化），确保 BGM 全局均匀播放
	UAudioComponent* NewComp = UGameplayStatics::CreateSound2D(
		World,
		Music,
		Volume,
		1.0f,   // Pitch
		0,       // StartTime
		nullptr, // ConcurrencySettings
		false,   // bAutoDestroy — 我们手动管理生命周期
		true     // bPersistAcrossLevelTransition — 切关卡不中断
	);

	return NewComp;
}
