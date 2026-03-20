#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Grid/GridTypes.h"
#include "TileStyleCatalog.generated.h"

class UTextureRenderTarget2D;
class ATileVisualActor;

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

    /** 该变体对应的蓝图 Actor 类（Tile/Box 均为 ATileVisualActor 子类，行为靠 Component 区分） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TileStyle")
    TSubclassOf<AActor> ActorClass;

    /** 自动从蓝图的 MeshComp 渲染生成，无需手动设置 */
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

    /** 确保缩略图已生成（PIE 运行时按需调用） */
    void EnsureThumbnailsGenerated();

private:
    /** 在预览世界中生成蓝图实例并渲染缩略图 */
    static void RenderActorClassToTarget(UWorld* PreviewWorld, UTextureRenderTarget2D* RT,
        TSubclassOf<AActor> ActorClass);
#endif
};
