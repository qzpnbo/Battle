// Fill out your copyright notice in the Description page of Project Settings.

#include "Enemy.h"
#include "GameFramework/CharacterMovementComponent.h"

// Sets default values
AEnemy::AEnemy()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	MaxHealth = 100;
	CurrentHealth = MaxHealth;
	InitialLocation = FVector(200, 200, 0);
}

void AEnemy::MoveLoop()
{
	FVector TargetLocation = FVector(1000, 1000, 0);
	FVector Direction = (TargetLocation - GetActorLocation()).GetSafeNormal();
	AddMovementInput(Direction, 0.3f);
}

// Called when the game starts or when spawned
void AEnemy::BeginPlay()
{
	Super::BeginPlay();
	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::White, TEXT("Hello World"));

	// SetActorLocation(InitialLocation);
}

// Called every frame
void AEnemy::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	// MoveLoop();
}

// Called to bind functionality to input
void AEnemy::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

}
