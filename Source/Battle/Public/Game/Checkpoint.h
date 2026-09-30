// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Checkpoint.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UPointLightComponent;
class UArrowComponent;

/**
 * 检查点（篝火）：玩家走进范围即激活
 *  - 成为玩家的重生点（死亡后在箭头位置重生）
 *  - 激活时恢复玩家全部属性
 *  - 激活后灯光变亮，可在蓝图中通过 OnCheckpointActivated 追加特效/音效
 */
UCLASS()
class BATTLE_API ACheckpoint : public AActor
{
	GENERATED_BODY()

public:
	ACheckpoint();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Checkpoint")
	TObjectPtr<USphereComponent> Trigger;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Checkpoint")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Checkpoint")
	TObjectPtr<UPointLightComponent> Light;

	// 重生位置与朝向
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Checkpoint")
	TObjectPtr<UArrowComponent> RespawnPoint;

	// 激活时是否恢复玩家属性
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Checkpoint")
	bool bRestoreAttributesOnActivate = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Checkpoint")
	float InactiveLightIntensity = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Checkpoint")
	float ActiveLightIntensity = 5000.0f;

	UFUNCTION(BlueprintPure, Category = "Checkpoint")
	FTransform GetRespawnTransform() const;

	UFUNCTION(BlueprintPure, Category = "Checkpoint")
	bool IsActivated() const { return bActivated; }

	// 由重生系统调用：切换激活/熄灭表现
	void SetActivated(bool bNewActivated);

	UFUNCTION(BlueprintImplementableEvent, Category = "Checkpoint")
	void OnCheckpointActivated();

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	                           int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	bool bActivated = false;
};
