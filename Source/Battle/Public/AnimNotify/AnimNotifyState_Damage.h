// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "AnimNotifyState_Damage.generated.h"

/**
 * 伤害检测通知状态
 * 在 NotifyBegin 时开启武器碰撞检测，NotifyEnd 时关闭
 */
UCLASS()
class BATTLE_API UAnimNotifyState_Damage : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	// 通知名称，显示在蒙太奇编辑器中
	virtual FString GetNotifyName_Implementation() const override;

	// 通知开始：开启伤害检测
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;

	// 通知结束：关闭伤害检测
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};
