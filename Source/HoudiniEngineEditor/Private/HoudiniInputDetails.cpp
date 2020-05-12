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

#pragma once

#include "HoudiniInputDetails.h"

#include "HoudiniInput.h"
#include "HoudiniAssetActor.h"
#include "HoudiniEngineEditor.h"
#include "HoudiniEngineEditorUtils.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniLandscapeTranslator.h"
#include "HoudiniEngineBakeUtils.h"
#include "HoudiniPackageParams.h"

#include "Editor.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "IDetailCustomization.h"
#include "DetailWidgetRow.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SRotatorInputBox.h"
#include "Widgets/Input/SVectorInputBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SEditableText.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Editor/UnrealEd/Public/AssetThumbnail.h"
#include "Editor/PropertyEditor/Private/SDetailsViewBase.h"
#include "Editor/PropertyEditor/Public/PropertyCustomizationHelpers.h"
#include "SAssetDropTarget.h"
#include "ScopedTransaction.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Selection.h"
#include "EngineUtils.h"
#include "AssetData.h"
#include "Framework/SlateDelegates.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Modules/ModuleManager.h"
#include "SceneOutlinerModule.h"

#define LOCTEXT_NAMESPACE HOUDINI_LOCTEXT_NAMESPACE

void
FHoudiniInputDetails::CreateWidget(
	IDetailCategoryBuilder& HouInputCategory,
	TArray<UHoudiniInput*> InInputs,
	FDetailWidgetRow* InputRow)
{
	if (InInputs.Num() <= 0)
		return;

	UHoudiniInput* MainInput = InInputs[0];
	if (!MainInput || MainInput->IsPendingKill())
		return;

	// Get thumbnail pool for this builder.
	TSharedPtr< FAssetThumbnailPool > AssetThumbnailPool = HouInputCategory.GetParentLayout().GetThumbnailPool();
	
	EHoudiniInputType MainInputType = MainInput->GetInputType();

	// Create a widget row, or get the given row.
	FDetailWidgetRow* Row = InputRow;
	Row = InputRow == nullptr ? &(HouInputCategory.AddCustomRow(FText::GetEmpty())) : InputRow;
	if (!Row)
		return;

	// Create the standard input name widget if this is not a operator path parameter.
	// Operator path parameter's name widget is handled by HoudiniParameterDetails.
	if (!InputRow)
		CreateNameWidget(MainInput, *Row, true, InInputs.Num());

	// Create a vertical Box for storing the UI
	TSharedRef< SVerticalBox > VerticalBox = SNew(SVerticalBox);

	// ComboBox :  Input Type
	const IDetailsView* DetailsView = HouInputCategory.GetParentLayout().GetDetailsView();
	AddInputTypeComboBox(VerticalBox, InInputs, DetailsView);

	// Checkbox : Keep World Transform
	AddKeepWorldTransformCheckBox(VerticalBox, InInputs);


	// Checkbox : CurveInput trigger cook on curve changed
	AddCurveInputCookOnChangeCheckBox(VerticalBox, InInputs);


	
	if (MainInputType == EHoudiniInputType::Geometry || MainInputType == EHoudiniInputType::World)
	{
		// Checkbox : Pack before merging
		AddPackBeforeMergeCheckbox(VerticalBox, InInputs);

		// Checkboxes : Export LODs / Sockets / Collisions
		AddExportCheckboxes(VerticalBox, InInputs);
	}

	switch (MainInput->GetInputType())
	{
		case EHoudiniInputType::Geometry:
		{
			AddGeometryInputUI(VerticalBox, InInputs, AssetThumbnailPool);
		}
		break;

		case EHoudiniInputType::Asset:
		{
			AddAssetInputUI(VerticalBox, InInputs);
		}
		break;

		case EHoudiniInputType::Curve:
		{
			AddCurveInputUI(VerticalBox, InInputs, AssetThumbnailPool);
		}
		break;

		case EHoudiniInputType::Landscape:
		{
			AddLandscapeInputUI(VerticalBox, InInputs);
		}
		break;

		case EHoudiniInputType::World:
		{
			AddWorldInputUI(VerticalBox, InInputs, DetailsView);
		}
		break;

		case EHoudiniInputType::Skeletal:
		{
			AddSkeletalInputUI(VerticalBox, InInputs, AssetThumbnailPool);
		}
		break;
	}


	Row->ValueWidget.Widget = VerticalBox;

	Row->ValueWidget.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH);
	//Row.ValueWidget.Widget->SetEnabled(!MainParam->IsDisabled());

}

void
FHoudiniInputDetails::CreateNameWidget(
	UHoudiniInput* InInput, FDetailWidgetRow & Row, bool bLabel, int32 InInputCount)
{
	if (!InInput || InInput->IsPendingKill())
		return;

	FString InputLabelStr = InInput->GetLabel();
	if (InInputCount > 1)
	{
		InputLabelStr += TEXT(" (") + FString::FromInt(InInputCount) + TEXT(")");
	}

	const FText & FinalInputLabelText = bLabel ? FText::FromString(InputLabelStr) : FText::GetEmpty();
	FText InputTooltip = GetInputTooltip(InInput);
	{
		Row.NameWidget.Widget =
			SNew(STextBlock)
			.Text(FinalInputLabelText)
			.ToolTipText(InputTooltip)
			.Font(FEditorStyle::GetFontStyle(!InInput->HasChanged() ? TEXT("PropertyWindow.NormalFont") : TEXT("PropertyWindow.BoldFont")));
	}
}

FText
FHoudiniInputDetails::GetInputTooltip(UHoudiniInput* InParam)
{
	// TODO
	return FText();
}

void
FHoudiniInputDetails::AddInputTypeComboBox(TSharedRef<SVerticalBox> VerticalBox, TArray<UHoudiniInput*>& InInputs, const IDetailsView* DetailsView)
{
	// Get the details view name and locked status
	bool bDetailsLocked = false;
	FName DetailsPanelName = "LevelEditorSelectionDetails";
	if (DetailsView)
	{
		DetailsPanelName = DetailsView->GetIdentifier();
		if (DetailsView->IsLocked())
			bDetailsLocked = true;
	}

	// Lambda return a FText correpsonding to an input's current type
	auto GetInputText = [](UHoudiniInput* InInput)
	{
		return FText::FromString(InInput->GetInputTypeAsString());
	};

	// Lambda for changing inputs type
	auto OnSelChanged = [DetailsPanelName](TArray<UHoudiniInput*> InInputsToUpdate, TSharedPtr<FString> InNewChoice)
	{
		if (!InNewChoice.IsValid())
			return;
		
		EHoudiniInputType NewInputType = UHoudiniInput::StringToInputType(*InNewChoice.Get());
		if (NewInputType != EHoudiniInputType::World)
		{
			Helper_CancelWorldSelection(InInputsToUpdate, DetailsPanelName);
		}

		for (auto CurInput : InInputsToUpdate)
		{
			if (CurInput->GetInputType() == NewInputType)
				continue;

			// Record a transaction for undo/redo
			FScopedTransaction Transaction(
				TEXT(HOUDINI_MODULE_EDITOR),
				LOCTEXT("HoudiniInputChange", "Houdini Input: Changing Input Type"),
				CurInput->GetOuter());
			CurInput->Modify();

			CurInput->SetInputType(NewInputType);
			CurInput->MarkChanged(true);
			
			ReselectSelectedActors();

			// TODO: Not needed?
			FHoudiniEngineUtils::UpdateEditorProperties(CurInput, true);
		}
	};

	UHoudiniInput* MainInput = InInputs[0];

	// ComboBox :  Input Type
	TSharedPtr< SComboBox< TSharedPtr< FString > > > ComboBoxInputType;
	VerticalBox->AddSlot().Padding(2, 2, 5, 2)
	[
		SAssignNew(ComboBoxInputType, SComboBox<TSharedPtr<FString>>)
		.OptionsSource(FHoudiniEngineEditor::Get().GetInputTypeChoiceLabels())
		.InitiallySelectedItem((*FHoudiniEngineEditor::Get().GetInputTypeChoiceLabels())[((int32)MainInput->GetInputType() - 1)])
		.OnGenerateWidget_Lambda(
			[](TSharedPtr< FString > ChoiceEntry)
		{
			FText ChoiceEntryText = FText::FromString(*ChoiceEntry);
			return SNew(STextBlock)
				.Text(ChoiceEntryText)
				.ToolTipText(ChoiceEntryText)
				.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")));
		})
		.OnSelectionChanged_Lambda([=](TSharedPtr<FString> NewChoice, ESelectInfo::Type SelectType)
		{
			return OnSelChanged(InInputs, NewChoice);
		})
		[
			SNew( STextBlock )
			.Text_Lambda([=]()
			{
				return GetInputText(MainInput); 
			})            
			.Font( FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
		]
	];
}

void
FHoudiniInputDetails:: AddCurveInputCookOnChangeCheckBox(TSharedRef< SVerticalBox > VerticalBox, TArray<UHoudiniInput*>& InInputs)
{
	if (InInputs.Num() <= 0)
		return;

	UHoudiniInput * MainInput = InInputs[0];

	if (!MainInput || MainInput->GetInputType() != EHoudiniInputType::Curve)
		return;

	auto IsCheckedCookOnChange = [MainInput]()
	{
		if (!MainInput)
			return ECheckBoxState::Checked;

		return MainInput->GetCookOnCurveChange() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	};

	auto CheckStateChangedCookOnChange = [InInputs](ECheckBoxState NewState)
	{
		bool bChecked = NewState == ECheckBoxState::Checked;
		for (auto & NextInput : InInputs) 
		{
			if (!NextInput)
				continue;

			NextInput->SetCookOnCurveChange(bChecked);
		}
	};

	// Checkbox : Trigger cook on input curve changed
	TSharedPtr< SCheckBox > CheckBoxCookOnCurveChanged;
	VerticalBox->AddSlot().Padding(2, 2, 5, 2).AutoHeight()
	[
		SAssignNew(CheckBoxCookOnCurveChanged, SCheckBox)
		.Content()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("CookOnCurveChangedCheckbox", "Auto-update"))
			.ToolTipText(LOCTEXT("CookOnCurveChangeCheckboxTip", "When checked, cook is triggered automatically when the curve is modified."))
			.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
		]
		.IsChecked_Lambda([IsCheckedCookOnChange, MainInput]()
		{
			return IsCheckedCookOnChange();
		})
		.OnCheckStateChanged_Lambda([CheckStateChangedCookOnChange](ECheckBoxState NewState)
		{
			return CheckStateChangedCookOnChange( NewState);
		})
	];
	
}

void
FHoudiniInputDetails::AddKeepWorldTransformCheckBox(TSharedRef< SVerticalBox > VerticalBox, TArray<UHoudiniInput*>& InInputs)
{
	// Lambda returning a CheckState from the input's current KeepWorldTransform state
	auto IsCheckedKeepWorldTransform = [&](UHoudiniInput* InInput)
	{
		return InInput->GetKeepWorldTransform() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	};

	// Lambda for changing KeepWorldTransform state
	auto CheckStateChangedKeepWorldTransform = [&](TArray<UHoudiniInput*> InInputsToUpdate, ECheckBoxState NewState)
	{
		bool bNewState = (NewState == ECheckBoxState::Checked);
		for (auto CurInput : InInputsToUpdate)
		{
			if (CurInput->GetKeepWorldTransform() == bNewState)
				continue;

			// Record a transaction for undo/redo
			FScopedTransaction Transaction(
				TEXT(HOUDINI_MODULE_EDITOR),
				LOCTEXT("HoudiniInputChange", "Houdini Input: Changing Keep World Transform"),
				CurInput->GetOuter());
			CurInput->Modify();

			CurInput->SetKeepWorldTransform(bNewState);
			CurInput->MarkChanged(true);
		}
	};

	UHoudiniInput* MainInput = InInputs[0];

	// Checkbox : Keep World Transform
	TSharedPtr< SCheckBox > CheckBoxTranformType;
	VerticalBox->AddSlot().Padding(2, 2, 5, 2).AutoHeight()
	[
		SAssignNew(CheckBoxTranformType, SCheckBox)
		.Content()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("KeepWorldTransformCheckbox", "Keep World Transform"))
			.ToolTipText(LOCTEXT("KeepWorldTransformCheckboxTip", "Set this Input's object_merge Transform Type to INTO_THIS_OBJECT. If unchecked, it will be set to NONE."))
			.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
		]		
		.IsChecked_Lambda([=]()
		{
			return IsCheckedKeepWorldTransform(MainInput);
		})
		.OnCheckStateChanged_Lambda([=](ECheckBoxState NewState)
		{
			return CheckStateChangedKeepWorldTransform(InInputs, NewState);
		})
	];

	// the checkbox is read only for geo inputs
	if (MainInput->GetInputType() == EHoudiniInputType::Geometry)
		CheckBoxTranformType->SetEnabled(false);

	// Checkbox is read only if the input is an object-path parameter
	//if (MainInput->IsObjectPathParameter() )
	//    CheckBoxTranformType->SetEnabled(false);
}

void
FHoudiniInputDetails::AddPackBeforeMergeCheckbox(TSharedRef< SVerticalBox > VerticalBox, TArray<UHoudiniInput*>& InInputs)
{
	// Lambda returning a CheckState from the input's current PackBeforeMerge state
	auto IsCheckedPackBeforeMerge = [&](UHoudiniInput* InInput)
	{
		return InInput->GetPackBeforeMerge() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	};

	// Lambda for changing PackBeforeMerge state
	auto CheckStateChangedPackBeforeMerge = [&](TArray<UHoudiniInput*> InInputsToUpdate, ECheckBoxState NewState)
	{
		bool bNewState = (NewState == ECheckBoxState::Checked);
		for (auto CurInput : InInputsToUpdate)
		{
			if (CurInput->GetPackBeforeMerge() == bNewState)
				continue;

			// Record a transaction for undo/redo
			FScopedTransaction Transaction(
				TEXT(HOUDINI_MODULE_EDITOR),
				LOCTEXT("HoudiniInputChange", "Houdini Input: Changing Pack before merge"),
				CurInput->GetOuter());
			CurInput->Modify();

			CurInput->SetPackBeforeMerge(bNewState);
			CurInput->MarkChanged(true);
		}
	};

	UHoudiniInput* MainInput = InInputs[0];

	TSharedPtr< SCheckBox > CheckBoxPackBeforeMerge;
	VerticalBox->AddSlot().Padding( 2, 2, 5, 2 ).AutoHeight()
	[
		SAssignNew( CheckBoxPackBeforeMerge, SCheckBox )
		.Content()
		[
			SNew( STextBlock )
			.Text( LOCTEXT( "PackBeforeMergeCheckbox", "Pack Geometry before merging" ) )
			.ToolTipText( LOCTEXT( "PackBeforeMergeCheckboxTip", "Pack each separate piece of geometry before merging them into the input." ) )
			.Font( FEditorStyle::GetFontStyle( TEXT( "PropertyWindow.NormalFont" ) ) )
		]
		.IsChecked_Lambda([=]()
		{
			return IsCheckedPackBeforeMerge(MainInput);
		})
		.OnCheckStateChanged_Lambda([=](ECheckBoxState NewState)
		{
			return CheckStateChangedPackBeforeMerge(InInputs, NewState);
		})
	];
}

void
FHoudiniInputDetails::AddExportCheckboxes(TSharedRef< SVerticalBox > VerticalBox, TArray<UHoudiniInput*>& InInputs)
{
	// Lambda returning a CheckState from the input's current ExportLODs state
	auto IsCheckedExportLODs = [&](UHoudiniInput* InInput)
	{
		return InInput->GetExportLODs() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	};

	// Lambda returning a CheckState from the input's current ExportSockets state
	auto IsCheckedExportSockets = [&](UHoudiniInput* InInput)
	{
		return InInput->GetExportSockets() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	};

	// Lambda returning a CheckState from the input's current ExportColliders state
	auto IsCheckedExportColliders = [&](UHoudiniInput* InInput)
	{
		return InInput->GetExportColliders() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	};

	// Lambda for changing ExportLODs state
	auto CheckStateChangedExportLODs = [&](TArray<UHoudiniInput*> InInputsToUpdate, ECheckBoxState NewState)
	{
		bool bNewState = (NewState == ECheckBoxState::Checked);
		for (auto CurInput : InInputsToUpdate)
		{
			if (CurInput->GetExportLODs() == bNewState)
				continue;

			// Record a transaction for undo/redo
			FScopedTransaction Transaction(
				TEXT(HOUDINI_MODULE_EDITOR),
				LOCTEXT("HoudiniInputChange", "Houdini Input: Changing Export LODs"),
				CurInput->GetOuter());
			CurInput->Modify();

			CurInput->SetExportLODs(bNewState);
			CurInput->MarkChanged(true);
			CurInput->MarkAllInputObjectsChanged(true);
		}
	};

	// Lambda for changing ExportSockets state
	auto CheckStateChangedExportSockets = [&](TArray<UHoudiniInput*> InInputsToUpdate, ECheckBoxState NewState)
	{
		bool bNewState = (NewState == ECheckBoxState::Checked);
		for (auto CurInput : InInputsToUpdate)
		{
			if (CurInput->GetExportSockets() == bNewState)
				continue;

			// Record a transaction for undo/redo
			FScopedTransaction Transaction(
				TEXT(HOUDINI_MODULE_EDITOR),
				LOCTEXT("HoudiniInputChange", "Houdini Input: Changing Export Sockets"),
				CurInput->GetOuter());
			CurInput->Modify();

			CurInput->SetExportSockets(bNewState);
			CurInput->MarkChanged(true);
			CurInput->MarkAllInputObjectsChanged(true);
		}
	};

	// Lambda for changing ExportColliders state
	auto CheckStateChangedExportColliders = [&](TArray<UHoudiniInput*> InInputsToUpdate, ECheckBoxState NewState)
	{
		bool bNewState = (NewState == ECheckBoxState::Checked);
		for (auto CurInput : InInputsToUpdate)
		{
			if (CurInput->GetExportColliders() == bNewState)
				continue;

			// Record a transaction for undo/redo
			FScopedTransaction Transaction(
				TEXT(HOUDINI_MODULE_EDITOR),
				LOCTEXT("HoudiniInputChange", "Houdini Input: Changing Export Colliders"),
				CurInput->GetOuter());
			CurInput->Modify();
			
			CurInput->SetExportColliders(bNewState);
			CurInput->MarkChanged(true);
			CurInput->MarkAllInputObjectsChanged(true);
		}
	};

	UHoudiniInput* MainInput = InInputs[0];

	TSharedPtr< SCheckBox > CheckBoxExportLODs;
    TSharedPtr< SCheckBox > CheckBoxExportSockets;
	TSharedPtr< SCheckBox > CheckBoxExportColliders;
    VerticalBox->AddSlot().Padding( 2, 2, 5, 2 ).AutoHeight()
    [
        SNew( SHorizontalBox )
        + SHorizontalBox::Slot()
        .Padding( 1.0f )
        .VAlign( VAlign_Center )
        .AutoWidth()
        [
            SAssignNew(CheckBoxExportLODs, SCheckBox )
            .Content()
            [
                SNew( STextBlock )
                .Text( LOCTEXT( "ExportAllLOD", "Export LODs" ) )
                .ToolTipText( LOCTEXT( "ExportAllLODCheckboxTip", "If enabled, all LOD Meshes in this static mesh will be sent to Houdini." ) )
                .Font( FEditorStyle::GetFontStyle( TEXT( "PropertyWindow.NormalFont" ) ) )
            ]
			.IsChecked_Lambda([=]()
			{
				return IsCheckedExportLODs(MainInput);
			})
			.OnCheckStateChanged_Lambda([=](ECheckBoxState NewState)
			{
				return CheckStateChangedExportLODs(InInputs, NewState);
			})
        ]
        + SHorizontalBox::Slot()
        .Padding( 1.0f )
        .VAlign( VAlign_Center )
        .AutoWidth()
        [
            SAssignNew( CheckBoxExportSockets, SCheckBox )
            .Content()
            [
                SNew( STextBlock )
                .Text( LOCTEXT( "ExportSockets", "Export Sockets" ) )
                .ToolTipText( LOCTEXT( "ExportSocketsTip", "If enabled, all Mesh Sockets in this static mesh will be sent to Houdini." ) )
                .Font( FEditorStyle::GetFontStyle( TEXT( "PropertyWindow.NormalFont" ) ) )
            ]
			.IsChecked_Lambda([=]()
			{
				return IsCheckedExportSockets(MainInput);
			})
				.OnCheckStateChanged_Lambda([=](ECheckBoxState NewState)
			{
				return CheckStateChangedExportSockets(InInputs, NewState);
			})
        ]
		+ SHorizontalBox::Slot()
        .Padding( 1.0f )
        .VAlign( VAlign_Center )
        .AutoWidth()
        [
            SAssignNew( CheckBoxExportColliders, SCheckBox )
            .Content()
            [
                SNew( STextBlock )
                .Text( LOCTEXT( "ExportColliders", "Export Colliders" ) )
                .ToolTipText( LOCTEXT( "ExportCollidersTip", "If enabled, collision geometry for this static mesh will be sent to Houdini." ) )
                .Font( FEditorStyle::GetFontStyle( TEXT( "PropertyWindow.NormalFont" ) ) )
            ]
			.IsChecked_Lambda([=]()
			{
				return IsCheckedExportColliders(MainInput);
			})
			.OnCheckStateChanged_Lambda([=](ECheckBoxState NewState)
			{
				return CheckStateChangedExportColliders(InInputs, NewState);
			})
        ]
    ];
}

void
FHoudiniInputDetails::AddGeometryInputUI(
	TSharedRef<SVerticalBox> InVerticalBox, 
	TArray<UHoudiniInput*>& InInputs,
	TSharedPtr<FAssetThumbnailPool> AssetThumbnailPool )
{
	if (InInputs.Num() <= 0)
		return;

	UHoudiniInput* MainInput = InInputs[0];

	if (!MainInput)
		return;

	const int32 NumInputObjects = MainInput->GetNumberOfInputObjects(EHoudiniInputType::Geometry);

	// Lambda for changing ExportColliders state
	auto SetGeometryInputObjectsCount = [&](TArray<UHoudiniInput*> InInputsToUpdate, const int32& NewInputCount)
	{
		for (auto CurInput : InInputsToUpdate)
		{
			if (CurInput->GetNumberOfInputObjects(EHoudiniInputType::Geometry) == NewInputCount)
				continue;

			// Record a transaction for undo/redo
			FScopedTransaction Transaction(
				TEXT(HOUDINI_MODULE_EDITOR),
				LOCTEXT("HoudiniInputChange", "Houdini Input: Changing the number of Geometry Input Objects"),
				CurInput->GetOuter());
			CurInput->Modify();

			CurInput->SetInputObjectsNumber(EHoudiniInputType::Geometry, NewInputCount);
			CurInput->MarkChanged(true);

			// 
			FHoudiniEngineUtils::UpdateEditorProperties(CurInput, true);
		}
	};

	InVerticalBox->AddSlot().Padding(2, 2, 5, 2).AutoHeight()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.Padding(1.0f)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(STextBlock)
			.Text(FText::Format(LOCTEXT("NumArrayItemsFmt", "{0} elements"), FText::AsNumber(NumInputObjects)))
			.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
		]
		+ SHorizontalBox::Slot()
		.Padding(1.0f)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			PropertyCustomizationHelpers::MakeAddButton(FSimpleDelegate::CreateLambda([SetGeometryInputObjectsCount, InInputs, NumInputObjects]()
			{
				return SetGeometryInputObjectsCount(InInputs, NumInputObjects + 1);
			}),
			LOCTEXT("AddInput", "Adds a Geometry Input"), true)
		]
		+ SHorizontalBox::Slot()
		.Padding(1.0f)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			PropertyCustomizationHelpers::MakeEmptyButton(FSimpleDelegate::CreateLambda([SetGeometryInputObjectsCount, InInputs]()
			{
				return SetGeometryInputObjectsCount(InInputs, 0);
			}),
			LOCTEXT("EmptyInputs", "Removes All Inputs"), true)
		]
	];

	for (int32 GeometryObjectIdx = 0; GeometryObjectIdx < NumInputObjects; GeometryObjectIdx++)
	{
		//UObject* InputObject = InParam.GetInputObject(Idx);
		Helper_CreateGeometryWidget(InInputs, GeometryObjectIdx, AssetThumbnailPool, InVerticalBox);
	}
}



// Create a single geometry widget for the given input object
void 
FHoudiniInputDetails::Helper_CreateGeometryWidget(
	TArray<UHoudiniInput*>& InInputs, 
	const int32& InGeometryObjectIdx,
	TSharedPtr< FAssetThumbnailPool > AssetThumbnailPool,
	TSharedRef< SVerticalBox > VerticalBox )
{
	UHoudiniInput* MainInput = InInputs[0];

	// Access the object used in the corresponding geometry input
	UHoudiniInputObject* HoudiniInputObject = MainInput->GetHoudiniInputObjectAt(EHoudiniInputType::Geometry, InGeometryObjectIdx);
	UObject* InputObject = HoudiniInputObject ? HoudiniInputObject->GetObject() : nullptr;

    // Create thumbnail for this static mesh.
    TSharedPtr<FAssetThumbnail> StaticMeshThumbnail = MakeShareable(
        new FAssetThumbnail(InputObject, 64, 64, AssetThumbnailPool));

	// Lambda for adding new geometry input objects
	auto UpdateGeometryObjectAt = [&](TArray<UHoudiniInput*> InInputsToUpdate, const int32& AtIndex, UObject* InObject)
	{
		for (auto CurInput : InInputsToUpdate)
		{
			UObject* InputObject = CurInput->GetInputObjectAt(EHoudiniInputType::Geometry, AtIndex);
			if (InObject == InputObject)
				continue;
			
			// Record a transaction for undo/redo
			FScopedTransaction Transaction(
				TEXT(HOUDINI_MODULE_EDITOR),
				LOCTEXT("HoudiniInputChange", "Houdini Input: Changing a Geometry Input Object"),
				CurInput->GetOuter());
			CurInput->Modify();

			CurInput->SetInputObjectAt(EHoudiniInputType::Geometry, AtIndex, InObject);
			CurInput->MarkChanged(true);
			
			// TODO: Not needed?
			FHoudiniEngineUtils::UpdateEditorProperties(CurInput, true);
		}
	};

    // Drop Target: Static/Skeletal Mesh
	TSharedPtr<SHorizontalBox> HorizontalBox = NULL;
    VerticalBox->AddSlot().Padding( 0, 2 ).AutoHeight()
    [
        SNew( SAssetDropTarget )
        .OnIsAssetAcceptableForDrop_Lambda([]( const UObject* InObject)
        {
			return UHoudiniInput::IsObjectAcceptable(EHoudiniInputType::Geometry, InObject);
		})
		.OnAssetDropped_Lambda([InInputs, InGeometryObjectIdx, UpdateGeometryObjectAt](UObject* InObject)
		{
			return UpdateGeometryObjectAt(InInputs, InGeometryObjectIdx, InObject);
		})
        [
            SAssignNew(HorizontalBox, SHorizontalBox)
        ]
    ];

    // Thumbnail : Static Mesh
    FText ParameterLabelText = FText::FromString(MainInput->GetLabel());
	
    TSharedPtr< SBorder > StaticMeshThumbnailBorder;
    HorizontalBox->AddSlot().Padding(0.0f, 0.0f, 2.0f, 0.0f).AutoWidth()
    [
        SAssignNew(StaticMeshThumbnailBorder, SBorder)
        .Padding(5.0f)
		.OnMouseDoubleClick_Lambda([MainInput, InGeometryObjectIdx](const FGeometry&, const FPointerEvent&)
		{
			UObject* InputObject = MainInput->GetInputObjectAt(EHoudiniInputType::Geometry, InGeometryObjectIdx);
			if (GEditor && InputObject)
				GEditor->EditObject(InputObject);

			return FReply::Handled(); 
		})		
        [
            SNew(SBox)
            .WidthOverride(64)
            .HeightOverride(64)
            .ToolTipText(ParameterLabelText)
            [
               StaticMeshThumbnail->MakeThumbnailWidget()
            ]
        ]
    ];
	
    StaticMeshThumbnailBorder->SetBorderImage(TAttribute<const FSlateBrush *>::Create(
        TAttribute<const FSlateBrush *>::FGetter::CreateLambda([StaticMeshThumbnailBorder]()
        {
            if (StaticMeshThumbnailBorder.IsValid() && StaticMeshThumbnailBorder->IsHovered())
                return FEditorStyle::GetBrush("PropertyEditor.AssetThumbnailLight");
            else
                return FEditorStyle::GetBrush("PropertyEditor.AssetThumbnailShadow");
        }
    ) ) );
	
    FText MeshNameText = FText::GetEmpty();
    if (InputObject)
        MeshNameText = FText::FromString(InputObject->GetName());

    // ComboBox : Static Mesh
    TSharedPtr<SComboButton> StaticMeshComboButton;

    TSharedPtr<SHorizontalBox> ButtonBox;
    HorizontalBox->AddSlot()
    .FillWidth(1.0f)
    .Padding(0.0f, 4.0f, 4.0f, 4.0f)
    .VAlign(VAlign_Center)
    [
        SNew(SVerticalBox)
        + SVerticalBox::Slot()
        .HAlign(HAlign_Fill)
        [
            SAssignNew( ButtonBox, SHorizontalBox )
            + SHorizontalBox::Slot()
            [
                SAssignNew( StaticMeshComboButton, SComboButton )
                .ButtonStyle( FEditorStyle::Get(), "PropertyEditor.AssetComboStyle" )
                .ForegroundColor( FEditorStyle::GetColor( "PropertyEditor.AssetName.ColorAndOpacity" ) )
                .ContentPadding( 2.0f )
                .ButtonContent()
                [
                    SNew( STextBlock )
                    .TextStyle( FEditorStyle::Get(), "PropertyEditor.AssetClass" )
                    .Font( FEditorStyle::GetFontStyle( FName( TEXT( "PropertyWindow.NormalFont" ) ) ) )
                    .Text( MeshNameText )
                ]
            ]
        ]
    ];

    StaticMeshComboButton->SetOnGetMenuContent( FOnGetContent::CreateLambda(
        [ MainInput, InInputs, InGeometryObjectIdx, StaticMeshComboButton, UpdateGeometryObjectAt]()
        {
			TArray< const UClass * > AllowedClasses = UHoudiniInput::GetAllowedClasses(EHoudiniInputType::Geometry);
			UObject* DefaultObj = MainInput->GetInputObjectAt(InGeometryObjectIdx);

            TArray< UFactory * > NewAssetFactories;
            return PropertyCustomizationHelpers::MakeAssetPickerWithMenu(
                FAssetData(DefaultObj),
                true,
                AllowedClasses,
                NewAssetFactories,
                FOnShouldFilterAsset(),
                FOnAssetSelected::CreateLambda( [InInputs, InGeometryObjectIdx, StaticMeshComboButton, UpdateGeometryObjectAt]( const FAssetData & AssetData )
				{
                    if ( StaticMeshComboButton.IsValid() )
                    {
                        StaticMeshComboButton->SetIsOpen( false );
                        UObject * Object = AssetData.GetAsset();
						UpdateGeometryObjectAt(InInputs, InGeometryObjectIdx, Object);
                    }
                } ),
                FSimpleDelegate::CreateLambda( []() {} ) );
        } ) 
	);

    // Create tooltip.
    FFormatNamedArguments Args;
    Args.Add( TEXT( "Asset" ), MeshNameText );
	FText StaticMeshTooltip = FText::Format(
		LOCTEXT("BrowseToSpecificAssetInContentBrowser",
            "Browse to '{Asset}' in Content Browser" ), Args );

    // Button : Browse Static Mesh
    ButtonBox->AddSlot()
    .AutoWidth()
    .Padding( 2.0f, 0.0f )
    .VAlign( VAlign_Center )
    [
        PropertyCustomizationHelpers::MakeBrowseButton(
			FSimpleDelegate::CreateLambda([MainInput, InGeometryObjectIdx]()
			{
				UObject* InputObject = MainInput->GetInputObjectAt(InGeometryObjectIdx);
				if (GEditor && InputObject)
				{
					TArray<UObject*> Objects;
					Objects.Add(InputObject);
					GEditor->SyncBrowserToObjects(Objects);
				}
			}),
            TAttribute< FText >( StaticMeshTooltip )
		)
    ];

    // ButtonBox : Reset
    ButtonBox->AddSlot()
    .AutoWidth()
    .Padding( 2.0f, 0.0f )
    .VAlign( VAlign_Center )
    [
        SNew( SButton )
        .ToolTipText( LOCTEXT( "ResetToBase", "Reset to default static mesh" ) )
        .ButtonStyle( FEditorStyle::Get(), "NoBorder" )
        .ContentPadding( 0 )
        .Visibility( EVisibility::Visible )
        .OnClicked_Lambda( [UpdateGeometryObjectAt, InInputs, InGeometryObjectIdx]()
		{
			UpdateGeometryObjectAt(InInputs, InGeometryObjectIdx, nullptr);
			return FReply::Handled();
		})
        [
            SNew( SImage )
            .Image( FEditorStyle::GetBrush( "PropertyWindow.DiffersFromDefault" ) )
        ]
    ];

	// Insert/Delete/Duplicate
    ButtonBox->AddSlot()
    .Padding( 1.0f )
    .VAlign( VAlign_Center )
    .AutoWidth()
    [
        PropertyCustomizationHelpers::MakeInsertDeleteDuplicateButton(
        FExecuteAction::CreateLambda( [ InInputs, InGeometryObjectIdx ]() 
            {
				// Insert
				for (auto CurInput : InInputs)
				{
					FScopedTransaction Transaction(
						TEXT(HOUDINI_MODULE_EDITOR),
						LOCTEXT("HoudiniInputChange", "Houdini Input: insert a Geometry Input Object"),
						CurInput->GetOuter());
					CurInput->Modify();
					CurInput->InsertInputObjectAt(EHoudiniInputType::Geometry, InGeometryObjectIdx);
				}
            } 
        ),
		FExecuteAction::CreateLambda([InInputs, InGeometryObjectIdx]()
			{
				// Delete
				for (auto CurInput : InInputs)
				{
					FScopedTransaction Transaction(
						TEXT(HOUDINI_MODULE_EDITOR),
						LOCTEXT("HoudiniInputChange", "Houdini Input: delete a Geometry Input Object"),
						CurInput->GetOuter());
					CurInput->Modify();
					CurInput->DeleteInputObjectAt(EHoudiniInputType::Geometry, InGeometryObjectIdx);
				}
			}
        ),
		FExecuteAction::CreateLambda([InInputs, InGeometryObjectIdx]()
			{
				// Duplicate
				for (auto CurInput : InInputs)
				{
					FScopedTransaction Transaction(
						TEXT(HOUDINI_MODULE_EDITOR),
						LOCTEXT("HoudiniInputChange", "Houdini Input: duplicate a Geometry Input Object"),
						CurInput->GetOuter());
					CurInput->Modify();
					CurInput->DuplicateInputObjectAt(EHoudiniInputType::Geometry, InGeometryObjectIdx);
				}
			}
        ) )
    ];
    
	// TRANSFORM OFFSET EXPANDER
    {
        TSharedPtr<SButton> ExpanderArrow;
        TSharedPtr<SImage> ExpanderImage;
        VerticalBox->AddSlot().Padding( 0, 2 ).AutoHeight()
        [
            SNew( SHorizontalBox )
            +SHorizontalBox::Slot()
            .Padding( 1.0f )
            .VAlign( VAlign_Center )
            .AutoWidth()
            [
                SAssignNew( ExpanderArrow, SButton )
                .ButtonStyle( FEditorStyle::Get(), "NoBorder" )
                .ClickMethod( EButtonClickMethod::MouseDown )
                .Visibility( EVisibility::Visible )
				.OnClicked(FOnClicked::CreateLambda([InInputs, InGeometryObjectIdx]()
				{
					// Expand transform
					for (auto CurInput : InInputs)
					{
						FScopedTransaction Transaction(
							TEXT(HOUDINI_MODULE_EDITOR),
							LOCTEXT("HoudiniInputChange", "Houdini Input: duplicate a Geometry Input Object"),
							CurInput->GetOuter());
						CurInput->Modify();	
						CurInput->OnTransformUIExpand(InGeometryObjectIdx);
					}

					// TODO: Not needed?
					FHoudiniEngineUtils::UpdateEditorProperties(InInputs.Num() > 0 ? InInputs[0] : nullptr, true);

					return FReply::Handled();
				}))
				[
					SAssignNew(ExpanderImage, SImage)
					.ColorAndOpacity(FSlateColor::UseForeground())
				]
            ]
            +SHorizontalBox::Slot()
            .Padding( 1.0f )
            .VAlign( VAlign_Center )
            .AutoWidth()
            [
                SNew( STextBlock )
                .Text( LOCTEXT("GeoInputTransform", "Transform Offset") )
                .ToolTipText( LOCTEXT( "GeoInputTransformTooltip", "Transform offset used for correction before sending the asset to Houdini" ) )
                .Font( FEditorStyle::GetFontStyle( TEXT( "PropertyWindow.NormalFont" ) ) )
            ]
        ];

        // Set delegate for image
        ExpanderImage->SetImage(
            TAttribute<const FSlateBrush*>::Create(
                TAttribute<const FSlateBrush*>::FGetter::CreateLambda( [=]() {
            FName ResourceName;
            if (MainInput->IsTransformUIExpanded(InGeometryObjectIdx) )
            {
				ResourceName = ExpanderArrow->IsHovered() ? "TreeArrow_Expanded_Hovered" : "TreeArrow_Expanded";
            }
            else
            {
                ResourceName = ExpanderArrow->IsHovered() ? "TreeArrow_Collapsed_Hovered" : "TreeArrow_Collapsed";
            }
            return FEditorStyle::GetBrush( ResourceName );
        } ) ) );
    }

	// Lambda for changing the transform values
	auto ChangeTransformOffsetAt = [&](const float& Value, const int32& AtIndex, const int32& PosRotScaleIndex, const int32& XYZIndex, const bool& DoChange, TArray<UHoudiniInput*> InInputs)
	{
		// Record a transaction for undo/redo
		FScopedTransaction Transaction(
			TEXT(HOUDINI_MODULE_EDITOR),
			LOCTEXT("HoudiniInputTransformChange", "Houdini Input: Changing Transform offset"),
			InInputs[0]->GetOuter());

		bool bChanged = true;
		for (int Idx = 0; Idx < InInputs.Num(); Idx++)
		{
			InInputs[Idx]->Modify();
			bChanged &= InInputs[Idx]->SetTransformOffsetAt(Value, AtIndex, PosRotScaleIndex, XYZIndex);
		}

		if (bChanged && DoChange)
		{
			// Mark the values as changed to trigger an update
			for (int Idx = 0; Idx < InInputs.Num(); Idx++)
			{
				InInputs[Idx]->MarkChanged(true);
			}
		}
		else
		{
			// Cancel the transaction
			Transaction.Cancel();
		}
	};

    // TRANSFORM OFFSET
	if (MainInput->IsTransformUIExpanded(InGeometryObjectIdx))
    {
        // Position
        VerticalBox->AddSlot().Padding( 0, 2 ).AutoHeight()
        [
            SNew( SHorizontalBox )
            +SHorizontalBox::Slot()
            .Padding( 1.0f )
            .VAlign( VAlign_Center )
            .AutoWidth()
            [
                SNew(STextBlock)
                .Text( LOCTEXT("GeoInputTranslate", "T") )
                .ToolTipText( LOCTEXT( "GeoInputTranslateTooltip", "Translate" ) )
                .Font( FEditorStyle::GetFontStyle( TEXT( "PropertyWindow.NormalFont" ) ) )
            ]
            + SHorizontalBox::Slot()
			.FillWidth(1.0f)
            [
                SNew( SVectorInputBox )
                .bColorAxisLabels( true )
				.AllowSpin(true)
				.X(TAttribute<TOptional<float>>::Create(
					TAttribute<TOptional<float>>::FGetter::CreateUObject(
						MainInput, &UHoudiniInput::GetPositionOffsetX, InGeometryObjectIdx)))
				.Y(TAttribute<TOptional<float>>::Create(
					TAttribute<TOptional<float>>::FGetter::CreateUObject(
						MainInput, &UHoudiniInput::GetPositionOffsetY, InGeometryObjectIdx)))
				.Z(TAttribute<TOptional<float>>::Create(
					TAttribute<TOptional<float>>::FGetter::CreateUObject(
						MainInput, &UHoudiniInput::GetPositionOffsetZ, InGeometryObjectIdx)))
				/*
				.OnXChanged_Lambda([=](float Val)
					{ ChangeTransformOffsetAt(Val, InGeometryObjectIdx, 0, 0, false, InInputs); })
				.OnYChanged_Lambda([=](float Val)
					{ ChangeTransformOffsetAt(Val, InGeometryObjectIdx, 0, 1, false, InInputs); })
				.OnZChanged_Lambda([=](float Val)
					{ ChangeTransformOffsetAt(Val, InGeometryObjectIdx, 0, 2, false, InInputs); })
				*/
				.OnXCommitted_Lambda([=](float Val, ETextCommit::Type TextCommitType)
					{ ChangeTransformOffsetAt(Val, InGeometryObjectIdx, 0, 0, true, InInputs); })
				.OnYCommitted_Lambda([=](float Val, ETextCommit::Type TextCommitType)
					{ ChangeTransformOffsetAt(Val, InGeometryObjectIdx, 0, 1, true, InInputs); })
				.OnZCommitted_Lambda([=](float Val, ETextCommit::Type TextCommitType)
					{ ChangeTransformOffsetAt(Val, InGeometryObjectIdx, 0, 2, true, InInputs); })
            ]
        ];

        // Rotation
        VerticalBox->AddSlot().Padding( 0, 2 ).AutoHeight()
        [
            SNew( SHorizontalBox )
            +SHorizontalBox::Slot()
            .Padding(1.0f)
            .VAlign(VAlign_Center)
            .AutoWidth()
            [
                SNew(STextBlock)
                .Text( LOCTEXT("GeoInputRotate", "R") )
                .ToolTipText( LOCTEXT( "GeoInputRotateTooltip", "Rotate" ) )
                .Font( FEditorStyle::GetFontStyle( TEXT( "PropertyWindow.NormalFont" ) ) )
            ]
            + SHorizontalBox::Slot()
			.FillWidth(1.0f)
            [
                SNew( SRotatorInputBox )
                .AllowSpin( true )
                .bColorAxisLabels( true )
				.Roll(TAttribute<TOptional<float>>::Create(
					TAttribute<TOptional<float>>::FGetter::CreateUObject(
						MainInput, &UHoudiniInput::GetRotationOffsetRoll, InGeometryObjectIdx)))
				.Pitch(TAttribute<TOptional<float>>::Create(
					TAttribute<TOptional<float>>::FGetter::CreateUObject(
						MainInput, &UHoudiniInput::GetRotationOffsetPitch, InGeometryObjectIdx)))
				.Yaw(TAttribute<TOptional<float>>::Create(
					TAttribute<TOptional<float>>::FGetter::CreateUObject(
						MainInput, &UHoudiniInput::GetRotationOffsetYaw, InGeometryObjectIdx)))
				.OnRollCommitted_Lambda([=](float Val, ETextCommit::Type TextCommitType)
					{ ChangeTransformOffsetAt(Val, InGeometryObjectIdx, 1, 0, true, InInputs); })
				.OnPitchCommitted_Lambda([=](float Val, ETextCommit::Type TextCommitType)
					{ ChangeTransformOffsetAt(Val, InGeometryObjectIdx, 1, 1, true, InInputs); })
				.OnYawCommitted_Lambda([=](float Val, ETextCommit::Type TextCommitType)
					{ ChangeTransformOffsetAt(Val, InGeometryObjectIdx, 1, 2, true, InInputs); })
            ]
        ];

        // Scale
        VerticalBox->AddSlot().Padding( 0, 2 ).AutoHeight()
        [
            SNew( SHorizontalBox )
            +SHorizontalBox::Slot()
            .Padding( 1.0f )
            .VAlign( VAlign_Center )
            .AutoWidth()
            [
                SNew( STextBlock )
                .Text( LOCTEXT( "GeoInputScale", "S" ) )
                .ToolTipText( LOCTEXT( "GeoInputScaleTooltip", "Scale" ) )
                .Font( FEditorStyle::GetFontStyle( TEXT( "PropertyWindow.NormalFont" ) ) )
            ]
            + SHorizontalBox::Slot()
			.FillWidth(1.0f)
            [
                SNew( SVectorInputBox )
                .bColorAxisLabels( true )
				.X(TAttribute<TOptional<float>>::Create(
					TAttribute<TOptional<float>>::FGetter::CreateUObject(
						MainInput, &UHoudiniInput::GetScaleOffsetX, InGeometryObjectIdx)))
				.Y(TAttribute<TOptional<float>>::Create(
					TAttribute<TOptional<float>>::FGetter::CreateUObject(
						MainInput, &UHoudiniInput::GetScaleOffsetY, InGeometryObjectIdx)))
				.Z(TAttribute<TOptional<float>>::Create(
					TAttribute<TOptional<float>>::FGetter::CreateUObject(
						MainInput, &UHoudiniInput::GetScaleOffsetZ, InGeometryObjectIdx)))
				.OnXCommitted_Lambda([=](float Val, ETextCommit::Type TextCommitType)
					{ ChangeTransformOffsetAt(Val, InGeometryObjectIdx, 2, 0, true, InInputs); })
				.OnYCommitted_Lambda([=](float Val, ETextCommit::Type TextCommitType)
					{ ChangeTransformOffsetAt(Val, InGeometryObjectIdx, 2, 1, true, InInputs); })
				.OnZCommitted_Lambda([=](float Val, ETextCommit::Type TextCommitType)
					{ ChangeTransformOffsetAt(Val, InGeometryObjectIdx, 2, 2, true, InInputs); })
            ]
        ];
    }
}

void
FHoudiniInputDetails::AddAssetInputUI(TSharedRef< SVerticalBox > VerticalBox, TArray<UHoudiniInput*>& InInputs)
{
	if (InInputs.Num() <= 0)
		return;

	UHoudiniInput* MainInput = InInputs[0];

	if (!MainInput || MainInput->IsPendingKill())
		return;
	
	// Houdini Asset Picker Widget
	{
		FMenuBuilder MenuBuilder = Helper_CreateHoudiniAssetPickerWidget(InInputs);

		VerticalBox->AddSlot()
		.Padding(2.0f, 2.0f, 5.0f, 2.0f)
		.AutoHeight()
		[
			MenuBuilder.MakeWidget()
		];	
	}	

	// Button : Select All + Clear Selection
	{
		TSharedPtr< SHorizontalBox > HorizontalBox = NULL;
		FOnClicked OnClearSelect = FOnClicked::CreateLambda([InInputs]()
		{
			for (auto CurrentInput : InInputs)
			{
				TArray<UHoudiniInputObject*>* AssetInputObjectsArray = CurrentInput->GetHoudiniInputObjectArray(EHoudiniInputType::Asset);
				if (!AssetInputObjectsArray)
					continue;

				FScopedTransaction Transaction(
					TEXT(HOUDINI_MODULE_EDITOR),
					LOCTEXT("HoudiniInputChange", "Houdini Input: Clear asset input selection"),
					CurrentInput->GetOuter());
				CurrentInput->Modify();

				AssetInputObjectsArray->Empty();
				CurrentInput->MarkChanged(true);
			}

			return FReply::Handled();
		});

		VerticalBox->AddSlot()
		.Padding(2, 2, 5, 2)
		.AutoHeight()
		[
			SAssignNew(HorizontalBox, SHorizontalBox)
			+ SHorizontalBox::Slot()
			[
				// Button :  Clear Selection
				SNew(SButton)
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				.Text(LOCTEXT("ClearSelection", "Clear Selection"))
				.ToolTipText(LOCTEXT("ClearSelectionTooltip", "Clear the input's current selection"))
				.OnClicked(OnClearSelect)
			]
		];

		// Do not enable select all/clear select when selection has been started and details are locked
		//HorizontalBox->SetEnabled(!bDetailsLocked);
	}


}

void
FHoudiniInputDetails::AddCurveInputUI(TSharedRef< SVerticalBox > VerticalBox, TArray<UHoudiniInput*>& InInputs, TSharedPtr<FAssetThumbnailPool> AssetThumbnailPool)
{
	if (InInputs.Num() <= 0)
		return;

	UHoudiniInput* MainInput = InInputs[0];
	if (!MainInput)
		return;

	const int32 NumInputObjects = MainInput->GetNumberOfInputObjects(EHoudiniInputType::Curve);
	
	// lambda for inserting an input Houdini curve.
	auto InsertAnInputCurve = [MainInput](const int32& NewInputCount) 
	{
		// Clear the to be inserted object array, which records the pointers of the input objects to be inserted.
		MainInput->LastInsertedInputs.Empty();
		// Record the pointer of the object to be inserted before transaction for undo the insert action.
		UHoudiniInputHoudiniSplineComponent* NewInput = MainInput->CreateHoudiniSplineInput(nullptr, false);
		MainInput->LastInsertedInputs.Add(NewInput);

		ReselectSelectedActors();

		// Record a transaction for undo/redo
		FScopedTransaction Transaction(FText::FromString("Modifying Houdini input: Adding curve input."));
		MainInput->Modify();

		// Modify the MainInput.
		MainInput->GetHoudiniInputObjectArray(MainInput->GetInputType())->Add(NewInput);

		MainInput->SetInputObjectsNumber(EHoudiniInputType::Curve, NewInputCount);

		FHoudiniEngineUtils::UpdateEditorProperties(MainInput, true);
	};

	
	VerticalBox->AddSlot().Padding(2, 2, 5, 2).AutoHeight()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.Padding(1.0f)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(STextBlock)
			.Text(FText::Format(LOCTEXT("NumArrayItemsFmt", "{0} elements"), FText::AsNumber(NumInputObjects)))
			.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
		]
		+ SHorizontalBox::Slot()
		.Padding(1.0f)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			PropertyCustomizationHelpers::MakeAddButton(FSimpleDelegate::CreateLambda([InsertAnInputCurve, NumInputObjects]()
			{
				return InsertAnInputCurve(NumInputObjects+1);
				//return SetCurveInputObjectsCount(NumInputObjects+1);
			}),

			LOCTEXT("AddInputCurve", "Adds a Curve Input"), true)
		]
		+ SHorizontalBox::Slot()
		.Padding(1.0f)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			PropertyCustomizationHelpers::MakeEmptyButton( FSimpleDelegate::CreateLambda([InInputs, MainInput]()
			{
				TArray<UHoudiniInputObject*> * CurveInputComponentArray = MainInput->GetHoudiniInputObjectArray(EHoudiniInputType::Curve);

				// Detach all curves before deleting.
				for (int n = CurveInputComponentArray->Num() - 1; n >= 0; n--)
				{
					UHoudiniInputHoudiniSplineComponent* HoudiniInput = 
						Cast <UHoudiniInputHoudiniSplineComponent>((*CurveInputComponentArray)[n]);
					if (!HoudiniInput || HoudiniInput->IsPendingKill())
						continue;

					UHoudiniSplineComponent* HoudiniSplineComponent = HoudiniInput->GetCurveComponent();
					if (!HoudiniSplineComponent || HoudiniSplineComponent->IsPendingKill())
						continue;

					FDetachmentTransformRules DetachTransRules(EDetachmentRule::KeepRelative, EDetachmentRule::KeepRelative, EDetachmentRule::KeepRelative, false);
					HoudiniSplineComponent->DetachFromComponent(DetachTransRules);
				}

				// Clear the insert objects buffer before transaction.
				MainInput->LastInsertedInputs.Empty();

				// Record a transaction for undo/redo
				FScopedTransaction Transaction(FText::FromString("Modifying Houdini Input: Delete curve inputs."));
				MainInput->Modify();

				// actual delete.
				for (int n = CurveInputComponentArray->Num() - 1; n >= 0; n--)
				{
					UHoudiniInputHoudiniSplineComponent* HoudiniInput =
						Cast <UHoudiniInputHoudiniSplineComponent>((*CurveInputComponentArray)[n]);
					if (!HoudiniInput || HoudiniInput->IsPendingKill())
						continue;

					UHoudiniSplineComponent* HoudiniSplineComponent = HoudiniInput->GetCurveComponent();
					if (!HoudiniSplineComponent || HoudiniSplineComponent->IsPendingKill())
						continue;

					MainInput->DeleteInputObjectAt(EHoudiniInputType::Curve, n);
				}

				MainInput->SetInputObjectsNumber(EHoudiniInputType::Curve, 0);
				FHoudiniEngineUtils::UpdateEditorProperties(MainInput, true);
		
			}),
			LOCTEXT("EmptyInputsCurve", "Removes All Curve Inputs"), true)
		]
		+ SHorizontalBox::Slot().FillWidth(80.f).MaxWidth(80.f)
		[
			SNew(SButton)
			.Text(LOCTEXT("ResetCurveOffsetStr", "Reset Offset"))
			.OnClicked_Lambda([MainInput]()->FReply 
			{
				MainInput->ResetDefaultCurveOffset();
				return FReply::Handled(); 
			})
		]
	];

	for (int n = 0; n < NumInputObjects; n++) 
	{
		Helper_CreateCurveWidget(InInputs, n, AssetThumbnailPool ,VerticalBox);
	}
}

void
FHoudiniInputDetails::Helper_CreateCurveWidget(
	TArray<UHoudiniInput*>& InInputs,
	const int32& InCurveObjectIdx,
	TSharedPtr< FAssetThumbnailPool > AssetThumbnailPool,
	TSharedRef< SVerticalBox > VerticalBox)
{
	UHoudiniInput* MainInput = InInputs[0];

	if (!MainInput || MainInput->IsPendingKill())
		return;

	UHoudiniAssetComponent * OuterHAC = Cast<UHoudiniAssetComponent>(MainInput->GetOuter());
	if (!OuterHAC || OuterHAC->IsPendingKill())
		return;

	auto GetHoudiniSplineComponentAtIndex = [](UHoudiniInput * Input, int32 Index)
	{
		UHoudiniSplineComponent* FoundHoudiniSplineComponent = nullptr;
		if (!Input || Input->IsPendingKill())
			return FoundHoudiniSplineComponent;

		// Get the TArray ptr to the curve objects in this input
		TArray<UHoudiniInputObject*> * CurveInputComponentArray = Input->GetHoudiniInputObjectArray(EHoudiniInputType::Curve);
		if (!CurveInputComponentArray)
			return FoundHoudiniSplineComponent;

		if (!CurveInputComponentArray->IsValidIndex(Index))
			return FoundHoudiniSplineComponent;

		// Access the object used in the corresponding Houdini curve input
		UHoudiniInputObject* HoudiniInputObject = (*CurveInputComponentArray)[Index];
		UHoudiniInputHoudiniSplineComponent* HoudiniSplineInputObject =
			Cast<UHoudiniInputHoudiniSplineComponent>(HoudiniInputObject);

		FoundHoudiniSplineComponent = HoudiniSplineInputObject->GetCurveComponent();

		return FoundHoudiniSplineComponent;
	};


	// Get the TArray ptr to the curve objects in this input
	TArray<UHoudiniInputObject*> * CurveInputComponentArray = MainInput->GetHoudiniInputObjectArray(EHoudiniInputType::Curve);
	if (!CurveInputComponentArray)
		return;

	if (!CurveInputComponentArray->IsValidIndex(InCurveObjectIdx))
		return;

	// Access the object used in the corresponding Houdini curve input
	UHoudiniInputObject* HoudiniInputObject = (*CurveInputComponentArray)[InCurveObjectIdx];
	UHoudiniInputHoudiniSplineComponent* HoudiniSplineInputObject =
		Cast<UHoudiniInputHoudiniSplineComponent>(HoudiniInputObject);

	UHoudiniSplineComponent * HoudiniSplineComponent = HoudiniSplineInputObject->GetCurveComponent();
	if (!HoudiniSplineComponent)
		return;

	FString HoudiniSplineName = HoudiniSplineComponent->GetHoudiniSplineName();

	// Editable label for the current Houdini curve
	TSharedPtr <SHorizontalBox> LabelHorizontalBox;
	VerticalBox->AddSlot()
		.Padding(0, 2)
		.AutoHeight()
		[
			SAssignNew(LabelHorizontalBox, SHorizontalBox)
		];

	TSharedPtr <SEditableText> LabelBlock;
	LabelHorizontalBox->AddSlot()
		.Padding(0, 15, 0, 2)
		.MaxWidth(150.f)
		.FillWidth(150.f)
		.VAlign(VAlign_Bottom)
		.HAlign(HAlign_Left)
		[
			SAssignNew(LabelBlock, SEditableText)
			.Text(FText::FromString(HoudiniSplineName))
		.OnTextCommitted_Lambda([HoudiniSplineComponent](FText NewText, ETextCommit::Type CommitType)
	{
		if (CommitType == ETextCommit::Type::OnEnter)
		{
			HoudiniSplineComponent->SetHoudiniSplineName(NewText.ToString());
		}
	})
		];

	// Lambda for deleting the current curve input
	auto DeleteHoudiniCurveAtIndex = [InInputs, InCurveObjectIdx, OuterHAC, CurveInputComponentArray]()
	{
		// Record a transaction for undo/redo.
		FScopedTransaction Transaction(FText::FromString("Modifying Houdini Input: Deleting a curve input."));
		OuterHAC->Modify();

		int MainInputCurveArraySize = CurveInputComponentArray->Num();
		for (auto & Input : InInputs)
		{
			if (!Input || Input->IsPendingKill())
				continue;

			TArray<UHoudiniInputObject*>* InputObjectArr = Input->GetHoudiniInputObjectArray(EHoudiniInputType::Curve);
			if (!InputObjectArr)
				continue;

			if (!InputObjectArr->IsValidIndex(InCurveObjectIdx))
				continue;

			if (MainInputCurveArraySize != InputObjectArr->Num())
				continue;

			UHoudiniInputHoudiniSplineComponent* HoudiniInput =
				Cast<UHoudiniInputHoudiniSplineComponent>((*InputObjectArr)[InCurveObjectIdx]);
			if (!HoudiniInput)
				return;

			UHoudiniSplineComponent* HoudiniSplineComponent = HoudiniInput->GetCurveComponent();
			if (!HoudiniSplineComponent)
				return;

			// Clear the insert objects buffer before transaction.
			//Input->LastInsertedInputs.Empty();

			// Detach the spline component before delete.
			FDetachmentTransformRules DetachTransRules(EDetachmentRule::KeepRelative, EDetachmentRule::KeepRelative, EDetachmentRule::KeepRelative, false);
			HoudiniSplineComponent->DetachFromComponent(DetachTransRules);

			// This input is marked changed when an input component is deleted.
			Input->DeleteInputObjectAt(EHoudiniInputType::Curve, InCurveObjectIdx);
		}

		FHoudiniEngineUtils::UpdateEditorProperties(OuterHAC, true);
	};

	// Add delete button UI
	LabelHorizontalBox->AddSlot().Padding(0, 2, 0, 2).HAlign(HAlign_Right).VAlign(VAlign_Bottom).AutoWidth()
		[
			PropertyCustomizationHelpers::MakeEmptyButton(FSimpleDelegate::CreateLambda([DeleteHoudiniCurveAtIndex]()
	{
		return DeleteHoudiniCurveAtIndex();
	}))
		];


	TSharedPtr <SHorizontalBox> HorizontalBox = NULL;
	VerticalBox->AddSlot().Padding(0, 2).AutoHeight()[SAssignNew(HorizontalBox, SHorizontalBox)];

	// Closed check box
	// Lambda returning a closed state
	auto IsCheckedClosedCurve = [HoudiniSplineComponent]()
	{
		return HoudiniSplineComponent->IsClosedCurve() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	};

	// Lambda for changing Closed state
	auto CheckStateChangedClosedCurve = [GetHoudiniSplineComponentAtIndex, InInputs, InCurveObjectIdx, OuterHAC](ECheckBoxState NewState)
	{
		bool bNewState = (NewState == ECheckBoxState::Checked);
		
		// Record a transaction for undo/redo.
		FScopedTransaction Transaction(FText::FromString("Modifying Houdini Input: Change a curve parameter."));
		OuterHAC->Modify();

		for (auto & Input : InInputs) 
		{
			if (!Input || Input->IsPendingKill())
				continue;

			UHoudiniSplineComponent * HoudiniSplineComponent = GetHoudiniSplineComponentAtIndex(Input, InCurveObjectIdx);

			if (!HoudiniSplineComponent)
				continue;

			if (HoudiniSplineComponent->IsClosedCurve() == bNewState)
				continue;

			HoudiniSplineComponent->SetClosedCurve(bNewState);
			HoudiniSplineComponent->MarkChanged(true);
			HoudiniSplineComponent->MarkInputObjectChanged();
		}
	};

	// Add Closed check box UI
	TSharedPtr <SCheckBox> CheckBoxClosed = NULL;
	HorizontalBox->AddSlot().Padding(0, 2).AutoWidth()
		[
			SAssignNew(CheckBoxClosed, SCheckBox).Content()
			[
				SNew(STextBlock).Text(LOCTEXT("ClosedCurveCheckBox", "Closed"))
				.ToolTipText(LOCTEXT("ClosedCurveCheckboxTip", "Close this input curve."))
		.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			]
	.IsChecked_Lambda([IsCheckedClosedCurve]()
	{
		return IsCheckedClosedCurve();
	})
		.OnCheckStateChanged_Lambda([CheckStateChangedClosedCurve](ECheckBoxState NewState)
	{
		return CheckStateChangedClosedCurve(NewState);
	})
		];

	// Reversed check box
	// Lambda returning a reversed state
	auto IsCheckedReversedCurve = [HoudiniSplineComponent]()
	{
		return HoudiniSplineComponent->IsReversed() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	};

	// Lambda for changing reversed state
	auto CheckStateChangedReversedCurve = [GetHoudiniSplineComponentAtIndex, InInputs, InCurveObjectIdx, OuterHAC](ECheckBoxState NewState)
	{
		bool bNewState = (NewState == ECheckBoxState::Checked);

		// Record a transaction for undo/redo.
		FScopedTransaction Transaction(FText::FromString("Modifying Houdini Input: Change a curve parameter."));
		OuterHAC->Modify();

		for (auto & Input : InInputs) 
		{
			if (!Input || Input->IsPendingKill())
				continue;

			UHoudiniSplineComponent * HoudiniSplineComponent = GetHoudiniSplineComponentAtIndex(Input, InCurveObjectIdx);
			if (!HoudiniSplineComponent)
				continue;

			if (HoudiniSplineComponent->IsReversed() == bNewState)
				continue;

			HoudiniSplineComponent->SetReversed(bNewState);
			HoudiniSplineComponent->MarkChanged(true);
			HoudiniSplineComponent->MarkInputObjectChanged();
		}
	};

	// Add reversed check box UI
	TSharedPtr <SCheckBox> CheckBoxReversed = NULL;
	HorizontalBox->AddSlot()
		.Padding(2, 2)
		.AutoWidth()
		[
			SAssignNew(CheckBoxReversed, SCheckBox).Content()
			[
				SNew(STextBlock).Text(LOCTEXT("ReversedCurveCheckBox", "Reversed"))
				.ToolTipText(LOCTEXT("ReversedCurveCheckboxTip", "Reverse this input curve."))
		.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			]
	.IsChecked_Lambda([IsCheckedReversedCurve]()
	{
		return IsCheckedReversedCurve();
	})
		.OnCheckStateChanged_Lambda([CheckStateChangedReversedCurve](ECheckBoxState NewState)
	{
		return CheckStateChangedReversedCurve(NewState);
	})
		];

	// Visible check box
	// Lambda returning a visible state
	auto IsCheckedVisibleCurve = [HoudiniSplineComponent]()
	{
		return HoudiniSplineComponent->IsHoudiniSplineVisible() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	};

	// Lambda for changing visible state
	auto CheckStateChangedVisibleCurve = [GetHoudiniSplineComponentAtIndex, InInputs, OuterHAC, InCurveObjectIdx](ECheckBoxState NewState)
	{
		bool bNewState = (NewState == ECheckBoxState::Checked);

		for (auto & Input : InInputs) 
		{
			if (!Input || Input->IsPendingKill())
				continue;

			UHoudiniSplineComponent * HoudiniSplineComponent = GetHoudiniSplineComponentAtIndex(Input, InCurveObjectIdx);
			if (!HoudiniSplineComponent)
				continue;

			if (HoudiniSplineComponent->IsHoudiniSplineVisible() == bNewState)
				return;

			HoudiniSplineComponent->SetHoudiniSplineVisible(bNewState);
		}
	};

	// Add visible check box UI
	TSharedPtr <SCheckBox> CheckBoxVisible = NULL;
	HorizontalBox->AddSlot().Padding(2, 2).AutoWidth()
		[
			SAssignNew(CheckBoxVisible, SCheckBox).Content()
			[
				SNew(STextBlock).Text(LOCTEXT("VisibleCurveCheckBox", "Visible"))
				.ToolTipText(LOCTEXT("VisibleCurveCheckboxTip", "Set the visibility of this curve."))
		.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			]
	.IsChecked_Lambda([IsCheckedVisibleCurve]()
	{
		return IsCheckedVisibleCurve();
	})
		.OnCheckStateChanged_Lambda([CheckStateChangedVisibleCurve](ECheckBoxState NewState)
	{
		return CheckStateChangedVisibleCurve(NewState);
	})
		];

	// Curve type comboBox
	// Lambda for changing Houdini curve type
	auto OnCurveTypeChanged = [GetHoudiniSplineComponentAtIndex, InInputs, InCurveObjectIdx, OuterHAC](TSharedPtr<FString> InNewChoice)
	{
		if (!InNewChoice.IsValid())
			return;

		EHoudiniCurveType NewInputType = UHoudiniInput::StringToHoudiniCurveType(*InNewChoice.Get());

		// Record a transaction for undo/redo.
		FScopedTransaction Transaction(FText::FromString("Modifying Houdini Input: Change a curve parameter."));
		OuterHAC->Modify();

		for (auto & Input : InInputs) 
		{
			if (!Input || Input->IsPendingKill())
				continue;

			UHoudiniSplineComponent * HoudiniSplineComponent = GetHoudiniSplineComponentAtIndex(Input, InCurveObjectIdx);
			if (!HoudiniSplineComponent)
				continue;

			if (HoudiniSplineComponent->GetCurveType() == NewInputType)
				continue;

			HoudiniSplineComponent->SetCurveType(NewInputType);
			HoudiniSplineComponent->MarkChanged(true);
			HoudiniSplineComponent->MarkInputObjectChanged();
		}
	};

	// Lambda for getting Houdini curve type 
	auto GetCurveTypeText = [HoudiniSplineComponent]()
	{
		return FText::FromString(FHoudiniEngineEditorUtils::HoudiniCurveTypeToString(HoudiniSplineComponent->GetCurveType()));
	};

	// Add curve type combo box UI
	TSharedPtr<SHorizontalBox> CurveTypeHorizontalBox;
	VerticalBox->AddSlot()
		.Padding(0, 2, 2, 0)
		.AutoHeight()
		[
			SAssignNew(CurveTypeHorizontalBox, SHorizontalBox)
		];

	// Add curve type label UI
	CurveTypeHorizontalBox->AddSlot().Padding(0, 10, 0, 2).AutoWidth()
		[
			SNew(STextBlock).Text(LOCTEXT("CurveTypeText", "Curve Type     "))
			.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
		];

	TSharedPtr < SComboBox < TSharedPtr < FString > > > ComboBoxCurveType;
	CurveTypeHorizontalBox->AddSlot()
		.Padding(2, 2, 5, 2)
		.FillWidth(150.f)
		.MaxWidth(150.f)
		[
			SAssignNew(ComboBoxCurveType, SComboBox<TSharedPtr<FString>>)
			.OptionsSource(FHoudiniEngineEditor::Get().GetHoudiniCurveTypeChoiceLabels())
		.InitiallySelectedItem((*FHoudiniEngineEditor::Get().GetHoudiniCurveTypeChoiceLabels())[(int)HoudiniSplineComponent->GetCurveType()])
		.OnGenerateWidget_Lambda([](TSharedPtr<FString> ChoiceEntry)
	{
		FText ChoiceEntryText = FText::FromString(*ChoiceEntry);
		return SNew(STextBlock)
			.Text(ChoiceEntryText)
			.ToolTipText(ChoiceEntryText)
			.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")));
	})
		.OnSelectionChanged_Lambda([OnCurveTypeChanged](TSharedPtr<FString> NewChoice, ESelectInfo::Type SelectType)
	{
		return OnCurveTypeChanged(NewChoice);
	})
		[
			SNew(STextBlock)
			.Text_Lambda([GetCurveTypeText]()
	{
		return GetCurveTypeText();
	})
		.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
		]
		];

	// Houdini curve method combo box
	// Lambda for changing Houdini curve method
	auto OnCurveMethodChanged = [GetHoudiniSplineComponentAtIndex, InInputs, InCurveObjectIdx, OuterHAC](TSharedPtr<FString> InNewChoice)
	{
		if (!InNewChoice.IsValid())
			return;

		EHoudiniCurveMethod NewInputMethod = UHoudiniInput::StringToHoudiniCurveMethod(*InNewChoice.Get());

		// Record a transaction for undo/redo.
		FScopedTransaction Transaction(FText::FromString("Modifying Houdini Input: Change a curve parameter."));
		OuterHAC->Modify();

		for (auto & Input : InInputs)
		{
			if (!Input || Input->IsPendingKill())
				continue;

			UHoudiniSplineComponent * HoudiniSplineComponent = GetHoudiniSplineComponentAtIndex(Input, InCurveObjectIdx);
			if (!HoudiniSplineComponent)
				continue;

			if (HoudiniSplineComponent->GetCurveMethod() == NewInputMethod)
				return;

			HoudiniSplineComponent->SetCurveMethod(NewInputMethod);
			HoudiniSplineComponent->MarkChanged(true);
			HoudiniSplineComponent->MarkInputObjectChanged();
		}
	};

	// Lambda for getting Houdini curve method 
	auto GetCurveMethodText = [HoudiniSplineComponent]()
	{
		return FText::FromString(FHoudiniEngineEditorUtils::HoudiniCurveMethodToString(HoudiniSplineComponent->GetCurveMethod()));
	};

	// Add curve method combo box UI
	TSharedPtr< SHorizontalBox > CurveMethodHorizontalBox;
	VerticalBox->AddSlot().Padding(0, 2, 2, 0).AutoHeight()[SAssignNew(CurveMethodHorizontalBox, SHorizontalBox)];

	// Add curve method label UI
	CurveMethodHorizontalBox->AddSlot().Padding(0, 10, 0, 2).AutoWidth()
		[
			SNew(STextBlock).Text(LOCTEXT("CurveMethodText", "Curve Method "))
			.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
		];

	TSharedPtr < SComboBox < TSharedPtr < FString > > > ComboBoxCurveMethod;
	CurveMethodHorizontalBox->AddSlot().Padding(2, 2, 5, 2).FillWidth(150.f).MaxWidth(150.f)
		[
			SAssignNew(ComboBoxCurveMethod, SComboBox<TSharedPtr<FString>>)
			.OptionsSource(FHoudiniEngineEditor::Get().GetHoudiniCurveMethodChoiceLabels())
		.InitiallySelectedItem((*FHoudiniEngineEditor::Get().GetHoudiniCurveMethodChoiceLabels())[(int)HoudiniSplineComponent->GetCurveMethod()])
		.OnGenerateWidget_Lambda([](TSharedPtr<FString> ChoiceEntry)
	{
		FText ChoiceEntryText = FText::FromString(*ChoiceEntry);
		return SNew(STextBlock)
			.Text(ChoiceEntryText)
			.ToolTipText(ChoiceEntryText)
			.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")));
	})
		.OnSelectionChanged_Lambda([OnCurveMethodChanged](TSharedPtr<FString> NewChoice, ESelectInfo::Type SelectType)
	{
		return OnCurveMethodChanged(NewChoice);
	})
		[
			SNew(STextBlock)
			.Text_Lambda([GetCurveMethodText]()
	{
		return GetCurveMethodText();
	})
		.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
		]
		];

	auto BakeInputCurveLambda = [](TArray<UHoudiniInput*> Inputs, int32 Index, bool bBakeToBlueprint)
	{
		for (auto & NextInput : Inputs)
		{
			if (!NextInput || NextInput->IsPendingKill())
				continue;

			UHoudiniAssetComponent * OuterHAC = Cast<UHoudiniAssetComponent>(NextInput->GetOuter());
			if (!OuterHAC || OuterHAC->IsPendingKill())
				continue;

			AActor * OwnerActor = OuterHAC->GetOwner();
			if (!OwnerActor || OwnerActor->IsPendingKill())
				continue;

			TArray<UHoudiniInputObject*> * CurveInputComponentArray = NextInput->GetHoudiniInputObjectArray(EHoudiniInputType::Curve);
			if (!CurveInputComponentArray)
				continue;

			if (!CurveInputComponentArray->IsValidIndex(Index))
				continue;

			UHoudiniInputObject* HoudiniInputObject = (*CurveInputComponentArray)[Index];
			UHoudiniInputHoudiniSplineComponent* HoudiniSplineInputObject =
				Cast<UHoudiniInputHoudiniSplineComponent>(HoudiniInputObject);

			if (!HoudiniSplineInputObject || HoudiniSplineInputObject->IsPendingKill())
				continue;

			if (!HoudiniSplineInputObject->MyHoudiniSplineComponent || HoudiniSplineInputObject->MyHoudiniSplineComponent->IsPendingKill())
				continue;

			FHoudiniPackageParams PackageParams;
			PackageParams.BakeFolder = OuterHAC->BakeFolder.Path;
			PackageParams.HoudiniAssetName = OuterHAC->GetName();
			PackageParams.GeoId = NextInput->GetAssetNodeId();
			PackageParams.PackageMode = EPackageMode::Bake;
			PackageParams.ObjectId = Index;
			PackageParams.ObjectName = OwnerActor->GetName() + "InputHoudiniSpline" + FString::FromInt(Index);

			if (bBakeToBlueprint) 
			{
				FHoudiniEngineBakeUtils::BakeInputHoudiniCurveToBlueprint(
					HoudiniSplineInputObject->MyHoudiniSplineComponent,
					PackageParams,
					OwnerActor->GetWorld(), OwnerActor->GetActorTransform());
			}
			else
			{
				FHoudiniEngineBakeUtils::BakeInputHoudiniCurveToActor(
					HoudiniSplineInputObject->MyHoudiniSplineComponent,
					PackageParams,
					OwnerActor->GetWorld(), OwnerActor->GetActorTransform());
			}
		}

		return FReply::Handled();
	};

	// Add input curve bake button
	TSharedPtr< SHorizontalBox > InputCurveBakeHorizontalBox;
	VerticalBox->AddSlot().Padding(0, 2, 2, 0).AutoHeight()[SAssignNew(InputCurveBakeHorizontalBox, SHorizontalBox)];
	VerticalBox->AddSlot()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().MaxWidth(110.f)
		[
			SNew(SButton)
			.VAlign(VAlign_Center)
		.HAlign(HAlign_Center)
		.Text(LOCTEXT("HoudiniInputCurveBakeToActorButton", "Bake to Actor"))
		.IsEnabled(true)
		.OnClicked_Lambda([InInputs, InCurveObjectIdx, BakeInputCurveLambda]()
		{
			return BakeInputCurveLambda(InInputs, InCurveObjectIdx, false);
		})
		.ToolTipText(LOCTEXT("HoudiniInputCurveBakeToActorButtonToolTip", "Bake this input curve to Actor"))
		]

	+ SHorizontalBox::Slot().MaxWidth(110.f)
		[
			SNew(SButton)
			.VAlign(VAlign_Center)
		.HAlign(HAlign_Center)
		.Text(LOCTEXT("HoudiniInputCurveBakeToBPButton", "Bake to Blueprint"))
		.IsEnabled(true)
		.OnClicked_Lambda([InInputs, InCurveObjectIdx, BakeInputCurveLambda]()
		{
			return BakeInputCurveLambda(InInputs, InCurveObjectIdx, true);
		})
		.ToolTipText(LOCTEXT("HoudiniInputCurveBakeToBPButtonToolTip", "Bake this input curve to Blueprint"))
		]
	];

	// Do we actually need to set enable the UI components?
	if (MainInput->GetInputType() == EHoudiniInputType::Curve) 
	{
		LabelBlock->SetEnabled(true);
		CheckBoxClosed->SetEnabled(true);
		CheckBoxReversed->SetEnabled(true);
		CheckBoxVisible->SetEnabled(true);
		ComboBoxCurveType->SetEnabled(true);
		ComboBoxCurveMethod->SetEnabled(true);
	}
	else 
	{
		LabelBlock->SetEnabled(false);
		CheckBoxClosed->SetEnabled(false);
		CheckBoxReversed->SetEnabled(false);
		CheckBoxVisible->SetEnabled(false);
		ComboBoxCurveType->SetEnabled(false);
		ComboBoxCurveMethod->SetEnabled(false);
	}
}

void
FHoudiniInputDetails::AddLandscapeInputUI(TSharedRef<SVerticalBox> VerticalBox, TArray<UHoudiniInput*>& InInputs)
{
	if (InInputs.Num() <= 0)
		return;

	UHoudiniInput* MainInput = InInputs[0];
	if (!MainInput)
		return;

	// Lambda returning a CheckState from the input's current KeepWorldTransform state
	auto IsCheckedUpdateInputLandscape = [](UHoudiniInput* InInput)
	{
		return InInput->GetUpdateInputLandscape() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	};

	// Lambda for changing KeepWorldTransform state
	auto CheckStateChangedUpdateInputLandscape = [](TArray<UHoudiniInput*> InInputsToUpdate, ECheckBoxState NewState)
	{
		bool bNewState = (NewState == ECheckBoxState::Checked);
		for (auto CurInput : InInputsToUpdate)
		{
			if (bNewState == CurInput->GetUpdateInputLandscape())
				continue;

			// Record a transaction for undo/redo
			FScopedTransaction Transaction(
				TEXT(HOUDINI_MODULE_EDITOR),
				LOCTEXT("HoudiniInputChange", "Houdini Input: Changing Keep World Transform"),
				CurInput->GetOuter());
			CurInput->Modify();

			UHoudiniAssetComponent* HAC = Cast<UHoudiniAssetComponent>(CurInput->GetOuter());
			if (!HAC)
				continue;

			TArray<UHoudiniInputObject*>* LandscapeInputObjects = CurInput->GetHoudiniInputObjectArray(CurInput->GetInputType());
			if (!LandscapeInputObjects)
				continue;

			for (UHoudiniInputObject* NextInputObj : *LandscapeInputObjects)
			{
				UHoudiniInputLandscape* CurrentInputLandscape = Cast<UHoudiniInputLandscape>(NextInputObj);
				if (!CurrentInputLandscape)
					continue;

				ALandscapeProxy* CurrentInputLandscapeProxy = CurrentInputLandscape->GetLandscapeProxy();
				if (!CurrentInputLandscapeProxy)
					continue;

				if (bNewState)
				{
					// We want to update this landscape data directly, start by backing it up to image files in the temp folder
					FString BackupBaseName = HAC->TemporaryCookFolder.Path
						+ TEXT("/")
						+ CurrentInputLandscapeProxy->GetName()
						+ TEXT("_")
						+ HAC->GetComponentGUID().ToString().Left(FHoudiniEngineUtils::PackageGUIDComponentNameLength);

					// We need to cache the input landscape to a file
					FHoudiniLandscapeTranslator::BackupLandscapeToImageFiles(BackupBaseName, CurrentInputLandscapeProxy);
					
					// Cache its transform on the input
					CurrentInputLandscape->CachedInputLandscapeTraqnsform = CurrentInputLandscapeProxy->ActorToWorld();

					HAC->SetMobility(EComponentMobility::Static);
					CurrentInputLandscapeProxy->AttachToComponent(HAC, FAttachmentTransformRules::KeepWorldTransform);
				}
				else
				{
					// We are not updating this input landscape anymore, detach it and restore its backed-up values
					CurrentInputLandscapeProxy->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

					// Restore the input landscape's backup data
					FHoudiniLandscapeTranslator::RestoreLandscapeFromImageFiles(CurrentInputLandscapeProxy);

					// Reapply the source Landscape's transform
					CurrentInputLandscapeProxy->SetActorTransform(CurrentInputLandscape->CachedInputLandscapeTraqnsform);

					// TODO: 
					// Clear the input obj map?
				}
			}

			CurInput->bUpdateInputLandscape = (NewState == ECheckBoxState::Checked);
			CurInput->MarkChanged(true);
		}
	};

	// CheckBox : Update Input Landscape Data
	TSharedPtr< SCheckBox > CheckBoxUpdateInput;
	VerticalBox->AddSlot().Padding(2, 2, 5, 2).AutoHeight()
	[
		SAssignNew( CheckBoxUpdateInput, SCheckBox).Content()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("LandscapeUpdateInputCheckbox", "Update Input Landscape Data"))
			.ToolTipText(LOCTEXT("LandscapeSelectedTooltip", "If enabled, the input landscape's data will be updated instead of creating a new landscape Actor"))
			.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
		]
		.IsChecked_Lambda([IsCheckedUpdateInputLandscape, MainInput]()
		{
			return IsCheckedUpdateInputLandscape(MainInput);
		})
		.OnCheckStateChanged_Lambda([CheckStateChangedUpdateInputLandscape, InInputs](ECheckBoxState NewState)
		{
			return CheckStateChangedUpdateInputLandscape(InInputs, NewState);
		})
	];
	
	// Actor picker: Landscape.
	FMenuBuilder MenuBuilder = Helper_CreateLandscapePickerWidget(InInputs);
	VerticalBox->AddSlot().Padding(2, 2, 5, 2).AutoHeight()
	[
		MenuBuilder.MakeWidget()
	];

	// Checkboxes : Export landscape as Heightfield/Mesh/Points
	{
		VerticalBox->AddSlot().Padding(2, 2, 5, 2).AutoHeight()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("LandscapeExportAs", "Export Landscape As"))
			.ToolTipText(LOCTEXT("LandscapeExportAsToolTip", "Choose the type of data you want the ladscape to be exported to:\n * Heightfield\n * Mesh\n * Points"))
			.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
		];
	
		TSharedPtr <SUniformGridPanel> ButtonOptionsPanel;
		VerticalBox->AddSlot().Padding(5, 2, 5, 2).AutoHeight()
		[
			SAssignNew(ButtonOptionsPanel, SUniformGridPanel)
		];

		auto IsCheckedExportAs = [](UHoudiniInput* Input, const EHoudiniLandscapeExportType& LandscapeExportType)
		{
			if (Input && Input->GetLandscapeExportType() == LandscapeExportType)
				return ECheckBoxState::Checked;
			else
				return ECheckBoxState::Unchecked;
		};

		auto CheckStateChangedExportAs = [](UHoudiniInput* Input, const EHoudiniLandscapeExportType& LandscapeExportType)
		{
			if (!Input)
				return false;

			if (Input->GetLandscapeExportType() == LandscapeExportType)
				return false;

			// Record a transaction for undo/redo
			FScopedTransaction Transaction(
				TEXT(HOUDINI_MODULE_EDITOR),
				LOCTEXT("HoudiniInputChange", "Houdini Input: Changed Landscape export type."),
				Input->GetOuter());
			Input->Modify();

			Input->SetLandscapeExportType(LandscapeExportType);
			Input->SetHasLandscapeExportTypeChanged(true);
			Input->MarkChanged(true);

			TArray<UHoudiniInputObject*>* LandscapeInputObjectsArray = Input->GetHoudiniInputObjectArray(Input->GetInputType());
			if (!LandscapeInputObjectsArray)
				return true;

			for (UHoudiniInputObject *NextInputObj : *LandscapeInputObjectsArray)
			{
				if (!NextInputObj)
					continue;
				NextInputObj->MarkChanged(true);
			}

			return true;
		};
		
		// Heightfield
		FText HeightfieldTooltip = LOCTEXT("LandscapeExportAsHeightfieldTooltip", "If enabled, the landscape will be exported to Houdini as a heightfield.");
		ButtonOptionsPanel->AddSlot(0, 0)
		[
			SNew(SCheckBox)
			.Style(FEditorStyle::Get(), "Property.ToggleButton.Start")
			.IsChecked_Lambda([IsCheckedExportAs, MainInput]()
			{
				return IsCheckedExportAs(MainInput, EHoudiniLandscapeExportType::Heightfield);
			})
			.OnCheckStateChanged_Lambda([CheckStateChangedExportAs, InInputs](ECheckBoxState NewState)
			{
				for(auto CurrentInput : InInputs)
					CheckStateChangedExportAs(CurrentInput, EHoudiniLandscapeExportType::Heightfield);
			})
			.ToolTipText(HeightfieldTooltip)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(2, 2)
				[
					SNew(SImage)
					.Image(FEditorStyle::GetBrush("ClassIcon.LandscapeComponent"))
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				.Padding(2, 2)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("LandscapeExportAsHeightfieldCheckbox", "Heightfield"))
					.ToolTipText(HeightfieldTooltip)
					.Font(IDetailLayoutBuilder::GetDetailFont())
				]
			]
		];

		// Mesh
		FText MeshTooltip = LOCTEXT("LandscapeExportAsHeightfieldTooltip", "If enabled, the landscape will be exported to Houdini as a mesh.");
		ButtonOptionsPanel->AddSlot(1, 0)
		[
			SNew(SCheckBox)
			.Style(FEditorStyle::Get(), "Property.ToggleButton.Middle")
			.IsChecked_Lambda([IsCheckedExportAs, MainInput]()
			{
				return IsCheckedExportAs(MainInput, EHoudiniLandscapeExportType::Mesh);
			})
			.OnCheckStateChanged_Lambda([CheckStateChangedExportAs, InInputs](ECheckBoxState NewState)
			{
				for (auto CurrentInput : InInputs)
					CheckStateChangedExportAs(CurrentInput, EHoudiniLandscapeExportType::Mesh);
			})
			.ToolTipText(MeshTooltip)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(2, 2)
				[
					SNew(SImage)
					.Image(FEditorStyle::GetBrush("ClassIcon.StaticMeshComponent"))
				]

				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				.Padding(2, 2)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("LandscapeExportAsMeshCheckbox", "Mesh"))
					.ToolTipText(MeshTooltip)
					.Font(IDetailLayoutBuilder::GetDetailFont())
				]
			]
		];

		// Points
		FText PointsTooltip = LOCTEXT("LandscapeExportAsPointsTooltip", "If enabled, the landscape will be exported to Houdini as points.");
		ButtonOptionsPanel->AddSlot(2, 0)
		[
			SNew(SCheckBox)
			.Style(FEditorStyle::Get(), "Property.ToggleButton.End")
			.IsChecked_Lambda([IsCheckedExportAs, MainInput]()
			{
				return IsCheckedExportAs(MainInput, EHoudiniLandscapeExportType::Points);
			})
			.OnCheckStateChanged_Lambda([CheckStateChangedExportAs, InInputs](ECheckBoxState NewState)
			{
				for (auto CurrentInput : InInputs)
					CheckStateChangedExportAs(CurrentInput, EHoudiniLandscapeExportType::Points);
			})
			.ToolTipText(PointsTooltip)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(2, 2)
				[
					SNew(SImage)
					.Image(FEditorStyle::GetBrush("Mobility.Static"))
				]

				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				.Padding(2, 2)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("LandscapeExportAsPointsCheckbox", "Points"))
					.ToolTipText(PointsTooltip)
					.Font(IDetailLayoutBuilder::GetDetailFont())
				]
			]
		];
	}

	// CheckBox : Export selected components only
	{
		TSharedPtr< SCheckBox > CheckBoxExportSelected;
		VerticalBox->AddSlot().Padding(2, 2, 5, 2).AutoHeight()
		[
			SAssignNew(CheckBoxExportSelected, SCheckBox)
			.Content()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("LandscapeSelectedCheckbox", "Export Selected Landscape Components Only"))
				.ToolTipText(LOCTEXT("LandscapeSelectedTooltip", "If enabled, only the selected Landscape Components will be exported."))
				.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			]
			.IsChecked_Lambda([MainInput]()
			{
				return MainInput->bLandscapeExportSelectionOnly ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			})
			.OnCheckStateChanged_Lambda([InInputs](ECheckBoxState NewState)
			{
				for (auto CurrentInput : InInputs)
				{
					if (!CurrentInput)
						continue;

					bool bNewState = (NewState == ECheckBoxState::Checked);
					if (bNewState == CurrentInput->bLandscapeExportSelectionOnly)
						continue;

					// Record a transaction for undo/redo
					FScopedTransaction Transaction(
						TEXT(HOUDINI_MODULE_EDITOR),
						LOCTEXT("HoudiniInputChange", "Houdini Input: Changed Landscape export only selected component."),
						CurrentInput->GetOuter());
					CurrentInput->Modify();

					CurrentInput->bLandscapeExportSelectionOnly = bNewState;
					CurrentInput->MarkChanged(true);
				}
			})
		];
	}

	// Checkbox:  auto select components
	{		
		TSharedPtr< SCheckBox > CheckBoxAutoSelectComponents;
		VerticalBox->AddSlot().Padding(10, 2, 5, 2).AutoHeight()
		[
			SAssignNew(CheckBoxAutoSelectComponents, SCheckBox)
			.Content()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("AutoSelectComponentCheckbox", "Auto-select component in asset bounds"))
				.ToolTipText(LOCTEXT("AutoSelectComponentCheckboxTooltip", "If enabled, when no Landscape components are curremtly selected, the one within the asset's bounding box will be exported."))
				.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			]
			.IsChecked_Lambda([MainInput]()
			{
				return MainInput->bLandscapeAutoSelectComponent ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			})
			.OnCheckStateChanged_Lambda([InInputs](ECheckBoxState NewState)
			{
				for (auto CurrentInput : InInputs)
				{
					if (!CurrentInput)
						continue;

					bool bNewState = (NewState == ECheckBoxState::Checked);
					if (bNewState == CurrentInput->bLandscapeAutoSelectComponent)
						continue;

					// Record a transaction for undo/redo
					FScopedTransaction Transaction(
						TEXT(HOUDINI_MODULE_EDITOR),
						LOCTEXT("HoudiniInputChange", "Houdini Input: Changed Landscape input auto-selects components."),
						CurrentInput->GetOuter());
					CurrentInput->Modify();

					CurrentInput->bLandscapeAutoSelectComponent = bNewState;
					CurrentInput->MarkChanged(true);
				}
			})
		];

		// Enable only when exporting selection	or when exporting heighfield (for now)
		bool bEnable = false;
		for (auto CurrentInput : InInputs)
		{
			if (!MainInput->bLandscapeExportSelectionOnly)
				continue;

			bEnable = true;
			break;
		}
		CheckBoxAutoSelectComponents->SetEnabled(bEnable);
	}


	// The following checkbox are only added when not in heightfield mode
	if (MainInput->LandscapeExportType != EHoudiniLandscapeExportType::Heightfield)
	{
		// Checkbox : Export materials
		{
			TSharedPtr< SCheckBox > CheckBoxExportMaterials;
			VerticalBox->AddSlot().Padding(2, 2, 5, 2).AutoHeight()
			[
				SAssignNew(CheckBoxExportMaterials, SCheckBox)
				.Content()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("LandscapeMaterialsCheckbox", "Export Landscape Materials"))
					.ToolTipText(LOCTEXT("LandscapeMaterialsTooltip", "If enabled, the landscape materials will be exported with it."))
					.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				]
				.IsChecked_Lambda([MainInput]()
				{
					return MainInput->bLandscapeExportMaterials ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
				})
				.OnCheckStateChanged_Lambda([InInputs](ECheckBoxState NewState)
				{
					for (auto CurrentInput : InInputs)
					{
						if (!CurrentInput)
							continue;

						bool bNewState = (NewState == ECheckBoxState::Checked);
						if (bNewState == CurrentInput->bLandscapeExportMaterials)
							continue;

						// Record a transaction for undo/redo
						FScopedTransaction Transaction(
							TEXT(HOUDINI_MODULE_EDITOR),
							LOCTEXT("HoudiniInputChange", "Houdini Input: Changed Landscape input export materials."),
							CurrentInput->GetOuter());
						CurrentInput->Modify();

						CurrentInput->bLandscapeExportMaterials = bNewState;
						CurrentInput->MarkChanged(true);
					}
				})
			];

			/*
			// Disable when exporting heightfields
			if (MainInput->LandscapeExportType == EHoudiniLandscapeExportType::Heightfield)
				CheckBoxExportMaterials->SetEnabled(false);
			*/
		}

		// Checkbox : Export Tile UVs
		{
			TSharedPtr< SCheckBox > CheckBoxExportTileUVs;
			VerticalBox->AddSlot().Padding(2, 2, 5, 2).AutoHeight()
			[
				SAssignNew(CheckBoxExportTileUVs, SCheckBox)
				.Content()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("LandscapeTileUVsCheckbox", "Export Landscape Tile UVs"))
					.ToolTipText(LOCTEXT("LandscapeTileUVsTooltip", "If enabled, UVs will be exported separately for each Landscape tile."))
					.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				]
				.IsChecked_Lambda([MainInput]()
				{
					return MainInput->bLandscapeExportTileUVs ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
				})
				.OnCheckStateChanged_Lambda([InInputs](ECheckBoxState NewState)
				{
					for (auto CurrentInput : InInputs)
					{
						if (!CurrentInput)
							continue;

						bool bNewState = (NewState == ECheckBoxState::Checked);
						if (bNewState == CurrentInput->bLandscapeExportTileUVs)
							continue;

						// Record a transaction for undo/redo
						FScopedTransaction Transaction(
							TEXT(HOUDINI_MODULE_EDITOR),
							LOCTEXT("HoudiniInputChange", "Houdini Input: Changed Landscape export tile UVs."),
							CurrentInput->GetOuter());
						CurrentInput->Modify();

						CurrentInput->bLandscapeExportTileUVs = bNewState;
						CurrentInput->MarkChanged(true);
					}
				})
			];

			/*
			// Disable when exporting heightfields
			if (MainInput->LandscapeExportType == EHoudiniLandscapeExportType::Heightfield)
				CheckBoxExportTileUVs->SetEnabled(false);
			*/
		}

// Checkbox : Export normalized UVs
		{
		TSharedPtr< SCheckBox > CheckBoxExportNormalizedUVs;
		VerticalBox->AddSlot().Padding(2, 2, 5, 2).AutoHeight()
			[
				SAssignNew(CheckBoxExportNormalizedUVs, SCheckBox)
				.Content()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("LandscapeNormalizedUVsCheckbox", "Export Landscape Normalized UVs"))
			.ToolTipText(LOCTEXT("LandscapeNormalizedUVsTooltip", "If enabled, landscape UVs will be exported in [0, 1]."))
			.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			]
		.IsChecked_Lambda([MainInput]()
		{
			return MainInput->bLandscapeExportNormalizedUVs ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
		})
			.OnCheckStateChanged_Lambda([InInputs](ECheckBoxState NewState)
		{
			for (auto CurrentInput : InInputs)
			{
				if (!CurrentInput)
					continue;

				bool bNewState = (NewState == ECheckBoxState::Checked);
				if (bNewState == CurrentInput->bLandscapeExportNormalizedUVs)
					continue;

				// Record a transaction for undo/redo
				FScopedTransaction Transaction(
					TEXT(HOUDINI_MODULE_EDITOR),
					LOCTEXT("HoudiniInputChange", "Houdini Input: Changed Landscape export normalized UVs."),
					CurrentInput->GetOuter());
				CurrentInput->Modify();

				CurrentInput->bLandscapeExportNormalizedUVs = bNewState;
				CurrentInput->MarkChanged(true);
			}
		})
			];

		/*
		// Disable when exporting heightfields
		if (MainInput->LandscapeExportType == EHoudiniLandscapeExportType::Heightfield)
			CheckBoxExportNormalizedUVs->SetEnabled(false);
		*/
		}

		// Checkbox : Export lighting
		{
			TSharedPtr< SCheckBox > CheckBoxExportLighting;
			VerticalBox->AddSlot().Padding(2, 2, 5, 2).AutoHeight()
				[
					SAssignNew(CheckBoxExportLighting, SCheckBox)
					.Content()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("LandscapeLightingCheckbox", "Export Landscape Lighting"))
				.ToolTipText(LOCTEXT("LandscapeLightingTooltip", "If enabled, lightmap information will be exported with the landscape."))
				.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				]
			.IsChecked_Lambda([MainInput]()
			{
				return MainInput->bLandscapeExportLighting ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			})
				.OnCheckStateChanged_Lambda([InInputs](ECheckBoxState NewState)
			{
				for (auto CurrentInput : InInputs)
				{
					if (!CurrentInput)
						continue;

					bool bNewState = (NewState == ECheckBoxState::Checked);
					if (bNewState == CurrentInput->bLandscapeExportLighting)
						continue;

					// Record a transaction for undo/redo
					FScopedTransaction Transaction(
						TEXT(HOUDINI_MODULE_EDITOR),
						LOCTEXT("HoudiniInputChange", "Houdini Input: Changed Landscape export lighting."),
						CurrentInput->GetOuter());
					CurrentInput->Modify();

					CurrentInput->bLandscapeExportLighting = bNewState;
					CurrentInput->MarkChanged(true);
				}
			})
				];

			/*
			// Disable when exporting heightfields
			if (MainInput->LandscapeExportType == EHoudiniLandscapeExportType::Heightfield)
				CheckBoxExportLighting->SetEnabled(false);
				*/
		}

	}

	// Button : Recommit
	{
		auto OnButtonRecommitClicked = [InInputs]()
		{
			for (auto CurrentInput : InInputs)
			{
				TArray<UHoudiniInputObject*>* LandscapeInputObjectsArray = CurrentInput->GetHoudiniInputObjectArray(CurrentInput->GetInputType());
				if (!LandscapeInputObjectsArray)
					continue;

				for (UHoudiniInputObject* NextLandscapeInput : *LandscapeInputObjectsArray)
				{
					if (!NextLandscapeInput)
						continue;

					NextLandscapeInput->MarkChanged(true);
				}

				CurrentInput->MarkChanged(true);
			}
		
			return FReply::Handled();
		};

		VerticalBox->AddSlot().Padding(2, 2, 5, 2).AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(1, 2, 4, 2)
			[
				SNew(SButton)
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				.Text(LOCTEXT("LandscapeInputRecommit", "Recommit Landscape"))
				.ToolTipText(LOCTEXT("LandscapeInputRecommitTooltip", "Recommits the Landscape to Houdini."))
				.OnClicked_Lambda(OnButtonRecommitClicked)
			]
		];
	}
}

/*
FMenuBuilder
FHoudiniInputDetails::Helper_CreateCustomActorPickerWidget(TArray<UHoudiniInput*>& InInputs, const TAttribute<FText>& HeadingText, const bool& bShowCurrentSelectionSection)
{
	UHoudiniInput* MainInput = InInputs.Num() > 0 ? InInputs[0] : nullptr;

	// Filters are only based on the MainInput
	auto OnShouldFilterLandscape = [](const AActor* const Actor, UHoudiniInput* InInput)
	{
		if (!Actor || Actor->IsPendingKill())
			return false;

		if (!Actor->IsA<ALandscapeProxy>())
			return false;

		ALandscapeProxy* LandscapeProxy = const_cast<ALandscapeProxy *>(Cast<const ALandscapeProxy>(Actor));
		if (!LandscapeProxy)
			return false;

		// Get the landscape's actor
		AActor* OwnerActor = LandscapeProxy->GetOwner();
		
		// Get our Actor
		UHoudiniAssetComponent* MyHAC = Cast<UHoudiniAssetComponent>(InInput->GetOuter());
		AActor* MyOwner = MyHAC ? MyHAC->GetOwner() : nullptr;

		// TODO: FIX ME!
		// IF the landscape is owned by ourself, skip it!
		if (OwnerActor == MyOwner)
			return false;

		return true;
	};

	auto OnShouldFilterWorld = [](const AActor* const Actor, UHoudiniInput* InInput)
	{		
		if (!Actor || Actor->IsPendingKill())
			return false;

		const TArray<UHoudiniInputObject*>* InputObjects = InInput->GetHoudiniInputObjectArray(EHoudiniInputType::World);
		if (!InputObjects)
			return false;

		// Only return actors that are currently selected by our input
		for (const auto& CurInputObject : *InputObjects)
		{
			if (!CurInputObject || CurInputObject->IsPendingKill())
				continue;

			AActor* CurActor = Cast<AActor>(CurInputObject->GetObject());
			if (!CurActor || CurActor->IsPendingKill())
				continue;

			if (CurActor == Actor)
				return true;
		}

		return false;
	};

	auto OnShouldFilterHoudiniAsset = [](const AActor* const Actor, UHoudiniInput* InInput)
	{
		if (!Actor)
			return false;

		// Only return HoudiniAssetActors, but not our HAA
		if (!Actor->IsA<AHoudiniAssetActor>())
			return false;

		// But not our own Asset Actor
		if (const USceneComponent* RootComp = Cast<const USceneComponent>(InInput->GetOuter()))
		{
			if (RootComp && Cast<AHoudiniAssetActor>(RootComp->GetOwner()) != Actor)
				return true;
		}

		return false;
	};

	auto OnShouldFilterActor = [MainInput, OnShouldFilterLandscape, OnShouldFilterWorld, OnShouldFilterHoudiniAsset](const AActor* const Actor)
	{
		if (!MainInput || MainInput->IsPendingKill())
			return true;

		switch (MainInput->GetInputType())
		{
		case EHoudiniInputType::Landscape:
			return OnShouldFilterLandscape(Actor, MainInput);
		case EHoudiniInputType::World:
			return OnShouldFilterWorld(Actor, MainInput);
		case EHoudiniInputType::Asset:
			return OnShouldFilterHoudiniAsset(Actor, MainInput);
		default:
			return true;
		}

		return false;
	};


	// Selection uses the input arrays
	auto OnLandscapeSelected = [](AActor* Actor, UHoudiniInput* Input)
	{
		if (!Actor || !Input)
			return;

		ALandscapeProxy* LandscapeProxy = Cast<ALandscapeProxy>(Actor);
		if (!LandscapeProxy)
			return;

		TArray<UHoudiniInputObject*>* LandscapeInputObjectsArray = Input->GetHoudiniInputObjectArray(Input->GetInputType());
		if (!LandscapeInputObjectsArray)
			return;

		LandscapeInputObjectsArray->Empty();

		FName LandscapeName = MakeUniqueObjectName(Input->GetOuter(), ALandscapeProxy::StaticClass(), TEXT("Landscape"));

		// Create a Houdini Input Object.
		UHoudiniInputObject* NewInputObject = UHoudiniInputLandscape::Create(
			LandscapeProxy, Input, LandscapeName.ToString());

		UHoudiniInputLandscape* LandscapeInput = Cast<UHoudiniInputLandscape>(NewInputObject);
		LandscapeInput->MarkChanged(true);

		LandscapeInputObjectsArray->Add(LandscapeInput);
		Input->MarkChanged(true);
	};

	auto OnHoudiniAssetActorSelected = [](AActor* Actor, UHoudiniInput* Input) 
	{
		if (!Actor || !Input)
			return;

		AHoudiniAssetActor* HoudiniAssetActor = Cast<AHoudiniAssetActor>(Actor);
		if (!HoudiniAssetActor)
			return;

		TArray<UHoudiniInputObject*>* AssetInputObjectsArray = Input->GetHoudiniInputObjectArray(Input->GetInputType());
		if (!AssetInputObjectsArray)
			return;

		AssetInputObjectsArray->Empty();

		FName HoudiniAssetActorName = MakeUniqueObjectName(Input->GetOuter(), AHoudiniAssetActor::StaticClass(), TEXT("HoudiniAsset"));

		// Create a Houdini Asset Input Object
		UHoudiniInputObject* NewInputObject = UHoudiniInputHoudiniAsset::Create(HoudiniAssetActor->GetHoudiniAssetComponent(), Input, HoudiniAssetActorName.ToString());

		UHoudiniInputHoudiniAsset* AssetInput = Cast<UHoudiniInputHoudiniAsset>(NewInputObject);
		AssetInput->MarkChanged(true);

		AssetInputObjectsArray->Add(AssetInput);
		Input->MarkChanged(true);
	};

	auto OnWorldSelected = [](AActor* Actor, UHoudiniInput* Input)
	{
		// Do Nothing
	};

	auto OnActorSelected = [OnLandscapeSelected, OnWorldSelected, OnHoudiniAssetActorSelected](AActor* Actor, TArray<UHoudiniInput*> InInputs)
	{
		for (auto& CurInput : InInputs)
		{
			if (!CurInput || CurInput->IsPendingKill())
				return;

			switch (CurInput->GetInputType())
			{
			case EHoudiniInputType::Landscape:
				return OnLandscapeSelected(Actor, CurInput);
			case EHoudiniInputType::World:
				return OnWorldSelected(Actor, CurInput);
			case EHoudiniInputType::Asset:
				return OnHoudiniAssetActorSelected(Actor, CurInput);
			default:
				return;
			}
		}

		return;
	};
	
	FMenuBuilder MenuBuilder(true, nullptr);
	FOnShouldFilterActor ActorFilter = FOnShouldFilterActor::CreateLambda(OnShouldFilterActor);
	
	if (bShowCurrentSelectionSection) 
	{
		MenuBuilder.BeginSection(NAME_None, LOCTEXT("CurrentActorOperationHeader", "Current Selection"));
		{
			MenuBuilder.AddMenuEntry(
				TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateUObject(MainInput, &UHoudiniInput::GetCurrentSelectionText)),
				TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateUObject(MainInput, &UHoudiniInput::GetCurrentSelectionText)),
				FSlateIcon(),
				FUIAction(),
				NAME_None,
				EUserInterfaceActionType::Button,
				NAME_None);
		}
		MenuBuilder.EndSection();
	}


	MenuBuilder.BeginSection(NAME_None, HeadingText);
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
		TSharedRef< SWidget > MenuWidget =
			SNew(SBox)
			.WidthOverride(SceneOutlinerWindowSize.X)
			.HeightOverride(SceneOutlinerWindowSize.Y)
			[
				SNew(SBorder)
				.BorderImage(FEditorStyle::GetBrush("Menu.Background"))
				[
					SceneOutlinerModule.CreateSceneOutliner(
						InitOptions,
						FOnActorPicked::CreateLambda(OnActorSelected, InInputs))
				]
			];

		MenuBuilder.AddWidget(MenuWidget, FText::GetEmpty(), true);
	}
	MenuBuilder.EndSection();

	return MenuBuilder;
}
*/


FMenuBuilder
FHoudiniInputDetails::Helper_CreateHoudiniAssetPickerWidget(TArray<UHoudiniInput*>& InInputs)
{
	UHoudiniInput* MainInput = InInputs.Num() > 0 ? InInputs[0] : nullptr;
	auto OnShouldFilterHoudiniAsset = [InInputs](const AActor* const Actor)
	{
		if (!Actor)
			return false;

		// Only return HoudiniAssetActors, but not our HAA
		if (!Actor->IsA<AHoudiniAssetActor>())
			return false;

		// But not our selected Asset Actor
		for (auto & NextSelectedInput : InInputs) 
		{
			if (!NextSelectedInput)
				continue;

			const USceneComponent* RootComp = Cast<const USceneComponent>(NextSelectedInput->GetOuter());
			if (RootComp && Cast<AHoudiniAssetActor>(RootComp->GetOwner()) == Actor)
				return false;

		}

		return true;
	};

	// Filters are only based on the MainInput
	auto OnShouldFilterActor = [MainInput, OnShouldFilterHoudiniAsset](const AActor* const Actor)
	{
		if (!MainInput || MainInput->IsPendingKill())
			return true;

		return OnShouldFilterHoudiniAsset(Actor);
	};

	auto OnHoudiniAssetActorSelected = [OnShouldFilterHoudiniAsset](AActor* Actor, UHoudiniInput* Input)
	{
		if (!Actor || !Input)
			return;
		
		AHoudiniAssetActor* HoudiniAssetActor = Cast<AHoudiniAssetActor>(Actor);
		if (!HoudiniAssetActor)
			return;

		// Make sure that the actor is valid for this input
		if (!OnShouldFilterHoudiniAsset(Actor))
			return;

		TArray<UHoudiniInputObject*>* AssetInputObjectsArray = Input->GetHoudiniInputObjectArray(EHoudiniInputType::Asset);
		if (!AssetInputObjectsArray)
			return;

		AssetInputObjectsArray->Empty();

		FName HoudiniAssetActorName = MakeUniqueObjectName(Input->GetOuter(), AHoudiniAssetActor::StaticClass(), TEXT("HoudiniAsset"));

		// Create a Houdini Asset Input Object
		UHoudiniInputObject* NewInputObject = UHoudiniInputHoudiniAsset::Create(HoudiniAssetActor->GetHoudiniAssetComponent(), Input, HoudiniAssetActorName.ToString());

		UHoudiniInputHoudiniAsset* AssetInput = Cast<UHoudiniInputHoudiniAsset>(NewInputObject);
		AssetInput->MarkChanged(true);

		AssetInputObjectsArray->Add(AssetInput);
		Input->MarkChanged(true);
	};

	auto OnActorSelected = [OnHoudiniAssetActorSelected](AActor* Actor, TArray<UHoudiniInput*> InInputs)
	{
		for (auto& CurInput : InInputs)
		{
			if (!CurInput || CurInput->IsPendingKill())
				return;

			OnHoudiniAssetActorSelected(Actor, CurInput);
		}
	};

	FMenuBuilder MenuBuilder(true, nullptr);
	FOnShouldFilterActor ActorFilter = FOnShouldFilterActor::CreateLambda(OnShouldFilterActor);

	// Show current selection
	MenuBuilder.BeginSection(NAME_None, LOCTEXT("CurrentActorOperationHeader", "Current Selection"));
	{
		MenuBuilder.AddMenuEntry(
			TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateUObject(MainInput, &UHoudiniInput::GetCurrentSelectionText)),
			TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateUObject(MainInput, &UHoudiniInput::GetCurrentSelectionText)),
			FSlateIcon(),
			FUIAction(),
			NAME_None,
			EUserInterfaceActionType::Button,
			NAME_None);
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection(NAME_None, LOCTEXT("AssetInputSelectableActors", "Houdini Assets"));
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
		TSharedRef< SWidget > MenuWidget =
			SNew(SBox)
			.WidthOverride(SceneOutlinerWindowSize.X)
			.HeightOverride(SceneOutlinerWindowSize.Y)
			[
				SNew(SBorder)
				.BorderImage(FEditorStyle::GetBrush("Menu.Background"))
				[
					SceneOutlinerModule.CreateSceneOutliner(
						InitOptions,
						FOnActorPicked::CreateLambda(OnActorSelected, InInputs))
				]
			];

		MenuBuilder.AddWidget(MenuWidget, FText::GetEmpty(), true);
	}
	MenuBuilder.EndSection();

	return MenuBuilder;
}

FMenuBuilder
FHoudiniInputDetails::Helper_CreateLandscapePickerWidget(TArray<UHoudiniInput*>& InInputs)
{
	UHoudiniInput* MainInput = InInputs.Num() > 0 ? InInputs[0] : nullptr;	
	auto OnShouldFilterLandscape = [](const AActor* const Actor, UHoudiniInput* InInput)
	{
		if (!Actor || Actor->IsPendingKill())
			return false;

		if (!Actor->IsA<ALandscapeProxy>())
			return false;

		ALandscapeProxy* LandscapeProxy = const_cast<ALandscapeProxy *>(Cast<const ALandscapeProxy>(Actor));
		if (!LandscapeProxy)
			return false;

		// Get the landscape's actor
		AActor* OwnerActor = LandscapeProxy->GetOwner();

		// Get our Actor
		UHoudiniAssetComponent* MyHAC = Cast<UHoudiniAssetComponent>(InInput->GetOuter());
		AActor* MyOwner = MyHAC ? MyHAC->GetOwner() : nullptr;

		// TODO: FIX ME!
		// IF the landscape is owned by ourself, skip it!
		if (OwnerActor == MyOwner)
			return false;

		return true;
	};

	// Filters are only based on the MainInput
	auto OnShouldFilterActor = [MainInput, OnShouldFilterLandscape](const AActor* const Actor)
	{
		if (!MainInput || MainInput->IsPendingKill())
			return true;

		return OnShouldFilterLandscape(Actor, MainInput);
	};

	// Selection uses the input arrays
	auto OnLandscapeSelected = [OnShouldFilterLandscape](AActor* Actor, UHoudiniInput* Input)
	{
		if (!Actor || !Input)
			return;

		// Make sure that the actor is valid for this input
		if (!OnShouldFilterLandscape(Actor, Input))
			return;

		ALandscapeProxy* LandscapeProxy = Cast<ALandscapeProxy>(Actor);
		if (!LandscapeProxy)
			return;

		TArray<UHoudiniInputObject*>* LandscapeInputObjectsArray = Input->GetHoudiniInputObjectArray(Input->GetInputType());
		if (!LandscapeInputObjectsArray)
			return;

		LandscapeInputObjectsArray->Empty();

		FName LandscapeName = MakeUniqueObjectName(Input->GetOuter(), ALandscapeProxy::StaticClass(), TEXT("Landscape"));

		// Create a Houdini Input Object.
		UHoudiniInputObject* NewInputObject = UHoudiniInputLandscape::Create(
			LandscapeProxy, Input, LandscapeName.ToString());

		UHoudiniInputLandscape* LandscapeInput = Cast<UHoudiniInputLandscape>(NewInputObject);
		LandscapeInput->MarkChanged(true);

		LandscapeInputObjectsArray->Add(LandscapeInput);
		Input->MarkChanged(true);
	};
	
	auto OnActorSelected = [OnLandscapeSelected](AActor* Actor, TArray<UHoudiniInput*> InInputs)
	{
		for (auto CurInput : InInputs)
		{
			if (!CurInput || CurInput->IsPendingKill())
				continue;

			OnLandscapeSelected(Actor, CurInput);
		}
	};

	FMenuBuilder MenuBuilder(true, nullptr);
	FOnShouldFilterActor ActorFilter = FOnShouldFilterActor::CreateLambda(OnShouldFilterActor);

	// Show current selection
	MenuBuilder.BeginSection(NAME_None, LOCTEXT("CurrentActorOperationHeader", "Current Selection"));
	{
		MenuBuilder.AddMenuEntry(
			TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateUObject(MainInput, &UHoudiniInput::GetCurrentSelectionText)),
			TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateUObject(MainInput, &UHoudiniInput::GetCurrentSelectionText)),
			FSlateIcon(),
			FUIAction(),
			NAME_None,
			EUserInterfaceActionType::Button,
			NAME_None);
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection(NAME_None, LOCTEXT("LandscapeInputSelectableActors", "Landscapes"));
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
		TSharedRef< SWidget > MenuWidget =
			SNew(SBox)
			.WidthOverride(SceneOutlinerWindowSize.X)
			.HeightOverride(SceneOutlinerWindowSize.Y)
			[
				SNew(SBorder)
				.BorderImage(FEditorStyle::GetBrush("Menu.Background"))
				[
					SceneOutlinerModule.CreateSceneOutliner(
						InitOptions,
						FOnActorPicked::CreateLambda(OnActorSelected, InInputs))
				]
			];

		MenuBuilder.AddWidget(MenuWidget, FText::GetEmpty(), true);
	}
	MenuBuilder.EndSection();

	return MenuBuilder;
}

FMenuBuilder
FHoudiniInputDetails::Helper_CreateWorldActorPickerWidget(TArray<UHoudiniInput*>& InInputs)
{
	UHoudiniInput* MainInput = InInputs.Num() > 0 ? InInputs[0] : nullptr;
	auto OnShouldFilterWorld = [MainInput](const AActor* const Actor)
	{
		if (!MainInput || MainInput->IsPendingKill())
			return true;

		const TArray<UHoudiniInputObject*>* InputObjects = MainInput->GetHoudiniInputObjectArray(EHoudiniInputType::World);
		if (!InputObjects)
			return false;

		// Only return actors that are currently selected by our input
		for (const auto& CurInputObject : *InputObjects)
		{
			if (!CurInputObject || CurInputObject->IsPendingKill())
				continue;

			AActor* CurActor = Cast<AActor>(CurInputObject->GetObject());
			if (!CurActor || CurActor->IsPendingKill())
				continue;

			if (CurActor == Actor)
				return true;
		}

		return false;
	};

	auto OnWorldSelected = [](AActor* Actor)
	{
		// Do Nothing
	};

	FMenuBuilder MenuBuilder(true, nullptr);
	FOnShouldFilterActor ActorFilter = FOnShouldFilterActor::CreateLambda(OnShouldFilterWorld);

	MenuBuilder.BeginSection(NAME_None, LOCTEXT("WorldInputSelectedActors", "Currently Selected Actors"));
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
		TSharedRef< SWidget > MenuWidget =
			SNew(SBox)
			.WidthOverride(SceneOutlinerWindowSize.X)
			.HeightOverride(SceneOutlinerWindowSize.Y)
			[
				SNew(SBorder)
				.BorderImage(FEditorStyle::GetBrush("Menu.Background"))
				[
					SceneOutlinerModule.CreateSceneOutliner(
						InitOptions,
						FOnActorPicked::CreateLambda(OnWorldSelected))
				]
			];

		MenuBuilder.AddWidget(MenuWidget, FText::GetEmpty(), true);
	}
	MenuBuilder.EndSection();

	return MenuBuilder;
}

FMenuBuilder
FHoudiniInputDetails::Helper_CreateBoundSelectorPickerWidget(TArray<UHoudiniInput*>& InInputs)
{
	UHoudiniInput* MainInput = InInputs.Num() > 0 ? InInputs[0] : nullptr;
	auto OnShouldFilter = [MainInput](const AActor* const Actor)
	{
		if (!Actor || Actor->IsPendingKill())
			return false;

		const TArray<AActor*>* BoundObjects = MainInput->GetBoundSelectorObjectArray();
		if (!BoundObjects)
			return false;

		// Only return actors that are currently selected by our input
		for (const auto& CurActor : *BoundObjects)
		{
			if (!CurActor || CurActor->IsPendingKill())
				continue;

			if (CurActor == Actor)
				return true;
		}

		return false;
	};

	
	auto OnSelected = [](AActor* Actor)
	{
		// Do Nothing
	};
	
	FMenuBuilder MenuBuilder(true, nullptr);
	MenuBuilder.BeginSection(NAME_None, LOCTEXT("WorldInputBoundSelectors", "Bound Selectors"));
	{
		FSceneOutlinerModule & SceneOutlinerModule =
			FModuleManager::Get().LoadModuleChecked< FSceneOutlinerModule >(TEXT("SceneOutliner"));
		SceneOutliner::FInitializationOptions InitOptions;
		{
			InitOptions.Mode = ESceneOutlinerMode::ActorPicker;
			InitOptions.Filters->AddFilterPredicate(FOnShouldFilterActor::CreateLambda(OnShouldFilter));
			InitOptions.bFocusSearchBoxWhenOpened = true;
			InitOptions.bShowCreateNewFolder = false;

			// Add the gutter so we can change the selection's visibility
			InitOptions.ColumnMap.Add(SceneOutliner::FBuiltInColumnTypes::Gutter(), SceneOutliner::FColumnInfo(SceneOutliner::EColumnVisibility::Visible, 0));
			InitOptions.ColumnMap.Add(SceneOutliner::FBuiltInColumnTypes::Label(), SceneOutliner::FColumnInfo(SceneOutliner::EColumnVisibility::Visible, 10));
			InitOptions.ColumnMap.Add(SceneOutliner::FBuiltInColumnTypes::ActorInfo(), SceneOutliner::FColumnInfo(SceneOutliner::EColumnVisibility::Visible, 20));
		}

		static const FVector2D SceneOutlinerWindowSize(350.0f, 200.0f);
		TSharedRef< SWidget > MenuWidget =
			SNew(SBox)
			.WidthOverride(SceneOutlinerWindowSize.X)
			.HeightOverride(SceneOutlinerWindowSize.Y)
			[
				SNew(SBorder)
				.BorderImage(FEditorStyle::GetBrush("Menu.Background"))
				[
					SceneOutlinerModule.CreateSceneOutliner(
						InitOptions,
						FOnActorPicked::CreateLambda(OnSelected))
				]
			];

		MenuBuilder.AddWidget(MenuWidget, FText::GetEmpty(), true);
	}
	MenuBuilder.EndSection();

	return MenuBuilder;
}

void
FHoudiniInputDetails::AddWorldInputUI(
	TSharedRef<SVerticalBox> VerticalBox,
	TArray<UHoudiniInput*>& InInputs,
	const IDetailsView* DetailsView)
{
	if (InInputs.Num() <= 0)
		return;

	UHoudiniInput* MainInput = InInputs[0];
	if (!MainInput)
		return;

	const int32 NumInputObjects = MainInput->GetNumberOfInputObjects(EHoudiniInputType::World);
	
	// Get the details view name and locked status
	bool bDetailsLocked = false;
	FName DetailsPanelName = "LevelEditorSelectionDetails";
	if (DetailsView)
	{
		DetailsPanelName = DetailsView->GetIdentifier();
		if (DetailsView->IsLocked())
			bDetailsLocked = true;
	}

	// Check of we're in bound selector mode
	bool bIsBoundSelector = MainInput->IsWorldInputBoundSelector();

	// Button : Start Selection / Use current selection + refresh        
	{
		TSharedPtr< SHorizontalBox > HorizontalBox = NULL;
		FPropertyEditorModule & PropertyModule =
			FModuleManager::Get().GetModuleChecked< FPropertyEditorModule >("PropertyEditor");

		auto ButtonLabel = LOCTEXT("WorldInputStartSelection", "Start Selection (Locks Details Panel)");
		auto ButtonTooltip = LOCTEXT("WorldInputStartSelectionTooltip", "Locks the Details Panel, and allow you to select object in the world that you can commit to the input afterwards.");
		if (!bIsBoundSelector)
		{
			// Button :  Start Selection / Use current selection
			if (bDetailsLocked)
			{
				ButtonLabel = LOCTEXT("WorldInputUseCurrentSelection", "Use Current Selection (Unlocks Details Panel)");
				ButtonTooltip = LOCTEXT("WorldInputUseCurrentSelectionTooltip", "Fill the asset's input with the current selection  and unlocks the Details Panel.");
			}
			/*
			FOnClicked OnSelectActors = FOnClicked::CreateStatic(
				&FHoudiniInputDetails::Helper_OnButtonClickSelectActors, InInputs, DetailsPanelName);
			*/
			VerticalBox->AddSlot().Padding(2, 2, 5, 2).AutoHeight()
			[
				SAssignNew(HorizontalBox, SHorizontalBox)
				+ SHorizontalBox::Slot()
				[
					SNew(SButton)
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Center)
					.Text(ButtonLabel)
					.ToolTipText(ButtonTooltip)
					//.OnClicked(OnSelectActors)
					.OnClicked_Lambda([InInputs, DetailsPanelName]()
					{
						return Helper_OnButtonClickSelectActors(InInputs, DetailsPanelName);
					})
					
				]
			];
		}
		else
		{
			// Button :  Start Selection / Use current selection as Bound selector
			if (bDetailsLocked)
			{
				ButtonLabel = LOCTEXT("WorldInputUseSelectionAsBoundSelector", "Use Selection as Bound Selector (Unlocks Details Panel)");
				ButtonTooltip = LOCTEXT("WorldInputUseSelectionAsBoundSelectorTooltip", "Fill the asset's input with all the actors contains in the current's selection bound, and unlocks the Details Panel.");
			}
			
			/*
			FOnClicked OnSelectBounds = FOnClicked::CreateStatic(
				&FHoudiniInputDetails::Helper_OnButtonClickUseSelectionAsBoundSelector, InInputs, DetailsPanelName);
			*/
			VerticalBox->AddSlot().Padding(2, 2, 5, 2).AutoHeight()
			[
				SAssignNew(HorizontalBox, SHorizontalBox)
				+ SHorizontalBox::Slot()
				[
					SNew(SButton)
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Center)
					.Text(ButtonLabel)
					.ToolTipText(ButtonTooltip)
					//.OnClicked(OnSelectBounds)
					.OnClicked_Lambda([InInputs, DetailsPanelName]()
					{
						return Helper_OnButtonClickUseSelectionAsBoundSelector(InInputs, DetailsPanelName);
					})
				]
			];
		}
	}

	// Button : Select All + Clear Selection
	{
		TSharedPtr< SHorizontalBox > HorizontalBox = NULL;

		FOnClicked OnSelectAll = FOnClicked::CreateLambda([InInputs]()
		{
			for (auto CurrentInput : InInputs)
			{
				// Get the parent component/actor/world of the current input
				USceneComponent* ParentComponent = Cast<USceneComponent>(CurrentInput->GetOuter());
				AActor* ParentActor = ParentComponent ? ParentComponent->GetOwner() : nullptr;
				UWorld* MyWorld = CurrentInput->GetWorld();

				TArray<AActor*> NewSelectedActors;
				for (TActorIterator<AActor> ActorItr(MyWorld); ActorItr; ++ActorItr)
				{
					AActor *CurrentActor = *ActorItr;
					if (!CurrentActor || CurrentActor->IsPendingKill())
						continue;

					// Ignore the SkySpheres?
					FString ClassName = CurrentActor->GetClass() ? CurrentActor->GetClass()->GetName() : FString();
					if (ClassName.Contains("BP_Sky_Sphere"))
						continue;

					// Don't allow selection of ourselves. Bad things happen if we do.
					if (ParentActor && (CurrentActor == ParentActor))
						continue;

					NewSelectedActors.Add(CurrentActor);
				}

				// Record a transaction for undo/redo
				FScopedTransaction Transaction(
					TEXT(HOUDINI_MODULE_EDITOR),
					LOCTEXT("HoudiniInputChange", "Houdini Input: Selected all actor in the current world"),
					CurrentInput->GetOuter());
				CurrentInput->Modify();

				bool bHasChanged = CurrentInput->UpdateWorldSelection(NewSelectedActors);

				// Cancel the transaction if the selection hasn't changed
				if(!bHasChanged)
					Transaction.Cancel();
			}

			return FReply::Handled();
		});

		FOnClicked OnClearSelect = FOnClicked::CreateLambda([InInputs]()
		{
			for (auto CurrentInput : InInputs)
			{
				// Record a transaction for undo/redo
				FScopedTransaction Transaction(
					TEXT(HOUDINI_MODULE_EDITOR),
					LOCTEXT("HoudiniInputChange", "Houdini Input: Clear world input selection"),
					CurrentInput->GetOuter());
				CurrentInput->Modify();

				TArray<AActor*> EmptySelection;
				bool bHasChanged = CurrentInput->UpdateWorldSelection(EmptySelection);

				// Cancel the transaction if the selection hasn't changed
				if (!bHasChanged)
					Transaction.Cancel();
			}

			return FReply::Handled();
		});

		VerticalBox->AddSlot().Padding(2, 2, 5, 2).AutoHeight()
		[
			SAssignNew(HorizontalBox, SHorizontalBox)
			+ SHorizontalBox::Slot()
			[
				// Button :  SelectAll
				SNew(SButton)
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				.Text(LOCTEXT("WorldInputSelectAll", "Select All"))
				.ToolTipText(LOCTEXT("WorldInputSelectAll", "Fill the asset's input with all actors."))
				.OnClicked(OnSelectAll)
			]
			+ SHorizontalBox::Slot()
			[
				// Button :  Clear Selection
				SNew(SButton)
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				.Text(LOCTEXT("ClearSelection", "Clear Selection"))
				.ToolTipText(LOCTEXT("ClearSelectionTooltip", "Clear the input's current selection"))
				.OnClicked(OnClearSelect)
			]
		];

		// Do not enable select all/clear select when selection has been started and details are locked
		HorizontalBox->SetEnabled(!bDetailsLocked);
	}

	// Checkbox: Bound Selector
	{
		// Lambda returning a CheckState from the input's current bound selector state
		auto IsCheckedBoundSelector = [](UHoudiniInput* InInput)
		{
			return InInput->IsWorldInputBoundSelector() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
		};

		// Lambda for changing bound selector state
		auto CheckStateChangedIsBoundSelector = [](TArray<UHoudiniInput*> InInputsToUpdate, ECheckBoxState NewState)
		{
			bool bNewState = (NewState == ECheckBoxState::Checked);
			for (auto CurInput : InInputsToUpdate)
			{
				if (CurInput->IsWorldInputBoundSelector() == bNewState)
					continue;

				// Record a transaction for undo/redo
				FScopedTransaction Transaction(
					TEXT(HOUDINI_MODULE_EDITOR),
					LOCTEXT("HoudiniInputChange", "Houdini Input: Changed world input to bound selector"),
					CurInput->GetOuter());
				CurInput->Modify();

				CurInput->SetWorldInputBoundSelector(bNewState);
				CurInput->MarkChanged(true);
			}
		};

		// Checkbox : Is Bound Selector
		TSharedPtr< SCheckBox > CheckBoxBoundSelector;
		VerticalBox->AddSlot().Padding(2, 2, 5, 2).AutoHeight()
		[
			SAssignNew(CheckBoxBoundSelector, SCheckBox)
			.Content()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("BoundSelector", "Bound Selector"))
				.ToolTipText(LOCTEXT("BoundSelectorTip", "When enabled, this world input works as a bound selector, sending all the objects contained in the bound selector bounding boxes."))
				.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			]		
			.IsChecked_Lambda([IsCheckedBoundSelector, MainInput]()
			{
				return IsCheckedBoundSelector(MainInput);
			})
			.OnCheckStateChanged_Lambda([CheckStateChangedIsBoundSelector, InInputs](ECheckBoxState NewState)
			{
				return CheckStateChangedIsBoundSelector(InInputs, NewState);
			})
		];
	}

	// Checkbox: Bound Selector Auto update
	{
		// Lambda returning a CheckState from the input's current auto update state
		auto IsCheckedAutoUpdate = [](UHoudiniInput* InInput)
		{
			return InInput->GetWorldInputBoundSelectorAutoUpdates() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
		};

		// Lambda for changing the auto update state
		auto CheckStateChangedBoundAutoUpdates = [](TArray<UHoudiniInput*> InInputsToUpdate, ECheckBoxState NewState)
		{
			bool bNewState = (NewState == ECheckBoxState::Checked);
			for (auto CurInput : InInputsToUpdate)
			{
				if (CurInput->GetWorldInputBoundSelectorAutoUpdates() == bNewState)
					continue;

				// Record a transaction for undo/redo
				FScopedTransaction Transaction(
					TEXT(HOUDINI_MODULE_EDITOR),
					LOCTEXT("HoudiniInputChange", "Houdini Input: Changed bound selector auto-aupdate state."),
					CurInput->GetOuter());
				CurInput->Modify();

				CurInput->SetWorldInputBoundSelectorAutoUpdates(bNewState);
				CurInput->MarkChanged(true);
			}
		};

		// Checkbox : Is Bound Selector
		TSharedPtr< SCheckBox > CheckBoxBoundAutoUpdate;
		VerticalBox->AddSlot().Padding(2, 2, 5, 2).AutoHeight()
		[
			SAssignNew(CheckBoxBoundAutoUpdate, SCheckBox)
			.Content()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("BoundAutoUpdate", "Update bound selection automatically"))
				.ToolTipText(LOCTEXT("BoundAutoUpdateTip", "If enabled and if this world input is set as a bound selector, the objects selected by the bounds will update automatically."))
				.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			]
			.IsChecked_Lambda([IsCheckedAutoUpdate, MainInput]()
			{
				return IsCheckedAutoUpdate(MainInput);
			})
			.OnCheckStateChanged_Lambda([CheckStateChangedBoundAutoUpdates, InInputs](ECheckBoxState NewState)
			{
				return CheckStateChangedBoundAutoUpdates(InInputs, NewState);
			})
		];

		CheckBoxBoundAutoUpdate->SetEnabled(MainInput->IsWorldInputBoundSelector());
	}

	// ActorPicker : Bound Selector
	if(bIsBoundSelector)
	{
		FMenuBuilder MenuBuilder = Helper_CreateBoundSelectorPickerWidget(InInputs);
		VerticalBox->AddSlot().Padding(2, 2, 5, 2).AutoHeight()
		[
			MenuBuilder.MakeWidget()
		];
	}

	// ActorPicker : World Outliner
	{
		FMenuBuilder MenuBuilder = Helper_CreateWorldActorPickerWidget(InInputs);
		VerticalBox->AddSlot().Padding(2, 2, 5, 2).AutoHeight()
		[
			MenuBuilder.MakeWidget()
		];
	}

	{
		// Spline Resolution
		TSharedPtr<SNumericEntryBox<float>> NumericEntryBox;
		int32 Idx = 0;
		VerticalBox->AddSlot().Padding(2, 2, 5, 2).AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("SplineRes", "Unreal Spline Resolution"))
				.ToolTipText(LOCTEXT("SplineResTooltip", "Resolution used when marshalling the Unreal Splines to HoudiniEngine.\n(step in cm betweem control points)\nSet this to 0 to only export the control points."))
				.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			]
			+ SHorizontalBox::Slot()
			.Padding(2.0f, 0.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SNumericEntryBox<float>)
				.AllowSpin(true)
				.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				.MinValue(-1.0f)
				.MaxValue(1000.0f)
				.MinSliderValue(0.0f)
				.MaxSliderValue(1000.0f)
				.Value(MainInput->GetUnrealSplineResolution())
				.OnValueChanged_Lambda([InInputs](float Val) 
				{
					for (auto CurrentInput : InInputs)
					{
						if (!CurrentInput || CurrentInput->IsPendingKill())
							continue;

						if (CurrentInput->GetUnrealSplineResolution() == Val)
							continue;

						// Record a transaction for undo/redo
						FScopedTransaction Transaction(
							TEXT(HOUDINI_MODULE_EDITOR),
							LOCTEXT("HoudiniInputChange", "Houdini Input: Changed world input spline resolution"),
							CurrentInput->GetOuter());
						CurrentInput->Modify();

						CurrentInput->SetUnrealSplineResolution(Val);
						CurrentInput->MarkChanged(true);
					}					
				})
				/*
				.Value(TAttribute< TOptional< float > >::Create(TAttribute< TOptional< float > >::FGetter::CreateUObject(
					&InParam, &UHoudiniAssetInput::GetSplineResolutionValue)))
				.OnValueChanged(SNumericEntryBox< float >::FOnValueChanged::CreateUObject(
					&InParam, &UHoudiniAssetInput::SetSplineResolutionValue))
				.IsEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateUObject(
					&InParam, &UHoudiniAssetInput::IsSplineResolutionEnabled)))
				*/
				.SliderExponent(1.0f)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f, 0.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SButton)
				.ToolTipText(LOCTEXT("SplineResToDefault", "Reset to default value."))
				.ButtonStyle(FEditorStyle::Get(), "NoBorder")
				.ContentPadding(0)
				.Visibility(EVisibility::Visible)
				// TODO: FINISH ME!
				//.OnClicked(FOnClicked::CreateUObject(&InParam, &UHoudiniAssetInput::OnResetSplineResolutionClicked))
				[
					SNew(SImage)
					.Image(FEditorStyle::GetBrush("PropertyWindow.DiffersFromDefault"))
				]
			]
		];
	}
}

void
FHoudiniInputDetails::AddSkeletalInputUI(
	TSharedRef<SVerticalBox> VerticalBox,
	TArray<UHoudiniInput*>& InInputs, 
	TSharedPtr<FAssetThumbnailPool> AssetThumbnailPool )
{
}


void 
FHoudiniInputDetails::ReselectSelectedActors() 
{
	// TODO: Duplicate with FHoudiniEngineUtils::UpdateEditorProperties ??
	USelection* Selection = GEditor->GetSelectedActors();
	TArray<AActor*> SelectedActors;
	SelectedActors.SetNumUninitialized(GEditor->GetSelectedActorCount());
	Selection->GetSelectedObjects(SelectedActors);

	GEditor->SelectNone(false, false, false);
	
	for (AActor* NextSelected : SelectedActors)
	{
		GEditor->SelectActor(NextSelected, true, true, true, true);
	}
}

FReply
FHoudiniInputDetails::Helper_OnButtonClickSelectActors(TArray<UHoudiniInput*> InInputs, const FName& DetailsPanelName)
{
	return Helper_OnButtonClickSelectActors(InInputs, DetailsPanelName, false);
}

FReply
FHoudiniInputDetails::Helper_OnButtonClickUseSelectionAsBoundSelector(TArray<UHoudiniInput*> InInputs, const FName& DetailsPanelName)
{
	return Helper_OnButtonClickSelectActors(InInputs, DetailsPanelName, true);
}

FReply
FHoudiniInputDetails::Helper_OnButtonClickSelectActors(TArray<UHoudiniInput*> InInputs, const FName& DetailsPanelName, const bool& bUseWorldInAsWorldSelector)
{
	UHoudiniInput* MainInput = InInputs.Num() > 0 ? InInputs[0] : nullptr;
	if (!MainInput || MainInput->IsPendingKill())
		return FReply::Handled();

	// There's no undo operation for button.
	FPropertyEditorModule & PropertyModule =
		FModuleManager::Get().GetModuleChecked< FPropertyEditorModule >("PropertyEditor");

	// Locate the details panel.
	TSharedPtr<IDetailsView> DetailsView = PropertyModule.FindDetailView(DetailsPanelName);
	if (!DetailsView.IsValid())
		return FReply::Handled();

	class SLocalDetailsView : public SDetailsViewBase
	{
		public:
			void LockDetailsView() { SDetailsViewBase::bIsLocked = true; }
			void UnlockDetailsView() { SDetailsViewBase::bIsLocked = false; }
	};
	auto * LocalDetailsView = static_cast<SLocalDetailsView *>(DetailsView.Get());

	if (!DetailsView->IsLocked())
	{
		//
		// START SELECTION
		//  Locks the details view and select our currently selected actors
		//
		LocalDetailsView->LockDetailsView();
		check(DetailsView->IsLocked());

		// Force refresh of details view.
		TArray<UObject*> InputOuters;
		for (auto CurIn : InInputs)
			InputOuters.Add(CurIn->GetOuter());
		FHoudiniEngineUtils::UpdateEditorProperties(InputOuters, true);
		//ReselectSelectedActors();

		if (bUseWorldInAsWorldSelector)
		{
			// Bound Selection
			// Select back the previously chosen bound selectors
			GEditor->SelectNone(false, true);
			int32 NumBoundSelectors = MainInput->GetNumberOfBoundSelectorObjects();
			for (int32 Idx = 0; Idx < NumBoundSelectors; Idx++)
			{
				AActor* Actor = MainInput->GetBoundSelectorObjectAt(Idx);
				if (!Actor || Actor->IsPendingKill())
					continue;

				GEditor->SelectActor(Actor, true, true);
			}
		}
		else
		{
			// Regular selection
			// Select the already chosen input Actors from the World Outliner.
			GEditor->SelectNone(false, true);
			int32 NumInputObjects = MainInput->GetNumberOfInputObjects(EHoudiniInputType::World);
			for (int32 Idx = 0; Idx < NumInputObjects; Idx++)
			{
				UHoudiniInputActor* InputActor = Cast<UHoudiniInputActor>(MainInput->GetHoudiniInputObjectAt(Idx));
				if (!InputActor || InputActor->IsPendingKill())
					continue;

				AActor* Actor = InputActor->GetActor();
				if (!Actor || Actor->IsPendingKill())
					continue;

				GEditor->SelectActor(Actor, true, true);
			}
		}

		return FReply::Handled();
	}
	else
	{
		//
		// UPDATE SELECTION
		//  Unlocks the input's selection and select the HDA back.
		//

		if (!GEditor || !GEditor->GetSelectedObjects())
			return FReply::Handled();

		USelection * SelectedActors = GEditor->GetSelectedActors();
		if (!SelectedActors)
			return FReply::Handled();

		TArray<UObject*> AllActors;
		for (auto CurrentInput : InInputs)
		{
			// Get our parent component/actor
			USceneComponent* ParentComponent = Cast<USceneComponent>(CurrentInput->GetOuter());
			AActor* ParentActor = ParentComponent ? ParentComponent->GetOwner() : nullptr;
			AllActors.Add(ParentActor);

			// Create a transaction
			FScopedTransaction Transaction(
				TEXT(HOUDINI_MODULE_RUNTIME),
				LOCTEXT("HoudiniInputChange", "Houdini World Outliner Input Change"),
				CurrentInput->GetOuter());

			CurrentInput->Modify();

			bool bHasChanged = true;
			if (bUseWorldInAsWorldSelector)
			{
				//
				// Update bound selectors
				//

				// Clean up the selected actors
				TArray<AActor*> ValidBoundSelectedActors;
				for (FSelectionIterator It(*SelectedActors); It; ++It)
				{
					AActor* CurrentBoundActor = Cast<AActor>(*It);
					if (!CurrentBoundActor)
						continue;

					// Don't allow selection of ourselves. Bad things happen if we do.
					if (ParentActor && (CurrentBoundActor == ParentActor))
						continue;

					ValidBoundSelectedActors.Add(CurrentBoundActor);
				}

				// See if the bound selector have changed
				int32 PreviousBoundSelectorCount = CurrentInput->GetNumberOfBoundSelectorObjects();
				if (PreviousBoundSelectorCount == ValidBoundSelectedActors.Num())
				{
					// Same number of BoundSelectors, see if they have changed
					bHasChanged = false;
					for (int32 BoundIdx = 0; BoundIdx < PreviousBoundSelectorCount; BoundIdx++)
					{
						AActor* PreviousBound = CurrentInput->GetBoundSelectorObjectAt(BoundIdx);
						if (!PreviousBound)
							continue;

						if (!ValidBoundSelectedActors.Contains(PreviousBound))
						{
							bHasChanged = true;
							break;
						}
					}
				}

				if (bHasChanged)
				{
					// Only update the bound selector objects on the input if they have changed
					CurrentInput->SetBoundSelectorObjectsNumber(ValidBoundSelectedActors.Num());
					int32 InputObjectIdx = 0;
					for (auto CurActor : ValidBoundSelectedActors)
					{
						CurrentInput->SetBoundSelectorObjectAt(InputObjectIdx++, CurActor);
					}

					// Update the current selection from the BoundSelectors
					CurrentInput->UpdateWorldSelectionFromBoundSelectors();
				}
			}
			else
			{
				//
				// Update our selection directly with the currently selected actors
				//

				TArray<AActor*> ValidSelectedActors;
				for (FSelectionIterator It(*SelectedActors); It; ++It)
				{
					AActor* CurrentActor = Cast<AActor>(*It);
					if (!CurrentActor)
						continue;

					// Don't allow selection of ourselves. Bad things happen if we do.
					if (ParentActor && (CurrentActor == ParentActor))
						continue;

					ValidSelectedActors.Add(CurrentActor);
				}

				// Update the input objects from the valid selected actors array
				// Only new/remove input objects will be marked as changed
				bHasChanged = CurrentInput->UpdateWorldSelection(ValidSelectedActors);
			}

			// If we didnt change the selection, cancel the transaction
			if (!bHasChanged)
				Transaction.Cancel();
		}

		// We can now unlock the details view...
		LocalDetailsView->UnlockDetailsView();
		check(!DetailsView->IsLocked());

		// .. reset the selected actors, force refresh and override the lock.
		DetailsView->SetObjects(AllActors, true, true);

		// We now need to reselect all our Asset Actors.
		// If we don't do this, our Asset parameters will stop refreshing and the user will be very confused.
		// It is also resetting the state of the selection before the input actor selection process was started.
		GEditor->SelectNone(false, true);
		for (auto CurrentActor : AllActors)
		{
			AActor* ParentActor = Cast<AActor>(CurrentActor);
			if (!ParentActor)
				continue;

			GEditor->SelectActor(ParentActor, true, true);
		}

		// Update the input details layout.
		//FHoudiniEngineUtils::UpdateEditorProperties(MainInput->GetOuter(), true);
	}

	return FReply::Handled();
}


bool
FHoudiniInputDetails::Helper_CancelWorldSelection(TArray<UHoudiniInput*>& InInputs, const FName& DetailsPanelName)
{
	if (InInputs.Num() <= 0)
		return false;

	// Get the property module to access the details view
	FPropertyEditorModule & PropertyModule =
		FModuleManager::Get().GetModuleChecked< FPropertyEditorModule >("PropertyEditor");

	// Locate the details panel.
	TSharedPtr<IDetailsView> DetailsView = PropertyModule.FindDetailView(DetailsPanelName);
	if (!DetailsView.IsValid())
		return false;

	if (!DetailsView->IsLocked())
		return false;

	class SLocalDetailsView : public SDetailsViewBase
	{
	public:
		void LockDetailsView() { SDetailsViewBase::bIsLocked = true; }
		void UnlockDetailsView() { SDetailsViewBase::bIsLocked = false; }
	};
	auto * LocalDetailsView = static_cast<SLocalDetailsView *>(DetailsView.Get());

	// Get all our parent components / actors
	TArray<UObject*> AllComponents;
	TArray<UObject*> AllActors;
	for (auto CurrentInput : InInputs)
	{
		// Get our parent component/actor
		USceneComponent* ParentComponent = Cast<USceneComponent>(CurrentInput->GetOuter());
		if (!ParentComponent)
			continue;

		AllComponents.Add(ParentComponent);

		AActor* ParentActor = ParentComponent ? ParentComponent->GetOwner() : nullptr;
		if (!ParentActor)
			continue;

		AllActors.Add(ParentActor);
	}

	// Unlock the detail view and re-select our parent actors
	{
		LocalDetailsView->UnlockDetailsView();
		check(!DetailsView->IsLocked());

		// Reset selected actor to itself, force refresh and override the lock.
		DetailsView->SetObjects(AllActors, true, true);
	}

	// Reselect the Asset Actor. If we don't do this, our Asset parameters will stop
	// refreshing and the user will be very confused. It is also resetting the state
	// of the selection before the input actor selection process was started.
	GEditor->SelectNone(false, true);
	for (auto ParentActorObj : AllActors)
	{
		AActor* ParentActor = Cast<AActor>(ParentActorObj);
		if (!ParentActor)
			continue;

		GEditor->SelectActor(ParentActor, true, true);
	}

	// Update the input details layout.
	//FHoudiniEngineUtils::UpdateEditorProperties(AllComponents, true);

	return true;
}

#undef LOCTEXT_NAMESPACE