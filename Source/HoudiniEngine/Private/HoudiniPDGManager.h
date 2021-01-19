/*
* Copyright (c) <2021> Side Effects Software Inc.
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

#include "HAL/PlatformProcess.h"

#include "MessageEndpoint.h"

class UHoudiniAssetComponent;
class UHoudiniPDGAssetLink;
class UTOPNetwork;
class UTOPNode;
class FSocket;

enum class EPDGNodeState : uint8;

// BGEO commandlet status
enum class HOUDINIENGINE_API EHoudiniBGEOCommandletStatus : uint8
{
	// PDG manager has not tried to start the commandlet
	NotStarted,
	// PDG manager has PID for the commandlet and the host OS indicates it is running, but no message has been
	// received from it yet
	Running,
	// PDG manager has PID for the commandlet, the host OS indicates it is running, and a discover message has been
	// received
	Connected,
	// After being in the Connected state, the commandlet stopped running (host OS indicates PID is not valid)
	Crashed
};

struct HOUDINIENGINE_API FHoudiniPDGManager
{

public:

	FHoudiniPDGManager();
	
	virtual ~FHoudiniPDGManager();
	
	// Initialize the PDG Asset Link for a HoudiniAssetComponent
	// returns true if the HAC uses a PDG asset, and a PDGAssetLink was successfully created
	bool InitializePDGAssetLink(UHoudiniAssetComponent* InHAC);

	// Updates an existing PDG AssetLink
	static bool UpdatePDGAssetLink(UHoudiniPDGAssetLink* PDGAssetLink);

	// Find all TOP networks from linked HDA, as well as the TOP nodes within, and populate internal state.
	static bool PopulateTOPNetworks(UHoudiniPDGAssetLink* PDGAssetLink, bool bInZeroWorkItemTallys=false);

	static void RefreshPDGAssetLinkUI(UHoudiniPDGAssetLink* InAssetLink);

	// Indicates if the Asset is a PDG Asset
	// This will look for TOP nodes in all SOP/TOP net in the HDA.
	static bool IsPDGAsset(const HAPI_NodeId& InAssetId);

	// Given TOP nodes from a TOP network, populate internal state from each TOP node.
	static bool PopulateTOPNodes(
		const TArray<HAPI_NodeId>& InTopNodeIDs,
		UTOPNetwork* InTOPNetwork,
		UHoudiniPDGAssetLink* InPDGAssetLink,
		bool bInZeroWorkItemTallys=false);

	// Cook the specified TOP node.
	static void CookTOPNode(UTOPNode* InTOPNode);

	// Dirty the specified TOP node and clear its work item results.
	static void DirtyTOPNode(UTOPNode* InTOPNode);

	// // Dirty all the tasks/work items of the specified TOP node. Does not
	// // clear its work item results.
	// static void DirtyAllTasksOfTOPNode(FTOPNode& InTOPNode);

	// Dirty the TOP network and clear all work item results.
	static void DirtyAll(UTOPNetwork* InTOPNet);

	// Cook the output TOP node of the currently selected TOP network.
	static void CookOutput(UTOPNetwork* InTOPNet);

	// Pause the PDG cook of the currently selected TOP network
	static void PauseCook(UTOPNetwork* InTOPNet);

	// Cancel the PDG cook of the currently selected TOP network
	static void CancelCook(UTOPNetwork* InTOPNet);

	static void NotifyAssetCooked(UHoudiniPDGAssetLink* InAssetLink, const bool& bSuccess);

	// Update all registered PDG Asset links
	void Update();

	void ReinitializePDGContext();
	
	// Clear all of the specified work item's results from the specified TOP node. This destroys any loaded results
	// (geometry etc), but keeps the work item struct.
	//void ClearWorkItemResult(const HAPI_PDG_GraphContextId& InContextID, const HAPI_PDG_EventInfo& InEventInfo, FTOPNode& TOPNode);
	void ClearWorkItemResult(UHoudiniPDGAssetLink* InAssetLink, const HAPI_PDG_WorkitemId& InWorkItemID, UTOPNode* InTOPNode);

	// Clear the specified work item's results from the specified TOP node and remove the work item struct from the TOP
	// node. This destroys any loaded results (geometry etc), and the work item struct.
	void RemoveWorkItem(UHoudiniPDGAssetLink* InAssetLink, const HAPI_PDG_WorkitemId& InWorkItemID, UTOPNode* InTOPNode);

	// Create FTOPWorkResult for a given TOP node, and optionally (via bInLoadResultObjects) create its FTOPWorkResultObjects.
	// Geometry is not directly loaded by this function, the FTOPWorkResultObjects' states will be set to ToLoad and
	// the ProcessWorkItemResults function will take care of loading the geo.
	// Results must be tagged with 'file', and must have a file path, otherwise will not included.
	bool CreateWorkItemResult(UTOPNode* InTOPNode, const HAPI_PDG_GraphContextId& InContextID, HAPI_PDG_WorkitemId InWorkItemID, bool bInLoadResultObjects=false);

	// Handles replies from commandlets in response to a FHoudiniPDGImportBGEODiscoverMessage
	void HandleImportBGEODiscoverMessage(
		const struct FHoudiniPDGImportBGEODiscoverMessage& InMessage,
		const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext);

	// Handles messages sent by the commandlet once an import of a bgeo is complete, and uassets have been created.
	void HandleImportBGEOResultMessage(
		const struct FHoudiniPDGImportBGEOResultMessage& InMessage, 
		const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext);

	// Create the bgeo commandlet endpoint and start the commandlet (if not already running).
	bool CreateBGEOCommandletAndEndpoint();

	void StopBGEOCommandletAndEndpoint();

	// Updates and returns the BGEO commandlet status
	EHoudiniBGEOCommandletStatus UpdateAndGetBGEOCommandletStatus();

private:
	
	void UpdatePDGContexts();

	void ProcessWorkItemResults();

	void ProcessPDGEvent(const HAPI_PDG_GraphContextId& InContextID, HAPI_PDG_EventInfo& EventInfo);

	static void ResetPDGEventInfo(HAPI_PDG_EventInfo& InEventInfo);

	// Returns the PDGAssetLink and FTOPNode associated with this TOP node ID
	bool GetTOPAssetLinkAndNode(const HAPI_NodeId& InNodeID, UHoudiniPDGAssetLink*& OutAssetLink, UTOPNode*& OutTOPNode);

	void SetTOPNodePDGState(UHoudiniPDGAssetLink* InPDGAssetLink, UTOPNode* InTOPNode, const EPDGNodeState& InPDGState);

	void NotifyTOPNodePDGStateClear(UHoudiniPDGAssetLink* InPDGAssetLink, UTOPNode* InTOPNode);

	void NotifyTOPNodeTotalWorkItem(UHoudiniPDGAssetLink* InPDGAssetLink, UTOPNode* InTOPNode, const int32& Increment);

	void NotifyTOPNodeCookedWorkItem(UHoudiniPDGAssetLink* InPDGAssetLink, UTOPNode* InTOPNode, const int32& Increment);

	void NotifyTOPNodeErrorWorkItem(UHoudiniPDGAssetLink* InPDGAssetLink, UTOPNode* InTOPNode, const int32& Increment);

	void NotifyTOPNodeWaitingWorkItem(UHoudiniPDGAssetLink* InPDGAssetLink, UTOPNode* InTOPNode, const int32& Increment);

	void NotifyTOPNodeScheduledWorkItem(UHoudiniPDGAssetLink* InPDGAssetLink, UTOPNode* InTOPNode, const int32& Increment);

	void NotifyTOPNodeCookingWorkItem(UHoudiniPDGAssetLink* InPDGAssetLink, UTOPNode* InTOPNode, const int32& Increment);

private:

	TArray<HAPI_StringHandle> PDGContextNames;
	TArray<HAPI_PDG_GraphContextId> PDGContextIDs;
	TArray<HAPI_PDG_EventInfo> PDGEventInfos;

	TArray<TWeakObjectPtr<UHoudiniPDGAssetLink>> PDGAssetLinks;

	int32 MaxNumberOfPDGEvents = 20;
	int32 MaxNumberOPDGContexts = 20;

	TSharedPtr<FMessageEndpoint, ESPMode::ThreadSafe> BGEOCommandletEndpoint;
	FMessageAddress BGEOCommandletAddress;
	FProcHandle BGEOCommandletProcHandle;
	FGuid BGEOCommandletGuid;
	uint32 BGEOCommandletProcessId;
	// Keep track of the BGEO commandlet status
	EHoudiniBGEOCommandletStatus BGEOCommandletStatus;
};