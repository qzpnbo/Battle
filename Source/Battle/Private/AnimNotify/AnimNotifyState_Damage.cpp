// Fill out your copyright notice in the Description page of Project Settings.

#include "AnimNotify/AnimNotifyState_Damage.h"
#include "Component/CombatComponent.h"

FString UAnimNotifyState_Damage::GetNotifyName_Implementation() const
{
	return TEXT("Damage Trace");
}

void UAnimNotifyState_Damage::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (!MeshComp) return;

	AActor* Owner = MeshComp->GetOwner();
	if (!Owner) return;

	// 从 Owner 身上获取 CombatComponent，调用开始伤害检测
	UCombatComponent* CombatComp = Owner->FindComponentByClass<UCombatComponent>();
	if (CombatComp)
	{
		CombatComp->StartDamageTrace();
	}
}

void UAnimNotifyState_Damage::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	if (!MeshComp) return;

	AActor* Owner = MeshComp->GetOwner();
	if (!Owner) return;

	// 从 Owner 身上获取 CombatComponent，调用结束伤害检测
	UCombatComponent* CombatComp = Owner->FindComponentByClass<UCombatComponent>();
	if (CombatComp)
	{
		CombatComp->EndDamageTrace();
	}
}
