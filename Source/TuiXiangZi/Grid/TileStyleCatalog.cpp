#include "Grid/TileStyleCatalog.h"
#include "Grid/TileActor.h"

#if WITH_EDITOR
#include "Components/SceneCaptureComponent2D.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
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
// 检查 Actor 类（含蓝图）是否拥有指定 Component 子类
static bool ActorClassHasComponent(TSubclassOf<AActor> ActorClass, UClass* ComponentClass)
{
    if (!ActorClass || !ComponentClass) return false;

    // C++ 构造函数中添加的 component 在 CDO 上可见
    AActor* CDO = ActorClass->GetDefaultObject<AActor>();
    TArray<UActorComponent*> Comps;
    CDO->GetComponents(ComponentClass, Comps);
    if (Comps.Num() > 0) return true;

    // 蓝图编辑器中添加的 component 在 SimpleConstructionScript 中
    UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(ActorClass.Get());
    while (BPGC)
    {
        if (USimpleConstructionScript* SCS = BPGC->SimpleConstructionScript)
        {
            for (USCS_Node* Node : SCS->GetAllNodes())
            {
                if (Node->ComponentClass && Node->ComponentClass->IsChildOf(ComponentClass))
                    return true;
            }
        }
        BPGC = Cast<UBlueprintGeneratedClass>(BPGC->GetSuperClass());
    }

    return false;
}

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

    // 验证每个 Style 的 ActorClass 是否包含其 CellType 所需的 Component
    for (const FTileVisualStyle& Style : Styles)
    {
        if (!Style.ActorClass) continue;

        UClass* RequiredComp = GridTypeUtils::GetRequiredTileComponentClass(Style.ApplicableType);
        if (!RequiredComp) continue;

        if (!ActorClassHasComponent(Style.ActorClass, RequiredComp))
        {
            UE_LOG(LogTemp, Error,
                TEXT("TileStyleCatalog: Style '%s' (type=%s) → ActorClass '%s' is missing required component '%s'. "
                     "This tile will not function correctly at runtime!"),
                *Style.StyleId.ToString(),
                *GridTypeUtils::CellTypeToString(Style.ApplicableType),
                *Style.ActorClass->GetName(),
                *RequiredComp->GetName());
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

        // 找第一个有 Mesh 的 StaticMeshComponent（蓝图可能在 C++ MeshComp 之外另建了组件）
        UStaticMeshComponent* MeshComp = nullptr;
        TArray<UStaticMeshComponent*> AllMeshComps;
        PreviewActor->GetComponents<UStaticMeshComponent>(AllMeshComps);
        for (UStaticMeshComponent* SMC : AllMeshComps)
        {
            if (SMC->GetStaticMesh())
            {
                MeshComp = SMC;
                break;
            }
        }

        if (!MeshComp)
        {
            UE_LOG(LogTemp, Warning, TEXT("  [%s] No StaticMeshComponent with valid mesh found"), *Style.StyleId.ToString());
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

        // 灯光：相机方向主光 + 三向补光，无阴影，均匀照亮
        auto AddLight = [&](FRotator Rot, float Intensity)
        {
            UDirectionalLightComponent* L = NewObject<UDirectionalLightComponent>(PreviewActor);
            L->SetWorldRotation(Rot);
            L->Intensity = Intensity;
            L->CastShadows = false;
            L->CastStaticShadows = false;
            L->CastDynamicShadows = false;
            L->RegisterComponent();
        };

        AddLight(CamRot, 4.0f);                            // 相机方向主光
        AddLight(FRotator(-45.0f, -45.0f, 0.0f), 2.0f);   // 左上前
        AddLight(FRotator(-45.0f, 135.0f, 0.0f), 2.0f);   // 右上后
        AddLight(FRotator(-90.0f, 0.0f, 0.0f), 1.5f);     // 正上方

        // 创建 SceneCapture（Actor 已在远处，不需要 ShowOnlyList）
        USceneCaptureComponent2D* Capture = NewObject<USceneCaptureComponent2D>(PreviewActor);
        Capture->TextureTarget = RT;
        Capture->SetWorldLocationAndRotation(CamPos, CamRot);
        Capture->FOVAngle = 30.0f;
        Capture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
        Capture->bCaptureEveryFrame = false;
        Capture->bCaptureOnMovement = false;
        Capture->bAlwaysPersistRenderingState = true;
        Capture->RegisterComponent();

        // 等待着色器编译和场景代理就绪
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

    // 标记 DataAsset 为已修改，提示用户保存
    MarkPackageDirty();

    UE_LOG(LogTemp, Log, TEXT("TileStyleCatalog: Generated %d/%d thumbnails. Save the asset to persist."),
        SuccessCount, Styles.Num());
}
#endif
