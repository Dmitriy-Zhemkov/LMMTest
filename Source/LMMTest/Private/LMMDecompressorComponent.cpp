#include "LMMDecompressorComponent.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFilemanager.h"

// Include ONNX Runtime headers
#include <onnxruntime_cxx_api.h>

// For animation and control rig
#include "ControlRig.h"
#include "ControlRigComponent.h"
#include "RigVMHost.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"

ULMMDecompressorComponent::ULMMDecompressorComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    Session = nullptr;
}

bool ULMMDecompressorComponent::LoadTxtToArray(const FString& FilePath, TArray<float>& OutArray)
{
    FString Content;
    if (!FFileHelper::LoadFileToString(Content, *FilePath)) return false;

    TArray<FString> Values;
    Content.ParseIntoArray(Values, TEXT(","), true);
    OutArray.Empty();
    for (FString& Val : Values)
    {
        OutArray.Add(FCString::Atof(*Val));
    }
    return true;
}

bool ULMMDecompressorComponent::LoadModel(const FString& OnnxModelPath, const FString& MeanPath, const FString& StdPath)
{
    if (!LoadTxtToArray(MeanPath, PoseMean)) return false;
    if (!LoadTxtToArray(StdPath, PoseStd)) return false;

    static Ort::Env Env(ORT_LOGGING_LEVEL_WARNING, "LMM");
    Ort::SessionOptions SessionOptions;
    SessionOptions.SetIntraOpNumThreads(1);
    Session = new Ort::Session(Env, *OnnxModelPath, SessionOptions);
    return Session != nullptr;
}

bool ULMMDecompressorComponent::PredictPose(const TArray<float>& InputFeatures, TArray<float>& OutPose)
{
    if (!Session || InputFeatures.Num() != 16) return false;

    Ort::AllocatorWithDefaultOptions Allocator;
    const int64_t inputShape[] = { 1, 16 };
    Ort::MemoryInfo memInfo = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);

    auto inputTensor = Ort::Value::CreateTensor<float>(memInfo, const_cast<float*>(InputFeatures.GetData()), 16, inputShape, 2);

    const char* inputNames[] = { "features" };
    const char* outputNames[] = { "pose_normalized" };

    auto outputTensors = ((Ort::Session*)Session)->Run(Ort::RunOptions{ nullptr }, inputNames, &inputTensor, 1, outputNames, 1);

    float* rawOutput = outputTensors[0].GetTensorMutableData<float>();
    size_t count = outputTensors[0].GetTensorTypeAndShapeInfo().GetElementCount();

    if (PoseMean.Num() < (int32)count || PoseStd.Num() < (int32)count)
    {
        UE_LOG(LogTemp, Error, TEXT("PoseMean/PoseStd too small: mean=%d std=%d need=%d"),
            PoseMean.Num(), PoseStd.Num(), (int32)count);
        return false;
    }

    OutPose.SetNum(count);
    for (size_t i = 0; i < count; ++i)
    {
        OutPose[i] = rawOutput[i] * PoseStd[i] + PoseMean[i];  // денормализация
    }

    return true;
}

void ULMMDecompressorComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    USkeletalMeshComponent* SkelMesh = GetOwner()->FindComponentByClass<USkeletalMeshComponent>();
    UControlRigComponent* RigComp = GetOwner()->FindComponentByClass<UControlRigComponent>();
    if (!SkelMesh || !RigComp)
    {
        return;
    }

    UControlRig* Rig = RigComp->GetControlRig();
    if (!Rig)
    {
        return;
    }

    // 1) ВХОД в модель (пока заглушка: 16 фичей = 0)
    TArray<float> InputFeatures;
    InputFeatures.Init(0.0f, 16);

    // 2) ВЫХОД модели = поза (то, что ты хочешь передать в Control Rig)
    TArray<float> PoseData;
    if (!PredictPose(InputFeatures, PoseData))
    {
        return; // нет модели/не загружены mean/std/не те размеры
    }

    // 3) В твоих логах RigVM ругался на TArray<double> (Real), поэтому шлём double
    TArray<double> PoseReal;
    PoseReal.Reserve(PoseData.Num());
    for (float v : PoseData)
    {
        PoseReal.Add((double)v);
    }

    // 4) Передаём в переменную Control Rig (Expose to Rig Instance)
    Rig->SetPublicVariableValue(FName(TEXT("PoseData")), PoseReal);
}

