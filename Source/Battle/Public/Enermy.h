// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Enermy.generated.h"

UCLASS()
class BATTLE_API AEnermy : public ACharacter
{
	GENERATED_BODY()

private:
	float CurrentHealth; // 当前生命值

public:
	// Sets default values for this character's properties
	AEnermy();

	UPROPERTY(BlueprintReadWrite, Category = "Enermy")
	float MaxHealth; // 最大生命值

	UFUNCTION(BlueprintCallable, Category = "Enermy")
	void MoveLoop(); // 移动

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
};
