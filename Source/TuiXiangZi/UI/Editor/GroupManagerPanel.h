#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LevelData/LevelDataTypes.h"
#include "GroupManagerPanel.generated.h"

class UScrollBox;
class UTextBlock;
class UGroupEntryWidget;
class ALevelEditorGameMode;

UCLASS(Blueprintable)
class TUIXIANGZI_API UGroupManagerPanel : public UUserWidget
{
	GENERATED_BODY()

public:
	// --- Public Interface (called by MainWidget) ---
	UFUNCTION(BlueprintCallable, Category = "Editor|Group")
	void AddGroupEntry(int32 GroupId, const FMechanismGroupStyleData& Style);

	UFUNCTION(BlueprintCallable, Category = "Editor|Group")
	void RemoveGroupEntry(int32 GroupId);

	UFUNCTION(BlueprintCallable, Category = "Editor|Group")
	void RefreshActiveGroup(int32 ActiveGroupId);

	UFUNCTION(BlueprintCallable, Category = "Editor|Group")
	void UpdateGroupColor(int32 GroupId, FLinearColor NewBaseColor);

	UFUNCTION(BlueprintCallable, Category = "Editor|Group")
	void ClearAll();

	UFUNCTION(BlueprintCallable, Category = "Editor|Group")
	void RebuildFromGameMode(ALevelEditorGameMode* GameMode);

	UFUNCTION(BlueprintCallable, Category = "Editor|Group")
	void SetPlacementMode(bool bIsPlacing, int32 PlacingGroupId = 0);

	/** Show direction toggle on a group entry (for teleporter groups). */
	UFUNCTION(BlueprintCallable, Category = "Editor|Group")
	void SetGroupDirectionInfo(int32 GroupId, const FString& DirText);

	// --- Delegates (for MainWidget to bind) ---
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGroupManagerAction, int32, GroupId);

	UPROPERTY(BlueprintAssignable)
	FOnGroupManagerAction OnSelectGroup;

	UPROPERTY(BlueprintAssignable)
	FOnGroupManagerAction OnEditGroupColor;

	UPROPERTY(BlueprintAssignable)
	FOnGroupManagerAction OnDeleteGroup;

	UPROPERTY(BlueprintAssignable)
	FOnGroupManagerAction OnDirectionCycleGroup;

protected:

	UPROPERTY(meta = (BindWidget))
	UScrollBox* GroupList;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* EmptyHintText;

	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UGroupEntryWidget> GroupEntryWidgetClass;

	UPROPERTY()
	TMap<int32, UGroupEntryWidget*> EntryMap;

	int32 CurrentActiveGroupId = 0;

	virtual void NativeConstruct() override;

	UFUNCTION()
	void HandleEntryRowClicked(int32 GroupId);

	UFUNCTION()
	void HandleEntryColorEditClicked(int32 GroupId);

	UFUNCTION()
	void HandleEntryDeleteClicked(int32 GroupId);

	UFUNCTION()
	void HandleEntryDirectionCycleClicked(int32 GroupId);

	void UpdateEmptyHint();
};
