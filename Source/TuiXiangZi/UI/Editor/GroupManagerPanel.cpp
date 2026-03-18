#include "GroupManagerPanel.h"
#include "GroupEntryWidget.h"
#include "Components/Button.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Editor/LevelEditorGameMode.h"

void UGroupManagerPanel::NativeConstruct()
{
	Super::NativeConstruct();

	if (NewGroupButton)
	{
		NewGroupButton->OnClicked.AddDynamic(this, &UGroupManagerPanel::HandleNewGroupClicked);
	}

	UpdateEmptyHint();
}

void UGroupManagerPanel::AddGroupEntry(int32 GroupId, const FMechanismGroupStyleData& Style)
{
	if (!GroupEntryWidgetClass || !GroupList)
	{
		return;
	}

	UGroupEntryWidget* Entry = CreateWidget<UGroupEntryWidget>(this, GroupEntryWidgetClass);
	if (!Entry)
	{
		return;
	}

	Entry->Setup(GroupId, FText::FromString(Style.DisplayName), Style.BaseColor);

	Entry->OnRowClicked.AddDynamic(this, &UGroupManagerPanel::HandleEntryRowClicked);
	Entry->OnColorEditClicked.AddDynamic(this, &UGroupManagerPanel::HandleEntryColorEditClicked);
	Entry->OnDeleteClicked.AddDynamic(this, &UGroupManagerPanel::HandleEntryDeleteClicked);

	GroupList->AddChild(Entry);
	EntryMap.Add(GroupId, Entry);

	UpdateEmptyHint();
}

void UGroupManagerPanel::RemoveGroupEntry(int32 GroupId)
{
	UGroupEntryWidget** FoundEntry = EntryMap.Find(GroupId);
	if (FoundEntry && *FoundEntry)
	{
		UGroupEntryWidget* Entry = *FoundEntry;
		GroupList->RemoveChild(Entry);
		Entry->RemoveFromParent();
		EntryMap.Remove(GroupId);
	}

	if (GroupId == CurrentActiveGroupId)
	{
		CurrentActiveGroupId = 0;
	}

	UpdateEmptyHint();
}

void UGroupManagerPanel::RefreshActiveGroup(int32 ActiveGroupId)
{
	CurrentActiveGroupId = ActiveGroupId;

	for (auto& Pair : EntryMap)
	{
		if (Pair.Value)
		{
			Pair.Value->SetSelected(Pair.Key == ActiveGroupId);
		}
	}
}

void UGroupManagerPanel::UpdateGroupColor(int32 GroupId, FLinearColor NewBaseColor)
{
	UGroupEntryWidget** FoundEntry = EntryMap.Find(GroupId);
	if (FoundEntry && *FoundEntry)
	{
		(*FoundEntry)->SetBaseColor(NewBaseColor);
	}
}

void UGroupManagerPanel::ClearAll()
{
	if (GroupList)
	{
		GroupList->ClearChildren();
	}
	EntryMap.Empty();
	CurrentActiveGroupId = 0;

	UpdateEmptyHint();
}

void UGroupManagerPanel::RebuildFromGameMode(ALevelEditorGameMode* GameMode)
{
	if (!GameMode)
	{
		return;
	}

	ClearAll();

	TArray<int32> Ids = GameMode->GetAllGroupIds();
	for (int32 Id : Ids)
	{
		AddGroupEntry(Id, GameMode->GetGroupStyle(Id));
	}

	RefreshActiveGroup(GameMode->GetCurrentGroupId());
}

void UGroupManagerPanel::SetPlacementMode(bool bIsPlacing, int32 PlacingGroupId)
{
	if (NewGroupButton)
	{
		NewGroupButton->SetIsEnabled(!bIsPlacing);
	}

	for (auto& Pair : EntryMap)
	{
		if (!Pair.Value)
		{
			continue;
		}

		if (bIsPlacing)
		{
			if (Pair.Key == PlacingGroupId)
			{
				// Current placing group: only ColorEdit is useful
				// We enable the entry but Row and Delete should be restricted
				// SetInteractionEnabled enables all buttons; for the placing group
				// we enable it so ColorEdit works. The Row/Delete restriction
				// is handled at the MainWidget level via delegate logic.
				Pair.Value->SetInteractionEnabled(true);
			}
			else
			{
				Pair.Value->SetInteractionEnabled(false);
			}
		}
		else
		{
			Pair.Value->SetInteractionEnabled(true);
		}
	}
}

void UGroupManagerPanel::HandleNewGroupClicked()
{
	OnRequestNewGroup.Broadcast();
}

void UGroupManagerPanel::HandleEntryRowClicked(int32 GroupId)
{
	OnSelectGroup.Broadcast(GroupId);
}

void UGroupManagerPanel::HandleEntryColorEditClicked(int32 GroupId)
{
	OnEditGroupColor.Broadcast(GroupId);
}

void UGroupManagerPanel::HandleEntryDeleteClicked(int32 GroupId)
{
	OnDeleteGroup.Broadcast(GroupId);
}

void UGroupManagerPanel::UpdateEmptyHint()
{
	if (EmptyHintText)
	{
		EmptyHintText->SetVisibility(EntryMap.Num() == 0 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}
