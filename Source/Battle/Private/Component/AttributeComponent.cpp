// Fill out your copyright notice in the Description page of Project Settings.

#include "Component/AttributeComponent.h"
#include "Engine/World.h"

UAttributeComponent::UAttributeComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	bWantsInitializeComponent = true;
}

void UAttributeComponent::InitializeComponent()
{
	Super::InitializeComponent();

	// 在 BeginPlay 之前初始化当前值，保证其他组件/UI 在 BeginPlay 中读取到的是正确数值
	Health = MaxHealth;
	Stamina = MaxStamina;
	Poise = MaxPoise;
}

void UAttributeComponent::BeginPlay()
{
	Super::BeginPlay();

	// 广播初始值，确保 UI 无论初始化时序如何都能显示正确数值
	OnHealthChanged.Broadcast(Health, MaxHealth);
	OnStaminaChanged.Broadcast(Stamina, MaxStamina);
}

void UAttributeComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const float Now = GetWorldTime();

	// 耐力回复：未暂停、已过回复延迟、未满
	if (bUseStamina && !bStaminaRegenPaused && Stamina < MaxStamina && Now - LastStaminaUseTime >= StaminaRegenDelay)
	{
		SetStamina(Stamina + StaminaRegenRate * StaminaRegenMultiplier * DeltaTime);
	}

	// 韧性重置：一段时间没有受到韧性伤害则回满
	if (Poise < MaxPoise && Now - LastPoiseDamageTime >= PoiseResetDelay)
	{
		Poise = MaxPoise;
	}
}

float UAttributeComponent::ApplyHealthDamage(float Amount)
{
	if (Amount <= 0.0f || Health <= 0.0f)
	{
		return 0.0f;
	}

	const float OldHealth = Health;
	Health = FMath::Clamp(Health - Amount, 0.0f, MaxHealth);
	OnHealthChanged.Broadcast(Health, MaxHealth);
	return OldHealth - Health;
}

void UAttributeComponent::Heal(float Amount)
{
	if (Amount <= 0.0f || Health <= 0.0f)
	{
		return;
	}

	Health = FMath::Clamp(Health + Amount, 0.0f, MaxHealth);
	OnHealthChanged.Broadcast(Health, MaxHealth);
}

bool UAttributeComponent::TryConsumeStamina(float Cost)
{
	if (!bUseStamina)
	{
		return true;
	}

	if (Stamina <= 0.0f)
	{
		return false;
	}

	LastStaminaUseTime = GetWorldTime();
	SetStamina(Stamina - FMath::Max(0.0f, Cost));
	return true;
}

bool UAttributeComponent::DrainStamina(float Amount)
{
	if (!bUseStamina)
	{
		return true;
	}

	LastStaminaUseTime = GetWorldTime();
	SetStamina(Stamina - FMath::Max(0.0f, Amount));
	return Stamina > 0.0f;
}

void UAttributeComponent::SetStaminaRegenPaused(bool bPaused)
{
	if (bStaminaRegenPaused && !bPaused)
	{
		// 动作结束时重新计时回复延迟（类魂：动作做完停顿一下才开始回耐力）
		LastStaminaUseTime = GetWorldTime();
	}
	bStaminaRegenPaused = bPaused;
}

bool UAttributeComponent::ApplyPoiseDamage(float Amount)
{
	if (Amount <= 0.0f)
	{
		return false;
	}

	LastPoiseDamageTime = GetWorldTime();
	Poise -= Amount;

	if (Poise <= 0.0f)
	{
		Poise = MaxPoise;
		OnPoiseBroken.Broadcast();
		return true;
	}
	return false;
}

void UAttributeComponent::RestoreAll()
{
	Health = MaxHealth;
	Poise = MaxPoise;
	OnHealthChanged.Broadcast(Health, MaxHealth);
	SetStamina(MaxStamina);
}

void UAttributeComponent::SetStamina(float NewStamina)
{
	const float Clamped = FMath::Clamp(NewStamina, 0.0f, MaxStamina);
	if (!FMath::IsNearlyEqual(Clamped, Stamina))
	{
		Stamina = Clamped;
		OnStaminaChanged.Broadcast(Stamina, MaxStamina);
	}
}

float UAttributeComponent::GetWorldTime() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetTimeSeconds() : 0.0f;
}
