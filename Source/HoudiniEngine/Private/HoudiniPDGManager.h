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

#include "HAPI/HAPI_Common.h"

class UHoudiniAssetComponent;
class UHoudiniPDGAssetLink;
struct FTOPNetwork;
struct FTOPNode;
enum class EPDGNodeState : uint8;

struct HOUDINIENGINE_API FHoudiniPDGManager
{

public:

	// Initialize the PDG Asset Link for a HoudiniAssetComponent
	// returns true if the HAC uses a PDG asset, and a PDGAssetLink was successfully created
	bool InitializePDGAssetLink(UHoudiniAssetComponent* InHAC);

	// Updates an existing PDG AssetLink
	static bool UpdatePDGAssetLink(UHoudiniPDGAssetLink* PDGAssetLink);

	// Find all TOP networks from linked HDA, as well as the TOP nodes within, and populate internal state.
	static bool PopulateTOPNetworks(UHoudiniPDGAssetLink* PDGAssetLink);

	static void RefreshPDGAssetLinkUI(UHoudiniPDGAssetLink* InAssetLink);

	// Given TOP nodes from a TOP network, populate internal state from each TOP node.
	static bool PopulateTOPNodes(
		const TArray<HAPI_NodeId>& InTopNodeIDs,
		FTOPNetwork& InTOPNetwork,
		UHoudiniPDGAssetLink* InPDGAssetLink);

	// Cook the specified TOP node.
	static void CookTOPNode(FTOPNode& TOPNode);

	// Dirty the specified TOP node and clear its work item results.
	static void DirtyTOPNode(FTOPNode& TOPNode);

	// // Dirty all the tasks/work items of the specified TOP node. Does not
	// // clear its work item results.
	// static void DirtyAllTasksOfTOPNode(FTOPNode& InTOPNode);

	// Dirty the TOP network and clear all work item results.
	static void DirtyAll(FTOPNetwork& InTOPNet);

	// Cook the output TOP node of the currently selected TOP network.
	static void CookOutput(FTOPNetwork& InTOPNet);

	// Pause the PDG cook of the currently selected TOP network
	static void PauseCook(FTOPNetwork& InTOPNet);

	// Cancel the PDG cook of the currently selected TOP network
	static void CancelCook(FTOPNetwork& InTOPNet);

	static void NotifyAssetCooked(UHoudiniPDGAssetLink* InAssetLink, const bool& bSuccess);

	// Update all registered PDG Asset links
	void Update();

	void ReinitializePDGContext();
	
	// Clear all of the specified work item's results from the specified TOP node. This destroys any loaded results
	// (geometry etc), but keeps the work item struct.
	//void ClearWorkItemResult(const HAPI_PDG_GraphContextId& InContextID, const HAPI_PDG_EventInfo& InEventInfo, FTOPNode& TOPNode);
	void ClearWorkItemResult(UHoudiniPDGAssetLink* InAssetLink, const HAPI_PDG_WorkitemId& InWorkItemID, FTOPNode& TOPNode);

	// Clear the specified work item's results from the specified TOP node and remove the work item struct from the TOP
	// node. This destroys any loaded results (geometry etc), and the work item struct.
	void RemoveWorkItem(UHoudiniPDGAssetLink* InAssetLink, const HAPI_PDG_WorkitemId& InWorkItemID, FTOPNode& TOPNode);

	// Create FTOPWorkResult for a given TOP node, and optionally (via bInLoadResultObjects) create its FTOPWorkResultObjects.
	// Geometry is not directly loaded by this function, the FTOPWorkResultObjects' states will be set to ToLoad and
	// the ProcessWorkItemResults function will take care of loading the geo.
	// Results must be tagged with 'file', and must have a file path, otherwise will not included.
	bool CreateWorkItemResult(FTOPNode& InTOPNode, const HAPI_PDG_GraphContextId& InContextID, HAPI_PDG_WorkitemId InWorkItemID, bool bInLoadResultObjects=false);

private:
	
	void UpdatePDGContexts();

	void ProcessWorkItemResults();

	void ProcessPDGEvent(const HAPI_PDG_GraphContextId& InContextID, HAPI_PDG_EventInfo& EventInfo);

	static void ResetPDGEventInfo(HAPI_PDG_EventInfo& InEventInfo);

	// Returns the PDGAssetLink and FTOPNode associated with this TOP node ID
	bool GetTOPAssetLinkAndNode(const HAPI_NodeId& InNodeID, UHoudiniPDGAssetLink*& OutAssetLink, FTOPNode*& OutTOPNode);

	void SetTOPNodePDGState(UHoudiniPDGAssetLink* InPDGAssetLink, FTOPNode& TOPNode, const EPDGNodeState& InPDGState);

	void NotifyTOPNodePDGStateClear(UHoudiniPDGAssetLink* InPDGAssetLink, FTOPNode& TOPNode);

	void NotifyTOPNodeTotalWorkItem(UHoudiniPDGAssetLink* InPDGAssetLink, FTOPNode& TOPNode, const int32& Increment);

	void NotifyTOPNodeCookedWorkItem(UHoudiniPDGAssetLink* InPDGAssetLink, FTOPNode& TOPNode, const int32& Increment);

	void NotifyTOPNodeErrorWorkItem(UHoudiniPDGAssetLink* InPDGAssetLink, FTOPNode& TOPNode, const int32& Increment);

	void NotifyTOPNodeWaitingWorkItem(UHoudiniPDGAssetLink* InPDGAssetLink, FTOPNode& TOPNode, const int32& Increment);

	void NotifyTOPNodeScheduledWorkItem(UHoudiniPDGAssetLink* InPDGAssetLink, FTOPNode& TOPNode, const int32& Increment);

	void NotifyTOPNodeCookingWorkItem(UHoudiniPDGAssetLink* InPDGAssetLink, FTOPNode& TOPNode, const int32& Increment);

private:

	TArray<HAPI_StringHandle> PDGContextNames;
	TArray<HAPI_PDG_GraphContextId> PDGContextIDs;
	TArray<HAPI_PDG_EventInfo> PDGEventInfos;

	TArray<TWeakObjectPtr<UHoudiniPDGAssetLink>> PDGAssetLinks;

	int32 MaxNumberOfPDGEvents = 20;
	int32 MaxNumberOPDGContexts = 20;

};