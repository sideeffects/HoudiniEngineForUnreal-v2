/*
* Copyright (c) <2018> Side Effects Software Inc.
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* 1. Redistributions of source code must retain the above copyright notice,
*    this list of conditions and the following disclaimer.
*
* 2. The name of Side Effects Software may not be used to endorse or
*    promote products derived from this software without specific prior
*    written permission.
*
* THIS SOFTWARE IS PROVIDED BY SIDE EFFECTS SOFTWARE "AS IS" AND ANY EXPRESS
* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
* OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
* NO EVENT SHALL SIDE EFFECTS SOFTWARE BE LIABLE FOR ANY DIRECT, INDIRECT,
* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
* OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
* LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
* NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
* EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#include "CoreMinimal.h"
#include "HoudiniEngineDetails.h"
#include "HoudiniAssetComponent.h"
#include "HoudiniAssetActor.h"
#include "HoudiniAsset.h"
#include "HoudiniParameter.h"
#include "HoudiniEngineEditorPrivatePCH.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniEngineRuntime.h"
#include "HoudiniEngineBakeUtils.h"
#include "HoudiniPackageParams.h"
#include "HoudiniEngineEditor.h"

#include "DetailCategoryBuilder.h"
#include "IDetailGroup.h"
#include "DetailWidgetRow.h"
#include "Widgets/Images/SImage.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Brushes/SlateImageBrush.h"
#include "Widgets/Input/SComboBox.h"

#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "ActorPickerMode.h"
#include "SceneOutlinerModule.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/IMainFrameModule.h"

#include "AssetThumbnail.h"
#include "DetailLayoutBuilder.h"
#include "SAssetDropTarget.h"
#include "PropertyCustomizationHelpers.h"
#include "ScopedTransaction.h"
#include "SEnumCombobox.h"

#include "HAL/FileManager.h"

#define LOCTEXT_NAMESPACE HOUDINI_LOCTEXT_NAMESPACE

#define HOUDINI_ENGINE_UI_SECTION_GENERATE												1
#define HOUDINI_ENGINE_UI_SECTION_BAKE													2
#define HOUDINI_ENGINE_UI_SECTION_ASSET_OPTIONS											3
#define HOUDINI_ENGINE_UI_SECTION_HELP_AND_DEBUG										4	

#define HOUDINI_ENGINE_UI_BUTTON_WIDTH											   135.0f

#define HOUDINI_ENGINE_UI_SECTION_GENERATE_HEADER_TEXT							   "Generate"
#define HOUDINI_ENGINE_UI_SECTION_BAKE_HEADER_TEXT							       "Bake"
#define HOUDINI_ENGINE_UI_SECTION_ASSET_OPTIONS_HEADER_TEXT						   "Asset Options"
#define HOUDINI_ENGINE_UI_SECTION_HELP_AND_DEBUG_HEADER_TEXT					   "Help and Debug"


void
SHoudiniAssetLogWidget::Construct(const FArguments & InArgs)
{
	this->ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FEditorStyle::GetBrush(TEXT("Menu.Background")))
		.Content()
		[
			SNew(SScrollBox)
			+ SScrollBox::Slot()
			[
				SNew(SMultiLineEditableTextBox)
				.Text(FText::FromString(InArgs._LogText))
				.AutoWrapText(true)
				.IsReadOnly(true)
			]
		]
	];
}


void 
FHoudiniEngineDetails::CreateWidget(
	IDetailCategoryBuilder& HoudiniEngineCategoryBuilder,
	TArray<UHoudiniAssetComponent*>& InHACs) 
{
	if (InHACs.Num() <= 0)
		return;

	UHoudiniAssetComponent* MainHAC = InHACs[0];

	if (!MainHAC || MainHAC->IsPendingKill())
		return;

	// 0. Houdini Engine Icon
	FHoudiniEngineDetails::CreateHoudiniEngineIconWidget(HoudiniEngineCategoryBuilder, InHACs);
	
	// 1. Create Generate Category
	FHoudiniEngineDetails::CreateGenerateWidgets(HoudiniEngineCategoryBuilder, InHACs);
	
	// 2. Create Bake Category
	FHoudiniEngineDetails::CreateBakeWidgets(HoudiniEngineCategoryBuilder, InHACs);
	
	// 3. Create Asset Options Category
	FHoudiniEngineDetails::CreateAssetOptionsWidgets(HoudiniEngineCategoryBuilder, InHACs);

	// 4. Create Help and Debug Category
	FHoudiniEngineDetails::CreateHelpAndDebugWidgets(HoudiniEngineCategoryBuilder, InHACs);
	
}

void 
FHoudiniEngineDetails::CreateHoudiniEngineIconWidget(
	IDetailCategoryBuilder& HoudiniEngineCategoryBuilder,
	TArray<UHoudiniAssetComponent*>& InHACs) 
{
	if (InHACs.Num() <= 0)
		return;

	UHoudiniAssetComponent* MainHAC = InHACs[0];

	if (!MainHAC || MainHAC->IsPendingKill())
		return;

	// Skip drawing the icon if the icon image is not loaded correctly.
	TSharedPtr<FSlateDynamicImageBrush> HoudiniEngineUIIconBrush = FHoudiniEngineEditor::Get().GetHoudiniEngineUIIconBrush();
	if (!HoudiniEngineUIIconBrush.IsValid())
		return;

	FDetailWidgetRow & Row = HoudiniEngineCategoryBuilder.AddCustomRow(FText::GetEmpty());
	TSharedRef<SHorizontalBox> Box = SNew(SHorizontalBox);
	TSharedPtr<SImage> Image;
	
	Box->AddSlot()
	.AutoWidth()
	.Padding(0.0f, 5.0f, 5.0f, 10.0f)
	.HAlign(HAlign_Left)
	[
		SNew(SBox)
		.HeightOverride(30)
		.WidthOverride(208)
		[
			SAssignNew(Image, SImage)
			.ColorAndOpacity(FSlateColor::UseForeground())
		]
	];
	
	Image->SetImage(
		TAttribute<const FSlateBrush*>::Create(
			TAttribute<const FSlateBrush*>::FGetter::CreateLambda([HoudiniEngineUIIconBrush]() {
		return HoudiniEngineUIIconBrush.Get();
	})));

	Row.WholeRowWidget.Widget = Box;
	Row.IsEnabled(false);
}

void 
FHoudiniEngineDetails::CreateGenerateWidgets(
	IDetailCategoryBuilder& HoudiniEngineCategoryBuilder,
	TArray<UHoudiniAssetComponent*>& InHACs)
{
	if (InHACs.Num() <= 0)
		return;

	UHoudiniAssetComponent* MainHAC = InHACs[0];

	if (!MainHAC || MainHAC->IsPendingKill())
		return;

	auto OnReBuildClickedLambda = [InHACs]()
	{
		for (auto& NextHAC : InHACs)
		{
			if (!NextHAC || NextHAC->IsPendingKill())
				continue;

			NextHAC->MarkAsNeedRebuild();
		}

		return FReply::Handled();
	};

	auto OnRecookClickedLambda = [InHACs]()
	{
		for (auto& NextHAC : InHACs)
		{
			if (!NextHAC || NextHAC->IsPendingKill())
				continue;

			NextHAC->MarkAsNeedCook();
		}

		return FReply::Handled();
	};

	auto ShouldEnableResetParametersButtonLambda = [InHACs]() 
	{
		for (auto& NextHAC : InHACs)
		{
			if (!NextHAC || NextHAC->IsPendingKill())
				continue;

			// Reset parameters to default values?
			for (int32 n = 0; n < NextHAC->GetNumParameters(); ++n)
			{
				UHoudiniParameter* NextParm = NextHAC->GetParameterAt(n);

				if (NextParm && !NextParm->IsDefault())
					return true;
			}
		}

		return false;
	};

	auto OnResetParametersClickedLambda = [InHACs]()
	{
		for (auto& NextHAC : InHACs)
		{
			if (!NextHAC || NextHAC->IsPendingKill())
				continue;

			// Reset parameters to default values?
			for (int32 n = 0; n < NextHAC->GetNumParameters(); ++n)
			{
				UHoudiniParameter* NextParm = NextHAC->GetParameterAt(n);

				if (NextParm && !NextParm->IsDefault())
				{
					NextParm->RevertToDefault();
				}
			}
		}

		return FReply::Handled();
	};

	auto OnCookFolderTextCommittedLambda = [InHACs, MainHAC](const FText& Val, ETextCommit::Type TextCommitType)
	{
		if (!MainHAC || MainHAC->IsPendingKill())
			return;

		FString NewPathStr = Val.ToString();
		
		if (NewPathStr.IsEmpty())
			return;

		if (NewPathStr.StartsWith("Game/")) 
		{
			NewPathStr = "/" + NewPathStr;
		}

		FString AbsolutePath;
		if (NewPathStr.StartsWith("/Game/")) 
		{
			FString RelativePath = FPaths::ProjectContentDir() + NewPathStr.Mid(6, NewPathStr.Len() - 6);
			AbsolutePath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*RelativePath);
		}
		else 
		{
			AbsolutePath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*NewPathStr);
		}

		if (!FPaths::DirectoryExists(AbsolutePath)) 
		{
			HOUDINI_LOG_WARNING(TEXT("Invalid path"));

			FHoudiniEngineUtils::UpdateEditorProperties(MainHAC, true);
			return;
		}

		for (auto& NextHAC : InHACs)
		{
			if (!NextHAC || NextHAC->IsPendingKill())
				continue;

			if (NextHAC->TemporaryCookFolder.Path.Equals(NewPathStr))
				continue;

			NextHAC->TemporaryCookFolder.Path = NewPathStr;
		}
	};

	FHoudiniEngineDetails::AddHeaderRowForHoudiniAssetComponent(HoudiniEngineCategoryBuilder, MainHAC, HOUDINI_ENGINE_UI_SECTION_GENERATE);
	
	// Button Row (draw only if expanded)
	if (!MainHAC->bGenerateMenuExpanded) 
		return;
	
	TSharedPtr<FSlateDynamicImageBrush> HoudiniEngineUIRebuildIconBrush = FHoudiniEngineEditor::Get().GetHoudiniEngineUIRebuildIconBrush();
	TSharedPtr<FSlateDynamicImageBrush> HoudiniEngineUIRecookIconBrush = FHoudiniEngineEditor::Get().GetHoudiniEngineUIRecookIconBrush();
	TSharedPtr<FSlateDynamicImageBrush> HoudiniEngineUIResetParametersIconBrush = FHoudiniEngineEditor::Get().GetHoudiniEngineUIResetParametersIconBrush();

	FDetailWidgetRow & ButtonRow = HoudiniEngineCategoryBuilder.AddCustomRow(FText::GetEmpty());
	TSharedRef<SHorizontalBox> ButtonHorizontalBox = SNew(SHorizontalBox);

	// Rebuild button
	TSharedPtr<SButton> RebuildButton;
	TSharedPtr<SHorizontalBox> RebuildButtonHorizontalBox;
	ButtonHorizontalBox->AddSlot()
	.MaxWidth(HOUDINI_ENGINE_UI_BUTTON_WIDTH)
	.Padding(15.0f, 0.0f, 0.0f, 2.0f)
	[
		SNew(SBox)
		.WidthOverride(HOUDINI_ENGINE_UI_BUTTON_WIDTH)
		[
			SAssignNew(RebuildButton, SButton)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			.ToolTipText(LOCTEXT("HoudiniAssetDetailsRebuildAssetButton", "Rebuild the selected Houdini Asset: its source .HDA file is reimported and updated, the asset's nodes in Houdini are destroyed and recreated, and the asset is then forced to recook."))
			//.Text(FText::FromString("Rebuild"))
			.Visibility(EVisibility::Visible)
			.Content()
			[
				SAssignNew(RebuildButtonHorizontalBox, SHorizontalBox)
			]
			.OnClicked_Lambda(OnReBuildClickedLambda)
		]
	];

	if (HoudiniEngineUIRebuildIconBrush.IsValid())
	{
		TSharedPtr<SImage> RebuildImage;
		RebuildButtonHorizontalBox->AddSlot()
		.Padding(25.0f, 0.0f, 3.0f, 0.0f)
		.MaxWidth(10.0f)
		[
			SNew(SBox)
			.WidthOverride(10.0f)
			.HeightOverride(10.0f)
			[
				SAssignNew(RebuildImage, SImage)
				//.ColorAndOpacity(FSlateColor::UseForeground())
			]
		];

		RebuildImage->SetImage(
			TAttribute<const FSlateBrush*>::Create(
				TAttribute<const FSlateBrush*>::FGetter::CreateLambda([HoudiniEngineUIRebuildIconBrush]() {
			return HoudiniEngineUIRebuildIconBrush.Get();
		})));
	}

	RebuildButtonHorizontalBox->AddSlot()
	.Padding(5.0, 0.0, 0.0, 0.0)
	.MaxWidth(HOUDINI_ENGINE_UI_BUTTON_WIDTH)
	[
		SNew(SBox)
		.WidthOverride(HOUDINI_ENGINE_UI_BUTTON_WIDTH)
		[
			SNew(STextBlock)
			.Text(FText::FromString("Rebuild"))
		]
	];

	ButtonRow.WholeRowWidget.Widget = ButtonHorizontalBox;
	ButtonRow.IsEnabled(false);
	
	// Recook button
	TSharedPtr<SButton> RecookButton;
	TSharedPtr<SHorizontalBox> RecookButtonHorizontalBox;
	ButtonHorizontalBox->AddSlot()
	.MaxWidth(HOUDINI_ENGINE_UI_BUTTON_WIDTH)
	.Padding(2.0f, 0.0f, 0.0f, 2.0f)
	[
		SNew(SBox)
		.WidthOverride(HOUDINI_ENGINE_UI_BUTTON_WIDTH)
		[
			SAssignNew(RecookButton, SButton)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			.ToolTipText(LOCTEXT("HoudiniAssetDetailsRecookAssetButton", "Recook the selected Houdini Asset: all parameters and inputs are re-upload to Houdini and the asset is then forced to recook."))
			//.Text(FText::FromString("Recook"))
			.Visibility(EVisibility::Visible)
			.OnClicked_Lambda(OnRecookClickedLambda)
			.Content()
			[
				SAssignNew(RecookButtonHorizontalBox, SHorizontalBox)
			]
		]
	];

	if (HoudiniEngineUIRecookIconBrush.IsValid())
	{
		TSharedPtr<SImage> RecookImage;
		RecookButtonHorizontalBox->AddSlot()
		.MaxWidth(10.0f)
		.Padding(23.0f, 0.0f, 3.0f, 0.0f)
		[
			SNew(SBox)
			.WidthOverride(10.0f)
			.HeightOverride(10.0f)
			[
				SAssignNew(RecookImage, SImage)
				//.ColorAndOpacity(FSlateColor::UseForeground())
			]
		];

		RecookImage->SetImage(
			TAttribute<const FSlateBrush*>::Create(
				TAttribute<const FSlateBrush*>::FGetter::CreateLambda([HoudiniEngineUIRecookIconBrush]() {
			return HoudiniEngineUIRecookIconBrush.Get();
		})));
	}

	RecookButtonHorizontalBox->AddSlot()
	.Padding(5.0, 0.0, 0.0, 0.0)
	.MaxWidth(HOUDINI_ENGINE_UI_BUTTON_WIDTH)
	[
		SNew(SBox)
		.WidthOverride(HOUDINI_ENGINE_UI_BUTTON_WIDTH)
		[
			SNew(STextBlock)
			.Text(FText::FromString("Recook"))
		]
	];

	// Reset Parameters button
	TSharedPtr<SButton> ResetParametersButton;
	TSharedPtr<SHorizontalBox> ResetParametersButtonHorizontalBox;
	ButtonHorizontalBox->AddSlot()
	.MaxWidth(HOUDINI_ENGINE_UI_BUTTON_WIDTH)
	.Padding(2.0f, 0.0f, 0.0f, 2.0f)
	[
		SNew(SBox)
		.WidthOverride(HOUDINI_ENGINE_UI_BUTTON_WIDTH)
		[
			SAssignNew(ResetParametersButton, SButton)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			.ToolTipText(LOCTEXT("HoudiniAssetDetailsResetParametersAssetButton", "Reset the selected Houdini Asset's parameters to their default values."))
			//.Text(FText::FromString("Reset Parameters"))
			.IsEnabled_Lambda(ShouldEnableResetParametersButtonLambda)
			.Visibility(EVisibility::Visible)
			.OnClicked_Lambda(OnResetParametersClickedLambda)
			.Content()
			[
				SAssignNew(ResetParametersButtonHorizontalBox, SHorizontalBox)
			]
		]
	];

	if (HoudiniEngineUIResetParametersIconBrush.IsValid())
	{
		TSharedPtr<SImage> ResetParametersImage;
		ResetParametersButtonHorizontalBox->AddSlot()
		.MaxWidth(10.0f)
		.Padding(0.0f, 0.0f, 3.0f, 0.0f)
		[
			SNew(SBox)
			.WidthOverride(10.0f)
			.HeightOverride(10.0f)
			[
				SAssignNew(ResetParametersImage, SImage)
				//.ColorAndOpacity(FSlateColor::UseForeground())
			]
		];

		ResetParametersImage->SetImage(
			TAttribute<const FSlateBrush*>::Create(
				TAttribute<const FSlateBrush*>::FGetter::CreateLambda([HoudiniEngineUIResetParametersIconBrush]() {
			return HoudiniEngineUIResetParametersIconBrush.Get();
		})));
	}

	ResetParametersButtonHorizontalBox->AddSlot()
	.Padding(5.0, 0.0, 0.0, 0.0)
	.FillWidth(4.2f)
	[
		SNew(STextBlock)
		.MinDesiredWidth(160.f)
		.Text(FText::FromString("Reset Parameters"))
	];


	// Temp Cook Folder Row
	FDetailWidgetRow & TempCookFolderRow = HoudiniEngineCategoryBuilder.AddCustomRow(FText::GetEmpty());

	TSharedRef<SHorizontalBox> TempCookFolderRowHorizontalBox = SNew(SHorizontalBox);

	TempCookFolderRowHorizontalBox->AddSlot()
	.Padding(30.0f, 0.0f, 6.0f, 0.0f)
	.MaxWidth(155.0f)
	[
		SNew(SBox)
		.WidthOverride(155.0f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("HoudiniEngineTemporaryCookFolderLabel", "Temporary Cook Folder"))
			.ToolTipText(LOCTEXT("HoudiniEngineTemporaryCookFolderTooltip", "Default folder used to store the temporary files (Static Meshes, MAterials, Textures..) that are generated by Houdini Assets when they cook."))
		]
	];

	TempCookFolderRowHorizontalBox->AddSlot()
	.MaxWidth(235.0f)
	[
		SNew(SBox)
		.WidthOverride(235.0f)
		[
			SNew(SEditableTextBox)
			.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
			.ToolTipText(LOCTEXT("HoudiniEngineTemporaryCookFolderTooltip", "Default folder used to store the temporary files (Static Meshes, Materials, Textures..) that are generated by Houdini Assets when they cook."))
			.HintText(LOCTEXT("HoudiniEngineTempCookFolderHintText", "Input to set temporary cook folder"))
			.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			.Text(FText::FromString(MainHAC->TemporaryCookFolder.Path))
			.OnTextCommitted_Lambda(OnCookFolderTextCommittedLambda)
		]
	];
	
	TempCookFolderRow.WholeRowWidget.Widget = TempCookFolderRowHorizontalBox;
}

void 
FHoudiniEngineDetails::CreateBakeWidgets(
	IDetailCategoryBuilder& HoudiniEngineCategoryBuilder,
	TArray<UHoudiniAssetComponent*>& InHACs) 
{
	if (InHACs.Num() <= 0)
		return;

	UHoudiniAssetComponent * MainHAC = InHACs[0];
	if (!MainHAC || MainHAC->IsPendingKill())
		return;

	FHoudiniEngineDetails::AddHeaderRowForHoudiniAssetComponent(HoudiniEngineCategoryBuilder, MainHAC, HOUDINI_ENGINE_UI_SECTION_BAKE);

	if (!MainHAC->bBakeMenuExpanded)
		return;

	auto OnBakeButtonClickedLambda = [InHACs, MainHAC]() 
	{
		for (auto & NextHAC : InHACs)
		{
			if (!NextHAC || NextHAC->IsPendingKill())
				continue;

			FHoudiniEngineBakeUtils::BakeHoudiniAssetComponent(NextHAC, MainHAC->bIsReplace, MainHAC->HoudiniEngineBakeOption);
		}

		return FReply::Handled();	
	};

	auto OnBakeFolderTextCommittedLambda = [InHACs, MainHAC](const FText& Val, ETextCommit::Type TextCommitType)
	{
		if (!MainHAC || MainHAC->IsPendingKill())
			return;

		FString NewPathStr = Val.ToString();

		if (NewPathStr.IsEmpty())
			return;

		if (NewPathStr.StartsWith("Game/"))
		{
			NewPathStr = "/" + NewPathStr;
		}

		FString AbsolutePath;
		if (NewPathStr.StartsWith("/Game/"))
		{
			FString RelativePath = FPaths::ProjectContentDir() + NewPathStr.Mid(6, NewPathStr.Len() - 6);
			AbsolutePath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*RelativePath);
		}
		else
		{
			AbsolutePath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*NewPathStr);
		}

		if (!FPaths::DirectoryExists(AbsolutePath))
		{
			HOUDINI_LOG_WARNING(TEXT("Invalid path"));

			FHoudiniEngineUtils::UpdateEditorProperties(MainHAC, true);
			return;
		}


		for (auto& NextHAC : InHACs)
		{
			if (!NextHAC || NextHAC->IsPendingKill())
				continue;

			if (NextHAC->BakeFolder.Path.Equals(NewPathStr))
				continue;

			NextHAC->BakeFolder.Path = NewPathStr;
		}
	};

	// Button Row
	FDetailWidgetRow & ButtonRow = HoudiniEngineCategoryBuilder.AddCustomRow(FText::GetEmpty());

	TSharedRef<SHorizontalBox> ButtonRowHorizontalBox = SNew(SHorizontalBox);

	// Bake Button
	TSharedPtr<SButton> BakeButton;
	ButtonRowHorizontalBox->AddSlot()
	/*.AutoWidth()*/
	.Padding(15.f, 0.0f, 0.0f, 0.0f)
	.MaxWidth(65.0f)
	[
		SNew(SBox).WidthOverride(65.0f)
		[
			SAssignNew(BakeButton, SButton)
			.Text(FText::FromString("Bake"))
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			.ToolTipText(LOCTEXT("HoudiniAssetDetailsBakeButton", "Bake the Houdini Asset Component(s)"))
			.Visibility(EVisibility::Visible)
			.OnClicked_Lambda(OnBakeButtonClickedLambda)
		]
	];
	
	// bake Type ComboBox
	TSharedPtr<SComboBox<TSharedPtr<FString>>> TypeComboBox;

	TArray<TSharedPtr<FString>>* OptionSource = FHoudiniEngineEditor::Get().GetHoudiniEngineBakeTypeOptionsLabels();
	TSharedPtr<FString> IntialSelec;
	if (OptionSource) 
	{
		IntialSelec = (*OptionSource)[(int)MainHAC->HoudiniEngineBakeOption];
	}

	ButtonRowHorizontalBox->AddSlot()
	/*.AutoWidth()*/
	.Padding(3.0, 0.0, 4.0f, 0.0f)
	.MaxWidth(103.f)
	[
		SNew(SBox)
		.WidthOverride(103.f)
		[
			SAssignNew(TypeComboBox, SComboBox<TSharedPtr<FString>>)
			.OptionsSource(OptionSource)
			.InitiallySelectedItem(IntialSelec)
			.OnGenerateWidget_Lambda(
				[](TSharedPtr< FString > InItem)
			{
				FText ChoiceEntryText = FText::FromString(*InItem);
				return SNew(STextBlock)
						.Text(ChoiceEntryText)
						.ToolTipText(ChoiceEntryText)
						.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")));
			})
			.OnSelectionChanged_Lambda(
				[MainHAC, InHACs](TSharedPtr< FString > NewChoice, ESelectInfo::Type SelectType)
			{
				if (!NewChoice.IsValid())
					return;

				const EHoudiniEngineBakeOption NewOption = 
					FHoudiniEngineEditor::Get().StringToHoudiniEngineBakeOption(*NewChoice.Get());

				for (auto & NextHAC : InHACs) 
				{
					if (!NextHAC || NextHAC->IsPendingKill())
						continue;

					MainHAC->HoudiniEngineBakeOption = NewOption;
				}

				FHoudiniEngineUtils::UpdateEditorProperties(MainHAC, true);
			})
			[
				SNew(STextBlock)
				.Text_Lambda([MainHAC]() 
				{ 
					return FText::FromString(
						FHoudiniEngineEditor::Get().GetStringFromHoudiniEngineBakeOption(MainHAC->HoudiniEngineBakeOption));
				})
				.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			]
		]
	];
	
	// Replace Checkbox
	TSharedPtr<SCheckBox> CheckBoxReplace;
	ButtonRowHorizontalBox->AddSlot()
	/*.AutoWidth()*/
	.MaxWidth(275.f)
	[
		SNew(SBox)
		.WidthOverride(275.f)
		[
			SAssignNew(CheckBoxReplace, SCheckBox)
			.Content()
			[
				SNew(STextBlock).Text(LOCTEXT("HoudiniEngineUIReplaceCheckBox", "Replace"))
				.ToolTipText(LOCTEXT("HoudiniEngineUIReplaceCheckBoxToolTip", "Upon baking, this Houdini Asset Actor will be destroyed and replaced by the baked object(s)."))
				.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			]
			.IsChecked_Lambda([MainHAC]()
			{
				return MainHAC->bIsReplace ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			})
			.OnCheckStateChanged_Lambda([MainHAC, InHACs](ECheckBoxState NewState)
			{
				bool bNewState = (NewState == ECheckBoxState::Checked);

				for (auto & NextHAC : InHACs) 
				{
					if (!NextHAC || NextHAC->IsPendingKill())
						continue;

					MainHAC->bIsReplace = bNewState;
				}

				FHoudiniEngineUtils::UpdateEditorProperties(MainHAC, true);
			})
		]
	];

	ButtonRow.WholeRowWidget.Widget = ButtonRowHorizontalBox;

	// Bake Folder Row
	FDetailWidgetRow & BakeFolderRow = HoudiniEngineCategoryBuilder.AddCustomRow(FText::GetEmpty());

	TSharedRef<SHorizontalBox> BakeFolderRowHorizontalBox = SNew(SHorizontalBox);

	BakeFolderRowHorizontalBox->AddSlot()
	/*.AutoWidth()*/
	.Padding(30.0f, 0.0f, 6.0f, 0.0f)
	.MaxWidth(155.0f)
	[
		SNew(SBox)
		.WidthOverride(155.0f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("HoudiniEngineBakeFolderLabel", "Bake Folder"))
			.ToolTipText(LOCTEXT("HoudiniEngineBakeFolderTooltip", "Default folder used to store the objects that are generated by this Houdini Asset when baking."))
		]
	];

	BakeFolderRowHorizontalBox->AddSlot()
	/*.AutoWidth()*/
	.MaxWidth(235.0)
	[
		SNew(SBox)
		.WidthOverride(235.0f)
		[
			SNew(SEditableTextBox)
			.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
			.ToolTipText(LOCTEXT("HoudiniEngineBakeFolderTooltip", "Default folder used to store the objects that are generated by this Houdini Asset when baking."))
			.HintText(LOCTEXT("HoudiniEngineBakeFolderHintText", "Input to set bake folder"))
			.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			.Text(FText::FromString(MainHAC->BakeFolder.Path))
			.OnTextCommitted_Lambda(OnBakeFolderTextCommittedLambda)
		]
	];

	BakeFolderRow.WholeRowWidget.Widget = BakeFolderRowHorizontalBox;

	switch (MainHAC->HoudiniEngineBakeOption) 
	{
		case EHoudiniEngineBakeOption::ToActor:
		{
			if (MainHAC->bIsReplace) 
			{
				BakeButton->SetToolTipText(LOCTEXT("HoudiniEngineBakeButtonReplaceWithActorToolTip", 
					"Replace this Houdini Asset Actor with native Unreal Actors."));
			}
			else 
			{
				BakeButton->SetToolTipText(LOCTEXT("HoudiniEngineBakeButtonBakeToActorToolTip", 
					"Bake this Houdini Asset Actor and its components to native unreal actors and components."));
			}
		}
		break;

		case EHoudiniEngineBakeOption::ToBlueprint:
		{
			if (MainHAC->bIsReplace)
			{
				BakeButton->SetToolTipText(LOCTEXT("HoudiniEngineBakeButtonReplaceWithBlueprintToolTip",
					"Bake this Houdini Asset Actor to a blueprint, and replace it with an instance of the baked blueprint."));
			}
			else
			{
				BakeButton->SetToolTipText(LOCTEXT("HoudiniEngineBakeButtonBakeToBlueprintToolTip",
					"Bake this Houdini Asset Actor to a blueprint."));
			}
		}
		break;

		case EHoudiniEngineBakeOption::ToFoliage:
		{
			if (!FHoudiniEngineBakeUtils::CanHoudiniAssetComponentBakeToFoliage(MainHAC))
			{
				// If the HAC does not have instanced output, disable Bake to Foliage
				BakeButton->SetEnabled(false);
				BakeButton->SetToolTipText(LOCTEXT("HoudiniEngineBakeButtonNoInstancedOutputToolTip",
					"The Houdini Asset must be outputing at least one instancer in order to be able to bake to Foliage."));
			}
			else 
			{
				if (MainHAC->bIsReplace)
				{
					BakeButton->SetToolTipText(LOCTEXT("HoudiniEngineBakeButtonReplaceWithFoliageToolTip",
						"Replace this Houdini Asset Actor's instancers with Foliage instances."));
				}
				else
				{
					BakeButton->SetToolTipText(LOCTEXT("HoudiniEngineBakeButtonBakeToFoliageToolTip",
						"Add this Houdini Asset Actor's instancers to the current level's Foliage."));
				}
			}
		}
		break;

		case EHoudiniEngineBakeOption::ToWorldOutliner:
		{
			if (MainHAC->bIsReplace)
			{
				BakeButton->SetToolTipText(LOCTEXT("HoudiniEngineBakeButtonReplaceWithFoliageToolTip",
					"Not implemented."));
			}
			else
			{
				BakeButton->SetToolTipText(LOCTEXT("HoudiniEngineBakeButtonBakeToGoliageToolTip",
					"Not implemented."));
			}
		}
		break;
	}

	// Todo: remove me!
	if (MainHAC->HoudiniEngineBakeOption == EHoudiniEngineBakeOption::ToWorldOutliner)
		BakeButton->SetEnabled(false);

}

void 
FHoudiniEngineDetails::CreateAssetOptionsWidgets(
	IDetailCategoryBuilder& HoudiniEngineCategoryBuilder,
	TArray<UHoudiniAssetComponent*>& InHACs) 
{
	if (InHACs.Num() <= 0)
		return;

	UHoudiniAssetComponent * MainHAC = InHACs[0];
	if (!MainHAC || MainHAC->IsPendingKill())
		return;

	// Header Row
	FHoudiniEngineDetails::AddHeaderRowForHoudiniAssetComponent(HoudiniEngineCategoryBuilder, MainHAC, HOUDINI_ENGINE_UI_SECTION_ASSET_OPTIONS);

	if (!MainHAC->bAssetOptionMenuExpanded)
		return;

	auto IsCheckedParameterChangedLambda = [MainHAC]()
	{
		if (!MainHAC || MainHAC->IsPendingKill())
			return 	ECheckBoxState::Unchecked;

		return MainHAC->bCookOnParameterChange ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	};

	auto OnCheckStateParameterChangedLambda = [InHACs](ECheckBoxState NewState)
	{
		bool bChecked = (NewState == ECheckBoxState::Checked);
		for (auto& NextHAC : InHACs)
		{
			if (!NextHAC || NextHAC->IsPendingKill())
				continue;

			NextHAC->bCookOnParameterChange = bChecked;
		}
	};

	auto IsCheckedTransformChangeLambda = [MainHAC]()
	{
		if (!MainHAC || MainHAC->IsPendingKill())
			return 	ECheckBoxState::Unchecked;

		return MainHAC->bCookOnTransformChange ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	};

	auto OnCheckStateChangedTransformChangeLambda = [InHACs](ECheckBoxState NewState)
	{
		bool bChecked = (NewState == ECheckBoxState::Checked);
		for (auto& NextHAC : InHACs)
		{
			if (!NextHAC || NextHAC->IsPendingKill())
				continue;

			NextHAC->bCookOnTransformChange = bChecked;

			NextHAC->MarkAsNeedCook();
		}
	};

	auto IsCheckedAssetInputCookLambda = [MainHAC]()
	{
		if (!MainHAC || MainHAC->IsPendingKill())
			return 	ECheckBoxState::Unchecked;

		return MainHAC->bCookOnAssetInputCook ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	};

	auto OnCheckStateChangedAssetInputCookLambda = [InHACs](ECheckBoxState NewState)
	{
		bool bChecked = (NewState == ECheckBoxState::Checked);
		for (auto& NextHAC : InHACs)
		{
			if (!NextHAC || NextHAC->IsPendingKill())
				continue;

			NextHAC->bCookOnAssetInputCook = bChecked;
		}
	};

	auto IsCheckedPushTransformToHoudiniLambda = [MainHAC]()
	{
		if (!MainHAC || MainHAC->IsPendingKill())
			return 	ECheckBoxState::Unchecked;

		return MainHAC->bUploadTransformsToHoudiniEngine ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	};

	auto OnCheckStateChangedPushTransformToHoudiniLambda = [InHACs](ECheckBoxState NewState)
	{
		bool bChecked = (NewState == ECheckBoxState::Checked);
		for (auto& NextHAC : InHACs)
		{
			if (!NextHAC || NextHAC->IsPendingKill())
				continue;

			NextHAC->bUploadTransformsToHoudiniEngine = bChecked;

			NextHAC->MarkAsNeedCook();
		}
	};

	auto IsCheckedDoNotGenerateOutputsLambda = [MainHAC]()
	{
		if (!MainHAC || MainHAC->IsPendingKill())
			return 	ECheckBoxState::Unchecked;

		return MainHAC->bOutputless ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	};

	auto OnCheckStateChangedDoNotGenerateOutputsLambda = [InHACs](ECheckBoxState NewState)
	{
		bool bChecked = (NewState == ECheckBoxState::Checked);
		for (auto& NextHAC : InHACs)
		{
			if (!NextHAC || NextHAC->IsPendingKill())
				continue;

			NextHAC->bOutputless = bChecked;

			NextHAC->MarkAsNeedCook();
		}
	};

	auto IsCheckedOutputTemplatedGeosLambda = [MainHAC]()
	{
		if (!MainHAC || MainHAC->IsPendingKill())
			return 	ECheckBoxState::Unchecked;

		return MainHAC->bOutputTemplateGeos ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	};

	auto OnCheckStateChangedOutputTemplatedGeosLambda = [InHACs](ECheckBoxState NewState)
	{
		bool bChecked = (NewState == ECheckBoxState::Checked);
		for (auto& NextHAC : InHACs)
		{
			if (!NextHAC || NextHAC->IsPendingKill())
				continue;

			NextHAC->bOutputTemplateGeos = bChecked;

			NextHAC->MarkAsNeedCook();
		}
	};

	// Checkboxes row
	FDetailWidgetRow & CheckBoxesRow = HoudiniEngineCategoryBuilder.AddCustomRow(FText::GetEmpty());
	TSharedRef<SHorizontalBox> CheckBoxesHorizontalBox = SNew(SHorizontalBox);

	TSharedPtr<SVerticalBox> LeftColumnVerticalBox;
	TSharedPtr<SVerticalBox> RightColumnVerticalBox;

	CheckBoxesHorizontalBox->AddSlot()
	.Padding(30.0f, 5.0f, 0.0f, 0.0f)
	.MaxWidth(200.f)
	[
		SNew(SBox)
		.WidthOverride(200.f)
		[
			SAssignNew(LeftColumnVerticalBox, SVerticalBox)
		]
	];

	CheckBoxesHorizontalBox->AddSlot()
	.Padding(20.0f, 5.0f, 0.0f, 0.0f)
	.MaxWidth(200.f)
	[
		SNew(SBox)
		[
			SAssignNew(RightColumnVerticalBox, SVerticalBox)
		]
	];

	LeftColumnVerticalBox->AddSlot()
	.AutoHeight()
	.Padding(0.0f, 0.0f, 0.0f, 3.5f)
	[
		SNew(SBox)
		.WidthOverride(160.0f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("HoudiniEngineCookTriggersLabel", "Cook Triggers"))
		]
	];

	// Parameter change check box
	FText TooltipText = LOCTEXT("HoudiniEngineParameterChangeTooltip", "If enabled, modifying a parameter or input on this Houdini Asset will automatically trigger a cook of the HDA in Houdini.");
	LeftColumnVerticalBox->AddSlot()
	.AutoHeight()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(4.0f)
		[
			SNew(STextBlock)
			.MinDesiredWidth(160.f)
			.Text(LOCTEXT("HoudiniEngineParameterChangeCheckBoxLabel", "On Parameter/Input Change"))
			.ToolTipText(TooltipText)
		]
		+ SHorizontalBox::Slot()
		[
			SNew(SCheckBox)
			.OnCheckStateChanged_Lambda(OnCheckStateParameterChangedLambda)
			.IsChecked_Lambda(IsCheckedParameterChangedLambda)
			.ToolTipText(TooltipText)
		]
	];

	// Transform change check box
	TooltipText = LOCTEXT("HoudiniEngineTransformChangeTooltip", "If enabled, changing the Houdini Asset Actor's transform in Unreal will also update its HDA's node transform in Houdini, and trigger a recook of the HDA with the updated transform.");
	LeftColumnVerticalBox->AddSlot()
	.AutoHeight()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(4.0f)
		[
			SNew(STextBlock)
			.MinDesiredWidth(160.f)
			.Text(LOCTEXT("HoudiniEngineTransformChangeCheckBoxLabel", "On Transform Change"))
			.ToolTipText(TooltipText)
		]
		+ SHorizontalBox::Slot()
		[
			SNew(SCheckBox)
			.OnCheckStateChanged_Lambda(OnCheckStateChangedTransformChangeLambda)
			.IsChecked_Lambda(IsCheckedTransformChangeLambda)
			.ToolTipText(TooltipText)
		]
	];

	// Triggers Downstream cook checkbox
	TooltipText = LOCTEXT("HoudiniEngineAssetInputCookTooltip", "When enabled, this asset will automatically re-cook after one its asset input has finished cooking.");
	LeftColumnVerticalBox->AddSlot()
	.AutoHeight()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(4.0f)
		[
			SNew(STextBlock)
			.MinDesiredWidth(160.f)
			.Text(LOCTEXT("HoudiniEngineAssetInputCheckBoxLabel", "On Asset Input Cook"))
			.ToolTipText(TooltipText)
		]

		+ SHorizontalBox::Slot()
		[
			SNew(SCheckBox)
			.OnCheckStateChanged_Lambda(OnCheckStateChangedAssetInputCookLambda)
			.IsChecked_Lambda(IsCheckedAssetInputCookLambda)
			.ToolTipText(TooltipText)
		]
	];

	RightColumnVerticalBox->AddSlot()
	.AutoHeight()
	.Padding(0.0f, 0.0f, 0.0f, 3.5f)
	[
		SNew(STextBlock)
		.Text(LOCTEXT("HoudiniEngineMiscLabel", "Miscellaneous"))
	];

	// Push Transform to Houdini check box
	TooltipText = LOCTEXT("HoudiniEnginePushTransformTooltip", "If enabled, modifying this Houdini Asset Actor's transform will automatically update the HDA's node transform in Houdini.");
	RightColumnVerticalBox->AddSlot()
	.AutoHeight()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(4.0f)
		[
			SNew(STextBlock)
			.MinDesiredWidth(160.f)
			.Text(LOCTEXT("HoudiniEnginePushTransformToHoudiniCheckBoxLabel", "Push Transform to Houdini"))
			.ToolTipText(TooltipText)
		]

		+ SHorizontalBox::Slot()
		[
			SNew(SCheckBox)
			.OnCheckStateChanged_Lambda(OnCheckStateChangedPushTransformToHoudiniLambda)
			.IsChecked_Lambda(IsCheckedPushTransformToHoudiniLambda)
			.ToolTipText(TooltipText)
		]
	];

	// Do not generate output check box
	TooltipText = LOCTEXT("HoudiniEnginOutputlessTooltip", "If enabled, this Houdini Asset will cook normally but will not generate any output in Unreal. This is especially usefull when chaining multiple assets together via Asset Inputs.");
	RightColumnVerticalBox->AddSlot()
	.AutoHeight()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(4.0f)
		[
			SNew(STextBlock)
			.MinDesiredWidth(160.f)
			.Text(LOCTEXT("HoudiniEngineDoNotGenerateOutputsCheckBoxLabel", "Do Not Generate Outputs"))
			.ToolTipText(TooltipText)
		]
		+ SHorizontalBox::Slot()
		[
			SNew(SCheckBox)
			.OnCheckStateChanged_Lambda(OnCheckStateChangedDoNotGenerateOutputsLambda)
			.IsChecked_Lambda(IsCheckedDoNotGenerateOutputsLambda)
			.ToolTipText(TooltipText)
		]
	];

	// Output templated geos check box
	TooltipText = LOCTEXT("HoudiniEnginOutputTemplatesTooltip", "If enabled, Geometry nodes in the asset that have the template flag will be outputed.");
	RightColumnVerticalBox->AddSlot()
	.AutoHeight()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(4.0f)
		[
			SNew(STextBlock)
			.MinDesiredWidth(160.f)
			.Text(LOCTEXT("HoudiniEnginOutputTemplatesCheckBoxLabel", "Output Templated Geos"))
			.ToolTipText(TooltipText)
		]
		+ SHorizontalBox::Slot()
		[
			SNew(SCheckBox)
			.OnCheckStateChanged_Lambda(OnCheckStateChangedOutputTemplatedGeosLambda)
			.IsChecked_Lambda(IsCheckedOutputTemplatedGeosLambda)
			.ToolTipText(TooltipText)
		]
	];

	CheckBoxesRow.WholeRowWidget.Widget = CheckBoxesHorizontalBox;
}

void 
FHoudiniEngineDetails::CreateHelpAndDebugWidgets(
	IDetailCategoryBuilder& HoudiniEngineCategoryBuilder,
	TArray<UHoudiniAssetComponent*>& InHACs) 
{
	if (InHACs.Num() <= 0)
		return;

	UHoudiniAssetComponent * MainHAC = InHACs[0];
	if (!MainHAC || MainHAC->IsPendingKill())
		return;

	// Header Row
	FHoudiniEngineDetails::AddHeaderRowForHoudiniAssetComponent(HoudiniEngineCategoryBuilder, MainHAC, HOUDINI_ENGINE_UI_SECTION_HELP_AND_DEBUG);

	if (!MainHAC->bHelpAndDebugMenuExpanded)
		return;

	auto OnFetchCookLogButtonClickedLambda = [InHACs]()
	{
		return ShowCookLog(InHACs);
	};

	auto OnHelpButtonClickedLambda = [MainHAC]()
	{
		return ShowAssetHelp(MainHAC);
	};

	// Button Row
	FDetailWidgetRow & ButtonRow = HoudiniEngineCategoryBuilder.AddCustomRow(FText::GetEmpty());
	TSharedRef<SHorizontalBox> ButtonRowHorizontalBox = SNew(SHorizontalBox);

	// Fetch Cook Log button
	ButtonRowHorizontalBox->AddSlot()
	.Padding(15.0f, 0.0f, 0.0f, 0.0f)
	.MaxWidth(HOUDINI_ENGINE_UI_BUTTON_WIDTH)
	[
		SNew(SBox)
		.WidthOverride(HOUDINI_ENGINE_UI_BUTTON_WIDTH)
		[
			SNew(SButton)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			.ToolTipText(FText::FromString("Fetch and display all cook logs available for this Houdini Asset Actor."))
			.Text(FText::FromString("Fetch Cook Log"))
			.Visibility(EVisibility::Visible)
			.OnClicked_Lambda(OnFetchCookLogButtonClickedLambda)
		]
	];

	// Help Button
	ButtonRowHorizontalBox->AddSlot()
	.Padding(4.0, 0.0f, 0.0f, 0.0f)
	.MaxWidth(HOUDINI_ENGINE_UI_BUTTON_WIDTH)
	[
		SNew(SBox)
		.WidthOverride(HOUDINI_ENGINE_UI_BUTTON_WIDTH)
		[
			SNew(SButton)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			.ToolTipText(FText::FromString("Display this Houdini Asset Actor's HDA help."))
			.Text(FText::FromString("Asset Help"))
			.Visibility(EVisibility::Visible)
			.OnClicked_Lambda(OnHelpButtonClickedLambda)
		]
	];

	ButtonRow.WholeRowWidget.Widget = ButtonRowHorizontalBox;
}

FMenuBuilder 
FHoudiniEngineDetails::Helper_CreateHoudiniAssetPicker() 
{
	auto OnShouldFilterHoudiniAssetLambda = [](const AActor* const Actor)
	{
		if (!Actor)
			return false;

		// Only return HoudiniAssetActors, but not our HAA
		if (!Actor->IsA<AHoudiniAssetActor>())
			return false;

		return true;
	};

	auto OnActorSelected = [](AActor* Actor)
	{
		UE_LOG(LogTemp, Warning, TEXT("Actor Selected"));

		return;
	};

	FMenuBuilder MenuBuilder(true, nullptr);
	FOnShouldFilterActor ActorFilter = FOnShouldFilterActor::CreateLambda(OnShouldFilterHoudiniAssetLambda);

	MenuBuilder.BeginSection(NAME_None, LOCTEXT("HoudiniEngineDetailsAssetPicker", "Asset"));
	{
		FSceneOutlinerModule & SceneOutlinerModule =
			FModuleManager::Get().LoadModuleChecked< FSceneOutlinerModule >(TEXT("SceneOutliner"));
		SceneOutliner::FInitializationOptions InitOptions;
		{
			InitOptions.Mode = ESceneOutlinerMode::ActorPicker;
			InitOptions.Filters->AddFilterPredicate(ActorFilter);
			InitOptions.bFocusSearchBoxWhenOpened = true;
			InitOptions.bShowCreateNewFolder = false;

			// Add the gutter so we can change the selection's visibility
			InitOptions.ColumnMap.Add(SceneOutliner::FBuiltInColumnTypes::Gutter(), SceneOutliner::FColumnInfo(SceneOutliner::EColumnVisibility::Visible, 0));
			InitOptions.ColumnMap.Add(SceneOutliner::FBuiltInColumnTypes::Label(), SceneOutliner::FColumnInfo(SceneOutliner::EColumnVisibility::Visible, 10));
			InitOptions.ColumnMap.Add(SceneOutliner::FBuiltInColumnTypes::ActorInfo(), SceneOutliner::FColumnInfo(SceneOutliner::EColumnVisibility::Visible, 20));
		}

		static const FVector2D SceneOutlinerWindowSize(350.0f, 200.0f);
		TSharedRef<SWidget> MenuWidget =
			SNew(SBox)
			.WidthOverride(SceneOutlinerWindowSize.X)
			.HeightOverride(SceneOutlinerWindowSize.Y)
			[
				SNew(SBorder)
				.BorderImage(FEditorStyle::GetBrush("Menu.Background"))
				[
					SceneOutlinerModule.CreateSceneOutliner(
						InitOptions,
						FOnActorPicked::CreateLambda(OnActorSelected))
				]
			];

		MenuBuilder.AddWidget(MenuWidget, FText::GetEmpty(), true);
	}
	MenuBuilder.EndSection();

	return MenuBuilder;
}

const FSlateBrush *
FHoudiniEngineDetails::GetHoudiniAssetThumbnailBorder(TSharedPtr< SBorder > HoudiniAssetThumbnailBorder) const
{
	if (HoudiniAssetThumbnailBorder.IsValid() && HoudiniAssetThumbnailBorder->IsHovered())
		return FEditorStyle::GetBrush("PropertyEditor.AssetThumbnailLight");
	else
		return FEditorStyle::GetBrush("PropertyEditor.AssetThumbnailShadow");
}

/*
TSharedRef< SWidget >
FHoudiniEngineDetails::OnGetHoudiniAssetMenuContent(TArray<UHoudiniAssetComponent*> InHACs)
{
	TArray< const UClass * > AllowedClasses;
	AllowedClasses.Add(UHoudiniAsset::StaticClass());

	TArray< UFactory * > NewAssetFactories;

	UHoudiniAsset * HoudiniAsset = nullptr;
	if (InHACs.Num() > 0)
	{
		UHoudiniAssetComponent * HoudiniAssetComponent = InHACs[0];
		HoudiniAsset = HoudiniAssetComponent->HoudiniAsset;
	}
	
	auto OnShouldFilterHoudiniAssetLambda = [](const AActor* const Actor)
	{
		if (!Actor)
			return false;

		// Only return HoudiniAssetActors, but not our HAA
		if (!Actor->IsA<AHoudiniAssetActor>())
			return false;

		return true;
	};

	// Delegate for filtering Houdini assets.
	FOnShouldFilterAsset OnShouldFilterHoudiniAsset = FOnShouldFilterAsset::CreateLambda(OnShouldFilterHoudiniAssetLambda);
	
	return PropertyCustomizationHelpers::MakeAssetPickerWithMenu(
		FAssetData(HoudiniAsset), true,
		AllowedClasses, NewAssetFactories, OnShouldFilterHoudiniAsset,
		FOnAssetSelected::CreateLambda([](const FAssetData & AssetData) {}),
		FSimpleDelegate::CreateLambda([]() { })
		);
}
*/

FReply
FHoudiniEngineDetails::ShowCookLog(TArray<UHoudiniAssetComponent *> InHACS)
{
	TSharedPtr< SWindow > ParentWindow;
	FString CookLog = FHoudiniEngineUtils::GetCookLog(InHACS);

	// Check if the main frame is loaded. When using the old main frame it may not be.
	if (FModuleManager::Get().IsModuleLoaded("MainFrame"))
	{
		IMainFrameModule & MainFrame = FModuleManager::LoadModuleChecked<IMainFrameModule>("MainFrame");
		ParentWindow = MainFrame.GetParentWindow();
	}

	if (ParentWindow.IsValid())
	{
		TSharedPtr<SHoudiniAssetLogWidget> HoudiniAssetCookLog;

		TSharedRef<SWindow> Window =
			SNew(SWindow)
			.Title(LOCTEXT("WindowTitle", "Houdini Cook Log"))
			.ClientSize(FVector2D(640, 480));

		Window->SetContent(
			SAssignNew(HoudiniAssetCookLog, SHoudiniAssetLogWidget)
			.LogText(CookLog));

		if (FSlateApplication::IsInitialized())
			FSlateApplication::Get().AddModalWindow(Window, ParentWindow, false);
	}

	return FReply::Handled();
}

FReply
FHoudiniEngineDetails::ShowAssetHelp(UHoudiniAssetComponent * InHAC)
{
	if (!InHAC)
		return FReply::Handled();

	FString AssetHelp = FHoudiniEngineUtils::GetAssetHelp(InHAC);

	TSharedPtr< SWindow > ParentWindow;

	// Check if the main frame is loaded. When using the old main frame it may not be.
	if (FModuleManager::Get().IsModuleLoaded("MainFrame"))
	{
		IMainFrameModule& MainFrame = FModuleManager::LoadModuleChecked< IMainFrameModule >("MainFrame");
		ParentWindow = MainFrame.GetParentWindow();
	}

	if (ParentWindow.IsValid())
	{
		TSharedPtr<SHoudiniAssetLogWidget> HoudiniAssetHelpLog;

		TSharedRef<SWindow> Window =
			SNew(SWindow)
			.Title(LOCTEXT("WindowTitle", "Houdini Asset Help"))
			.ClientSize(FVector2D(640, 480));

		Window->SetContent(
			SAssignNew(HoudiniAssetHelpLog, SHoudiniAssetLogWidget)
			.LogText(AssetHelp));

		if (FSlateApplication::IsInitialized())
			FSlateApplication::Get().AddModalWindow(Window, ParentWindow, false);
	}
	return FReply::Handled();
}

void 
FHoudiniEngineDetails::AddHeaderRowForHoudiniAssetComponent(IDetailCategoryBuilder& HoudiniEngineCategoryBuilder, UHoudiniAssetComponent * HoudiniAssetComponent, int32 MenuSection)
{
	if (!HoudiniAssetComponent || HoudiniAssetComponent->IsPendingKill())
		return;

	FOnClicked OnExpanderClick = FOnClicked::CreateLambda([HoudiniAssetComponent, MenuSection]()
	{
		switch (MenuSection) 
		{
			case HOUDINI_ENGINE_UI_SECTION_GENERATE:
				HoudiniAssetComponent->bGenerateMenuExpanded = !HoudiniAssetComponent->bGenerateMenuExpanded;
			break;

			case HOUDINI_ENGINE_UI_SECTION_BAKE:
				HoudiniAssetComponent->bBakeMenuExpanded = !HoudiniAssetComponent->bBakeMenuExpanded;
			break;

			case HOUDINI_ENGINE_UI_SECTION_ASSET_OPTIONS:
				HoudiniAssetComponent->bAssetOptionMenuExpanded = !HoudiniAssetComponent->bAssetOptionMenuExpanded;
			break;

			case HOUDINI_ENGINE_UI_SECTION_HELP_AND_DEBUG:
				HoudiniAssetComponent->bHelpAndDebugMenuExpanded = !HoudiniAssetComponent->bHelpAndDebugMenuExpanded;
		}

		FHoudiniEngineUtils::UpdateEditorProperties(HoudiniAssetComponent, true);

		return FReply::Handled();
	});

	TFunction<FText(void)> GetText = [MenuSection]() 
	{
		switch (MenuSection)
		{
			case HOUDINI_ENGINE_UI_SECTION_GENERATE:
				return FText::FromString(HOUDINI_ENGINE_UI_SECTION_GENERATE_HEADER_TEXT);
			break;

			case HOUDINI_ENGINE_UI_SECTION_BAKE:
				return FText::FromString(HOUDINI_ENGINE_UI_SECTION_BAKE_HEADER_TEXT);
			break;

			case HOUDINI_ENGINE_UI_SECTION_ASSET_OPTIONS:
				return FText::FromString(HOUDINI_ENGINE_UI_SECTION_ASSET_OPTIONS_HEADER_TEXT);
			break;

			case HOUDINI_ENGINE_UI_SECTION_HELP_AND_DEBUG:
				return FText::FromString(HOUDINI_ENGINE_UI_SECTION_HELP_AND_DEBUG_HEADER_TEXT);
			break;
		}
		return FText::FromString("");
	};

	TFunction<const FSlateBrush*(SButton* InExpanderArrow)> GetExpanderBrush = [HoudiniAssetComponent, MenuSection](SButton* InExpanderArrow)
	{
		FName ResourceName;
		bool bMenuExpanded = false;

		switch (MenuSection)
		{
		case HOUDINI_ENGINE_UI_SECTION_GENERATE:
			bMenuExpanded = HoudiniAssetComponent->bGenerateMenuExpanded;
			break;

		case HOUDINI_ENGINE_UI_SECTION_BAKE:
			bMenuExpanded = HoudiniAssetComponent->bBakeMenuExpanded;
			break;

		case HOUDINI_ENGINE_UI_SECTION_ASSET_OPTIONS:
			bMenuExpanded = HoudiniAssetComponent->bAssetOptionMenuExpanded;
			break;

		case HOUDINI_ENGINE_UI_SECTION_HELP_AND_DEBUG:
			bMenuExpanded = HoudiniAssetComponent->bHelpAndDebugMenuExpanded;
		}

		if (bMenuExpanded)
		{
			ResourceName = InExpanderArrow->IsHovered() ? "TreeArrow_Expanded_Hovered" : "TreeArrow_Expanded";
		}
		else
		{
			ResourceName = InExpanderArrow->IsHovered() ? "TreeArrow_Collapsed_Hovered" : "TreeArrow_Collapsed";
		}
		
		return FEditorStyle::GetBrush(ResourceName);
	};

	return AddHeaderRow(HoudiniEngineCategoryBuilder, OnExpanderClick, GetText, GetExpanderBrush);
}

void
FHoudiniEngineDetails::AddHeaderRowForHoudiniPDGAssetLink(IDetailCategoryBuilder& PDGCategoryBuilder, UHoudiniPDGAssetLink* InPDGAssetLink, int32 MenuSection)
{
	if (!InPDGAssetLink || InPDGAssetLink->IsPendingKill())
		return;

	FOnClicked OnExpanderClick = FOnClicked::CreateLambda([InPDGAssetLink, MenuSection]()
	{
		// Record a transaction for undo/redo
		FScopedTransaction Transaction(
			TEXT(HOUDINI_MODULE_RUNTIME),
			LOCTEXT("HoudiniPDGAssetLinkParameterChange", "Houdini PDG Asset Link Parameter: Changing a value"),
			InPDGAssetLink);

		switch (MenuSection) 
		{
			case HOUDINI_ENGINE_UI_SECTION_BAKE:
				InPDGAssetLink->Modify();
				InPDGAssetLink->bBakeMenuExpanded = !InPDGAssetLink->bBakeMenuExpanded;
				InPDGAssetLink->NotifyPostEditChangeProperty(
					GET_MEMBER_NAME_STRING_CHECKED(UHoudiniPDGAssetLink, bBakeMenuExpanded));
			break;
		}

		//FHoudiniEngineUtils::UpdateEditorProperties(InPDGAssetLink, true);

		return FReply::Handled();
	});

	TFunction<FText(void)> GetText = [MenuSection]() 
	{
		switch (MenuSection)
		{
			case HOUDINI_ENGINE_UI_SECTION_BAKE:
				return FText::FromString(HOUDINI_ENGINE_UI_SECTION_BAKE_HEADER_TEXT);
			break;
		}
		return FText::FromString("");
	};

	TFunction<const FSlateBrush*(SButton* InExpanderArrow)> GetExpanderBrush = [InPDGAssetLink, MenuSection](SButton* InExpanderArrow)
	{
		FName ResourceName;
		bool bMenuExpanded = false;

		switch (MenuSection)
		{
		case HOUDINI_ENGINE_UI_SECTION_BAKE:
			bMenuExpanded = InPDGAssetLink->bBakeMenuExpanded;
			break;
		}

		if (bMenuExpanded)
		{
			ResourceName = InExpanderArrow->IsHovered() ? "TreeArrow_Expanded_Hovered" : "TreeArrow_Expanded";
		}
		else
		{
			ResourceName = InExpanderArrow->IsHovered() ? "TreeArrow_Collapsed_Hovered" : "TreeArrow_Collapsed";
		}
		
		return FEditorStyle::GetBrush(ResourceName);
	};

	return AddHeaderRow(PDGCategoryBuilder, OnExpanderClick, GetText, GetExpanderBrush);	
}

void 
FHoudiniEngineDetails::AddHeaderRow(
	IDetailCategoryBuilder& HoudiniEngineCategoryBuilder,
	FOnClicked& InOnExpanderClick,
	TFunction<FText(void)>& InGetText,
	TFunction<const FSlateBrush*(SButton* InExpanderArrow)>& InGetExpanderBrush) 
{
	// Header Row
	FDetailWidgetRow & HeaderRow = HoudiniEngineCategoryBuilder.AddCustomRow(FText::GetEmpty());
	TSharedPtr<SHorizontalBox> HeaderHorizontalBox;
	HeaderRow.WholeRowWidget.Widget = SAssignNew(HeaderHorizontalBox, SHorizontalBox);

	TSharedPtr<SImage> ExpanderImage;
	TSharedPtr<SButton> ExpanderArrow;
	HeaderHorizontalBox->AddSlot().VAlign(VAlign_Center).HAlign(HAlign_Left).AutoWidth()
	[
		SAssignNew(ExpanderArrow, SButton)
		.ButtonStyle(FEditorStyle::Get(), "NoBorder")
		.ClickMethod(EButtonClickMethod::MouseDown)
		.Visibility(EVisibility::Visible)
		.OnClicked(InOnExpanderClick)
		[
			SAssignNew(ExpanderImage, SImage)
			.ColorAndOpacity(FSlateColor::UseForeground())
		]
	];

	HeaderHorizontalBox->AddSlot().Padding(1.0f).VAlign(VAlign_Center).AutoWidth()
	[
		SNew(STextBlock)
		.Text_Lambda([InGetText](){return InGetText(); })
		.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
	];

	ExpanderImage->SetImage(
		TAttribute<const FSlateBrush*>::Create(
			[ExpanderArrow, InGetExpanderBrush]()
			{
				return InGetExpanderBrush(ExpanderArrow.Get());
			}));
}

#undef LOCTEXT_NAMESPACE