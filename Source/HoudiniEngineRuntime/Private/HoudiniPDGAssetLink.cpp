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

#include "HoudiniEngineRuntime.h"
#include "HoudiniEngineRuntimePrivatePCH.h"
#include "HoudiniEngineRuntimeUtils.h"
#include "HoudiniOutput.h"

#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "Landscape.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "InstancedFoliageActor.h"

#include "PropertyPathHelpers.h"
#if WITH_EDITOR
	#include "FileHelpers.h"
#endif

//
UHoudiniPDGAssetLink::UHoudiniPDGAssetLink(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, AssetName()
	, AssetNodePath()
	, AssetID(-1)
	, SelectedTOPNetworkIndex(-1)
	, LinkState(EPDGLinkState::Inactive)
	, bAutoCook(false)
	, bUseTOPNodeFilter(true)
	, bUseTOPOutputFilter(true)
	, NumWorkitems(0)
	, WorkItemTally()
	, OutputCachePath()
	, bNeedsUIRefresh(false)
	, OutputParentActor(nullptr)
{
	TOPNodeFilter = HAPI_UNREAL_PDG_DEFAULT_TOP_FILTER;
	TOPOutputFilter = HAPI_UNREAL_PDG_DEFAULT_TOP_OUTPUT_FILTER;

#if WITH_EDITORONLY_DATA
	bBakeMenuExpanded = true;
	HoudiniEngineBakeOption = EHoudiniEngineBakeOption::ToActor;
	PDGBakeSelectionOption = EPDGBakeSelectionOption::All;
	PDGBakePackageReplaceMode = EPDGBakePackageReplaceModeOption::ReplaceExistingAssets;
#endif
	
	// Folder used for baking PDG outputs
	BakeFolder.Path = HAPI_UNREAL_DEFAULT_BAKE_FOLDER;

	// TODO:
	// Update init, move default filter to PCH
}

FOutputActorOwner::FOutputActorOwner()
{
	OutputActor = nullptr;
}

FOutputActorOwner::~FOutputActorOwner()
{
	// DestroyOutputActor();
}

FTOPWorkResultObject::FTOPWorkResultObject()
{
	// ResultObjects = nullptr;
	Name = FString();
	FilePath = FString();
	State = EPDGWorkResultState::None;
}

FTOPWorkResultObject::~FTOPWorkResultObject()
{
	// DestroyResultOutputs();
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
	NodePath = FString();
	ParentName = FString();

	WorkResultParent = nullptr;
	WorkResult.SetNum(0);

	bHidden = false;
	bAutoLoad = false;

	NodeState = EPDGNodeState::None;

	bShow = false;
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

void
FTOPNode::SetVisibleInLevel(bool bInVisible)
{
	if (bShow == bInVisible)
		return;
	
	bShow = bInVisible;
	UpdateOutputVisibilityInLevel();
}

void
FTOPNode::UpdateOutputVisibilityInLevel()
{
	AActor* Actor = GetOutputActor();
	if (IsValid(Actor))
	{
		//Actor->SetHidden(!bShow);
		Actor->bHidden = !bShow;
#if WITH_EDITOR
		Actor->SetIsTemporarilyHiddenInEditor(!bShow);
#endif
	}
	for (FTOPWorkResult& WorkItem : WorkResult)
	{
		for (FTOPWorkResultObject& WRO : WorkItem.ResultObjects)
		{
			AActor* WROActor = WRO.GetOutputActor();
			if (IsValid(WROActor))
			{
				//WROActor->SetHidden(!bShow);
				WROActor->bHidden = !bShow;
#if WITH_EDITOR
				WROActor->SetIsTemporarilyHiddenInEditor(!bShow);
#endif
			}

			// We need to manually handle child landscape's visiblity
			for (UHoudiniOutput* ResultOutput : WRO.GetResultOutputs())
			{
				if (!ResultOutput || ResultOutput->IsPendingKill())
					continue;

				for (auto Pair : ResultOutput->GetOutputObjects())
				{
					FHoudiniOutputObject OutputObject = Pair.Value;
					ALandscapeProxy* LandscapeProxy = Cast<ALandscapeProxy>(OutputObject.OutputObject);
					if (!LandscapeProxy || LandscapeProxy->IsPendingKill())
						continue;

					ALandscape* Landscape = LandscapeProxy->GetLandscapeActor();
					if (!Landscape || Landscape->IsPendingKill())
						continue;

					//Landscape->SetHidden(!bShow);
					Landscape->bHidden = !bShow;
#if WITH_EDITOR
					Landscape->SetIsTemporarilyHiddenInEditor(!bShow);
#endif
				}
			}
		}
	}	
}

void
FTOPNode::SetNotLoadedWorkResultsToLoad()
{
	for (FTOPWorkResult& WorkItem : WorkResult)
	{
		for (FTOPWorkResultObject& WRO : WorkItem.ResultObjects)
		{
			if (WRO.State == EPDGWorkResultState::NotLoaded)
				WRO.State = EPDGWorkResultState::ToLoad;
		}
    }	
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
UHoudiniPDGAssetLink::GetTOPNetworkByName(const FString& InName, TArray<FTOPNetwork>& InTOPNetworks, int32& OutIndex)
{
	OutIndex = INDEX_NONE;
	int32 Index = -1;
	for (FTOPNetwork& CurrentTOPNet : InTOPNetworks)
	{
		Index += 1;
		if (CurrentTOPNet.NodeName.Equals(InName))
		{
			OutIndex = Index;
			return &CurrentTOPNet;
		}
	}

	return nullptr;
}

FTOPNode*
UHoudiniPDGAssetLink::GetTOPNodeByName(const FString& InName, TArray<FTOPNode>& InTOPNodes, int32& OutIndex)
{
	OutIndex = INDEX_NONE;
	int32 Index = -1;
	for (FTOPNode& CurrentTOPNode : InTOPNodes)
	{
		Index += 1;
		if (CurrentTOPNode.NodeName.Equals(InName))
		{
			OutIndex = Index;
			return &CurrentTOPNode;
		}
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
	
	AActor* OutputActor = TOPNode.GetOutputActor();
	if (IsValid(OutputActor))
	{
		// Destroy any attached actors (which we'll assume that any attachments left
		// are untracked actors associated with the TOPNode)
		TArray<AActor*> AttachedActors;
		OutputActor->GetAttachedActors(AttachedActors);
		for (AActor* Actor : AttachedActors)
		{
			if (!IsValid(Actor))
				continue;
			Actor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
			Actor->Destroy();
		}
	}

	if (TOPNode.WorkResultParent && !TOPNode.WorkResultParent->IsPendingKill())
	{

		// TODO: Destroy the Parent Object
		// DestroyImmediate(topNode._workResultParentGO);
	}

	TOPNode.DestroyOutputActor();
}


void
UHoudiniPDGAssetLink::ClearWorkItemResultByID(const int32& InWorkItemID, FTOPNode& TOPNode)
{
	FTOPWorkResult* WorkResult = GetWorkResultByID(InWorkItemID, TOPNode);
	if (WorkResult)
	{
		DestroyWorkItemResultData(*WorkResult, TOPNode);
		// TODO: Should we destroy the FTOPWorkResult struct entirely here?
		//TOPNode.WorkResult.RemoveByPredicate 
	}
}

void
UHoudiniPDGAssetLink::DestroyWorkItemByID(const int32& InWorkItemID, FTOPNode& InTOPNode)
{
	// TODO: Update ClearWorkItemResultByID or GetWorkResultByID to return the index of the work item
	// so that we don't have to find its index again to remove it from the array
	ClearWorkItemResultByID(InWorkItemID, InTOPNode);
	// Find the index of the FTOPWorkResult for InWorkItemID in InTOPNode.WorkResult and remove it
	const int32 Index = InTOPNode.WorkResult.IndexOfByPredicate(
		[InWorkItemID](const FTOPWorkResult& InWorkItem) { return InWorkItem.WorkItemID == InWorkItemID; });
	if (Index != INDEX_NONE && Index >= 0)
		InTOPNode.WorkResult.RemoveAt(Index);
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

FDirectoryPath
UHoudiniPDGAssetLink::GetTemporaryCookFolder() const
{
	UObject* Owner = GetOuter();
	UHoudiniAssetComponent* HAC = Cast<UHoudiniAssetComponent>(Owner);
	if (HAC)
		return HAC->TemporaryCookFolder;
	
	FDirectoryPath TempPath;
	TempPath.Path = FHoudiniEngineRuntime::Get().GetDefaultTemporaryCookFolder();
	return TempPath;
}

void
UHoudiniPDGAssetLink::DestoryWorkResultObjectData(FTOPWorkResultObject& ResultObject)
{
	ResultObject.DestroyResultOutputs();
	ResultObject.DestroyOutputActor();
}

void
UHoudiniPDGAssetLink::DestroyWorkItemResultData(FTOPWorkResult& Result, FTOPNode& InTOPNode)
{
	if (Result.ResultObjects.Num() <= 0)
		return;

	for (FTOPWorkResultObject& ResultObject : Result.ResultObjects)
	{
		DestoryWorkResultObjectData(ResultObject);
	}
	
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

AActor*
UHoudiniPDGAssetLink::GetOwnerActor() const
{
	UObject* Outer = GetOuter();
	UActorComponent* Component = Cast<UActorComponent>(Outer);
	if (IsValid(Component))
		return Component->GetOwner();
	else
		return Cast<AActor>(Outer);
}

bool
UHoudiniPDGAssetLink::HasTemporaryOutputs() const
{
	// Loop over all networks, all nodes, all work items and check for any valid output objects
	for (const FTOPNetwork& TOPNetwork : AllTOPNetworks)
	{
		for (const FTOPNode& TOPNode : TOPNetwork.AllTOPNodes)
		{
			for (const FTOPWorkResult& WorkResult : TOPNode.WorkResult)
			{
				for (const FTOPWorkResultObject& WorkResultObject : WorkResult.ResultObjects)
				{
					// If the WorkResultObject's actor is not valid, then it no longer has temporary objects in the
					// scene
					if (!IsValid(WorkResultObject.GetOutputActor()))
						continue;
					
					for (UHoudiniOutput* Output : WorkResultObject.GetResultOutputs())
					{
						if (!IsValid(Output))
							continue;

						const EHoudiniOutputType OutputType = Output->GetType();
						for (const auto& OutputObjectPair : Output->GetOutputObjects())
						{
							if ((OutputType == EHoudiniOutputType::Landscape && IsValid(OutputObjectPair.Value.OutputObject)) ||
								IsValid(OutputObjectPair.Value.OutputComponent))
							{
								return true;								
							}
						}
					}
				}
			}
		}
	}

	return false;
}

void
UHoudiniPDGAssetLink::UpdatePostDuplicate()
{
	// Loop over all networks, all nodes, all work items and clear output actors
	for (FTOPNetwork& TOPNetwork : AllTOPNetworks)
	{
		for (FTOPNode& TOPNode : TOPNetwork.AllTOPNodes)
		{
			for (FTOPWorkResult& WorkResult : TOPNode.WorkResult)
			{
				for (FTOPWorkResultObject& WorkResultObject : WorkResult.ResultObjects)
				{
					WorkResultObject.SetOutputActor(nullptr);
				}
			}
			TOPNode.SetOutputActor(nullptr);
		}
	}
}

void
UHoudiniPDGAssetLink::UpdateTOPNodeAutoloadAndVisibility()
{
	for (FTOPNetwork& TOPNetwork : AllTOPNetworks)
	{
		for (FTOPNode& TOPNode : TOPNetwork.AllTOPNodes)
		{
			if (TOPNode.bAutoLoad)
			{
				// Set work results that are cooked but in NotLoaded state to ToLoad
				TOPNode.SetNotLoadedWorkResultsToLoad();
			}
			
			TOPNode.UpdateOutputVisibilityInLevel();
		}
	}
}

void
UHoudiniPDGAssetLink::FilterTOPNodesAndOutputs()
{
	for (FTOPNetwork& TOPNetwork : AllTOPNetworks)
	{
		for (FTOPNode& TOPNode : TOPNetwork.AllTOPNodes)
		{
			// TOP Node visibility filter via TOPNodeFilter
			if (bUseTOPNodeFilter)
			{
				TOPNode.bHidden = !TOPNodeFilter.IsEmpty() && !TOPNode.NodeName.StartsWith(TOPNodeFilter);
			}
			else
			{
				TOPNode.bHidden = false;
			}

			// Auto load results filter via TOPNodeOutputFilter
			if (bUseTOPOutputFilter)
			{
				const bool bNewAutoLoad = TOPOutputFilter.IsEmpty() || TOPNode.NodeName.StartsWith(TOPOutputFilter);
				if (bNewAutoLoad != TOPNode.bAutoLoad)
				{
					if (bNewAutoLoad)
					{
						// Set work results that are cooked but in NotLoaded state to ToLoad
						TOPNode.bAutoLoad = true;
						TOPNode.SetNotLoadedWorkResultsToLoad();
						TOPNode.SetVisibleInLevel(true);
					}
					else
					{
						TOPNode.bAutoLoad = false;
						TOPNode.SetVisibleInLevel(false);
					}
					TOPNode.UpdateOutputVisibilityInLevel();
				}
			}
		}
	}
}

#if WITH_EDITORONLY_DATA
void
UHoudiniPDGAssetLink::PostEditChangeChainProperty(FPropertyChangedChainEvent& InPropertyChangedChainEvent)
{
	Super::PostEditChangeChainProperty(InPropertyChangedChainEvent);

	const FName PropertyName = InPropertyChangedChainEvent.GetPropertyName();
	if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UHoudiniPDGAssetLink, bUseTOPNodeFilter) ||
		PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UHoudiniPDGAssetLink, TOPNodeFilter) ||
		PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UHoudiniPDGAssetLink, bUseTOPOutputFilter) ||
		PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UHoudiniPDGAssetLink, TOPOutputFilter))
	{
		// Refilter TOP nodes
		FilterTOPNodesAndOutputs();
		bNeedsUIRefresh = true;
	}
	else if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UHoudiniPDGAssetLink, SelectedTOPNetworkIndex) ||
			 PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UHoudiniPDGAssetLink, bBakeMenuExpanded))
	{
		bNeedsUIRefresh = true;
	}
	else
	{
		if (Cast<UArrayProperty>(InPropertyChangedChainEvent.MemberProperty) &&
			InPropertyChangedChainEvent.MemberProperty->GetName() == GET_MEMBER_NAME_STRING_CHECKED(FTOPNetwork, AllTOPNodes))
		{
			// Get the TOPNode instance via AllTOPNetworks and AllTOPNodes within the FTOPNetwork, using the array
			// indices on the property change event
			const int32 TOPNetworkIndex = InPropertyChangedChainEvent.GetArrayIndex(GET_MEMBER_NAME_STRING_CHECKED(UHoudiniPDGAssetLink, AllTOPNetworks));
			const int32 TOPNodeIndex = InPropertyChangedChainEvent.GetArrayIndex(GET_MEMBER_NAME_STRING_CHECKED(FTOPNetwork, AllTOPNodes));

			if (TOPNetworkIndex != INDEX_NONE && TOPNodeIndex != INDEX_NONE && AllTOPNetworks.IsValidIndex(TOPNetworkIndex))
			{
				FTOPNetwork& TOPNetwork = AllTOPNetworks[TOPNetworkIndex];
				if (TOPNetwork.AllTOPNodes.IsValidIndex(TOPNodeIndex))
				{
					FTOPNode& TOPNode = TOPNetwork.AllTOPNodes[TOPNodeIndex];
					if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FTOPNode, bAutoLoad))
					{
						if (TOPNode.bAutoLoad)
						{
							TOPNode.SetNotLoadedWorkResultsToLoad();
						}
					}
					else if (PropertyName == TEXT("bShow"))  // GET_MEMBER_NAME_STRING_CHECKED(FTOPNode, bShow))
					{
						TOPNode.UpdateOutputVisibilityInLevel();
					}
				}
			}
		}
	}
}

#endif

void
UHoudiniPDGAssetLink::NotifyPostEditChangeProperty(FName InPropertyPath)
{
#if WITH_EDITORONLY_DATA
	const FCachedPropertyPath CachedPath(InPropertyPath.ToString());
	if (CachedPath.Resolve(this))
	{
		// Notify that we have changed the property
		// FPropertyChangedEvent Evt = CachedPath.ToPropertyChangedEvent(EPropertyChangeType::Unspecified);
		// Construct FPropertyChangedEvent from the cached property path
		const int32 NumSegments = CachedPath.GetNumSegments();
		
		TArray<const UObject*> TopLevelObjects;
		TopLevelObjects.Add(this);

		FPropertyChangedEvent Evt(
			CastChecked<UProperty>(CachedPath.GetLastSegment().GetField()),
			EPropertyChangeType::Unspecified,
			&TopLevelObjects);
		
		if(NumSegments > 1)
		{
			Evt.SetActiveMemberProperty(CastChecked<UProperty>(CachedPath.GetSegment(NumSegments - 2).GetField()));
		}

		// Set the array of indices to the changed property
		TArray<TMap<FString,int32>> ArrayIndicesPerObject;
		ArrayIndicesPerObject.AddDefaulted(1);
		for (int32 SegmentIdx = 0; SegmentIdx < NumSegments; ++SegmentIdx)
		{
			const FPropertyPathSegment& Segment = CachedPath.GetSegment(SegmentIdx);
			const int32 ArrayIndex = Segment.GetArrayIndex();
			if (ArrayIndex != INDEX_NONE)
			{
				ArrayIndicesPerObject[0].Add(Segment.GetName().ToString(), ArrayIndex);
			}
		}
		Evt.SetArrayIndexPerObject(ArrayIndicesPerObject);
		
		FEditPropertyChain Chain;
		CachedPath.ToEditPropertyChain(Chain);
		FPropertyChangedChainEvent ChainEvent(Chain, Evt);
		ChainEvent.ObjectIteratorIndex = 0;
		PostEditChangeChainProperty(ChainEvent);
	}
	else
	{
		HOUDINI_LOG_WARNING(TEXT("Could not resolve property path '%s' on %s."), *InPropertyPath.ToString(), *GetFullName());
	}
#endif
}

#if WITH_EDITORONLY_DATA
void
UHoudiniPDGAssetLink::PostTransacted(const FTransactionObjectEvent& TransactionEvent)
{
	Super::PostTransacted(TransactionEvent);
	
	if (TransactionEvent.GetEventType() != ETransactionObjectEventType::UndoRedo)
		return;

	bool bDoFilterTOPNodesAndOutputs = false;
	bool bDoUpdateTOPNodeAutoloadAndVisibility = false;
	for (const FName& PropName : TransactionEvent.GetChangedProperties())
	{
		if (PropName == GET_MEMBER_NAME_STRING_CHECKED(UHoudiniPDGAssetLink, bUseTOPNodeFilter) ||
			PropName == GET_MEMBER_NAME_STRING_CHECKED(UHoudiniPDGAssetLink, TOPNodeFilter) ||
			PropName == GET_MEMBER_NAME_STRING_CHECKED(UHoudiniPDGAssetLink, bUseTOPOutputFilter) ||
			PropName == GET_MEMBER_NAME_STRING_CHECKED(UHoudiniPDGAssetLink, TOPOutputFilter))
		{
			bDoFilterTOPNodesAndOutputs = true;
			bNeedsUIRefresh = true;
		}
		else if (PropName == GET_MEMBER_NAME_STRING_CHECKED(UHoudiniPDGAssetLink, SelectedTOPNetworkIndex) ||
				 PropName == GET_MEMBER_NAME_STRING_CHECKED(UHoudiniPDGAssetLink, HoudiniEngineBakeOption) ||
				 PropName == GET_MEMBER_NAME_STRING_CHECKED(UHoudiniPDGAssetLink, PDGBakeSelectionOption) ||
				 PropName == GET_MEMBER_NAME_STRING_CHECKED(UHoudiniPDGAssetLink, PDGBakePackageReplaceMode) ||
				 PropName == GET_MEMBER_NAME_STRING_CHECKED(UHoudiniPDGAssetLink, bBakeMenuExpanded))
		{
			bNeedsUIRefresh = true;
		}
		else if (PropName == GET_MEMBER_NAME_STRING_CHECKED(UHoudiniPDGAssetLink, AllTOPNetworks))
		{
			// Currently we don't get the full property path from the TransactionEvent (contrary to the comment
			// on the GetChangedProperties function :/), so we have to handle possible FTOPNode.bShow and
			// FTOPNode.bAutoload changes here
			bDoUpdateTOPNodeAutoloadAndVisibility = true;
			bNeedsUIRefresh = true;
		}
	}

	if (bDoFilterTOPNodesAndOutputs)
		FilterTOPNodesAndOutputs();

	if (bDoUpdateTOPNodeAutoloadAndVisibility)
		UpdateTOPNodeAutoloadAndVisibility();
}
#endif

void
FTOPWorkResultObject::DestroyResultOutputs()
{
	// Delete output components and gather output objects for deletion
	bool bDidDestroyObjects = false;
	for (UHoudiniOutput* CurOutput : ResultOutputs)
	{
		for (auto& Pair : CurOutput->GetOutputObjects())
		{
			FHoudiniOutputObjectIdentifier& Identifier = Pair.Key;
			FHoudiniOutputObject& OutputObject = Pair.Value;
			if (OutputObject.OutputComponent && !OutputObject.OutputComponent->IsPendingKill())
			{
				// Instancer components require some special handling around foliage
				// TODO: move/refactor so that we can use the InstanceTranslator's helper functions (RemoveAndDestroyComponent and CleanupFoliageInstances)
				// When destroying a component, we have to be sure it's not an HISMC owned by an InstanceFoliageActor
				bool bDestroyComponent = true;
				if (OutputObject.OutputComponent->IsA<UHierarchicalInstancedStaticMeshComponent>())
				{
					// When destroying a component, we have to be sure it's not an HISMC owned by an InstanceFoliageActor
					UHierarchicalInstancedStaticMeshComponent* HISMC = Cast<UHierarchicalInstancedStaticMeshComponent>(OutputObject.OutputComponent);
					if (HISMC->GetOwner() && HISMC->GetOwner()->IsA<AInstancedFoliageActor>())
						bDestroyComponent = false;
				}
				if (bDestroyComponent)
				{
					UFoliageInstancedStaticMeshComponent* FISMC = Cast<UFoliageInstancedStaticMeshComponent>(OutputObject.OutputComponent);
					if (FISMC && !FISMC->IsPendingKill())
					{
						// Make sure foliage our foliage instances have been removed
						USceneComponent* ParentComponent = Cast<USceneComponent>(FISMC->GetOuter());
						if (ParentComponent && !ParentComponent->IsPendingKill())
						{
							UStaticMesh* FoliageSM = FISMC->GetStaticMesh();
							if (!FoliageSM || FoliageSM->IsPendingKill())
								return;

							// If we are a foliage HISMC, then our owner is an Instanced Foliage Actor,
							// if it is not, then we are just a "regular" HISMC
							AInstancedFoliageActor* InstancedFoliageActor = Cast<AInstancedFoliageActor>(FISMC->GetOwner());
							if (!InstancedFoliageActor || InstancedFoliageActor->IsPendingKill())
								return;

							UFoliageType *FoliageType = InstancedFoliageActor->GetLocalFoliageTypeForSource(FoliageSM);
							if (!FoliageType || FoliageType->IsPendingKill())
								return;
#if WITH_EDITOR
							// Clean up the instances previously generated for that component
							InstancedFoliageActor->DeleteInstancesForComponent(ParentComponent, FoliageType);

							// Remove the foliage type if it doesn't have any more instances
							if(FISMC->GetInstanceCount() == 0)
								InstancedFoliageActor->RemoveFoliageType(&FoliageType, 1);
#endif
						}

						// do not delete FISMC that still have instances left
						// as we have cleaned up our instances before, these have been hand-placed
						if (FISMC->GetInstanceCount() > 0)
							bDestroyComponent = false;
					}

					if (bDestroyComponent)
					{
						USceneComponent* SceneComponent = Cast<USceneComponent>(OutputObject.OutputComponent);
						if (SceneComponent && !SceneComponent->IsPendingKill())
						{
							// Remove from its actor first
							if (SceneComponent->GetOwner())
								SceneComponent->GetOwner()->RemoveOwnedComponent(SceneComponent);
						
							// Detach from its parent component if attached
							SceneComponent->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
							SceneComponent->UnregisterComponent();
							SceneComponent->DestroyComponent();
						
							bDidDestroyObjects = true;
							
							OutputObject.OutputComponent = nullptr;
						}
					}
				}
			}
			if (OutputObject.OutputObject && !OutputObject.OutputObject->IsPendingKill())
			{
				// For actors we detach them first and then destroy
				AActor* Actor = Cast<AActor>(OutputObject.OutputObject);
				UHoudiniLandscapePtr* LandscapePtr = Cast<UHoudiniLandscapePtr>(OutputObject.OutputObject);
				if (LandscapePtr)
				{
					Actor = LandscapePtr->GetRawPtr();
				}

				if (Actor)
				{
					Actor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
					Actor->Destroy();

					bDidDestroyObjects = true;
				}
				else
				{
					// ... if not an actor, mark as pending kill
					// OutputObject.OutputObject->MarkPendingKill();
					if (IsValid(OutputObject.OutputObject))
						OutputObjectsToDelete.Add(OutputObject.OutputObject);
					OutputObject.OutputObject = nullptr;
				}
			}
		}
	}

	ResultOutputs.Empty();

	if (bDidDestroyObjects)
		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
	
	// Delete the output objects we found
	if (OutputObjectsToDelete.Num() > 0)
		FHoudiniEngineRuntimeUtils::SafeDeleteObjects(OutputObjectsToDelete);
}

bool
FOutputActorOwner::CreateOutputActor(UWorld* InWorld, UHoudiniPDGAssetLink* InAssetLink, AActor *InParentActor, const FName& InName)
{
	// InAssetLink and InWorld must not be null
	if (!InAssetLink || InAssetLink->IsPendingKill())
	{
		HOUDINI_LOG_ERROR(TEXT("[FTOPWorkResultObject::CreateWorkResultActor]: InAssetLink is null!"));
		return false;
	}
	if (!InWorld || InWorld->IsPendingKill())
	{
		HOUDINI_LOG_ERROR(TEXT("[FTOPWorkResultObject::CreateWorkResultActor]: InWorld is null!"));
		return false;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Name = MakeUniqueObjectName(InWorld, AActor::StaticClass(), InName);
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	//SpawnParams.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;

	OutputActor = InWorld->SpawnActor<AActor>(SpawnParams);
#if WITH_EDITOR
	OutputActor->SetActorLabel(InName.ToString());
#endif
	
	// Set the actor transform: create a root component if it does not have one
	USceneComponent* RootComponent = OutputActor->GetRootComponent();
	if (!RootComponent || RootComponent->IsPendingKill())
	{
		RootComponent = NewObject<USceneComponent>(OutputActor, USceneComponent::StaticClass(), NAME_None, RF_Transactional);

		// Change the creation method so the component is listed in the details panels
		RootComponent->CreationMethod = EComponentCreationMethod::Instance;
		OutputActor->SetRootComponent(RootComponent);
		RootComponent->OnComponentCreated();
		RootComponent->RegisterComponent();
	}
	
	RootComponent->SetVisibility(true);
	RootComponent->SetMobility(EComponentMobility::Static);

	const FVector ActorSpawnLocation = InParentActor ? InParentActor->GetActorLocation() : FVector::ZeroVector;
	const FRotator ActorSpawnRotator = InParentActor ? InParentActor->GetActorRotation() : FRotator::ZeroRotator;
	OutputActor->SetActorLocation(ActorSpawnLocation);
	OutputActor->SetActorRotation(ActorSpawnRotator);

	UObject* AssetLinkOuter = InAssetLink->GetOuter();
	AActor* AssetLinkActor = nullptr;
	UActorComponent* AssetLinkComponent = Cast<UActorComponent>(AssetLinkOuter);
	if (AssetLinkComponent)
	{
		AssetLinkActor = AssetLinkComponent->GetOwner();
	}
	else
	{
		AssetLinkActor = Cast<AActor>(AssetLinkOuter);
	}
	
#if WITH_EDITOR
	if (IsValid(InParentActor))
	{
		OutputActor->SetFolderPath(InParentActor->GetFolderPath());
		OutputActor->AttachToActor(InParentActor, FAttachmentTransformRules::KeepWorldTransform);
	}
	else if (IsValid(AssetLinkActor))
	{
		OutputActor->SetFolderPath(*FString::Format(
			TEXT("{0}/{1}_Output"), 
			{ FStringFormatArg(AssetLinkActor->GetFolderPath().ToString()), FStringFormatArg(AssetLinkActor->GetActorLabel()) }
		));
	}
	else
	{
		OutputActor->SetFolderPath(*FString::Format(TEXT("{0}_Output"), { FStringFormatArg(InAssetLink->GetName()) }));
	}
#else
	if(IsValid(InParentActor))
	{
		OutputActor->AttachToActor(InParentActor, FAttachmentTransformRules::KeepWorldTransform);
	}
#endif

	return true;
}

bool
FOutputActorOwner::DestroyOutputActor()
{
	bool bDestroyed = false;
	if (IsValid(OutputActor))
	{
		// Detach from parent before destroying the actor
		OutputActor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		OutputActor->Destroy();

		bDestroyed = true;
	}

	OutputActor = nullptr;

	return bDestroyed;
}
