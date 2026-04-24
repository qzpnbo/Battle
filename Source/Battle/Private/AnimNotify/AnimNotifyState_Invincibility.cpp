// Fill out your copyright notice in the Description page of Project Settings.

#include "AnimNotify/AnimNotifyState_Invincibility.h"
#include "Component/CombatComponent.h"

FString UAnimNotifyState_Invincibility::GetNotifyName_Implementation() const
{
	return TEXT("Invincibility");
}

void UAnimNotifyState_Invincibility::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (!MeshComp) return;

	AActor* Owner = MeshComp->GetOwner();
	if (!Owner) return;

	// 从 Owner 身上获取 CombatComponent，开启无敌帧
	UCombatComponent* CombatComp = Owner->FindComponentByClass<UCombatComponent>();
	if (CombatComp)
	{
		CombatComp->bIsInvincible = true;
		UE_LOG(LogTemp, Log, TEXT("I-Frame ON (NotifyState)"));
	}
}

void UAnimNotifyState_Invincibility::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	if (!MeshComp) return;

	AActor* Owner = MeshComp->GetOwner();
	if (!Owner) return;

	// 从 Owner 身上获取 CombatComponent，关闭无敌帧
	UCombatComponent* CombatComp = Owner->FindComponentByClass<UCombatComponent>();
	if (CombatComp)
	{
		CombatComp->bIsInvincible = false;
		UE_LOG(LogTemp, Log, TEXT("I-Frame OFF (NotifyState)"));
	}
}
