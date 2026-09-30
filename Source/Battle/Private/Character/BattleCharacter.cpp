// Fill out your copyright notice in the Description page of Project Settings.

#include "Character/BattleCharacter.h"
#include "Component/CombatComponent.h"
#include "Game/BattleRespawnSubsystem.h"
#include "Types/BattleTypes.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"
#include "InputActionValue.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Sound/SoundBase.h"
#include <Kismet/KismetMathLibrary.h>
#include <Kismet/GameplayStatics.h>

// Sets default values
ABattleCharacter::ABattleCharacter()
{
    // 保留 Tick：BP_BattleCharacter 中使用了 Event Tick
    PrimaryActorTick.bCanEverTick = true;

    // 设置CameraBoom，挂载到根组件
    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    CameraBoom->SetupAttachment(RootComponent);
    CameraBoom->TargetArmLength = 400.0f;
    CameraBoom->bUsePawnControlRotation = true; // 让弹簧臂跟随角色旋转

    // 设置FollowCamera，挂载到CameraBoom上
    FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
    FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
    FollowCamera->bUsePawnControlRotation = false; // 不让相机跟随角色旋转

    CombatComponent->Team = ECombatTeam::Player;
}

// Called when the game starts or when spawned
void ABattleCharacter::BeginPlay()
{
    Super::BeginPlay();

    AddInputMappingContexts();

    // 注册到重生系统（监听死亡 → 显示死亡界面 → 在检查点重生）
    if (UBattleRespawnSubsystem* RespawnSubsystem = GetWorld()->GetSubsystem<UBattleRespawnSubsystem>())
    {
        RespawnSubsystem->RegisterPlayer(this);
    }
}

void ABattleCharacter::NotifyControllerChanged()
{
    Super::NotifyControllerChanged();
    AddInputMappingContexts();
}

void ABattleCharacter::UnPossessed()
{
    // 必须在 Super 之前处理：Super::UnPossessed 会把 Controller 置空
    if (APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
        {
            if (RuntimeMappingContext)
            {
                Subsystem->RemoveMappingContext(RuntimeMappingContext);
            }
        }
    }

    Super::UnPossessed();
}

void ABattleCharacter::AddInputMappingContexts()
{
    APlayerController* PC = Cast<APlayerController>(GetController());
    if (!PC)
    {
        return;
    }

    UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer());
    if (!Subsystem)
    {
        return;
    }

    // 重复添加同一个 Context 只会更新优先级，不会产生重复映射
    if (IMC_Default)
    {
        Subsystem->AddMappingContext(IMC_Default, 0);
    }

    if (RuntimeMappingContext)
    {
        Subsystem->AddMappingContext(RuntimeMappingContext, 1);
    }
}

// Called every frame
void ABattleCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}

// Called to bind functionality to input
void ABattleCharacter::SetupPlayerInputComponent(UInputComponent *PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    // 绑定增强输入动作（Cast 失败时安全跳过，不再 CastChecked 后又判空）
    if (UEnhancedInputComponent *EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent))
    {
        // 视角输入
        if (IA_Look)
        {
            EnhancedInput->BindAction(IA_Look, ETriggerEvent::Triggered, this, &ABattleCharacter::Look);
        }

        // 移动输入
        if (IA_Move)
        {
            EnhancedInput->BindAction(IA_Move, ETriggerEvent::Triggered, this, &ABattleCharacter::Move);
            // 松开移动输入时重置方向，避免锁定状态下用"上一次的方向"翻滚
            EnhancedInput->BindAction(IA_Move, ETriggerEvent::Completed, this, &ABattleCharacter::StopMove);
        }

        // 跳跃输入
        if (IA_Jump)
        {
            // Started事件绑定Jump（开始跳跃）
            EnhancedInput->BindAction(IA_Jump, ETriggerEvent::Started, this, &ABattleCharacter::Jump);
            // Completed事件绑定StopJumping（停止跳跃）
            EnhancedInput->BindAction(IA_Jump, ETriggerEvent::Completed, this, &ABattleCharacter::StopJumping);
        }

        // 锁定敌人输入
        if (IA_Lock)
        {
            // Started事件绑定LockTarget（锁定目标）
            EnhancedInput->BindAction(IA_Lock, ETriggerEvent::Started, this, &ABattleCharacter::LockTarget);
        }

        // 攻击输入
        if (IA_Attack)
        {
            // Started事件绑定Attack（按下瞬间触发一次，避免按住时每帧重复调用）
            EnhancedInput->BindAction(IA_Attack, ETriggerEvent::Started, this, &ABattleCharacter::Attack);
        }

        // 翻滚输入
        if (IA_Dodge)
        {
            EnhancedInput->BindAction(IA_Dodge, ETriggerEvent::Started, this, &ABattleCharacter::Dodge);
        }

        // 格挡输入：按下开始、松开（或被取消）结束
        if (IA_Block)
        {
            EnhancedInput->BindAction(IA_Block, ETriggerEvent::Started, this, &ABattleCharacter::StartBlock);
            EnhancedInput->BindAction(IA_Block, ETriggerEvent::Completed, this, &ABattleCharacter::StopBlock);
            EnhancedInput->BindAction(IA_Block, ETriggerEvent::Canceled, this, &ABattleCharacter::StopBlock);
        }
    }
}

void ABattleCharacter::Look(const FInputActionValue &Value)
{
    // 获取视角输入的 X/Y 值
    FVector2D LookAxisVector = Value.Get<FVector2D>();

    // 检查战斗组件的目标锁定角色是否有效
    if (CombatComponent && IsValid(CombatComponent->TargetLockActor))
    {
        // 锁定目标时不允许手动转镜头，但将水平输入用于目标切换
        CombatComponent->HandleLockLookInput(LookAxisVector.X);
        return;
    }

    // 未锁定目标时，左右/上下旋转视角
    AddControllerYawInput(LookAxisVector.X);   // Left/Right
    AddControllerPitchInput(LookAxisVector.Y); // Up/Down
}

void ABattleCharacter::Move(const FInputActionValue &Value)
{
    // 计算移动方向并同步到战斗组件
    EMovementDirection Direction = GetMovementDirection(Value);
    if (CombatComponent)
    {
        CombatComponent->SetMovementDirection(Direction);
    }

    // 获取控制器旋转
    const FRotator ControlRotation = GetControlRotation();

    // 左右移动
    const FVector RightDirection = UKismetMathLibrary::GetRightVector(FRotator(0.0f, ControlRotation.Yaw, 0.0f));
    AddMovementInput(RightDirection, Value.Get<FVector2D>().X);

    // 前后移动
    const FVector ForwardDirection = UKismetMathLibrary::GetForwardVector(FRotator(0.0f, ControlRotation.Yaw, 0.0f));
    AddMovementInput(ForwardDirection, Value.Get<FVector2D>().Y);
}

void ABattleCharacter::StopMove(const FInputActionValue &Value)
{
    if (CombatComponent)
    {
        CombatComponent->ClearMovementInput();
    }
}

void ABattleCharacter::Jump()
{
    // 非 Idle 状态不允许跳跃（攻击中、翻滚中、受击硬直、格挡中等）
    if (CombatComponent && !CombatComponent->CanPerformAction())
    {
        return;
    }

    Super::Jump();
}

void ABattleCharacter::StopJumping()
{
    Super::StopJumping();
}

void ABattleCharacter::OnJumped_Implementation()
{
    Super::OnJumped_Implementation();

    if (JumpSound)
    {
        UGameplayStatics::PlaySoundAtLocation(this, JumpSound, GetActorLocation());
    }
}

void ABattleCharacter::LockTarget()
{
    if (CombatComponent)
    {
        CombatComponent->LockTarget();
    }
}

void ABattleCharacter::Attack()
{
    if (!CombatComponent)
    {
        return;
    }

    // 检测Shift是否按下，按下则执行重攻击
    APlayerController* PC = Cast<APlayerController>(GetController());
    if (PC && PC->IsInputKeyDown(EKeys::LeftShift))
    {
        HeavyAttack();
        return;
    }

    CombatComponent->Attack();
}

void ABattleCharacter::HeavyAttack()
{
    if (CombatComponent)
    {
        CombatComponent->HeavyAttack();
    }
}

void ABattleCharacter::Dodge()
{
    if (CombatComponent)
    {
        CombatComponent->Dodge();
    }
}

void ABattleCharacter::StartBlock()
{
    if (CombatComponent)
    {
        CombatComponent->StartBlock();
    }
}

void ABattleCharacter::StopBlock()
{
    if (CombatComponent)
    {
        CombatComponent->StopBlock();
    }
}

EMovementDirection ABattleCharacter::GetMovementDirection(const FInputActionValue &Value)
{
    FVector2D MoveVector = Value.Get<FVector2D>();

    // 根据输入向量判断主要移动方向
    if (FMath::Abs(MoveVector.Y) >= FMath::Abs(MoveVector.X))
    {
        // 前后为主
        return MoveVector.Y >= 0.0f ? EMovementDirection::Forward : EMovementDirection::Backward;
    }
    else
    {
        // 左右为主
        return MoveVector.X >= 0.0f ? EMovementDirection::Right : EMovementDirection::Left;
    }
}
