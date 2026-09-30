// Fill out your copyright notice in the Description page of Project Settings.

#include "Game/Checkpoint.h"
#include "Game/BattleRespawnSubsystem.h"
#include "Character/BattleCharacter.h"
#include "Component/AttributeComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/ArrowComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

ACheckpoint::ACheckpoint()
{
	PrimaryActorTick.bCanEverTick = false;

	Trigger = CreateDefaultSubobject<USphereComponent>(TEXT("Trigger"));
	Trigger->InitSphereRadius(180.0f);
	Trigger->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	RootComponent = Trigger;

	// 默认外观：引擎自带的圆柱体当作"篝火底座"，可在蓝图/关卡中替换
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(RootComponent);
	Mesh->SetRelativeScale3D(FVector(0.6f, 0.6f, 0.15f));
	Mesh->SetRelativeLocation(FVector(0.0f, 0.0f, -80.0f));
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMesh.Succeeded())
	{
		Mesh->SetStaticMesh(CylinderMesh.Object);
	}

	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(RootComponent);
	Light->SetRelativeLocation(FVector(0.0f, 0.0f, 20.0f));
	// 构造函数中直接设置默认值（Set* 函数会触发渲染状态更新，不适合在 CDO 构造阶段调用）
	Light->LightColor = FLinearColor(1.0f, 0.55f, 0.2f).ToFColor(true);
	Light->AttenuationRadius = 600.0f;
	Light->Intensity = InactiveLightIntensity;

	RespawnPoint = CreateDefaultSubobject<UArrowComponent>(TEXT("RespawnPoint"));
	RespawnPoint->SetupAttachment(RootComponent);
	RespawnPoint->SetRelativeLocation(FVector(150.0f, 0.0f, 0.0f));
}

void ACheckpoint::BeginPlay()
{
	Super::BeginPlay();

	Light->SetIntensity(bActivated ? ActiveLightIntensity : InactiveLightIntensity);
	Trigger->OnComponentBeginOverlap.AddUniqueDynamic(this, &ACheckpoint::OnTriggerBeginOverlap);
}

FTransform ACheckpoint::GetRespawnTransform() const
{
	// 只保留 Yaw，避免检查点被旋转后角色歪着重生
	const FVector Location = RespawnPoint->GetComponentLocation();
	const FRotator Rotation(0.0f, RespawnPoint->GetComponentRotation().Yaw, 0.0f);
	return FTransform(Rotation, Location);
}

void ACheckpoint::SetActivated(bool bNewActivated)
{
	if (bActivated == bNewActivated)
	{
		return;
	}

	bActivated = bNewActivated;
	Light->SetIntensity(bActivated ? ActiveLightIntensity : InactiveLightIntensity);

	if (bActivated)
	{
		OnCheckpointActivated();
	}
}

void ACheckpoint::OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
                                        int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	ABattleCharacter* Player = Cast<ABattleCharacter>(OtherActor);
	if (!Player || Player->IsDead())
	{
		return;
	}

	if (bRestoreAttributesOnActivate && Player->AttributeComponent)
	{
		Player->AttributeComponent->RestoreAll();
	}

	if (UBattleRespawnSubsystem* RespawnSubsystem = GetWorld()->GetSubsystem<UBattleRespawnSubsystem>())
	{
		RespawnSubsystem->SetActiveCheckpoint(this);
	}
}
