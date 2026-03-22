#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EditorGridVisualizer.generated.h"

class UProceduralMeshComponent;
class UMaterialInstanceDynamic;
class ATextRenderActor;

UCLASS(Blueprintable)
class TUIXIANGZI_API AEditorGridVisualizer : public AActor
{
    GENERATED_BODY()

public:
    AEditorGridVisualizer();

    UFUNCTION(BlueprintCallable, Category = "Editor|Grid")
    void UpdateGridLines(FIntPoint MinBound, FIntPoint MaxBound, float InCellSize);

    /** Toggle coordinate labels on each grid cell. */
    UFUNCTION(BlueprintCallable, Category = "Editor|Grid")
    void ToggleCoordinateLabels();

    /** Whether coordinate labels are currently visible. */
    UFUNCTION(BlueprintCallable, Category = "Editor|Grid")
    bool AreCoordinateLabelsVisible() const { return bShowCoordinateLabels; }

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Editor|Grid")
    UProceduralMeshComponent* GridMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Editor|Grid")
    int32 PaddingCells = 2;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Editor|Grid")
    float LineThickness = 2.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Editor|Grid")
    float LineZ = 0.1f;

    FIntPoint CachedMinBound;
    FIntPoint CachedMaxBound;
    float CellSize = 100.0f;

    UPROPERTY()
    UMaterialInstanceDynamic* GridLineMaterial;

    // ===== Coordinate Labels =====
    bool bShowCoordinateLabels = false;

    UPROPERTY()
    TArray<ATextRenderActor*> CoordinateLabelActors;

    void RebuildCoordinateLabels();
    void DestroyCoordinateLabels();

    void AddLineQuad(FVector Start, FVector End, float Thickness,
        TArray<FVector>& Vertices, TArray<int32>& Triangles,
        TArray<FVector>& Normals, TArray<FVector2D>& UVs, TArray<FColor>& Colors);

    void CreateGridMaterial();
};
