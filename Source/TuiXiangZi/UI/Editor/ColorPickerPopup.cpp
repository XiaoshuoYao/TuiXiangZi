#include "ColorPickerPopup.h"
#include "Components/Image.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/CanvasPanelSlot.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/SlateBlueprintLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"

void UColorPickerPopup::NativeConstruct()
{
	Super::NativeConstruct();

	if (ApplyButton)
	{
		ApplyButton->OnClicked.AddDynamic(this, &UColorPickerPopup::HandleApplyClicked);
	}
	if (CancelButton)
	{
		CancelButton->OnClicked.AddDynamic(this, &UColorPickerPopup::HandleCancelClicked);
	}
	if (HexInput)
	{
		HexInput->OnTextCommitted.AddDynamic(this, &UColorPickerPopup::HandleHexCommitted);
	}

	// Create MID for SVPlane
	if (SVPlaneMaterial && SVPlane)
	{
		SVPlaneMID = UMaterialInstanceDynamic::Create(SVPlaneMaterial, this);
		if (SVPlaneMID)
		{
			FSlateBrush Brush;
			Brush.SetResourceObject(SVPlaneMID);
			Brush.ImageSize = FVector2D(220.0f, 220.0f);
			SVPlane->SetBrush(Brush);
		}
	}
}

void UColorPickerPopup::Setup(int32 InGroupId, FLinearColor CurrentBaseColor)
{
	GroupId = InGroupId;
	OriginalColor = CurrentBaseColor;

	if (OldColorPreview)
	{
		OldColorPreview->SetColorAndOpacity(CurrentBaseColor);
	}

	LinearToHSV(CurrentBaseColor, CurrentH, CurrentS, CurrentV);
	UpdateFromHSV();
	UpdateCursorPositions();
	RegenerateSVTexture();
}

FReply UColorPickerPopup::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return FReply::Unhandled();
	}

	// Check if click is on SVPlane
	FVector2D LocalPos, Size;
	if (SVPlane && GetLocalHitPosition(SVPlane, InGeometry, InMouseEvent, LocalPos, Size))
	{
		bDraggingSV = true;
		CurrentS = FMath::Clamp(LocalPos.X / Size.X, 0.0f, 1.0f);
		CurrentV = FMath::Clamp(1.0f - (LocalPos.Y / Size.Y), 0.0f, 1.0f);
		UpdateFromHSV();
		UpdateCursorPositions();
		return FReply::Handled().CaptureMouse(TakeWidget());
	}

	// Check if click is on HueBar
	if (HueBar && GetLocalHitPosition(HueBar, InGeometry, InMouseEvent, LocalPos, Size))
	{
		bDraggingHue = true;
		CurrentH = FMath::Clamp((LocalPos.Y / Size.Y) * 360.0f, 0.0f, 360.0f);
		RegenerateSVTexture();
		UpdateFromHSV();
		UpdateCursorPositions();
		return FReply::Handled().CaptureMouse(TakeWidget());
	}

	return FReply::Unhandled();
}

FReply UColorPickerPopup::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bDraggingSV && SVPlane)
	{
		FVector2D LocalPos, Size;
		if (GetLocalHitPosition(SVPlane, InGeometry, InMouseEvent, LocalPos, Size))
		{
			CurrentS = FMath::Clamp(LocalPos.X / Size.X, 0.0f, 1.0f);
			CurrentV = FMath::Clamp(1.0f - (LocalPos.Y / Size.Y), 0.0f, 1.0f);
		}
		else
		{
			// Allow dragging outside the region, but clamp
			CurrentS = FMath::Clamp(LocalPos.X / Size.X, 0.0f, 1.0f);
			CurrentV = FMath::Clamp(1.0f - (LocalPos.Y / Size.Y), 0.0f, 1.0f);
		}
		UpdateFromHSV();
		UpdateCursorPositions();
		return FReply::Handled();
	}

	if (bDraggingHue && HueBar)
	{
		FVector2D LocalPos, Size;
		GetLocalHitPosition(HueBar, InGeometry, InMouseEvent, LocalPos, Size);
		CurrentH = FMath::Clamp((LocalPos.Y / Size.Y) * 360.0f, 0.0f, 360.0f);
		RegenerateSVTexture();
		UpdateFromHSV();
		UpdateCursorPositions();
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

FReply UColorPickerPopup::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bDraggingSV || bDraggingHue)
	{
		bDraggingSV = false;
		bDraggingHue = false;
		return FReply::Handled().ReleaseMouseCapture();
	}
	return FReply::Unhandled();
}

void UColorPickerPopup::UpdateFromHSV()
{
	FLinearColor NewColor = HSVToLinear(CurrentH, CurrentS, CurrentV);

	if (NewColorPreview)
	{
		NewColorPreview->SetColorAndOpacity(NewColor);
	}

	// Update HEX display
	FColor NewFColor = NewColor.ToFColor(true);
	FString Hex = FString::Printf(TEXT("#%02X%02X%02X"), NewFColor.R, NewFColor.G, NewFColor.B);
	if (HexInput)
	{
		HexInput->SetText(FText::FromString(Hex));
	}
}

void UColorPickerPopup::UpdateCursorPositions()
{
	// Position the SV cursor within the SV plane
	if (SVCursor && SVPlane)
	{
		UCanvasPanelSlot* CursorSlot = Cast<UCanvasPanelSlot>(SVCursor->Slot);
		UCanvasPanelSlot* PlaneSlot = Cast<UCanvasPanelSlot>(SVPlane->Slot);
		if (CursorSlot && PlaneSlot)
		{
			FVector2D PlanePos = PlaneSlot->GetPosition();
			FVector2D PlaneSize = PlaneSlot->GetSize();

			float CursorX = PlanePos.X + CurrentS * PlaneSize.X;
			float CursorY = PlanePos.Y + (1.0f - CurrentV) * PlaneSize.Y;

			// Center the cursor on the point
			FVector2D CursorSize = CursorSlot->GetSize();
			CursorSlot->SetPosition(FVector2D(CursorX - CursorSize.X * 0.5f, CursorY - CursorSize.Y * 0.5f));
		}
	}

	// Position the Hue cursor on the Hue bar
	if (HueCursor && HueBar)
	{
		UCanvasPanelSlot* CursorSlot = Cast<UCanvasPanelSlot>(HueCursor->Slot);
		UCanvasPanelSlot* BarSlot = Cast<UCanvasPanelSlot>(HueBar->Slot);
		if (CursorSlot && BarSlot)
		{
			FVector2D BarPos = BarSlot->GetPosition();
			FVector2D BarSize = BarSlot->GetSize();

			float CursorY = BarPos.Y + (CurrentH / 360.0f) * BarSize.Y;

			FVector2D CursorSize = CursorSlot->GetSize();
			CursorSlot->SetPosition(FVector2D(BarPos.X, CursorY - CursorSize.Y * 0.5f));
		}
	}
}

void UColorPickerPopup::RegenerateSVTexture()
{
	if (SVPlaneMID)
	{
		SVPlaneMID->SetScalarParameterValue(TEXT("Hue"), CurrentH / 360.0f);
	}
}

FLinearColor UColorPickerPopup::HSVToLinear(float H, float S, float V) const
{
	// FLinearColor(H, S, V, A) in HSV space, then convert to RGB
	// LinearRGBToHSV produces R=H(0~360), G=S(0~1), B=V(0~1)
	// HSVToLinearRGB is the inverse
	FLinearColor HSVColor(H, S, V, 1.0f);
	return HSVColor.HSVToLinearRGB();
}

void UColorPickerPopup::LinearToHSV(FLinearColor Color, float& OutH, float& OutS, float& OutV) const
{
	// Convert to HSV using UE's FLinearColor
	FLinearColor HSV = Color.LinearRGBToHSV();
	OutH = HSV.R;  // Hue: 0~360
	OutS = HSV.G;  // Saturation: 0~1
	OutV = HSV.B;  // Value: 0~1
}

FLinearColor UColorPickerPopup::CalculateActiveColor(FLinearColor Base) const
{
	return FLinearColor(
		FMath::Min(Base.R * 1.3f, 1.0f),
		FMath::Min(Base.G * 1.3f, 1.0f),
		FMath::Min(Base.B * 1.3f, 1.0f),
		1.0f
	);
}

bool UColorPickerPopup::GetLocalHitPosition(UWidget* TargetWidget, const FGeometry& MyGeometry,
	const FPointerEvent& MouseEvent, FVector2D& OutLocal, FVector2D& OutSize) const
{
	if (!TargetWidget)
	{
		return false;
	}

	const TSharedPtr<SWidget> SafeWidget = TargetWidget->GetCachedWidget();
	if (!SafeWidget.IsValid())
	{
		return false;
	}

	const FGeometry WidgetGeometry = SafeWidget->GetCachedGeometry();
	OutSize = WidgetGeometry.GetLocalSize();

	// Convert absolute mouse position to local position within the target widget
	FVector2D AbsolutePos = MouseEvent.GetScreenSpacePosition();
	OutLocal = WidgetGeometry.AbsoluteToLocal(AbsolutePos);

	return (OutLocal.X >= 0.0f && OutLocal.X <= OutSize.X &&
		OutLocal.Y >= 0.0f && OutLocal.Y <= OutSize.Y);
}

void UColorPickerPopup::HandleHexCommitted(const FText& Text, ETextCommit::Type CommitType)
{
	FString Hex = Text.ToString();

	// Remove '#' prefix if present
	Hex.RemoveFromStart(TEXT("#"));

	if (Hex.Len() == 6)
	{
		FColor Parsed = FColor::FromHex(Hex);
		FLinearColor NewColor(Parsed);
		LinearToHSV(NewColor, CurrentH, CurrentS, CurrentV);
		RegenerateSVTexture();
		UpdateFromHSV();
		UpdateCursorPositions();
	}
	else
	{
		// Invalid hex - restore from current HSV
		UpdateFromHSV();
	}
}

void UColorPickerPopup::HandleApplyClicked()
{
	FLinearColor Base = HSVToLinear(CurrentH, CurrentS, CurrentV);
	FLinearColor Active = CalculateActiveColor(Base);
	OnColorConfirmed.Broadcast(GroupId, Base, Active);
}

void UColorPickerPopup::HandleCancelClicked()
{
	OnCancelled.Broadcast();
}
