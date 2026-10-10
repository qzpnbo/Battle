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
#include "Component/AttributeComponent.h"
#include "Sound/SoundBase.h"
#include "Engine/DamageEvents.h"
#include "TimerManager.h"
#include "DrawDebugHelpers.h"
#include "HAL/IConsoleManager.h"
#include <Components/BoxComponent.h>

// ============================================================================
// 战斗调试显示（控制台输入 Battle.Debug 1 / 2 开启，0 关闭）
// ============================================================================

namespace BattleCombatDebug
{
    static TAutoConsoleVariable<int32> CVarBattleDebug(
        TEXT("Battle.Debug"),
        0,
        TEXT("Combat debug display. 0 = off, 1 = state/attributes text above characters, 2 = also draw block arc and weapon hitbox"),
        ECVF_Default);

    static FColor GetStateColor(ECombatState State)
    {
        switch (State)
        {
        case ECombatState::Attacking:
        case ECombatState::HeavyAttacking:
        case ECombatState::FallingAttacking:
        case ECombatState::SpecialAttacking:
            return FColor::Red;
        case ECombatState::Dodging:
            return FColor::Cyan;
        case ECombatState::Blocking:
            return FColor(80, 140, 255);
        case ECombatState::Staggered:
            return FColor::Yellow;
        case ECombatState::Dead:
            return FColor::Silver;
        default:
            return FColor::White;
        }
    }

    static void Draw(const UCombatComponent &Combat)
    {
#if ENABLE_DRAW_DEBUG
        const int32 Level = CVarBattleDebug.GetValueOnGameThread();
        const AActor *Owner = Combat.GetOwner();
        UWorld *World = Combat.GetWorld();
        if (Level <= 0 || !Owner || !World)
        {
            return;
        }

        const ECombatState State = Combat.GetCombatState();
        const FColor StateColor = GetStateColor(State);

        // --- 头顶文字：状态 / 攻击阶段 / 属性 / 标记 ---
        // 注：调试字体不支持中文，这里统一使用英文
        FString Text = FString::Printf(TEXT("%s\n%s"),
            *Owner->GetActorNameOrLabel(),
            *StaticEnum<ECombatState>()->GetNameStringByValue(static_cast<int64>(State)));

        if (State == ECombatState::Attacking)
        {
            Text += FString::Printf(TEXT(" [%s #%d]"),
                *StaticEnum<EAttackPhase>()->GetNameStringByValue(static_cast<int64>(Combat.AttackPhase)),
                Combat.AttackComboIndex + 1);
        }

        if (const UAttributeComponent *Attributes = Owner->FindComponentByClass<UAttributeComponent>())
        {
            Text += FString::Printf(TEXT("\nHP %.0f/%.0f"), Attributes->GetHealth(), Attributes->GetMaxHealth());
            if (Attributes->bUseStamina)
            {
                Text += FString::Printf(TEXT("  ST %.0f/%.0f"), Attributes->GetStamina(), Attributes->GetMaxStamina());
            }
            Text += FString::Printf(TEXT("  Poise %.0f/%.0f"), Attributes->GetPoise(), Attributes->MaxPoise);
        }

        FString Flags;
        if (Combat.bIsInvincible)
        {
            Flags += TEXT("[I-Frame] ");
        }
        if (Combat.IsParryStunned())
        {
            Flags += TEXT("[Parried] ");
        }
        if (Combat.BufferedAction != EBufferedInputAction::None)
        {
            Flags += FString::Printf(TEXT("[Buffer:%s] "), *StaticEnum<EBufferedInputAction>()->GetNameStringByValue(static_cast<int64>(Combat.BufferedAction)));
        }
        if (!Flags.IsEmpty())
        {
            Text += TEXT("\n") + Flags;
        }

        const FVector TextLocation = Owner->GetActorLocation() + FVector(0.0f, 0.0f, Owner->GetSimpleCollisionHalfHeight() + 45.0f);
        DrawDebugString(World, TextLocation, Text, nullptr, StateColor, 0.0f, true, 1.0f);

        if (Level < 2)
        {
            return;
        }

        // --- 格挡角度：举盾时画出可格挡的扇形边界 ---
        if (State == ECombatState::Blocking)
        {
            const FVector Origin = Owner->GetActorLocation();
            const FVector Forward = Owner->GetActorForwardVector().GetSafeNormal2D();
            const float Radius = 160.0f;
            const FVector Left = Forward.RotateAngleAxis(-Combat.BlockHalfAngle, FVector::UpVector) * Radius;
            const FVector Right = Forward.RotateAngleAxis(Combat.BlockHalfAngle, FVector::UpVector) * Radius;
            DrawDebugLine(World, Origin, Origin + Left, StateColor, false, 0.0f, 0, 2.0f);
            DrawDebugLine(World, Origin, Origin + Right, StateColor, false, 0.0f, 0, 2.0f);
            DrawDebugLine(World, Origin, Origin + Forward * Radius, StateColor, false, 0.0f, 0, 1.0f);
        }

        // --- 武器判定框：只在判定窗口开启时绘制 ---
        if (const UBoxComponent *Box = Cast<UBoxComponent>(Combat.SwordCollisionRef))
        {
            if (Box->IsCollisionEnabled())
            {
                DrawDebugBox(World, Box->GetComponentLocation(), Box->GetScaledBoxExtent(), Box->GetComponentQuat(), FColor::Red, false, 0.0f, 0, 1.5f);
            }
        }
#endif
    }
}

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

    if (AActor *Owner = GetOwner())
    {
        CachedAttributes = Owner->FindComponentByClass<UAttributeComponent>();
    }
}

void UCombatComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // 弹反慢动作期间角色被销毁（死亡重生、关卡切换），必须恢复全局时间，否则游戏会一直处于慢动作
    StopParrySlowMo();

    Super::EndPlay(EndPlayReason);
}

// Called every frame
void UCombatComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // 锁定目标被销毁（或被 GC 置空）时，后续检测都会因 IsValid 失败而提前返回，
    // 必须在这里主动解锁，否则旋转模式无法恢复、锁定 UI 会残留
    if (bIsTargetLocked && !IsValid(TargetLockActor))
    {
        UnlockTarget();
    }

    HandleFaceTarget(DeltaTime);

    CheckTargetInRange();

    CheckTargetOcclusion();

    UpdateAttackRotation(DeltaTime);

    UpdateAimPitch(DeltaTime);

    // 锁定目标切换冷却时间递减
    if (TargetSwitchCooldownRemaining > 0.0f)
    {
        TargetSwitchCooldownRemaining = FMath::Max(0.0f, TargetSwitchCooldownRemaining - DeltaTime);
    }

    // 调试显示（Battle.Debug）
    BattleCombatDebug::Draw(*this);
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

        // --- 只锁定带战斗组件的敌对且存活目标 ---
        UCombatComponent *CandidateCombat = Candidate->FindComponentByClass<UCombatComponent>();
        if (!CandidateCombat || !IsHostileTo(Candidate) || CandidateCombat->GetCombatState() == ECombatState::Dead)
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
        float AngleDeg = SafeAngleDegrees(CamForward, DirToCandidate);

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
    bIsTargetLocked = true;

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
    // 不依赖 IsValid(TargetLockActor)：目标可能已被销毁，但清理工作仍必须执行
    const bool bWasLocked = bIsTargetLocked || TargetLockActor != nullptr;

    TargetLockActor = nullptr;
    bIsTargetLocked = false;

    // 只有真正处于锁定状态时才恢复旋转模式，避免影响未锁定过的角色（如 AI）
    if (bWasLocked)
    {
        ACharacter *OwnerCharacter = CachedOwnerCharacter.Get();
        if (OwnerCharacter)
        {
            UCharacterMovementComponent *MoveComp = OwnerCharacter->GetCharacterMovement();
            if (MoveComp)
            {
                MoveComp->bOrientRotationToMovement = true;
            }
        }
    }

    if (IsValid(TargetLockWidget))
    {
        TargetLockWidget->Destroy();
    }
    TargetLockWidget = nullptr;

    // 清除遮挡延迟解锁定时器，防止旧定时器在重新锁定后误解锁新目标
    if (UWorld *World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(OcclusionTimerHandle);
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

        // 只切换到带战斗组件的敌对且存活目标
        UCombatComponent *CandidateCombat = Candidate->FindComponentByClass<UCombatComponent>();
        if (!CandidateCombat || !IsHostileTo(Candidate) || CandidateCombat->GetCombatState() == ECombatState::Dead)
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
        const float AngleDeg = SafeAngleDegrees(CamForward, DirToCandidateNormalized);
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

    // 默认关闭武器碰撞，只在 AnimNotifyState_Damage 窗口内开启
    // （运行时再设置一次，防止蓝图里覆盖了碰撞设置导致非攻击帧也能造成伤害）
    SwordCollisionRef->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    // 绑定 Overlap 回调
    SwordCollisionRef->OnComponentBeginOverlap.AddUniqueDynamic(this, &UCombatComponent::OnSwordOverlapBegin);
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

    // 同阵营不造成伤害（如 Boss 挥刀误伤小怪）
    if (!IsHostileTo(OtherActor))
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

    if (!OtherActor->CanBeDamaged())
    {
        return;
    }

    // 最终伤害 = 武器基础伤害 × 当前招式倍率（招式规格在开始攻击时写入）
    const float Damage = SwordDamage * CurrentAttackDamageMultiplier;

    // 自定义伤害事件：携带削韧值与可否弹反，供受击方 ResolveIncomingHit 使用
    FBattleDamageEvent DamageEvent;
    DamageEvent.PoiseDamage = CurrentAttackPoiseDamage;
    DamageEvent.bCanBeParried = bCurrentAttackCanBeParried;
    DamageEvent.bCanBeBlocked = true;

    // 应用伤害（返回实际造成的伤害，目标无敌帧/被弹反/已死亡时为 0）
    const float AppliedDamage = OtherActor->TakeDamage(Damage, DamageEvent, InstigatorController, GetOwner());

    UE_LOG(LogTemp, Warning, TEXT("Sword Overlap Hit: %s, Damage: %.1f, Applied: %.1f (State: %s)"),
           *OtherActor->GetName(), Damage, AppliedDamage, *UEnum::GetValueAsString(CombatState));

    // 只有真正造成伤害才触发命中反馈（被无敌帧闪避或打在尸体上时不顿帧）
    if (AppliedDamage > 0.0f)
    {
        ApplyHitFeedback();
    }
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
    case ECombatState::SpecialAttacking:
        return CurrentSpecialMontage;
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

    // 使用普通游戏时间定时器在 LagDuration 后恢复播放速率
    // 注意：该定时器受全局时间膨胀影响；本项目采用局部 Hit Lag 方案，不修改全局时间膨胀
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
        // 恢复到动作速率（而非固定 1.0），Boss 二阶段加速时不会被顿帧"还原"
        AnimInstance->Montage_SetPlayRate(CurrentMontage, ActionPlayRate);
        UE_LOG(LogTemp, Log, TEXT("Hit Lag recovered: Rate restored to %.2f"), ActionPlayRate);
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

        OnCombatStateTransition(OldState, NewState);

        // 进入死亡状态时执行完整清理
        if (NewState == ECombatState::Dead)
        {
            HandleDeath();
        }
    }
}

void UCombatComponent::OnCombatStateTransition(ECombatState OldState, ECombatState NewState)
{
    // --- 格挡移速：进入时减速，离开时恢复 ---
    ACharacter *OwnerChar = CachedOwnerCharacter.Get();
    UCharacterMovementComponent *MoveComp = OwnerChar ? OwnerChar->GetCharacterMovement() : nullptr;
    if (MoveComp)
    {
        if (NewState == ECombatState::Blocking && OldState != ECombatState::Blocking)
        {
            SavedMaxWalkSpeed = MoveComp->MaxWalkSpeed;
            MoveComp->MaxWalkSpeed = SavedMaxWalkSpeed * BlockWalkSpeedMultiplier;
        }
        else if (OldState == ECombatState::Blocking && NewState != ECombatState::Blocking && SavedMaxWalkSpeed > 0.0f)
        {
            MoveComp->MaxWalkSpeed = SavedMaxWalkSpeed;
        }
    }

    // --- 耐力回复：出招/翻滚期间暂停，举盾期间减缓 ---
    if (UAttributeComponent *Attributes = CachedAttributes.Get())
    {
        const bool bSpendingState = NewState == ECombatState::Attacking || NewState == ECombatState::HeavyAttacking ||
                                    NewState == ECombatState::FallingAttacking || NewState == ECombatState::Dodging ||
                                    NewState == ECombatState::SpecialAttacking;
        Attributes->SetStaminaRegenPaused(bSpendingState);
        Attributes->SetStaminaRegenMultiplier(NewState == ECombatState::Blocking ? BlockStaminaRegenMultiplier : 1.0f);
    }
}

bool UCombatComponent::IsInAnyAttackState() const
{
    return CombatState == ECombatState::Attacking || CombatState == ECombatState::HeavyAttacking ||
           CombatState == ECombatState::FallingAttacking || CombatState == ECombatState::SpecialAttacking;
}

void UCombatComponent::ReturnToIdle()
{
    SetCombatState(ECombatState::Idle);

    // 尝试执行缓存的输入（跨动作预输入）
    ConsumeBufferedInput();

    // 没有预输入被执行、且格挡键仍按住 → 自动恢复举盾（类魂：出完招按住 L1 会直接举盾）
    if (CombatState == ECombatState::Idle && bBlockInputHeld)
    {
        EnterBlock();
    }
}

void UCombatComponent::SetCurrentAttackSpec(float DamageMultiplier, float PoiseDamage, bool bCanBeParried)
{
    CurrentAttackDamageMultiplier = DamageMultiplier;
    CurrentAttackPoiseDamage = PoiseDamage;
    bCurrentAttackCanBeParried = bCanBeParried;
}

UAnimInstance *UCombatComponent::GetOwnerAnimInstance() const
{
    USkeletalMeshComponent *Mesh = CachedOwnerMesh.Get();
    return Mesh ? Mesh->GetAnimInstance() : nullptr;
}

bool UCombatComponent::HasStaminaForAction() const
{
    const UAttributeComponent *Attributes = CachedAttributes.Get();
    return !Attributes || Attributes->HasStamina();
}

void UCombatComponent::ConsumeStamina(float Cost)
{
    if (UAttributeComponent *Attributes = CachedAttributes.Get())
    {
        Attributes->TryConsumeStamina(Cost);
    }
}

void UCombatComponent::PlayCombatSound(USoundBase *Sound) const
{
    if (Sound && GetOwner())
    {
        UGameplayStatics::PlaySoundAtLocation(this, Sound, GetOwner()->GetActorLocation());
    }
}

bool UCombatComponent::IsHostileTo(const AActor *Other) const
{
    if (!IsValid(Other) || Other == GetOwner())
    {
        return false;
    }

    const UCombatComponent *OtherCombat = Other->FindComponentByClass<UCombatComponent>();
    if (!OtherCombat)
    {
        // 非战斗单位（可破坏物等）允许被攻击
        return true;
    }

    return OtherCombat->Team != Team;
}

float UCombatComponent::SafeAngleDegrees(const FVector &A, const FVector &B)
{
    const float Dot = FMath::Clamp(static_cast<float>(FVector::DotProduct(A, B)), -1.0f, 1.0f);
    return FMath::RadiansToDegrees(FMath::Acos(Dot));
}

// ============================================================================
// 攻击系统
// ============================================================================

void UCombatComponent::Attack()
{
    // 举盾中允许直接出招（类魂：格挡姿态下可以直接攻击），耐力不足则保持举盾
    if (CombatState == ECombatState::Blocking)
    {
        if (!HasStaminaForAction())
        {
            return;
        }
        ExitBlock(false);
        SetCombatState(ECombatState::Idle);
    }

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

        if (!FallingAttackMontage || !HasStaminaForAction())
        {
            return;
        }

        UAnimInstance *AnimInstance = GetOwnerAnimInstance();
        if (!AnimInstance)
        {
            return;
        }

        SetCombatState(ECombatState::FallingAttacking);
        ClearBufferedInput();
        SetCurrentAttackSpec(FallingAttackDamageMultiplier, FallingAttackPoiseDamage, false);
        ConsumeStamina(FallingAttackStaminaCost);

        // 播放下落攻击蒙太奇
        AnimInstance->Montage_Play(FallingAttackMontage, ActionPlayRate, EMontagePlayReturnType::MontageLength, 0.0f);

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

    // 未在攻击中，开始全新攻击：先检查资源，全部满足后再切换状态（避免状态"闪一下"又回退）
    UAnimInstance *AnimInstance = GetOwnerAnimInstance();
    if (!AnimInstance || !AttackMontage || !HasStaminaForAction())
    {
        return;
    }

    SetCombatState(ECombatState::Attacking);
    ClearBufferedInput();
    AttackComboIndex = 0;
    AttackPhase = EAttackPhase::Startup;
    SetCurrentAttackSpec(1.0f, LightAttackPoiseDamage, true);
    ConsumeStamina(LightAttackStaminaCost);

    // 设置攻击初始朝向（以角色当前面朝方向出招）
    SetAttackRotation();

    // 播放攻击蒙太奇（从第一段 Section 开始）
    AnimInstance->Montage_Play(AttackMontage, ActionPlayRate, EMontagePlayReturnType::MontageLength, 0.0f);

    // 绑定蒙太奇结束回调（On Completed / On Interrupted）单播委托
    // 注意：必须在 Montage_Play 之后调用，否则没有活跃的蒙太奇实例，委托绑定会静默失败
    FOnMontageEnded MontageEndedDelegate;
    MontageEndedDelegate.BindUObject(this, &UCombatComponent::OnAttackMontageEnded);
    AnimInstance->Montage_SetEndDelegate(MontageEndedDelegate, AttackMontage);

    // 绑定蒙太奇通知回调（On Notify Begin），用于连击判定 多播动态委托
    // 使用 AddUniqueDynamic：异常路径下未解绑时再次绑定不会触发 ensure / 重复回调
    AnimInstance->OnPlayMontageNotifyBegin.AddUniqueDynamic(this, &UCombatComponent::OnAttackMontageNotifyBegin);

    // 攻击音效
    PlayCombatSound(attackSound);
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
        ReturnToIdle();
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

    // 耐力耗尽则连击断开：当前段正常播完后结束
    if (!HasStaminaForAction())
    {
        return;
    }
    ConsumeStamina(LightAttackStaminaCost);

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
        // 回到 Idle 并执行缓存的输入（跨动作预输入，如翻滚、重攻击等）
        // 连击跳转已在 OnAttackMontageNotifyBegin 中通过 SetNextSection 处理，
        // 这里消费的攻击缓存来自 InputBufferWindow 阶段，会通过 Attack() 从 S0 重新开始
        ReturnToIdle();
    }
}

// --- 重攻击系统 ---

float UCombatComponent::HeavyAttack()
{
    // 举盾中允许直接重攻击，耐力不足则保持举盾
    if (CombatState == ECombatState::Blocking)
    {
        if (!HasStaminaForAction())
        {
            return 0.0f;
        }
        ExitBlock(false);
        SetCombatState(ECombatState::Idle);
    }

    // 只有 Idle 状态才允许重攻击，其他所有状态一律拒绝并尝试缓存输入
    if (CombatState != ECombatState::Idle)
    {
        BufferInput(EBufferedInputAction::HeavyAttack);
        return 0.0f;
    }

    // 跳跃中不允许重攻击
    ACharacter *OwnerChar = CachedOwnerCharacter.Get();
    if (OwnerChar && OwnerChar->GetCharacterMovement() && OwnerChar->GetCharacterMovement()->IsFalling())
    {
        return 0.0f;
    }

    UAnimInstance *AnimInstance = GetOwnerAnimInstance();
    if (!HeavyAttackMontage || !AnimInstance || !HasStaminaForAction())
    {
        return 0.0f;
    }

    SetCombatState(ECombatState::HeavyAttacking);
    ClearBufferedInput();
    SetCurrentAttackSpec(HeavyAttackDamageMultiplier, HeavyAttackPoiseDamage, false);
    ConsumeStamina(HeavyAttackStaminaCost);

    // 设置攻击初始朝向（以角色当前面朝方向出招）
    SetAttackRotation();

    // 先播放攻击蒙太奇
    float Duration = AnimInstance->Montage_Play(HeavyAttackMontage, ActionPlayRate, EMontagePlayReturnType::MontageLength, 0.0f);

    // 绑定重攻击蒙太奇结束回调
    FOnMontageEnded HeavyEndedDelegate;
    HeavyEndedDelegate.BindUObject(this, &UCombatComponent::OnHeavyAttackMontageEnded);
    AnimInstance->Montage_SetEndDelegate(HeavyEndedDelegate, HeavyAttackMontage);

    // 绑定蒙太奇通知回调
    AnimInstance->OnPlayMontageNotifyBegin.AddUniqueDynamic(this, &UCombatComponent::OnHeavyAttackMontageNotifyBegin);

    return Duration;
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
    if (CombatState == ECombatState::HeavyAttacking)
    {
        ReturnToIdle();
    }
}

void UCombatComponent::OnHeavyAttackMontageNotifyBegin(FName NotifyName, const FBranchingPointNotifyPayload &BranchingPointPayload)
{
    // OnPlayMontageNotifyBegin 是 AnimInstance 级多播，所有蒙太奇的通知都会进入这里，只处理来自重攻击蒙太奇的通知
    if (!HeavyAttackMontage || BranchingPointPayload.SequenceAsset != HeavyAttackMontage)
    {
        return;
    }

    // 收到 InputBufferWindow 通知时，开启跨动作预输入窗口
    if (NotifyName == FName(TEXT("InputBufferWindow")))
    {
        bCanBufferInput = true;
    }
}

void UCombatComponent::OnAttackMontageNotifyBegin(FName NotifyName, const FBranchingPointNotifyPayload &BranchingPointPayload)
{
    // 只处理来自轻攻击连击蒙太奇的通知
    if (!AttackMontage || BranchingPointPayload.SequenceAsset != AttackMontage)
    {
        return;
    }

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
    // 举盾中允许直接翻滚，耐力不足则保持举盾
    if (CombatState == ECombatState::Blocking)
    {
        if (!HasStaminaForAction())
        {
            return;
        }
        ExitBlock(false);
        SetCombatState(ECombatState::Idle);
    }

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

    // 耐力耗尽时无法翻滚
    if (!HasStaminaForAction())
    {
        return;
    }

    ACharacter *OwnerCharacter = CachedOwnerCharacter.Get();
    if (!OwnerCharacter)
    {
        return;
    }

    // 翻滚方向规则（类魂惯例）：
    //   无移动输入：原地后撤步（使用后翻滚蒙太奇）
    //   锁定目标 + 有输入：按输入方向四向翻滚
    //   未锁定 + 有输入：向前翻滚（角色朝向由 OrientRotationToMovement 决定）
    // AI 没有输入概念，保持原有行为（不走后撤步分支）
    const bool bIsLocked = IsValid(TargetLockActor);
    const bool bNoMoveInput = OwnerCharacter->IsPlayerControlled() && !bHasMovementInput;

    EMovementDirection DodgeDirection = EMovementDirection::Forward;
    if (bNoMoveInput)
    {
        DodgeDirection = EMovementDirection::Backward;
    }
    else if (bIsLocked)
    {
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

    UAnimInstance *AnimInstance = GetOwnerAnimInstance();
    if (!SelectedMontage || !AnimInstance)
    {
        return;
    }

    // 资源检查全部通过后再切换状态并扣耐力
    SetCombatState(ECombatState::Dodging);
    ClearBufferedInput();
    ConsumeStamina(DodgeStaminaCost);

    // 播放翻滚蒙太奇
    CurrentDodgeMontage = SelectedMontage;
    AnimInstance->Montage_Play(SelectedMontage, 1.0f, EMontagePlayReturnType::MontageLength, 0.0f);

    // 绑定蒙太奇结束回调
    FOnMontageEnded DodgeEndedDelegate;
    DodgeEndedDelegate.BindUObject(this, &UCombatComponent::OnDodgeMontageEnded);
    AnimInstance->Montage_SetEndDelegate(DodgeEndedDelegate, SelectedMontage);

    // 绑定蒙太奇通知回调，用于开启预输入窗口
    AnimInstance->OnPlayMontageNotifyBegin.AddUniqueDynamic(this, &UCombatComponent::OnDodgeMontageNotifyBegin);
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

    // 必须在 ConsumeBufferedInput 之前清空：连续翻滚会在其中设置新的 CurrentDodgeMontage
    if (Montage == CurrentDodgeMontage)
    {
        CurrentDodgeMontage = nullptr;
    }

    // 仅在状态仍为 Dodging 时重置为 Idle
    // 避免覆盖更高优先级的状态（如受击硬直 Staggered）
    if (CombatState == ECombatState::Dodging)
    {
        // 回到 Idle 并执行缓存的输入（包括连续翻滚和跨动作预输入）
        ReturnToIdle();
    }
}

void UCombatComponent::OnDodgeMontageNotifyBegin(FName NotifyName, const FBranchingPointNotifyPayload &BranchingPointPayload)
{
    // 只处理来自当前翻滚蒙太奇的通知
    if (!CurrentDodgeMontage || BranchingPointPayload.SequenceAsset != CurrentDodgeMontage)
    {
        return;
    }

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

EHitResponse UCombatComponent::ResolveIncomingHit(FIncomingHit &Hit, AActor *DamageCauser)
{
    // 已死亡不处理
    if (CombatState == ECombatState::Dead)
    {
        return EHitResponse::Ignored;
    }

    // 无敌帧期间免疫伤害（翻滚 I-Frame / 特殊动作无敌）
    if (bIsInvincible)
    {
        UE_LOG(LogTemp, Log, TEXT("Damage blocked by I-Frame!"));
        return EHitResponse::Dodged;
    }

    // 被弹反后的硬直期间：这一击伤害提高，且无法格挡/弹反（弹反后的反击）
    if (bIsParryStunned)
    {
        Hit.Damage *= ParriedDamageMultiplier;
        Hit.bCanBeBlocked = false;
        Hit.bCanBeParried = false;
        bIsParryStunned = false;
    }

    UAttributeComponent *Attributes = CachedAttributes.Get();

    // ---------------------------------------------------------------------------
    // 格挡：必须处于举盾状态、攻击可被格挡、且攻击来自正面
    // ---------------------------------------------------------------------------
    if (CombatState == ECombatState::Blocking && Hit.bCanBeBlocked && IsAttackerInBlockArc(DamageCauser))
    {
        // 举盾后的短时间窗口内被击中 → 弹反
        const UWorld *World = GetWorld();
        const float TimeSinceBlockStart = World ? World->GetTimeSeconds() - BlockStartTime : TNumericLimits<float>::Max();
        if (Hit.bCanBeParried && TimeSinceBlockStart <= ParryWindow)
        {
            HandleParrySuccess(DamageCauser);
            return EHitResponse::Parried;
        }

        // 普通格挡：减伤，按格挡前的伤害扣除耐力
        const float DamageBeforeBlock = Hit.Damage;
        Hit.Damage *= (1.0f - BlockDamageReduction);

        const bool bGuardHolds = !Attributes || Attributes->DrainStamina(DamageBeforeBlock * BlockStaminaCostPerDamage);
        if (!bGuardHolds)
        {
            // 耐力被打空 → 破防硬直
            InterruptCurrentAction();
            SetCombatState(ECombatState::Staggered);
            PlayStaggerMontage(GuardBreakMontage ? GuardBreakMontage : SelectDirectionalHitReact(DamageCauser));
            UE_LOG(LogTemp, Log, TEXT("Guard broken!"));
            return EHitResponse::GuardBroken;
        }

        PlayCombatSound(BlockSound);
        PlayBlockReaction(BlockReactMontage);
        return EHitResponse::Blocked;
    }

    // ---------------------------------------------------------------------------
    // 韧性：出招期间享受霸体倍率，韧性被打空才进入硬直
    // ---------------------------------------------------------------------------
    float PoiseDamage = Hit.PoiseDamage;
    if (IsInAnyAttackState())
    {
        PoiseDamage *= AttackingPoiseDamageScale;
    }

    const bool bPoiseBroken = !Attributes || Attributes->ApplyPoiseDamage(PoiseDamage);
    if (!bPoiseBroken)
    {
        // 霸体：扣血但不打断当前动作
        return EHitResponse::Hit;
    }

    // 中断当前正在执行的动作（攻击/翻滚/格挡等），播放方向性受击
    InterruptCurrentAction();
    SetCombatState(ECombatState::Staggered);
    PlayStaggerMontage(SelectDirectionalHitReact(DamageCauser));
    PlayCombatSound(HitSound);
    return EHitResponse::Staggered;
}

void UCombatComponent::PlayStaggerMontage(UAnimMontage *Montage)
{
    UAnimInstance *AnimInstance = GetOwnerAnimInstance();
    if (!Montage || !AnimInstance)
    {
        // 没有受击蒙太奇，直接恢复 Idle
        CurrentStaggerMontage = nullptr;
        bIsParryStunned = false;
        ReturnToIdle();
        return;
    }

    CurrentStaggerMontage = Montage;
    AnimInstance->Montage_Play(Montage, 1.0f, EMontagePlayReturnType::MontageLength, 0.0f);

    FOnMontageEnded HitReactEndedDelegate;
    HitReactEndedDelegate.BindUObject(this, &UCombatComponent::OnHitReactMontageEnded);
    AnimInstance->Montage_SetEndDelegate(HitReactEndedDelegate, Montage);

    // 绑定蒙太奇通知回调，用于在受击后半段开启预输入窗口
    AnimInstance->OnPlayMontageNotifyBegin.AddUniqueDynamic(this, &UCombatComponent::OnHitReactMontageNotifyBegin);
}

void UCombatComponent::ReceiveParried(AActor *Parrier)
{
    if (CombatState == ECombatState::Dead)
    {
        return;
    }

    InterruptCurrentAction();
    SetCombatState(ECombatState::Staggered);

    if (UAttributeComponent *Attributes = CachedAttributes.Get())
    {
        Attributes->ResetPoise();
    }

    PlayStaggerMontage(ParriedMontage ? ParriedMontage : HitReactMontage_F);

    // 只有真正进入了硬直才标记为"可被反击"（没有蒙太奇时 PlayStaggerMontage 会直接回到 Idle）
    bIsParryStunned = (CombatState == ECombatState::Staggered);

    UE_LOG(LogTemp, Log, TEXT("%s was parried by %s"), *GetNameSafe(GetOwner()), *GetNameSafe(Parrier));
}

// ============================================================================
// 格挡 / 弹反系统
// ============================================================================

void UCombatComponent::StartBlock()
{
    bBlockInputHeld = true;

    // 其他状态下按住格挡：当前动作结束时 ReturnToIdle 会自动举盾
    if (CombatState != ECombatState::Idle)
    {
        return;
    }

    ACharacter *OwnerChar = CachedOwnerCharacter.Get();
    if (OwnerChar && OwnerChar->GetCharacterMovement() && OwnerChar->GetCharacterMovement()->IsFalling())
    {
        return;
    }

    EnterBlock();
}

void UCombatComponent::StopBlock()
{
    bBlockInputHeld = false;

    if (CombatState == ECombatState::Blocking)
    {
        ExitBlock(true);
    }
}

void UCombatComponent::EnterBlock()
{
    if (CombatState != ECombatState::Idle)
    {
        return;
    }

    SetCombatState(ECombatState::Blocking);
    BlockStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

    // 没有配置举盾动画时仍可格挡（纯逻辑），方便先调数值
    PlayBlockLoop();
}

void UCombatComponent::ExitBlock(bool bReturnToIdle)
{
    if (UAnimInstance *AnimInstance = GetOwnerAnimInstance())
    {
        if (BlockMontage)
        {
            AnimInstance->Montage_Stop(0.2f, BlockMontage);
        }
        if (BlockReactMontage)
        {
            AnimInstance->Montage_Stop(0.2f, BlockReactMontage);
        }
        if (ParryMontage)
        {
            AnimInstance->Montage_Stop(0.2f, ParryMontage);
        }
    }

    if (bReturnToIdle && CombatState == ECombatState::Blocking)
    {
        ReturnToIdle();
    }
}

void UCombatComponent::PlayBlockLoop()
{
    UAnimInstance *AnimInstance = GetOwnerAnimInstance();
    if (!AnimInstance || !BlockMontage)
    {
        return;
    }

    AnimInstance->Montage_Play(BlockMontage, 1.0f, EMontagePlayReturnType::MontageLength, 0.0f);

    // 把第一个 Section 的下一段设为自己 → 无限循环，直到 ExitBlock 主动停止
    if (BlockMontage->CompositeSections.Num() > 0)
    {
        const FName LoopSection = BlockMontage->GetSectionName(0);
        AnimInstance->Montage_SetNextSection(LoopSection, LoopSection, BlockMontage);
    }
}

void UCombatComponent::PlayBlockReaction(UAnimMontage *Montage)
{
    UAnimInstance *AnimInstance = GetOwnerAnimInstance();
    if (!AnimInstance || !Montage)
    {
        return;
    }

    AnimInstance->Montage_Play(Montage, 1.0f, EMontagePlayReturnType::MontageLength, 0.0f);

    // 使用 BlendingOut 而非 End：反馈动画开始淡出时就接回举盾循环，过渡更顺滑
    FOnMontageBlendingOutStarted BlendOutDelegate;
    BlendOutDelegate.BindUObject(this, &UCombatComponent::OnBlockReactionMontageEnded);
    AnimInstance->Montage_SetBlendingOutDelegate(BlendOutDelegate, Montage);
}

void UCombatComponent::OnBlockReactionMontageEnded(UAnimMontage *Montage, bool bInterrupted)
{
    // 被打断（松开格挡、出招、受击）时由打断方负责后续状态
    if (!bInterrupted && CombatState == ECombatState::Blocking)
    {
        PlayBlockLoop();
    }
}

bool UCombatComponent::IsAttackerInBlockArc(const AActor *Attacker) const
{
    const AActor *Owner = GetOwner();
    if (!Owner || !IsValid(Attacker))
    {
        // 没有攻击者信息（如环境伤害）视为正面
        return true;
    }

    const FVector ToAttacker = (Attacker->GetActorLocation() - Owner->GetActorLocation()).GetSafeNormal2D();
    if (ToAttacker.IsNearlyZero())
    {
        return true;
    }

    return SafeAngleDegrees(Owner->GetActorForwardVector().GetSafeNormal2D(), ToAttacker) <= BlockHalfAngle;
}

void UCombatComponent::HandleParrySuccess(AActor *Attacker)
{
    // 让攻击者进入被弹反硬直
    if (IsValid(Attacker))
    {
        if (UCombatComponent *AttackerCombat = Attacker->FindComponentByClass<UCombatComponent>())
        {
            AttackerCombat->ReceiveParried(GetOwner());
        }
    }

    PlayCombatSound(ParrySound ? ParrySound : BlockSound);
    PlayBlockReaction(ParryMontage ? ParryMontage : BlockReactMontage);

    // 玩家参与的弹反：慢动作 + 重击镜头震动
    const APawn *OwnerPawn = Cast<APawn>(GetOwner());
    const APawn *AttackerPawn = Cast<APawn>(Attacker);
    const bool bPlayerInvolved = (OwnerPawn && OwnerPawn->IsPlayerControlled()) || (AttackerPawn && AttackerPawn->IsPlayerControlled());
    if (bPlayerInvolved)
    {
        StartParrySlowMo();

        if (bEnableHitCameraShake && HeavyHitCameraShake)
        {
            if (APlayerController *PC = UGameplayStatics::GetPlayerController(this, 0))
            {
                PC->ClientStartCameraShake(HeavyHitCameraShake);
            }
        }
    }

    UE_LOG(LogTemp, Log, TEXT("%s parried %s!"), *GetNameSafe(GetOwner()), *GetNameSafe(Attacker));
}

void UCombatComponent::StartParrySlowMo()
{
    UWorld *World = GetWorld();
    if (!World || ParrySlowMoDuration <= 0.0f || ParrySlowMoTimeDilation >= 1.0f)
    {
        return;
    }

    UGameplayStatics::SetGlobalTimeDilation(this, ParrySlowMoTimeDilation);
    bParrySlowMoActive = true;

    // 定时器按游戏时间计时（受全局时间膨胀影响）：游戏时间 = 真实时长 × 膨胀系数
    World->GetTimerManager().SetTimer(
        ParrySlowMoTimerHandle,
        this,
        &UCombatComponent::StopParrySlowMo,
        ParrySlowMoDuration * ParrySlowMoTimeDilation,
        false);
}

void UCombatComponent::StopParrySlowMo()
{
    if (!bParrySlowMoActive)
    {
        return;
    }

    bParrySlowMoActive = false;
    if (UWorld *World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(ParrySlowMoTimerHandle);
        UGameplayStatics::SetGlobalTimeDilation(this, 1.0f);
    }
}

// ============================================================================
// AI / 特殊动作
// ============================================================================

bool UCombatComponent::PerformAttackMontage(UAnimMontage *Montage, float DamageMultiplier, float PoiseDamage, bool bCanBeParried)
{
    if (!Montage || CombatState != ECombatState::Idle)
    {
        return false;
    }

    ACharacter *OwnerChar = CachedOwnerCharacter.Get();
    if (OwnerChar && OwnerChar->GetCharacterMovement() && OwnerChar->GetCharacterMovement()->IsFalling())
    {
        return false;
    }

    UAnimInstance *AnimInstance = GetOwnerAnimInstance();
    if (!AnimInstance)
    {
        return false;
    }

    SetCombatState(ECombatState::SpecialAttacking);
    ClearBufferedInput();
    SetCurrentAttackSpec(DamageMultiplier, PoiseDamage, bCanBeParried);
    SetAttackRotation();

    CurrentSpecialMontage = Montage;
    bSpecialActionInvincible = false;

    if (AnimInstance->Montage_Play(Montage, ActionPlayRate, EMontagePlayReturnType::MontageLength, 0.0f) <= 0.0f)
    {
        CurrentSpecialMontage = nullptr;
        SetCombatState(ECombatState::Idle);
        return false;
    }

    FOnMontageEnded EndedDelegate;
    EndedDelegate.BindUObject(this, &UCombatComponent::OnSpecialMontageEnded);
    AnimInstance->Montage_SetEndDelegate(EndedDelegate, Montage);
    return true;
}

bool UCombatComponent::PlayForcedActionMontage(UAnimMontage *Montage, bool bInvincibleDuringAction)
{
    if (!Montage || CombatState == ECombatState::Dead)
    {
        return false;
    }

    UAnimInstance *AnimInstance = GetOwnerAnimInstance();
    if (!AnimInstance)
    {
        return false;
    }

    // 强制打断当前任何动作（包括硬直）
    InterruptCurrentAction();

    SetCombatState(ECombatState::SpecialAttacking);
    SetCurrentAttackSpec(0.0f, 0.0f, false);

    CurrentSpecialMontage = Montage;
    bSpecialActionInvincible = bInvincibleDuringAction;
    if (bInvincibleDuringAction)
    {
        bIsInvincible = true;
    }

    AnimInstance->Montage_Play(Montage, 1.0f, EMontagePlayReturnType::MontageLength, 0.0f);

    FOnMontageEnded EndedDelegate;
    EndedDelegate.BindUObject(this, &UCombatComponent::OnSpecialMontageEnded);
    AnimInstance->Montage_SetEndDelegate(EndedDelegate, Montage);
    return true;
}

void UCombatComponent::OnSpecialMontageEnded(UAnimMontage *Montage, bool bInterrupted)
{
    // 旧实例的回调（已被新的特殊动作替换）
    if (Montage != CurrentSpecialMontage)
    {
        return;
    }

    // 同一个蒙太奇被重新播放（连续使用同一招）：旧实例被打断，新实例仍在播放
    UAnimInstance *AnimInstance = GetOwnerAnimInstance();
    if (bInterrupted && AnimInstance && AnimInstance->Montage_IsPlaying(Montage))
    {
        return;
    }

    CurrentSpecialMontage = nullptr;
    AttackRotationElapsed = 0.0f;
    if (bSpecialActionInvincible)
    {
        bIsInvincible = false;
        bSpecialActionInvincible = false;
    }

    if (CombatState == ECombatState::SpecialAttacking)
    {
        ReturnToIdle();
    }
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
        || Montage == HitReactMontage_R
        || Montage == GuardBreakMontage
        || Montage == ParriedMontage
        || Montage == CurrentStaggerMontage;
}

void UCombatComponent::OnHitReactMontageEnded(UAnimMontage *Montage, bool bInterrupted)
{
    UE_LOG(LogTemp, Warning, TEXT("OnHitReactMontageEnded: bInterrupted=%s, CombatState=%s"),
           bInterrupted ? TEXT("true") : TEXT("false"),
           *UEnum::GetValueAsString(CombatState));

    // 旧实例的回调：连续受击时新的硬直蒙太奇已替换了 CurrentStaggerMontage，忽略
    if (Montage != CurrentStaggerMontage)
    {
        return;
    }

    // 同一个受击蒙太奇被重新播放（连续受击同方向）：旧实例被打断，但新实例仍在播放，忽略
    // 注意：这里不能解绑通知回调，否则会把新实例刚绑定的回调一并移除
    UAnimInstance *AnimInstance = GetOwnerAnimInstance();
    if (bInterrupted && AnimInstance && AnimInstance->Montage_IsPlaying(Montage))
    {
        return;
    }

    // 硬直真正结束（自然播完，或被外部打断且没有新的硬直）
    if (AnimInstance)
    {
        AnimInstance->OnPlayMontageNotifyBegin.RemoveDynamic(this, &UCombatComponent::OnHitReactMontageNotifyBegin);
    }
    CurrentStaggerMontage = nullptr;
    bIsParryStunned = false;

    if (CombatState == ECombatState::Staggered)
    {
        ReturnToIdle();
    }
}

void UCombatComponent::OnHitReactMontageNotifyBegin(FName NotifyName, const FBranchingPointNotifyPayload &BranchingPointPayload)
{
    // 只处理来自当前硬直蒙太奇（受击 / 破防 / 被弹反）的通知
    if (!CurrentStaggerMontage || BranchingPointPayload.SequenceAsset != CurrentStaggerMontage)
    {
        return;
    }

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
            // 精确停止当前翻滚蒙太奇（为空时 Montage_Stop 会停止所有蒙太奇，作为兜底）
            AnimInstance->Montage_Stop(0.15f, CurrentDodgeMontage);
        }
        CurrentDodgeMontage = nullptr;
        bIsInvincible = false;
        break;

    case ECombatState::Staggered:
        // 连续受击时：解绑通知回调（新的硬直会重新绑定），清除被弹反标记
        if (AnimInstance)
        {
            AnimInstance->OnPlayMontageNotifyBegin.RemoveDynamic(this, &UCombatComponent::OnHitReactMontageNotifyBegin);
        }
        CurrentStaggerMontage = nullptr;
        bIsParryStunned = false;
        break;

    case ECombatState::Blocking:
        // 中断格挡：停止举盾/格挡反馈动画（移速由 OnCombatStateTransition 恢复）
        ExitBlock(false);
        break;

    case ECombatState::SpecialAttacking:
        // 中断特殊动作：停止蒙太奇，关闭特殊动作开启的无敌
        if (AnimInstance && CurrentSpecialMontage)
        {
            AnimInstance->Montage_Stop(0.15f, CurrentSpecialMontage);
        }
        CurrentSpecialMontage = nullptr;
        if (bSpecialActionInvincible)
        {
            bIsInvincible = false;
            bSpecialActionInvincible = false;
        }
        AttackRotationElapsed = 0.0f;
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
                        AnimForLag->Montage_SetPlayRate(CurrentMontage, ActionPlayRate);
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

    // 解锁当前锁定的目标（销毁锁定UI、恢复旋转模式、清除遮挡定时器）
    // UnlockTarget 内部会判断是否真的处于锁定状态，这里无条件调用即可
    UnlockTarget();

    // 关闭无敌帧，清理格挡/弹反/特殊动作相关状态
    bIsInvincible = false;
    bSpecialActionInvincible = false;
    bIsParryStunned = false;
    bBlockInputHeld = false;
    CurrentSpecialMontage = nullptr;
    CurrentStaggerMontage = nullptr;
    StopParrySlowMo();

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