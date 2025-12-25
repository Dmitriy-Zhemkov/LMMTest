// LMMAnimInstance.cpp

#include "LMMAnimInstance.h"
#include "LMMDecompressorComponent.h"
#include "GameFramework/Actor.h"

void ULMMAnimInstance::NativeInitializeAnimation()
{
    Super::NativeInitializeAnimation();

    AActor* Owner = GetOwningActor();
    if (Owner)
    {
        LMMComponent = Owner->FindComponentByClass<ULMMDecompressorComponent>();
        if (LMMComponent)
        {
            UE_LOG(LogTemp, Warning, TEXT("[LMM AnimInstance] Found LMM Component!"));
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("[LMM AnimInstance] LMM Component NOT found!"));
        }
    }
}

void ULMMAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
    Super::NativeUpdateAnimation(DeltaSeconds);

    if (!LMMComponent)
    {
        bHasValidPose = false;
        return;
    }

    TArray<float> Pose = LMMComponent->GetCurrentPose();
    if (Pose.Num() < 66)
    {
        bHasValidPose = false;
        return;
    }

    bHasValidPose = true;

    // Маппинг LAFAN1 → Rotators
    // Pose содержит углы в градусах (Euler XYZ)
    // FRotator = (Pitch=Y, Yaw=Z, Roll=X)

    HipsRotation = PoseToRotator(Pose, 0);
    LeftUpLegRotation = PoseToRotator(Pose, 3);
    LeftLegRotation = PoseToRotator(Pose, 6);
    LeftFootRotation = PoseToRotator(Pose, 9);
    LeftToeRotation = PoseToRotator(Pose, 12);
    RightUpLegRotation = PoseToRotator(Pose, 15);
    RightLegRotation = PoseToRotator(Pose, 18);
    RightFootRotation = PoseToRotator(Pose, 21);
    RightToeRotation = PoseToRotator(Pose, 24);
    SpineRotation = PoseToRotator(Pose, 27);
    Spine1Rotation = PoseToRotator(Pose, 30);
    Spine2Rotation = PoseToRotator(Pose, 33);
    NeckRotation = PoseToRotator(Pose, 36);
    HeadRotation = PoseToRotator(Pose, 39);
    LeftShoulderRotation = PoseToRotator(Pose, 42);
    LeftArmRotation = PoseToRotator(Pose, 45);
    LeftForeArmRotation = PoseToRotator(Pose, 48);
    LeftHandRotation = PoseToRotator(Pose, 51);
    RightShoulderRotation = PoseToRotator(Pose, 54);
    RightArmRotation = PoseToRotator(Pose, 57);
    RightForeArmRotation = PoseToRotator(Pose, 60);
    RightHandRotation = PoseToRotator(Pose, 63);
}

FRotator ULMMAnimInstance::PoseToRotator(const TArray<float>& Pose, int32 StartIndex)
{
    // LAFAN1 данные: углы в градусах
    // Пробуем прямой маппинг: X->Roll, Y->Pitch, Z->Yaw
    float X = Pose[StartIndex];
    float Y = Pose[StartIndex + 1];
    float Z = Pose[StartIndex + 2];

    // FRotator(Pitch, Yaw, Roll)
    return FRotator(X, Y, Z);
}