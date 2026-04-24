// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "AnimNotifyState_Invincibility.generated.h"

/**
 * 翻滚无敌帧通知状态
 * 在 NotifyBegin 时开启无敌帧，NotifyEnd 时关闭
 * 即使蒙太奇被中断，NotifyEnd 也会被调用，确保无敌帧不会永久开启
 */
UCLASS()
class BATTLE_API UAnimNotifyState_Invincibility : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	// 通知名称，显示在蒙太奇编辑器中
	virtual FString GetNotifyName_Implementation() const override;

	// 通知开始：开启无敌帧
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;

	// 通知结束：关闭无敌帧（蒙太奇被中断时也会调用）
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};
