#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GridTileComponent.generated.h"

class UStaticMeshComponent;

/** Describes what editor workflow a tile component triggers when placed. */
UENUM()
enum class EEditorPlacementFlow : uint8
{
	None           UMETA(DisplayName = "None"),
	AssignGroup    UMETA(DisplayName = "Assign Group"),
	// PairPlacement  — future: teleporter pair placement
};

UCLASS(Abstract, Blueprintable, ClassGroup = "GridTile",
	meta = (BlueprintSpawnableComponent))
class TUIXIANGZI_API UGridTileComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GridTile")
	FIntPoint GridPos;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GridTile")
	int32 GroupId = -1;

	// ---- Editor Placement Flow ----
	virtual EEditorPlacementFlow GetEditorPlacementFlow() const { return EEditorPlacementFlow::None; }

	// ---- Color system ----
	virtual void SetGroupColor(FLinearColor BaseColor, FLinearColor ActiveColor);

	FLinearColor CachedBaseColor = FLinearColor::White;
	FLinearColor CachedActiveColor = FLinearColor::White;

protected:
	UMaterialInstanceDynamic* DynMaterial = nullptr;

	void CreateDynamicMaterial();
	void ApplyColor(FLinearColor Color);
	UStaticMeshComponent* FindOwnerMeshComp() const;
};
