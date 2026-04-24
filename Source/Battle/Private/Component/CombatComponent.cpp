// Fill out your copyright notice in the Description page of Project Settings.

#include "Component/CombatComponent.h"
#include "Types/BattleTypes.h"
#include "UObject/ConstructorHelpers.h"
#include <Kismet/GameplayStatics.h>
#include <Kismet/KismetMathLibrary.h>
#include <Kismet/KismetSystemLibrary.h>
#include <Camera/CameraComponent.h>
#include <Animation/AnimInstance.h>
#include <Components/StaticMeshComponent.h>
#include <Components/ShapeComponent.h>
#include <Components/CapsuleComponent.h>

// Sets default values for this component's properties
UCombatComponent::UCombatComponent()
{
    // Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
    // off to improve performance if you don't need them.
    PrimaryComponentTick.bCanEverTick = true;

    static ConstructorHelpers::FClassFinder<AActor> TargetLockWidgetFinder(TEXT("/Game/UI/Widgets/BP_TargetLockWidget"));
    if (TargetLockWidgetFinder.Succeeded())
    {
        TargetLockWidgetBP = TargetLockWidgetFinder.Class;
    }
}

// Called when the game starts
void UCombatComponent::BeginPlay()
{
    Super::BeginPlay();

    ACharacter *OwnerChar = Cast<ACharacter>(GetOwner());
    if (OwnerChar)
    {
        CachedOwnerCharacter = OwnerChar;
        CachedOwnerMesh = OwnerChar->GetMesh();
    }
}

// Called every frame
void UCombatComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    HandleFaceTarget(DeltaTime);

    CheckTargetInRange();

    CheckTargetOcclusion();

    CheckHeavyAttackAirborne(DeltaTime);

    UpdateAttackRotation(DeltaTime);

    UpdateAimPitch(DeltaTime);

    // 锁定目标切换冷却时间递减
    if (TargetSwitchCooldownRemaining > 0.0f)
    {
        TargetSwitchCooldownRemaining = FMath::Max(0.0f, TargetSwitchCooldownRemaining - DeltaTime);
    }
}

// ============================================================================
// 目标锁定系统
// ============================================================================

void UCombatComponent::LockTarget()
{
    if (CombatState == ECombatState::Dead)
    {
        return;
    }

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

        // --- 过滤已死亡的目标：检查候选目标身上的战斗组件是否处于Dead状态 ---
        UCombatComponent *CandidateCombat = Candidate->FindComponentByClass<UCombatComponent>();
        if (CandidateCombat && CandidateCombat->GetCombatState() == ECombatState::Dead)
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
    ACharacter *OwnerCharacter = CachedOwnerCharacter.Get();
    if (OwnerCharacter)
    {
        UCharacterMovementComponent *MoveComp = OwnerCharacter->GetCharacterMovement();
        if (MoveComp)
        {
            MoveComp->bOrientRotationToMovement = false;
        }
    }

    // 生成并附着锁定 UI Widget 到目标 Mesh
    AttachLockWidgetToActor(TargetLockActor);
}

void UCombatComponent::UnlockTarget()
{
    if (IsValid(TargetLockActor))
    {
        TargetLockActor = nullptr;

        ACharacter *OwnerCharacter = CachedOwnerCharacter.Get();
        if (OwnerCharacter)
        {
            UCharacterMovementComponent *MoveComp = OwnerCharacter->GetCharacterMovement();
            if (MoveComp)
            {
                MoveComp->bOrientRotationToMovement = true;
            }
        }

        if (IsValid(TargetLockWidget))
        {
            TargetLockWidget->Destroy();
            TargetLockWidget = nullptr;
        }
    }

    // 解锁后重置切换相关状态，下次锁定时从干净状态开始
    TargetSwitchCooldownRemaining = 0.0f;
    TargetSwitchLookAccumulator = 0.0f;
}

void UCombatComponent::AttachLockWidgetToActor(AActor* TargetActor)
{
    if (!IsValid(TargetActor))
    {
        return;
    }

    // 如果尚未生成锁定 Widget，则生成一个
    if (!IsValid(TargetLockWidget))
    {
        TargetLockWidget = GetWorld()->SpawnActor<AActor>(TargetLockWidgetBP, FVector::ZeroVector, FRotator::ZeroRotator);
    }

    if (!IsValid(TargetLockWidget))
    {
        return;
    }

    // 获取目标的 Mesh 组件
    UMeshComponent *TargetMesh = TargetActor->FindComponentByClass<UMeshComponent>();
    if (!TargetMesh)
    {
        return;
    }

    // 附着 UI Widget 到目标 Mesh
    FAttachmentTransformRules AttachRules(
        EAttachmentRule::KeepRelative,
        EAttachmentRule::KeepRelative,
        EAttachmentRule::KeepRelative,
        true);

    TargetLockWidget->AttachToComponent(TargetMesh, AttachRules, TargetSocketName);
}

void UCombatComponent::HandleLockLookInput(float LookDeltaX)
{
    // 未锁定或已死亡时不处理
    if (!IsValid(TargetLockActor) || CombatState == ECombatState::Dead)
    {
        TargetSwitchLookAccumulator = 0.0f;
        return;
    }

    // 冷却期间忽略输入
    if (TargetSwitchCooldownRemaining > 0.0f)
    {
        return;
    }

    // 如果输入方向与当前累积值方向相反，则重置累积（避免来回微调时误触发）
    if ((LookDeltaX > 0.0f && TargetSwitchLookAccumulator < 0.0f) ||
        (LookDeltaX < 0.0f && TargetSwitchLookAccumulator > 0.0f))
    {
        TargetSwitchLookAccumulator = 0.0f;
    }

    TargetSwitchLookAccumulator += LookDeltaX;

    // 累积值达到阈值时触发切换
    if (FMath::Abs(TargetSwitchLookAccumulator) >= TargetSwitchThreshold)
    {
        const bool bRight = TargetSwitchLookAccumulator > 0.0f;
        if (SwitchLockTarget(bRight))
        {
            // 切换成功：重置累积值并进入冷却
            TargetSwitchCooldownRemaining = TargetSwitchCooldown;
        }
        TargetSwitchLookAccumulator = 0.0f;
    }
}

bool UCombatComponent::SwitchLockTarget(bool bRight)
{
    if (!IsValid(TargetLockActor))
    {
        return false;
    }

    APlayerCameraManager *CamManager = UGameplayStatics::GetPlayerCameraManager(this, 0);
    if (!CamManager)
    {
        return false;
    }

    AActor *Owner = GetOwner();
    if (!Owner)
    {
        return false;
    }

    const FVector CamLocation = CamManager->GetCameraLocation();
    const FRotator CamRotation = CamManager->GetCameraRotation();
    const FVector CamForward = UKismetMathLibrary::GetForwardVector(CamRotation);
    const FVector CamRight = UKismetMathLibrary::GetRightVector(CamRotation);

    // 当前锁定目标在摄像机空间中的横向坐标（作为基准）
    const FVector CurrentTargetLocation = TargetLockActor->GetActorLocation();
    const FVector DirToCurrent = (CurrentTargetLocation - CamLocation);
    const float CurrentRightDot = FVector::DotProduct(DirToCurrent, CamRight);

    // 以玩家位置为中心搜索附近所有 Pawn
    TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
    ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECollisionChannel::ECC_Pawn));

    TArray<AActor *> ActorsToIgnore;
    ActorsToIgnore.Add(Owner);
    ActorsToIgnore.Add(TargetLockActor); // 排除当前锁定目标

    TArray<AActor *> OverlappedActors;
    const bool bFound = UKismetSystemLibrary::SphereOverlapActors(
        GetWorld(),
        Owner->GetActorLocation(),
        LockOnRadius,
        ObjectTypes,
        nullptr,
        ActorsToIgnore,
        OverlappedActors);

    if (!bFound || OverlappedActors.Num() == 0)
    {
        return false;
    }

    // 在候选中筛选出符合切换方向的目标，并按"距离当前目标最近"排序
    AActor *BestTarget = nullptr;
    float BestScore = TNumericLimits<float>::Max(); // 分数越小越好（横向距离差）

    for (AActor *Candidate : OverlappedActors)
    {
        if (!IsValid(Candidate))
        {
            continue;
        }

        // 过滤已死亡敌人
        UCombatComponent *CandidateCombat = Candidate->FindComponentByClass<UCombatComponent>();
        if (CandidateCombat && CandidateCombat->GetCombatState() == ECombatState::Dead)
        {
            continue;
        }

        // 视线遮挡检测
        FHitResult WallHit;
        FCollisionQueryParams WallQueryParams;
        WallQueryParams.AddIgnoredActor(Owner);
        WallQueryParams.AddIgnoredActor(Candidate);

        const bool bWallHit = GetWorld()->LineTraceSingleByChannel(
            WallHit,
            CamLocation,
            Candidate->GetActorLocation(),
            ECC_Visibility,
            WallQueryParams);

        if (bWallHit)
        {
            continue;
        }

        const FVector DirToCandidate = Candidate->GetActorLocation() - CamLocation;

        // 必须在摄像机前方（防止锁定到视野外/身后的目标）
        if (FVector::DotProduct(DirToCandidate, CamForward) <= 0.0f)
        {
            continue;
        }

        // 角度过滤：与摄像机前方的夹角需在允许范围内
        const FVector DirToCandidateNormalized = DirToCandidate.GetSafeNormal();
        const float AngleDeg = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(CamForward, DirToCandidateNormalized)));
        if (AngleDeg > LockOnMaxAngle)
        {
            continue;
        }

        // 计算候选目标在摄像机右方的投影值（横向位置）
        const float CandidateRightDot = FVector::DotProduct(DirToCandidate, CamRight);

        // 根据切换方向筛选：向右切换要求 CandidateRightDot > CurrentRightDot
        // 向左切换要求 CandidateRightDot < CurrentRightDot
        const float RightDiff = CandidateRightDot - CurrentRightDot;
        if (bRight && RightDiff <= 0.0f)
        {
            continue;
        }
        if (!bRight && RightDiff >= 0.0f)
        {
            continue;
        }

        // 评分：与当前目标的横向距离差（绝对值越小，越"相邻"）
        const float Score = FMath::Abs(RightDiff);
        if (Score < BestScore)
        {
            BestScore = Score;
            BestTarget = Candidate;
        }
    }

    if (!IsValid(BestTarget))
    {
        return false;
    }

    // 切换到新目标：更新引用并重新附着 Widget
    TargetLockActor = BestTarget;

    // Widget 从旧目标 detach，然后重新 attach 到新目标
    if (IsValid(TargetLockWidget))
    {
        TargetLockWidget->DetachFromActor(FDetachmentTransformRules::KeepRelativeTransform);
    }
    AttachLockWidgetToActor(TargetLockActor);

    return true;
}

// 处理锁定敌人时让玩家控制器和角色都面向敌人
void UCombatComponent::HandleFaceTarget(float DeltaTime)
{
    if (!IsValid(TargetLockActor))
    {
        return;
    }

    ACharacter *OwnerChar = CachedOwnerCharacter.Get();
    if (!OwnerChar)
    {
        return;
    }

    AController *PC = OwnerChar->GetController();
    if (!PC)
    {
        return;
    }

    // 获取玩家位置
    FVector PlayerLocation = OwnerChar->GetActorLocation();
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

    // 设置控制器旋转（摄像机视角）
    FRotator CurrentControllerRotation = PC->GetControlRotation();
    FRotator NewControllerRotation = FMath::RInterpTo(CurrentControllerRotation, TargetRotation, DeltaTime, TargetInterpSpeed);
    PC->SetControlRotation(NewControllerRotation);

    // 设置角色旋转（攻击方向）
    // 计算从玩家到目标的水平方向（忽略垂直方向）
    FVector DirectionToTarget = (SocketLocation - PlayerLocation).GetSafeNormal();
    DirectionToTarget.Z = 0.0f;

    if (!DirectionToTarget.IsNearlyZero())
    {
        FRotator TargetActorRotation = DirectionToTarget.Rotation();
        FRotator CurrentActorRotation = OwnerChar->GetActorRotation();
        FRotator NewActorRotation = FMath::RInterpTo(CurrentActorRotation, TargetActorRotation, DeltaTime, TargetInterpSpeed);
        OwnerChar->SetActorRotation(NewActorRotation);
    }
}

// ============================================================================
// 武器碰撞伤害系统
// ============================================================================

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
    ACharacter *OwnerChar = CachedOwnerCharacter.Get();
    if (OwnerChar)
    {
        InstigatorController = OwnerChar->GetController();
    }

    // 根据当前攻击类型计算最终伤害
    float Damage = SwordDamage;
    switch (CombatState)
    {
    case ECombatState::HeavyAttacking:
        Damage *= HeavyAttackDamageMultiplier;
        break;
    case ECombatState::FallingAttacking:
        Damage *= FallingAttackDamageMultiplier;
        break;
    default:
        // 轻攻击：使用基础伤害，倍率 1.0x
        break;
    }

    // 应用伤害
    UGameplayStatics::ApplyDamage(
        OtherActor,
        Damage,
        InstigatorController,
        GetOwner(), // DamageCauser
        nullptr     // DamageTypeClass
    );

    UE_LOG(LogTemp, Warning, TEXT("Sword Overlap Hit: %s, Damage: %.1f (State: %s)"),
           *OtherActor->GetName(), Damage, *UEnum::GetValueAsString(CombatState));

    // 触发命中反馈（顿帧 + 镜头震动）
    ApplyHitFeedback();
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
        return;
    }

    // 检查锁定目标是否已死亡，死亡则自动解锁
    UCombatComponent *TargetCombat = TargetLockActor->FindComponentByClass<UCombatComponent>();
    if (TargetCombat && TargetCombat->GetCombatState() == ECombatState::Dead)
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
    ACharacter *OwnerChar = CachedOwnerCharacter.Get();
    if (!OwnerChar)
    {
        return;
    }

    // 获取FollowCamera的世界位置作为射线起点
    UCameraComponent *Camera = OwnerChar->FindComponentByClass<UCameraComponent>();
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

// ============================================================================
// 命中反馈系统（Hit Stop + Hit Lag + Camera Shake）
// ============================================================================

void UCombatComponent::ApplyHitFeedback()
{
    UWorld *World = GetWorld();
    if (!World)
    {
        return;
    }

    // 根据攻击类型确定参数
    bool bIsHeavyHit = (CombatState == ECombatState::HeavyAttacking || CombatState == ECombatState::FallingAttacking);
    TSubclassOf<UCameraShakeBase> ShakeClass = bIsHeavyHit ? HeavyHitCameraShake : LightHitCameraShake;

    // --- Hit Lag（攻击者动画局部减速） ---
    if (bEnableHitLag)
    {
        ApplyHitLag();
    }

    // --- Camera Shake（镜头震动） ---
    if (bEnableHitCameraShake && ShakeClass)
    {
        APlayerController *PC = UGameplayStatics::GetPlayerController(World, 0);
        if (PC)
        {
            PC->ClientStartCameraShake(ShakeClass);
        }
    }
}



// ============================================================================
// Hit Lag 系统（攻击者动画局部减速）
// ============================================================================

UAnimMontage *UCombatComponent::GetCurrentAttackMontage() const
{
    switch (CombatState)
    {
    case ECombatState::Attacking:
        return AttackMontage;
    case ECombatState::HeavyAttacking:
        return HeavyAttackMontage;
    case ECombatState::FallingAttacking:
        return FallingAttackMontage;
    default:
        return nullptr;
    }
}

void UCombatComponent::ApplyHitLag()
{
    USkeletalMeshComponent *Mesh = CachedOwnerMesh.Get();
    if (!Mesh)
    {
        return;
    }

    UAnimInstance *AnimInstance = Mesh->GetAnimInstance();
    if (!AnimInstance)
    {
        return;
    }

    UAnimMontage *CurrentMontage = GetCurrentAttackMontage();
    if (!CurrentMontage || !AnimInstance->Montage_IsPlaying(CurrentMontage))
    {
        return;
    }

    UWorld *World = GetWorld();
    if (!World)
    {
        return;
    }

    bool bIsHeavyHit = (CombatState == ECombatState::HeavyAttacking || CombatState == ECombatState::FallingAttacking);
    float LagRate = bIsHeavyHit ? HeavyHitLagRate : LightHitLagRate;
    float LagDuration = bIsHeavyHit ? HeavyHitLagDuration : LightHitLagDuration;

    // 如果已有 Hit Lag 定时器在运行，先恢复速率再重新应用
    if (World->GetTimerManager().IsTimerActive(HitLagTimerHandle))
    {
        World->GetTimerManager().ClearTimer(HitLagTimerHandle);
    }

    // 降低攻击者当前蒙太奇的播放速率（只影响攻击者自己的动画）
    AnimInstance->Montage_SetPlayRate(CurrentMontage, LagRate);

    UE_LOG(LogTemp, Log, TEXT("Hit Lag applied: Rate=%.3f, Duration=%.3f"), LagRate, LagDuration);

    // 使用真实时间定时器恢复播放速率
    // 注意：如果同时启用了 Hit Stop（全局时间膨胀），普通定时器会被减速
    // 但蒙太奇播放速率是独立于全局时间膨胀的，所以 Hit Lag 和 Hit Stop 可以叠加使用
    FTimerDelegate LagTimerDelegate;
    LagTimerDelegate.BindUObject(this, &UCombatComponent::OnHitLagTimerExpired);
    World->GetTimerManager().SetTimer(
        HitLagTimerHandle,
        LagTimerDelegate,
        LagDuration,
        false);
}

void UCombatComponent::OnHitLagTimerExpired()
{
    // 恢复攻击者蒙太奇的正常播放速率
    USkeletalMeshComponent *Mesh = CachedOwnerMesh.Get();
    if (!Mesh)
    {
        return;
    }

    UAnimInstance *AnimInstance = Mesh->GetAnimInstance();
    if (!AnimInstance)
    {
        return;
    }

    UAnimMontage *CurrentMontage = GetCurrentAttackMontage();
    if (CurrentMontage && AnimInstance->Montage_IsPlaying(CurrentMontage))
    {
        AnimInstance->Montage_SetPlayRate(CurrentMontage, 1.0f);
        UE_LOG(LogTemp, Log, TEXT("Hit Lag recovered: Rate restored to 1.0"));
    }
}

// ============================================================================
// 战斗状态管理
// ============================================================================

void UCombatComponent::SetCombatState(ECombatState NewState)
{
    if (CombatState != NewState)
    {
        ECombatState OldState = CombatState;
        CombatState = NewState;
        UE_LOG(LogTemp, Log, TEXT("CombatState: %s -> %s"),
               *UEnum::GetValueAsString(OldState),
               *UEnum::GetValueAsString(NewState));

        // 进入死亡状态时执行完整清理
        if (NewState == ECombatState::Dead)
        {
            HandleDeath();
        }
    }
}

bool UCombatComponent::IsInAnyAttackState() const
{
    return CombatState == ECombatState::Attacking || CombatState == ECombatState::HeavyAttacking || CombatState == ECombatState::FallingAttacking;
}

// ============================================================================
// 攻击系统
// ============================================================================

void UCombatComponent::Attack()
{
    // 只有 Idle 和 Attacking（连击）状态才允许直接攻击
    // 其他所有状态（受击硬直、翻滚、重攻击、下落攻击、死亡等）一律拒绝并尝试缓存输入
    // 使用白名单方式确保新增状态时默认不可攻击，彻底杜绝受击中攻击的问题
    if (CombatState != ECombatState::Idle && CombatState != ECombatState::Attacking)
    {
        BufferInput(EBufferedInputAction::Attack);
        return;
    }

    // 跳跃中执行下落攻击
    ACharacter *OwnerChar = CachedOwnerCharacter.Get();
    if (OwnerChar && OwnerChar->GetCharacterMovement() && OwnerChar->GetCharacterMovement()->IsFalling())
    {
        // 已经在攻击中（包括下落攻击中），不重复触发
        if (IsInAnyAttackState())
        {
            return;
        }

        if (!FallingAttackMontage)
        {
            return;
        }

        USkeletalMeshComponent *Mesh = CachedOwnerMesh.Get();
        if (!Mesh)
        {
            return;
        }

        UAnimInstance *AnimInstance = Mesh->GetAnimInstance();
        if (!AnimInstance)
        {
            return;
        }

        SetCombatState(ECombatState::FallingAttacking);
        ClearBufferedInput();

        // 播放下落攻击蒙太奇
        AnimInstance->Montage_Play(FallingAttackMontage, 1.0f, EMontagePlayReturnType::MontageLength, 0.0f);

        // 绑定下落攻击蒙太奇结束回调
        FOnMontageEnded FallingEndedDelegate;
        FallingEndedDelegate.BindUObject(this, &UCombatComponent::OnFallingAttackMontageEnded);
        AnimInstance->Montage_SetEndDelegate(FallingEndedDelegate, FallingAttackMontage);

        return;
    }

    if (CombatState == ECombatState::Attacking)
    {
        // 根据当前攻击阶段决定攻击输入的处理方式
        switch (AttackPhase)
        {
        case EAttackPhase::Combo:
            // 连击窗口内：缓存攻击输入并立即尝试跳转到下一段
            // （最后一段没有 Combo 阶段，不会进入此分支）
            BufferedAction = EBufferedInputAction::Attack;
            TrySetComboNextSection();
            break;

        case EAttackPhase::Buffer:
            // 预输入窗口内：缓存攻击输入，蒙太奇结束后从 S0 重新开始
            BufferedAction = EBufferedInputAction::Attack;
            break;

        default:
            // Startup 阶段：攻击输入无效，忽略
            break;
        }
        return;
    }

    // 未在攻击中，开始全新攻击
    SetCombatState(ECombatState::Attacking);
    ClearBufferedInput();
    AttackComboIndex = 0;
    AttackPhase = EAttackPhase::Startup;

    USkeletalMeshComponent *Mesh = CachedOwnerMesh.Get();
    if (!Mesh || !AttackMontage)
    {
        SetCombatState(ECombatState::Idle);
        return;
    }

    UAnimInstance *AnimInstance = Mesh->GetAnimInstance();
    if (!AnimInstance)
    {
        SetCombatState(ECombatState::Idle);
        return;
    }

    // 设置攻击初始朝向（以角色当前面朝方向出招）
    SetAttackRotation();

    // 播放攻击蒙太奇（从第一段 Section 开始）
    AnimInstance->Montage_Play(AttackMontage, 1.0f, EMontagePlayReturnType::MontageLength, 0.0f);

    // 绑定蒙太奇结束回调（On Completed / On Interrupted）单播委托
    // 注意：必须在 Montage_Play 之后调用，否则没有活跃的蒙太奇实例，委托绑定会静默失败
    FOnMontageEnded MontageEndedDelegate;
    MontageEndedDelegate.BindUObject(this, &UCombatComponent::OnAttackMontageEnded);
    AnimInstance->Montage_SetEndDelegate(MontageEndedDelegate, AttackMontage);

    // 绑定蒙太奇通知回调（On Notify Begin），用于连击判定 多播动态委托
    AnimInstance->OnPlayMontageNotifyBegin.AddDynamic(this, &UCombatComponent::OnAttackMontageNotifyBegin);
}

// ============================================================================
// 蒙太奇回调（生命周期事件）
// ============================================================================

void UCombatComponent::OnFallingAttackMontageEnded(UAnimMontage *Montage, bool bInterrupted)
{
    // 下落攻击结束，仅在状态仍为 FallingAttacking 时重置
    // 避免覆盖更高优先级的状态（如受击硬直 Staggered）
    if (CombatState == ECombatState::FallingAttacking)
    {
        SetCombatState(ECombatState::Idle);
        // 尝试执行缓存的输入
        ConsumeBufferedInput();
    }
}

void UCombatComponent::TrySetComboNextSection()
{
    // 只有在攻击状态且有缓存的攻击输入时才处理连击
    if (CombatState != ECombatState::Attacking || BufferedAction != EBufferedInputAction::Attack)
    {
        return;
    }

    int32 NextComboIndex = AttackComboIndex + 1;
    if (NextComboIndex >= AttackComboSectionNames.Num())
    {
        return;
    }

    // 清空缓存的攻击输入（连击已被消费）
    BufferedAction = EBufferedInputAction::None;

    // 跳转到新段后重置为 Startup 阶段
    // 当前段剩余的 InputBufferWindow 通知触发时，会发现 AttackPhase 已经是 Startup，
    // 不会错误地将其切换为 Buffer（因为只有 Combo → Buffer 的转换才合法）
    AttackPhase = EAttackPhase::Startup;

    // 记录下一段连击索引
    AttackComboIndex = NextComboIndex;

    // 连击时重新设置攻击朝向
    SetAttackRotation();

    // 动态设置当前 Section 的 NextSection 为下一段
    // 当前 Section 自然播完后蒙太奇会自动跳转到下一段，动画无缝衔接
    USkeletalMeshComponent *Mesh = CachedOwnerMesh.Get();
    if (Mesh && AttackMontage)
    {
        UAnimInstance *AnimInstance = Mesh->GetAnimInstance();
        if (AnimInstance)
        {
            FName CurrentSection = AttackComboSectionNames[AttackComboIndex - 1];
            FName NextSection = AttackComboSectionNames[NextComboIndex];
            AnimInstance->Montage_SetNextSection(CurrentSection, NextSection, AttackMontage);
        }
    }
}

void UCombatComponent::OnAttackMontageEnded(UAnimMontage *Montage, bool bInterrupted)
{
    // 解绑通知回调，避免重复绑定（多播动态委托才需要解绑）
    USkeletalMeshComponent *Mesh = CachedOwnerMesh.Get();
    if (Mesh)
    {
        UAnimInstance *AnimInstance = Mesh->GetAnimInstance();
        if (AnimInstance)
        {
            AnimInstance->OnPlayMontageNotifyBegin.RemoveDynamic(this, &UCombatComponent::OnAttackMontageNotifyBegin);
        }
    }

    // 重置攻击朝向调整计时器
    AttackRotationElapsed = 0.0f;
    // 重置连击索引（蒙太奇真正结束意味着连击链断开）
    AttackComboIndex = 0;
    // 重置攻击阶段
    AttackPhase = EAttackPhase::None;

    // 仅在状态仍为 Attacking 时重置为 Idle
    // 避免覆盖更高优先级的状态（如受击硬直 Staggered）
    if (CombatState == ECombatState::Attacking)
    {
        SetCombatState(ECombatState::Idle);
        // 尝试执行缓存的输入（跨动作预输入，如翻滚、重攻击等）
        // 连击跳转已在 OnAttackMontageNotifyBegin 中通过 SetNextSection 处理，
        // 这里消费的攻击缓存来自 InputBufferWindow 阶段，会通过 Attack() 从 S0 重新开始
        ConsumeBufferedInput();
    }
}

// --- 重攻击系统 ---

void UCombatComponent::HeavyAttack()
{
    // 只有 Idle 状态才允许重攻击，其他所有状态一律拒绝并尝试缓存输入
    if (CombatState != ECombatState::Idle)
    {
        BufferInput(EBufferedInputAction::HeavyAttack);
        return;
    }

    // 跳跃中不允许重攻击
    ACharacter *OwnerChar = CachedOwnerCharacter.Get();
    if (OwnerChar && OwnerChar->GetCharacterMovement() && OwnerChar->GetCharacterMovement()->IsFalling())
    {
        return;
    }

    if (!HeavyAttackMontage)
    {
        return;
    }

    USkeletalMeshComponent *Mesh = CachedOwnerMesh.Get();
    if (!Mesh)
    {
        return;
    }

    UAnimInstance *AnimInstance = Mesh->GetAnimInstance();
    if (!AnimInstance)
    {
        return;
    }

    SetCombatState(ECombatState::HeavyAttacking);
    bHeavyAttackLaunched = false;

    // 设置攻击初始朝向（以角色当前面朝方向出招）
    SetAttackRotation();

    // 先播放攻击蒙太奇
    AnimInstance->Montage_Play(HeavyAttackMontage, 1.0f, EMontagePlayReturnType::MontageLength, 0.0f);

    // 绑定重攻击蒙太奇结束回调
    FOnMontageEnded HeavyEndedDelegate;
    HeavyEndedDelegate.BindUObject(this, &UCombatComponent::OnHeavyAttackMontageEnded);
    AnimInstance->Montage_SetEndDelegate(HeavyEndedDelegate, HeavyAttackMontage);

    // 绑定蒙太奇通知回调，用于在起跳帧触发 LaunchCharacter
    AnimInstance->OnPlayMontageNotifyBegin.AddDynamic(this, &UCombatComponent::OnHeavyAttackMontageNotifyBegin);
}

void UCombatComponent::OnHeavyAttackMontageEnded(UAnimMontage *Montage, bool bInterrupted)
{
    // 解绑通知回调，避免重复绑定（多播动态委托需要手动解绑）
    USkeletalMeshComponent *Mesh = CachedOwnerMesh.Get();
    if (Mesh)
    {
        UAnimInstance *AnimInstance = Mesh->GetAnimInstance();
        if (AnimInstance)
        {
            AnimInstance->OnPlayMontageNotifyBegin.RemoveDynamic(this, &UCombatComponent::OnHeavyAttackMontageNotifyBegin);
        }
    }

    // 重置攻击朝向调整计时器
    AttackRotationElapsed = 0.0f;

    // 重攻击结束，仅在状态仍为 HeavyAttacking 时重置
    // 避免覆盖更高优先级的状态（如受击硬直 Staggered）
    bHeavyAttackLaunched = false;
    if (CombatState == ECombatState::HeavyAttacking)
    {
        SetCombatState(ECombatState::Idle);
        // 尝试执行跨动作类型的缓存输入
        ConsumeBufferedInput();
    }
}

void UCombatComponent::HeavyAttackLaunch()
{
    // 由蒙太奇通知 HeavyAttackLaunch 调用，给角色一个向上的冲量
    // 这样角色会自然进入 Falling 状态，重力始终生效
    if (CombatState != ECombatState::HeavyAttacking || bHeavyAttackLaunched)
    {
        return;
    }

    ACharacter *OwnerChar = CachedOwnerCharacter.Get();
    if (!OwnerChar)
    {
        return;
    }

    bHeavyAttackLaunched = true;

    // LaunchCharacter 会自动将角色从 Walking 切换到 Falling
    // 重力始终生效，角色会自然抛物线运动
    float LaunchForce = HeavyAttackLaunchForce;
    OwnerChar->LaunchCharacter(FVector(0.0f, 0.0f, LaunchForce), false, true);

    UE_LOG(LogTemp, Warning, TEXT("HeavyAttack Launch! Force: %.1f"), LaunchForce);
}

void UCombatComponent::OnHeavyAttackMontageNotifyBegin(FName NotifyName, const FBranchingPointNotifyPayload &BranchingPointPayload)
{
    // 收到 InputBufferWindow 通知时，开启跨动作预输入窗口
    if (NotifyName == FName(TEXT("InputBufferWindow")))
    {
        bCanBufferInput = true;
        return;
    }

    // 收到 HeavyAttackLaunch 通知时，触发起跳冲量
    if (NotifyName == FName(TEXT("HeavyAttackLaunch")))
    {
        HeavyAttackLaunch();
    }
}

void UCombatComponent::CheckHeavyAttackAirborne(float DeltaTime)
{
    // 仅在重攻击期间检测
    if (CombatState != ECombatState::HeavyAttacking)
    {
        return;
    }

    ACharacter *OwnerChar = CachedOwnerCharacter.Get();
    if (!OwnerChar || !OwnerChar->GetCharacterMovement())
    {
        return;
    }

    UCharacterMovementComponent *MoveComp = OwnerChar->GetCharacterMovement();

    // 只在角色处于 Falling 状态时检测
    if (!MoveComp->IsFalling())
    {
        return;
    }

    // 从角色脚底向下发射射线，检测到地面的实际距离
    FVector CharLocation = OwnerChar->GetActorLocation();
    // 获取胶囊体半高，射线从胶囊体底部开始
    float CapsuleHalfHeight = OwnerChar->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    FVector TraceStart = CharLocation - FVector(0.0f, 0.0f, CapsuleHalfHeight);
    FVector TraceEnd = TraceStart - FVector(0.0f, 0.0f, HeavyAttackTraceDistance);

    FHitResult HitResult;
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(OwnerChar);

    bool bHitGround = GetWorld()->LineTraceSingleByChannel(
        HitResult,
        TraceStart,
        TraceEnd,
        ECC_Visibility,
        QueryParams);

    float DistanceToGround = 0.0f;
    if (bHitGround)
    {
        // 射线命中地面，计算脚底到地面的距离
        DistanceToGround = (TraceStart - HitResult.ImpactPoint).Size();
    }
    else
    {
        // 射线未命中任何物体，说明脚下很深（超过射线检测距离），视为悬空
        DistanceToGround = HeavyAttackTraceDistance;
    }

    // 如果离地高度超过阈值，说明角色从悬崖边掉落，中断重攻击蒙太奇
    float MaxAirborneHeight = HeavyAttackMaxAirborneHeight;
    if (DistanceToGround > MaxAirborneHeight)
    {
        USkeletalMeshComponent *Mesh = CachedOwnerMesh.Get();
        if (Mesh)
        {
            UAnimInstance *AnimInstance = Mesh->GetAnimInstance();
            if (AnimInstance && HeavyAttackMontage)
            {
                AnimInstance->Montage_Stop(0.25f, HeavyAttackMontage);
                UE_LOG(LogTemp, Warning, TEXT("HeavyAttack interrupted: too high above ground (%.1f cm)"), DistanceToGround);
            }
        }
    }
}

void UCombatComponent::OnAttackMontageNotifyBegin(FName NotifyName, const FBranchingPointNotifyPayload &BranchingPointPayload)
{
    // ---- ComboWindow 通知：进入连击窗口阶段（在攻击动画中段触发） ----
    if (NotifyName == FName(TEXT("ComboWindow")))
    {
        // 最后一段连击没有下一段可跳转，不进入 Combo 阶段
        if (AttackComboIndex >= AttackComboSectionNames.Num() - 1)
        {
            return;
        }

        AttackPhase = EAttackPhase::Combo;

        // 如果在 ComboWindow 触发前已经缓存了攻击输入（极快速连按），立即跳转
        TrySetComboNextSection();
        return;
    }

    // ---- InputBufferWindow 通知：进入预输入窗口阶段（在攻击动画后段触发） ----
    if (NotifyName != FName(TEXT("InputBufferWindow")))
    {
        return;
    }

    // 只有从 Combo 阶段自然过渡才进入 Buffer 阶段
    // 如果连击跳转已发生（AttackPhase 被重置为 Startup），说明这是旧段的通知，跳过
    // 特殊情况：最后一段攻击没有 ComboWindow，AttackPhase 会一直是 Startup，
    //           此时 InputBufferWindow 应该直接从 Startup 进入 Buffer
    bool bIsLastCombo = (AttackComboIndex >= AttackComboSectionNames.Num() - 1);
    if (AttackPhase == EAttackPhase::Combo || (AttackPhase == EAttackPhase::Startup && bIsLastCombo))
    {
        AttackPhase = EAttackPhase::Buffer;
    }
}

void UCombatComponent::SetAttackRotation()
{
    // 按下攻击键时，记录当前朝向作为初始攻击方向
    // 重置计时器，允许在攻击前几帧内通过移动输入调整朝向
    AttackRotationElapsed = 0.0f;
}

void UCombatComponent::UpdateAttackRotation(float DeltaTime)
{
    // 仅在攻击状态下且未锁定目标时允许朝向调整
    if (!IsInAnyAttackState() || IsValid(TargetLockActor))
    {
        return;
    }

    // 超过允许调整朝向的时间窗口，不再更新
    AttackRotationElapsed += DeltaTime;
    float WindowDuration = AttackRotationWindowDuration;
    if (AttackRotationElapsed > WindowDuration)
    {
        return;
    }

    ACharacter *OwnerChar = CachedOwnerCharacter.Get();
    if (!OwnerChar || !OwnerChar->GetCharacterMovement())
    {
        return;
    }

    // 未锁定目标时：允许通过移动输入调整朝向
    // 获取当前帧的移动输入方向
    FVector InputVector = OwnerChar->GetCharacterMovement()->GetLastInputVector();

    // 只有在有移动输入时才更新朝向（无输入则保持角色当前朝向不变）
    if (InputVector.IsNearlyZero())
    {
        return;
    }

    FRotator TargetRotation = InputVector.Rotation();
    TargetRotation.Pitch = 0.0f;

    // 快速平滑旋转到目标方向
    FRotator CurrentRotation = OwnerChar->GetActorRotation();
    float RotInterpSpeed = AttackRotationInterpSpeed;
    FRotator NewRotation = FMath::RInterpTo(CurrentRotation, TargetRotation, DeltaTime, RotInterpSpeed);
    OwnerChar->SetActorRotation(NewRotation);
}

// ============================================================================
// 翻滚系统
// ============================================================================

void UCombatComponent::Dodge()
{
    // 正在翻滚中，通过通用缓存系统缓存翻滚输入（允许连续翻滚）
    if (CombatState == ECombatState::Dodging)
    {
        BufferInput(EBufferedInputAction::Dodge);
        return;
    }

    // 只有 Idle 状态才允许直接翻滚，其他所有状态一律缓存输入
    // 使用白名单方式确保新增状态时默认不可翻滚，彻底杜绝受击中翻滚的问题
    if (CombatState != ECombatState::Idle)
    {
        BufferInput(EBufferedInputAction::Dodge);
        return;
    }

    // 跳跃中不允许翻滚
    ACharacter *OwnerChar = CachedOwnerCharacter.Get();
    if (OwnerChar && OwnerChar->GetCharacterMovement() && OwnerChar->GetCharacterMovement()->IsFalling())
    {
        return;
    }

    SetCombatState(ECombatState::Dodging);
    ClearBufferedInput();

    ACharacter *OwnerCharacter = CachedOwnerCharacter.Get();
    if (!OwnerCharacter)
    {
        SetCombatState(ECombatState::Idle);
        return;
    }

    // 锁定目标时四方向翻滚,未锁定目标时固定向前翻滚
    bool bIsLocked = IsValid(TargetLockActor);

    // 根据是否锁定目标决定翻滚方向
    EMovementDirection DodgeDirection = EMovementDirection::Forward;
    if (bIsLocked)
    {
        // 锁定目标时，根据当前移动方向四向翻滚
        DodgeDirection = MovementDirection;
    }

    // 根据翻滚方向选择对应的蒙太奇
    UAnimMontage *SelectedMontage = nullptr;
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
        SetCombatState(ECombatState::Idle);
        return;
    }

    USkeletalMeshComponent *Mesh = CachedOwnerMesh.Get();
    if (!Mesh)
    {
        SetCombatState(ECombatState::Idle);
        return;
    }

    UAnimInstance *AnimInstance = Mesh->GetAnimInstance();
    if (!AnimInstance)
    {
        SetCombatState(ECombatState::Idle);
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

void UCombatComponent::OnDodgeMontageEnded(UAnimMontage *Montage, bool bInterrupted)
{
    // 解绑通知回调，避免重复绑定
    USkeletalMeshComponent *Mesh = CachedOwnerMesh.Get();
    if (Mesh)
    {
        UAnimInstance *AnimInstance = Mesh->GetAnimInstance();
        if (AnimInstance)
        {
            AnimInstance->OnPlayMontageNotifyBegin.RemoveDynamic(this, &UCombatComponent::OnDodgeMontageNotifyBegin);
        }
    }

    // 翻滚结束时确保关闭无敌帧（防止蒙太奇被中断时通知未触发）
    bIsInvincible = false;

    // 仅在状态仍为 Dodging 时重置为 Idle
    // 避免覆盖更高优先级的状态（如受击硬直 Staggered）
    if (CombatState == ECombatState::Dodging)
    {
        SetCombatState(ECombatState::Idle);
        // 尝试执行缓存的输入（包括连续翻滚和跨动作预输入）
        ConsumeBufferedInput();
    }
}

void UCombatComponent::OnDodgeMontageNotifyBegin(FName NotifyName, const FBranchingPointNotifyPayload &BranchingPointPayload)
{
    // 收到 InputBufferWindow 通知时，开启通用预输入窗口
    if (NotifyName == FName(TEXT("InputBufferWindow")))
    {
        bCanBufferInput = true;
    }
}

// ============================================================================
// 通用输入缓存系统
// ============================================================================

void UCombatComponent::BufferInput(EBufferedInputAction Action)
{
    // 攻击蒙太奇中由 AttackPhase 控制预输入窗口
    // 其他蒙太奇（翻滚、重攻击等）由 bCanBufferInput 控制
    bool bCanBuffer = bCanBufferInput || (CombatState == ECombatState::Attacking && AttackPhase == EAttackPhase::Buffer);
    if (bCanBuffer)
    {
        BufferedAction = Action;
        UE_LOG(LogTemp, Log, TEXT("Input buffered: %s"), *UEnum::GetValueAsString(Action));
    }
}

void UCombatComponent::ConsumeBufferedInput()
{
    // 取出缓存的输入动作并清空
    EBufferedInputAction ActionToExecute = BufferedAction;
    ClearBufferedInput();

    // 根据缓存的输入类型执行对应动作
    switch (ActionToExecute)
    {
    case EBufferedInputAction::Attack:
        Attack();
        break;
    case EBufferedInputAction::HeavyAttack:
        HeavyAttack();
        break;
    case EBufferedInputAction::Dodge:
        Dodge();
        break;
    case EBufferedInputAction::None:
    default:
        break;
    }
}

void UCombatComponent::ClearBufferedInput()
{
    BufferedAction = EBufferedInputAction::None;
    bCanBufferInput = false;
    AttackPhase = EAttackPhase::None;
}

// ============================================================================
// 受击硬直系统
// ============================================================================

bool UCombatComponent::HandleTakeDamage(AActor* DamageCauser)
{
    // 无敌帧期间免疫伤害
    if (bIsInvincible)
    {
        UE_LOG(LogTemp, Log, TEXT("Damage blocked by I-Frame!"));
        return false;
    }

    // 已死亡不处理
    if (CombatState == ECombatState::Dead)
    {
        return false;
    }

    // 中断当前正在执行的动作（攻击/翻滚等）
    InterruptCurrentAction();

    // 设置受击硬直状态
    SetCombatState(ECombatState::Staggered);

    // --- 根据攻击来源方向选择受击蒙太奇 ---
    UAnimMontage* SelectedHitReact = SelectDirectionalHitReact(DamageCauser);

    // 播放受击蒙太奇
    if (SelectedHitReact)
    {
        USkeletalMeshComponent *Mesh = CachedOwnerMesh.Get();
        if (Mesh)
        {
            UAnimInstance *AnimInstance = Mesh->GetAnimInstance();
            if (AnimInstance)
            {
                // 先解绑旧的通知多播委托（如果有的话），防止连续受击时重复绑定
                AnimInstance->OnPlayMontageNotifyBegin.RemoveDynamic(this, &UCombatComponent::OnHitReactMontageNotifyBegin);

                AnimInstance->Montage_Play(SelectedHitReact, 1.0f, EMontagePlayReturnType::MontageLength, 0.0f);

                // 使用单播委托 Montage_SetEndDelegate 绑定结束回调
                FOnMontageEnded HitReactEndedDelegate;
                HitReactEndedDelegate.BindUFunction(this, FName("OnHitReactMontageEnded"));
                AnimInstance->Montage_SetEndDelegate(HitReactEndedDelegate, SelectedHitReact);

                // 绑定蒙太奇通知回调，用于在受击后半段开启预输入窗口
                AnimInstance->OnPlayMontageNotifyBegin.AddDynamic(this, &UCombatComponent::OnHitReactMontageNotifyBegin);
            }
        }
    }
    else
    {
        // 没有受击蒙太奇，直接恢复 Idle
        SetCombatState(ECombatState::Idle);
    }

    return true;
}

UAnimMontage* UCombatComponent::SelectDirectionalHitReact(AActor* DamageCauser) const
{
    // 没有攻击者信息时，默认使用前方受击蒙太奇
    if (!IsValid(DamageCauser))
    {
        return HitReactMontage_F;
    }

    AActor* Owner = GetOwner();
    if (!Owner)
    {
        return HitReactMontage_F;
    }

    // 计算攻击者相对于受击者的方向（水平面上）
    FVector HitDirection = (DamageCauser->GetActorLocation() - Owner->GetActorLocation()).GetSafeNormal2D();
    FVector OwnerForward = Owner->GetActorForwardVector().GetSafeNormal2D();
    FVector OwnerRight = Owner->GetActorRightVector().GetSafeNormal2D();

    // 计算攻击方向与受击者前方的点积和叉积
    // DotForward > 0 表示攻击者在前方，< 0 表示在后方
    // DotRight > 0 表示攻击者在右侧，< 0 表示在左侧
    float DotForward = FVector::DotProduct(HitDirection, OwnerForward);
    float DotRight = FVector::DotProduct(HitDirection, OwnerRight);

    UAnimMontage* SelectedMontage = nullptr;

    // 根据点积判断主要方向（取绝对值较大的轴作为主方向）
    if (FMath::Abs(DotForward) >= FMath::Abs(DotRight))
    {
        // 前后方向为主
        if (DotForward >= 0.0f)
        {
            // 攻击者在前方 → 播放前方受击动画（向后仰）
            SelectedMontage = HitReactMontage_F;
        }
        else
        {
            // 攻击者在后方 → 播放后方受击动画（向前弯）
            SelectedMontage = HitReactMontage_B;
        }
    }
    else
    {
        // 左右方向为主
        if (DotRight >= 0.0f)
        {
            // 攻击者在右侧 → 播放右侧受击动画（向左歪）
            SelectedMontage = HitReactMontage_R;
        }
        else
        {
            // 攻击者在左侧 → 播放左侧受击动画（向右歪）
            SelectedMontage = HitReactMontage_L;
        }
    }

    UE_LOG(LogTemp, Log, TEXT("Directional HitReact: DotFwd=%.2f, DotRight=%.2f, Montage=%s"),
           DotForward, DotRight, SelectedMontage ? *SelectedMontage->GetName() : TEXT("None"));

    return SelectedMontage;
}

bool UCombatComponent::IsAnyHitReactMontage(UAnimMontage* Montage) const
{
    if (!Montage)
    {
        return false;
    }

    return Montage == HitReactMontage_F
        || Montage == HitReactMontage_B
        || Montage == HitReactMontage_L
        || Montage == HitReactMontage_R;
}

void UCombatComponent::OnHitReactMontageEnded(UAnimMontage *Montage, bool bInterrupted)
{
    UE_LOG(LogTemp, Warning, TEXT("OnHitReactMontageEnded: bInterrupted=%s, CombatState=%s"),
           bInterrupted ? TEXT("true") : TEXT("false"),
           *UEnum::GetValueAsString(CombatState));

    // 如果有任何受击蒙太奇仍在播放，说明是新的受击实例触发了旧实例的 blend out 回调
    // 此时应忽略旧实例的回调，避免错误地将状态恢复为 Idle
    USkeletalMeshComponent *MeshCheck = CachedOwnerMesh.Get();
    if (MeshCheck)
    {
        UAnimInstance *AnimInstanceCheck = MeshCheck->GetAnimInstance();
        if (AnimInstanceCheck)
        {
            // 检查当前是否有任何受击蒙太奇正在播放
            UAnimMontage* CurrentMontage = AnimInstanceCheck->GetCurrentActiveMontage();
            if (CurrentMontage && IsAnyHitReactMontage(CurrentMontage) && CurrentMontage != Montage)
            {
                UE_LOG(LogTemp, Warning, TEXT("OnHitReactMontageEnded: Another HitReact montage still playing, ignoring stale callback"));
                return;
            }
        }
    }

    // 如果是被新的受击打断（bInterrupted=true），不恢复 Idle，也不消费缓存输入
    // 新的受击会重新设置 Staggered 状态，由新的受击蒙太奇结束时再处理
    if (bInterrupted)
    {
        UE_LOG(LogTemp, Warning, TEXT("OnHitReactMontageEnded: Interrupted, skipping state recovery"));
        // 解绑通知回调（防止泄漏）
        USkeletalMeshComponent *Mesh = CachedOwnerMesh.Get();
        if (Mesh)
        {
            UAnimInstance *AnimInstance = Mesh->GetAnimInstance();
            if (AnimInstance)
            {
                AnimInstance->OnPlayMontageNotifyBegin.RemoveDynamic(this, &UCombatComponent::OnHitReactMontageNotifyBegin);
            }
        }
        return;
    }

    // 受击蒙太奇自然播放完毕，恢复 Idle 状态
    if (CombatState == ECombatState::Staggered)
    {
        // 解绑通知多播委托（单播结束委托由蒙太奇实例自动管理，无需手动解绑）
        USkeletalMeshComponent *Mesh = CachedOwnerMesh.Get();
        if (Mesh)
        {
            UAnimInstance *AnimInstance = Mesh->GetAnimInstance();
            if (AnimInstance)
            {
                AnimInstance->OnPlayMontageNotifyBegin.RemoveDynamic(this, &UCombatComponent::OnHitReactMontageNotifyBegin);
            }
        }

        SetCombatState(ECombatState::Idle);
        // 尝试执行缓存的输入
        ConsumeBufferedInput();
    }
}

void UCombatComponent::OnHitReactMontageNotifyBegin(FName NotifyName, const FBranchingPointNotifyPayload &BranchingPointPayload)
{
    // 收到 InputBufferWindow 通知时，开启预输入窗口
    // 允许玩家在受击硬直后半段提前输入下一个动作
    if (NotifyName == FName(TEXT("InputBufferWindow")))
    {
        bCanBufferInput = true;
    }
}

void UCombatComponent::InterruptCurrentAction()
{
    USkeletalMeshComponent *Mesh = CachedOwnerMesh.Get();
    UAnimInstance *AnimInstance = nullptr;
    if (Mesh)
    {
        AnimInstance = Mesh->GetAnimInstance();
    }

    switch (CombatState)
    {
    case ECombatState::Attacking:
        // 中断轻攻击：停止蒙太奇、解绑回调、重置攻击状态
        if (AnimInstance)
        {
            AnimInstance->OnPlayMontageNotifyBegin.RemoveDynamic(this, &UCombatComponent::OnAttackMontageNotifyBegin);
            if (AttackMontage)
            {
                AnimInstance->Montage_Stop(0.15f, AttackMontage);
            }
        }
        AttackComboIndex = 0;
        AttackPhase = EAttackPhase::None;
        AttackRotationElapsed = 0.0f;
        break;

    case ECombatState::HeavyAttacking:
        // 中断重攻击：停止蒙太奇、解绑回调
        if (AnimInstance)
        {
            AnimInstance->OnPlayMontageNotifyBegin.RemoveDynamic(this, &UCombatComponent::OnHeavyAttackMontageNotifyBegin);
            if (HeavyAttackMontage)
            {
                AnimInstance->Montage_Stop(0.15f, HeavyAttackMontage);
            }
        }
        bHeavyAttackLaunched = false;
        AttackRotationElapsed = 0.0f;
        break;

    case ECombatState::FallingAttacking:
        // 中断下落攻击
        if (AnimInstance && FallingAttackMontage)
        {
            AnimInstance->Montage_Stop(0.15f, FallingAttackMontage);
        }
        break;

    case ECombatState::Dodging:
        // 中断翻滚：停止蒙太奇、解绑回调、关闭无敌帧
        if (AnimInstance)
        {
            AnimInstance->OnPlayMontageNotifyBegin.RemoveDynamic(this, &UCombatComponent::OnDodgeMontageNotifyBegin);
            // 停止当前正在播放的翻滚蒙太奇
            AnimInstance->Montage_Stop(0.15f);
        }
        bIsInvincible = false;
        break;

    case ECombatState::Staggered:
        // 连续受击时：解绑通知回调
        if (AnimInstance)
        {
            AnimInstance->OnPlayMontageNotifyBegin.RemoveDynamic(this, &UCombatComponent::OnHitReactMontageNotifyBegin);
        }
        break;

    default:
        break;
    }

    // 清空输入缓存
    ClearBufferedInput();
    // 关闭武器碰撞检测（如果正在攻击中被打断）
    EndDamageTrace();

    // 清除 Hit Lag 定时器并恢复蒙太奇播放速率
    if (GetWorld())
    {
        if (GetWorld()->GetTimerManager().IsTimerActive(HitLagTimerHandle))
        {
            GetWorld()->GetTimerManager().ClearTimer(HitLagTimerHandle);
            // 恢复蒙太奇播放速率（如果蒙太奇还在播放的话）
            USkeletalMeshComponent *MeshForLag = CachedOwnerMesh.Get();
            if (MeshForLag)
            {
                UAnimInstance *AnimForLag = MeshForLag->GetAnimInstance();
                if (AnimForLag)
                {
                    UAnimMontage *CurrentMontage = GetCurrentAttackMontage();
                    if (CurrentMontage && AnimForLag->Montage_IsPlaying(CurrentMontage))
                    {
                        AnimForLag->Montage_SetPlayRate(CurrentMontage, 1.0f);
                    }
                }
            }
        }
    }
}

// ============================================================================
// 死亡清理
// ============================================================================

void UCombatComponent::HandleDeath()
{
    // 中断当前正在执行的动作（攻击/翻滚等），清理蒙太奇回调和输入缓存
    InterruptCurrentAction();

    // 清除 Hit Lag 定时器（死亡后不再需要恢复动画速率）
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(HitLagTimerHandle);
    }

    // 解锁当前锁定的目标（销毁锁定UI、恢复旋转模式）
    if (IsValid(TargetLockActor))
    {
        UnlockTarget();
    }

    // 清除遮挡检测延迟定时器
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(OcclusionTimerHandle);
    }

    // 关闭无敌帧
    bIsInvincible = false;

    // 停止 Tick，死亡后不再需要每帧检测
    SetComponentTickEnabled(false);

    UE_LOG(LogTemp, Log, TEXT("CombatComponent: HandleDeath cleanup completed"));
}

// ============================================================================
// 方向性攻击系统（Aim Offset Pitch）
// ============================================================================

void UCombatComponent::UpdateAimPitch(float DeltaTime)
{
    if (!bEnableDirectionalAttack)
    {
        AttackAimPitch = 0.0f;
        return;
    }

    // 目标 Pitch：攻击状态下或锁定目标时取控制器（摄像机）的 Pitch，其余情况归零
    float TargetPitch = 0.0f;

    if (IsInAnyAttackState() || IsValid(TargetLockActor))
    {
        ACharacter *OwnerChar = CachedOwnerCharacter.Get();
        if (OwnerChar)
        {
            AController *PC = OwnerChar->GetController();
            if (PC)
            {
                TargetPitch = PC->GetControlRotation().Pitch;

                // 将 Pitch 从 [0, 360) 归一化到 [-180, 180) 范围
                if (TargetPitch > 180.0f)
                {
                    TargetPitch -= 360.0f;
                }

                // 限制 Pitch 范围
                TargetPitch = FMath::Clamp(TargetPitch, -AttackAimPitchClamp, AttackAimPitchClamp);
            }
        }
    }

    // 平滑插值到目标 Pitch，避免突变
    AttackAimPitch = FMath::FInterpTo(AttackAimPitch, TargetPitch, DeltaTime, AttackAimPitchInterpSpeed);
}