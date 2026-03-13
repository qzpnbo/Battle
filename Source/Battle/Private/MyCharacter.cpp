// Fill out your copyright notice in the Description page of Project Settings.


#include "MyCharacter.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"

// Sets default values
AMyCharacter::AMyCharacter()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	// 初始化逻辑组件
	CombatComponent = CreateDefaultSubobject<UCombatComponent>(TEXT("CombatComponent"));
	StatsComponent = CreateDefaultSubobject<UStatsComponent>(TEXT("StatsComponent"));

    // 初始化 Sword 及其层级
    SwordMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Sword"));
    SwordMesh->SetupAttachment(GetMesh(), TEXT("hand_r_Socket")); // 绑定到骨骼插槽
}

// Called when the game starts or when spawned
void AMyCharacter::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AMyCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

// Called to bind functionality to input
void AMyCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    // 将原生的 InputComponent 转换为增强输入组件
    if (UEnhancedInputComponent* EnhancedInputComponent = CastChecked<UEnhancedInputComponent>(PlayerInputComponent))
    {
        // 绑定 IA_Lock 到 RequestTargetLock 函数
        // ETriggerEvent::Started 对应蓝图里的 Started (按下那一瞬间)
        EnhancedInputComponent->BindAction(IA_Lock, ETriggerEvent::Started, this, &AMyCharacter::RequestTargetLock);
    }
}

void AMyCharacter::RequestTargetLock()
{
    // 调用组件里的逻辑
    if (CombatComponent)
    {
        CombatComponent->LockTarget();
    }
}