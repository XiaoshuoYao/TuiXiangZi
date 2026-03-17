#include "Gameplay/PushableBox.h"
#include "Grid/GridManager.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "Curves/CurveFloat.h"

APushableBox::APushableBox()
{
    PrimaryActorTick.bCanEverTick = false;

    MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
    RootComponent = MeshComp;
    MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    // 默认使用 Cube Mesh
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (CubeMesh.Succeeded())
    {
        MeshComp->SetStaticMesh(CubeMesh.Object);
    }

    MoveTimeline = CreateDefaultSubobject<UTimelineComponent>(TEXT("MoveTimeline"));
}

void APushableBox::BeginPlay()
{
    Super::BeginPlay();

    // 查找 GridManager
    GridManagerRef = Cast<AGridManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AGridManager::StaticClass()));

    // 缩放 0.9 留缝隙
    if (GridManagerRef)
    {
        const float Scale = GridManagerRef->CellSize / 100.0f * 0.9f;
        MeshComp->SetWorldScale3D(FVector(Scale));
    }
    else
    {
        MeshComp->SetWorldScale3D(FVector(0.9f));
    }

    // 创建动态材质实例
    UMaterialInterface* BaseMaterial = MeshComp->GetMaterial(0);
    if (BaseMaterial)
    {
        DynamicMaterialInst = UMaterialInstanceDynamic::Create(BaseMaterial, this);
        MeshComp->SetMaterial(0, DynamicMaterialInst);
    }

    // 设置 Timeline
    if (MoveCurve)
    {
        FOnTimelineFloat UpdateDelegate;
        UpdateDelegate.BindUFunction(this, FName("OnMoveTimelineUpdate"));
        MoveTimeline->AddInterpFloat(MoveCurve, UpdateDelegate);

        FOnTimelineEvent FinishedDelegate;
        FinishedDelegate.BindUFunction(this, FName("OnMoveTimelineFinished"));
        MoveTimeline->SetTimelineFinishedFunc(FinishedDelegate);

        MoveTimeline->SetPlayRate(1.0f / MoveDuration);
    }

    // 绑定 GridManager 广播
    if (GridManagerRef)
    {
        GridManagerRef->OnActorLogicalMoved.AddUObject(this, &APushableBox::OnActorLogicalMoved);
    }
}

void APushableBox::SnapToGridPos(FIntPoint GridPos)
{
    CurrentGridPos = GridPos;
    if (GridManagerRef)
    {
        FVector WorldPos = GridManagerRef->GridToWorld(GridPos);
        WorldPos.Z = GetActorLocation().Z;
        SetActorLocation(WorldPos);
    }
}

void APushableBox::SmoothMoveTo(FVector TargetWorldPos)
{
    if (!MoveCurve)
    {
        // 无曲线时直接瞬移
        SetActorLocation(TargetWorldPos);
        return;
    }

    bIsMoving = true;
    MoveStartLocation = GetActorLocation();
    MoveTargetLocation = TargetWorldPos;
    MoveTargetLocation.Z = MoveStartLocation.Z; // 保持 Z 不变

    MoveTimeline->PlayFromStart();
}

void APushableBox::OnMoveTimelineUpdate(float Alpha)
{
    FVector NewLocation = FMath::Lerp(MoveStartLocation, MoveTargetLocation, Alpha);
    SetActorLocation(NewLocation);
}

void APushableBox::OnMoveTimelineFinished()
{
    OnMoveCompleted();
}

void APushableBox::OnMoveCompleted()
{
    bIsMoving = false;
    // 精确对齐格子中心
    if (GridManagerRef)
    {
        FVector SnapPos = GridManagerRef->GridToWorld(CurrentGridPos);
        SnapPos.Z = GetActorLocation().Z;
        SetActorLocation(SnapPos);
    }
}

void APushableBox::OnActorLogicalMoved(AActor* Actor, FIntPoint From, FIntPoint To)
{
    // 只响应自身的移动事件
    if (Actor != this) return;

    CurrentGridPos = To;
    if (GridManagerRef)
    {
        SmoothMoveTo(GridManagerRef->GridToWorld(To));
    }
}

void APushableBox::PlayFallIntoHoleAnim()
{
    // Hide immediately and disable collision, then schedule destroy
    SetActorHiddenInGame(true);
    SetActorEnableCollision(false);
    SetLifeSpan(1.0f);
}
