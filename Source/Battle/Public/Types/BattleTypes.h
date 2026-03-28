// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BattleTypes.generated.h"

// 移动方向枚举
UENUM(BlueprintType)
enum class EMovementDirection : uint8
{
    Forward, Backward, Left, Right
};
