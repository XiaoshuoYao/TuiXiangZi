#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GridMechanismComponent.generated.h"

class UStaticMeshComponent;

UCLASS(Abstract, Blueprintable, ClassGroup = "GridMechanism",
	meta = (BlueprintSpawnableComponent))
class TUIXIANGZI_API UGridMechanismComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mechanism")
	FIntPoint GridPos;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mechanism")
	int32 GroupId = -1;

	// ---- Activation interface ----
	virtual void OnActivate();
	virtual void OnDeactivate();
	bool IsActivated() const { return bIsActivated; }

	// ---- Passability ----
	virtual bool BlocksPassage() const { return false; }
	virtual bool IsCurrentlyBlocking() const { return false; }

	// ---- Color system ----
	virtual void SetGroupColor(FLinearColor BaseColor, FLinearColor ActiveColor);

	FLinearColor CachedBaseColor = FLinearColor::White;
	FLinearColor CachedActiveColor = FLinearColor::White;

protected:
	bool bIsActivated = false;
	UMaterialInstanceDynamic* DynMaterial = nullptr;

	void CreateDynamicMaterial();
	void ApplyColor(FLinearColor Color);
	UStaticMeshComponent* FindOwnerMeshComp() const;
};
