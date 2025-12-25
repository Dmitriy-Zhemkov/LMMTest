// LMMDecompressorComponent.h
// œÓÒÚ‡ˇ ‚ÂÒËˇ - ÒÌ‡˜‡Î‡ ÔÓ‚ÂˇÂÏ ˜ÚÓ ‡·ÓÚ‡ÂÚ ·ÂÁ ONNX

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "LMMDecompressorComponent.generated.h"

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class LMMTEST_API ULMMDecompressorComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    ULMMDecompressorComponent();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction) override;

    // ==================== Õ¿—“–Œ… » ====================

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LMM|Paths")
    FString DataFolder = TEXT("Data/LMM");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LMM|Settings")
    int32 ProjectionInterval = 10;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LMM|Settings")
    FName ControlRigVariableName = TEXT("PoseData");

    // ==================== —Œ—“ŒﬂÕ»≈ ====================

    UPROPERTY(BlueprintReadOnly, Category = "LMM|State")
    bool bIsInitialized = false;

    UPROPERTY(BlueprintReadOnly, Category = "LMM|State")
    bool bOnnxLoaded = false;

    UPROPERTY(BlueprintReadOnly, Category = "LMM|State")
    int32 FrameCounter = 0;

    // ==================== ”œ–¿¬À≈Õ»≈ ====================

    UFUNCTION(BlueprintCallable, Category = "LMM")
    void SetDesiredVelocity(FVector Velocity);

    UFUNCTION(BlueprintCallable, Category = "LMM")
    void SetDesiredFacing(FVector Direction);

    UFUNCTION(BlueprintCallable, Category = "LMM")
    void ForceProjection();

    UFUNCTION(BlueprintCallable, Category = "LMM")
    TArray<float> GetCurrentPose() const;

private:
    // ==================== –¿«Ã≈–ÕŒ—“» ====================

    static constexpr int32 FEATURE_DIM = 22;
    static constexpr int32 LATENT_DIM = 32;
    static constexpr int32 POSE_DIM = 66;

    // ==================== ÕŒ–Ã¿À»«¿÷»ﬂ ====================

    TArray<float> FeatureMean;
    TArray<float> FeatureStd;
    TArray<float> PoseMean;
    TArray<float> PoseStd;

    // ==================== —Œ—“ŒﬂÕ»≈ ====================

    TArray<float> CurrentFeatures;
    TArray<float> CurrentLatentZ;
    TArray<float> CurrentPose;

    FVector DesiredVelocity = FVector::ZeroVector;
    FVector DesiredFacing = FVector::ForwardVector;

    // ==================== ONNX (ÛÍ‡Á‡ÚÂÎË - forward declared) ====================

    void* ProjectorSession = nullptr;
    void* StepperSession = nullptr;
    void* DecompressorSession = nullptr;
    void* OrtEnv = nullptr;
    void* OrtSessionOptions = nullptr;

    // ==================== Ã≈“Œƒ€ ====================

    bool Initialize();
    bool InitOnnxRuntime();
    bool LoadModel(const FString& Path, void*& OutSession);
    void CleanupOnnx();

    bool LoadTxtToArray(const FString& FilePath, TArray<float>& OutArray);

    TArray<float> BuildQueryFeatures();
    TArray<float> NormalizeFeatures(const TArray<float>& Raw);
    TArray<float> DenormalizePose(const TArray<float>& Normalized);

    bool RunProjector(const TArray<float>& Query, TArray<float>& OutFeatures, TArray<float>& OutZ);
    bool RunStepper(const TArray<float>& Features, const TArray<float>& Z,
        TArray<float>& OutDeltaF, TArray<float>& OutDeltaZ);
    bool RunDecompressor(const TArray<float>& Features, const TArray<float>& Z,
        TArray<float>& OutPose);

    void ApplyPoseToControlRig();
};