#include "Grid/TileStyleCatalog.h"
#include "Grid/TileVisualActor.h"
#include "Engine/TextureRenderTarget2D.h"

#if WITH_EDITOR
#include "Components/SceneCaptureComponent2D.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "RenderingThread.h"
#endif

const FTileVisualStyle* UTileStyleCatalog::FindStyle(FName StyleId) const
{
    for (const FTileVisualStyle& Style : Styles)
    {
        if (Style.StyleId == StyleId)
        {
            return &Style;
        }
    }
    return nullptr;
}

TArray<const FTileVisualStyle*> UTileStyleCatalog::GetStylesForType(EGridCellType Type) const
{
    TArray<const FTileVisualStyle*> Result;
    for (const FTileVisualStyle& Style : Styles)
    {
        if (Style.ApplicableType == Type)
        {
            Result.Add(&Style);
        }
    }
    return Result;
}

bool UTileStyleCatalog::HasStyle(FName StyleId) const
{
    return FindStyle(StyleId) != nullptr;
}

#if WITH_EDITOR
void UTileStyleCatalog::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);

    // 检查重复 StyleId
    TSet<FName> SeenIds;
    for (const FTileVisualStyle& Style : Styles)
    {
        if (Style.StyleId.IsNone()) continue;
        if (SeenIds.Contains(Style.StyleId))
        {
            UE_LOG(LogTemp, Warning, TEXT("TileStyleCatalog: Duplicate StyleId '%s' detected!"), *Style.StyleId.ToString());
        }
        else
        {
            SeenIds.Add(Style.StyleId);
        }
    }

    // VisualActorClass 变化时自动重新渲染缩略图
    RegenerateAllThumbnails();
}

void UTileStyleCatalog::EnsureThumbnailsGenerated()
{
    // 检查是否有缩略图缺失（Transient 属性在 PIE 启动后为 nullptr）
    bool bAnyMissing = false;
    for (const FTileVisualStyle& Style : Styles)
    {
        if (Style.ActorClass && !Style.Thumbnail)
        {
            bAnyMissing = true;
            break;
        }
    }
    if (bAnyMissing)
    {
        RegenerateAllThumbnails();
    }
}

void UTileStyleCatalog::RegenerateAllThumbnails()
{
    // 1. 检查是否有需要渲染的条目
    bool bAnyNeedRender = false;
    for (const FTileVisualStyle& Style : Styles)
    {
        if (Style.ActorClass) { bAnyNeedRender = true; break; }
    }

    if (!bAnyNeedRender)
    {
        // 全部清空，旧 RT 由 GC 回收（Outer 是 GetTransientPackage）
        for (FTileVisualStyle& Style : Styles)
            Style.Thumbnail = nullptr;
        return;
    }

    // 2. 创建共享预览世界（所有条目复用同一个）
    UWorld* PreviewWorld = UWorld::CreateWorld(EWorldType::EditorPreview, false, TEXT("TileThumbWorld"));
    if (!PreviewWorld) return;

    FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::EditorPreview);
    WorldContext.SetCurrentWorld(PreviewWorld);

    // 确保场景完全初始化
    PreviewWorld->InitializeActorsForPlay(FURL());

    // 3. 添加灯光（Key Light + Fill Light + Sky Light）
    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    AActor* LightActor = PreviewWorld->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, SpawnParams);
    {
        // 主光：从左上方 45° 打下来
        UDirectionalLightComponent* KeyLight = NewObject<UDirectionalLightComponent>(LightActor);
        KeyLight->SetWorldRotation(FRotator(-45.0f, -45.0f, 0.0f));
        KeyLight->Intensity = 4.0f;
        KeyLight->RegisterComponent();

        // 补光：从右下方打，强度较低
        UDirectionalLightComponent* FillLight = NewObject<UDirectionalLightComponent>(LightActor);
        FillLight->SetWorldRotation(FRotator(-30.0f, 135.0f, 0.0f));
        FillLight->Intensity = 1.5f;
        FillLight->RegisterComponent();
    }

    // 4. 逐条目渲染
    for (FTileVisualStyle& Style : Styles)
    {
        TSubclassOf<AActor> ClassToRender = Style.ActorClass;
        if (!ClassToRender)
        {
            Style.Thumbnail = nullptr;
            continue;
        }

        // 复用已有 RT，避免反复分配
        if (!Style.Thumbnail || !IsValid(Style.Thumbnail)
            || Style.Thumbnail->SizeX != ThumbnailResolution)
        {
            Style.Thumbnail = NewObject<UTextureRenderTarget2D>(GetTransientPackage());
            Style.Thumbnail->InitAutoFormat(ThumbnailResolution, ThumbnailResolution);
            Style.Thumbnail->ClearColor = FLinearColor(0.15f, 0.15f, 0.15f, 1.0f);
            Style.Thumbnail->UpdateResourceImmediate(true);
        }

        RenderActorClassToTarget(PreviewWorld, Style.Thumbnail, ClassToRender);
    }

    // 5. 清理：一次性销毁共享世界
    LightActor->Destroy();
    GEngine->DestroyWorldContext(PreviewWorld);
    PreviewWorld->DestroyWorld(false);
}

void UTileStyleCatalog::RenderActorClassToTarget(
    UWorld* PreviewWorld, UTextureRenderTarget2D* RT,
    TSubclassOf<AActor> ActorClass)
{
    if (!PreviewWorld || !RT || !ActorClass) return;

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    // 在预览世界中临时生成蓝图实例（支持 ATileVisualActor 及其子类）
    AActor* PreviewActor = PreviewWorld->SpawnActor<AActor>(
        ActorClass, FTransform::Identity, SpawnParams);
    if (!PreviewActor) return;

    // 从蓝图实例中找到第一个 StaticMeshComponent 来确定包围盒和渲染内容
    UStaticMeshComponent* MeshComp = PreviewActor->FindComponentByClass<UStaticMeshComponent>();
    UStaticMesh* Mesh = MeshComp ? MeshComp->GetStaticMesh() : nullptr;

    if (!Mesh)
    {
        PreviewActor->Destroy();
        return;
    }

    // 根据包围盒计算相机位置
    FBoxSphereBounds Bounds = Mesh->GetBounds();
    float Radius = FMath::Max(Bounds.SphereRadius, 1.0f);

    FVector CamPos = Bounds.Origin + FVector(-Radius * 2.0f, Radius * 1.0f, Radius * 1.5f);
    FRotator CamRot = (Bounds.Origin - CamPos).Rotation();

    // SceneCapture 组件
    USceneCaptureComponent2D* Capture = NewObject<USceneCaptureComponent2D>(PreviewActor);
    Capture->RegisterComponent();
    Capture->TextureTarget = RT;
    Capture->SetWorldLocationAndRotation(CamPos, CamRot);
    Capture->FOVAngle = 30.0f;
    Capture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
    Capture->bCaptureEveryFrame = false;
    Capture->bCaptureOnMovement = false;

    // 确保场景中所有 PrimitiveSceneInfo 已注册，否则捕获到空场景
    PreviewWorld->SendAllEndOfFrameUpdates();

    // 执行一次捕获
    Capture->CaptureScene();

    // 等待 GPU 渲染完成，确保 RT 内容已写入
    FlushRenderingCommands();

    // 销毁临时 Actor（灯光留在世界里，被所有条目共享）
    PreviewActor->Destroy();
}
#endif
