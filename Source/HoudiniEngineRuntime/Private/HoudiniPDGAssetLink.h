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

//#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"

#include "HoudiniAsset.h"
#include "HoudiniAssetComponent.h"


#include "HoudiniPDGAssetLink.generated.h"

class FHoudiniPackageParams;

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
	GENERATED_USTRUCT_BODY()
	
public:
	FOutputActorOwner();

	virtual ~FOutputActorOwner();

	// Create OutputActor, an actor to hold work result output
	bool CreateOutputActor(UWorld* InWorld, UHoudiniPDGAssetLink* InAssetLink, AActor *InParentActor, const FName& InName);

	// Return OutputActor
	AActor* GetOutputActor() const { return OutputActor; }

	// Setter for OutputActor
	void SetOutputActor(AActor* InActor) { OutputActor = InActor; }
	
	// Destroy OutputActor if it is valid.
	bool DestroyOutputActor();
	
private:
	UPROPERTY()
	AActor* OutputActor;
};

USTRUCT()
struct HOUDINIENGINERUNTIME_API FTOPWorkResultObject : public FOutputActorOwner
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

public:

	UPROPERTY()
	FString					Name;
	UPROPERTY()
	FString					FilePath;
	UPROPERTY()
	EPDGWorkResultState		State;

protected:
	// UPROPERTY()
	// TArray<UObject*>		ResultObjects;

	UPROPERTY()
	TArray<UHoudiniOutput*> ResultOutputs;
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

	UPROPERTY()
	int32							WorkItemIndex;
	UPROPERTY()
	int32							WorkItemID;
	
	UPROPERTY()
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


USTRUCT()
struct HOUDINIENGINERUNTIME_API FTOPNode : public FOutputActorOwner
{
	GENERATED_USTRUCT_BODY()

public:

	// Constructor
	FTOPNode();

	virtual ~FTOPNode() {};

	// Comparison operator, used by hashing containers and arrays.
	bool operator==(const FTOPNode& Other) const;

	void Reset();

	bool AreAllWorkItemsComplete() const { return WorkItemTally.AreAllWorkItemsComplete(); };
	bool AnyWorkItemsFailed() const { return WorkItemTally.AnyWorkItemsFailed(); };
	bool AnyWorkItemsPending() const { return WorkItemTally.AnyWorkItemsPending(); };

	bool IsVisibleInLevel() const { return bShow; }
	void SetVisibleInLevel(bool bInVisible);
	void UpdateOutputVisibilityInLevel();

	// Sets all WorkResultObjects that are in the NotLoaded state to ToLoad.
	void SetNotLoadedWorkResultsToLoad();

public:

	UPROPERTY(Transient)
	int32					NodeId;
	UPROPERTY()
	FString					NodeName;
	UPROPERTY()
	FString					NodePath;
	UPROPERTY()
	FString					ParentName;
	
	UPROPERTY()
	UObject*				WorkResultParent;
	UPROPERTY()
	TArray<FTOPWorkResult>	WorkResult;

	// Hidden in the nodes combobox
	UPROPERTY()
	bool					bHidden;
	UPROPERTY()
	bool					bAutoLoad;

	UPROPERTY(Transient)
	EPDGNodeState 			NodeState;

	UPROPERTY(Transient)
	FWorkItemTally			WorkItemTally;

protected:
	// Visible in the level
	UPROPERTY()
	bool					bShow;
};


USTRUCT()
struct HOUDINIENGINERUNTIME_API FTOPNetwork
{
	GENERATED_USTRUCT_BODY()

public:

	// Constructor
	FTOPNetwork();

	// Comparison operator, used by hashing containers and arrays.
	bool operator==(const FTOPNetwork& Other) const;

public:

	UPROPERTY()
	int32				NodeId;
	UPROPERTY()
	FString				NodeName;
	// HAPI path to this node (relative to the HDA)
	UPROPERTY()
	FString				NodePath;

	UPROPERTY()
	TArray<FTOPNode>	AllTOPNodes;

	// TODO: Should be using SelectedNodeName instead?
	// Index is not consistent after updating filter
	UPROPERTY()
	int32				SelectedTOPIndex;
	
	UPROPERTY()
	FString 			ParentName;

	UPROPERTY()
	bool				bShowResults;
	UPROPERTY()
	bool				bAutoLoadResults;
};


UCLASS()
class HOUDINIENGINERUNTIME_API UHoudiniPDGAssetLink : public UObject
{
	GENERATED_UCLASS_BODY()

public:

	friend class UHoudiniAssetComponent;
	
	static FString GetAssetLinkStatus(const EPDGLinkState& InLinkState);
	static FString GetTOPNodeStatus(const FTOPNode& InTOPNode);
	static FLinearColor GetTOPNodeStatusColor(const FTOPNode& InTOPNode);

	void UpdateWorkItemTally();
	static void ResetTOPNetworkWorkItemTally(FTOPNetwork& TOPNetwork);

	// Set the TOP network at the given index as currently selected TOP network
	void SelectTOPNetwork(const int32& AtIndex);
	// Set the TOP node at the given index in the given TOP network as currently selected TOP node
	void SelectTOPNode(FTOPNetwork& TOPNetwork, const int32& AtIndex);

	FTOPNode* GetSelectedTOPNode();
	FTOPNetwork* GetSelectedTOPNetwork();
	
	FString GetSelectedTOPNodeName();
	FString GetSelectedTOPNetworkName();

	FTOPNode* GetTOPNode(const int32& InNodeID);
	FTOPNetwork* GetTOPNetwork(const int32& AtIndex);
	
	static FTOPNode* GetTOPNodeByName(const FString& InName, TArray<FTOPNode>& InTOPNodes, int32& OutIndex);
	static FTOPNetwork* GetTOPNetworkByName(const FString& InName, TArray<FTOPNetwork>& InTOPNetworks, int32& OutIndex);

	static void ClearTOPNodeWorkItemResults(FTOPNode& TOPNode);
	static void ClearTOPNetworkWorkItemResults(FTOPNetwork& TOPNetwork);
	// Clear the result objects of a work item (FTOPWorkResult.ResultObjects), but don't delete the work item from
	// TOPNode.WorkResults (for example, the work item was dirtied but not removed from PDG)
	static void ClearWorkItemResultByID(const int32& InWorkItemID, FTOPNode& TOPNode);
	// Calls ClearWorkItemResultByID and then deletes the FTOPWorkResult from InTOPNode.Result as well. For example:
	// the work item was removed in PDG.
	static void DestroyWorkItemByID(const int32& InWorkItemID, FTOPNode& InTOPNode);
	static FTOPWorkResult* GetWorkResultByID(const int32& InWorkItemID, FTOPNode& InTOPNode);

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

private:

	void ClearAllTOPData();
	
	static void DestroyWorkItemResultData(FTOPWorkResult& Result, FTOPNode& InTOPNode);

public:

	//UPROPERTY()
	//UHoudiniAsset*				HoudiniAsset;

	//UPROPERTY()
	//UHoudiniAssetComponent*		ParentHAC;

	UPROPERTY(DuplicateTransient)
	FString						AssetName;

	// The full path to the HDA in HAPI
	UPROPERTY(DuplicateTransient)
	FString						AssetNodePath;

	UPROPERTY(DuplicateTransient)
	int32						AssetID;

	UPROPERTY()
	TArray<FTOPNetwork>			AllTOPNetworks;

	UPROPERTY()
	int32						SelectedTOPNetworkIndex;

	UPROPERTY(Transient)
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

	UPROPERTY()
	int32						NumWorkitems;
	UPROPERTY(Transient)
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
#endif
};
