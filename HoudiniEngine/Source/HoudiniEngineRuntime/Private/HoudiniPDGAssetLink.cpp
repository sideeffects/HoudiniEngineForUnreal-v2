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

#include "HoudiniPDGAssetLink.h"

#include "HoudiniEngineRuntimePrivatePCH.h"

//
UHoudiniPDGAssetLink::UHoudiniPDGAssetLink(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	//, HoudiniAsset(nullptr)
	//, ParentHAC(nullptr)
	//, AssetName()
	, SelectedTOPNetworkIndex(-1)
	, LinkState(EPDGLinkState::Inactive)
	, bAutoCook(false)
	, bUseTOPNodeFilter(true)
	, bUseTOPOutputFilter(true)
	, NumWorkitems(0)
	, WorkItemTally()
	, OutputCachePath()
	, bNeedsUIRefresh(false)
{
	TOPNodeFilter = HAPI_UNREAL_PDG_DEFAULT_TOP_FILTER;
	TOPOutputFilter = HAPI_UNREAL_PDG_DEFAULT_TOP_OUTPUT_FILTER;

	// TODO:
	// Update init, move default filter to PCH
}


FTOPWorkResultObject::FTOPWorkResultObject()
{
	ResultObjects = nullptr;
	Name = FString();
	FilePath = FString();
	State = EPDGWorkResultState::None;
}


FTOPWorkResult::FTOPWorkResult()
{
	WorkItemIndex = -1;
	WorkItemID = -1;

	ResultObjects.SetNum(0);
}

bool 
FTOPWorkResult::operator==(const FTOPWorkResult& OtherWorkResult) const
{
	if (WorkItemIndex != OtherWorkResult.WorkItemIndex)
		return false;
	if (WorkItemID != OtherWorkResult.WorkItemID)
		return false;
	/*
	if (ResultObjects != OtherWorkResult.ResultObjects)
		return false;
	*/

	return true;
}


FWorkItemTally::FWorkItemTally()
{
	TotalWorkItems = 0;
	WaitingWorkItems = 0;
	ScheduledWorkItems = 0;
	CookingWorkItems = 0;
	CookedWorkItems = 0;
	ErroredWorkItems = 0;
}

void
FWorkItemTally::ZeroAll()
{
	TotalWorkItems = 0;
	WaitingWorkItems = 0;
	ScheduledWorkItems = 0;
	CookingWorkItems = 0;
	CookedWorkItems = 0;
	ErroredWorkItems = 0;
}

bool
FWorkItemTally::AreAllWorkItemsComplete() const
{
	return (
		WaitingWorkItems == 0 && CookingWorkItems == 0 && ScheduledWorkItems == 0 
		&& (TotalWorkItems == (CookedWorkItems + ErroredWorkItems)) );
}

bool
FWorkItemTally::AnyWorkItemsFailed() const
{
	return ErroredWorkItems > 0;
}

bool
FWorkItemTally::AnyWorkItemsPending() const
{
	return (TotalWorkItems > 0 && (WaitingWorkItems > 0 || CookingWorkItems > 0 || ScheduledWorkItems > 0));
}

FString
FWorkItemTally::ProgressRatio() const
{
	float Ratio = TotalWorkItems > 0 ? (CookedWorkItems / TotalWorkItems) * 100.f : 0;

	return FString::Printf(TEXT("%.1f%%"), Ratio);
}


FTOPNode::FTOPNode()
{
	NodeId = -1;
	NodeName = FString();
	ParentName = FString();

	WorkResultParent = nullptr;
	WorkResult.SetNum(0);

	bShow = false;
	bAutoLoad = false;

	NodeState = EPDGNodeState::None;
}

bool
FTOPNode::operator==(const FTOPNode& Other) const
{
	if (!NodeName.Equals(Other.NodeName))
		return false;

	if (!ParentName.Equals(Other.ParentName))
		return false;

	//if (NodeId != Other.NodeId)
	//	return false;

	return true;
}

void 
FTOPNode::Reset()
{
	NodeState = EPDGNodeState::None;
	WorkItemTally.ZeroAll();
}


FTOPNetwork::FTOPNetwork()
{
	NodeId = -1;
	NodeName = FString();

	AllTOPNodes.SetNum(0);
	SelectedTOPIndex = -1;

	ParentName = FString();

	bShowResults = false;
	bAutoLoadResults = false;
}

bool
FTOPNetwork::operator==(const FTOPNetwork& Other) const
{
	if (!NodeName.Equals(Other.NodeName))
		return false;

	if (!ParentName.Equals(Other.ParentName))
		return false;

	//if (NodeId != Other.NodeId)
	//	return false;

	return true;
}


void
UHoudiniPDGAssetLink::SelectTOPNetwork(const int32& AtIndex)
{
	if (!AllTOPNetworks.IsValidIndex(AtIndex))
		return;

	SelectedTOPNetworkIndex = AtIndex;
}


void
UHoudiniPDGAssetLink::SelectTOPNode(FTOPNetwork& TOPNetwork, const int32& AtIndex)
{
	if (!TOPNetwork.AllTOPNodes.IsValidIndex(AtIndex))
		return;

	TOPNetwork.SelectedTOPIndex = AtIndex;
}


FTOPNetwork*
UHoudiniPDGAssetLink::GetSelectedTOPNetwork()
{
	return GetTOPNetwork(SelectedTOPNetworkIndex);
}


FTOPNode*
UHoudiniPDGAssetLink::GetSelectedTOPNode()
{
	FTOPNetwork* SelectedTOPNetwork = GetSelectedTOPNetwork();
	if (!SelectedTOPNetwork)
		return nullptr;

	if (!SelectedTOPNetwork->AllTOPNodes.IsValidIndex(SelectedTOPNetwork->SelectedTOPIndex))
		return nullptr;

	return &(SelectedTOPNetwork->AllTOPNodes[SelectedTOPNetwork->SelectedTOPIndex]);
}

FString
UHoudiniPDGAssetLink::GetSelectedTOPNodeName()
{
	FString NodeName = FString();
	FTOPNetwork* SelectedTOPNetwork = GetSelectedTOPNetwork();
	if (!SelectedTOPNetwork)
		return NodeName;

	if (SelectedTOPNetwork->AllTOPNodes.IsValidIndex(SelectedTOPNetwork->SelectedTOPIndex))
		NodeName = SelectedTOPNetwork->AllTOPNodes[SelectedTOPNetwork->SelectedTOPIndex].NodeName;

	return NodeName;
}

FString
UHoudiniPDGAssetLink::GetSelectedTOPNetworkName()
{
	FString NetworkName = FString();
	if (AllTOPNetworks.IsValidIndex(SelectedTOPNetworkIndex))
		NetworkName = AllTOPNetworks[SelectedTOPNetworkIndex].NodeName;

	return NetworkName;
}

FTOPNetwork* 
UHoudiniPDGAssetLink::GetTOPNetwork(const int32& AtIndex)
{
	if(AllTOPNetworks.IsValidIndex(AtIndex))
	{
		return &(AllTOPNetworks[AtIndex]);
	}

	return nullptr;
}

FTOPNetwork*
UHoudiniPDGAssetLink::GetTOPNetworkByName(const FString& InName, TArray<FTOPNetwork>& InTOPNetworks)
{
	for (FTOPNetwork& CurrentTOPNet : InTOPNetworks)
	{
		if (CurrentTOPNet.NodeName.Equals(InName))
			return &CurrentTOPNet;
	}

	return nullptr;
}

FTOPNode*
UHoudiniPDGAssetLink::GetTOPNodeByName(const FString& InName, TArray<FTOPNode>& InTOPNodes)
{
	for (FTOPNode& CurrentTOPNode : InTOPNodes)
	{
		if (CurrentTOPNode.NodeName.Equals(InName))
			return &CurrentTOPNode;
	}

	return nullptr;
}

void
UHoudiniPDGAssetLink::ClearAllTOPData()
{
	// Clears all TOP data
	for(FTOPNetwork& CurrentNetwork : AllTOPNetworks)
	{
		for(FTOPNode& CurrentTOPNode : CurrentNetwork.AllTOPNodes)
		{
			ClearTOPNodeWorkItemResults(CurrentTOPNode);
		}
	}

	AllTOPNetworks.Empty();
}

void 
UHoudiniPDGAssetLink::ClearTOPNetworkWorkItemResults(FTOPNetwork& TOPNetwork)
{
	for(FTOPNode& CurrentTOPNode : TOPNetwork.AllTOPNodes)
	{
		ClearTOPNodeWorkItemResults(CurrentTOPNode);
	}
}

void
UHoudiniPDGAssetLink::ClearTOPNodeWorkItemResults(FTOPNode& TOPNode)
{
	for(FTOPWorkResult& CurrentWorkResult : TOPNode.WorkResult)
	{
		DestroyWorkItemResultData(CurrentWorkResult, TOPNode);
	}
	TOPNode.WorkResult.Empty();

	if (TOPNode.WorkResultParent && !TOPNode.WorkResultParent->IsPendingKill())
	{
		// TODO: Destroy the Parent Object
		// DestroyImmediate(topNode._workResultParentGO);
	}
}


void
UHoudiniPDGAssetLink::ClearWorkItemResultByID(const int32& InWorkItemID, FTOPNode& TOPNode)
{
	FTOPWorkResult* WorkResult = GetWorkResultByID(InWorkItemID, TOPNode);
	if (WorkResult)
	{
		ClearWorkItemResult(*WorkResult, TOPNode);
	}	
}


void 
UHoudiniPDGAssetLink::ClearWorkItemResult(FTOPWorkResult& InResult, FTOPNode& TOPNode)
{
	DestroyWorkItemResultData(InResult, TOPNode);
	TOPNode.WorkResult.Remove(InResult);
}


FTOPWorkResult*
UHoudiniPDGAssetLink::GetWorkResultByID(const int32& InWorkItemID, FTOPNode& InTOPNode)
{
	for(FTOPWorkResult& CurResult : InTOPNode.WorkResult)
	{
		if (CurResult.WorkItemID == InWorkItemID)
		{
			return &CurResult;
		}
	}

	return nullptr;
}

void
UHoudiniPDGAssetLink::DestroyWorkItemResultData(FTOPWorkResult& Result, FTOPNode& InTOPNode)
{
	if (Result.ResultObjects.Num() <= 0)
		return;

	Result.ResultObjects.Empty();
}


FTOPNode*
UHoudiniPDGAssetLink::GetTOPNode(const int32& InNodeID)
{
	for (FTOPNetwork& CurrentTOPNet : AllTOPNetworks)
	{
		for (FTOPNode& CurrentTOPNode : CurrentTOPNet.AllTOPNodes)
		{
			if (CurrentTOPNode.NodeId == InNodeID)
				return &CurrentTOPNode;
		}
	}

	return nullptr;
}


void
UHoudiniPDGAssetLink::UpdateWorkItemTally()
{
	WorkItemTally.ZeroAll();		
	for(FTOPNetwork& CurrentTOPNet : AllTOPNetworks)
	{
		for(FTOPNode& CurrentTOPNode : CurrentTOPNet.AllTOPNodes)
		{
			WorkItemTally.TotalWorkItems += CurrentTOPNode.WorkItemTally.TotalWorkItems;
			WorkItemTally.WaitingWorkItems += CurrentTOPNode.WorkItemTally.WaitingWorkItems;
			WorkItemTally.ScheduledWorkItems += CurrentTOPNode.WorkItemTally.ScheduledWorkItems;
			WorkItemTally.CookingWorkItems += CurrentTOPNode.WorkItemTally.CookingWorkItems;
			WorkItemTally.CookedWorkItems += CurrentTOPNode.WorkItemTally.CookedWorkItems;
			WorkItemTally.ErroredWorkItems += CurrentTOPNode.WorkItemTally.ErroredWorkItems;
		}
	}
}


void
UHoudiniPDGAssetLink::ResetTOPNetworkWorkItemTally(FTOPNetwork& TOPNetwork)
{
	for (FTOPNode& CurTOPNode : TOPNetwork.AllTOPNodes)
	{
		CurTOPNode.WorkItemTally.ZeroAll();
	}
}


FString 
UHoudiniPDGAssetLink::GetAssetLinkStatus(const EPDGLinkState& InLinkState)
{
	FString Status;
	switch (InLinkState)
	{
	case EPDGLinkState::Inactive:
		Status = TEXT("Inactive");
	case EPDGLinkState::Linking:
		Status = TEXT("Linking");
	case EPDGLinkState::Linked:
		Status = TEXT("Linked");
	case EPDGLinkState::Error_Not_Linked:
		Status = TEXT("Not Linked");
	default:
		Status = TEXT("");
	}

	return Status;
}

FString
UHoudiniPDGAssetLink::GetTOPNodeStatus(const FTOPNode& InTOPNode)
{
	if (InTOPNode.NodeState == EPDGNodeState::Cook_Failed || InTOPNode.AnyWorkItemsFailed())
	{
		return TEXT("Cook Failed");
	}
	else if (InTOPNode.NodeState == EPDGNodeState::Cook_Complete)
	{
		return TEXT("Cook Completed");
	}
	else if (InTOPNode.NodeState == EPDGNodeState::Cooking)
	{
		return TEXT("Cook In Progress");
	}
	else if (InTOPNode.NodeState == EPDGNodeState::Dirtied)
	{
		return TEXT("Dirtied");
	}
	else if (InTOPNode.NodeState == EPDGNodeState::Dirtying)
	{
		return TEXT("Dirtying");
	}

	return TEXT("");
}

FLinearColor
UHoudiniPDGAssetLink::GetTOPNodeStatusColor(const FTOPNode& InTOPNode)
{
	if (InTOPNode.NodeState == EPDGNodeState::Cook_Failed || InTOPNode.AnyWorkItemsFailed())
	{
		return FLinearColor::Red;
	}
	else if (InTOPNode.NodeState == EPDGNodeState::Cook_Complete)
	{
		return FLinearColor::Green;
	}
	else if (InTOPNode.NodeState == EPDGNodeState::Cooking)
	{
		return FLinearColor(0.0, 1.0f, 1.0f);
	}
	else if (InTOPNode.NodeState == EPDGNodeState::Dirtied)
	{
		return FLinearColor(1.0f, 0.5f, 0.0f);
	}
	else if (InTOPNode.NodeState == EPDGNodeState::Dirtying)
	{
		return FLinearColor::Yellow;
	}

	return FLinearColor::White;
}
