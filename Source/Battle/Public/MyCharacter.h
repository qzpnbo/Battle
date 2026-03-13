// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "StatsComponent.h"
#include "CombatComponent.h"
#include "MyCharacter.generated.h"

UCLASS()
class BATTLE_API AMyCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	AMyCharacter();

protected:
    // 战斗组件
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	class UCombatComponent* CombatComponent;

	// 状态组件
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats")
	class UStatsComponent* StatsComponent;

	// 武器网格体组件
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
    class UStaticMeshComponent* SwordMesh;

	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	/** 目标锁定输入动作 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    class UInputAction* IA_Lock;

    /** 绑定函数 */
    void RequestTargetLock();

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

};
