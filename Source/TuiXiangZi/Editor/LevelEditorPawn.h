#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Editor/EditorBrushTypes.h"
#include "LevelEditorPawn.generated.h"

class UCameraComponent;
class AGridManager;
class UInputMappingContext;
class UInputAction;
class UEditorMainWidget;
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

    /** Move camera to look at the center of existing grid cells. */
    void FocusCameraOnGrid();

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

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
    float MoveSpeed = 800.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
    float MouseSensitivity = 0.3f;

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

    // ===== UI =====
    UPROPERTY(EditDefaultsOnly, Category = "UI")
    TSubclassOf<UEditorMainWidget> MainWidgetClass;

    UPROPERTY()
    UEditorMainWidget* MainWidget = nullptr;

    // ===== Input State =====
    bool bIsPainting = false;
    bool bIsErasing = false;
    bool bIsRotating = false;
    FVector2D LastMousePos;

    FIntPoint LastPaintedGridPos;
    bool bHasLastPaintedPos = false;

    FIntPoint LastErasedGridPos;
    bool bHasLastErasedPos = false;
    bool bEraseConfirmPending = false;

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
    void HandleRotation();
    void HandleKeyboardMovement(float DeltaTime);

    // ===== Keyboard Shortcuts =====
    void HandleKeyBrush1();
    void HandleKeyBrush2();
    void HandleKeyBrush3();
    void HandleKeyBrush4();
    void HandleKeyBrush5();
    void HandleKeyBrush6();
    void HandleKeyBrush7();
    void HandleKeyBrush8();
    void HandleKeyBrushEraser();
    void HandleShortcutNew();
    void HandleShortcutSave();
    void HandleShortcutLoad();
    void HandleShortcutTest();
    void HandleShortcutEsc();

    /** Helper: attempt to set brush via MainWidget, respecting dialog/mode guards. */
    void TrySetBrush(EEditorBrush Brush);
};
