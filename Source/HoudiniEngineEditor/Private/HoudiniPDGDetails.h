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

#include "CoreMinimal.h"
#include "Templates/SharedPointer.h"

class UHoudiniPDGAssetLink;

class IDetailGroup;
class IDetailCategoryBuilder;
class FDetailWidgetRow;

struct FWorkItemTally;
enum class EPDGLinkState : uint8;

// Convenience struct to hold a label and tooltip for widgets.
struct FTextAndTooltip
{
public:
	FTextAndTooltip(const FString& InText);
	FTextAndTooltip(const FString& InText, const FString &InTooltip);
	FTextAndTooltip(FString&& InText);
	FTextAndTooltip(FString&& InText, FString&& InTooltip);
	
	FString Text;
	
	FString ToolTip;
};

class FHoudiniPDGDetails : public TSharedFromThis<FHoudiniPDGDetails>
{
	public:

		void CreateWidget(
			IDetailCategoryBuilder & HouPDGCategory,
			UHoudiniPDGAssetLink* InPDGAssetLink);
			//UHoudiniAssetComponent* InHAC);

		void AddPDGAssetWidget(
			IDetailCategoryBuilder& InPDGCategory, UHoudiniPDGAssetLink* InPDGAssetLink);

		void AddWorkItemStatusWidget(
			FDetailWidgetRow& InRow, const FString& TitleString, const FWorkItemTally& InWorkItemTally);

		void AddPDGAssetStatus(
			IDetailCategoryBuilder& InPDGCategory, const EPDGLinkState& InLinkState);

		void AddTOPNetworkWidget(
			IDetailCategoryBuilder& InPDGCategory, UHoudiniPDGAssetLink* InPDGAssetLink);

		void AddTOPNodeWidget(
			IDetailGroup& InGroup, UHoudiniPDGAssetLink* InPDGAssetLink);

		static void RefreshPDGAssetLink(
			UHoudiniPDGAssetLink* InPDGAssetLink);

		static void RefreshUI(
			UHoudiniPDGAssetLink* InPDGAssetLink, const bool& InFullUpdate = true);

	private:

		TArray<TSharedPtr<FTextAndTooltip>> TOPNetworksPtr;

		TArray<TSharedPtr<FTextAndTooltip>> TOPNodesPtr;

};
