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

#include "HoudiniPDGManager.h"

#include "HoudiniApi.h"
#include "HoudiniEngine.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniEngineString.h"
#include "HoudiniEngineRuntime.h"
#include "HoudiniAssetComponent.h"
#include "HoudiniPDGAssetLink.h"
#include "HoudiniPackageParams.h"

#include "HoudiniEnginePrivatePCH.h"
#include "HoudiniMeshTranslator.h"
#include "HoudiniPDGTranslator.h"

#include "HAPI/HAPI_Common.h"

bool
FHoudiniPDGManager::InitializePDGAssetLink(UHoudiniAssetComponent* InHAC)
{
	if (!InHAC || InHAC->IsPendingKill())
		return false;

	int32 AssetId = InHAC->GetAssetId();
	if (AssetId < 0)
		return false;

	if (!FHoudiniEngineUtils::IsHoudiniNodeValid((HAPI_NodeId)AssetId))
		return false;

	// Create a new PDG Asset Link Object
	bool bRegisterPDGAssetLink = false;
	UHoudiniPDGAssetLink* PDGAssetLink = InHAC->GetPDGAssetLink();		
	if (!PDGAssetLink || PDGAssetLink->IsPendingKill())
	{
		PDGAssetLink = NewObject<UHoudiniPDGAssetLink>(InHAC, UHoudiniPDGAssetLink::StaticClass(), NAME_None, RF_Transactional);
		bRegisterPDGAssetLink = true;
	}

	if (!PDGAssetLink || PDGAssetLink->IsPendingKill())
		return false;
	
	PDGAssetLink->AssetID = AssetId;
	
	// Get the HDA's info
	HAPI_NodeInfo AssetInfo;
	FHoudiniApi::NodeInfo_Init(&AssetInfo);
	HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetNodeInfo(
		FHoudiniEngine::Get().GetSession(), (HAPI_NodeId)PDGAssetLink->AssetID, &AssetInfo), false);

	// Get the node path
	FString AssetNodePath;
	FHoudiniEngineString::ToFString(AssetInfo.internalNodePathSH, PDGAssetLink->AssetNodePath);

	// Get the node name
	FString AssetName;
	FHoudiniEngineString::ToFString(AssetInfo.nameSH, PDGAssetLink->AssetName);

	if (!FHoudiniPDGManager::PopulateTOPNetworks(PDGAssetLink))
	{
		// We couldn't find any valid TOPNet/TOPNode, this is not a PDG Asset
		// Make sure the HDA doesn't have a PDGAssetLink
		InHAC->SetPDGAssetLink(nullptr);
		return false;
	}

	// If the PDG asset link comes from a loaded asset, we also need to register it
	if (InHAC->HasBeenLoaded())
	{
		bRegisterPDGAssetLink = true;
	}

	// We have found valid TOPNetworks and TOPNodes,
	// This is a PDG HDA, so add the AssetLink to it
	PDGAssetLink->LinkState = EPDGLinkState::Linked;

	if (PDGAssetLink->SelectedTOPNetworkIndex < 0)
		PDGAssetLink->SelectedTOPNetworkIndex = 0;

	InHAC->SetPDGAssetLink(PDGAssetLink);

	if (bRegisterPDGAssetLink)
	{
		// Register this PDG Asset Link to the PDG Manager
		TWeakObjectPtr<UHoudiniPDGAssetLink> AssetLinkPtr(PDGAssetLink);
		PDGAssetLinks.Add(AssetLinkPtr);
	}

	return true;
}

bool
FHoudiniPDGManager::UpdatePDGAssetLink(UHoudiniPDGAssetLink* PDGAssetLink)
{
	if (!PDGAssetLink || PDGAssetLink->IsPendingKill())
		return false;

	// If the PDG Asset link is inactive, indicate that our HDA must be instantiated
	if (PDGAssetLink->LinkState == EPDGLinkState::Inactive)
	{
		UHoudiniAssetComponent* ParentHAC = Cast<UHoudiniAssetComponent>(PDGAssetLink->GetOuter());
		if(!ParentHAC)
		{
			// No valid parent HAC, error!
			PDGAssetLink->LinkState = EPDGLinkState::Error_Not_Linked;
			HOUDINI_LOG_ERROR(TEXT("No valid Houdini Asset Component parent for PDG Asset Link!"));
		}
		else if (ParentHAC && ParentHAC->GetAssetState() == EHoudiniAssetState::NeedInstantiation)
		{
			PDGAssetLink->LinkState = EPDGLinkState::Linking;
			ParentHAC->AssetState = EHoudiniAssetState::PreInstantiation;
		}
		else
		{
			PDGAssetLink->LinkState = EPDGLinkState::Error_Not_Linked;
			HOUDINI_LOG_ERROR(TEXT("Unable to link the PDG Asset link! Try to rebuild the HDA."));
		}
	}
	
	if (PDGAssetLink->LinkState == EPDGLinkState::Linking)
	{
		return true;
	}

	if (PDGAssetLink->LinkState != EPDGLinkState::Linked)
	{
		UHoudiniAssetComponent* ParentHAC = Cast<UHoudiniAssetComponent>(PDGAssetLink->GetOuter());
		int32 AssetId = ParentHAC->GetAssetId();
		if (AssetId < 0)
			return false;

		if (!FHoudiniEngineUtils::IsHoudiniNodeValid((HAPI_NodeId)AssetId))
			return false;

		PDGAssetLink->AssetID = AssetId;
	}

	if(!PopulateTOPNetworks(PDGAssetLink))
	{
		PDGAssetLink->LinkState = EPDGLinkState::Error_Not_Linked;
		HOUDINI_LOG_ERROR(TEXT("Failed to populte the PDG Asset Link."));
		return false;
	}

	return true;
}


bool
FHoudiniPDGManager::PopulateTOPNetworks(UHoudiniPDGAssetLink* PDGAssetLink)
{
	// Find all TOP networks from linked HDA, as well as the TOP nodes within, and populate internal state.
	if (!PDGAssetLink || PDGAssetLink->IsPendingKill())
		return false;

	// Get all the network nodes within the asset, recursively.
	// We're getting all networks because TOP network SOPs aren't considered being of TOP network type, but SOP type
	int32 NetworkNodeCount = 0;
	HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::ComposeChildNodeList(
		FHoudiniEngine::Get().GetSession(), (HAPI_NodeId)PDGAssetLink->AssetID,
		HAPI_NODETYPE_ANY, HAPI_NODEFLAGS_NETWORK, true, &NetworkNodeCount), false);

	if (NetworkNodeCount <= 0)
		return false;

	TArray<HAPI_NodeId> AllNetworkNodeIDs;
	AllNetworkNodeIDs.SetNum(NetworkNodeCount);
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetComposedChildNodeList(
		FHoudiniEngine::Get().GetSession(), (HAPI_NodeId)PDGAssetLink->AssetID,
		AllNetworkNodeIDs.GetData(), NetworkNodeCount), false);

	// For each Network we found earlier, only add those with TOP child nodes 
	// Therefore guaranteeing that we only add TOP networks
	TArray<FTOPNetwork> AllTOPNetworks;
	for (const HAPI_NodeId& CurrentNodeId : AllNetworkNodeIDs)
	{
		if (CurrentNodeId < 0)
		{
			continue;
		}

		HAPI_NodeInfo CurrentNodeInfo;
		FHoudiniApi::NodeInfo_Init(&CurrentNodeInfo);
		if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetNodeInfo(
			FHoudiniEngine::Get().GetSession(), CurrentNodeId, &CurrentNodeInfo))
		{
			continue;
		}
		
		// Skip non TOP or SOP networks
		if (CurrentNodeInfo.type != HAPI_NodeType::HAPI_NODETYPE_TOP
			&& CurrentNodeInfo.type != HAPI_NodeType::HAPI_NODETYPE_SOP)
		{
			continue;
		}

		// Get the list of all TOP nodes within the current network (ignoring schedulers)		
		int32 TOPNodeCount = 0;
		if (HAPI_RESULT_SUCCESS != FHoudiniApi::ComposeChildNodeList(
			FHoudiniEngine::Get().GetSession(), CurrentNodeId,
			HAPI_NodeType::HAPI_NODETYPE_TOP, HAPI_NodeFlags::HAPI_NODEFLAGS_TOP_NONSCHEDULER, true, &TOPNodeCount))
		{
			continue;
		}

		TArray<HAPI_NodeId> AllTOPNodeIDs;
		AllTOPNodeIDs.SetNum(TOPNodeCount);
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetComposedChildNodeList(
			FHoudiniEngine::Get().GetSession(), CurrentNodeId, AllTOPNodeIDs.GetData(), TOPNodeCount), false);

		// Skip networks without TOP nodes
		if (AllTOPNodeIDs.Num() <= 0)
		{
			continue;
		}

		// TODO:
		// Apply the show and output filter on that node
		bool bShow = true;

		// Get the node path
		FString CurrentNodePath;
		FHoudiniEngineString::ToFString(CurrentNodeInfo.internalNodePathSH, CurrentNodePath);
		FPaths::MakePathRelativeTo(CurrentNodePath, *PDGAssetLink->AssetNodePath);
		
		// Get the node name
		FString CurrentNodeName;
		FHoudiniEngineString::ToFString(CurrentNodeInfo.nameSH, CurrentNodeName);

		FTOPNetwork CurrentTOPNetwork;
		int32 FoundTOPNetIndex = INDEX_NONE;	
		FTOPNetwork* FoundTOPNet = PDGAssetLink->GetTOPNetworkByName(CurrentNodeName, PDGAssetLink->AllTOPNetworks, FoundTOPNetIndex);
		if (FoundTOPNet)
		{
			// Reuse the existing corresponding TOP NET
			CurrentTOPNetwork = (*FoundTOPNet);
			PDGAssetLink->AllTOPNetworks.RemoveAt(FoundTOPNetIndex);
		}

		// Update the TOP NET
		CurrentTOPNetwork.NodeId = CurrentNodeId;
		CurrentTOPNetwork.NodeName = CurrentNodeName;
		CurrentTOPNetwork.NodePath = CurrentNodePath;
		CurrentTOPNetwork.ParentName = PDGAssetLink->AssetName;
		CurrentTOPNetwork.bShowResults = bShow;
		
		// Only add network that have valid TOP Nodes
		if (PopulateTOPNodes(AllTOPNodeIDs, CurrentTOPNetwork, PDGAssetLink))
		{
			if(CurrentTOPNetwork.SelectedTOPIndex < 0)
				CurrentTOPNetwork.SelectedTOPIndex = 0;

			AllTOPNetworks.Add(CurrentTOPNetwork);
		}
	}

	// Clear previous TOP networks, nodes and generated data
	for (FTOPNetwork& CurTopNet : PDGAssetLink->AllTOPNetworks)
	{
		PDGAssetLink->ClearTOPNetworkWorkItemResults(CurTopNet);
	}
	//PDGAssetLink->ClearAllTOPData();
	PDGAssetLink->AllTOPNetworks = AllTOPNetworks;

	return (AllTOPNetworks.Num() > 0);
}


bool
FHoudiniPDGManager::PopulateTOPNodes(
	const TArray<HAPI_NodeId>& InTopNodeIDs, FTOPNetwork& TOPNetwork, UHoudiniPDGAssetLink* InPDGAssetLink)
{
	if (!InPDGAssetLink || InPDGAssetLink->IsPendingKill())
		return false;

	// 
	int32 TOPNodeCount = 0;

	// Holds list of found TOP nodes
	TArray<FTOPNode> AllTOPNodes;
	for(const HAPI_NodeId& CurrentTOPNodeID : InTopNodeIDs)
	{
		HAPI_NodeInfo CurrentNodeInfo;
		FHoudiniApi::NodeInfo_Init(&CurrentNodeInfo);
		if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetNodeInfo(
			FHoudiniEngine::Get().GetSession(), CurrentTOPNodeID, &CurrentNodeInfo))
		{
			continue;
		}

		// Increase the number of valid TOP Node
		// (before applying the node filter)
		TOPNodeCount++;

		// Get the node path
		FString NodePath;
		FHoudiniEngineString::ToFString(CurrentNodeInfo.internalNodePathSH, NodePath);
		FPaths::MakePathRelativeTo(NodePath, *InPDGAssetLink->AssetNodePath);
		FPaths::MakePathRelativeTo(NodePath, *TOPNetwork.NodePath);

		// Get the node name
		FString NodeName;
		FHoudiniEngineString::ToFString(CurrentNodeInfo.nameSH, NodeName);

		// See if we can find an existing version of this TOPNOde
		FTOPNode CurrentTopNode;
		int32 FoundNodeIndex = INDEX_NONE;
		FTOPNode* FoundNode = InPDGAssetLink->GetTOPNodeByName(NodeName, TOPNetwork.AllTOPNodes, FoundNodeIndex);
		if (FoundNode)
		{
			CurrentTopNode = (*FoundNode);
			TOPNetwork.AllTOPNodes.RemoveAt(FoundNodeIndex);
		}

		CurrentTopNode.NodeId = CurrentTOPNodeID;
		CurrentTopNode.NodeName = NodeName;
		CurrentTopNode.NodePath = NodePath;
		CurrentTopNode.ParentName = TOPNetwork.ParentName + TEXT("_") + TOPNetwork.NodeName;

		// Filter display/autoload using name
		CurrentTopNode.bHidden = false;
		if (InPDGAssetLink->bUseTOPNodeFilter && !InPDGAssetLink->TOPNodeFilter.IsEmpty())
		{
			// Only display nodes that matches the filter
			if (!NodeName.StartsWith(InPDGAssetLink->TOPNodeFilter))
				CurrentTopNode.bHidden = true;
		}

		// Automatically load results for nodes that match the filter
		if (InPDGAssetLink->bUseTOPOutputFilter)
		{
			bool bAutoLoad = false;
			if (InPDGAssetLink->TOPOutputFilter.IsEmpty())
				bAutoLoad = true;
			else if (NodeName.StartsWith(InPDGAssetLink->TOPOutputFilter))
				bAutoLoad = true;

			CurrentTopNode.bAutoLoad = bAutoLoad;

			// Show autoloaded results by default
			CurrentTopNode.SetVisibleInLevel(bAutoLoad);
		}

		AllTOPNodes.Add(CurrentTopNode);
	}

	for (auto& CurTOPNode : TOPNetwork.AllTOPNodes)
	{
		InPDGAssetLink->ClearTOPNodeWorkItemResults(CurTOPNode);
	}

	TOPNetwork.AllTOPNodes = AllTOPNodes;

	return (TOPNodeCount > 0);
}


void 
FHoudiniPDGManager::DirtyTOPNode(FTOPNode& TOPNode)
{
	// Dirty the specified TOP node...
	if (HAPI_RESULT_SUCCESS != FHoudiniApi::DirtyPDGNode(
		FHoudiniEngine::Get().GetSession(), TOPNode.NodeId, true))
	{
		HOUDINI_LOG_ERROR(TEXT("PDG Dirty TOP Node - Failed to dirty %s!"), *TOPNode.NodeName);
	}

	// ... and clear its work item results.
	UHoudiniPDGAssetLink::ClearTOPNodeWorkItemResults(TOPNode);
}

// void
// FHoudiniPDGManager::DirtyAllTasksOfTOPNode(FTOPNode& InTOPNode)
// {
// 	// Dirty the specified TOP node...
// 	if (HAPI_RESULT_SUCCESS != FHoudiniApi::DirtyPDGNode(
// 		FHoudiniEngine::Get().GetSession(), InTOPNode.NodeId, true))
// 	{
// 		HOUDINI_LOG_ERROR(TEXT("PDG Dirty TOP Node - Failed to dirty %s!"), *InTOPNode.NodeName);
// 	}
// }

void
FHoudiniPDGManager::CookTOPNode(FTOPNode& TOPNode)
{	
	if (HAPI_RESULT_SUCCESS != FHoudiniApi::CookPDG(
		FHoudiniEngine::Get().GetSession(), TOPNode.NodeId, 0, 0))
	{
		HOUDINI_LOG_ERROR(TEXT("PDG Cook TOP Node - Failed to cook %s!"), *TOPNode.NodeName);
	}
}


void
FHoudiniPDGManager::DirtyAll(FTOPNetwork& InTOPNet)
{
	// Dirty the specified TOP network...
	if (HAPI_RESULT_SUCCESS != FHoudiniApi::DirtyPDGNode(
		FHoudiniEngine::Get().GetSession(), InTOPNet.NodeId, true))
	{
		HOUDINI_LOG_ERROR(TEXT("PDG Dirty All - Failed to dirty all of %s's TOP nodes!"), *InTOPNet.NodeName);
		return;
	}

	// ... and clear its work item results.
	UHoudiniPDGAssetLink::ClearTOPNetworkWorkItemResults(InTOPNet);
}


void
FHoudiniPDGManager::CookOutput(FTOPNetwork& InTOPNet)
{
	// Cook the output TOP node of the currently selected TOP network.
	//WorkItemTally.ZeroAll();
	//UHoudiniPDGAssetLink::ResetTOPNetworkWorkItemTally(InTOPNet);

	if (!FHoudiniEngine::Get().GetSession())
		return;

	// TODO: ???
	// Cancel all cooks. This is required as otherwise the graph gets into an infinite cook state (bug?)
	if(HAPI_RESULT_SUCCESS != FHoudiniApi::CookPDG(
		FHoudiniEngine::Get().GetSession(), InTOPNet.NodeId, 0, 0))
	{
		HOUDINI_LOG_ERROR(TEXT("PDG Cook Output - Failed to cook %s's output!"), *InTOPNet.NodeName);
	}
}


void 
FHoudiniPDGManager::PauseCook(FTOPNetwork& InTOPNet)
{
	// Pause the PDG cook of the currently selected TOP network
	//WorkItemTally.ZeroAll();
	//UHoudiniPDGAssetLink::ResetTOPNetworkWorkItemTally(InTOPNet);

	if (!FHoudiniEngine::Get().GetSession())
		return;

	HAPI_PDG_GraphContextId GraphContextId = -1;
	if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetPDGGraphContextId(
		FHoudiniEngine::Get().GetSession(), InTOPNet.NodeId, &GraphContextId))
	{
		HOUDINI_LOG_ERROR(TEXT("PDG Pause Cook - Failed to get %s's graph context ID!"), *InTOPNet.NodeName);
		return;
	}

	if (HAPI_RESULT_SUCCESS != FHoudiniApi::PausePDGCook(
		FHoudiniEngine::Get().GetSession(), GraphContextId))
	{
		HOUDINI_LOG_ERROR(TEXT("PDG Pause Cook - Failed to pause %s!"), *InTOPNet.NodeName);
		return;
	}
}


void
FHoudiniPDGManager::CancelCook(FTOPNetwork& InTOPNet)
{
	// Cancel the PDG cook of the currently selected TOP network
	//WorkItemTally.ZeroAll();
	//UHoudiniPDGAssetLink::ResetTOPNetworkWorkItemTally(InTOPNet);

	if (!FHoudiniEngine::Get().GetSession())
		return;

	HAPI_PDG_GraphContextId GraphContextId = -1;
	if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetPDGGraphContextId(
		FHoudiniEngine::Get().GetSession(), InTOPNet.NodeId, &GraphContextId))
	{
		HOUDINI_LOG_ERROR(TEXT("PDG Cancel Cook - Failed to get %s's graph context ID!"), *InTOPNet.NodeName);
		return;
	}

	if (HAPI_RESULT_SUCCESS != FHoudiniApi::CancelPDGCook(
		FHoudiniEngine::Get().GetSession(), GraphContextId))
	{
		HOUDINI_LOG_ERROR(TEXT("PDG Cancel Cook - Failed to cancel cook for %s!"), *InTOPNet.NodeName);
		return;
	}
}

void
FHoudiniPDGManager::Update()
{
	// Clean up registered PDG Asset Links
	for(int32 Idx = PDGAssetLinks.Num() - 1; Idx >= 0; Idx--)
	{
		TWeakObjectPtr<UHoudiniPDGAssetLink> Ptr = PDGAssetLinks[Idx];
		if (!Ptr.IsValid() || Ptr.IsStale())
		{
			PDGAssetLinks.RemoveAt(Idx);
			continue;
		}

		UHoudiniPDGAssetLink* CurPDGAssetLink = PDGAssetLinks[Idx].Get();
		if (!CurPDGAssetLink || CurPDGAssetLink->IsPendingKill())
		{
			PDGAssetLinks.RemoveAt(Idx);
			continue;
		}
	}

	// Do nothing if we dont have any valid PDG asset Link
	if (PDGAssetLinks.Num() <= 0)
		return;

	// Update the PDG contexts and handle all pdg events and work item status updates
	UpdatePDGContexts();

	// Prcoess any workitem result if we have any
	ProcessWorkItemResults();
}

// Query all the PDG graph context in the current Houdini Engine session.
// Handle PDG events, work item status updates.
// Forward relevant events to PDGAssetLink objects.
void
FHoudiniPDGManager::UpdatePDGContexts()
{
	// Get current PDG graph contexts
	ReinitializePDGContext();

	// Process next set of events for each graph context
	if (PDGContextIDs.Num() <= 0)
		return;

	// Only initialize event array if not valid, or user resized max size
	if(PDGEventInfos.Num() != MaxNumberOfPDGEvents)
		PDGEventInfos.SetNum(MaxNumberOfPDGEvents);

	// TODO: member?
	//HAPI_PDG_State PDGState;
	for(const HAPI_PDG_GraphContextId& CurrentContextID : PDGContextIDs)
	{
		/*
		// TODO: No need to reset events at each tick
		int32 PDGStateInt;
		if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetPDGState(
			FHoudiniEngine::Get().GetSession(), CurrentContextID, &PDGStateInt))
		{
			HOUDINI_LOG_ERROR(TEXT("Failed to get PDG state"));
			continue;
		}

		PDGState = (HAPI_PDG_State)PDGStateInt;
		
		for (int32 Idx = 0; Idx < PDGEventInfos.Num(); Idx++)
		{
			ResetPDGEventInfo(PDGEventInfos[Idx]);
		}
		*/

		int32 PDGEventCount = 0;
		int32 RemainingPDGEventCount = 0;
		if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetPDGEvents(
			FHoudiniEngine::Get().GetSession(), CurrentContextID, PDGEventInfos.GetData(),
			MaxNumberOfPDGEvents, &PDGEventCount, &RemainingPDGEventCount))
		{
			HOUDINI_LOG_ERROR(TEXT("Failed to get PDG events"));
			continue;
		}

		if (PDGEventCount < 1)
			continue;
		
		for (int32 EventIdx = 0; EventIdx < PDGEventCount; EventIdx++)
		{
			ProcessPDGEvent(CurrentContextID, PDGEventInfos[EventIdx]);
		}

		// Refresh UI if necessary
		for (auto CurAssetLink : PDGAssetLinks)
		{
			UHoudiniPDGAssetLink* AssetLink = CurAssetLink.Get();
			if (AssetLink && AssetLink->bNeedsUIRefresh)
			{
				FHoudiniPDGManager::RefreshPDGAssetLinkUI(AssetLink);
				AssetLink->bNeedsUIRefresh = false;
			}
		}

		HOUDINI_LOG_MESSAGE(TEXT("PDG: Tick processed %d events, %d remaining."), PDGEventCount, RemainingPDGEventCount);
	}	
}

// Query the currently active PDG graph contexts in the Houdini Engine session.
// Should be done each time to get latest set of graph contexts.
void
FHoudiniPDGManager::ReinitializePDGContext()
{
	int32 NumContexts = 0;

	PDGContextNames.SetNum(MaxNumberOPDGContexts);
	PDGContextIDs.SetNum(MaxNumberOPDGContexts);
	
	if(HAPI_RESULT_SUCCESS != FHoudiniApi::GetPDGGraphContexts(
		FHoudiniEngine::Get().GetSession(),
		&NumContexts, PDGContextNames.GetData(), PDGContextIDs.GetData(), MaxNumberOPDGContexts) || NumContexts <= 0)
	{
		PDGContextNames.SetNum(0);
		PDGContextIDs.SetNum(0);
		return;
	}

	if(PDGContextIDs.Num() != NumContexts)
		PDGContextIDs.SetNum(NumContexts);

	if (PDGContextNames.Num() != NumContexts)
		PDGContextNames.SetNum(NumContexts);
}

// Process a PDG event. Notify the relevant PDGAssetLink object.
void
FHoudiniPDGManager::ProcessPDGEvent(const HAPI_PDG_GraphContextId& InContextID, HAPI_PDG_EventInfo& EventInfo)
{
	UHoudiniPDGAssetLink* PDGAssetLink = nullptr;
	FTOPNode* TOPNode = nullptr;

	if(!GetTOPAssetLinkAndNode(EventInfo.nodeId, PDGAssetLink, TOPNode)
		|| PDGAssetLink == nullptr || PDGAssetLink->IsPendingKill() 
		|| TOPNode == nullptr || TOPNode->NodeId != EventInfo.nodeId)
	{
		return;
	}
	
	HAPI_PDG_EventType EventType = (HAPI_PDG_EventType)EventInfo.eventType;
	HAPI_PDG_WorkitemState CurrentWorkItemState = (HAPI_PDG_WorkitemState)EventInfo.currentState;
	HAPI_PDG_WorkitemState LastWorkItemState = (HAPI_PDG_WorkitemState)EventInfo.lastState;

	FLinearColor MsgColor = FLinearColor::White;

	bool bUpdatePDGNodeState = false;
	switch (EventType)
	{	
		case HAPI_PDG_EVENT_NULL:
			SetTOPNodePDGState(PDGAssetLink, *TOPNode, EPDGNodeState::None);
			break;

		case HAPI_PDG_EVENT_NODE_CLEAR:
			NotifyTOPNodePDGStateClear(PDGAssetLink, *TOPNode);
			break;

		case HAPI_PDG_EVENT_WORKITEM_ADD:
			bUpdatePDGNodeState = true;
			NotifyTOPNodeTotalWorkItem(PDGAssetLink, *TOPNode, 1);
			break;

		case HAPI_PDG_EVENT_WORKITEM_REMOVE:
			RemoveWorkItem(PDGAssetLink, EventInfo.workitemId, *TOPNode);
			bUpdatePDGNodeState = true;
			NotifyTOPNodeTotalWorkItem(PDGAssetLink, *TOPNode, -1);
			break;

		case HAPI_PDG_EVENT_COOK_WARNING:
			MsgColor = FLinearColor::Yellow;
			break;

		case HAPI_PDG_EVENT_COOK_ERROR:
			MsgColor = FLinearColor::Red;
			break;

		case HAPI_PDG_EVENT_COOK_COMPLETE:
			SetTOPNodePDGState(PDGAssetLink, *TOPNode, EPDGNodeState::Cook_Complete);
			break;

		case HAPI_PDG_EVENT_DIRTY_START:
			SetTOPNodePDGState(PDGAssetLink, *TOPNode, EPDGNodeState::Dirtying);
			break;

		case HAPI_PDG_EVENT_DIRTY_STOP:
			SetTOPNodePDGState(PDGAssetLink, *TOPNode, EPDGNodeState::Dirtied);
			break;

		case HAPI_PDG_EVENT_WORKITEM_STATE_CHANGE:
		{
			// Last states
			bUpdatePDGNodeState = true;
			if (LastWorkItemState == HAPI_PDG_WorkitemState::HAPI_PDG_WORKITEM_WAITING && CurrentWorkItemState != HAPI_PDG_WorkitemState::HAPI_PDG_WORKITEM_WAITING)
			{
				NotifyTOPNodeWaitingWorkItem(PDGAssetLink, *TOPNode, -1);
			}
			else if (LastWorkItemState == HAPI_PDG_WorkitemState::HAPI_PDG_WORKITEM_COOKING && CurrentWorkItemState != HAPI_PDG_WorkitemState::HAPI_PDG_WORKITEM_COOKING)
			{
				NotifyTOPNodeCookingWorkItem(PDGAssetLink, *TOPNode, -1);
			}
			else if (LastWorkItemState == HAPI_PDG_WorkitemState::HAPI_PDG_WORKITEM_SCHEDULED && CurrentWorkItemState != HAPI_PDG_WorkitemState::HAPI_PDG_WORKITEM_SCHEDULED)
			{
				NotifyTOPNodeScheduledWorkItem(PDGAssetLink, *TOPNode, -1);
			}
			else if ( (LastWorkItemState == HAPI_PDG_WorkitemState::HAPI_PDG_WORKITEM_COOKED_CACHE || LastWorkItemState == HAPI_PDG_WorkitemState::HAPI_PDG_WORKITEM_COOKED_SUCCESS)
					&& CurrentWorkItemState != HAPI_PDG_WorkitemState::HAPI_PDG_WORKITEM_COOKED_CACHE &&  CurrentWorkItemState != HAPI_PDG_WorkitemState::HAPI_PDG_WORKITEM_COOKED_SUCCESS)
			{
				// Handled previously cooked WI
				NotifyTOPNodeCookedWorkItem(PDGAssetLink, *TOPNode, -1);
			}
			else if (LastWorkItemState == HAPI_PDG_WorkitemState::HAPI_PDG_WORKITEM_COOKED_FAIL && CurrentWorkItemState != HAPI_PDG_WorkitemState::HAPI_PDG_WORKITEM_COOKED_FAIL)
			{
				NotifyTOPNodeScheduledWorkItem(PDGAssetLink, *TOPNode, -1);
			}
			else
			{
				// TODO: 
				// unhandled state change
				NotifyTOPNodeCookedWorkItem(PDGAssetLink, *TOPNode, 0);
			}

			if (LastWorkItemState == CurrentWorkItemState)
			{
				// TODO: 
				// Not a change!! shouldnt happen!
				NotifyTOPNodeCookedWorkItem(PDGAssetLink, *TOPNode, 0);
			}

			// New states
			if (CurrentWorkItemState == HAPI_PDG_WorkitemState::HAPI_PDG_WORKITEM_WAITING)
			{
				NotifyTOPNodeWaitingWorkItem(PDGAssetLink, *TOPNode, 1);
			}
			else if (CurrentWorkItemState == HAPI_PDG_WorkitemState::HAPI_PDG_WORKITEM_UNCOOKED)
			{

			}
			else if (CurrentWorkItemState == HAPI_PDG_WorkitemState::HAPI_PDG_WORKITEM_DIRTY)
			{
				// ClearWorkItemResult(InContextID, EventInfo, *TOPNode);
				ClearWorkItemResult(PDGAssetLink, EventInfo.workitemId, *TOPNode);
				// RemoveWorkItem(PDGAssetLink, EventInfo.workitemId, *TOPNode);
			}
			else if (CurrentWorkItemState == HAPI_PDG_WorkitemState::HAPI_PDG_WORKITEM_SCHEDULED)
			{
				NotifyTOPNodeScheduledWorkItem(PDGAssetLink, *TOPNode, 1);
			}
			else if (CurrentWorkItemState == HAPI_PDG_WorkitemState::HAPI_PDG_WORKITEM_COOKING)
			{
				NotifyTOPNodeCookingWorkItem(PDGAssetLink, *TOPNode, 1);
			}
			else if (CurrentWorkItemState == HAPI_PDG_WorkitemState::HAPI_PDG_WORKITEM_COOKED_SUCCESS 
				|| CurrentWorkItemState == HAPI_PDG_WorkitemState::HAPI_PDG_WORKITEM_COOKED_CACHE)
			{
				NotifyTOPNodeCookedWorkItem(PDGAssetLink, *TOPNode, 1);

				// On cook success, handle results
				CreateWorkItemResult(*TOPNode, InContextID, EventInfo.workitemId, TOPNode->bAutoLoad);
			}
			else if (CurrentWorkItemState == HAPI_PDG_WorkitemState::HAPI_PDG_WORKITEM_COOKED_FAIL)
			{
				// TODO: on cook failure, get log path?
				NotifyTOPNodeErrorWorkItem(PDGAssetLink, *TOPNode, 1);
				MsgColor = FLinearColor::Red;
			}
			else if (CurrentWorkItemState == HAPI_PDG_WorkitemState::HAPI_PDG_WORKITEM_COOKED_CANCEL)
			{
				// Ignore it because in-progress cooks can be cancelled when automatically recooking graph
			}
		}
		break;

		// Unhandled events
		case HAPI_PDG_EVENT_DIRTY_ALL:
		case HAPI_PDG_EVENT_COOK_START:
		case HAPI_PDG_EVENT_WORKITEM_ADD_DEP:
		case HAPI_PDG_EVENT_WORKITEM_REMOVE_DEP:
		case HAPI_PDG_EVENT_WORKITEM_ADD_PARENT:
		case HAPI_PDG_EVENT_WORKITEM_REMOVE_PARENT:
		case HAPI_PDG_EVENT_UI_SELECT:
		case HAPI_PDG_EVENT_NODE_CREATE:
		case HAPI_PDG_EVENT_NODE_REMOVE:
		case HAPI_PDG_EVENT_NODE_RENAME:
		case HAPI_PDG_EVENT_NODE_CONNECT:
		case HAPI_PDG_EVENT_NODE_DISCONNECT:
		case HAPI_PDG_EVENT_WORKITEM_SET_INT:
		case HAPI_PDG_EVENT_WORKITEM_SET_FLOAT:
		case HAPI_PDG_EVENT_WORKITEM_SET_STRING:
		case HAPI_PDG_EVENT_WORKITEM_SET_FILE:
		case HAPI_PDG_EVENT_WORKITEM_SET_PYOBJECT:
		case HAPI_PDG_EVENT_WORKITEM_SET_GEOMETRY:
		case HAPI_PDG_EVENT_WORKITEM_RESULT:
		case HAPI_PDG_EVENT_WORKITEM_PRIORITY:
		case HAPI_PDG_EVENT_WORKITEM_ADD_STATIC_ANCESTOR:
		case HAPI_PDG_EVENT_WORKITEM_REMOVE_STATIC_ANCESTOR:
		case HAPI_PDG_EVENT_NODE_PROGRESS_UPDATE:
		case HAPI_PDG_EVENT_ALL:
		case HAPI_PDG_EVENT_LOG:
		case HAPI_PDG_CONTEXT_EVENTS:
			break;
	}

	if (bUpdatePDGNodeState)
	{
		// Work item events
		EPDGNodeState CurrentTOPNodeState = TOPNode->NodeState;
		if (CurrentTOPNodeState == EPDGNodeState::Cooking)
		{
			if (TOPNode->AreAllWorkItemsComplete())
			{
				if (TOPNode->AnyWorkItemsFailed())
				{
					SetTOPNodePDGState(PDGAssetLink, *TOPNode, EPDGNodeState::Cook_Failed);
				}
				else
				{
					SetTOPNodePDGState(PDGAssetLink, *TOPNode, EPDGNodeState::Cook_Complete);
				}
			}
		}
		else if (TOPNode->AnyWorkItemsPending())
		{
			SetTOPNodePDGState(PDGAssetLink, *TOPNode, EPDGNodeState::Cooking);
		}
	}

	if (EventInfo.msgSH >= 0)
	{
		FString EventMsg;
		FHoudiniEngineString::ToFString(EventInfo.msgSH, EventMsg);
		if (!EventMsg.IsEmpty())
		{
			// TODO: Event MSG?
			// Somehow update the PDG event msg UI ??
			// Simply log for now...
			if (MsgColor == FLinearColor::Red)
			{
				HOUDINI_LOG_ERROR(TEXT("%s"), *EventMsg);
			}
			else if (MsgColor == FLinearColor::Yellow)
			{
				HOUDINI_LOG_WARNING(TEXT("%s"), *EventMsg);
			}
			else
			{
				HOUDINI_LOG_MESSAGE(TEXT("%s"), *EventMsg);
			}
		}
	}
}

void
FHoudiniPDGManager::ResetPDGEventInfo(HAPI_PDG_EventInfo& InEventInfo)
{
	InEventInfo.nodeId = -1;
	InEventInfo.workitemId = -1;
	InEventInfo.dependencyId = -1;
	InEventInfo.currentState = HAPI_PDG_WorkitemState::HAPI_PDG_WORKITEM_UNDEFINED;
	InEventInfo.lastState = HAPI_PDG_WorkitemState::HAPI_PDG_WORKITEM_UNDEFINED;
	InEventInfo.eventType = HAPI_PDG_EventType::HAPI_PDG_EVENT_NULL;
}


bool
FHoudiniPDGManager::GetTOPAssetLinkAndNode(
	const HAPI_NodeId& InNodeID, UHoudiniPDGAssetLink*& OutAssetLink, FTOPNode*& OutTOPNode)
{	
	// Returns the PDGAssetLink and FTOPNode data associated with this TOP node ID
	OutAssetLink = nullptr;
	OutTOPNode = nullptr;
	for (TWeakObjectPtr<UHoudiniPDGAssetLink>& CurAssetLinkPtr : PDGAssetLinks)
	{
		if (!CurAssetLinkPtr.IsValid() || CurAssetLinkPtr.IsStale())
			continue;

		UHoudiniPDGAssetLink* CurAssetLink = CurAssetLinkPtr.Get();
		if (!CurAssetLink || CurAssetLink->IsPendingKill())
			continue;

		OutTOPNode = CurAssetLink->GetTOPNode((int32)InNodeID);
		
		if (OutTOPNode != nullptr)
		{
			OutAssetLink = CurAssetLink;
			return true;
		}
	}

	return false;
}

void
FHoudiniPDGManager::SetTOPNodePDGState(UHoudiniPDGAssetLink* InPDGAssetLink, FTOPNode& TOPNode, const EPDGNodeState& InPDGState)
{
	TOPNode.NodeState = InPDGState;

	InPDGAssetLink->bNeedsUIRefresh = true;
	//FHoudiniPDGManager::RefreshPDGAssetLinkUI(InPDGAssetLink);
}

void
FHoudiniPDGManager::NotifyTOPNodePDGStateClear(UHoudiniPDGAssetLink* InPDGAssetLink, FTOPNode& TOPNode)
{
	//Debug.LogFormat("NotifyTOPNodePDGStateClear:: {0}", topNode._nodeName);
	TOPNode.NodeState = EPDGNodeState::None;
	TOPNode.WorkItemTally.ZeroAll();

	InPDGAssetLink->bNeedsUIRefresh = true;
	//FHoudiniPDGManager::RefreshPDGAssetLinkUI(InPDGAssetLink);
	
}

void
FHoudiniPDGManager::NotifyTOPNodeTotalWorkItem(UHoudiniPDGAssetLink* InPDGAssetLink, FTOPNode& TOPNode, const int32& Increment)
{
	TOPNode.WorkItemTally.TotalWorkItems = FMath::Max(TOPNode.WorkItemTally.TotalWorkItems + Increment, 0);
	
	InPDGAssetLink->bNeedsUIRefresh = true;
	//FHoudiniPDGManager::RefreshPDGAssetLinkUI(InPDGAssetLink);
}

void
FHoudiniPDGManager::NotifyTOPNodeCookedWorkItem(UHoudiniPDGAssetLink* InPDGAssetLink, FTOPNode& TOPNode, const int32& Increment)
{
	TOPNode.WorkItemTally.CookedWorkItems = FMath::Max(TOPNode.WorkItemTally.CookedWorkItems + Increment, 0);
	InPDGAssetLink->bNeedsUIRefresh = true;
	//FHoudiniPDGManager::RefreshPDGAssetLinkUI(InPDGAssetLink);
}

void
FHoudiniPDGManager::NotifyTOPNodeErrorWorkItem(UHoudiniPDGAssetLink* InPDGAssetLink, FTOPNode& TOPNode, const int32& Increment)
{
	TOPNode.WorkItemTally.ErroredWorkItems = FMath::Max(TOPNode.WorkItemTally.ErroredWorkItems + Increment, 0);
	
	InPDGAssetLink->bNeedsUIRefresh = true;
	//FHoudiniPDGManager::RefreshPDGAssetLinkUI(InPDGAssetLink);
}

void
FHoudiniPDGManager::NotifyTOPNodeWaitingWorkItem(UHoudiniPDGAssetLink* InPDGAssetLink, FTOPNode& TOPNode, const int32& Increment)
{
	TOPNode.WorkItemTally.WaitingWorkItems = FMath::Max(TOPNode.WorkItemTally.WaitingWorkItems + Increment, 0);

	InPDGAssetLink->bNeedsUIRefresh = true;
	//FHoudiniPDGManager::RefreshPDGAssetLinkUI(InPDGAssetLink);
}

void
FHoudiniPDGManager::NotifyTOPNodeScheduledWorkItem(UHoudiniPDGAssetLink* InPDGAssetLink, FTOPNode& TOPNode, const int32& Increment)
{
	TOPNode.WorkItemTally.ScheduledWorkItems = FMath::Max(TOPNode.WorkItemTally.ScheduledWorkItems + Increment, 0);

	InPDGAssetLink->bNeedsUIRefresh = true;
	//FHoudiniPDGManager::RefreshPDGAssetLinkUI(InPDGAssetLink);
}

void
FHoudiniPDGManager::NotifyTOPNodeCookingWorkItem(UHoudiniPDGAssetLink* InPDGAssetLink, FTOPNode& TOPNode, const int32& Increment)
{
	TOPNode.WorkItemTally.CookingWorkItems = FMath::Max(TOPNode.WorkItemTally.CookingWorkItems + Increment, 0);

	InPDGAssetLink->bNeedsUIRefresh = true;
	//FHoudiniPDGManager::RefreshPDGAssetLinkUI(InPDGAssetLink);
}

void
FHoudiniPDGManager::ClearWorkItemResult(UHoudiniPDGAssetLink* InAssetLink, const HAPI_PDG_WorkitemId& InWorkItemID, FTOPNode& TOPNode)
{
	if (!InAssetLink || InAssetLink->IsPendingKill())
		return;

	// TODO!!!
	// Clear all work items' results for the specified TOP node. 
	// This destroys any loaded results (geometry etc).
	//session.LogErrorOverride = false;
	InAssetLink->ClearWorkItemResultByID(InWorkItemID, TOPNode);
	// session.LogErrorOverride = true;
}

void
FHoudiniPDGManager::RemoveWorkItem(UHoudiniPDGAssetLink* InAssetLink, const HAPI_PDG_WorkitemId& InWorkItemID, FTOPNode& TOPNode)
{
	if (!InAssetLink || InAssetLink->IsPendingKill())
		return;

	// Clear all of the work item's results for the specified TOP node and also remove the work item itself from
	// the TOP node.
	InAssetLink->DestroyWorkItemByID(InWorkItemID, TOPNode);
}

void
FHoudiniPDGManager::RefreshPDGAssetLinkUI(UHoudiniPDGAssetLink* InAssetLink)
{
	if (!InAssetLink || InAssetLink->IsPendingKill())
		return;

	// Only update the editor properties if the PDG asset link's Actor is selected
	// else, just update the workitemtally
	InAssetLink->UpdateWorkItemTally();

	UHoudiniAssetComponent* HAC = Cast<UHoudiniAssetComponent>(InAssetLink->GetOuter());
	if (!HAC || HAC->IsPendingKill())
		return;
	
	AActor* ActorOwner = HAC->GetOwner();
	if (ActorOwner != nullptr && ActorOwner->IsSelected())
	{
		FHoudiniEngineUtils::UpdateEditorProperties(HAC, true);
	}
}

void
FHoudiniPDGManager::NotifyAssetCooked(UHoudiniPDGAssetLink* InAssetLink, const bool& bSuccess)
{
	if (!InAssetLink || InAssetLink->IsPendingKill())
		return;

	if (bSuccess)
	{
		if (InAssetLink->LinkState == EPDGLinkState::Linked)
		{
			if (InAssetLink->bAutoCook)
			{
				FHoudiniPDGManager::CookOutput(*(InAssetLink->GetSelectedTOPNetwork()));
			}
		}
		else
		{
			UpdatePDGAssetLink(InAssetLink);
		}
	}
	else
	{
		InAssetLink->LinkState = EPDGLinkState::Error_Not_Linked;
	}
}

bool
FHoudiniPDGManager::CreateWorkItemResult(
	FTOPNode& InTOPNode,
	const HAPI_PDG_GraphContextId& InContextID,
	HAPI_PDG_WorkitemId InWorkItemID,
	bool bInLoadResultObjects)
{
	HAPI_PDG_WorkitemInfo WorkItemInfo;
	if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetWorkitemInfo(
		FHoudiniEngine::Get().GetSession(), InContextID, InWorkItemID, &WorkItemInfo))
	{
		HOUDINI_LOG_ERROR(TEXT("Failed to get work item %d info for %s"), InWorkItemID, *(InTOPNode.NodeName));
		// TODO? continue?
		return false;
	}

	if (WorkItemInfo.numResults > 0)
	{
		TArray<HAPI_PDG_WorkitemResultInfo> ResultInfos;
		ResultInfos.SetNum(WorkItemInfo.numResults);
		const int32 resultCount = WorkItemInfo.numResults;
		if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetWorkitemResultInfo(
            FHoudiniEngine::Get().GetSession(),
            InTOPNode.NodeId, InWorkItemID, ResultInfos.GetData(), resultCount))
		{
			HOUDINI_LOG_ERROR(TEXT("Failed to get work item %s result info for {0}"), InWorkItemID, *(InTOPNode.NodeName));
			// TODO? continue?
			return false;
		}

		FTOPWorkResult* WorkResult = UHoudiniPDGAssetLink::GetWorkResultByID(InWorkItemID, InTOPNode);
		if (!WorkResult)
		{
			FTOPWorkResult LocalWorkResult;
			LocalWorkResult.WorkItemID = InWorkItemID;
			LocalWorkResult.WorkItemIndex = WorkItemInfo.index;
			const int32 Idx = InTOPNode.WorkResult.Add(LocalWorkResult);
			WorkResult = &(InTOPNode.WorkResult[Idx]);
		}

		FString WorkItemName;
		FHoudiniEngineString::ToFString(WorkItemInfo.nameSH, WorkItemName);

		// Load each result geometry
		const int32 NumResults = ResultInfos.Num();
		for (int32 Idx = 0; Idx < NumResults; Idx++)
		{
			const HAPI_PDG_WorkitemResultInfo& ResultInfo = ResultInfos[Idx];
			if (ResultInfo.resultTagSH <= 0 || ResultInfo.resultSH <= 0)
				continue;

			FString CurrentTag;
			FHoudiniEngineString::ToFString(ResultInfo.resultTagSH, CurrentTag);
			if(CurrentTag.IsEmpty() || !CurrentTag.StartsWith(TEXT("file")))
				continue;

			FString CurrentPath = FString();
			FHoudiniEngineString::ToFString(ResultInfo.resultSH, CurrentPath);

			// Build a new result object for this result
			FTOPWorkResultObject ResultObj;
			ResultObj.Name = FString::Printf(
                TEXT("%s_%s_%d"),
                *InTOPNode.ParentName,
                *WorkItemName,
                WorkItemInfo.index);
			ResultObj.FilePath = CurrentPath;
			ResultObj.State = bInLoadResultObjects ? EPDGWorkResultState::ToLoad : EPDGWorkResultState::NotLoaded; 

			WorkResult->ResultObjects.Add(ResultObj);
		}
	}

	return true;
}

void
FHoudiniPDGManager::ProcessWorkItemResults()
{
	for (auto& CurrentPDGAssetLink : PDGAssetLinks)
	{
		// Iterate through all PDG Asset Link
		UHoudiniPDGAssetLink* AssetLink = CurrentPDGAssetLink.Get();
		if (!AssetLink)
			continue;

		// Set up package parameters to:
		// Cook to temp houdini engine directory
		// and if the PDG asset link is associated with a Houdini Asset Component (HAC):
		//		set the outer package to the HAC
		//		set the HoudiniAssetName according to the HAC
		//		set the ComponentGUID according to the HAC
		// otherwise we set the outer to the asset link's parent and leave naming and GUID blank
		FHoudiniPackageParams PackageParams;
		PackageParams.PackageMode = FHoudiniPackageParams::GetDefaultStaticMeshesCookMode();
		PackageParams.ReplaceMode = FHoudiniPackageParams::GetDefaultReplaceMode();

		PackageParams.BakeFolder = FHoudiniEngineRuntime::Get().GetDefaultBakeFolder();
		PackageParams.TempCookFolder = FHoudiniEngineRuntime::Get().GetDefaultTemporaryCookFolder();

		// AActor* ParentActor = nullptr;
		UObject* AssetLinkParent = AssetLink->GetOuter();
		UHoudiniAssetComponent* HAC = AssetLinkParent != nullptr ? Cast<UHoudiniAssetComponent>(AssetLinkParent) : nullptr;
		if (HAC)
		{
			PackageParams.OuterPackage = HAC->GetComponentLevel();
			PackageParams.HoudiniAssetName = HAC->GetHoudiniAsset() ? HAC->GetHoudiniAsset()->GetName() : FString();
			PackageParams.HoudiniAssetActorName = HAC->GetOwner()->GetName();
			PackageParams.ComponentGUID = HAC->GetComponentGUID();

			// ParentActor = HAC->GetOwner();
		}
		else
		{
			PackageParams.OuterPackage = AssetLinkParent->GetOutermost();
			PackageParams.HoudiniAssetName = FString();
			PackageParams.HoudiniAssetActorName = FString();
			// PackageParams.ComponentGUID = HAC->GetComponentGUID();

			// // Try to find a parent actor
			// UObject* Parent = AssetLinkParent;
			// while (Parent && !ParentActor)
			// {
			// 	ParentActor = Cast<AActor>(Parent);
			// 	if (!ParentActor)
			// 		Parent = ParentActor->GetOuter();
			// }
		}
		PackageParams.ObjectName = FString();

		// UWorld *World = ParentActor ? ParentActor->GetWorld() : AssetLink->GetWorld();
		UWorld *World = AssetLink->GetWorld();

		// .. All TOP Nets
		for (FTOPNetwork& CurrentTOPNet : AssetLink->AllTOPNetworks)
		{
			// .. All TOP Nodes
			for (FTOPNode& CurrentTOPNode : CurrentTOPNet.AllTOPNodes)
			{
				// ... All WorkResult
				for (FTOPWorkResult& CurrentWorkResult : CurrentTOPNode.WorkResult) 
				{
					// ... All WorkResultObjects
					for (FTOPWorkResultObject& CurrentWorkResultObj : CurrentWorkResult.ResultObjects)
					{
						if (CurrentWorkResultObj.State == EPDGWorkResultState::ToLoad)
						{
							CurrentWorkResultObj.State = EPDGWorkResultState::Loading;

							// Load this WRObj
							PackageParams.PDGTOPNetworkName = CurrentTOPNet.NodeName;
							PackageParams.PDGTOPNodeName = CurrentTOPNode.NodeName;
							PackageParams.PDGWorkItemIndex = CurrentWorkResult.WorkItemIndex;
							if (FHoudiniPDGTranslator::CreateAllResultObjectsForPDGWorkItem(
								AssetLink,
								CurrentTOPNode,
								CurrentWorkResultObj,
								PackageParams))
							{
								CurrentWorkResultObj.State = EPDGWorkResultState::Loaded;
							}
							else
							{
								CurrentWorkResultObj.State = EPDGWorkResultState::None;
							}
						}
						else if (CurrentWorkResultObj.State == EPDGWorkResultState::ToDelete)
						{
							CurrentWorkResultObj.State = EPDGWorkResultState::Deleting;

							// Delete and clean up that WRObj
							CurrentWorkResultObj.DestroyResultOutputs();
							CurrentWorkResultObj.DestroyOutputActor();
							CurrentWorkResultObj.State = EPDGWorkResultState::Deleted;
						}
					}
				}
			}
		}
	}
}
