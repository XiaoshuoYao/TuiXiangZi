#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "LevelEditorPawn.generated.h"

class UCameraComponent;
class AGridManager;
class UInputMappingContext;
class UInputAction;
struct FInputActionValue;

UCLASS(Blueprintable)
class TUIXIANGZI_API ALevelEditorPawn : public APawn
{
    GENERATED_BODY()

public:
    ALevelEditorPawn();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

    // Raycast from mouse screen position to the grid plane (Z=0)
    bool RaycastToGrid(FIntPoint& OutGridPos) const;

protected:
    // ===== Camera =====
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
    UCameraComponent* EditorCamera;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
    float ZoomSpeed = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
    float MinOrthoWidth = 256.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
    float MaxOrthoWidth = 4096.0f;

    // ===== Enhanced Input =====
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    UInputMappingContext* EditorMappingContext;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    UInputAction* LeftClickAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    UInputAction* RightClickAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    UInputAction* MiddleClickAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    UInputAction* MouseWheelAction;

    // ===== Input State =====
    bool bIsPainting = false;
    bool bIsErasing = false;
    bool bIsPanning = false;
    FVector2D PanStartMousePos;
    FVector PanStartActorLocation;

    FIntPoint LastPaintedGridPos;
    bool bHasLastPaintedPos = false;

    // ===== References =====
    UPROPERTY()
    AGridManager* GridManagerRef;

    // ===== Input Handlers =====
    void OnLeftClickStarted(const FInputActionValue& Value);
    void OnLeftClickCompleted(const FInputActionValue& Value);
    void OnRightClickStarted(const FInputActionValue& Value);
    void OnRightClickCompleted(const FInputActionValue& Value);
    void OnMiddleClickStarted(const FInputActionValue& Value);
    void OnMiddleClickCompleted(const FInputActionValue& Value);
    void OnMouseWheel(const FInputActionValue& Value);

    void HandlePainting();
    void HandleErasing();
    void HandlePanning(float DeltaTime);
};
