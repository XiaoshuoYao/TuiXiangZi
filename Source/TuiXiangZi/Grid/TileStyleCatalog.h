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

#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

    /** 为所有 Style 条目重新渲染缩略图 */
    void RegenerateAllThumbnails();

    /** 为单个 Style 渲染缩略图（Mesh+Material → RenderTarget） */
    static UTextureRenderTarget2D* RenderStyleThumbnail(UObject* Outer, UStaticMesh* Mesh, UMaterialInterface* Material, int32 Resolution = 128);
#endif
};
