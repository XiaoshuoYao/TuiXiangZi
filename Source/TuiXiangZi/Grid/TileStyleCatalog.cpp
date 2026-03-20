#include "Grid/TileStyleCatalog.h"
#include "Grid/TileVisualActor.h"

#if WITH_EDITOR
#include "Components/SceneCaptureComponent2D.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/TextureRenderTarget2D.h"
#include "RenderingThread.h"
#include "Editor.h"
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
}

void UTileStyleCatalog::GenerateAllThumbnails()
{
    // 获取编辑器世界
    UWorld* EditorWorld = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!EditorWorld)
    {
        UE_LOG(LogTemp, Error, TEXT("TileStyleCatalog: No editor world available!"));
        return;
    }

    // 远离场景原点的临时位置
    const FVector ThumbOrigin(50000.0f, 50000.0f, 50000.0f);

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    // 创建临时灯光
    AActor* LightActor = EditorWorld->SpawnActor<AActor>(
        AActor::StaticClass(), FTransform(ThumbOrigin), SpawnParams);
    if (!LightActor)
    {
        UE_LOG(LogTemp, Error, TEXT("TileStyleCatalog: Failed to spawn light actor!"));
        return;
    }

    UDirectionalLightComponent* KeyLight = NewObject<UDirectionalLightComponent>(LightActor);
    KeyLight->SetWorldRotation(FRotator(-45.0f, -45.0f, 0.0f));
    KeyLight->Intensity = 3.0f;
    KeyLight->RegisterComponent();

    UDirectionalLightComponent* FillLight = NewObject<UDirectionalLightComponent>(LightActor);
    FillLight->SetWorldRotation(FRotator(-30.0f, 135.0f, 0.0f));
    FillLight->Intensity = 1.5f;
    FillLight->RegisterComponent();

    EditorWorld->SendAllEndOfFrameUpdates();
    FlushRenderingCommands();

    // 创建临时 RenderTarget
    UTextureRenderTarget2D* RT = NewObject<UTextureRenderTarget2D>(GetTransientPackage());
    RT->InitAutoFormat(ThumbnailResolution, ThumbnailResolution);
    RT->ClearColor = FLinearColor(0.15f, 0.15f, 0.15f, 1.0f);
    RT->UpdateResourceImmediate(true);

    int32 SuccessCount = 0;

    for (FTileVisualStyle& Style : Styles)
    {
        if (!Style.ActorClass)
        {
            Style.Thumbnail = nullptr;
            continue;
        }

        // 在编辑器世界中生成蓝图 Actor（蓝图构造链完整执行）
        AActor* PreviewActor = EditorWorld->SpawnActor<AActor>(
            Style.ActorClass, FTransform(ThumbOrigin), SpawnParams);
        if (!PreviewActor)
        {
            UE_LOG(LogTemp, Warning, TEXT("  [%s] Failed to spawn actor"), *Style.StyleId.ToString());
            continue;
        }

        UStaticMeshComponent* MeshComp = PreviewActor->FindComponentByClass<UStaticMeshComponent>();
        if (!MeshComp || !MeshComp->GetStaticMesh())
        {
            UE_LOG(LogTemp, Warning, TEXT("  [%s] No StaticMesh found"), *Style.StyleId.ToString());
            PreviewActor->Destroy();
            continue;
        }

        // 等待场景代理创建
        EditorWorld->SendAllEndOfFrameUpdates();
        FlushRenderingCommands();

        // 计算相机位置
        FBoxSphereBounds Bounds = MeshComp->Bounds;
        float Radius = FMath::Max(Bounds.SphereRadius, 1.0f);
        FVector CamPos = Bounds.Origin + FVector(-Radius * 2.5f, Radius * 1.5f, Radius * 2.0f);
        FRotator CamRot = (Bounds.Origin - CamPos).Rotation();

        // 创建 SceneCapture
        USceneCaptureComponent2D* Capture = NewObject<USceneCaptureComponent2D>(PreviewActor);
        Capture->TextureTarget = RT;
        Capture->SetWorldLocationAndRotation(CamPos, CamRot);
        Capture->FOVAngle = 30.0f;
        Capture->CaptureSource = ESceneCaptureSource::SCS_SceneColorHDR;
        Capture->bCaptureEveryFrame = false;
        Capture->bCaptureOnMovement = false;
        Capture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
        Capture->ShowOnlyActors.Add(PreviewActor);
        Capture->RegisterComponent();

        EditorWorld->SendAllEndOfFrameUpdates();
        FlushRenderingCommands();

        // 清空 RT 并捕获
        RT->UpdateResourceImmediate(true);
        Capture->CaptureScene();
        FlushRenderingCommands();

        // 将 RenderTarget 转为持久化的 UTexture2D（保存在 DataAsset 的 Package 中）
        FString TexName = FString::Printf(TEXT("Thumb_%s"), *Style.StyleId.ToString());
        UTexture2D* NewTex = RT->ConstructTexture2D(this, TexName, RF_NoFlags);
        if (NewTex)
        {
            NewTex->LODGroup = TEXTUREGROUP_UI;
            NewTex->UpdateResource();
            Style.Thumbnail = NewTex;
            SuccessCount++;
            UE_LOG(LogTemp, Log, TEXT("  [%s] OK"), *Style.StyleId.ToString());
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("  [%s] ConstructTexture2D failed"), *Style.StyleId.ToString());
        }

        PreviewActor->Destroy();
    }

    // 清理
    LightActor->Destroy();

    // 标记 DataAsset 为已修改，提示用户保存
    MarkPackageDirty();

    UE_LOG(LogTemp, Log, TEXT("TileStyleCatalog: Generated %d/%d thumbnails. Save the asset to persist."),
        SuccessCount, Styles.Num());
}
#endif
