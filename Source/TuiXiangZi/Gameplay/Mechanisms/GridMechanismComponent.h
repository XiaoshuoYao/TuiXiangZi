#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GridMechanismComponent.generated.h"

class UStaticMeshComponent;

/** Describes what editor workflow a mechanism triggers when placed. */
UENUM()
enum class EEditorPlacementFlow : uint8
{
	None           UMETA(DisplayName = "None"),
	AssignGroup    UMETA(DisplayName = "Assign Group"),
	// PairPlacement  — future: teleporter pair placement
};

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

	// ---- Group Role ----
	/** Whether this mechanism acts as a trigger in the group system (e.g. pressure plate). */
	virtual bool IsGroupTrigger() const { return false; }

	// ---- Editor Placement Flow ----
	virtual EEditorPlacementFlow GetEditorPlacementFlow() const { return EEditorPlacementFlow::None; }

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
