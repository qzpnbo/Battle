// Fill out your copyright notice in the Description page of Project Settings.


#include "CombatComponent.h"
#include <Kismet/GameplayStatics.h>

// Sets default values for this component's properties
UCombatComponent::UCombatComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}


// Called when the game starts
void UCombatComponent::BeginPlay()
{
	Super::BeginPlay();

	// ...
	
}


// Called every frame
void UCombatComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

void UCombatComponent::LockTarget()
{
    // 1. 对应左上角的 Branch：如果已经有目标，则调用之前的 UnlockTarget
    if (IsValid(TargetLockActor))
    {
        UnlockTarget();
        return;
    }

    // 2. 计算检测球体的位置 (对应图中 Camera Position + Forward * 500)
    APlayerCameraManager* CamManager = UGameplayStatics::GetPlayerCameraManager(this, 0);
    if (!CamManager) return;

    FVector CamLocation = CamManager->GetCameraLocation();
    FVector CamForward = CamManager->GetCameraRotation().Vector();
    // 对应图中 Multiply 500 和 Add 节点
    FVector SphereLocation = CamLocation + (CamForward * 500.0f);

    // 3. 执行球体范围检测 (Sphere Overlap Actors)
    TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
    ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECollisionChannel::ECC_Pawn)); // 假设检测 Pawn

    TArray<AActor*> ActorsToIgnore;
    ActorsToIgnore.Add(GetOwner()); // 忽略自己

    TArray<AActor*> OutActors;
    
    // 对应图中 Sphere Overlap Actors 节点，半径设为 500
    bool bFound = UKismetSystemLibrary::SphereOverlapActors(
        GetWorld(), 
        SphereLocation, 
        300.0f, 
        ObjectTypes, 
        nullptr, // 这里的 Filter 可以根据需要填你的 Enemy 类
        ActorsToIgnore, 
        OutActors
    );

    // 4. 对应图中的 GET (index 0) 和 IsValid 判断
    if (bFound && OutActors.Num() > 0)
    {
        TargetLockActor = OutActors[0]; // 获取找到第一个敌人

        if (IsValid(TargetLockActor))
        {
            // 5. 设置角色旋转模式 (对应右侧那三个 SET 节点)
            ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
            if (OwnerCharacter)
            {
                UCharacterMovementComponent* MoveComp = OwnerCharacter->GetCharacterMovement();
                if (MoveComp)
                {
                    // Orient Rotation to Movement = False
                    MoveComp->bOrientRotationToMovement = false;
                    // Use Controller Desired Rotation = True
                    MoveComp->bUseControllerDesiredRotation = true;
                }
            }
        }
    }

    // 5. 显示锁定UI
    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = GetOwner();
    SpawnParams.Instigator = GetOwner()->GetInstigator();

    // 在默认位置生成，稍后会 Attach
    TargetLockWidget = GetWorld()->SpawnActor<AActor>(TargetLockWidgetBP, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);

    if (TargetLockWidget)
    {
        // --- 2. GetComponentByClass (获取目标的 Mesh) ---
        // 蓝图中是 Get Component by Class (MeshComponent)
        UMeshComponent* TargetMesh = TargetLockActor->FindComponentByClass<UMeshComponent>();

        if (TargetMesh)
        {
            // --- 3. AttachActorToComponent (挂载) ---
            FAttachmentTransformRules AttachRules(
                EAttachmentRule::KeepRelative, // Location Rule
                EAttachmentRule::KeepRelative, // Rotation Rule
                EAttachmentRule::KeepRelative, // Scale Rule
                false // Weld Simulated Bodies
            );

            TargetLockWidget->AttachToComponent(TargetMesh, AttachRules, TargetSocketName);
        }
    }
}

void UCombatComponent::UnlockTarget()
{
    // 1. 对应蓝图的 Branch (Is Valid)
    if (IsValid(TargetLockActor))
    {
        // --- Then 0: 清除目标引用 ---
        TargetLockActor = nullptr;

        // --- Then 1: 修改角色旋转逻辑 ---
        // 获取持有该组件的 Character
        ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
        if (OwnerCharacter)
        {
            UCharacterMovementComponent* MoveComp = OwnerCharacter->GetCharacterMovement();
            if (MoveComp)
            {
                // 对应蓝图的 Set Orient Rotation to Movement (True)
                MoveComp->bOrientRotationToMovement = true;
                
                // 对应蓝图的 Set Use Controller Desired Rotation (False)
                // 注意：在C++中这属于 Actor 的属性，也可以通过 MoveComp 设置
                OwnerCharacter->bUseControllerRotationYaw = false; 
                // 或者如果是移动组件里的：MoveComp->bUseControllerDesiredRotation = false;
            }
        }

        // --- Then 2: 销毁目标显示件 ---
        if (IsValid(TargetLockWidget))
        {
            // 对应蓝图的 Destroy Actor
            TargetLockWidget->Destroy();
            TargetLockWidget = nullptr;
        }
    }
}

// 必须带上 UCombatComponent:: 前缀！
void UCombatComponent::Attack() 
{
    // ... 你的逻辑代码 ...
}