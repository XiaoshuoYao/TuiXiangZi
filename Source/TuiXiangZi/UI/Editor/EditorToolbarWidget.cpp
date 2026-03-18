#include "EditorToolbarWidget.h"
#include "Components/Button.h"

void UEditorToolbarWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Bind button click events
	if (NewButton)
	{
		NewButton->OnClicked.AddDynamic(this, &UEditorToolbarWidget::HandleNewClicked);
		NewButton->SetToolTipText(FText::FromString(TEXT("新建关卡 (Ctrl+N)")));
	}

	if (SaveButton)
	{
		SaveButton->OnClicked.AddDynamic(this, &UEditorToolbarWidget::HandleSaveClicked);
		SaveButton->SetToolTipText(FText::FromString(TEXT("保存关卡 (Ctrl+S)")));
	}

	if (LoadButton)
	{
		LoadButton->OnClicked.AddDynamic(this, &UEditorToolbarWidget::HandleLoadClicked);
		LoadButton->SetToolTipText(FText::FromString(TEXT("加载关卡 (Ctrl+O)")));
	}

	if (TestButton)
	{
		TestButton->OnClicked.AddDynamic(this, &UEditorToolbarWidget::HandleTestClicked);
		TestButton->SetToolTipText(FText::FromString(TEXT("测试关卡 (F5)")));
	}

	if (BackButton)
	{
		BackButton->OnClicked.AddDynamic(this, &UEditorToolbarWidget::HandleBackClicked);
		BackButton->SetToolTipText(FText::FromString(TEXT("返回主菜单 (Esc)")));
	}
}

void UEditorToolbarWidget::HandleNewClicked()
{
	OnNewClicked.Broadcast();
}

void UEditorToolbarWidget::HandleSaveClicked()
{
	OnSaveClicked.Broadcast();
}

void UEditorToolbarWidget::HandleLoadClicked()
{
	OnLoadClicked.Broadcast();
}

void UEditorToolbarWidget::HandleTestClicked()
{
	OnTestClicked.Broadcast();
}

void UEditorToolbarWidget::HandleBackClicked()
{
	OnBackClicked.Broadcast();
}
