// Fill out your copyright notice in the Description page of Project Settings.


#include "SimpleCube.h"

// Sets default values
ASimpleCube::ASimpleCube()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void ASimpleCube::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void ASimpleCube::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

#if 0
	FHitResult HitResult;
	AddActorLocalOffset(FVector(0, 1, 0), true, &HitResult);
	if (HitResult.IsValidBlockingHit())
	{
		UE_LOG(LogTemp, Warning, TEXT("X=%f Y=%f Z=%f"), HitResult.Location.X, HitResult.Location.Y, HitResult.Location.Z);
		FRotator NewRotation = GetActorRotation();
		NewRotation.Yaw += 90; // Rotate 90 degrees around the Z-axis
		SetActorRotation(NewRotation);
	}
#endif
}

