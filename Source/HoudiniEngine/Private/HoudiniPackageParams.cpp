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

#include "HoudiniPackageParams.h"

#include "HoudiniEnginePrivatePCH.h"
#include "HoudiniEngineRuntime.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniStaticMesh.h"
#include "HoudiniStringResolver.h"

#include "PackageTools.h"
#include "ObjectTools.h"
#include "Engine/StaticMesh.h"

//
FHoudiniPackageParams::FHoudiniPackageParams()
{
	PackageMode = EPackageMode::CookToTemp;
	ReplaceMode = EPackageReplaceMode::ReplaceExistingAssets;

	TempCookFolder = FHoudiniEngineRuntime::Get().GetDefaultTemporaryCookFolder();
	BakeFolder = FHoudiniEngineRuntime::Get().GetDefaultBakeFolder();

	OuterPackage = nullptr;
	ObjectName = FString();
	HoudiniAssetName = FString();
	HoudiniAssetActorName = FString();

	ObjectId = 0;
	GeoId = 0;
	PartId = 0;
	SplitStr = 0;

	ComponentGUID.Invalidate();

	PDGTOPNetworkName.Empty();
	PDGTOPNodeName.Empty();
	PDGWorkItemIndex = INDEX_NONE;
}


//
FHoudiniPackageParams::~FHoudiniPackageParams()
{


}


// Returns the object flags corresponding to the current package mode
EObjectFlags 
FHoudiniPackageParams::GetObjectFlags() const 
{
	if (PackageMode == EPackageMode::CookToTemp)
		return RF_Public | RF_Standalone;
	else if (PackageMode == EPackageMode::Bake)
		return RF_Public | RF_Standalone;
	else
		return RF_NoFlags;
}

FString
FHoudiniPackageParams::GetPackageName() const
{
	if (!ObjectName.IsEmpty())
		return ObjectName;

	// If we have PDG infos, generate a name including them
	if (!PDGTOPNetworkName.IsEmpty() && !PDGTOPNodeName.IsEmpty() && PDGWorkItemIndex >= 0)
	{
		return FString::Printf(
			TEXT("%s_%s_%s_%d_%d_%s"),
			*HoudiniAssetName, *PDGTOPNetworkName, *PDGTOPNodeName, PDGWorkItemIndex, PartId, *SplitStr);
	}
	else
	{
		// Generate an object name using the HGPO IDs and the HDA name
		return FString::Printf(TEXT("%s_%d_%d_%d_%s"), *HoudiniAssetName, ObjectId, GeoId, PartId, *SplitStr);
	}
}

FString
FHoudiniPackageParams::GetPackagePath() const
{
	FString PackagePath = FString();
	switch (PackageMode)
	{
		case EPackageMode::CookToLevel:
		{
			// Path to the persistent level
			//PackagePath = FPackageName::GetLongPackagePath(OuterPackage->GetOuter()->GetName());

			// In this mode, we'll use the persistent level as our package's outer
			// simply use the hda + component guid for the path	
			//  Add a subdir for the HDA
			if (!HoudiniAssetName.IsEmpty())
				PackagePath += TEXT("/") + HoudiniAssetName;
			// Add a subdir using the owner component GUID if possible
			if(ComponentGUID.IsValid())
				PackagePath += TEXT("/") + ComponentGUID.ToString().Left(PACKAGE_GUID_LENGTH);

			// TODO: FIX ME!!!
			// Old version
			// Build the package name
			PackagePath = FPackageName::GetLongPackagePath(OuterPackage->GetOuter()->GetName()) +
				TEXT("/") +
				HoudiniAssetName;
		}
		break;

		case EPackageMode::CookToTemp:
		{
			// Temporary Folder
			PackagePath = TempCookFolder;
			//  Add a subdir for the HDA
			if (!HoudiniAssetName.IsEmpty())
				PackagePath += TEXT("/") + HoudiniAssetName;
			// Add a subdir using the owner component GUID if possible
			if (ComponentGUID.IsValid())
				PackagePath += TEXT("/") + ComponentGUID.ToString().Left(PACKAGE_GUID_LENGTH);
		}
		break;

		case EPackageMode::Bake:
		{
			PackagePath = BakeFolder;
		}
		break;
	}

	return PackagePath;
}

UPackage*
FHoudiniPackageParams::CreatePackageForObject(FString& OutPackageName) const
{
	// GUID/counter used to differentiate with existing package
	int32 BakeCounter = 0;
	FGuid CurrentGuid = FGuid::NewGuid();

	// Get the appropriate package path/name for this object
	FString PackageName = GetPackageName();
	FString PackagePath = GetPackagePath();
	   
	// Iterate until we find a suitable name for the package
	UPackage * NewPackage = nullptr;
	while (true)
	{
		OutPackageName = PackageName;

		// Append the Bake guid/counter to the object name if needed
		if (BakeCounter > 0)
		{
			OutPackageName += (PackageMode == EPackageMode::Bake)
				? TEXT("_") + FString::FromInt(BakeCounter)
				: CurrentGuid.ToString().Left(PACKAGE_GUID_LENGTH);
		}

		// Build the final package name
		FString FinalPackageName = PackagePath + TEXT("/") + OutPackageName;
		// Sanitize package name.
		FinalPackageName = UPackageTools::SanitizePackageName(FinalPackageName);

		UObject * PackageOuter = nullptr;
		if (PackageMode == EPackageMode::CookToLevel)
		{
			// If we are not baking, then use outermost package, since objects within our package 
			// need to be visible to external operations, such as copy paste.
			PackageOuter = OuterPackage;
		}

		// See if a package named similarly already exists
		UPackage* FoundPackage = FindPackage(PackageOuter, *FinalPackageName);
		if (ReplaceMode == EPackageReplaceMode::CreateNewAssets
			&& FoundPackage && !FoundPackage->IsPendingKill())
		{
			// we need to generate a new name for it
			CurrentGuid = FGuid::NewGuid();
			BakeCounter++;
			continue;
		}

		// Create actual package.
		NewPackage = CreatePackage(PackageOuter, *FinalPackageName);
		break;
	}

	return NewPackage;
}


// Fixes link error with the template function under
void TemplateFixer()
{
	FHoudiniPackageParams PP;
	UStaticMesh* SM = PP.CreateObjectAndPackage<UStaticMesh>();
	UHoudiniStaticMesh* HSM = PP.CreateObjectAndPackage<UHoudiniStaticMesh>();
	//UMaterial* Mat = PP.CreateObjectAndPackage<UMaterial>();
	//UTexture2D* Text = PP.CreateObjectAndPackage<UTexture2D>();
}

template<typename T>
T* FHoudiniPackageParams::CreateObjectAndPackage()
{
	// Create the package for the object
	FString NewObjectName;
	UPackage* Package = CreatePackageForObject(NewObjectName);
	if (!Package || Package->IsPendingKill())
		return nullptr;

	const FString SanitizedObjectName = ObjectTools::SanitizeObjectName(NewObjectName);

	T* ExistingTypedObject = FindObject<T>(Package, *NewObjectName);
	UObject* ExistingObject = FindObject<UObject>(Package, *NewObjectName);

	if (ExistingTypedObject != nullptr && !ExistingTypedObject->IsPendingKill())
	{
		// An object of the appropriate type already exists, update it!
		ExistingTypedObject->PreEditChange(nullptr);
	}
	else if (ExistingObject != nullptr)
	{
		// Replacing an object of a different type, Delete it first.
		const bool bDeleteSucceeded = ObjectTools::DeleteSingleObject(ExistingObject);
		if (bDeleteSucceeded)
		{
			// Force GC so we can cleanly create a new asset (and not do an 'in place' replacement)
			CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);

			// Create a package for each mesh
			Package = CreatePackageForObject(NewObjectName);
			if (!Package || Package->IsPendingKill())
				return nullptr;
		}
		else
		{
			// failed to delete
			return nullptr;
		}
	}

	// Add meta information to this package.
	FHoudiniEngineUtils::AddHoudiniMetaInformationToPackage(
		Package, Package, HAPI_UNREAL_PACKAGE_META_GENERATED_OBJECT, TEXT("true"));
	FHoudiniEngineUtils::AddHoudiniMetaInformationToPackage(
		Package, Package, HAPI_UNREAL_PACKAGE_META_GENERATED_NAME, *NewObjectName);

	return NewObject<T>(Package, FName(*SanitizedObjectName), GetObjectFlags());
}

