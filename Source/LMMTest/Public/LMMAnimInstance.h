// LMMAnimInstance.h

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "LMMAnimInstance.generated.h"

class ULMMDecompressorComponent;

UCLASS()
class LMMTEST_API ULMMAnimInstance : public UAnimInstance
{
    GENERATED_BODY()

public:
    virtual void NativeInitializeAnimation() override;
    virtual void NativeUpdateAnimation(float DeltaSeconds) override;

    // ”глы дл€ каждой кости (в градусах)
    UPROPERTY(BlueprintReadOnly, Category = "LMM")
    FRotator HipsRotation;

    UPROPERTY(BlueprintReadOnly, Category = "LMM")
    FRotator LeftUpLegRotation;

    UPROPERTY(BlueprintReadOnly, Category = "LMM")
    FRotator LeftLegRotation;

    UPROPERTY(BlueprintReadOnly, Category = "LMM")
    FRotator LeftFootRotation;

    UPROPERTY(BlueprintReadOnly, Category = "LMM")
    FRotator LeftToeRotation;

    UPROPERTY(BlueprintReadOnly, Category = "LMM")
    FRotator RightUpLegRotation;

    UPROPERTY(BlueprintReadOnly, Category = "LMM")
    FRotator RightLegRotation;

    UPROPERTY(BlueprintReadOnly, Category = "LMM")
    FRotator RightFootRotation;

    UPROPERTY(BlueprintReadOnly, Category = "LMM")
    FRotator RightToeRotation;

    UPROPERTY(BlueprintReadOnly, Category = "LMM")
    FRotator SpineRotation;

    UPROPERTY(BlueprintReadOnly, Category = "LMM")
    FRotator Spine1Rotation;

    UPROPERTY(BlueprintReadOnly, Category = "LMM")
    FRotator Spine2Rotation;

    UPROPERTY(BlueprintReadOnly, Category = "LMM")
    FRotator NeckRotation;

    UPROPERTY(BlueprintReadOnly, Category = "LMM")
    FRotator HeadRotation;

    UPROPERTY(BlueprintReadOnly, Category = "LMM")
    FRotator LeftShoulderRotation;

    UPROPERTY(BlueprintReadOnly, Category = "LMM")
    FRotator LeftArmRotation;

    UPROPERTY(BlueprintReadOnly, Category = "LMM")
    FRotator LeftForeArmRotation;

    UPROPERTY(BlueprintReadOnly, Category = "LMM")
    FRotator LeftHandRotation;

    UPROPERTY(BlueprintReadOnly, Category = "LMM")
    FRotator RightShoulderRotation;

    UPROPERTY(BlueprintReadOnly, Category = "LMM")
    FRotator RightArmRotation;

    UPROPERTY(BlueprintReadOnly, Category = "LMM")
    FRotator RightForeArmRotation;

    UPROPERTY(BlueprintReadOnly, Category = "LMM")
    FRotator RightHandRotation;

    UPROPERTY(BlueprintReadOnly, Category = "LMM")
    bool bHasValidPose = false;

private:
    UPROPERTY()
    ULMMDecompressorComponent* LMMComponent = nullptr;

    FRotator PoseToRotator(const TArray<float>& Pose, int32 StartIndex);
};