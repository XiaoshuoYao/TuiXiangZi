#include "Grid/TileStyleCatalog.h"
#include "Engine/TextureRenderTarget2D.h"

#if WITH_EDITOR
#include "Components/SceneCaptureComponent2D.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
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

    // Mesh 或 Material 变化时自动重新渲染缩略图
    RegenerateAllThumbnails();
}

void UTileStyleCatalog::RegenerateAllThumbnails()
{
    for (FTileVisualStyle& Style : Styles)
    {
        if (Style.Mesh)
        {
            Style.Thumbnail = RenderStyleThumbnail(this, Style.Mesh, Style.Material);
        }
        else
        {
            Style.Thumbnail = nullptr;
        }
    }
}

UTextureRenderTarget2D* UTileStyleCatalog::RenderStyleThumbnail(
    UObject* Outer, UStaticMesh* Mesh, UMaterialInterface* Material, int32 Resolution)
{
    if (!Mesh) return nullptr;

    // 创建 RenderTarget
    UTextureRenderTarget2D* RT = NewObject<UTextureRenderTarget2D>(Outer);
    RT->InitAutoFormat(Resolution, Resolution);
    RT->ClearColor = FLinearColor(0.15f, 0.15f, 0.15f, 1.0f);

    // 创建临时预览世界
    UWorld* PreviewWorld = UWorld::CreateWorld(EWorldType::EditorPreview, false);
    if (!PreviewWorld) return nullptr;

    FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::EditorPreview);
    WorldContext.SetCurrentWorld(PreviewWorld);

    // 生成临时 Actor 承载 Mesh
    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AActor* PreviewActor = PreviewWorld->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, SpawnParams);
    if (!PreviewActor)
    {
        GEngine->DestroyWorldContext(PreviewWorld);
        PreviewWorld->DestroyWorld(false);
        return nullptr;
    }

    UStaticMeshComponent* MeshComp = NewObject<UStaticMeshComponent>(PreviewActor);
    MeshComp->RegisterComponent();
    PreviewActor->SetRootComponent(MeshComp);
    MeshComp->SetStaticMesh(Mesh);
    if (Material)
    {
        MeshComp->SetMaterial(0, Material);
    }

    // 根据 Mesh 包围盒计算相机位置
    FBoxSphereBounds MeshBounds = Mesh->GetBounds();
    float BoundsRadius = MeshBounds.SphereRadius;
    if (BoundsRadius < KINDA_SMALL_NUMBER) BoundsRadius = 100.0f;

    FVector CameraLocation = MeshBounds.Origin + FVector(-BoundsRadius * 1.5f, -BoundsRadius * 1.5f, BoundsRadius * 1.2f);
    FRotator CameraRotation = (MeshBounds.Origin - CameraLocation).Rotation();

    // 创建 SceneCapture
    USceneCaptureComponent2D* CaptureComp = NewObject<USceneCaptureComponent2D>(PreviewActor);
    CaptureComp->RegisterComponent();
    CaptureComp->TextureTarget = RT;
    CaptureComp->SetWorldLocationAndRotation(CameraLocation, CameraRotation);
    CaptureComp->FOVAngle = 30.0f;
    CaptureComp->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
    CaptureComp->bCaptureEveryFrame = false;
    CaptureComp->bCaptureOnMovement = false;
    CaptureComp->ShowOnlyComponent(MeshComp);

    // 执行一次捕获
    CaptureComp->CaptureScene();

    // 清理预览世界
    PreviewActor->Destroy();
    GEngine->DestroyWorldContext(PreviewWorld);
    PreviewWorld->DestroyWorld(false);

    return RT;
}
#endif
