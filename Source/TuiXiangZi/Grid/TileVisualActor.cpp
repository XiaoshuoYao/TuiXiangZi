#include "Grid/TileVisualActor.h"
#include "Components/StaticMeshComponent.h"

ATileVisualActor::ATileVisualActor()
{
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
	MeshComp->SetupAttachment(RootComponent);
	MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ATileVisualActor::InitializeForGrid_Implementation(float CellSize, FIntPoint GridPos)
{
	// 默认实现：按 CellSize 缩放
	SetActorScale3D(FVector(CellSize / 100.0f));
}
