
#include "HoudiniEngineOutputStats.h"

FHoudiniEngineOutputStats::FHoudiniEngineOutputStats()
	: NumPackagesCreated(0)
	, NumPackagesUpdated(0)
{ }

void FHoudiniEngineOutputStats::NotifyPackageCreated(int32 NumCreated)
{
	NumPackagesCreated += NumCreated;
}

void FHoudiniEngineOutputStats::NotifyPackageUpdated(int32 NumUpdated)
{
	NumPackagesUpdated += NumUpdated;
}

void FHoudiniEngineOutputStats::NotifyObjectsCreated(const FString& ObjectTypeName, int32 NumCreated)
{
	const int32 Count = OutputObjectsCreated.FindOrAdd(ObjectTypeName, 0);
	OutputObjectsCreated[ObjectTypeName] = Count + NumCreated;
}

void FHoudiniEngineOutputStats::NotifyObjectsUpdated(const FString& ObjectTypeName, int32 NumUpdated)
{
	const int32 Count = OutputObjectsUpdated.FindOrAdd(ObjectTypeName, 0);
	OutputObjectsUpdated[ObjectTypeName] = Count + NumUpdated;
}

void FHoudiniEngineOutputStats::NotifyObjectsReplaced(const FString& ObjectTypeName, int32 NumReplaced)
{
	const int32 Count = OutputObjectsReplaced.FindOrAdd(ObjectTypeName, 0);
	OutputObjectsReplaced[ObjectTypeName] = Count + NumReplaced;
}