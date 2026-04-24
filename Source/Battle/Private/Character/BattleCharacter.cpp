// Fill out your copyright notice in the Description page of Project Settings.

#include "Character/BattleCharacter.h"
#include "Component/CombatComponent.h"
#include "Types/BattleTypes.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"
#include "InputActionValue.h"
#include "Blueprint/UserWidget.h"
#include "Components/ProgressBar.h"
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Sound/SoundBase.h"
#include <Kismet/KismetMathLibrary.h>
#include <Kismet/GameplayStatics.h>

// Sets default values
ABattleCharacter::ABattleCharacter()
{
    // Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
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

    // 设置武器，挂载到Mesh
    SwordMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SwordMesh"));
    SwordMesh->SetupAttachment(GetMesh(), FName(TEXT("hand_r_Socket"))); // 绑定到骨骼插槽

    // 创建武器碰撞体，挂载到 SwordMesh 下（形状/大小可在蓝图中可视化调整）
    SwordCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("SwordCollision"));
    SwordCollision->SetupAttachment(SwordMesh);
    SwordCollision->SetGenerateOverlapEvents(true);

    // 初始化逻辑组件（不需要SetupAttachment，因为不是SceneComponent）
    CombatComponent = CreateDefaultSubobject<UCombatComponent>(TEXT("CombatComponent"));
}

// Called when the game starts or when spawned
void ABattleCharacter::BeginPlay()
{
    Super::BeginPlay();

	Health = MaxHealth;

    // 广播初始血量，确保 UI 无论初始化时序如何都能显示正确值
    OnHealthChanged.Broadcast(Health, MaxHealth);

    // 将武器碰撞体引用传递给战斗组件，并初始化 Overlap 回调
    if (CombatComponent && SwordCollision)
    {
        CombatComponent->SwordCollisionRef = SwordCollision;
        CombatComponent->InitSwordCollision();
    }

    if (APlayerController *PC = Cast<APlayerController>(GetController()))
    {
        // 增强输入映射
        if (IMC_Default)
        {
            if (UEnhancedInputLocalPlayerSubsystem *Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
            {
                Subsystem->AddMappingContext(IMC_Default, 0);
            }
        }
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

    // 绑定增强输入动作
    if (UEnhancedInputComponent *EnhancedInput = CastChecked<UEnhancedInputComponent>(PlayerInputComponent))
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

void ABattleCharacter::Jump()
{
    // 非 Idle 状态不允许跳跃（攻击中、翻滚中、受击硬直等）
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

float ABattleCharacter::TakeDamage(float DamageAmount, FDamageEvent const &DamageEvent, AController *EventInstigator, AActor *DamageCauser)
{
    // 已死亡则不再受伤
    if (bIsDead)
    {
        return 0.0f;
    }

    // 通过战斗组件处理受击（检查无敌帧、中断当前动作、播放受击动画等）
    // 传入 DamageCauser 用于计算受击方向，选择对应的方向性受击蒙太奇
    // 如果处于无敌帧期间，HandleTakeDamage 返回 false，不扣血
    if (CombatComponent && !CombatComponent->HandleTakeDamage(DamageCauser))
    {
        return 0.0f;
    }

    float ActualDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

    Health = FMath::Clamp(Health - ActualDamage, 0.0f, MaxHealth);
    OnHealthChanged.Broadcast(Health, MaxHealth);

    if (Health <= 0.0f)
    {
        Die();
        return ActualDamage;
    }

    return ActualDamage;
}

void ABattleCharacter::Die()
{
    if (bIsDead)
    {
        return;
    }

    bIsDead = true;

    // 通知战斗组件进入死亡状态
    if (CombatComponent)
    {
        CombatComponent->SetCombatState(ECombatState::Dead);
    }

    GetCharacterMovement()->DisableMovement();
    GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    GetMesh()->SetSimulatePhysics(true);
}
