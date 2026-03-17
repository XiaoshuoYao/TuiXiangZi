#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GridMechanism.generated.h"

UCLASS(Abstract)
class TUIXIANGZI_API AGridMechanism : public AActor
{
	GENERATED_BODY()

public:
	AGridMechanism();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mechanism")
	FIntPoint GridPos;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mechanism")
	int32 GroupId = -1;

	virtual void OnActivate();
	virtual void OnDeactivate();
	virtual bool IsActivated() const;
	virtual void SetGroupColor(FLinearColor BaseColor, FLinearColor ActiveColor);

protected:
	bool bIsActivated = false;
	FLinearColor CachedBaseColor = FLinearColor::White;
	FLinearColor CachedActiveColor = FLinearColor::White;

	UPROPERTY(VisibleAnywhere, Category = "Mechanism")
	UStaticMeshComponent* MeshComp;

	UMaterialInstanceDynamic* DynMaterial = nullptr;

	void CreateDynamicMaterial();
	void ApplyColor(FLinearColor Color);
};
