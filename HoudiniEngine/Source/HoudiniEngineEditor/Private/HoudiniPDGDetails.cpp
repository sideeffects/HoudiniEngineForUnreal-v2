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

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "IDetailGroup.h"
#include "IDetailCustomization.h"
#include "PropertyCustomizationHelpers.h"
#include "DetailWidgetRow.h"
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
	AddPDGAssetStatus(InPDGCategory, InPDGAssetLink->LinkState);

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
		// Lambda for changing the filter value
		auto ChangeTOPNodeFilter = [InPDGAssetLink](const FString& NewValue)
		{
			if (InPDGAssetLink->TOPNodeFilter.Equals(NewValue))
				return;

			InPDGAssetLink->TOPNodeFilter = NewValue;
			FHoudiniPDGDetails::RefreshPDGAssetLink(InPDGAssetLink);
		};

		FDetailWidgetRow& PDGFilterRow = InPDGCategory.AddCustomRow(FText::GetEmpty());
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

					InPDGAssetLink->bUseTOPNodeFilter = bNewState;
					FHoudiniPDGDetails::RefreshPDGAssetLink(InPDGAssetLink);
				})
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("TOP Node Filter")))
			];
		
		PDGFilterRow.ValueWidget.Widget =
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.0f)
			[
				SNew(SEditableTextBox)
				.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				.Text(FText::FromString(InPDGAssetLink->TOPNodeFilter))
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
		auto ChangeTOPOutputFilter = [InPDGAssetLink](const FString& NewValue)
		{
			if (InPDGAssetLink->TOPOutputFilter.Equals(NewValue))
				return;

			InPDGAssetLink->TOPOutputFilter = NewValue;
			FHoudiniPDGDetails::RefreshPDGAssetLink(InPDGAssetLink);
		};

		FDetailWidgetRow& PDGOutputFilterRow = InPDGCategory.AddCustomRow(FText::GetEmpty());

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

					InPDGAssetLink->bUseTOPOutputFilter = bNewState;
					FHoudiniPDGDetails::RefreshPDGAssetLink(InPDGAssetLink);
				})
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("TOP Output Filter")))
			];

		PDGOutputFilterRow.ValueWidget.Widget = 
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.0f)
			[
				SNew(SEditableTextBox)
				.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				.Text(FText::FromString(InPDGAssetLink->TOPOutputFilter))
				.OnTextCommitted_Lambda([ChangeTOPOutputFilter](const FText& Val, ETextCommit::Type TextCommitType)
				{
					ChangeTOPOutputFilter(Val.ToString());
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
		FDetailWidgetRow& PDGAutocookRow = InPDGCategory.AddCustomRow(FText::GetEmpty());
		PDGAutocookRow.NameWidget.Widget =
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("Auto-cook")))
				.ToolTipText(FText::FromString(
					TEXT("Automatically cooks the select TOP network's output after succesfully cooking the HDA.")))
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

					InPDGAssetLink->bAutoCook = bNewState;
					FHoudiniPDGDetails::RefreshUI(InPDGAssetLink);
				})
			];
	}
	// WORK ITEM STATUS
	{
		FDetailWidgetRow& PDGStatusRow = InPDGCategory.AddCustomRow(FText::GetEmpty());
		FHoudiniPDGDetails::AddWorkItemStatusWidget(
			PDGStatusRow, TEXT("Asset Work Item Status"), InPDGAssetLink->WorkItemTally);
	}
}


void
FHoudiniPDGDetails::AddPDGAssetStatus(
	IDetailCategoryBuilder& InPDGCategory, const EPDGLinkState& InLinkState)
{
	FString PDGStatusString;
	FLinearColor PDGStatusColor = FLinearColor::White;
	switch (InLinkState)
	{
		case EPDGLinkState::Linked:
			PDGStatusString = TEXT("PDG is READY");
			PDGStatusColor = FLinearColor::Green;
			break;
		case EPDGLinkState::Linking:
			PDGStatusString = TEXT("PDG is Linking");
			PDGStatusColor = FLinearColor::Yellow;
			break;
		case EPDGLinkState::Error_Not_Linked:
			PDGStatusString = TEXT("PDG is ERRORED");
			PDGStatusColor = FLinearColor::Red;
			break;
		case EPDGLinkState::Inactive:
			PDGStatusString = TEXT("PDG is INACTIVE");
			PDGStatusColor = FLinearColor::White;
			break;
	}
		
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
			.Text(FText::FromString(PDGStatusString))
			.ColorAndOpacity(PDGStatusColor)
		]
	];
}


void
FHoudiniPDGDetails::AddWorkItemStatusWidget(
	FDetailWidgetRow& InRow, const FString& InTitleString, const FWorkItemTally& InWorkItemTally)
{
	auto AddGridBox = [](const FString& Title, const int32& Value, const FLinearColor& Color) -> SHorizontalBox::FSlot&
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
						.ColorAndOpacity(Color)
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
						.Text(FText::AsNumber(Value))
						.ColorAndOpacity(Color)
					]
				]
			]
		];
	};

	FLinearColor WaitingColor = InWorkItemTally.WaitingWorkItems > 0 ? FLinearColor(0.0f, 1.0f, 1.0f) : FLinearColor::White;
	FLinearColor CookingColor = InWorkItemTally.CookingWorkItems > 0 ? FLinearColor::Yellow : FLinearColor::White;
	FLinearColor CookedColor = InWorkItemTally.CookedWorkItems > 0 ? FLinearColor::Green : FLinearColor::White;
	FLinearColor FailedColor = InWorkItemTally.ErroredWorkItems > 0 ? FLinearColor::Red : FLinearColor::White;
		
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
				.Text(FText::FromString(InTitleString))
				
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			.Padding(FMargin(5.0f, 2.0f))
			[
				SNew(SHorizontalBox)
				+ AddGridBox(TEXT("WAITING"), InWorkItemTally.WaitingWorkItems, WaitingColor)
				+ AddGridBox(TEXT("COOKING"), InWorkItemTally.CookingWorkItems, CookingColor)
				+ AddGridBox(TEXT("COOKED"), InWorkItemTally.CookedWorkItems, CookedColor)
				+ AddGridBox(TEXT("FAILED"), InWorkItemTally.ErroredWorkItems, FailedColor)
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
			TOPNetworksPtr[Idx] = MakeShareable(new FString(InPDGAssetLink->AllTOPNetworks[Idx].NodeName));
		}

		if(TOPNetworksPtr.Num() <= 0)
			TOPNetworksPtr.Add(MakeShareable(new FString("----")));

		// Lambda for selecting another TOPNet
		auto OnTOPNetChanged = [InPDGAssetLink](TSharedPtr<FString> InNewChoice)
		{
			if (!InNewChoice.IsValid())
				return;

			FString NewChoice = *InNewChoice.Get();
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

			InPDGAssetLink->SelectedTOPNetworkIndex = NewSelectedIndex;
						
			FHoudiniPDGDetails::RefreshUI(InPDGAssetLink);
		};
		
		TSharedPtr<SHorizontalBox, ESPMode::NotThreadSafe> HorizontalBoxTOPNet;
		TSharedPtr<SComboBox<TSharedPtr<FString>>> ComboBoxTOPNet;
		int32 SelectedIndex = InPDGAssetLink->SelectedTOPNetworkIndex > 0 ? InPDGAssetLink->SelectedTOPNetworkIndex : 0;

		PDGTOPNetRow.ValueWidget.Widget = 
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(2, 2, 5, 2)
			.FillWidth(300.f)
			.MaxWidth(300.f)
			[
				SAssignNew(ComboBoxTOPNet, SComboBox<TSharedPtr<FString>>)
				.OptionsSource(&TOPNetworksPtr)	
				.InitiallySelectedItem(TOPNetworksPtr[SelectedIndex])
				.OnGenerateWidget_Lambda([](TSharedPtr<FString> ChoiceEntry)
				{
					FText ChoiceEntryText = FText::FromString(*ChoiceEntry);
					return SNew(STextBlock)
					.Text(ChoiceEntryText)
					.ToolTipText(ChoiceEntryText)
					.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")));
				})
				.OnSelectionChanged_Lambda([OnTOPNetChanged](TSharedPtr<FString> NewChoice, ESelectInfo::Type SelectType)
				{
					return OnTOPNetChanged(NewChoice);
				})
				[
					SNew(STextBlock)
					.Text_Lambda([InPDGAssetLink]()
					{
						return FText::FromString(InPDGAssetLink->GetSelectedTOPNetworkName());
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
					.ToolTipText(LOCTEXT("DirtyAllTooltip", "Dirty all TOP nodes in the selected TOP network"))
					.ContentPadding(FMargin(5.0f, 5.0f))
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Center)
					.OnClicked_Lambda([InPDGAssetLink]()
					{
						if (InPDGAssetLink->GetSelectedTOPNetwork())
						{
							FHoudiniPDGManager::DirtyAll(*(InPDGAssetLink->GetSelectedTOPNetwork()));
							FHoudiniPDGDetails::RefreshUI(InPDGAssetLink);
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
					.OnClicked_Lambda([InPDGAssetLink]()
					{
						if (InPDGAssetLink->GetSelectedTOPNetwork())
						{
							//InPDGAssetLink->WorkItemTally.ZeroAll();
							FHoudiniPDGManager::CookOutput(*(InPDGAssetLink->GetSelectedTOPNetwork()));
							FHoudiniPDGDetails::RefreshUI(InPDGAssetLink);
						}
						return FReply::Handled();
					})
				]
			]
		];
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
					.OnClicked_Lambda([InPDGAssetLink]()
					{
						if (InPDGAssetLink->GetSelectedTOPNetwork())
						{
							//InPDGAssetLink->WorkItemTally.ZeroAll();
							FHoudiniPDGManager::PauseCook(*(InPDGAssetLink->GetSelectedTOPNetwork()));
							FHoudiniPDGDetails::RefreshUI(InPDGAssetLink);
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
					.OnClicked_Lambda([InPDGAssetLink]()
					{
						if (InPDGAssetLink->GetSelectedTOPNetwork())
						{
							//InPDGAssetLink->WorkItemTally.ZeroAll();
							FHoudiniPDGManager::CancelCook(*(InPDGAssetLink->GetSelectedTOPNetwork()));
							FHoudiniPDGDetails::RefreshUI(InPDGAssetLink);
						}
						return FReply::Handled();
					})
				]
			]
		];
	}
		
	// TOP NODE WIDGETS
	FHoudiniPDGDetails::AddTOPNodeWidget(TOPNetWorkGrp, InPDGAssetLink);
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

			TOPNodesPtr.Add(
				MakeShareable(new FString(InPDGAssetLink->GetSelectedTOPNetwork()->AllTOPNodes[Idx].NodeName)));
		}

		FString NodeErrorText = FString();
		FString NodeErrorTooltip = FString();
		FLinearColor NodeErrorColor = FLinearColor::White;
		if (InPDGAssetLink->GetSelectedTOPNetwork()->AllTOPNodes.Num() <= 0)
		{
			TOPNodesPtr.Add(MakeShareable(new FString("----")));
			NodeErrorText = TEXT("No valid TOP Node found!");
			NodeErrorTooltip = TEXT("There is no valid TOP Node found in the selected TOP Network!");
			NodeErrorColor = FLinearColor::Red;
		}
		else if(TOPNodesPtr.Num() <= 0)
		{
			TOPNodesPtr.Add(MakeShareable(new FString("----")));
			NodeErrorText = TEXT("No visible TOP Node found!");
			NodeErrorTooltip = TEXT("No visible TOP Node found, all nodes in this network are hidden. Please update your TOP Node Filter.");
			NodeErrorColor = FLinearColor::Yellow;
		}

		// Lambda for selecting a TOPNode
		auto OnTOPNodeChanged = [InPDGAssetLink](TSharedPtr<FString> InNewChoice)
		{
			if (!InNewChoice.IsValid() || !InPDGAssetLink->GetSelectedTOPNetwork())
				return;

			FString NewChoice = *InNewChoice.Get();

			int32 NewSelectedIndex = -1;
			for (int32 Idx = 0; Idx < InPDGAssetLink->GetSelectedTOPNetwork()->AllTOPNodes.Num(); Idx++)
			{
				if (NewChoice.Equals(InPDGAssetLink->GetSelectedTOPNetwork()->AllTOPNodes[Idx].NodeName))
					NewSelectedIndex = Idx;
			}

			if ((NewSelectedIndex < 0) || (InPDGAssetLink->GetSelectedTOPNetwork()->SelectedTOPIndex == NewSelectedIndex))
				return;

			InPDGAssetLink->GetSelectedTOPNetwork()->SelectedTOPIndex = NewSelectedIndex;

			FHoudiniPDGDetails::RefreshUI(InPDGAssetLink);
		};
		
		TSharedPtr<SHorizontalBox, ESPMode::NotThreadSafe> HorizontalBoxTOPNode;
		TSharedPtr<SComboBox<TSharedPtr<FString>>> ComboBoxTOPNode;
		int32 SelectedIndex = 0;
		if (InPDGAssetLink->GetSelectedTOPNetwork()->SelectedTOPIndex >= 0)
		{
			SelectedIndex = InPDGAssetLink->GetSelectedTOPNetwork()->SelectedTOPIndex;

			// When modifiying the nodefilter, it is possible that the selected index is no longer valid!
			if (!TOPNodesPtr.IsValidIndex(SelectedIndex))
				SelectedIndex = 0;
		}

		TSharedPtr<STextBlock> ErrorText;

		PDGTOPNodeRow.ValueWidget.Widget = 
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(2, 2, 5, 2)
			.FillWidth(300.f)
			.MaxWidth(300.f)
			[
				SAssignNew(ComboBoxTOPNode, SComboBox<TSharedPtr<FString>>)
				.OptionsSource(&TOPNodesPtr)
				.InitiallySelectedItem(TOPNodesPtr[SelectedIndex])
				.OnGenerateWidget_Lambda([](TSharedPtr<FString> ChoiceEntry)
				{
					FText ChoiceEntryText = FText::FromString(*ChoiceEntry);
					return SNew(STextBlock)
					.Text(ChoiceEntryText)
					.ToolTipText(ChoiceEntryText)
					.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")));
				})
				.OnSelectionChanged_Lambda([OnTOPNodeChanged](TSharedPtr<FString> NewChoice, ESelectInfo::Type SelectType)
				{
					return OnTOPNodeChanged(NewChoice);
				})
				[
					SNew(STextBlock)
					.Text_Lambda([InPDGAssetLink]()
					{
						return FText::FromString(InPDGAssetLink->GetSelectedTOPNodeName());
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
		PDGNodeStateResultRow.NameWidget.Widget =
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("TOP Node State")))
			];

		FString TOPNodeStatus = FString();
		FLinearColor TOPNodeColor = FLinearColor::White;
		if (InPDGAssetLink->GetSelectedTOPNode())
		{
			TOPNodeStatus = UHoudiniPDGAssetLink::GetTOPNodeStatus(*InPDGAssetLink->GetSelectedTOPNode());
			TOPNodeColor = UHoudiniPDGAssetLink::GetTOPNodeStatusColor(*InPDGAssetLink->GetSelectedTOPNode());
		}

		PDGNodeStateResultRow.ValueWidget.Widget =
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TOPNodeStatus))
				.ColorAndOpacity(TOPNodeColor)
			];
	}
	
	// Checkbox: Auto load results
	{
		FDetailWidgetRow& PDGNodeAutoLoadRow = TOPNodesGrp.AddWidgetRow();
		PDGNodeAutoLoadRow.NameWidget.Widget =
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("Autoload results")))
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
					bool bNewState = (NewState == ECheckBoxState::Checked) ? true : false;
					if (!InPDGAssetLink->GetSelectedTOPNode() || InPDGAssetLink->GetSelectedTOPNode()->bAutoLoad == bNewState)
						return;

					InPDGAssetLink->GetSelectedTOPNode()->bAutoLoad = bNewState;
					FHoudiniPDGDetails::RefreshUI(InPDGAssetLink);
				})
			];

		bool bEnabled = false;
		if (InPDGAssetLink->GetSelectedTOPNode() && !InPDGAssetLink->GetSelectedTOPNode()->bHidden)
			bEnabled = true;

		AutoLoadCheckBox->SetEnabled(bEnabled);
	}
	
	// Checkbox: Show results
	{
		FDetailWidgetRow& PDGNodeShowResultRow = TOPNodesGrp.AddWidgetRow();
		PDGNodeShowResultRow.NameWidget.Widget =
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("Show results")))
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
						? (InPDGAssetLink->GetSelectedTOPNode()->bShow ? ECheckBoxState::Checked : ECheckBoxState::Unchecked) 
						: ECheckBoxState::Unchecked;
				})
				.OnCheckStateChanged_Lambda([InPDGAssetLink](ECheckBoxState NewState)
				{
					bool bNewState = (NewState == ECheckBoxState::Checked) ? true : false;
					if (!InPDGAssetLink->GetSelectedTOPNode() || InPDGAssetLink->GetSelectedTOPNode()->bShow == bNewState)
						return;

					InPDGAssetLink->GetSelectedTOPNode()->bShow = bNewState;
					FHoudiniPDGDetails::RefreshUI(InPDGAssetLink);
				})
			];

		bool bEnabled = false;
		if (InPDGAssetLink->GetSelectedTOPNode() && !InPDGAssetLink->GetSelectedTOPNode()->bHidden)
			bEnabled = true;

		ShowResCheckBox->SetEnabled(bEnabled);
	}

	// Buttons: DIRTY NODE / COOK NODE
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
				.WidthOverride(200.0f)
				[
					SAssignNew(DirtyButton, SButton)
					.Text(LOCTEXT("DirtyNode", "Dirty Node"))
					.ToolTipText(LOCTEXT("DirtyNodeTooltip", "Dirties the selected TOP node."))
					.ContentPadding(FMargin(5.0f, 2.0f))
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Center)
					.OnClicked_Lambda([InPDGAssetLink]()
					{	
						if(InPDGAssetLink->GetSelectedTOPNode())
						{
							FHoudiniPDGManager::DirtyTOPNode(*(InPDGAssetLink->GetSelectedTOPNode()));
							FHoudiniPDGDetails::RefreshUI(InPDGAssetLink);
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
					SAssignNew(CookButton, SButton)
					.Text(LOCTEXT("CookNode", "Cook Node"))
					.ToolTipText(LOCTEXT("CookNodeTooltip", "Cooks the selected TOP Node."))
					.ContentPadding(FMargin(5.0f, 2.0f))
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Center)
					.OnClicked_Lambda([InPDGAssetLink]()
					{
						if (InPDGAssetLink->GetSelectedTOPNode())
						{
							FHoudiniPDGManager::CookTOPNode(*(InPDGAssetLink->GetSelectedTOPNode()));
							FHoudiniPDGDetails::RefreshUI(InPDGAssetLink);
						}
						return FReply::Handled();
					})
				]
			]
		];

		bool bEnabled = false;
		if (InPDGAssetLink->GetSelectedTOPNode() && !InPDGAssetLink->GetSelectedTOPNode()->bHidden)
			bEnabled = true;

		DirtyButton->SetEnabled(bEnabled);
		CookButton->SetEnabled(bEnabled);
	}
	
	// TOP Node WorkItem Status
	{
		if (InPDGAssetLink->GetSelectedTOPNode())
		{
			FDetailWidgetRow& PDGNodeWorkItemStatsRow = TOPNodesGrp.AddWidgetRow();
			FHoudiniPDGDetails::AddWorkItemStatusWidget(
				PDGNodeWorkItemStatsRow, TEXT("TOP Node Work Item Status"), InPDGAssetLink->GetSelectedTOPNode()->WorkItemTally);
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
#undef LOCTEXT_NAMESPACE