#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Grid/GridTypes.h"
#include "TileStyleCatalog.generated.h"

class UTextureRenderTarget2D;

USTRUCT(BlueprintType)
struct FTileVisualStyle
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TileStyle")
    FName StyleId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TileStyle")
    FString DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TileStyle")
    EGridCellType ApplicableType = EGridCellType::Floor;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TileStyle")
    UStaticMesh* Mesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TileStyle")
    UMaterialInterface* Material = nullptr;

    /** 自动从 Mesh+Material 渲染生成，无需手动设置 */
    UPROPERTY(Transient, VisibleAnywhere, Category = "TileStyle")
    UTextureRenderTarget2D* Thumbnail = nullptr;
};

UCLASS(BlueprintType)
class TUIXIANGZI_API UTileStyleCatalog : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TileStyle")
    TArray<FTileVisualStyle> Styles;

    const FTileVisualStyle* FindStyle(FName StyleId) const;

    TArray<const FTileVisualStyle*> GetStylesForType(EGridCellType Type) const;

    bool HasStyle(FName StyleId) const;

#if WITH_EDITORONLY_DATA
    /** 缩略图分辨率 */
    UPROPERTY(EditAnywhere, Category = "TileStyle|Thumbnail", meta = (ClampMin = 64, ClampMax = 512))
    int32 ThumbnailResolution = 128;
#endif

#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

    /** 为所有 Style 条目重新渲染缩略图 */
    void RegenerateAllThumbnails();

private:
    /** 将 Mesh+Material 渲染到已有的 RenderTarget 中 */
    static void RenderMeshToTarget(UWorld* PreviewWorld, UTextureRenderTarget2D* RT,
        UStaticMesh* Mesh, UMaterialInterface* Material);
#endif
};
