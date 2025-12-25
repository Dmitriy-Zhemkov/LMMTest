// LMMDecompressorComponent.cpp

#include "LMMDecompressorComponent.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformProcess.h"

// Control Rig
#include "ControlRig.h"
#include "ControlRigComponent.h"
#include "Components/SkeletalMeshComponent.h"

// ONNX Runtime - условная компиляция
#if WITH_ONNX
#include <onnxruntime_cxx_api.h>
#endif

// ==================== КОНСТРУКТОР ====================

ULMMDecompressorComponent::ULMMDecompressorComponent()
{
    PrimaryComponentTick.bCanEverTick = true;

    CurrentFeatures.SetNum(FEATURE_DIM);
    CurrentLatentZ.SetNum(LATENT_DIM);
    CurrentPose.SetNum(POSE_DIM);

    for (int32 i = 0; i < FEATURE_DIM; i++) CurrentFeatures[i] = 0.0f;
    for (int32 i = 0; i < LATENT_DIM; i++) CurrentLatentZ[i] = 0.0f;
    for (int32 i = 0; i < POSE_DIM; i++) CurrentPose[i] = 0.0f;
}

// ==================== LIFECYCLE ====================

void ULMMDecompressorComponent::BeginPlay()
{
    Super::BeginPlay();

    UE_LOG(LogTemp, Warning, TEXT("[LMM] ========================================"));
    UE_LOG(LogTemp, Warning, TEXT("[LMM] BeginPlay - LMM Component Starting"));
    UE_LOG(LogTemp, Warning, TEXT("[LMM] ========================================"));

#if WITH_ONNX
    UE_LOG(LogTemp, Warning, TEXT("[LMM] ONNX Runtime: ENABLED"));
#else
    UE_LOG(LogTemp, Warning, TEXT("[LMM] ONNX Runtime: DISABLED (no ThirdParty/onnxruntime)"));
#endif

    if (Initialize())
    {
        bIsInitialized = true;
        UE_LOG(LogTemp, Warning, TEXT("[LMM] ✅ Initialization SUCCESS!"));

        // Выводим имена костей для отладки
        AActor* Owner = GetOwner();
        if (Owner)
        {
            USkeletalMeshComponent* SkelMesh = Owner->FindComponentByClass<USkeletalMeshComponent>();
            if (SkelMesh && SkelMesh->GetSkeletalMeshAsset())
            {
                const FReferenceSkeleton& RefSkel = SkelMesh->GetSkeletalMeshAsset()->GetRefSkeleton();
                int32 NumBones = RefSkel.GetNum();
                UE_LOG(LogTemp, Warning, TEXT("[LMM] Skeleton has %d bones:"), NumBones);
                for (int32 i = 0; i < NumBones; i++)
                {
                    UE_LOG(LogTemp, Warning, TEXT("[LMM]   Bone %d: %s"), i, *RefSkel.GetBoneName(i).ToString());
                }
            }
        }

        ForceProjection();
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("[LMM] ❌ Initialization FAILED!"));
    }
}

void ULMMDecompressorComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    CleanupOnnx();
    Super::EndPlay(EndPlayReason);
}

// ==================== INITIALIZE ====================

bool ULMMDecompressorComponent::Initialize()
{
    FString ProjectDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
    FString DataDir = FPaths::Combine(ProjectDir, DataFolder);

    UE_LOG(LogTemp, Warning, TEXT("[LMM] Project: %s"), *ProjectDir);
    UE_LOG(LogTemp, Warning, TEXT("[LMM] Data: %s"), *DataDir);

    // Проверяем папку
    if (!FPaths::DirectoryExists(DataDir))
    {
        UE_LOG(LogTemp, Error, TEXT("[LMM] Data folder NOT FOUND!"));
        return false;
    }

    // Загружаем txt файлы
    bool bTxtOk = true;
    bTxtOk &= LoadTxtToArray(FPaths::Combine(DataDir, TEXT("feature_mean.txt")), FeatureMean);
    bTxtOk &= LoadTxtToArray(FPaths::Combine(DataDir, TEXT("feature_std.txt")), FeatureStd);
    bTxtOk &= LoadTxtToArray(FPaths::Combine(DataDir, TEXT("pose_mean.txt")), PoseMean);
    bTxtOk &= LoadTxtToArray(FPaths::Combine(DataDir, TEXT("pose_std.txt")), PoseStd);

    if (!bTxtOk)
    {
        UE_LOG(LogTemp, Error, TEXT("[LMM] Failed to load normalization files!"));
        return false;
    }

    UE_LOG(LogTemp, Warning, TEXT("[LMM] ✅ Normalization: feat=%d/%d, pose=%d/%d"),
        FeatureMean.Num(), FeatureStd.Num(), PoseMean.Num(), PoseStd.Num());

    // Логируем первые значения mean/std для проверки
    if (FeatureMean.Num() >= 3 && FeatureStd.Num() >= 3)
    {
        UE_LOG(LogTemp, Warning, TEXT("[LMM] FeatureMean[0-2]: %.4f, %.4f, %.4f"),
            FeatureMean[0], FeatureMean[1], FeatureMean[2]);
        UE_LOG(LogTemp, Warning, TEXT("[LMM] FeatureStd[0-2]: %.4f, %.4f, %.4f"),
            FeatureStd[0], FeatureStd[1], FeatureStd[2]);
    }
    if (PoseMean.Num() >= 3 && PoseStd.Num() >= 3)
    {
        UE_LOG(LogTemp, Warning, TEXT("[LMM] PoseMean[0-2]: %.4f, %.4f, %.4f"),
            PoseMean[0], PoseMean[1], PoseMean[2]);
        UE_LOG(LogTemp, Warning, TEXT("[LMM] PoseStd[0-2]: %.4f, %.4f, %.4f"),
            PoseStd[0], PoseStd[1], PoseStd[2]);
    }

    // Инициализируем ONNX
#if WITH_ONNX
    if (!InitOnnxRuntime())
    {
        UE_LOG(LogTemp, Warning, TEXT("[LMM] ONNX init failed - running without inference"));
        bOnnxLoaded = false;
    }
    else
    {
        // Загружаем модели
        FString ProjectorPath = FPaths::Combine(DataDir, TEXT("projector.onnx"));
        FString StepperPath = FPaths::Combine(DataDir, TEXT("stepper.onnx"));
        FString DecompressorPath = FPaths::Combine(DataDir, TEXT("decompressor.onnx"));

        bool bModelsOk = true;
        bModelsOk &= LoadModel(ProjectorPath, ProjectorSession);
        bModelsOk &= LoadModel(StepperPath, StepperSession);
        bModelsOk &= LoadModel(DecompressorPath, DecompressorSession);

        if (bModelsOk)
        {
            bOnnxLoaded = true;
            UE_LOG(LogTemp, Warning, TEXT("[LMM] ✅ All ONNX models loaded!"));
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("[LMM] Some models failed to load"));
            bOnnxLoaded = false;
        }
    }
#else
    UE_LOG(LogTemp, Warning, TEXT("[LMM] Running without ONNX (test mode)"));
    bOnnxLoaded = false;
#endif

    return true; // Возвращаем true даже без ONNX - компонент работает
}

bool ULMMDecompressorComponent::InitOnnxRuntime()
{
#if WITH_ONNX
    UE_LOG(LogTemp, Warning, TEXT("[LMM] Initializing ONNX Runtime..."));

    // ШАГ 1: Вручную загружаем DLL
    FString ProjectDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
    FString DllPath = FPaths::Combine(ProjectDir, TEXT("Source/ThirdParty/onnxruntime/lib/onnxruntime.dll"));

    // Также пробуем shared providers
    FString SharedDllPath = FPaths::Combine(ProjectDir, TEXT("Source/ThirdParty/onnxruntime/lib/onnxruntime_providers_shared.dll"));

    UE_LOG(LogTemp, Warning, TEXT("[LMM] Loading DLL: %s"), *DllPath);

    if (FPaths::FileExists(SharedDllPath))
    {
        void* SharedHandle = FPlatformProcess::GetDllHandle(*SharedDllPath);
        if (SharedHandle)
        {
            UE_LOG(LogTemp, Warning, TEXT("[LMM] ✅ Loaded providers_shared.dll"));
        }
    }

    if (!FPaths::FileExists(DllPath))
    {
        UE_LOG(LogTemp, Error, TEXT("[LMM] DLL not found: %s"), *DllPath);
        return false;
    }

    void* DllHandle = FPlatformProcess::GetDllHandle(*DllPath);
    if (!DllHandle)
    {
        UE_LOG(LogTemp, Error, TEXT("[LMM] Failed to load DLL!"));
        return false;
    }

    UE_LOG(LogTemp, Warning, TEXT("[LMM] ✅ DLL loaded successfully!"));

    // ШАГ 2: Теперь создаём ONNX объекты
    try
    {
        UE_LOG(LogTemp, Warning, TEXT("[LMM] Creating Ort::Env..."));

        // Создаём Environment
        Ort::Env* Env = new Ort::Env(ORT_LOGGING_LEVEL_WARNING, "LMM");
        OrtEnv = Env;

        // Создаём SessionOptions
        Ort::SessionOptions* Options = new Ort::SessionOptions();
        Options->SetIntraOpNumThreads(1);
        Options->SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_BASIC);
        OrtSessionOptions = Options;

        UE_LOG(LogTemp, Warning, TEXT("[LMM] ✅ ONNX Runtime initialized!"));
        return true;
    }
    catch (const Ort::Exception& e)
    {
        UE_LOG(LogTemp, Error, TEXT("[LMM] ONNX Exception: %s"), ANSI_TO_TCHAR(e.what()));
        return false;
    }
    catch (const std::exception& e)
    {
        UE_LOG(LogTemp, Error, TEXT("[LMM] Std Exception: %s"), ANSI_TO_TCHAR(e.what()));
        return false;
    }
    catch (...)
    {
        UE_LOG(LogTemp, Error, TEXT("[LMM] Unknown exception!"));
        return false;
    }
#else
    return false;
#endif
}

bool ULMMDecompressorComponent::LoadModel(const FString& Path, void*& OutSession)
{
#if WITH_ONNX
    if (!FPaths::FileExists(Path))
    {
        UE_LOG(LogTemp, Error, TEXT("[LMM] Model not found: %s"), *Path);
        return false;
    }

    if (!OrtEnv || !OrtSessionOptions)
    {
        UE_LOG(LogTemp, Error, TEXT("[LMM] ONNX not initialized!"));
        return false;
    }

    FString AbsPath = FPaths::ConvertRelativePathToFull(Path);
    UE_LOG(LogTemp, Warning, TEXT("[LMM] Loading: %s"), *FPaths::GetCleanFilename(Path));

    try
    {
        Ort::Env* Env = static_cast<Ort::Env*>(OrtEnv);
        Ort::SessionOptions* Options = static_cast<Ort::SessionOptions*>(OrtSessionOptions);

        std::wstring WPath(*AbsPath);
        Ort::Session* Session = new Ort::Session(*Env, WPath.c_str(), *Options);
        OutSession = Session;

        UE_LOG(LogTemp, Warning, TEXT("[LMM] ✅ Loaded: %s"), *FPaths::GetCleanFilename(Path));
        return true;
    }
    catch (const Ort::Exception& e)
    {
        UE_LOG(LogTemp, Error, TEXT("[LMM] ONNX: %s"), ANSI_TO_TCHAR(e.what()));
        return false;
    }
    catch (...)
    {
        UE_LOG(LogTemp, Error, TEXT("[LMM] Unknown error loading model"));
        return false;
    }
#else
    return false;
#endif
}

void ULMMDecompressorComponent::CleanupOnnx()
{
#if WITH_ONNX
    if (ProjectorSession)
    {
        delete static_cast<Ort::Session*>(ProjectorSession);
        ProjectorSession = nullptr;
    }
    if (StepperSession)
    {
        delete static_cast<Ort::Session*>(StepperSession);
        StepperSession = nullptr;
    }
    if (DecompressorSession)
    {
        delete static_cast<Ort::Session*>(DecompressorSession);
        DecompressorSession = nullptr;
    }
    if (OrtSessionOptions)
    {
        delete static_cast<Ort::SessionOptions*>(OrtSessionOptions);
        OrtSessionOptions = nullptr;
    }
    if (OrtEnv)
    {
        delete static_cast<Ort::Env*>(OrtEnv);
        OrtEnv = nullptr;
    }
#endif
}

bool ULMMDecompressorComponent::LoadTxtToArray(const FString& FilePath, TArray<float>& OutArray)
{
    if (!FPaths::FileExists(FilePath))
    {
        UE_LOG(LogTemp, Error, TEXT("[LMM] File not found: %s"), *FilePath);
        return false;
    }

    FString Content;
    if (!FFileHelper::LoadFileToString(Content, *FilePath))
    {
        return false;
    }

    TArray<FString> Lines;
    Content.ParseIntoArrayLines(Lines);

    OutArray.Empty();
    for (const FString& Line : Lines)
    {
        FString Trimmed = Line.TrimStartAndEnd();
        if (!Trimmed.IsEmpty())
        {
            OutArray.Add(FCString::Atof(*Trimmed));
        }
    }

    return OutArray.Num() > 0;
}

// ==================== TICK ====================

void ULMMDecompressorComponent::TickComponent(float DeltaTime, ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!bIsInitialized) return;

    FrameCounter++;

    // Если ONNX не загружен - просто тикаем
    if (!bOnnxLoaded)
    {
        // Каждые 60 кадров выводим лог
        if (FrameCounter % 60 == 0)
        {
            UE_LOG(LogTemp, Log, TEXT("[LMM] Tick #%d (no ONNX)"), FrameCounter);
        }
        return;
    }

    // С ONNX - ТЕСТОВЫЙ РЕЖИМ: только Projector + Decompressor
    static int32 DebugCounter = 0;
    DebugCounter++;
    bool bLogThisFrame = (DebugCounter <= 5) || (DebugCounter % 60 == 0);

    // Каждый кадр вызываем Projector (без Stepper)
    {
        TArray<float> Query = BuildQueryFeatures();
        TArray<float> NormQuery = NormalizeFeatures(Query);

        if (bLogThisFrame && NormQuery.Num() >= 4)
        {
            UE_LOG(LogTemp, Warning, TEXT("[LMM] Frame#%d Query[0-3]: %.4f, %.4f, %.4f, %.4f"),
                DebugCounter, NormQuery[0], NormQuery[1], NormQuery[2], NormQuery[3]);
        }

        TArray<float> NewF, NewZ;
        if (RunProjector(NormQuery, NewF, NewZ))
        {
            CurrentFeatures = NewF;
            CurrentLatentZ = NewZ;
        }
    }

    TArray<float> NormPose;
    if (RunDecompressor(CurrentFeatures, CurrentLatentZ, NormPose))
    {
        CurrentPose = DenormalizePose(NormPose);

        if (bLogThisFrame && CurrentPose.Num() >= 6)
        {
            UE_LOG(LogTemp, Warning, TEXT("[LMM] Frame#%d POSE[0-5]: %.2f, %.2f, %.2f, %.2f, %.2f, %.2f"),
                DebugCounter, CurrentPose[0], CurrentPose[1], CurrentPose[2],
                CurrentPose[3], CurrentPose[4], CurrentPose[5]);
        }

        // Включаем Control Rig!
        ApplyPoseToControlRig();
    }
}

// ==================== PUBLIC ====================

void ULMMDecompressorComponent::SetDesiredVelocity(FVector Velocity) { DesiredVelocity = Velocity; }
void ULMMDecompressorComponent::SetDesiredFacing(FVector Direction)
{
    DesiredFacing = Direction.GetSafeNormal();
    if (DesiredFacing.IsNearlyZero()) DesiredFacing = FVector::ForwardVector;
}
void ULMMDecompressorComponent::ForceProjection() { FrameCounter = ProjectionInterval; }
TArray<float> ULMMDecompressorComponent::GetCurrentPose() const { return CurrentPose; }

// ==================== QUERY ====================

TArray<float> ULMMDecompressorComponent::BuildQueryFeatures()
{
    TArray<float> Q;
    Q.SetNum(FEATURE_DIM);

    // Позиции ног - пока используем mean значения (нейтральная поза)
    // Индексы 0-7: позиции левой и правой ноги
    for (int32 i = 0; i < 8; i++)
    {
        Q[i] = (i < FeatureMean.Num()) ? FeatureMean[i] : 0.0f;
    }

    // Скорость бедра (в метрах/сек, конвертируем из UE координат)
    // UE: X=forward, Y=right, Z=up (cm/s)
    // LMM: X=right, Z=forward (m/s)
    float VX = DesiredVelocity.Y / 100.0f;  // right
    float VZ = DesiredVelocity.X / 100.0f;  // forward
    Q[8] = VX;
    Q[9] = VZ;

    // Траектория (позиции в будущем)
    float T[] = { 0.33f, 0.66f, 1.0f };  // времена в секундах
    for (int32 i = 0; i < 3; i++)
    {
        Q[10 + i * 2] = VX * T[i];
        Q[11 + i * 2] = VZ * T[i];
    }

    // Направление взгляда
    for (int32 i = 0; i < 3; i++)
    {
        Q[16 + i * 2] = DesiredFacing.Y;  // right component
        Q[17 + i * 2] = DesiredFacing.X;  // forward component
    }

    return Q;
}

TArray<float> ULMMDecompressorComponent::NormalizeFeatures(const TArray<float>& Raw)
{
    TArray<float> N; N.SetNum(FEATURE_DIM);
    for (int32 i = 0; i < FEATURE_DIM; i++)
    {
        float M = (i < FeatureMean.Num()) ? FeatureMean[i] : 0.0f;
        float S = (i < FeatureStd.Num()) ? FeatureStd[i] : 1.0f;
        if (FMath::Abs(S) < 1e-8f) S = 1.0f;
        N[i] = (Raw[i] - M) / S;
    }
    return N;
}

TArray<float> ULMMDecompressorComponent::DenormalizePose(const TArray<float>& Norm)
{
    TArray<float> R; R.SetNum(POSE_DIM);
    for (int32 i = 0; i < POSE_DIM; i++)
    {
        float M = (i < PoseMean.Num()) ? PoseMean[i] : 0.0f;
        float S = (i < PoseStd.Num()) ? PoseStd[i] : 1.0f;
        R[i] = Norm[i] * S + M;
    }
    return R;
}

// ==================== INFERENCE ====================

bool ULMMDecompressorComponent::RunProjector(const TArray<float>& Query,
    TArray<float>& OutF, TArray<float>& OutZ)
{
#if WITH_ONNX
    if (!ProjectorSession || Query.Num() != FEATURE_DIM) return false;

    try
    {
        Ort::Session* Session = static_cast<Ort::Session*>(ProjectorSession);
        Ort::MemoryInfo Mem = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);

        std::vector<int64_t> Shape = { 1, FEATURE_DIM };
        std::vector<float> Data(Query.GetData(), Query.GetData() + Query.Num());

        Ort::Value In = Ort::Value::CreateTensor<float>(Mem, Data.data(), Data.size(), Shape.data(), Shape.size());

        const char* InN[] = { "query" };
        const char* OutN[] = { "features_z" };

        auto Out = Session->Run(Ort::RunOptions{ nullptr }, InN, &In, 1, OutN, 1);
        float* D = Out[0].GetTensorMutableData<float>();

        OutF.SetNum(FEATURE_DIM);
        OutZ.SetNum(LATENT_DIM);
        for (int32 i = 0; i < FEATURE_DIM; i++) OutF[i] = D[i];
        for (int32 i = 0; i < LATENT_DIM; i++) OutZ[i] = D[FEATURE_DIM + i];

        return true;
    }
    catch (...) { return false; }
#else
    return false;
#endif
}

bool ULMMDecompressorComponent::RunStepper(const TArray<float>& F, const TArray<float>& Z,
    TArray<float>& OutDF, TArray<float>& OutDZ)
{
#if WITH_ONNX
    if (!StepperSession || F.Num() != FEATURE_DIM || Z.Num() != LATENT_DIM) return false;

    try
    {
        Ort::Session* Session = static_cast<Ort::Session*>(StepperSession);
        Ort::MemoryInfo Mem = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);

        std::vector<int64_t> FS = { 1, FEATURE_DIM }, ZS = { 1, LATENT_DIM };
        std::vector<float> FD(F.GetData(), F.GetData() + F.Num());
        std::vector<float> ZD(Z.GetData(), Z.GetData() + Z.Num());

        Ort::Value FT = Ort::Value::CreateTensor<float>(Mem, FD.data(), FD.size(), FS.data(), FS.size());
        Ort::Value ZT = Ort::Value::CreateTensor<float>(Mem, ZD.data(), ZD.size(), ZS.data(), ZS.size());

        std::vector<Ort::Value> Ins;
        Ins.push_back(std::move(FT));
        Ins.push_back(std::move(ZT));

        const char* InN[] = { "features", "latent_z" };
        const char* OutN[] = { "delta" };

        auto Out = Session->Run(Ort::RunOptions{ nullptr }, InN, Ins.data(), 2, OutN, 1);
        float* D = Out[0].GetTensorMutableData<float>();

        OutDF.SetNum(FEATURE_DIM);
        OutDZ.SetNum(LATENT_DIM);
        for (int32 i = 0; i < FEATURE_DIM; i++) OutDF[i] = D[i];
        for (int32 i = 0; i < LATENT_DIM; i++) OutDZ[i] = D[FEATURE_DIM + i];

        return true;
    }
    catch (...) { return false; }
#else
    return false;
#endif
}

bool ULMMDecompressorComponent::RunDecompressor(const TArray<float>& F, const TArray<float>& Z,
    TArray<float>& OutP)
{
#if WITH_ONNX
    if (!DecompressorSession || F.Num() != FEATURE_DIM || Z.Num() != LATENT_DIM) return false;

    try
    {
        Ort::Session* Session = static_cast<Ort::Session*>(DecompressorSession);
        Ort::MemoryInfo Mem = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);

        std::vector<int64_t> FS = { 1, FEATURE_DIM }, ZS = { 1, LATENT_DIM };
        std::vector<float> FD(F.GetData(), F.GetData() + F.Num());
        std::vector<float> ZD(Z.GetData(), Z.GetData() + Z.Num());

        Ort::Value FT = Ort::Value::CreateTensor<float>(Mem, FD.data(), FD.size(), FS.data(), FS.size());
        Ort::Value ZT = Ort::Value::CreateTensor<float>(Mem, ZD.data(), ZD.size(), ZS.data(), ZS.size());

        std::vector<Ort::Value> Ins;
        Ins.push_back(std::move(FT));
        Ins.push_back(std::move(ZT));

        const char* InN[] = { "features", "latent_z" };
        const char* OutN[] = { "pose" };

        auto Out = Session->Run(Ort::RunOptions{ nullptr }, InN, Ins.data(), 2, OutN, 1);
        float* D = Out[0].GetTensorMutableData<float>();

        OutP.SetNum(POSE_DIM);
        for (int32 i = 0; i < POSE_DIM; i++) OutP[i] = D[i];

        return true;
    }
    catch (...) { return false; }
#else
    return false;
#endif
}

// ==================== APPLY ====================

void ULMMDecompressorComponent::ApplyPoseToControlRig()
{
    static int32 ApplyCounter = 0;
    ApplyCounter++;

    // Логируем позу иногда
    if (ApplyCounter % 120 == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("[LMM] ApplyPose #%d - Hips: (%.1f, %.1f, %.1f)"),
            ApplyCounter, CurrentPose[0], CurrentPose[1], CurrentPose[2]);
    }

    // TODO: Применение позы через Control Rig или Animation Blueprint
}