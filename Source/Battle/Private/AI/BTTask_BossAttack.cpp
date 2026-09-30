// Fill out your copyright notice in the Description page of Project Settings.

#include "AI/BTTask_BossAttack.h"
#include "Component/CombatComponent.h"
#include "Enemy/BossEnemy.h"
#include "Types/BattleTypes.h"
#include "AIController.h"
#include "Animation/AnimMontage.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"

UBTTask_BossAttack::UBTTask_BossAttack()
{
	NodeName = TEXT("Boss Attack");
	bNotifyTick = true;

	// 目标默认使用黑板中的 TargetActor
	BlackboardKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_BossAttack, BlackboardKey), AActor::StaticClass());
	BlackboardKey.SelectedKeyName = TEXT("TargetActor");
}

uint16 UBTTask_BossAttack::GetInstanceMemorySize() const
{
	return sizeof(FBTBossAttackMemory);
}

void UBTTask_BossAttack::InitializeMemory(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTMemoryInit::Type InitType) const
{
	// 节点内存是裸字节，需要显式构造（否则 LastOptionIndex 为 0 而非 INDEX_NONE）
	// 恢复子树（RestoringSubtree）时保留原有内存内容
	if (InitType == EBTMemoryInit::Initialize)
	{
		new (NodeMemory) FBTBossAttackMemory();
	}
}

int32 UBTTask_BossAttack::SelectAttackOption(float Distance, int32 Phase, int32 LastOptionIndex) const
{
	float TotalWeight = 0.0f;
	TArray<TPair<int32, float>> Candidates;

	for (int32 Index = 0; Index < AttackOptions.Num(); ++Index)
	{
		const FBossAttackOption& Option = AttackOptions[Index];
		if (!Option.Montage || Option.Weight <= 0.0f)
		{
			continue;
		}
		if (Phase < Option.MinPhase || Phase > Option.MaxPhase)
		{
			continue;
		}
		if (Distance < Option.MinDistance || Distance > Option.MaxDistance)
		{
			continue;
		}

		const float Weight = Option.Weight * (Index == LastOptionIndex ? RepeatWeightScale : 1.0f);
		if (Weight > 0.0f)
		{
			Candidates.Emplace(Index, Weight);
			TotalWeight += Weight;
		}
	}

	if (Candidates.Num() == 0)
	{
		return INDEX_NONE;
	}

	// 加权随机
	float Roll = FMath::FRandRange(0.0f, TotalWeight);
	for (const TPair<int32, float>& Candidate : Candidates)
	{
		Roll -= Candidate.Value;
		if (Roll <= 0.0f)
		{
			return Candidate.Key;
		}
	}
	return Candidates.Last().Key;
}

EBTNodeResult::Type UBTTask_BossAttack::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	FBTBossAttackMemory* Memory = reinterpret_cast<FBTBossAttackMemory*>(NodeMemory);
	Memory->Elapsed = 0.0f;

	AAIController* AIController = OwnerComp.GetAIOwner();
	APawn* Pawn = AIController ? AIController->GetPawn() : nullptr;
	UCombatComponent* Combat = Pawn ? Pawn->FindComponentByClass<UCombatComponent>() : nullptr;
	if (!Combat || Combat->GetCombatState() != ECombatState::Idle)
	{
		return EBTNodeResult::Failed;
	}

	const UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	const AActor* Target = Blackboard ? Cast<AActor>(Blackboard->GetValueAsObject(BlackboardKey.SelectedKeyName)) : nullptr;
	if (!IsValid(Target))
	{
		return EBTNodeResult::Failed;
	}

	const float Distance = FVector::Dist2D(Pawn->GetActorLocation(), Target->GetActorLocation());
	const ABossEnemy* Boss = Cast<ABossEnemy>(Pawn);
	const int32 Phase = Boss ? Boss->GetCurrentPhase() : 1;

	bool bStarted = false;
	const int32 OptionIndex = SelectAttackOption(Distance, Phase, Memory->LastOptionIndex);
	if (OptionIndex != INDEX_NONE)
	{
		const FBossAttackOption& Option = AttackOptions[OptionIndex];
		bStarted = Combat->PerformAttackMontage(Option.Montage, Option.DamageMultiplier, Option.PoiseDamage, Option.bCanBeParried);
		if (bStarted)
		{
			Memory->LastOptionIndex = OptionIndex;
		}
	}
	else if (AttackOptions.Num() == 0)
	{
		// 未配置招式：退回普通攻击（近距离轻攻击，远一点用重攻击）
		if (Distance > 250.0f)
		{
			Combat->HeavyAttack();
		}
		else
		{
			Combat->Attack();
		}
		bStarted = Combat->GetCombatState() != ECombatState::Idle;
	}

	if (!bStarted)
	{
		return EBTNodeResult::Failed;
	}

	// 出招期间不移动，由招式本身（Root Motion）决定位移
	AIController->StopMovement();
	return EBTNodeResult::InProgress;
}

void UBTTask_BossAttack::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	FBTBossAttackMemory* Memory = reinterpret_cast<FBTBossAttackMemory*>(NodeMemory);
	Memory->Elapsed += DeltaSeconds;

	AAIController* AIController = OwnerComp.GetAIOwner();
	APawn* Pawn = AIController ? AIController->GetPawn() : nullptr;
	UCombatComponent* Combat = Pawn ? Pawn->FindComponentByClass<UCombatComponent>() : nullptr;
	if (!Combat || Combat->GetCombatState() == ECombatState::Dead)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	// 出招前段追踪目标
	if (Memory->Elapsed <= TrackingDuration && Combat->IsInAnyAttackState())
	{
		const UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
		const AActor* Target = Blackboard ? Cast<AActor>(Blackboard->GetValueAsObject(BlackboardKey.SelectedKeyName)) : nullptr;
		if (IsValid(Target))
		{
			const FVector ToTarget = (Target->GetActorLocation() - Pawn->GetActorLocation()).GetSafeNormal2D();
			if (!ToTarget.IsNearlyZero())
			{
				const FRotator Desired(0.0f, ToTarget.Rotation().Yaw, 0.0f);
				Pawn->SetActorRotation(FMath::RInterpTo(Pawn->GetActorRotation(), Desired, DeltaSeconds, TrackingInterpSpeed));
			}
		}
	}

	// Boss 回到 Idle（招式结束、硬直结束）才结束任务
	if (Combat->GetCombatState() == ECombatState::Idle)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
		return;
	}

	if (Memory->Elapsed >= MaxTaskDuration)
	{
		UE_LOG(LogTemp, Warning, TEXT("BTTask_BossAttack timed out on %s (state=%s)"), *GetNameSafe(Pawn), *UEnum::GetValueAsString(Combat->GetCombatState()));
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
	}
}

EBTNodeResult::Type UBTTask_BossAttack::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	// 招式一旦出手不中断（类魂：Boss 招式有承诺性），由蒙太奇自然结束
	return EBTNodeResult::Aborted;
}

FString UBTTask_BossAttack::GetStaticDescription() const
{
	return FString::Printf(TEXT("%s\n招式数: %d（按距离/阶段加权随机）"), *Super::GetStaticDescription(), AttackOptions.Num());
}
