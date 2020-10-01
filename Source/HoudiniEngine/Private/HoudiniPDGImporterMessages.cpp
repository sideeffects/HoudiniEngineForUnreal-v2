#include "HoudiniPDGImporterMessages.h"


FHoudiniPDGImportBGEOMessage::FHoudiniPDGImportBGEOMessage()
{

}

FHoudiniPDGImportBGEOMessage::FHoudiniPDGImportBGEOMessage(
	const FString& InFilePath, 
	const FString& InName, 
	const FHoudiniPackageParams& InPackageParams
)
	: FilePath(InFilePath)
	, Name(InName)
{
	PopulateFromPackageParams(InPackageParams);
}

FHoudiniPDGImportBGEOMessage::FHoudiniPDGImportBGEOMessage(
	const FString& InFilePath,
	const FString& InName,
	const FHoudiniPackageParams& InPackageParams,
	HAPI_NodeId InTOPNodeId,
	HAPI_PDG_WorkitemId InWorkItemId
)
	: FilePath(InFilePath)
	, Name(InName)
	, TOPNodeId(InTOPNodeId)
	, WorkItemId(InWorkItemId)
{
	PopulateFromPackageParams(InPackageParams);
}

void FHoudiniPDGImportBGEOMessage::PopulateFromPackageParams(const FHoudiniPackageParams& InPackageParams)
{
	PackageMode = InPackageParams.PackageMode;
	ReplaceMode = InPackageParams.ReplaceMode;

	BakeFolder = InPackageParams.BakeFolder;
	TempCookFolder = InPackageParams.TempCookFolder;

	//OuterPackage = InPackageParams.OuterPackage;

	ObjectName = InPackageParams.ObjectName;

	HoudiniAssetName = InPackageParams.HoudiniAssetName;

	HoudiniAssetActorName = InPackageParams.HoudiniAssetActorName;

	ObjectId = InPackageParams.ObjectId;
	GeoId = InPackageParams.GeoId;
	PartId = InPackageParams.PartId;
	SplitStr = InPackageParams.SplitStr;

	ComponentGUID = InPackageParams.ComponentGUID;

	PDGTOPNetworkName = InPackageParams.PDGTOPNetworkName;
	PDGTOPNodeName = InPackageParams.PDGTOPNodeName;
	PDGWorkItemIndex = InPackageParams.PDGWorkItemIndex;
}

void FHoudiniPDGImportBGEOMessage::PopulatePackageParams(FHoudiniPackageParams& OutPackageParams) const
{
	OutPackageParams.PackageMode = PackageMode;
	OutPackageParams.ReplaceMode = ReplaceMode;

	OutPackageParams.BakeFolder = BakeFolder;
	OutPackageParams.TempCookFolder = TempCookFolder;

	//OutPackageParams.OuterPackage = OuterPackage;

	OutPackageParams.ObjectName = ObjectName;

	OutPackageParams.HoudiniAssetName = HoudiniAssetName;

	OutPackageParams.HoudiniAssetActorName = HoudiniAssetActorName;

	OutPackageParams.ObjectId = ObjectId;
	OutPackageParams.GeoId = GeoId;
	OutPackageParams.PartId = PartId;
	OutPackageParams.SplitStr = SplitStr;

	OutPackageParams.ComponentGUID = ComponentGUID;

	OutPackageParams.PDGTOPNetworkName = PDGTOPNetworkName;
	OutPackageParams.PDGTOPNodeName = PDGTOPNodeName;
	OutPackageParams.PDGWorkItemIndex = PDGWorkItemIndex;
}

FHoudiniPDGImportBGEOResultMessage::FHoudiniPDGImportBGEOResultMessage()
{

}

FHoudiniPDGImportBGEOResultMessage::FHoudiniPDGImportBGEOResultMessage(
	const FString& InFilePath, 
	const FString& InName, 
	const FHoudiniPackageParams& InPackageParams, 
	const EHoudiniPDGImportBGEOResult& InImportResult
)
	: FHoudiniPDGImportBGEOMessage(InFilePath, InName, InPackageParams)
	, ImportResult(InImportResult)
{
}

FHoudiniPDGImportBGEODiscoverMessage::FHoudiniPDGImportBGEODiscoverMessage()
	: CommandletGuid()
{
	
}

FHoudiniPDGImportBGEODiscoverMessage::FHoudiniPDGImportBGEODiscoverMessage(const FGuid& InCommandletGuid)
	: CommandletGuid(InCommandletGuid)
{
	
}
