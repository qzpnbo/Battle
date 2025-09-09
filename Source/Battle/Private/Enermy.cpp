// Fill out your copyright notice in the Description page of Project Settings.

#include "Enermy.h"
#include "GameFramework/CharacterMovementComponent.h" // ���Ӵ����԰�����������

// Sets default values
AEnermy::AEnermy()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	MaxHealth = 100;
	CurrentHealth = MaxHealth;

}

void AEnermy::MoveLoop()
{
	// �򵥵��ƶ��߼�ʾ��
	
	// �ƶ���ָ��λ��
	FVector TargetLocation = FVector(1000, 1000, 0); // Ŀ��λ��
	FVector Direction = (TargetLocation - GetActorLocation()).GetSafeNormal();
	AddMovementInput(Direction, 1.0f);
}

// Called when the game starts or when spawned
void AEnermy::BeginPlay()
{
	Super::BeginPlay();
	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::White, TEXT("Hello World"));
	MoveLoop();
}

// Called every frame
void AEnermy::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	//MoveLoop();
}

// Called to bind functionality to input
void AEnermy::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

}
