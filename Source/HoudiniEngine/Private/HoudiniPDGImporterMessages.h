#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"

#include "HAPI/HAPI_Common.h"

#include "HoudiniPackageParams.h"
#include "HoudiniGeoPartObject.h"
#include "HoudiniOutput.h"
#include "HoudiniGenericAttribute.h"
#include "HoudiniInstanceTranslator.h"

#include "HoudiniPDGImporterMessages.generated.h"

// Message used to find/discover running commandlets
USTRUCT()
struct HOUDINIENGINE_API FHoudiniPDGImportBGEODiscoverMessage
{
public:
	GENERATED_BODY();

	FHoudiniPDGImportBGEODiscoverMessage();

	FHoudiniPDGImportBGEODiscoverMessage(const FGuid& InCommandletGuid);
	
	// The GUID of the commandlet we are looking for
	UPROPERTY()
	FGuid CommandletGuid;
};

USTRUCT()
struct HOUDINIENGINE_API FHoudiniPDGImportBGEOMessage
{
public:
	GENERATED_BODY();

	FHoudiniPDGImportBGEOMessage();

	FHoudiniPDGImportBGEOMessage(const FString& InFilePath, const FString& InName, const FHoudiniPackageParams& InPackageParams);

	FHoudiniPDGImportBGEOMessage(
		const FString& InFilePath, 
		const FString& InName, 
		const FHoudiniPackageParams& InPackageParams,
		HAPI_NodeId InTOPNodeId,
		HAPI_PDG_WorkitemId InWorkItemId);

	void PopulateFromPackageParams(const FHoudiniPackageParams& InPackageParams);

	void PopulatePackageParams(FHoudiniPackageParams &OutPackageParams) const;

	// BGEO file path
	UPROPERTY()
	FString FilePath;

	// PDG work item name
	UPROPERTY()
	FString Name;

	// TOP/PDG info
	// TOP node ID
	UPROPERTY()
	// HAPI_NodeId TOPNodeId;
	int32 TOPNodeId;

	// Work item id
	UPROPERTY()
	// HAPI_PDG_WorkitemId WorkItemId;
	int32 WorkItemId;

	// Package params
	// The current cook/baking mode
	UPROPERTY()
	EPackageMode PackageMode;
	// How to handle existing assets? replace or rename?
	UPROPERTY()
	EPackageReplaceMode ReplaceMode;

	// When cooking in bake mode - folder to create assets in
	UPROPERTY()
	FString BakeFolder;
	// When cooking in temp mode - folder to create assets in
	UPROPERTY()
	FString TempCookFolder;

	// Package to save to
	//UPROPERTY()
	//UObject* OuterPackage;

	// Name of the package we want to create
	// If null, we'll generate one from:
	// (without PDG) ASSET_OBJ_GEO_PART_SPLIT,
	// (with PDG) ASSET_TOPNET_TOPNODE_WORKITEMINDEX_PART_SPLIT
	UPROPERTY()
	FString ObjectName;

	// Name of the HDA
	UPROPERTY()
	FString HoudiniAssetName;

	// Name of actor that is managing an instance of the HDA
	UPROPERTY()
	FString HoudiniAssetActorName;

	//
	UPROPERTY()
	int32	ObjectId;
	//
	UPROPERTY()
	int32	GeoId;
	//
	UPROPERTY()
	int32	PartId;
	//
	UPROPERTY()
	FString SplitStr;

	// GUID used for the owner
	UPROPERTY()
	FGuid ComponentGUID;

	// For PDG temporary outputs: the TOP network name
	UPROPERTY()
	FString PDGTOPNetworkName;
	// For PDG temporary outputs: the TOP node name
	UPROPERTY()
	FString PDGTOPNodeName;
	// For PDG temporary outputs: the work item index of the TOP node
	UPROPERTY()
	int32 PDGWorkItemIndex;
};


UENUM()
enum class EHoudiniPDGImportBGEOResult : uint8
{
	// Create uassets from the bgeo completely failed.
	HPIBR_Failed UMETA(DisplayName="Failed"),

	// Successfully created uassets for all content in the bgeo
	HPIBR_Success UMETA(DisplayName = "Success"),

	// Some uassets were created, but there were unsupported objects in the bgeo as well
	HPIBR_PartialSuccess UMETA(DisplayName = "Partial Success"),

	HIBPR_MAX
};

USTRUCT()
struct HOUDINIENGINE_API FHoudiniGenericAttributes
{
public:
	GENERATED_BODY()

	FHoudiniGenericAttributes() {};
	FHoudiniGenericAttributes(const TArray<FHoudiniGenericAttribute>& InPropertyAttributes) : PropertyAttributes(InPropertyAttributes) {};
	FHoudiniGenericAttributes(TArray<FHoudiniGenericAttribute>&& InPropertyAttributes) : PropertyAttributes(InPropertyAttributes) {};

	UPROPERTY()
	TArray<FHoudiniGenericAttribute> PropertyAttributes;
};

USTRUCT()
struct HOUDINIENGINE_API FHoudiniPDGImportNodeOutput
{
public:
	GENERATED_BODY();

	UPROPERTY()
	TArray<FHoudiniGeoPartObject> HoudiniGeoPartObjects;

	UPROPERTY()
	TArray<FHoudiniOutputObjectIdentifier> OutputObjectIdentifiers;

	UPROPERTY()		
	TArray<FString> OutputObjectPackagePaths;

	UPROPERTY()
	TArray<FHoudiniGenericAttributes> OutputObjectGenericAttributes;

	UPROPERTY()
	TArray<FHoudiniInstancedOutputPartData> InstancedOutputPartData;
};

USTRUCT()
struct HOUDINIENGINE_API FHoudiniPDGImportBGEOResultMessage : public FHoudiniPDGImportBGEOMessage
{
public:
	GENERATED_BODY();

	FHoudiniPDGImportBGEOResultMessage();

	FHoudiniPDGImportBGEOResultMessage(const FString& InFilePath, const FString& InName, const FHoudiniPackageParams& InPackageParams, const EHoudiniPDGImportBGEOResult& InImportResult);

	void operator=(const FHoudiniPDGImportBGEOMessage& InRHS) { (*static_cast<FHoudiniPDGImportBGEOMessage*>(this)) = InRHS; }

	// Result of the bgeo import -> uassets
	UPROPERTY()
	EHoudiniPDGImportBGEOResult ImportResult;

	UPROPERTY()
	TArray<FHoudiniPDGImportNodeOutput> Outputs;

};
