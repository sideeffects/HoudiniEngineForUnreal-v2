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

#include "HoudiniGeoFactory.h"

#include "HoudiniEngineEditorPrivatePCH.h"
#include "HoudiniGeoImporter.h"
#include "HoudiniPackageParams.h"
#include "HoudiniEngineRuntime.h"
#include "HoudiniOutput.h"
#include "HoudiniEngine.h"

#include "Engine/StaticMesh.h"
#include "EditorFramework/AssetImportData.h"
#include "Misc/FileHelper.h"
#include "Internationalization/Internationalization.h"

#define LOCTEXT_NAMESPACE HOUDINI_LOCTEXT_NAMESPACE 

UHoudiniGeoFactory::UHoudiniGeoFactory(const FObjectInitializer & ObjectInitializer)
	: Super(ObjectInitializer)
{
	// This factory is responsible for manufacturing HoudiniEngine assets.
	SupportedClass = nullptr;// UHoudiniAsset::StaticClass();

	// This factory does not manufacture new objects from scratch.
	bCreateNew = false;

	// This factory will not open the editor for each new object.
	bEditAfterNew = false;

	// This factory will import objects from files.
	bEditorImport = true;

	// Factory does not import objects from text.
	bText = false;

	// Add supported formats.
	Formats.Add(TEXT("bgeo;Houdini Geometry"));
	Formats.Add(TEXT("bgeo.sc;Houdini Geometry (compressed)"));
	Formats.Add(TEXT("sc;Houdini Geometry (compressed)"));
}

bool
UHoudiniGeoFactory::FactoryCanImport(const FString& Filename)
{
 	const FString Extension = FPaths::GetExtension(Filename);
	if(FPaths::GetExtension(Filename) == TEXT("bgeo"))
		return true;
	if (FPaths::GetExtension(Filename) == TEXT("bgeo.sc"))
		return true;
	if (FPaths::GetExtension(Filename) == TEXT("sc"))
		return true;

	return false;
}

bool
UHoudiniGeoFactory::DoesSupportClass(UClass * Class)
{
	return Class == UStaticMesh::StaticClass();
	//return (Class == UStaticMesh::StaticClass() || Class == UGeometryCache::StaticClass() 
	//	|| Class == USkeletalMesh::StaticClass() || Class == UAnimSequence::StaticClass());
}

UClass* 
UHoudiniGeoFactory::ResolveSupportedClass()
{
	return UStaticMesh::StaticClass();
}

FText
UHoudiniGeoFactory::GetDisplayName() const
{
	return LOCTEXT("HoudiniGeoFactoryDescription", "Houdini Engine Geo");
}

UObject*
UHoudiniGeoFactory::FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, const FString& Filename, const TCHAR* Parms, FFeedbackContext* Warn, bool& bOutOperationCanceled)
{
	// Make sure we're loading bgeo / bgeo.sc files
	FString FileExtension = FPaths::GetExtension(Filename);
	if (!FileExtension.Contains(TEXT("bgeo"), ESearchCase::IgnoreCase) && !FileExtension.Contains(TEXT("sc"), ESearchCase::IgnoreCase))
		return Super::FactoryCreateFile(InClass, InParent, InName, Flags, Filename, Parms, Warn, bOutOperationCanceled);

	//
	// TODO:
	// Handle import settings here?
	// 

	// Broadcast PreImport
	GEditor->GetEditorSubsystem<UImportSubsystem>()->BroadcastAssetPreImport(this, InClass, InParent, InName, TEXT("Houdini GEO"));

	// Create a new Geo importer
	TArray<UHoudiniOutput*> NewOutputs;
	UHoudiniGeoImporter* BGEOImporter = NewObject<UHoudiniGeoImporter>(this);
	BGEOImporter->AddToRoot();

	// Clean up lambda
	auto CleanUp = [&NewOutputs, &BGEOImporter]()
	{
		// Remove the importer and output objects from the root set
		BGEOImporter->RemoveFromRoot();
		for (auto Out : NewOutputs)
			Out->RemoveFromRoot();
	};

	// Failure lambda
	auto FailImportAndReturnNull = [this, &CleanUp, &NewOutputs, &BGEOImporter]()
	{
		CleanUp();

		// Failed to read the file info, fail the import
		GEditor->GetEditorSubsystem<UImportSubsystem>()->BroadcastAssetPostImport(this, nullptr);

		// Notifiy we failed to load the bgeo
		FString Notification = TEXT("BGEO Importer: Failed to load the BGEO file.");
		FHoudiniEngine::Get().UpdateTaskSlateNotification(FText::FromString(Notification));

		return nullptr;
	};

	if (Filename.IsEmpty())
		return FailImportAndReturnNull();

	// 1. Houdini Engine Session
	// See if we should/can start the default "first" HE session
	if (!BGEOImporter->AutoStartHoudiniEngineSessionIfNeeded())
		return FailImportAndReturnNull();

	// 2. Update the file paths
	if (!BGEOImporter->SetFilePath(Filename))
		return FailImportAndReturnNull();

	// 3. Load the BGEO file in HAPI
	HAPI_NodeId NodeId;
	if (!BGEOImporter->LoadBGEOFileInHAPI(NodeId))
		return FailImportAndReturnNull();

	// Prepare the package used for creating the mesh, landscape and instancer pacakges
	FHoudiniPackageParams PackageParams;
	PackageParams.PackageMode = EPackageMode::Bake;
	PackageParams.ReplaceMode = EPackageReplaceMode::CreateNewAssets;

	PackageParams.BakeFolder = FPackageName::GetLongPackagePath(InParent->GetOutermost()->GetName());
	PackageParams.TempCookFolder = FHoudiniEngineRuntime::Get().GetDefaultTemporaryCookFolder();

	PackageParams.OuterPackage = InParent;
	PackageParams.HoudiniAssetName = FString();
	PackageParams.ObjectName = FPaths::GetBaseFilename(InParent->GetName());

	// 4. Get the output from the file node
	bool bUseWorldComposition = false;
	if (!BGEOImporter->BuildOutputsForNode(NodeId, NewOutputs, bUseWorldComposition))
		return FailImportAndReturnNull();

	// 5. Create the static meshes in the outputs
	if (!BGEOImporter->CreateStaticMeshes(NewOutputs, InParent, PackageParams))
		return FailImportAndReturnNull();

	// 6. Create the landscape in the outputs
	if (!BGEOImporter->CreateLandscapes(NewOutputs, InParent, PackageParams))
		return FailImportAndReturnNull();

	// 7. Create the instancers in the outputs
	if (!BGEOImporter->CreateInstancers(NewOutputs, InParent, PackageParams))
		return FailImportAndReturnNull();

	// 8. Delete the created  node in Houdini
	if (!BGEOImporter->DeleteCreatedNode(NodeId))
	{
		// Not good, but not fatal..
		//return false;
	}

	// Notifiy we're done loading the bgeo
	FString Notification = TEXT("BGEO Importer: BGEO file imported succesfully.");
	FHoudiniEngine::Get().UpdateTaskSlateNotification(FText::FromString(Notification));

	// Get our result object and "finalize" them
	TArray<UObject*> Results = BGEOImporter->GetOutputObjects();
	for (UObject* Object : Results)
	{
		if (!Object || Object->IsPendingKill())
			continue;

		Object->SetFlags(Flags);

		GEditor->GetEditorSubsystem<UImportSubsystem>()->BroadcastAssetPostImport(this, Object);
		Object->MarkPackageDirty();
		Object->PostEditChange();
	}

	CleanUp();

	// Determine out parent according to the generated assets outer
	UObject* OutParent = (Results.Num() > 0 && InParent != Results[0]->GetOutermost()) ? Results[0]->GetOutermost() : InParent;
	return (Results.Num() > 0) ? OutParent : nullptr;
}

bool
UHoudiniGeoFactory::CanReimport(UObject * Obj, TArray<FString>& OutFilenames)
{
	UAssetImportData* ImportData = nullptr;
	if (Obj->GetClass() == UStaticMesh::StaticClass())
	{
		UStaticMesh* Mesh = Cast<UStaticMesh>(Obj);
		ImportData = Mesh->AssetImportData;
	}
	/*else if (Obj->GetClass() == UGeometryCache::StaticClass())
	{
		UGeometryCache* Cache = Cast<UGeometryCache>(Obj);
		ImportData = Cache->AssetImportData;
	}
	else if (Obj->GetClass() == USkeletalMesh::StaticClass())
	{
		USkeletalMesh* Cache = Cast<USkeletalMesh>(Obj);
		ImportData = Cache->AssetImportData;
	}
	else if (Obj->GetClass() == UAnimSequence::StaticClass())
	{
		UAnimSequence* Cache = Cast<UAnimSequence>(Obj);
		ImportData = Cache->AssetImportData;
	}*/

	if (ImportData)
	{
		/*
		if (FPaths::GetExtension(ImportData->GetFirstFilename()).Contains(TEXT("abc"))
			|| (Obj->GetClass() == UAnimSequence::StaticClass() && ImportData->GetFirstFilename().IsEmpty()))
			*/

		if (FPaths::GetExtension(ImportData->GetFirstFilename()).Contains(TEXT("bgeo")))
		{
			ImportData->ExtractFilenames(OutFilenames);
			return true;
		}
	}
	return false;
}

void
UHoudiniGeoFactory::SetReimportPaths(UObject * Obj, const TArray<FString>& NewReimportPaths)
{
	UStaticMesh* Mesh = Cast<UStaticMesh>(Obj);
	if (Mesh && Mesh->AssetImportData && ensure(NewReimportPaths.Num() == 1))
	{
		Mesh->AssetImportData->UpdateFilenameOnly(NewReimportPaths[0]);
	}
	/*
	USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(Obj);
	if (SkeletalMesh && SkeletalMesh->AssetImportData && ensure(NewReimportPaths.Num() == 1))
	{
		SkeletalMesh->AssetImportData->UpdateFilenameOnly(NewReimportPaths[0]);
	}

	UAnimSequence* Sequence = Cast<UAnimSequence>(Obj);
	if (Sequence && Sequence->AssetImportData && ensure(NewReimportPaths.Num() == 1))
	{
		Sequence->AssetImportData->UpdateFilenameOnly(NewReimportPaths[0]);
	}

	UGeometryCache* GeometryCache = Cast<UGeometryCache>(Obj);
	if (GeometryCache && GeometryCache->AssetImportData && ensure(NewReimportPaths.Num() == 1))
	{
		GeometryCache->AssetImportData->UpdateFilenameOnly(NewReimportPaths[0]);
	}
	*/
}

EReimportResult::Type
UHoudiniGeoFactory::Reimport(UObject * Obj)
{
	HOUDINI_LOG_MESSAGE(TEXT("Houdini Asset reimport has failed."));
	return EReimportResult::Failed;
}

int32 
UHoudiniGeoFactory::GetPriority() const
{
	return ImportPriority;
}


#undef LOCTEXT_NAMESPACE