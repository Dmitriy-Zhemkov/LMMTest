// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "LMMDecompressorComponent.generated.h"


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class LMMTEST_API ULMMDecompressorComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    ULMMDecompressorComponent();

    // Загружается один раз
    UFUNCTION(BlueprintCallable)
    bool LoadModel(const FString& OnnxModelPath, const FString& MeanPath, const FString& StdPath);

    // Вызывается каждый кадр
    UFUNCTION(BlueprintCallable)
    bool PredictPose(const TArray<float>& InputFeatures, TArray<float>& OutPose);

private:
    TArray<float> PoseMean;
    TArray<float> PoseStd;
    void* Session;  // ONNX Runtime session pointer

    bool LoadTxtToArray(const FString& FilePath, TArray<float>& OutArray);

    virtual void TickComponent(
        float DeltaTime,
        ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction
    ) override;
};