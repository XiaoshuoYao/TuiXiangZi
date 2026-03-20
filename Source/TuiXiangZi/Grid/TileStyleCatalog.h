#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Grid/GridTypes.h"
#include "TileStyleCatalog.generated.h"

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

    /** 该变体对应的蓝图 Actor 类 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TileStyle")
    TSubclassOf<AActor> ActorClass;

    /** 缩略图（点击 GenerateAllThumbnails 按钮自动生成，随 DataAsset 保存） */
    UPROPERTY(VisibleAnywhere, Category = "TileStyle")
    UTexture2D* Thumbnail = nullptr;
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

    /** 编辑器按钮：为所有 Style 生成缩略图 */
    UFUNCTION(CallInEditor, Category = "TileStyle|Thumbnail")
    void GenerateAllThumbnails();
#endif
};
