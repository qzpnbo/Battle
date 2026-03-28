// Fill out your copyright notice in the Description page of Project Settings.

#include "Component/CombatComponent.h"
#include <Kismet/GameplayStatics.h>
#include <Kismet/KismetMathLibrary.h>
#include <Kismet/KismetSystemLibrary.h>
#include <Camera/CameraComponent.h>
#include <Animation/AnimInstance.h>
#include <Components/StaticMeshComponent.h>
#include <Components/ShapeComponent.h>

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
void UCombatComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // 锁定敌人时每帧让玩家控制器面向敌人
    HandleFaceTarget();

    // 每帧球形检测锁定目标是否仍在范围内，超出范围则自动解锁
    CheckTargetInRange();

    // 检查玩家与锁定目标之间是否有物体遮挡
    CheckTargetOcclusion();

    // 碰撞体方案不需要每帧追踪，由 Overlap 事件驱动
}

void UCombatComponent::LockTarget()
{
    // 如果已经有目标，则解锁目标
    if (IsValid(TargetLockActor))
    {
        UnlockTarget();
        return;
    }

    APlayerCameraManager *CamManager = UGameplayStatics::GetPlayerCameraManager(this, 0);
    if (!CamManager)
        return;

    FVector CamLocation = CamManager->GetCameraLocation();
    FVector CamForward = UKismetMathLibrary::GetForwardVector(CamManager->GetCameraRotation());

    // --- 使用球形重叠检测（Sphere Overlap），替代射线检测 ---
    // 以玩家位置为中心，LockOnRadius为半径，检测范围内所有Pawn
    FVector OwnerLocation = GetOwner()->GetActorLocation();

    TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
    ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECollisionChannel::ECC_Pawn));

    TArray<AActor *> ActorsToIgnore;
    ActorsToIgnore.Add(GetOwner());

    TArray<AActor *> OverlappedActors;

    bool bFound = UKismetSystemLibrary::SphereOverlapActors(
        GetWorld(),
        OwnerLocation,
        LockOnRadius,
        ObjectTypes,
        nullptr, // 不限制特定类
        ActorsToIgnore,
        OverlappedActors);

    if (!bFound || OverlappedActors.Num() == 0)
    {
        return;
    }

    // --- 从检测到的所有敌人中，选取与摄像机前方向量夹角最小的那个 ---
    AActor *BestTarget = nullptr;
    float SmallestAngle = LockOnMaxAngle; // 只选择在最大允许角度内的目标

    for (AActor *Candidate : OverlappedActors)
    {
        if (!IsValid(Candidate))
        {
            continue;
        }

        // --- 视线遮挡检测：如果相机和候选目标之间有墙体则跳过 ---
        FHitResult WallHit;
        FCollisionQueryParams WallQueryParams;
        WallQueryParams.AddIgnoredActor(GetOwner()); // 忽略自身
        WallQueryParams.AddIgnoredActor(Candidate);  // 忽略候选目标

        bool bWallHit = GetWorld()->LineTraceSingleByChannel(
            WallHit,
            CamLocation,
            Candidate->GetActorLocation(),
            ECC_Visibility,
            WallQueryParams);

        // 如果射线命中了物体，说明中间有墙体遮挡，跳过此目标
        if (bWallHit)
        {
            continue;
        }

        // 计算从摄像机到候选目标的方向向量
        FVector DirToCandidate = (Candidate->GetActorLocation() - CamLocation).GetSafeNormal();

        // 计算该方向与摄像机前方的夹角（度）
        float AngleDeg = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(CamForward, DirToCandidate)));

        if (AngleDeg < SmallestAngle)
        {

            SmallestAngle = AngleDeg;
            BestTarget = Candidate;
        }
    }

    if (!IsValid(BestTarget))
    {
        return;
    }

    TargetLockActor = BestTarget;

    // 设置角色旋转模式
    ACharacter *OwnerCharacter = Cast<ACharacter>(GetOwner());
    if (OwnerCharacter)
    {
        UCharacterMovementComponent *MoveComp = OwnerCharacter->GetCharacterMovement();
        if (MoveComp)
        {
            MoveComp->bOrientRotationToMovement = false;
            MoveComp->bUseControllerDesiredRotation = true;
        }
    }

    // 显示锁定UI
    TargetLockWidget = GetWorld()->SpawnActor<AActor>(TargetLockWidgetBP, FVector::ZeroVector, FRotator::ZeroRotator);

    if (TargetLockWidget)
    {
        // 获取目标的 Mesh 组件
        UMeshComponent *TargetMesh = TargetLockActor->FindComponentByClass<UMeshComponent>();

        if (TargetMesh)
        {
            // 将锁定UI挂载到目标Mesh上
            FAttachmentTransformRules AttachRules(
                EAttachmentRule::KeepRelative,
                EAttachmentRule::KeepRelative,
                EAttachmentRule::KeepRelative,
                true);

            TargetLockWidget->AttachToComponent(TargetMesh, AttachRules, TargetSocketName);
        }
    }
}

void UCombatComponent::UnlockTarget()
{
    if (IsValid(TargetLockActor))
    {
        TargetLockActor = nullptr;

        ACharacter *OwnerCharacter = Cast<ACharacter>(GetOwner());
        if (OwnerCharacter)
        {
            UCharacterMovementComponent *MoveComp = OwnerCharacter->GetCharacterMovement();
            if (MoveComp)
            {
                MoveComp->bOrientRotationToMovement = true;
                MoveComp->bUseControllerDesiredRotation = false;
            }
        }

        if (IsValid(TargetLockWidget))
        {
            TargetLockWidget->Destroy();
            TargetLockWidget = nullptr;
        }
    }
}

// 处理锁定敌人时让玩家控制器面向敌人
void UCombatComponent::HandleFaceTarget()
{
    if (!IsValid(TargetLockActor))
    {
        return;
    }

    APawn *PlayerPawn = Cast<APawn>(GetOwner());
    if (!PlayerPawn)
    {
        return;
    }

    AController *PC = PlayerPawn->GetController();
    if (!PC)
    {
        return;
    }

    // 获取当前控制器旋转（Current）
    FRotator CurrentRotation = PC->GetControlRotation();

    // 获取玩家位置
    FVector PlayerLocation = PlayerPawn->GetActorLocation();
    FVector SocketLocation = TargetLockActor->GetActorLocation(); // 默认目标位置为敌人Actor位置

    // 根据玩家与敌人的距离动态调整目标点Z轴偏移，以让视角更开阔
    UMeshComponent *TargetMesh = TargetLockActor->FindComponentByClass<UMeshComponent>();
    if (TargetMesh)
    {
        SocketLocation = TargetMesh->GetSocketLocation(TargetSocketName);

        // 计算玩家与目标Socket的距离
        float Distance = FVector::Dist(PlayerLocation, SocketLocation);

        // MapRangeClamped: 距离50~LockOnRadius 映射到 Z偏移-70~0
        float MappedZ = FMath::GetMappedRangeValueClamped(
            FVector2D(50.0f, LockOnRadius),
            FVector2D(-70.0f, 0.0f),
            Distance);

        // 将Z偏移应用到目标位置
        SocketLocation += FVector(0.0f, 0.0f, MappedZ);
    }

    // FindLookAtRotation: 计算从玩家看向目标的旋转
    FRotator LookAtRotation = UKismetMathLibrary::FindLookAtRotation(PlayerLocation, SocketLocation);

    // MakeRotator: 使用LookAt的Roll和Yaw，Pitch加上偏移量(-25.0)
    FRotator TargetRotation = FRotator(LookAtRotation.Pitch + TargetPitchOffset, LookAtRotation.Yaw, LookAtRotation.Roll);

    // RInterpTo: 平滑插值旋转
    float DeltaTime = GetWorld()->GetDeltaSeconds();
    FRotator NewRotation = FMath::RInterpTo(CurrentRotation, TargetRotation, DeltaTime, TargetInterpSpeed);

    // SetControlRotation: 设置最终旋转
    PC->SetControlRotation(NewRotation);
}

// --- 武器追踪功能实现 ---

void UCombatComponent::InitSwordCollision()
{
    if (!SwordCollisionRef)
    {
        return;
    }

    // 绑定 Overlap 回调
    SwordCollisionRef->OnComponentBeginOverlap.AddDynamic(this, &UCombatComponent::OnSwordOverlapBegin);
}

void UCombatComponent::StartDamageTrace()
{
    // 清空已命中记录，开始新一次挥砍
    HitActorsSet.Empty();

    // 开启碰撞检测
    if (SwordCollisionRef)
    {
        SwordCollisionRef->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    }

    UE_LOG(LogTemp, Warning, TEXT("Start Damage Trace (Overlap)"));
}

void UCombatComponent::EndDamageTrace()
{
    // 关闭碰撞检测
    if (SwordCollisionRef)
    {
        SwordCollisionRef->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }

    // 清空已命中记录
    HitActorsSet.Empty();

    UE_LOG(LogTemp, Warning, TEXT("End Damage Trace (Overlap)"));
}

void UCombatComponent::OnSwordOverlapBegin(
    UPrimitiveComponent *OverlappedComponent,
    AActor *OtherActor,
    UPrimitiveComponent *OtherComp,
    int32 OtherBodyIndex,
    bool bFromSweep,
    const FHitResult &SweepResult)
{
    if (!IsValid(OtherActor) || OtherActor == GetOwner())
    {
        return;
    }

    // 同一次挥砍中对同一目标只造成一次伤害
    if (HitActorsSet.Contains(OtherActor))
    {
        return;
    }

    // 记录已命中的 Actor
    HitActorsSet.Add(OtherActor);

    // 获取攻击者的控制器作为 EventInstigator
    AController *InstigatorController = nullptr;
    APawn *OwnerPawn = Cast<APawn>(GetOwner());
    if (OwnerPawn)
    {
        InstigatorController = OwnerPawn->GetController();
    }

    // 应用伤害
    UGameplayStatics::ApplyDamage(
        OtherActor,
        SwordDamage,
        InstigatorController,
        GetOwner(), // DamageCauser
        nullptr     // DamageTypeClass
    );

    UE_LOG(LogTemp, Warning, TEXT("Sword Overlap Hit: %s, Damage: %.1f"), *OtherActor->GetName(), SwordDamage);
}

// 每帧进行球形检测，如果锁定目标不在球形范围内则自动解锁
void UCombatComponent::CheckTargetInRange()
{
    if (!IsValid(TargetLockActor))
    {
        return;
    }

    AActor *Owner = GetOwner();
    if (!Owner)
    {
        return;
    }

    // 以玩家位置为中心，LockOnRadius 为半径进行球形检测
    FVector OwnerLocation = Owner->GetActorLocation();

    TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
    ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECollisionChannel::ECC_Pawn));

    TArray<AActor *> ActorsToIgnore;
    ActorsToIgnore.Add(Owner);

    TArray<AActor *> OverlappedActors;

    UKismetSystemLibrary::SphereOverlapActors(
        GetWorld(),
        OwnerLocation,
        LockOnRadius,
        ObjectTypes,
        nullptr,
        ActorsToIgnore,
        OverlappedActors);

    // 如果当前锁定目标不在球形检测结果中，说明已超出范围，自动解锁
    if (!OverlappedActors.Contains(TargetLockActor))
    {
        UnlockTarget();
    }
}

// 检查相机与锁定目标之间是否有物体遮挡，有则延迟取消锁定
// 如果在延迟时间内重新获得视野，则取消解锁
void UCombatComponent::CheckTargetOcclusion()
{
    if (!IsValid(TargetLockActor))
    {
        return;
    }

    // 获取Owner角色
    APawn *OwnerPawn = Cast<APawn>(GetOwner());
    if (!OwnerPawn)
    {
        return;
    }

    // 获取FollowCamera的世界位置作为射线起点
    UCameraComponent *Camera = OwnerPawn->FindComponentByClass<UCameraComponent>();
    if (!Camera)
    {
        return;
    }
    FVector TraceStart = Camera->GetComponentLocation();

    // 获取目标的Mesh Socket位置作为射线终点
    FVector TraceEnd = TargetLockActor->GetActorLocation();
    UMeshComponent *TargetMesh = TargetLockActor->FindComponentByClass<UMeshComponent>();
    if (TargetMesh)
    {
        TraceEnd = TargetMesh->GetSocketLocation(TargetSocketName);
    }

    // 执行Line Trace By Channel（Visibility通道）
    FHitResult HitResult;
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(GetOwner()); // 忽略自身

    bool bHit = GetWorld()->LineTraceSingleByChannel(
        HitResult,
        TraceStart,
        TraceEnd,
        ECC_Visibility,
        QueryParams);

    // 判断是否有遮挡（射线命中了物体，且命中的不是锁定目标）
    bool bIsOccluded = bHit && HitResult.GetActor() != TargetLockActor;

    if (bIsOccluded)
    {
        // 有遮挡：如果延迟定时器尚未启动，则启动定时器
        if (!GetWorld()->GetTimerManager().IsTimerActive(OcclusionTimerHandle))
        {
            GetWorld()->GetTimerManager().SetTimer(
                OcclusionTimerHandle,
                this,
                &UCombatComponent::OnOcclusionTimerExpired,
                OcclusionUnlockDelay,
                false); // 不循环，只触发一次
        }
    }
    else
    {
        // 没有遮挡（重新获得视野）：如果定时器正在运行，则清除它，取消解锁
        if (GetWorld()->GetTimerManager().IsTimerActive(OcclusionTimerHandle))
        {
            GetWorld()->GetTimerManager().ClearTimer(OcclusionTimerHandle);
        }
    }
}

// 遮挡延迟定时器到期回调：延迟时间内一直被遮挡，执行解锁
void UCombatComponent::OnOcclusionTimerExpired()
{
    // 定时器到期，说明在延迟时间内一直有遮挡，执行解锁
    if (IsValid(TargetLockActor))
    {
        UnlockTarget();
    }
}

// --- 攻击与连击系统 ---

void UCombatComponent::Attack()
{
    if (bIsAttacking)
    {
        // 正在攻击中，标记连击输入
        bCombo = true;
        return;
    }

    // 未在攻击中，开始攻击
    bIsAttacking = true;
    bCombo = false;

    USkeletalMeshComponent *Mesh = GetOwnerMesh();
    if (!Mesh || !AttackMontage)
    {
        bIsAttacking = false;
        return;
    }

    UAnimInstance *AnimInstance = Mesh->GetAnimInstance();
    if (!AnimInstance)
    {
        bIsAttacking = false;
        return;
    }

    // 先播放攻击蒙太奇（必须先Play，才能产生活跃的蒙太奇实例）
    AnimInstance->Montage_Play(AttackMontage, 1.0f, EMontagePlayReturnType::MontageLength, 0.0f);

    // 绑定蒙太奇结束回调（On Completed / On Interrupted）单播委托
    // 注意：必须在 Montage_Play 之后调用，否则没有活跃的蒙太奇实例，委托绑定会静默失败
    FOnMontageEnded MontageEndedDelegate;
    MontageEndedDelegate.BindUObject(this, &UCombatComponent::OnAttackMontageEnded);
    AnimInstance->Montage_SetEndDelegate(MontageEndedDelegate, AttackMontage);

    // 绑定蒙太奇通知回调（On Notify Begin），用于连击判定 多播动态委托
    AnimInstance->OnPlayMontageNotifyBegin.AddDynamic(this, &UCombatComponent::OnAttackMontageNotifyBegin);
}

void UCombatComponent::OnAttackMontageEnded(UAnimMontage *Montage, bool bInterrupted)
{
    // 解绑通知回调，避免重复绑定（多播动态委托才需要解绑）
    USkeletalMeshComponent *Mesh = GetOwnerMesh();
    if (Mesh)
    {
        UAnimInstance *AnimInstance = Mesh->GetAnimInstance();
        if (AnimInstance)
        {
            AnimInstance->OnPlayMontageNotifyBegin.RemoveDynamic(this, &UCombatComponent::OnAttackMontageNotifyBegin);
        }
    }

    // 检查是否有待处理的连击输入（在 Montage_Stop 的 BlendOut 期间按下了攻击键）
    if (bCombo)
    {
        // 重置状态后立即发起新的攻击
        bIsAttacking = false;
        bCombo = false;
        Attack();
    }
    else
    {
        // 没有连击输入，正常重置攻击状态
        bIsAttacking = false;
        bCombo = false;
    }
}

void UCombatComponent::OnAttackMontageNotifyBegin(FName NotifyName, const FBranchingPointNotifyPayload &BranchingPointPayload)
{
    // 在播放攻击动作期间，如果玩家有攻击输入（IsAttacking && Combo），继续播放下一段攻击动作
    if (bIsAttacking && bCombo)
    {
        bCombo = false;
    }
    else
    {
        USkeletalMeshComponent *Mesh = GetOwnerMesh();
        if (!Mesh || !AttackMontage)
        {
            return;
        }

        UAnimInstance *AnimInstance = Mesh->GetAnimInstance();
        if (!AnimInstance)
        {
            return;
        }

        // 停止当前蒙太奇
        AnimInstance->Montage_Stop(0.2f, AttackMontage);
    }
}

USkeletalMeshComponent *UCombatComponent::GetOwnerMesh() const
{
    ACharacter *OwnerCharacter = Cast<ACharacter>(GetOwner());
    if (OwnerCharacter)
    {
        return OwnerCharacter->GetMesh();
    }
    return nullptr;
}

void UCombatComponent::Dodge()
{
    // 正在翻滚中，只有进入后摇阶段（bCanBufferDodge）才接受预输入
    if (bIsDodging)
    {
        if (bCanBufferDodge)
        {
            bDodgeBuffered = true;
        }
        return;
    }

    bIsDodging = true;
    bDodgeBuffered = false;
    bCanBufferDodge = false;

    ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
    if (!OwnerCharacter)
    {
        bIsDodging = false;
        return;
    }

    // 通过 UseControllerDesiredRotation 判断是否锁定了目标
    // 锁定目标时 bUseControllerDesiredRotation = true，此时支持四方向翻滚
    // 未锁定目标时 bUseControllerDesiredRotation = false，固定向前翻滚
    UCharacterMovementComponent* MovementComp = OwnerCharacter->GetCharacterMovement();
    bool bIsLocked = MovementComp && MovementComp->bUseControllerDesiredRotation;

    // 根据是否锁定目标决定翻滚方向
    EMovementDirection DodgeDirection = EMovementDirection::Forward;
    if (bIsLocked)
    {
        // 锁定目标时，根据当前移动方向四向翻滚
        DodgeDirection = MovementDirection;
    }

    // 根据翻滚方向选择对应的蒙太奇
    UAnimMontage* SelectedMontage = nullptr;
    switch (DodgeDirection)
    {
    case EMovementDirection::Forward:
        SelectedMontage = DodgeMontage_F;
        break;
    case EMovementDirection::Left:
        SelectedMontage = DodgeMontage_L;
        break;
    case EMovementDirection::Backward:
        SelectedMontage = DodgeMontage_B;
        break;
    case EMovementDirection::Right:
        SelectedMontage = DodgeMontage_R;
        break;
    }

    if (!SelectedMontage)
    {
        bIsDodging = false;
        return;
    }

    USkeletalMeshComponent* Mesh = GetOwnerMesh();
    if (!Mesh)
    {
        bIsDodging = false;
        return;
    }

    UAnimInstance* AnimInstance = Mesh->GetAnimInstance();
    if (!AnimInstance)
    {
        bIsDodging = false;
        return;
    }

    // 播放翻滚蒙太奇
    AnimInstance->Montage_Play(SelectedMontage, 1.0f, EMontagePlayReturnType::MontageLength, 0.0f);

    // 绑定蒙太奇结束回调
    FOnMontageEnded DodgeEndedDelegate;
    DodgeEndedDelegate.BindUObject(this, &UCombatComponent::OnDodgeMontageEnded);
    AnimInstance->Montage_SetEndDelegate(DodgeEndedDelegate, SelectedMontage);

    // 绑定蒙太奇通知回调，用于开启预输入窗口
    AnimInstance->OnPlayMontageNotifyBegin.AddDynamic(this, &UCombatComponent::OnDodgeMontageNotifyBegin);
}

void UCombatComponent::OnDodgeMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
    // 解绑通知回调，避免重复绑定
    USkeletalMeshComponent* Mesh = GetOwnerMesh();
    if (Mesh)
    {
        UAnimInstance* AnimInstance = Mesh->GetAnimInstance();
        if (AnimInstance)
        {
            AnimInstance->OnPlayMontageNotifyBegin.RemoveDynamic(this, &UCombatComponent::OnDodgeMontageNotifyBegin);
        }
    }

    bIsDodging = false;
    bCanBufferDodge = false;

    // 检查是否有翻滚预输入，有则立即触发下一次翻滚
    if (bDodgeBuffered)
    {
        bDodgeBuffered = false;
        Dodge();
    }
}

void UCombatComponent::OnDodgeMontageNotifyBegin(FName NotifyName, const FBranchingPointNotifyPayload& BranchingPointPayload)
{
    // 收到 DodgeBufferWindow 通知时，开启预输入窗口
    if (NotifyName == FName(TEXT("DodgeBufferWindow")))
    {
        bCanBufferDodge   = true;
    }
}
