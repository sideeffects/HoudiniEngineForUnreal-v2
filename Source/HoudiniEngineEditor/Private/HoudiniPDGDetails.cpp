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

#include "HoudiniPDGDetails.h"

#include "HoudiniPDGAssetLink.h"
#include "HoudiniPDGManager.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniEngineRuntimePrivatePCH.h"
#include "HoudiniEngineEditorPrivatePCH.h"
#include "HoudiniAssetActor.h"
#include "HoudiniEngine.h"

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "IDetailGroup.h"
#include "IDetailCustomization.h"
#include "PropertyCustomizationHelpers.h"
#include "DetailWidgetRow.h"
#include "HoudiniEngineBakeUtils.h"
#include "HoudiniEngineCommands.h"
#include "HoudiniEngineDetails.h"
#include "HoudiniEngineEditor.h"
#include "ScopedTransaction.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SSpacer.h"
#include "Framework/SlateDelegates.h"
#include "Templates/SharedPointer.h"

#include "Internationalization/Internationalization.h"

#define LOCTEXT_NAMESPACE HOUDINI_LOCTEXT_NAMESPACE

#define HOUDINI_ENGINE_UI_SECTION_PDG_BAKE 2

void 
FHoudiniPDGDetails::CreateWidget(
	IDetailCategoryBuilder& HouPDGCategory,
	UHoudiniPDGAssetLink* InPDGAssetLink)
{
	if (!InPDGAssetLink || InPDGAssetLink->IsPendingKill())
		return;

	// PDG ASSET
	FHoudiniPDGDetails::AddPDGAssetWidget(HouPDGCategory, InPDGAssetLink);
	
	// TOP NETWORKS
	FHoudiniPDGDetails::AddTOPNetworkWidget(HouPDGCategory, InPDGAssetLink);

	// PDG EVENT MESSAGES
}


void
FHoudiniPDGDetails::AddPDGAssetWidget(
	IDetailCategoryBuilder& InPDGCategory, UHoudiniPDGAssetLink* InPDGAssetLink)
{	
	// PDG STATUS ROW
	AddPDGAssetStatus(InPDGCategory, InPDGAssetLink);

	// Commandlet Status row
	AddPDGCommandletStatus(InPDGCategory, FHoudiniEngine::Get().GetPDGCommandletStatus());

	// REFRESH / RESET Buttons
	{		
		FDetailWidgetRow& PDGRefreshResetRow = InPDGCategory.AddCustomRow(FText::GetEmpty())
		.WholeRowContent()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			[
				SNew(SBox)
				.WidthOverride(200.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("Refresh", "Refresh"))
					.ToolTipText(LOCTEXT("RefreshTooltip", "Refreshes infos displayed by the the PDG Asset Link"))
					.ContentPadding(FMargin(5.0f, 5.0f))
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Center)
					.OnClicked_Lambda([InPDGAssetLink]()
					{
						FHoudiniPDGDetails::RefreshPDGAssetLink(InPDGAssetLink);
						return FReply::Handled();
					})
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SBox)
				.WidthOverride(200.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("Reset", "Reset"))
					.ToolTipText(LOCTEXT("ResetTooltip", "Resets the PDG Asset Link"))
					.ContentPadding(FMargin(5.0f, 5.0f))
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Center)
					.OnClicked_Lambda([InPDGAssetLink]()
					{
						// TODO: RESET USELESS? 
						FHoudiniPDGDetails::RefreshUI(InPDGAssetLink);
						return FReply::Handled();
					})
				]
			]
		];
	}

	// TOP NODE FILTER	
	{
		FText Tooltip = FText::FromString(TEXT("When enabled, the TOP Node Filter will only display the TOP Nodes found in the current network that start with the filter prefix. Disabling the Filter will display all of the TOP Network's TOP Nodes."));
		// Lambda for changing the filter value
		auto ChangeTOPNodeFilter = [InPDGAssetLink](const FString& NewValue)
		{
			if (InPDGAssetLink->TOPNodeFilter.Equals(NewValue))
				return;

			// Record a transaction for undo/redo
			FScopedTransaction Transaction(
				TEXT(HOUDINI_MODULE_RUNTIME),
				LOCTEXT("HoudiniPDGAssetLinkParameterChange", "Houdini PDG Asset Link Parameter: Changing a value"),
				InPDGAssetLink);
			
			InPDGAssetLink->Modify();
			InPDGAssetLink->TOPNodeFilter = NewValue;
			// Notify that we have changed the property
			InPDGAssetLink->NotifyPostEditChangeProperty(GET_MEMBER_NAME_STRING_CHECKED(UHoudiniPDGAssetLink, TOPNodeFilter));
		};

		FDetailWidgetRow& PDGFilterRow = InPDGCategory.AddCustomRow(FText::GetEmpty());
		// Disable if PDG is not linked
		DisableIfPDGNotLinked(PDGFilterRow, InPDGAssetLink);
		PDGFilterRow.NameWidget.Widget =
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f, 0.0f)
			[
				// Checkbox enable filter
				SNew(SCheckBox)
				.IsChecked_Lambda([InPDGAssetLink]()
				{
					return InPDGAssetLink->bUseTOPNodeFilter ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;;
				})
				.OnCheckStateChanged_Lambda([InPDGAssetLink](ECheckBoxState NewState)
				{
					bool bNewState = (NewState == ECheckBoxState::Checked) ? true : false;
					if (InPDGAssetLink->bUseTOPNodeFilter == bNewState)
						return;

					// Record a transaction for undo/redo
					FScopedTransaction Transaction(
						TEXT(HOUDINI_MODULE_RUNTIME),
						LOCTEXT("HoudiniPDGAssetLinkParameterChange", "Houdini PDG Asset Link Parameter: Changing a value"),
						InPDGAssetLink);

					InPDGAssetLink->Modify();
					InPDGAssetLink->bUseTOPNodeFilter = bNewState;
					// Notify that we have changed the property
					InPDGAssetLink->NotifyPostEditChangeProperty(GET_MEMBER_NAME_STRING_CHECKED(UHoudiniPDGAssetLink, bUseTOPNodeFilter));
				})
				.ToolTipText(Tooltip)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("TOP Node Filter")))
				.ToolTipText(Tooltip)
			];
		
		PDGFilterRow.ValueWidget.Widget =
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.0f)
			[
				SNew(SEditableTextBox)
				.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				.ToolTipText(Tooltip)
				.Text_Lambda([InPDGAssetLink]()
				{
					if (!IsValid(InPDGAssetLink))
						return FText();
					return FText::FromString(InPDGAssetLink->TOPNodeFilter);
				})
				.OnTextCommitted_Lambda([ChangeTOPNodeFilter](const FText& Val, ETextCommit::Type TextCommitType)
				{
					ChangeTOPNodeFilter(Val.ToString()); 
				})
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f, 0.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SButton)
				.ToolTipText(LOCTEXT("RevertToDefault", "Revert to default"))
				.ButtonStyle(FEditorStyle::Get(), "NoBorder")
				.ContentPadding(0)
				.Visibility(EVisibility::Visible)
				.OnClicked_Lambda([=]()
				{
					FString DefaultFilter = TEXT(HAPI_UNREAL_PDG_DEFAULT_TOP_FILTER);
					ChangeTOPNodeFilter(DefaultFilter);
					return FReply::Handled();
				})
				[
					SNew(SImage)
					.Image(FEditorStyle::GetBrush("PropertyWindow.DiffersFromDefault"))
				]
			];
	}

	// TOP OUTPUT FILTER	
	{		
		// Lambda for changing the filter value
		FText Tooltip = FText::FromString(TEXT("When enabled, the Work Item Output Files created for the TOP Nodes found in the current network that start with the filter prefix will be automatically loaded int the world after being cooked."));
		auto ChangeTOPOutputFilter = [InPDGAssetLink](const FString& NewValue)
		{
			if (InPDGAssetLink->TOPOutputFilter.Equals(NewValue))
				return;

			// Record a transaction for undo/redo
			FScopedTransaction Transaction(
				TEXT(HOUDINI_MODULE_RUNTIME),
				LOCTEXT("HoudiniPDGAssetLinkParameterChange", "Houdini PDG Asset Link Parameter: Changing a value"),
				InPDGAssetLink);
			
			InPDGAssetLink->Modify();
			InPDGAssetLink->TOPOutputFilter = NewValue;
			// Notify that we have changed the property
			InPDGAssetLink->NotifyPostEditChangeProperty(GET_MEMBER_NAME_STRING_CHECKED(UHoudiniPDGAssetLink, TOPOutputFilter));
		};

		FDetailWidgetRow& PDGOutputFilterRow = InPDGCategory.AddCustomRow(FText::GetEmpty());
		// Disable if PDG is not linked
		DisableIfPDGNotLinked(PDGOutputFilterRow, InPDGAssetLink);

		PDGOutputFilterRow.NameWidget.Widget = 
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f, 0.0f)
			[
				// Checkbox enable filter
				SNew(SCheckBox)
				.IsChecked_Lambda([InPDGAssetLink]()
				{
					return InPDGAssetLink->bUseTOPOutputFilter ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
				})
				.OnCheckStateChanged_Lambda([InPDGAssetLink](ECheckBoxState NewState)
				{
					bool bNewState = (NewState == ECheckBoxState::Checked) ? true : false;
					if (InPDGAssetLink->bUseTOPOutputFilter == bNewState)
						return;

					// Record a transaction for undo/redo
					FScopedTransaction Transaction(
						TEXT(HOUDINI_MODULE_RUNTIME),
						LOCTEXT("HoudiniPDGAssetLinkParameterChange", "Houdini PDG Asset Link Parameter: Changing a value"),
						InPDGAssetLink);

					InPDGAssetLink->Modify();
					InPDGAssetLink->bUseTOPOutputFilter = bNewState;
					// Notify that we have changed the property
					InPDGAssetLink->NotifyPostEditChangeProperty(GET_MEMBER_NAME_STRING_CHECKED(UHoudiniPDGAssetLink, bUseTOPOutputFilter));
				})
				.ToolTipText(Tooltip)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("TOP Output Filter")))
				.ToolTipText(Tooltip)
			];

		PDGOutputFilterRow.ValueWidget.Widget = 
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.0f)
			[
				SNew(SEditableTextBox)
				.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				.Text_Lambda([InPDGAssetLink]()
				{
					if (!IsValid(InPDGAssetLink))
						return FText();
					return FText::FromString(InPDGAssetLink->TOPOutputFilter);
				})
				.OnTextCommitted_Lambda([ChangeTOPOutputFilter](const FText& Val, ETextCommit::Type TextCommitType)
				{
					ChangeTOPOutputFilter(Val.ToString());
				})
				.ToolTipText(Tooltip)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f, 0.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SButton)
				.ToolTipText(LOCTEXT("RevertToDefault", "Revert to default"))
				.ButtonStyle(FEditorStyle::Get(), "NoBorder")
				.ContentPadding(0)
				.Visibility(EVisibility::Visible)
				.OnClicked_Lambda([ChangeTOPOutputFilter]()
				{
					FString DefaultFilter = TEXT(HAPI_UNREAL_PDG_DEFAULT_TOP_OUTPUT_FILTER);
					ChangeTOPOutputFilter(DefaultFilter);
					return FReply::Handled();
				})
				[
					SNew(SImage)
					.Image(FEditorStyle::GetBrush("PropertyWindow.DiffersFromDefault"))
				]
			];
	}

	// Checkbox: Autocook
	{
		FText Tooltip = FText::FromString(TEXT("When enabled, the selected TOP Network's output will automatically cook after succesfully cooking the PDG Asset Link HDA."));
		FDetailWidgetRow& PDGAutocookRow = InPDGCategory.AddCustomRow(FText::GetEmpty());
		// Disable if PDG is not linked
		DisableIfPDGNotLinked(PDGAutocookRow, InPDGAssetLink);
		PDGAutocookRow.NameWidget.Widget =
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("Auto-cook")))
				.ToolTipText(Tooltip)
			];

		TSharedPtr<SCheckBox> AutoCookCheckBox;
		PDGAutocookRow.ValueWidget.Widget =
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f, 0.0f)
			[
				// Checkbox
				SAssignNew(AutoCookCheckBox, SCheckBox)
				.IsChecked_Lambda([InPDGAssetLink]()
				{			
					return InPDGAssetLink->bAutoCook ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
				})
				.OnCheckStateChanged_Lambda([InPDGAssetLink](ECheckBoxState NewState)
				{
					bool bNewState = (NewState == ECheckBoxState::Checked) ? true : false;
					if (!InPDGAssetLink || InPDGAssetLink->bAutoCook == bNewState)
						return;

					// Record a transaction for undo/redo
					FScopedTransaction Transaction(
						TEXT(HOUDINI_MODULE_RUNTIME),
						LOCTEXT("HoudiniPDGAssetLinkParameterChange", "Houdini PDG Asset Link Parameter: Changing a value"),
						InPDGAssetLink);
					
					InPDGAssetLink->Modify();
					InPDGAssetLink->bAutoCook = bNewState;
					InPDGAssetLink->NotifyPostEditChangeProperty(GET_MEMBER_NAME_STRING_CHECKED(UHoudiniPDGAssetLink, bAutoCook));
				})
				.ToolTipText(Tooltip)
			];
	}
	// Output parent actor selector
	{
		IDetailPropertyRow* PDGOutputParentActorRow = InPDGCategory.AddExternalObjectProperty({ InPDGAssetLink }, "OutputParentActor");
		if (PDGOutputParentActorRow)
		{
			TAttribute<bool> PDGOutputParentActorRowEnabled;
			BindDisableIfPDGNotLinked(PDGOutputParentActorRowEnabled, InPDGAssetLink);
			PDGOutputParentActorRow->IsEnabled(PDGOutputParentActorRowEnabled);
			TSharedPtr<SWidget> NameWidget;
			TSharedPtr<SWidget> ValueWidget;
			PDGOutputParentActorRow->GetDefaultWidgets(NameWidget, ValueWidget);
			PDGOutputParentActorRow->DisplayName(FText::FromString(TEXT("Output Parent Actor")));
			PDGOutputParentActorRow->ToolTip(FText::FromString(
				TEXT("The PDG Output Actors will be created under this parent actor. If not set, then the PDG Output Actors will be created under a new folder.")));
		}
	}

	// Add bake widgets for PDG output
	CreatePDGBakeWidgets(InPDGCategory, InPDGAssetLink);
	
	// WORK ITEM STATUS
	{
		FDetailWidgetRow& PDGStatusRow = InPDGCategory.AddCustomRow(FText::GetEmpty());
		// Disable if PDG is not linked
		DisableIfPDGNotLinked(PDGStatusRow, InPDGAssetLink);
		FHoudiniPDGDetails::AddWorkItemStatusWidget(
			PDGStatusRow, TEXT("Asset Work Item Status"), InPDGAssetLink, false);
	}
}

bool
FHoudiniPDGDetails::GetPDGStatusAndColor(
	UHoudiniPDGAssetLink* InPDGAssetLink, FString& OutPDGStatusString, FLinearColor& OutPDGStatusColor)
{
	OutPDGStatusString = FString();
	OutPDGStatusColor = FLinearColor::White;
	
	if (!IsValid(InPDGAssetLink))
		return false;
	
	switch (InPDGAssetLink->LinkState)
	{
	case EPDGLinkState::Linked:
		OutPDGStatusString = TEXT("PDG is READY");
		OutPDGStatusColor = FLinearColor::Green;
		break;
	case EPDGLinkState::Linking:
		OutPDGStatusString = TEXT("PDG is Linking");
		OutPDGStatusColor = FLinearColor::Yellow;
		break;
	case EPDGLinkState::Error_Not_Linked:
		OutPDGStatusString = TEXT("PDG is ERRORED");
		OutPDGStatusColor = FLinearColor::Red;
		break;
	case EPDGLinkState::Inactive:
		OutPDGStatusString = TEXT("PDG is INACTIVE");
		OutPDGStatusColor = FLinearColor::White;
		break;
	default:
		return false;
	}

	return true;
}

void
FHoudiniPDGDetails::AddPDGAssetStatus(
	IDetailCategoryBuilder& InPDGCategory, UHoudiniPDGAssetLink *InPDGAssetLink)
{
	FDetailWidgetRow& PDGStatusRow = InPDGCategory.AddCustomRow(FText::GetEmpty())
	.WholeRowContent()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.Padding(2.0f, 0.0f)
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Center)
		[
			SNew(STextBlock)
			.Text_Lambda([InPDGAssetLink]()
			{
				FString PDGStatusString;
				FLinearColor PDGStatusColor;
				GetPDGStatusAndColor(InPDGAssetLink, PDGStatusString, PDGStatusColor);
				return FText::FromString(PDGStatusString);
			})
			.ColorAndOpacity_Lambda([InPDGAssetLink]()
			{
				FString PDGStatusString;
				FLinearColor PDGStatusColor;
				GetPDGStatusAndColor(InPDGAssetLink, PDGStatusString, PDGStatusColor);
				return FSlateColor(PDGStatusColor);
			})
		]
	];
}

void
FHoudiniPDGDetails::GetPDGCommandletStatus(FString& OutStatusString, FLinearColor& OutStatusColor)
{
	OutStatusString = FString();
	OutStatusColor = FLinearColor::White;
	switch (FHoudiniEngine::Get().GetPDGCommandletStatus())
	{
	case EHoudiniBGEOCommandletStatus::Connected:
		OutStatusString = TEXT("Async importer is CONNECTED");
		OutStatusColor = FLinearColor::Green;
		break;
	case EHoudiniBGEOCommandletStatus::Running:
		OutStatusString = TEXT("Async importer is Running");
		OutStatusColor = FLinearColor::Yellow;
		break;
	case EHoudiniBGEOCommandletStatus::Crashed:
		OutStatusString = TEXT("Async importer has CRASHED");
		OutStatusColor = FLinearColor::Red;
		break;
	case EHoudiniBGEOCommandletStatus::NotStarted:
		OutStatusString = TEXT("Async importer is NOT STARTED");
		OutStatusColor = FLinearColor::White;
		break;
	}
}

void
FHoudiniPDGDetails::AddPDGCommandletStatus(
	IDetailCategoryBuilder& InPDGCategory, const EHoudiniBGEOCommandletStatus& InCommandletStatus)
{
	FDetailWidgetRow& PDGStatusRow = InPDGCategory.AddCustomRow(FText::GetEmpty())
    .WholeRowContent()
    [
        SNew(SHorizontalBox)
        + SHorizontalBox::Slot()
        .FillWidth(1.0f)
        .Padding(2.0f, 0.0f)
        .VAlign(VAlign_Center)
        .HAlign(HAlign_Center)
        [
            SNew(STextBlock)
            .Visibility_Lambda([]()
            {
            	const UHoudiniRuntimeSettings* Settings = GetDefault<UHoudiniRuntimeSettings>();
            	if (IsValid(Settings))
            	{
            		return FHoudiniEngineCommands::IsPDGCommandletEnabled() ? EVisibility::Visible : EVisibility::Collapsed;
            	}
            	
	            return EVisibility::Visible;
            })
            .Text_Lambda([]()
            {
            	FString StatusString;
            	FLinearColor StatusColor;
            	GetPDGCommandletStatus(StatusString, StatusColor);
	            return FText::FromString(StatusString);
            })
            .ColorAndOpacity_Lambda([]()
            {
            	FString StatusString;
            	FLinearColor StatusColor;
            	GetPDGCommandletStatus(StatusString, StatusColor);
            	return FSlateColor(StatusColor);
            })
        ]
    ];
}

bool
FHoudiniPDGDetails::GetWorkItemTallyValueAndColor(
	UHoudiniPDGAssetLink* InAssetLink,
	bool bInForSelectedNode,
	const FString& InTallyItemString,
	int32& OutValue,
	FLinearColor& OutColor)
{
	OutValue = 0;
	OutColor = FLinearColor::White;
	
	if (!IsValid(InAssetLink))
		return false;

	bool bFound = false;
	FWorkItemTally* TallyPtr = nullptr;
	if (bInForSelectedNode)
	{
		FTOPNode* const TOPNode = InAssetLink->GetSelectedTOPNode();
		if (TOPNode && !TOPNode->bHidden)
			TallyPtr = &(TOPNode->WorkItemTally);
	}
	else
		TallyPtr = &(InAssetLink->WorkItemTally);

	if (TallyPtr)
	{
		if (InTallyItemString == TEXT("WAITING"))
		{
			// For now we add waiting and scheduled together, since there is no separate column for scheduled on the UI
			OutValue = TallyPtr->WaitingWorkItems + TallyPtr->ScheduledWorkItems;
			OutColor = OutValue > 0 ? FLinearColor(0.0f, 1.0f, 1.0f) : FLinearColor::White;
			bFound = true;
		}
		else if (InTallyItemString == TEXT("COOKING"))
		{
			OutValue = TallyPtr->CookingWorkItems;
			OutColor = OutValue > 0 ? FLinearColor::Yellow : FLinearColor::White;
			bFound = true;
		}
		else if (InTallyItemString == TEXT("COOKED"))
		{
			OutValue = TallyPtr->CookedWorkItems;
			OutColor = OutValue > 0 ? FLinearColor::Green : FLinearColor::White; 
			bFound = true;
		}
		else if (InTallyItemString == TEXT("FAILED"))
		{
			OutValue = TallyPtr->ErroredWorkItems;
			OutColor = OutValue > 0 ? FLinearColor::Red : FLinearColor::White;
			bFound = true;
		}
	}

	return bFound;
}

void
FHoudiniPDGDetails::AddWorkItemStatusWidget(
	FDetailWidgetRow& InRow, const FString& InTitleString, UHoudiniPDGAssetLink* InAssetLink, bool bInForSelectedNode)
{
	auto AddGridBox = [InAssetLink, bInForSelectedNode](const FString& Title) -> SHorizontalBox::FSlot&
	{
		return SHorizontalBox::Slot()
		.MaxWidth(500.0f)
		.Padding(10.0f, 0.0f)
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Center)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			.AutoHeight()
			.Padding(FMargin(5.0f, 2.0f))
			[
				SNew(SBorder)
				.IsEnabled_Lambda([InAssetLink]() { return IsPDGLinked(InAssetLink); })
				.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
				.BorderBackgroundColor(FSlateColor(FLinearColor(0.6, 0.6, 0.6)))
				.Padding(FMargin(5.0f, 5.0f))
				[
					SNew(SBox)
					.WidthOverride(100.0f)
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Center)
					[
						SNew(STextBlock)
						.Text(FText::FromString(Title))
						.ColorAndOpacity_Lambda([InAssetLink, bInForSelectedNode, Title]()
						{
							int32 Value;
							FLinearColor Color;
							GetWorkItemTallyValueAndColor(InAssetLink, bInForSelectedNode, Title, Value, Color);
							return FSlateColor(Color);
						})
					]
				]
			]
			+ SVerticalBox::Slot()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			.AutoHeight()
			.Padding(FMargin(5.0f, 2.0f))
			[
				SNew(SBorder)
				.IsEnabled_Lambda([InAssetLink]() { return IsPDGLinked(InAssetLink); })
				.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
				.BorderBackgroundColor(FSlateColor(FLinearColor(0.8, 0.8, 0.8)))
				.Padding(FMargin(5.0f, 5.0f))
				[
					SNew(SBox)
					.WidthOverride(100.0f)
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Center)
					[
						SNew(STextBlock)
						.Text_Lambda([InAssetLink, bInForSelectedNode, Title]()
						{
							int32 Value;
							FLinearColor Color;
							GetWorkItemTallyValueAndColor(InAssetLink, bInForSelectedNode, Title, Value, Color);
							return FText::AsNumber(Value);
						})
						.ColorAndOpacity_Lambda([InAssetLink, bInForSelectedNode, Title]()
						{
							int32 Value;
							FLinearColor Color;
							GetWorkItemTallyValueAndColor(InAssetLink, bInForSelectedNode, Title, Value, Color);
							return FSlateColor(Color);
						})
					]
				]
			]
		];
	};
	
	InRow.WholeRowContent()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.Padding(2.0f, 0.0f)
		.AutoWidth()
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			[
				SNew(SSpacer)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			.Padding(FMargin(5.0f, 2.0f))
			[
				SNew(STextBlock)
				.IsEnabled_Lambda([InAssetLink]() { return IsPDGLinked(InAssetLink); })
				.Text(FText::FromString(InTitleString))
				
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			.Padding(FMargin(5.0f, 2.0f))
			[
				SNew(SHorizontalBox)
				+ AddGridBox(TEXT("WAITING")) 
				+ AddGridBox(TEXT("COOKING")) 
				+ AddGridBox(TEXT("COOKED")) 
				+ AddGridBox(TEXT("FAILED")) 
			]
			+ SVerticalBox::Slot()
			[
				SNew(SSpacer)
			]
		]
	];
}


void
FHoudiniPDGDetails::AddTOPNetworkWidget(
	IDetailCategoryBuilder& InPDGCategory, UHoudiniPDGAssetLink* InPDGAssetLink )
{
	if (!InPDGAssetLink->GetSelectedTOPNetwork())
		return;

	if (InPDGAssetLink->AllTOPNetworks.Num() <= 0)
		return;

	TOPNetworksPtr.Reset();

	FString GroupLabel = TEXT("TOP Networks");
	IDetailGroup& TOPNetWorkGrp = InPDGCategory.AddGroup(FName(*GroupLabel), FText::FromString(GroupLabel), false, true);
	
	// Combobox: TOP Network
	{
		FDetailWidgetRow& PDGTOPNetRow = TOPNetWorkGrp.AddWidgetRow();
		// DisableIfPDGNotLinked(PDGTOPNetRow, InPDGAssetLink);
		PDGTOPNetRow.NameWidget.Widget = 
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("TOP Network")))
			];

		// Fill the TOP Networks SharedString array
		TOPNetworksPtr.SetNum(InPDGAssetLink->AllTOPNetworks.Num());
		for(int32 Idx = 0; Idx < InPDGAssetLink->AllTOPNetworks.Num(); Idx++)
		{
			const FTOPNetwork& Network = InPDGAssetLink->AllTOPNetworks[Idx];
			TOPNetworksPtr[Idx] = MakeShareable(new FTextAndTooltip(Network.NodeName, Network.NodePath));
		}

		if(TOPNetworksPtr.Num() <= 0)
			TOPNetworksPtr.Add(MakeShareable(new FTextAndTooltip("----")));

		// Lambda for selecting another TOPNet
		auto OnTOPNetChanged = [InPDGAssetLink](TSharedPtr<FTextAndTooltip> InNewChoice)
		{
			if (!InNewChoice.IsValid())
				return;

			const FString NewChoice = InNewChoice->Text;
			int32 NewSelectedIndex = -1;
			for (int32 Idx = 0; Idx < InPDGAssetLink->AllTOPNetworks.Num(); Idx++)
			{
				if (NewChoice == InPDGAssetLink->AllTOPNetworks[Idx].NodeName)
					NewSelectedIndex = Idx;
			}

			if (InPDGAssetLink->SelectedTOPNetworkIndex == NewSelectedIndex)
				return;

			if (NewSelectedIndex < 0)
				return;

			// Record a transaction for undo/redo
			FScopedTransaction Transaction(
				TEXT(HOUDINI_MODULE_RUNTIME),
				LOCTEXT("HoudiniPDGAssetLinkParameterChange", "Houdini PDG Asset Link Parameter: Changing a value"),
				InPDGAssetLink);

			InPDGAssetLink->Modify();
			InPDGAssetLink->SelectedTOPNetworkIndex = NewSelectedIndex;
			InPDGAssetLink->NotifyPostEditChangeProperty(GET_MEMBER_NAME_STRING_CHECKED(UHoudiniPDGAssetLink, SelectedTOPNetworkIndex));
		};
		
		TSharedPtr<SHorizontalBox, ESPMode::NotThreadSafe> HorizontalBoxTOPNet;
		TSharedPtr<SComboBox<TSharedPtr<FTextAndTooltip>>> ComboBoxTOPNet;
		int32 SelectedIndex = InPDGAssetLink->SelectedTOPNetworkIndex > 0 ? InPDGAssetLink->SelectedTOPNetworkIndex : 0;

		PDGTOPNetRow.ValueWidget.Widget = 
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(2, 2, 5, 2)
			.FillWidth(300.f)
			.MaxWidth(300.f)
			[
				SAssignNew(ComboBoxTOPNet, SComboBox<TSharedPtr<FTextAndTooltip>>)
				.OptionsSource(&TOPNetworksPtr)	
				.InitiallySelectedItem(TOPNetworksPtr[SelectedIndex])
				.OnGenerateWidget_Lambda([](TSharedPtr<FTextAndTooltip> ChoiceEntry)
				{
					const FText ChoiceEntryText = FText::FromString(ChoiceEntry->Text);
					const FText ChoiceEntryToolTip = FText::FromString(ChoiceEntry->ToolTip);
					return SNew(STextBlock)
					.Text(ChoiceEntryText)
					.ToolTipText(ChoiceEntryToolTip)
					.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")));
				})
				.OnSelectionChanged_Lambda([OnTOPNetChanged](TSharedPtr<FTextAndTooltip> NewChoice, ESelectInfo::Type SelectType)
				{
					return OnTOPNetChanged(NewChoice);
				})
				[
					SNew(STextBlock)
					.Text_Lambda([InPDGAssetLink]()
					{
						return FText::FromString(InPDGAssetLink->GetSelectedTOPNetworkName());
					})
					.ToolTipText_Lambda([InPDGAssetLink]()
					{
						FTOPNetwork const * const Network = InPDGAssetLink->GetSelectedTOPNetwork();
						if (Network)
							return FText::FromString(Network->NodePath);
						else
							return FText::FromString(Network->NodeName);
					})
					.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				]
			];
	}

	// Buttons: DIRTY ALL / COOK OUTPUT
	{
		FDetailWidgetRow& PDGDirtyCookRow = TOPNetWorkGrp.AddWidgetRow()
		.WholeRowContent()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			[
				SNew(SBox)
				.WidthOverride(200.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("DirtyAll", "Dirty All"))
					.ToolTipText(LOCTEXT("DirtyAllTooltip", "Dirty all TOP nodes in the selected TOP network and clears all of its work item results."))
					.ContentPadding(FMargin(5.0f, 5.0f))
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Center)
					.IsEnabled_Lambda([InPDGAssetLink]() { return IsPDGLinked(InPDGAssetLink) || (IsValid(InPDGAssetLink) && InPDGAssetLink->GetSelectedTOPNetwork()); })
					.OnClicked_Lambda([InPDGAssetLink]()
					{
						if (IsValid(InPDGAssetLink))
						{
							FTOPNetwork* const TOPNetwork = InPDGAssetLink->GetSelectedTOPNetwork();
							if (TOPNetwork)
							{
								if (IsPDGLinked(InPDGAssetLink))
								{
                                    FHoudiniPDGManager::DirtyAll(*TOPNetwork);
                                    // FHoudiniPDGDetails::RefreshUI(InPDGAssetLink);
                                }
                                else
                                {
                                    UHoudiniPDGAssetLink::ClearTOPNetworkWorkItemResults(*TOPNetwork);
                                }
							}
						}

						return FReply::Handled();
					})
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SBox)
				.WidthOverride(200.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("CookOut", "Cook Output"))
					.ToolTipText(LOCTEXT("CookOutTooltip", "Cooks the output nodes of the selected TOP network"))
					.ContentPadding(FMargin(5.0f, 5.0f))
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Center)
					.IsEnabled_Lambda([InPDGAssetLink]() { return IsPDGLinked(InPDGAssetLink); })
					.OnClicked_Lambda([InPDGAssetLink]()
					{
						if (InPDGAssetLink->GetSelectedTOPNetwork())
						{
							//InPDGAssetLink->WorkItemTally.ZeroAll();
							FHoudiniPDGManager::CookOutput(*(InPDGAssetLink->GetSelectedTOPNetwork()));
							// FHoudiniPDGDetails::RefreshUI(InPDGAssetLink);
						}
						return FReply::Handled();
					})
				]
			]
		];
		DisableIfPDGNotLinked(PDGDirtyCookRow, InPDGAssetLink);
	}

	// Buttons: PAUSE COOK / CANCEL COOK
	{
		FDetailWidgetRow& PDGDirtyCookRow = TOPNetWorkGrp.AddWidgetRow()
		.WholeRowContent()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SBox)
				.WidthOverride(200.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("Pause", "Pause Cook"))
					.ToolTipText(LOCTEXT("PauseTooltip", "Pauses cooking for the selected TOP Network"))
					.ContentPadding(FMargin(5.0f, 2.0f))
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Center)
					.IsEnabled_Lambda([InPDGAssetLink]() { return IsPDGLinked(InPDGAssetLink); })
					.OnClicked_Lambda([InPDGAssetLink]()
					{
						if (InPDGAssetLink->GetSelectedTOPNetwork())
						{
							//InPDGAssetLink->WorkItemTally.ZeroAll();
							FHoudiniPDGManager::PauseCook(*(InPDGAssetLink->GetSelectedTOPNetwork()));
							// FHoudiniPDGDetails::RefreshUI(InPDGAssetLink);
						}
						return FReply::Handled();
					})
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SBox)
				.WidthOverride(200.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("Cancel", "Cancel Cook"))
					.ToolTipText(LOCTEXT("CancelTooltip", "Cancels cooking the selected TOP network"))
					.ContentPadding(FMargin(5.0f, 2.0f))
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Center)
					.IsEnabled_Lambda([InPDGAssetLink]() { return IsPDGLinked(InPDGAssetLink); })
					.OnClicked_Lambda([InPDGAssetLink]()
					{
						if (InPDGAssetLink->GetSelectedTOPNetwork())
						{
							//InPDGAssetLink->WorkItemTally.ZeroAll();
							FHoudiniPDGManager::CancelCook(*(InPDGAssetLink->GetSelectedTOPNetwork()));
							// FHoudiniPDGDetails::RefreshUI(InPDGAssetLink);
						}
						return FReply::Handled();
					})
				]
			]
		];
		DisableIfPDGNotLinked(PDGDirtyCookRow, InPDGAssetLink);
	}
		
	// TOP NODE WIDGETS
	FHoudiniPDGDetails::AddTOPNodeWidget(TOPNetWorkGrp, InPDGAssetLink);
}

bool
FHoudiniPDGDetails::GetSelectedTOPNodeStatusAndColor(UHoudiniPDGAssetLink* InPDGAssetLink, FString& OutTOPNodeStatus, FLinearColor &OutTOPNodeStatusColor)
{
	OutTOPNodeStatus = FString();
	OutTOPNodeStatusColor = FLinearColor::White;
	if (IsValid(InPDGAssetLink))
	{
		FTOPNode* const TOPNode = InPDGAssetLink->GetSelectedTOPNode();
		if (TOPNode && !TOPNode->bHidden)
		{
			OutTOPNodeStatus = UHoudiniPDGAssetLink::GetTOPNodeStatus(*TOPNode);
			OutTOPNodeStatusColor = UHoudiniPDGAssetLink::GetTOPNodeStatusColor(*TOPNode);
			
			return true;
		}
	}

	return false;
}

void
FHoudiniPDGDetails::AddTOPNodeWidget(
	IDetailGroup& InGroup, UHoudiniPDGAssetLink* InPDGAssetLink )
{	
	if (!InPDGAssetLink->GetSelectedTOPNetwork())
		return;

	FString GroupLabel = TEXT("TOP Nodes");
	IDetailGroup& TOPNodesGrp = InGroup.AddGroup(FName(*GroupLabel), FText::FromString(GroupLabel), true);

	// Combobox: TOP Node
	{
		FDetailWidgetRow& PDGTOPNodeRow = TOPNodesGrp.AddWidgetRow();
		// DisableIfPDGNotLinked(PDGTOPNodeRow, InPDGAssetLink);
		PDGTOPNodeRow.NameWidget.Widget =
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("TOP Node")))
			];

		// Update the TOP Node SharedString
		TOPNodesPtr.Reset();
		for (int32 Idx = 0; Idx < InPDGAssetLink->GetSelectedTOPNetwork()->AllTOPNodes.Num(); Idx++)
		{
			if (InPDGAssetLink->GetSelectedTOPNetwork()->AllTOPNodes[Idx].bHidden)
				continue;
			const FTOPNode& Node = InPDGAssetLink->GetSelectedTOPNetwork()->AllTOPNodes[Idx];
			TOPNodesPtr.Add(
				MakeShareable(new FTextAndTooltip(Node.NodeName, Node.NodePath)));
		}

		FString NodeErrorText = FString();
		FString NodeErrorTooltip = FString();
		FLinearColor NodeErrorColor = FLinearColor::White;
		if (InPDGAssetLink->GetSelectedTOPNetwork()->AllTOPNodes.Num() <= 0)
		{
			TOPNodesPtr.Add(MakeShareable(new FTextAndTooltip("----")));
			NodeErrorText = TEXT("No valid TOP Node found!");
			NodeErrorTooltip = TEXT("There is no valid TOP Node found in the selected TOP Network!");
			NodeErrorColor = FLinearColor::Red;
		}
		else if(TOPNodesPtr.Num() <= 0)
		{
			TOPNodesPtr.Add(MakeShareable(new FTextAndTooltip("----")));
			NodeErrorText = TEXT("No visible TOP Node found!");
			NodeErrorTooltip = TEXT("No visible TOP Node found, all nodes in this network are hidden. Please update your TOP Node Filter.");
			NodeErrorColor = FLinearColor::Yellow;
		}

		// Lambda for selecting a TOPNode
		auto OnTOPNodeChanged = [InPDGAssetLink](TSharedPtr<FTextAndTooltip> InNewChoice)
		{
			if (!InNewChoice.IsValid() || !InPDGAssetLink->GetSelectedTOPNetwork())
				return;

			const FString NewChoice = InNewChoice->Text;

			int32 NewSelectedIndex = -1;
			for (int32 Idx = 0; Idx < InPDGAssetLink->GetSelectedTOPNetwork()->AllTOPNodes.Num(); Idx++)
			{
				if (NewChoice.Equals(InPDGAssetLink->GetSelectedTOPNetwork()->AllTOPNodes[Idx].NodeName))
				{
					NewSelectedIndex = Idx;
					break;
				}
			}

			// Allow selecting the same item twice, due to change in filter that could offset the indices!
			if((NewSelectedIndex >= 0))
			{
				FTOPNetwork* TOPNetwork = InPDGAssetLink->GetSelectedTOPNetwork();
				if (TOPNetwork)
				{
					// Record a transaction for undo/redo
					FScopedTransaction Transaction(
						TEXT(HOUDINI_MODULE_RUNTIME),
						LOCTEXT("HoudiniPDGAssetLinkParameterChange", "Houdini PDG Asset Link Parameter: Changing a value"),
						InPDGAssetLink);
					
					InPDGAssetLink->Modify();
					TOPNetwork->SelectedTOPIndex = NewSelectedIndex;
					InPDGAssetLink->NotifyPostEditChangeProperty(FName(
						FString::Printf(TEXT("%s[%d].%s"),
							GET_MEMBER_NAME_STRING_CHECKED(UHoudiniPDGAssetLink, AllTOPNetworks),
							InPDGAssetLink->SelectedTOPNetworkIndex,
							GET_MEMBER_NAME_STRING_CHECKED(FTOPNetwork, SelectedTOPIndex))
					));
				}
				
			}

			// FHoudiniPDGDetails::RefreshUI(InPDGAssetLink);
		};
		
		TSharedPtr<SHorizontalBox, ESPMode::NotThreadSafe> HorizontalBoxTOPNode;
		TSharedPtr<SComboBox<TSharedPtr<FTextAndTooltip>>> ComboBoxTOPNode;
		int32 SelectedIndex = 0;
		if (InPDGAssetLink->GetSelectedTOPNetwork()->SelectedTOPIndex >= 0)
		{
			//SelectedIndex = InPDGAssetLink->GetSelectedTOPNetwork()->SelectedTOPIndex;

			// We need to match the selection by name, not via indices!
			// Because of the nodefilter, it is possible that the selected index is no longer valid!
			FString SelectTOPNodeName = InPDGAssetLink->GetSelectedTOPNodeName();

			// Find the matching UI index
			for (int32 UIIndex = 0; UIIndex < TOPNodesPtr.Num(); UIIndex++)
			{
				if (TOPNodesPtr[UIIndex] && TOPNodesPtr[UIIndex]->Text != SelectTOPNodeName)
					continue;

				// We found the UI Index that matches the current TOP Node!
				SelectedIndex = UIIndex;
			}
		}

		TSharedPtr<STextBlock> ErrorText;

		PDGTOPNodeRow.ValueWidget.Widget = 
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(2, 2, 5, 2)
			.FillWidth(300.f)
			.MaxWidth(300.f)
			[
				SAssignNew(ComboBoxTOPNode, SComboBox<TSharedPtr<FTextAndTooltip>>)
				.OptionsSource(&TOPNodesPtr)
				.InitiallySelectedItem(TOPNodesPtr[SelectedIndex])
				.OnGenerateWidget_Lambda([](TSharedPtr<FTextAndTooltip> ChoiceEntry)
				{
					const FText ChoiceEntryText = FText::FromString(ChoiceEntry->Text);
					const FText ChoiceEntryToolTip = FText::FromString(ChoiceEntry->ToolTip);
					return SNew(STextBlock)
					.Text(ChoiceEntryText)
					.ToolTipText(ChoiceEntryToolTip)
					.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")));
				})
				.OnSelectionChanged_Lambda([OnTOPNodeChanged](TSharedPtr<FTextAndTooltip> NewChoice, ESelectInfo::Type SelectType)
				{
					return OnTOPNodeChanged(NewChoice);
				})
				[
					SNew(STextBlock)
					.Text_Lambda([InPDGAssetLink, ComboBoxTOPNode, Options = TOPNodesPtr]()
					{
						if (IsValid(InPDGAssetLink))
							return FText::FromString(InPDGAssetLink->GetSelectedTOPNodeName());
						else
							return FText();
					})
					.ToolTipText_Lambda([InPDGAssetLink]()
					{
						FTOPNode const * const TOPNode = InPDGAssetLink->GetSelectedTOPNode();
						if (TOPNode)
						{
							if (!TOPNode->NodePath.IsEmpty())
								return FText::FromString(TOPNode->NodePath);
							else
								return FText::FromString(TOPNode->NodeName);
						}
						else
						{
							return FText();
						}
					})
					.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				]
			]
			+ SHorizontalBox::Slot()
			.Padding(2, 2, 5, 2)
			.AutoWidth()
			[
				SAssignNew(ErrorText, STextBlock)
				.Text(FText::FromString(NodeErrorText))
				.ToolTipText(FText::FromString(NodeErrorText))
				.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				.ColorAndOpacity(FLinearColor::Red)
				//.ShadowColorAndOpacity(FLinearColor::Black)
			];

		// Update the error text if needed
		ErrorText->SetText(FText::FromString(NodeErrorText));
		ErrorText->SetToolTipText(FText::FromString(NodeErrorTooltip));
		ErrorText->SetColorAndOpacity(NodeErrorColor);

		// Hide the combobox if we have an error
		ComboBoxTOPNode->SetVisibility(NodeErrorText.IsEmpty() ? EVisibility::Visible : EVisibility::Hidden);
	}

	// TOP Node State
	{
		FDetailWidgetRow& PDGNodeStateResultRow = TOPNodesGrp.AddWidgetRow();
		DisableIfPDGNotLinked(PDGNodeStateResultRow, InPDGAssetLink);
		PDGNodeStateResultRow.NameWidget.Widget =
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("TOP Node State")))
			];

		PDGNodeStateResultRow.ValueWidget.Widget =
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text_Lambda([InPDGAssetLink]()
				{
					FString TOPNodeStatus = FString();
					FLinearColor TOPNodeStatusColor = FLinearColor::White;
					GetSelectedTOPNodeStatusAndColor(InPDGAssetLink, TOPNodeStatus, TOPNodeStatusColor);
					return FText::FromString(TOPNodeStatus);
				})
				.ColorAndOpacity_Lambda([InPDGAssetLink]()
				{
					FString TOPNodeStatus = FString();
					FLinearColor TOPNodeStatusColor = FLinearColor::White;
					GetSelectedTOPNodeStatusAndColor(InPDGAssetLink, TOPNodeStatus, TOPNodeStatusColor);
					return FSlateColor(TOPNodeStatusColor);
				})
			];
	}
	
	// Checkbox: Load Work Item Output Files
	{
		FText Tooltip = FText::FromString(TEXT("When enabled, Output files produced by this TOP Node's Work Items will automatically be loaded when cooked."));

		FDetailWidgetRow& PDGNodeAutoLoadRow = TOPNodesGrp.AddWidgetRow();
		DisableIfPDGNotLinked(PDGNodeAutoLoadRow, InPDGAssetLink);
		PDGNodeAutoLoadRow.NameWidget.Widget =
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("Load Work Item Output Files")))
				.ToolTipText(Tooltip)
			];

		TSharedPtr<SCheckBox> AutoLoadCheckBox;
		
		PDGNodeAutoLoadRow.ValueWidget.Widget =
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f, 0.0f)
			[
				// Checkbox
				SAssignNew(AutoLoadCheckBox, SCheckBox)
				.IsChecked_Lambda([InPDGAssetLink]()
				{			
					return InPDGAssetLink->GetSelectedTOPNode() 
						? (InPDGAssetLink->GetSelectedTOPNode()->bAutoLoad ? ECheckBoxState::Checked : ECheckBoxState::Unchecked) 
						: ECheckBoxState::Unchecked;
				})
				.OnCheckStateChanged_Lambda([InPDGAssetLink](ECheckBoxState NewState)
				{
					const bool bNewState = (NewState == ECheckBoxState::Checked) ? true : false;
					FTOPNode* TOPNode = InPDGAssetLink->GetSelectedTOPNode();
					if (!TOPNode || TOPNode->bAutoLoad == bNewState)
						return;

					// Record a transaction for undo/redo
					FScopedTransaction Transaction(
						TEXT(HOUDINI_MODULE_RUNTIME),
						LOCTEXT("HoudiniPDGAssetLinkParameterChange", "Houdini PDG Asset Link Parameter: Changing a value"),
						InPDGAssetLink);

					InPDGAssetLink->Modify();
					TOPNode->bAutoLoad = bNewState;
					InPDGAssetLink->NotifyPostEditChangeProperty(FName(
						FString::Printf(TEXT("%s[%d].%s[%d].%s"),
							GET_MEMBER_NAME_STRING_CHECKED(UHoudiniPDGAssetLink, AllTOPNetworks),
							InPDGAssetLink->SelectedTOPNetworkIndex,
							GET_MEMBER_NAME_STRING_CHECKED(FTOPNetwork, AllTOPNodes),
							InPDGAssetLink->GetSelectedTOPNetwork()->SelectedTOPIndex,
							GET_MEMBER_NAME_STRING_CHECKED(FTOPNode, bAutoLoad)
						)
					));
					
					// FHoudiniPDGDetails::RefreshUI(InPDGAssetLink);
				})
				.ToolTipText(Tooltip)
				.IsEnabled_Lambda([InPDGAssetLink]()
				{
					if (InPDGAssetLink->GetSelectedTOPNode() && !InPDGAssetLink->GetSelectedTOPNode()->bHidden)
						return true;
					return false;
				})
			];
	}
	
	// Checkbox: Work Item Output Files Visible
	{
		FText Tooltip = FText::FromString(TEXT("Toggles the visibility of the actors created from this TOP Node's Work Item File Outputs."));
		FDetailWidgetRow& PDGNodeShowResultRow = TOPNodesGrp.AddWidgetRow();
		// DisableIfPDGNotLinked(PDGNodeShowResultRow, InPDGAssetLink);
		PDGNodeShowResultRow.NameWidget.Widget =
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("Work Item Output Files Visible")))
				.ToolTipText(Tooltip)
			];

		TSharedPtr<SCheckBox> ShowResCheckBox;
		PDGNodeShowResultRow.ValueWidget.Widget =
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f, 0.0f)
			[
				// Checkbox
				SAssignNew(ShowResCheckBox, SCheckBox)
				.IsChecked_Lambda([InPDGAssetLink]()
				{
					return InPDGAssetLink->GetSelectedTOPNode() 
						? (InPDGAssetLink->GetSelectedTOPNode()->IsVisibleInLevel() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked) 
						: ECheckBoxState::Unchecked;
				})
				.OnCheckStateChanged_Lambda([InPDGAssetLink](ECheckBoxState NewState)
				{
					const bool bNewState = (NewState == ECheckBoxState::Checked) ? true : false;
					if (!InPDGAssetLink->GetSelectedTOPNode() || InPDGAssetLink->GetSelectedTOPNode()->IsVisibleInLevel() == bNewState)
						return;

					// Record a transaction for undo/redo
					FScopedTransaction Transaction(
						TEXT(HOUDINI_MODULE_RUNTIME),
						LOCTEXT("HoudiniPDGAssetLinkParameterChange", "Houdini PDG Asset Link Parameter: Changing a value"),
						InPDGAssetLink);

					InPDGAssetLink->Modify();
					InPDGAssetLink->GetSelectedTOPNode()->SetVisibleInLevel(bNewState);
					InPDGAssetLink->NotifyPostEditChangeProperty(FName(
						FString::Printf(TEXT("%s[%d].%s[%d].%s"),
							GET_MEMBER_NAME_STRING_CHECKED(UHoudiniPDGAssetLink, AllTOPNetworks),
							InPDGAssetLink->SelectedTOPNetworkIndex,
							GET_MEMBER_NAME_STRING_CHECKED(FTOPNetwork, AllTOPNodes),
							InPDGAssetLink->GetSelectedTOPNetwork()->SelectedTOPIndex,
							TEXT("bShow")  // GET_MEMBER_NAME_STRING_CHECKED(FTOPNode, bShow)
						)
					));
					// FHoudiniPDGDetails::RefreshUI(InPDGAssetLink);
				})
				.ToolTipText(Tooltip)
				.IsEnabled_Lambda([InPDGAssetLink]()
				{
					if (InPDGAssetLink->GetSelectedTOPNode() && !InPDGAssetLink->GetSelectedTOPNode()->bHidden)
						return true;
					return false;
				})
			];
	}

	// Buttons: DIRTY NODE / DIRTY ALL TASKS / COOK NODE
	{
		TSharedPtr<SButton> DirtyButton;
		TSharedPtr<SButton> CookButton;

		FDetailWidgetRow& PDGDirtyCookRow = TOPNodesGrp.AddWidgetRow()
		.WholeRowContent()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SBox)
				.IsEnabled_Lambda([InPDGAssetLink]() { return IsPDGLinked(InPDGAssetLink) || (IsValid(InPDGAssetLink) && InPDGAssetLink->GetSelectedTOPNode()); })
				.WidthOverride(200.0f)
				[
					SAssignNew(DirtyButton, SButton)
					.Text(LOCTEXT("DirtyNode", "Dirty Node"))
					.ToolTipText(LOCTEXT("DirtyNodeTooltip", "Dirties the selected TOP node and clears its work item results."))
					.ContentPadding(FMargin(5.0f, 2.0f))
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Center)
					.IsEnabled_Lambda([InPDGAssetLink]() { return IsPDGLinked(InPDGAssetLink) || (IsValid(InPDGAssetLink) && InPDGAssetLink->GetSelectedTOPNode()); })
					.OnClicked_Lambda([InPDGAssetLink]()
					{
						if (IsValid(InPDGAssetLink))
						{
							FTOPNode* const TOPNode = InPDGAssetLink->GetSelectedTOPNode();
							if (TOPNode)
							{
								if (IsPDGLinked(InPDGAssetLink))
								{
                                    FHoudiniPDGManager::DirtyTOPNode(*TOPNode);
                                    // FHoudiniPDGDetails::RefreshUI(InPDGAssetLink);
                                }
                                else
                                {
                                    UHoudiniPDGAssetLink::ClearTOPNodeWorkItemResults(*TOPNode);
                                }
							}
						}
						
						return FReply::Handled();
					})
					.IsEnabled_Lambda([InPDGAssetLink]()
					{
						if (InPDGAssetLink->GetSelectedTOPNode() && !InPDGAssetLink->GetSelectedTOPNode()->bHidden)
							return true;
						return false;
					})
				]
			]
			// + SHorizontalBox::Slot()
			// .AutoWidth()
			// [
			// 	SNew(SBox)
			// 	.WidthOverride(200.0f)
			// 	[
			// 		SAssignNew(DirtyButton, SButton)
			// 		.Text(LOCTEXT("DirtyAllTasks", "Dirty All Tasks"))
			// 		.ToolTipText(LOCTEXT("DirtyAllTasksTooltip", "Dirties all tasks/work items on the selected TOP node."))
			// 		.ContentPadding(FMargin(5.0f, 2.0f))
			// 		.VAlign(VAlign_Center)
			// 		.HAlign(HAlign_Center)
			// 		.OnClicked_Lambda([InPDGAssetLink]()
			// 		{	
			// 			if(InPDGAssetLink->GetSelectedTOPNode())
			// 			{
			// 				FHoudiniPDGManager::DirtyAllTasksOfTOPNode(*(InPDGAssetLink->GetSelectedTOPNode()));
			// 				FHoudiniPDGDetails::RefreshUI(InPDGAssetLink);
			// 			}
			// 			
			// 			return FReply::Handled();
			// 		})
			// 	]
			// ]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SBox)
				.IsEnabled_Lambda([InPDGAssetLink]() { return IsPDGLinked(InPDGAssetLink); })
				.WidthOverride(200.0f)
				[
					SAssignNew(CookButton, SButton)
					.Text(LOCTEXT("CookNode", "Cook Node"))
					.ToolTipText(LOCTEXT("CookNodeTooltip", "Cooks the selected TOP Node."))
					.ContentPadding(FMargin(5.0f, 2.0f))
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Center)
					.IsEnabled_Lambda([InPDGAssetLink]() { return IsPDGLinked(InPDGAssetLink); })
					.OnClicked_Lambda([InPDGAssetLink]()
					{
						if (InPDGAssetLink->GetSelectedTOPNode())
						{
							FHoudiniPDGManager::CookTOPNode(*(InPDGAssetLink->GetSelectedTOPNode()));
							// FHoudiniPDGDetails::RefreshUI(InPDGAssetLink);
						}
						return FReply::Handled();
					})
					.IsEnabled_Lambda([InPDGAssetLink]()
					{
						if (InPDGAssetLink->GetSelectedTOPNode() && !InPDGAssetLink->GetSelectedTOPNode()->bHidden)
							return true;
						return false;
					})
				]
			]
		];
		DisableIfPDGNotLinked(PDGDirtyCookRow, InPDGAssetLink);
	}
	
	// TOP Node WorkItem Status
	{
		if (InPDGAssetLink->GetSelectedTOPNode())
		{
			FDetailWidgetRow& PDGNodeWorkItemStatsRow = TOPNodesGrp.AddWidgetRow();
			DisableIfPDGNotLinked(PDGNodeWorkItemStatsRow, InPDGAssetLink);
			FHoudiniPDGDetails::AddWorkItemStatusWidget(
				PDGNodeWorkItemStatsRow, TEXT("TOP Node Work Item Status"), InPDGAssetLink, true);
		}
	}
}

void
FHoudiniPDGDetails::RefreshPDGAssetLink(UHoudiniPDGAssetLink* InPDGAssetLink)
{
	// Repopulate the network and nodes for the assetlink
	if (!FHoudiniPDGManager::UpdatePDGAssetLink(InPDGAssetLink))
		return;
	
	FHoudiniPDGDetails::RefreshUI(InPDGAssetLink, true);
}

void
FHoudiniPDGDetails::RefreshUI(UHoudiniPDGAssetLink* InPDGAssetLink, const bool& InFullUpdate)
{
	if (!InPDGAssetLink || InPDGAssetLink->IsPendingKill())
		return;

	// Update the workitem stats
	InPDGAssetLink->UpdateWorkItemTally();

	// Update the editor properties
	FHoudiniEngineUtils::UpdateEditorProperties(InPDGAssetLink, InFullUpdate);
}

void 
FHoudiniPDGDetails::CreatePDGBakeWidgets(IDetailCategoryBuilder& InPDGCategory, UHoudiniPDGAssetLink* InPDGAssetLink) 
{
	if (!InPDGAssetLink || InPDGAssetLink->IsPendingKill())
		return;

	FHoudiniEngineDetails::AddHeaderRowForHoudiniPDGAssetLink(InPDGCategory, InPDGAssetLink, HOUDINI_ENGINE_UI_SECTION_PDG_BAKE);

	if (!InPDGAssetLink->bBakeMenuExpanded)
		return;

	auto OnBakeButtonClickedLambda = [InPDGAssetLink]() 
	{
		switch (InPDGAssetLink->HoudiniEngineBakeOption)
		{
		case EHoudiniEngineBakeOption::ToActor:
		{
			// if (InPDGAssetLink->bIsReplace)
			// 	FHoudiniEngineBakeUtils::ReplaceHoudiniActorWithActors(InPDGAssetLink);
			// else
				FHoudiniEngineBakeUtils::BakePDGAssetLinkOutputsKeepActors(InPDGAssetLink);
		}
		break;
		
		case EHoudiniEngineBakeOption::ToBlueprint:
		{
			// if (InPDGAssetLink->bIsReplace)
			// 	FHoudiniEngineBakeUtils::ReplaceWithBlueprint(InPDGAssetLink);
			// else
				FHoudiniEngineBakeUtils::BakePDGAssetLinkBlueprints(InPDGAssetLink);
		}
		break;
		//
		// case EHoudiniEngineBakeOption::ToFoliage:
		// {
		// 	if (InPDGAssetLink->bIsReplace)
		// 		FHoudiniEngineBakeUtils::ReplaceHoudiniActorWithFoliage(InPDGAssetLink);
		// 	else
		// 		FHoudiniEngineBakeUtils::BakeHoudiniActorToFoliage(InPDGAssetLink);
		// }
		// break;
		//
		// case EHoudiniEngineBakeOption::ToWorldOutliner:
		// {
		// 	if (InPDGAssetLink->bIsReplace)
		// 	{
		// 		// Todo
		// 	}
		// 	else
		// 	{
		// 		//Todo
		// 	}
		// }
		// break;
		}
		
		return FReply::Handled();	
	};

	auto OnBakeFolderTextCommittedLambda = [InPDGAssetLink](const FText& Val, ETextCommit::Type TextCommitType)
	{
		FString NewPathStr = Val.ToString();
		if (NewPathStr.IsEmpty())
			return;

		// Record a transaction for undo/redo
		FScopedTransaction Transaction(
			TEXT(HOUDINI_MODULE_RUNTIME),
			LOCTEXT("HoudiniPDGAssetLinkParameterChange", "Houdini PDG Asset Link Parameter: Changing a value"),
			InPDGAssetLink);

		//Todo? Check if the new Bake folder path is valid
		InPDGAssetLink->Modify();
		InPDGAssetLink->BakeFolder.Path = NewPathStr;
		InPDGAssetLink->NotifyPostEditChangeProperty(
			GET_MEMBER_NAME_STRING_CHECKED(UHoudiniPDGAssetLink, BakeFolder));
	};

	// Button Row
	FDetailWidgetRow & ButtonRow = InPDGCategory.AddCustomRow(FText::GetEmpty());
	DisableIfPDGNotLinked(ButtonRow, InPDGAssetLink);

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
			//.ToolTipText(LOCTEXT("HoudiniPDGDetailsBakeButton", "Bake the Houdini PDG TOP Node(s)"))
			.ToolTipText_Lambda([InPDGAssetLink]()
			{
				switch (InPDGAssetLink->HoudiniEngineBakeOption) 
				{
					case EHoudiniEngineBakeOption::ToActor:
					{
						return LOCTEXT(
							"HoudiniEnginePDGBakeButtonBakeToActorToolTip",
							"Bake this Houdini PDG Asset's output assets and seperate the output actors from the PDG asset link.");
					}
					break;
					case EHoudiniEngineBakeOption::ToBlueprint:
					{
						return LOCTEXT(
							"HoudiniEnginePDGBakeButtonBakeToBlueprintToolTip", 
							"Bake this Houdini PDG Asset's output assets to blueprints and remove temporary output actors that no "
							"longer has output components from the PDG asset link.");
					}
					break;
					default:
					{
						return FText();
					}
				}
			})
			.Visibility(EVisibility::Visible)
			.IsEnabled_Lambda([InPDGAssetLink]() { return IsPDGLinked(InPDGAssetLink); })
			.OnClicked_Lambda(OnBakeButtonClickedLambda)
		]
	];
	
	// bake Type ComboBox
	TSharedPtr<SComboBox<TSharedPtr<FString>>> TypeComboBox;

	TArray<TSharedPtr<FString>>* OptionSource = FHoudiniEngineEditor::Get().GetHoudiniEnginePDGBakeTypeOptionsLabels();
	TSharedPtr<FString> IntialSelec;
	if (OptionSource) 
	{
		// IntialSelec = (*OptionSource)[(int)InPDGAssetLink->HoudiniEngineBakeOption];
		const FString DefaultStr = FHoudiniEngineEditor::Get().GetStringFromHoudiniEngineBakeOption(InPDGAssetLink->HoudiniEngineBakeOption);
		const TSharedPtr<FString>* DefaultOption = OptionSource->FindByPredicate(
			[DefaultStr](TSharedPtr<FString> InStringPtr)
			{
				return InStringPtr.IsValid() && *InStringPtr == DefaultStr;
			}
		);
		if (DefaultOption)
			IntialSelec = *DefaultOption;
	}

	ButtonRowHorizontalBox->AddSlot()
	/*.AutoWidth()*/
	.Padding(3.0, 0.0, 4.0f, 0.0f)
	.MaxWidth(103.f)
	[
		SNew(SBox)
		.IsEnabled_Lambda([InPDGAssetLink]() { return IsPDGLinked(InPDGAssetLink); })
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
				[InPDGAssetLink](TSharedPtr< FString > NewChoice, ESelectInfo::Type SelectType)
			{
				if (!NewChoice.IsValid() || SelectType == ESelectInfo::Type::Direct)
					return;

				const EHoudiniEngineBakeOption NewOption = 
					FHoudiniEngineEditor::Get().StringToHoudiniEngineBakeOption(*NewChoice.Get());

				if (NewOption != InPDGAssetLink->HoudiniEngineBakeOption)
				{
					// Record a transaction for undo/redo
					FScopedTransaction Transaction(
						TEXT(HOUDINI_MODULE_RUNTIME),
						LOCTEXT("HoudiniPDGAssetLinkParameterChange", "Houdini PDG Asset Link Parameter: Changing a value"),
						InPDGAssetLink);
					
					InPDGAssetLink->Modify();
					InPDGAssetLink->HoudiniEngineBakeOption = NewOption;
					InPDGAssetLink->NotifyPostEditChangeProperty(
						GET_MEMBER_NAME_STRING_CHECKED(UHoudiniPDGAssetLink, HoudiniEngineBakeOption));
				}
			})
			[
				SNew(STextBlock)
				.Text_Lambda([InPDGAssetLink, TypeComboBox, OptionSource]() 
				{
					return FText::FromString(FHoudiniEngineEditor::Get().GetStringFromHoudiniEngineBakeOption(InPDGAssetLink->HoudiniEngineBakeOption));
				})
				.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			]
		]
	];
	
	// bake selection ComboBox
	TSharedPtr<SComboBox<TSharedPtr<FString>>> BakeSelectionComboBox;

	TArray<TSharedPtr<FString>>* PDGBakeSelectionOptionSource = FHoudiniEngineEditor::Get().GetHoudiniEnginePDGBakeSelectionOptionsLabels();
	TSharedPtr<FString> PDGBakeSelectionIntialSelec;
	if (PDGBakeSelectionOptionSource) 
	{
		PDGBakeSelectionIntialSelec = (*PDGBakeSelectionOptionSource)[(int)InPDGAssetLink->PDGBakeSelectionOption];
	}

	ButtonRowHorizontalBox->AddSlot()
	/*.AutoWidth()*/
	.Padding(3.0, 0.0, 4.0f, 0.0f)
	.MaxWidth(163.f)
	[
		SNew(SBox)
		.IsEnabled_Lambda([InPDGAssetLink]() { return IsPDGLinked(InPDGAssetLink); })
		.WidthOverride(163.f)
		[
			SAssignNew(TypeComboBox, SComboBox<TSharedPtr<FString>>)
			.OptionsSource(PDGBakeSelectionOptionSource)
			.InitiallySelectedItem(PDGBakeSelectionIntialSelec)
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
				[InPDGAssetLink](TSharedPtr< FString > NewChoice, ESelectInfo::Type SelectType)
			{
				if (!NewChoice.IsValid())
					return;

				const EPDGBakeSelectionOption NewOption = 
					FHoudiniEngineEditor::Get().StringToPDGBakeSelectionOption(*NewChoice.Get());

				if (NewOption != InPDGAssetLink->PDGBakeSelectionOption)
				{
					// Record a transaction for undo/redo
					FScopedTransaction Transaction(
						TEXT(HOUDINI_MODULE_RUNTIME),
						LOCTEXT("HoudiniPDGAssetLinkParameterChange", "Houdini PDG Asset Link Parameter: Changing a value"),
						InPDGAssetLink);

					InPDGAssetLink->Modify();
					InPDGAssetLink->PDGBakeSelectionOption = NewOption;
					InPDGAssetLink->NotifyPostEditChangeProperty(
						GET_MEMBER_NAME_STRING_CHECKED(UHoudiniPDGAssetLink, PDGBakeSelectionOption));
				}
			})
			[
				SNew(STextBlock)
				.Text_Lambda([InPDGAssetLink]() 
				{ 
					return FText::FromString(
						FHoudiniEngineEditor::Get().GetStringFromPDGBakeTargetOption(InPDGAssetLink->PDGBakeSelectionOption));
				})
				.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			]
		]
	];

	ButtonRow.WholeRowWidget.Widget = ButtonRowHorizontalBox;

	// Bake package replacement mode row
	FDetailWidgetRow & BakePackageReplaceRow = InPDGCategory.AddCustomRow(FText::GetEmpty());
	DisableIfPDGNotLinked(BakePackageReplaceRow, InPDGAssetLink);

	TSharedRef<SHorizontalBox> BakePackageReplaceRowHorizontalBox = SNew(SHorizontalBox);
	
	BakePackageReplaceRowHorizontalBox->AddSlot()
	/*.AutoWidth()*/
	.Padding(30.0f, 0.0f, 6.0f, 0.0f)
	.MaxWidth(155.0f)
	[
		SNew(SBox)
		.IsEnabled_Lambda([InPDGAssetLink]() { return IsPDGLinked(InPDGAssetLink); })
		.WidthOverride(155.0f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("HoudiniEnginePDGBakePackageReplacementModeLabel", "Package Replace Mode"))
			.ToolTipText(
				LOCTEXT("HoudiniEnginePDGBakePackageReplacementModeTooltip", "Package replacement mode "
					"during baking. Create new assets, using numerical suffixes in package names when necessary, or "
					"replace existing assets with matching names."))
		]
	];

	// bake package replace mode ComboBox
	TSharedPtr<SComboBox<TSharedPtr<FString>>> BakePackageReplaceModeComboBox;

	TArray<TSharedPtr<FString>>* PDGBakePackageReplaceModeOptionSource = FHoudiniEngineEditor::Get().GetHoudiniEnginePDGBakePackageReplaceModeOptionsLabels();
	TSharedPtr<FString> PDGBakePackageReplaceModeInitialSelec;
	if (PDGBakePackageReplaceModeOptionSource) 
	{
		const FString DefaultStr = FHoudiniEngineEditor::Get().GetStringFromPDGBakePackageReplaceModeOption(InPDGAssetLink->PDGBakePackageReplaceMode);
		const TSharedPtr<FString>* DefaultOption = PDGBakePackageReplaceModeOptionSource->FindByPredicate(
			[DefaultStr](TSharedPtr<FString> InStringPtr)
			{
				return InStringPtr.IsValid() && *InStringPtr == DefaultStr;
			}
		);
		if (DefaultOption)
			PDGBakePackageReplaceModeInitialSelec = *DefaultOption;
	}

	BakePackageReplaceRowHorizontalBox->AddSlot()
	/*.AutoWidth()*/
	.Padding(3.0, 0.0, 4.0f, 0.0f)
	.MaxWidth(163.f)
	[
		SNew(SBox)
		.IsEnabled_Lambda([InPDGAssetLink]() { return IsPDGLinked(InPDGAssetLink); })
		.WidthOverride(163.f)
		[
			SAssignNew(TypeComboBox, SComboBox<TSharedPtr<FString>>)
			.OptionsSource(PDGBakePackageReplaceModeOptionSource)
			.InitiallySelectedItem(PDGBakePackageReplaceModeInitialSelec)
			.OnGenerateWidget_Lambda(
				[](TSharedPtr< FString > InItem)
			{
				const FText ChoiceEntryText = FText::FromString(*InItem);
				return SNew(STextBlock)
						.Text(ChoiceEntryText)
						.ToolTipText(ChoiceEntryText)
						.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")));
			})
			.OnSelectionChanged_Lambda(
				[InPDGAssetLink](TSharedPtr< FString > NewChoice, ESelectInfo::Type SelectType)
			{
				if (!NewChoice.IsValid())
					return;

				const EPDGBakePackageReplaceModeOption NewOption = 
					FHoudiniEngineEditor::Get().StringToPDGBakePackageReplaceModeOption(*NewChoice.Get());

				if (NewOption != InPDGAssetLink->PDGBakePackageReplaceMode)
				{
					// Record a transaction for undo/redo
                    FScopedTransaction Transaction(
                        TEXT(HOUDINI_MODULE_RUNTIME),
                        LOCTEXT("HoudiniPDGAssetLinkParameterChange", "Houdini PDG Asset Link Parameter: Changing a value"),
                        InPDGAssetLink);
						
                    InPDGAssetLink->Modify();
                    InPDGAssetLink->PDGBakePackageReplaceMode = NewOption;
					InPDGAssetLink->NotifyPostEditChangeProperty(
						GET_MEMBER_NAME_STRING_CHECKED(UHoudiniPDGAssetLink, PDGBakePackageReplaceMode));
				}
			})
			[
				SNew(STextBlock)
				.Text_Lambda([InPDGAssetLink]() 
				{ 
					return FText::FromString(
						FHoudiniEngineEditor::Get().GetStringFromPDGBakePackageReplaceModeOption(InPDGAssetLink->PDGBakePackageReplaceMode));
				})
				.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			]
		]
	];

	BakePackageReplaceRow.WholeRowWidget.Widget = BakePackageReplaceRowHorizontalBox;

	// Bake Folder Row
	FDetailWidgetRow & BakeFolderRow = InPDGCategory.AddCustomRow(FText::GetEmpty());
	DisableIfPDGNotLinked(BakeFolderRow, InPDGAssetLink);

	TSharedRef<SHorizontalBox> BakeFolderRowHorizontalBox = SNew(SHorizontalBox);

	BakeFolderRowHorizontalBox->AddSlot()
	/*.AutoWidth()*/
	.Padding(30.0f, 0.0f, 6.0f, 0.0f)
	.MaxWidth(155.0f)
	[
		SNew(SBox)
		.IsEnabled_Lambda([InPDGAssetLink]() { return IsPDGLinked(InPDGAssetLink); })
		.WidthOverride(155.0f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("HoudiniEngineBakeFolderLabel", "Bake Folder"))
			.ToolTipText(LOCTEXT("HoudiniEnginePDGBakeFolderTooltip", "Default folder used to store the objects that are generated by this Houdini PDG Asset when baking."))
		]
	];

	BakeFolderRowHorizontalBox->AddSlot()
	/*.AutoWidth()*/
	.MaxWidth(235.0)
	[
		SNew(SBox)
		.IsEnabled_Lambda([InPDGAssetLink]() { return IsPDGLinked(InPDGAssetLink); })
		.WidthOverride(235.0f)
		[
			SNew(SEditableTextBox)
			.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
			.ToolTipText(LOCTEXT("HoudiniEnginePDGBakeFolderTooltip", "Default folder used to store the objects that are generated by this Houdini PDG Asset when baking."))
			.HintText(LOCTEXT("HoudiniEngineBakeFolderHintText", "Input to set bake folder"))
			.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			.Text_Lambda([InPDGAssetLink](){ return FText::FromString(InPDGAssetLink->BakeFolder.Path); })
			.OnTextCommitted_Lambda(OnBakeFolderTextCommittedLambda)
		]
	];

	BakeFolderRow.WholeRowWidget.Widget = BakeFolderRowHorizontalBox;
}

FTextAndTooltip::FTextAndTooltip(const FString& InText)
	: Text(InText)
{
}

FTextAndTooltip::FTextAndTooltip(const FString& InText, const FString &InToolTip)
	: Text(InText)
	, ToolTip(InToolTip)
{
}

FTextAndTooltip::FTextAndTooltip(FString&& InText)
	: Text(InText)
{
}

FTextAndTooltip::FTextAndTooltip(FString&& InText, FString&& InToolTip)
	: Text(InText)
	, ToolTip(InToolTip)
{
}

#undef LOCTEXT_NAMESPACE