#include "Editor/EditorGridVisualizer.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

AEditorGridVisualizer::AEditorGridVisualizer()
{
    PrimaryActorTick.bCanEverTick = false;

    GridMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("GridMesh"));
    RootComponent = GridMesh;

    // Disable collision on grid lines
    GridMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    CachedMinBound = FIntPoint(0, 0);
    CachedMaxBound = FIntPoint(0, 0);
    GridLineMaterial = nullptr;
}

void AEditorGridVisualizer::CreateGridMaterial()
{
    if (GridLineMaterial) return;

    // Create a simple translucent material
    UMaterial* BaseMaterial = LoadObject<UMaterial>(nullptr,
        TEXT("/Engine/EngineMaterials/DefaultMaterial.DefaultMaterial"));
    if (BaseMaterial)
    {
        GridLineMaterial = UMaterialInstanceDynamic::Create(BaseMaterial, this);
        if (GridLineMaterial)
        {
            GridLineMaterial->SetVectorParameterValue(FName("BaseColor"),
                FLinearColor(0.5f, 0.5f, 0.5f, 0.3f));
        }
    }
}

void AEditorGridVisualizer::UpdateGridLines(FIntPoint MinBound, FIntPoint MaxBound, float InCellSize)
{
    // Skip rebuild if bounds haven't changed
    if (CachedMinBound == MinBound && CachedMaxBound == MaxBound && FMath::IsNearlyEqual(CellSize, InCellSize))
    {
        return;
    }

    CachedMinBound = MinBound;
    CachedMaxBound = MaxBound;
    CellSize = InCellSize;

    // Ensure material exists
    CreateGridMaterial();

    // Clear existing mesh
    GridMesh->ClearAllMeshSections();

    // Extend bounds by padding
    FIntPoint PaddedMin = MinBound - FIntPoint(PaddingCells, PaddingCells);
    FIntPoint PaddedMax = MaxBound + FIntPoint(PaddingCells, PaddingCells);

    TArray<FVector> Vertices;
    TArray<int32> Triangles;
    TArray<FVector> Normals;
    TArray<FVector2D> UVs;
    TArray<FColor> Colors;

    // Generate vertical lines (along Y axis)
    for (int32 X = PaddedMin.X; X <= PaddedMax.X; ++X)
    {
        FVector Start(X * CellSize, PaddedMin.Y * CellSize, LineZ);
        FVector End(X * CellSize, PaddedMax.Y * CellSize, LineZ);
        AddLineQuad(Start, End, LineThickness, Vertices, Triangles, Normals, UVs, Colors);
    }

    // Generate horizontal lines (along X axis)
    for (int32 Y = PaddedMin.Y; Y <= PaddedMax.Y; ++Y)
    {
        FVector Start(PaddedMin.X * CellSize, Y * CellSize, LineZ);
        FVector End(PaddedMax.X * CellSize, Y * CellSize, LineZ);
        AddLineQuad(Start, End, LineThickness, Vertices, Triangles, Normals, UVs, Colors);
    }

    if (Vertices.Num() > 0)
    {
        GridMesh->CreateMeshSection(0, Vertices, Triangles, Normals, UVs, Colors, TArray<FProcMeshTangent>(), false);
        if (GridLineMaterial)
        {
            GridMesh->SetMaterial(0, GridLineMaterial);
        }
    }
}

void AEditorGridVisualizer::AddLineQuad(FVector Start, FVector End, float Thickness,
    TArray<FVector>& Vertices, TArray<int32>& Triangles,
    TArray<FVector>& Normals, TArray<FVector2D>& UVs, TArray<FColor>& Colors)
{
    // Calculate perpendicular direction for line width
    FVector LineDir = (End - Start).GetSafeNormal();
    FVector UpDir(0.0f, 0.0f, 1.0f);
    FVector PerpDir = FVector::CrossProduct(LineDir, UpDir).GetSafeNormal();

    float HalfThickness = Thickness * 0.5f;
    FVector Offset = PerpDir * HalfThickness;

    int32 BaseIndex = Vertices.Num();

    // Four corners of the quad
    Vertices.Add(Start - Offset);  // 0: bottom-left
    Vertices.Add(Start + Offset);  // 1: top-left
    Vertices.Add(End + Offset);    // 2: top-right
    Vertices.Add(End - Offset);    // 3: bottom-right

    // Normals (all pointing up)
    Normals.Add(UpDir);
    Normals.Add(UpDir);
    Normals.Add(UpDir);
    Normals.Add(UpDir);

    // UVs
    UVs.Add(FVector2D(0.0f, 0.0f));
    UVs.Add(FVector2D(1.0f, 0.0f));
    UVs.Add(FVector2D(1.0f, 1.0f));
    UVs.Add(FVector2D(0.0f, 1.0f));

    // Colors (semi-transparent grey)
    FColor LineColor(128, 128, 128, 76);
    Colors.Add(LineColor);
    Colors.Add(LineColor);
    Colors.Add(LineColor);
    Colors.Add(LineColor);

    // Two triangles forming the quad
    Triangles.Add(BaseIndex + 0);
    Triangles.Add(BaseIndex + 1);
    Triangles.Add(BaseIndex + 2);

    Triangles.Add(BaseIndex + 0);
    Triangles.Add(BaseIndex + 2);
    Triangles.Add(BaseIndex + 3);
}
