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

//#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"

#include "HoudiniAsset.h"
#include "HoudiniAssetComponent.h"


#include "HoudiniPDGAssetLink.generated.h"

struct FHoudiniPackageParams;

UENUM()
enum class EPDGLinkState : uint8
{
	Inactive,
	Linking,
	Linked,
	Error_Not_Linked
};


UENUM()
enum class EPDGNodeState : uint8
{
	None,
	Dirtied,
	Dirtying,
	Cooking,
	Cook_Complete,
	Cook_Failed
};

UENUM()
enum class EPDGWorkResultState : uint8
{
	None,
	ToLoad,
	Loading,
	Loaded,
	ToDelete,
	Deleting,
	Deleted,
	NotLoaded
};

#if WITH_EDITORONLY_DATA
UENUM()
enum class EPDGBakeSelectionOption : uint8
{
	All,
	SelectedNetwork,
	SelectedNode
};
#endif

#if WITH_EDITORONLY_DATA
UENUM()
enum class EPDGBakePackageReplaceModeOption : uint8
{
	CreateNewAssets,
	ReplaceExistingAssets	
};
#endif

USTRUCT()
struct HOUDINIENGINERUNTIME_API FOutputActorOwner
{
	GENERATED_BODY();
public:
	FOutputActorOwner()
		: OutputActor(nullptr) {};
	
	virtual ~FOutputActorOwner() {};
	
	// Create OutputActor, an actor to hold work result output
	virtual bool CreateOutputActor(UWorld* InWorld, UHoudiniPDGAssetLink* InAssetLink, AActor *InParentActor, const FName& InName);

	// Return OutputActor
	virtual AActor* GetOutputActor() const { return OutputActor; }

	// Setter for OutputActor
	virtual void SetOutputActor(AActor* InActor) { OutputActor = InActor; }
	
	// Destroy OutputActor if it is valid.
	virtual bool DestroyOutputActor();

private:
	UPROPERTY(NonTransactional)
	AActor* OutputActor;
	
};

USTRUCT()
struct HOUDINIENGINERUNTIME_API FTOPWorkResultObject
{
	GENERATED_USTRUCT_BODY()

public:

	// Constructor
	FTOPWorkResultObject();

	// Call DestroyResultObjects in the destructor
	virtual ~FTOPWorkResultObject();

	// Set ResultObjects to a copy of InUpdatedOutputs
	void SetResultOutputs(const TArray<UHoudiniOutput*>& InUpdatedOutputs) { ResultOutputs = InUpdatedOutputs; }

	// Getter for ResultOutputs
	TArray<UHoudiniOutput*>& GetResultOutputs() { return ResultOutputs; }
	
	// Getter for ResultOutputs
	const TArray<UHoudiniOutput*>& GetResultOutputs() const { return ResultOutputs; }

	// Destroy ResultOutputs
	void DestroyResultOutputs();

	// Get the OutputActor owner struct
	FOutputActorOwner& GetOutputActorOwner() { return OutputActorOwner; }

	// Get the OutputActor owner struct
	const FOutputActorOwner& GetOutputActorOwner() const { return OutputActorOwner; }

public:

	UPROPERTY(NonTransactional)
	FString					Name;
	UPROPERTY(NonTransactional)
	FString					FilePath;
	UPROPERTY(NonTransactional)
	EPDGWorkResultState		State;

protected:
	// UPROPERTY()
	// TArray<UObject*>		ResultObjects;

	UPROPERTY(NonTransactional)
	TArray<UHoudiniOutput*> ResultOutputs;

private:
	// List of objects to delete, internal use only (DestroyResultOutputs)
	UPROPERTY(NonTransactional)
	TArray<UObject*> OutputObjectsToDelete;

	UPROPERTY(NonTransactional)
	FOutputActorOwner OutputActorOwner;
};

USTRUCT()
struct HOUDINIENGINERUNTIME_API FTOPWorkResult
{
	GENERATED_USTRUCT_BODY()

public:

	// Constructor
	FTOPWorkResult();

	// Comparison operator, used by hashing containers and arrays.
	bool operator==(const FTOPWorkResult& OtherWorkResult) const;

public:

	UPROPERTY(NonTransactional)
	int32							WorkItemIndex;
	UPROPERTY(Transient)
	int32							WorkItemID;
	
	UPROPERTY(NonTransactional)
	TArray<FTOPWorkResultObject>	ResultObjects;

	/*
	UPROPERTY()
	TArray<UObject*>				ResultObjects;

	UPROPERTY()
	TArray<FString>					ResultNames;
	UPROPERTY()
	TArray<FString>					ResultFilePaths;
	UPROPERTY()
	TArray<EPDGWorkResultState>		ResultStates;
	*/
};


USTRUCT()
struct HOUDINIENGINERUNTIME_API FWorkItemTally
{
	GENERATED_USTRUCT_BODY()

public:

	// Constructor
	FWorkItemTally();

	void ZeroAll();
	
	bool AreAllWorkItemsComplete() const;
	bool AnyWorkItemsFailed() const;
	bool AnyWorkItemsPending() const;

	FString ProgressRatio() const;

public:

	UPROPERTY()
	int32 TotalWorkItems;
	UPROPERTY()
	int32 WaitingWorkItems;
	UPROPERTY()
	int32 ScheduledWorkItems;
	UPROPERTY()
	int32 CookingWorkItems;
	UPROPERTY()
	int32 CookedWorkItems;
	UPROPERTY()
	int32 ErroredWorkItems;
};

// Container for baked outputs of a PDG work result object. 
USTRUCT()
struct HOUDINIENGINERUNTIME_API FHoudiniPDGWorkResultObjectBakedOutput
{
	GENERATED_BODY()

	public:
		// Array of baked output per output index of the work result object's outputs.
		UPROPERTY()
		TArray<FHoudiniBakedOutput> BakedOutputs;
};

UCLASS()
class HOUDINIENGINERUNTIME_API UTOPNode : public UObject
{
	GENERATED_BODY()

public:

	// Constructor
	UTOPNode();

	// Comparison operator, used by hashing containers and arrays.
	bool operator==(const UTOPNode& Other) const;

	void Reset();

	bool AreAllWorkItemsComplete() const { return WorkItemTally.AreAllWorkItemsComplete(); };
	bool AnyWorkItemsFailed() const { return WorkItemTally.AnyWorkItemsFailed(); };
	bool AnyWorkItemsPending() const { return WorkItemTally.AnyWorkItemsPending(); };

	bool IsVisibleInLevel() const { return bShow; }
	void SetVisibleInLevel(bool bInVisible);
	void UpdateOutputVisibilityInLevel();

	// Sets all WorkResultObjects that are in the NotLoaded state to ToLoad.
	void SetNotLoadedWorkResultsToLoad(bool bInAlsoSetDeletedToLoad=false);

	// Sets all WorkResultObjects that are in the Loaded state to ToDelete (will delete output objects and output
	// actors).
	void SetLoadedWorkResultsToDelete();

	// Immediately delete Loaded work result output objects (keeps the work items and result arrays but deletes the output
	// objects and actors and sets the state to Deleted.
	void DeleteWorkResultOutputObjects();

	// Get the OutputActor owner struct
	FOutputActorOwner& GetOutputActorOwner() { return OutputActorOwner; }

	// Get the OutputActor owner struct
	const FOutputActorOwner& GetOutputActorOwner() const { return OutputActorOwner; }

	// Get the baked outputs from the last bake. The map keys are [work_result_index]_[work_result_object_index]
	TMap<FString, FHoudiniPDGWorkResultObjectBakedOutput>& GetBakedWorkResultObjectsOutputs() { return BakedWorkResultObjectOutputs; }
	const TMap<FString, FHoudiniPDGWorkResultObjectBakedOutput>& GetBakedWorkResultObjectsOutputs() const { return BakedWorkResultObjectOutputs; }
	bool GetBakedWorkResultObjectOutputsKey(int32 InWorkResultIndex, int32 InWorkResultObjectIndex, FString& OutKey) const;
	static FString GetBakedWorkResultObjectOutputsKey(int32 InWorkResultIndex, int32 InWorkResultObjectIndex);
	bool GetBakedWorkResultObjectOutputs(int32 InWorkResultIndex, int32 InWorkResultObjectIndex, FHoudiniPDGWorkResultObjectBakedOutput*& OutBakedOutput);
	bool GetBakedWorkResultObjectOutputs(int32 InWorkResultIndex, int32 InWorkResultObjectIndex, FHoudiniPDGWorkResultObjectBakedOutput const*& OutBakedOutput) const;
	
#if WITH_EDITOR
	void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
#endif

#if WITH_EDITOR
	void PostTransacted(const FTransactionObjectEvent& TransactionEvent) override;
#endif

public:

	UPROPERTY(Transient, NonTransactional)
	int32					NodeId;
	UPROPERTY(NonTransactional)
	FString					NodeName;
	UPROPERTY(NonTransactional)
	FString					NodePath;
	UPROPERTY(NonTransactional)
	FString					ParentName;
	
	UPROPERTY()
	UObject*				WorkResultParent;
	UPROPERTY(NonTransactional)
	TArray<FTOPWorkResult>	WorkResult;

	// Hidden in the nodes combobox
	UPROPERTY()
	bool					bHidden;
	UPROPERTY()
	bool					bAutoLoad;

	UPROPERTY(Transient, NonTransactional)
	EPDGNodeState 			NodeState;

	UPROPERTY(Transient, NonTransactional)
	FWorkItemTally			WorkItemTally;

	// This is set when the TOP node's work items are processed by
	// FHoudiniPDGManager based on if any NotLoaded work result objects are found
	UPROPERTY(NonTransactional)
	bool bCachedHaveNotLoadedWorkResults;

	// This is set when the TOP node's work items are processed by
	// FHoudiniPDGManager based on if any Loaded work result objects are found
	UPROPERTY(NonTransactional)
	bool bCachedHaveLoadedWorkResults;

	// True if this node has child nodes
	UPROPERTY(NonTransactional)
	bool bHasChildNodes;

protected:
	// Visible in the level
	UPROPERTY()
	bool					bShow;

	// Map of [work_result_index]_[work_result_object_index] to the work result object's baked outputs. 
	UPROPERTY()
	TMap<FString, FHoudiniPDGWorkResultObjectBakedOutput> BakedWorkResultObjectOutputs;

private:
	UPROPERTY()
	FOutputActorOwner OutputActorOwner;
};


UCLASS()
class HOUDINIENGINERUNTIME_API UTOPNetwork : public UObject
{
	GENERATED_BODY()

public:

	// Constructor
	UTOPNetwork();

	// Comparison operator, used by hashing containers and arrays.
	bool operator==(const UTOPNetwork& Other) const;

	// Sets all WorkResultObjects that are in the Loaded state to ToDelete (will delete output objects and output
	// actors).
	void SetLoadedWorkResultsToDelete();

	// Immediately delete Loaded work result output objects (keeps the work items and result arrays but deletes the output
	// objects and actors and sets the state to Deleted.
	void DeleteWorkResultOutputObjects();

	// Returns true if any node in this TOP net has pending (waiting, scheduled, cooking) work items.
	bool AnyWorkItemsPending() const;

public:

	UPROPERTY(Transient, NonTransactional)
	int32				NodeId;
	UPROPERTY(NonTransactional)
	FString				NodeName;
	// HAPI path to this node (relative to the HDA)
	UPROPERTY(NonTransactional)
	FString				NodePath;

	UPROPERTY()
	TArray<UTOPNode*>	AllTOPNodes;

	// TODO: Should be using SelectedNodeName instead?
	// Index is not consistent after updating filter
	UPROPERTY()
	int32				SelectedTOPIndex;
	
	UPROPERTY(NonTransactional)
	FString 			ParentName;

	UPROPERTY()
	bool				bShowResults;
	UPROPERTY()
	bool				bAutoLoadResults;
};


class UHoudiniPDGAssetLink;
DECLARE_MULTICAST_DELEGATE_FourParams(FHoudiniPDGAssetLinkWorkResultObjectLoaded, UHoudiniPDGAssetLink*, UTOPNode*, int32 /*WorkItemId*/, const FString& /*WorkResultObjectName*/);

UCLASS()
class HOUDINIENGINERUNTIME_API UHoudiniPDGAssetLink : public UObject
{
	GENERATED_UCLASS_BODY()

public:

	friend class UHoudiniAssetComponent;
	
	static FString GetAssetLinkStatus(const EPDGLinkState& InLinkState);
	static FString GetTOPNodeStatus(const UTOPNode* InTOPNode);
	static FLinearColor GetTOPNodeStatusColor(const UTOPNode* InTOPNode);

	void UpdateTOPNodeWithChildrenWorkItemTallyAndState(UTOPNode* InNode, UTOPNetwork* InNetwork);
	void UpdateWorkItemTally();
	static void ResetTOPNetworkWorkItemTally(UTOPNetwork* TOPNetwork);

	// Set the TOP network at the given index as currently selected TOP network
	void SelectTOPNetwork(const int32& AtIndex);
	// Set the TOP node at the given index in the given TOP network as currently selected TOP node
	void SelectTOPNode(UTOPNetwork* InTOPNetwork, const int32& AtIndex);

	UTOPNode* GetSelectedTOPNode();
	UTOPNetwork* GetSelectedTOPNetwork();
	
	FString GetSelectedTOPNodeName();
	FString GetSelectedTOPNetworkName();

	UTOPNode* GetTOPNode(const int32& InNodeID);
	UTOPNetwork* GetTOPNetwork(const int32& AtIndex);

	// Find the node with relative path 'InNodePath' from its topnet.
	static UTOPNode* GetTOPNodeByNodePath(const FString& InNodePath, const TArray<UTOPNode*>& InTOPNodes, int32& OutIndex);
	// Find the network with relative path 'InNetPath' from the HDA
	static UTOPNetwork* GetTOPNetworkByNodePath(const FString& InNodePath, const TArray<UTOPNetwork*>& InTOPNetworks, int32& OutIndex);

	// Get the parent TOP node of the specified node. This is resolved 
	UTOPNode* GetParentTOPNode(const UTOPNode* InNode);

	static void ClearTOPNodeWorkItemResults(UTOPNode* TOPNode);
	static void ClearTOPNetworkWorkItemResults(UTOPNetwork* TOPNetwork);
	// Clear the result objects of a work item (FTOPWorkResult.ResultObjects), but don't delete the work item from
	// TOPNode.WorkResults (for example, the work item was dirtied but not removed from PDG)
	static void ClearWorkItemResultByID(const int32& InWorkItemID, UTOPNode* InTOPNode);
	// Calls ClearWorkItemResultByID and then deletes the FTOPWorkResult from InTOPNode.Result as well. For example:
	// the work item was removed in PDG.
	static void DestroyWorkItemByID(const int32& InWorkItemID, UTOPNode* InTOPNode);
	static FTOPWorkResult* GetWorkResultByID(const int32& InWorkItemID, UTOPNode* InTOPNode);

	// This should be called after the owner and this PDG asset link is duplicated. Set all output parent actors to
	// null in all TOP networks/nodes. Since the TOP Networks/TOP nodes are all structs, we cannot set
	// DuplicateTransient property on their OutputActor properties.
	void UpdatePostDuplicate();

	// Load the geometry generated as results of the given work item, of the given TOP node.
	// The load will be done asynchronously.
	// Results must be tagged with 'file', and must have a file path, otherwise will not be loaded.
	//void LoadResults(FTOPNode TOPNode, HAPI_PDG_WorkitemInfo workItemInfo, HAPI_PDG_WorkitemResultInfo[] resultInfos, HAPI_PDG_WorkitemId workItemID)

	// Gets the temporary cook folder. If the parent of this asset link is a HoudiniAssetComponent use that, otherwise
	// use the default static mesh temporary cook folder.
	FDirectoryPath GetTemporaryCookFolder() const;

	// Get the actor that owns this PDG asset link. If the asset link is owned by a component,
	// then the component's owning actor is returned. Can return null if this is now owned by
	// an actor or component.
	AActor* GetOwnerActor() const;

	// Checks if the asset link has any temporary outputs and returns true if it has
	bool HasTemporaryOutputs() const;

	// Filter TOP nodes and outputs (hidden/visible) by TOPNodeFilter and TOPOutputFilter.
	void FilterTOPNodesAndOutputs();

	// On all FTOPNodes: Load not loaded items if bAutoload is true, and update the level visibility of work items
	// result. Used when FTOPNode.bShow and/or FTOPNode.bAutoload changed.
	void UpdateTOPNodeAutoloadAndVisibility();
	
#if WITH_EDITORONLY_DATA
	void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
#endif

#if WITH_EDITORONLY_DATA
	void PostTransacted(const FTransactionObjectEvent& TransactionEvent) override;
#endif

private:

	void ClearAllTOPData();
	
	static void DestroyWorkItemResultData(FTOPWorkResult& Result, UTOPNode* InTOPNode);

	static void DestoryWorkResultObjectData(FTOPWorkResultObject& ResultObject);

public:

	//UPROPERTY()
	//UHoudiniAsset*				HoudiniAsset;

	//UPROPERTY()
	//UHoudiniAssetComponent*		ParentHAC;

	UPROPERTY(DuplicateTransient, NonTransactional)
	FString						AssetName;

	// The full path to the HDA in HAPI
	UPROPERTY(DuplicateTransient, NonTransactional)
	FString						AssetNodePath;

	UPROPERTY(DuplicateTransient, NonTransactional)
	int32						AssetID;

	UPROPERTY()
	TArray<UTOPNetwork*>		AllTOPNetworks;

	UPROPERTY()
	int32						SelectedTOPNetworkIndex;

	UPROPERTY(Transient, NonTransactional)
	EPDGLinkState				LinkState;

	UPROPERTY()
	bool						bAutoCook;
	UPROPERTY()
	bool						bUseTOPNodeFilter;
	UPROPERTY()
	bool						bUseTOPOutputFilter;
	UPROPERTY()
	FString						TOPNodeFilter;
	UPROPERTY()
	FString						TOPOutputFilter;

	UPROPERTY(NonTransactional)
	int32						NumWorkitems;
	UPROPERTY(Transient, NonTransactional)
	FWorkItemTally				WorkItemTally;

	UPROPERTY()
	FString						OutputCachePath;

	UPROPERTY(Transient)
	bool						bNeedsUIRefresh;

	// A parent actor to serve as the parent of any output actors
	// that are created.
	// If null, then output actors are created under a folder
	UPROPERTY(EditAnywhere, Category="Output")
	AActor*					 	OutputParentActor;

	// Folder used for baking PDG outputs
	UPROPERTY()
	FDirectoryPath BakeFolder;

	//
	// Notifications
	//

	// Delegate that is broadcast when a work result object has been loaded
	FHoudiniPDGAssetLinkWorkResultObjectLoaded OnWorkResultObjectLoaded;

	//
	// End: Notifications
	//

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	bool bBakeMenuExpanded;

	// What kind of output to bake, for example, bake actors, bake to blueprint
	UPROPERTY()
	EHoudiniEngineBakeOption HoudiniEngineBakeOption;

	// Which outputs to bake, for example, all, selected network, selected node
	UPROPERTY()
	EPDGBakeSelectionOption PDGBakeSelectionOption;

	// This determines if the baked assets should replace existing assets with the same name,
	// or always generate new assets (with numerical suffixes if needed to create unique names)
	UPROPERTY()
	EPDGBakePackageReplaceModeOption PDGBakePackageReplaceMode;

	// If true, recenter baked actors to their bounding box center after bake
	UPROPERTY()
	bool bRecenterBakedActors;

	// Auto-bake: if this is true, it indicates that a work result object should be baked after it is loaded.
	UPROPERTY()
	bool bBakeAfterWorkResultObjectLoaded;

	// The delegate handle of the auto bake helper function bound to OnWorkResultObjectLoaded.
	FDelegateHandle AutoBakeDelegateHandle;
#endif
};
