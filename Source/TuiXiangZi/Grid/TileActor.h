#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TileActor.generated.h"

class UStaticMeshComponent;

/**
 * 所有 Tile 蓝图的基类。
 * 简单变体可直接使用自带的 MeshComp，复杂变体可在蓝图中自由组合组件。
 */
UCLASS(Blueprintable)
class TUIXIANGZI_API ATileActor : public AActor
{
	GENERATED_BODY()

public:
	ATileActor();

	/** 默认静态网格组件，简单变体可直接在蓝图中设置 Mesh 和 Material */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TileVisual")
	TObjectPtr<UStaticMeshComponent> MeshComp;

	/**
	 * GridManager 在生成后调用，子类可覆写以自定义定位/缩放逻辑。
	 * 默认实现根据 CellType 设置缩放和位置偏移（Wall 抬高等）。
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "TileVisual")
	void InitializeForGrid(float CellSize, FIntPoint GridPos);
};
