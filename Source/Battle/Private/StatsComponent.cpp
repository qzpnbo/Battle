// Fill out your copyright notice in the Description page of Project Settings.


#include "StatsComponent.h"

// Sets default values for this component's properties
UStatsComponent::UStatsComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...

	Health = 100.f;
    MaxHealth = 100.f;
}


// Called when the game starts
void UStatsComponent::BeginPlay()
{
	Super::BeginPlay();

	// ...
	
}


// Called every frame
void UStatsComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

void UStatsComponent::IncreaseValue(float Value)
{
    // 1. 对应蓝图的 Float + Float 和 Clamp
    Health = FMath::Clamp(Health + Value, 0.0f, MaxHealth);

    // 2. 广播通知 UI 更新 (对应蓝图设置 ProgressBar 的逻辑)
    OnHealthChanged.Broadcast(Health, MaxHealth);

    // 3. 对应蓝图的 Branch (Health <= 0)
    if (Health <= 0.0f)
    {
        OnDeath.Broadcast();
        // 如果需要，可以在这里处理 Destroy Actor 或 播放死亡动画
    }
}