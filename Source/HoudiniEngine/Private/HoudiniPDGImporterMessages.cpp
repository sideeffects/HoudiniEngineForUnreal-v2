#include "HoudiniPDGImporterMessages.h"


FHoudiniPDGImportBGEOMessage::FHoudiniPDGImportBGEOMessage()
	: FilePath()
	, Name()
	, TOPNodeId(-1)
	, WorkItemId(-1)
{

}

FHoudiniPDGImportBGEOMessage::FHoudiniPDGImportBGEOMessage(
	const FString& InFilePath, 
	const FString& InName, 
	const FHoudiniPackageParams& InPackageParams
)
	: FilePath(InFilePath)
	, Name(InName)
	, TOPNodeId(-1)
	, WorkItemId(-1)
{
	SetPackageParams(InPackageParams);
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
	SetPackageParams(InPackageParams);
}

void FHoudiniPDGImportBGEOMessage::SetPackageParams(const FHoudiniPackageParams& InPackageParams)
{
	PackageParams = InPackageParams;
	PackageParams.OuterPackage = nullptr;
}

void FHoudiniPDGImportBGEOMessage::PopulatePackageParams(FHoudiniPackageParams& OutPackageParams) const
{
	UObject* KeepOuter = OutPackageParams.OuterPackage;
	OutPackageParams = PackageParams;
	OutPackageParams.OuterPackage = KeepOuter;
}

FHoudiniPDGImportBGEOResultMessage::FHoudiniPDGImportBGEOResultMessage()
	: ImportResult(EHoudiniPDGImportBGEOResult::HPIBR_Failed)
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
