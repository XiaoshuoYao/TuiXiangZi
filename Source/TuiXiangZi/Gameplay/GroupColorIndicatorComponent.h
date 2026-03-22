#pragma once

#include "CoreMinimal.h"
#include "Components/StaticMeshComponent.h"
#include "GroupColorIndicatorComponent.generated.h"

/**
 * StaticMeshComponent that displays the group color on a mechanism's visual actor.
 *
 * Add this component to a TileActor Blueprint, assign a mesh (e.g. torus)
 * and a material with "Color" (Vector) / "Intensity" (Scalar) parameters.
 * GridManager and GridMechanismComponent will automatically feed it
 * group colors and activation state.
 *
 * Blueprint subclasses can override OnUpdateVisual for fully custom visuals.
 */
UCLASS(Blueprintable, ClassGroup = "GridTile",
	meta = (BlueprintSpawnableComponent))
class TUIXIANGZI_API UGroupColorIndicatorComponent : public UStaticMeshComponent
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;

	/** Called by GridManager when group styles are applied. */
	void SetGroupColor(FLinearColor BaseColor, FLinearColor ActiveColor);

	/** Called by GridMechanismComponent on activate/deactivate. */
	void SetActivated(bool bNewActivated);

	/** Emissive intensity when activated. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GroupColor")
	float ActiveIntensity = 5.0f;

	/** Emissive intensity when not activated. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GroupColor")
	float InactiveIntensity = 2.0f;

	UPROPERTY(BlueprintReadOnly, Category = "GroupColor")
	FLinearColor CachedBaseColor = FLinearColor::White;

	UPROPERTY(BlueprintReadOnly, Category = "GroupColor")
	FLinearColor CachedActiveColor = FLinearColor::White;

	UPROPERTY(BlueprintReadOnly, Category = "GroupColor")
	bool bIsActivated = false;

protected:
	bool bColorPending = false;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> DynMaterial;

	/**
	 * Blueprint override point: update your custom visual here.
	 * Default implementation sets "Color" and "Intensity" on this mesh's dynamic material.
	 *
	 * @param CurrentColor  The color to display (BaseColor or ActiveColor).
	 * @param bActivated    Current activation state.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "GroupColor")
	void OnUpdateVisual(FLinearColor CurrentColor, bool bActivated);
};
