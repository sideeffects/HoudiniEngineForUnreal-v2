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

#include "HoudiniEngineBakeUtils.h"

#include "HoudiniEngineEditorPrivatePCH.h"

#include "HoudiniEngineUtils.h"
#include "HoudiniAssetActor.h"
#include "HoudiniAsset.h"
#include "HoudiniAssetComponent.h"
#include "HoudiniOutput.h"
#include "HoudiniSplineComponent.h"
#include "HoudiniGeoPartObject.h"
#include "HoudiniPackageParams.h"
#include "HoudiniEnginePrivatePCH.h"
#include "HoudiniRuntimeSettings.h"
#include "HoudiniEngineUtils.h"
#include "UnrealLandscapeTranslator.h"
#include "HoudiniInstanceTranslator.h"
#include "HoudiniInstancedActorComponent.h"
#include "HoudiniMeshSplitInstancerComponent.h"
#include "HoudiniPDGAssetLink.h"
#include "HoudiniStringResolver.h"
#include "HoudiniEngineCommands.h"

#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "RawMesh.h"
#include "UObject/Package.h"
#include "PackageTools.h"
#include "UObject/MetaData.h"
#include "AssetRegistryModule.h"
#include "Materials/Material.h"
#include "LandscapeProxy.h"
#include "LandscapeStreamingProxy.h"
#include "LandscapeInfo.h"
#include "Factories/WorldFactory.h"
#include "AssetToolsModule.h"
#include "InstancedFoliageActor.h"
#include "Components/SplineComponent.h"
#include "GameFramework/Actor.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "PhysicsEngine/BodySetup.h"
#include "ActorFactories/ActorFactoryStaticMesh.h"
#include "ActorFactories/ActorFactoryEmptyActor.h"
#include "BusyCursor.h"
#include "Editor.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "FileHelpers.h"
#include "HoudiniEngineEditor.h"
#include "HoudiniEngine.h"
#include "HoudiniLandscapeTranslator.h"
#include "HoudiniOutputTranslator.h"
#include "Editor/EditorEngine.h"
#include "Factories/BlueprintFactory.h"
#include "Engine/SimpleConstructionScript.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "LandscapeEdit.h"
#include "Containers/UnrealString.h"
#include "Components/AudioComponent.h"
#include "Engine/WorldComposition.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "MaterialEditor/Public/MaterialEditingLibrary.h"
#include "MaterialGraph/MaterialGraph.h"
#include "Particles/ParticleSystemComponent.h"
#include "Sound/SoundBase.h"
#include "UObject/UnrealType.h"

#define LOCTEXT_NAMESPACE HOUDINI_LOCTEXT_NAMESPACE

FHoudiniEngineBakedActor::FHoudiniEngineBakedActor()
	: Actor(nullptr)
	, OutputIndex(INDEX_NONE)
	, OutputObjectIdentifier()
	, ActorBakeName(NAME_None)
	, BakedObject(nullptr)
	, SourceObject(nullptr)
{
}

FHoudiniEngineBakedActor::FHoudiniEngineBakedActor(
	AActor* InActor,
	FName InActorBakeName,
	FName InWorldOutlinerFolder,
	int32 InOutputIndex,
	const FHoudiniOutputObjectIdentifier& InOutputObjectIdentifier,
	UObject* InBakedObject,
	UObject* InSourceObject)
	: Actor(InActor)
	, OutputIndex(InOutputIndex)
	, OutputObjectIdentifier(InOutputObjectIdentifier)
	, ActorBakeName(InActorBakeName)
	, WorldOutlinerFolder(InWorldOutlinerFolder)
	, BakedObject(InBakedObject)
	, SourceObject(InSourceObject)
{
}

bool
FHoudiniEngineBakeUtils::BakeHoudiniAssetComponent(
	UHoudiniAssetComponent* InHACToBake,
	bool bInReplacePreviousBake,
	EHoudiniEngineBakeOption InBakeOption,
	bool bInRemoveHACOutputOnSuccess)
{
	if (!IsValid(InHACToBake))
		return false;

	// Handle proxies: if the output has any current proxies, first refine them
	bool bHACNeedsToReCook;
	if (!CheckForAndRefineHoudiniProxyMesh(InHACToBake, bInReplacePreviousBake, InBakeOption, bInRemoveHACOutputOnSuccess, bHACNeedsToReCook))
	{
		// Either the component is invalid, or needs a recook to refine a proxy mesh
		return false;
	}

	bool bSuccess = false;
	switch (InBakeOption)
	{
	case EHoudiniEngineBakeOption::ToActor:
	{
		bSuccess = FHoudiniEngineBakeUtils::BakeHoudiniActorToActors(InHACToBake, bInReplacePreviousBake, bInReplacePreviousBake);
	}
	break;

	case EHoudiniEngineBakeOption::ToBlueprint:
	{
		bSuccess = FHoudiniEngineBakeUtils::BakeBlueprints(InHACToBake, bInReplacePreviousBake);
	}
	break;

	case EHoudiniEngineBakeOption::ToFoliage:
	{
		bSuccess = FHoudiniEngineBakeUtils::BakeHoudiniActorToFoliage(InHACToBake, bInReplacePreviousBake);
	}
	break;

	case EHoudiniEngineBakeOption::ToWorldOutliner:
	{
		//Todo
		bSuccess = false;
	}
	break;

	}

	if (bSuccess && bInRemoveHACOutputOnSuccess)
		FHoudiniOutputTranslator::ClearAndRemoveOutputs(InHACToBake);
	
	return bSuccess;
}

bool 
FHoudiniEngineBakeUtils::BakeHoudiniActorToActors(
	UHoudiniAssetComponent* HoudiniAssetComponent, bool bInReplaceActors, bool bInReplaceAssets) 
{
	if (!HoudiniAssetComponent || HoudiniAssetComponent->IsPendingKill())
		return false;

	TArray<FHoudiniEngineBakedActor> NewActors;
	TArray<UPackage*> PackagesToSave;
	FHoudiniEngineOutputStats BakeStats;

	if (!FHoudiniEngineBakeUtils::BakeHoudiniActorToActors(
		HoudiniAssetComponent, bInReplaceActors, bInReplaceAssets, NewActors, PackagesToSave, BakeStats))
	{
		// TODO ?
		HOUDINI_LOG_WARNING(TEXT("Errors when baking"));
	}

	// Save the created packages
	FHoudiniEngineBakeUtils::SaveBakedPackages(PackagesToSave);

	// Recenter and select the baked actors
	if (GEditor && NewActors.Num() > 0)
		GEditor->SelectNone(false, true);
	
	for (const FHoudiniEngineBakedActor& Entry : NewActors)
	{
		if (!IsValid(Entry.Actor))
			continue;
		
		if (HoudiniAssetComponent->bRecenterBakedActors)
			CenterActorToBoundingBoxCenter(Entry.Actor);

		if (GEditor)
			GEditor->SelectActor(Entry.Actor, true, false);
	}
	
	if (GEditor && NewActors.Num() > 0)
		GEditor->NoteSelectionChange();

	{
		const FString FinishedTemplate = TEXT("Baking finished. Created {0} packages. Updated {1} packages.");
		FString Msg = FString::Format(*FinishedTemplate, { BakeStats.NumPackagesCreated, BakeStats.NumPackagesUpdated } );
		FHoudiniEngine::Get().FinishTaskSlateNotification( FText::FromString(Msg) );
	}

	return true;
}

bool
FHoudiniEngineBakeUtils::BakeHoudiniActorToActors(
	UHoudiniAssetComponent* HoudiniAssetComponent,
	bool bInReplaceActors,
	bool bInReplaceAssets,
	TArray<FHoudiniEngineBakedActor>& OutNewActors, 
	TArray<UPackage*>& OutPackagesToSave,
	FHoudiniEngineOutputStats& OutBakeStats,
	TArray<EHoudiniOutputType> const* InOutputTypesToBake,
	TArray<EHoudiniInstancerComponentType> const* InInstancerComponentTypesToBake,
	AActor* InFallbackActor,
	const FString& InFallbackWorldOutlinerFolder)
{
	if (!HoudiniAssetComponent || HoudiniAssetComponent->IsPendingKill())
		return false;

	AActor* OwnerActor = HoudiniAssetComponent->GetOwner();
	if (!IsValid(OwnerActor))
		return false;

	const FString HoudiniAssetName = OwnerActor->GetName();

	// Get an array of the outputs
	const int32 NumOutputs = HoudiniAssetComponent->GetNumOutputs();
	TArray<UHoudiniOutput*> Outputs;
	Outputs.Reserve(NumOutputs);
	for (int32 OutputIdx = 0; OutputIdx < NumOutputs; ++OutputIdx)
	{
		Outputs.Add(HoudiniAssetComponent->GetOutputAt(OutputIdx));
	}

	// Get the previous bake objects and grow/shrink to match asset outputs
	TArray<FHoudiniBakedOutput>& BakedOutputs = HoudiniAssetComponent->GetBakedOutputs();
	// Ensure we have an entry for each output
	if (BakedOutputs.Num() != NumOutputs)
		BakedOutputs.SetNum(NumOutputs);

	return BakeHoudiniOutputsToActors(
		Outputs,
		BakedOutputs,
		HoudiniAssetName,
		HoudiniAssetComponent->GetComponentTransform(),
		HoudiniAssetComponent->BakeFolder,
		HoudiniAssetComponent->TemporaryCookFolder,
		bInReplaceActors,
		bInReplaceAssets,
		OutNewActors,
		OutPackagesToSave,
		OutBakeStats,
		InOutputTypesToBake,
		InInstancerComponentTypesToBake,
		InFallbackActor,
		InFallbackWorldOutlinerFolder);
}

bool
FHoudiniEngineBakeUtils::BakeHoudiniOutputsToActors(
	const TArray<UHoudiniOutput*>& InOutputs,
	TArray<FHoudiniBakedOutput>& InBakedOutputs,
	const FString& InHoudiniAssetName,
	const FTransform& InParentTransform,
	const FDirectoryPath& InBakeFolder,
	const FDirectoryPath& InTempCookFolder,
	bool bInReplaceActors,
	bool bInReplaceAssets,
	TArray<FHoudiniEngineBakedActor>& OutNewActors, 
	TArray<UPackage*>& OutPackagesToSave,
	FHoudiniEngineOutputStats& OutBakeStats,
	TArray<EHoudiniOutputType> const* InOutputTypesToBake,
	TArray<EHoudiniInstancerComponentType> const* InInstancerComponentTypesToBake,
	AActor* InFallbackActor,
	const FString& InFallbackWorldOutlinerFolder)
{
	const int32 NumOutputs = InOutputs.Num();
	
	const FString MsgTemplate = TEXT("Baking output: {0}/{1}.");
	FString Msg = FString::Format(*MsgTemplate, { 0, NumOutputs });
	FHoudiniEngine::Get().CreateTaskSlateNotification(FText::FromString(Msg));

	TArray<FHoudiniEngineBakedActor> BakedActors;

	// First bake everything except instancers, then bake instancers. Since instancers might use meshes in
	// from the other outputs.
	bool bHasAnyInstancers = false;
	int32 NumProcessedOutputs = 0;
	for (int32 OutputIdx = 0; OutputIdx < NumOutputs; ++OutputIdx)
	{
		UHoudiniOutput* Output = InOutputs[OutputIdx];
		if (!Output || Output->IsPendingKill())
		{
			NumProcessedOutputs++;
			continue;
		}

		Msg = FString::Format(*MsgTemplate, { NumProcessedOutputs + 1, NumOutputs });
		FHoudiniEngine::Get().UpdateTaskSlateNotification(FText::FromString(Msg));

		const EHoudiniOutputType OutputType = Output->GetType();
		// Check if we should skip this output type
		if (InOutputTypesToBake && InOutputTypesToBake->Find(OutputType) == INDEX_NONE)
		{
			NumProcessedOutputs++;
			continue;
		}

		switch (OutputType)
		{
		case EHoudiniOutputType::Mesh:
		{
			FHoudiniEngineBakeUtils::BakeStaticMeshOutputToActors(
				OutputIdx,
				InOutputs,
				InBakedOutputs,
				InHoudiniAssetName,
				InBakeFolder,
				InTempCookFolder,
				bInReplaceActors,
				bInReplaceAssets,
				BakedActors,
				OutPackagesToSave,
				InFallbackActor,
				InFallbackWorldOutlinerFolder);
		}
		break;

		case EHoudiniOutputType::Instancer:
		{
			if (!bHasAnyInstancers)
				bHasAnyInstancers = true;
			NumProcessedOutputs--;
		}
		break;

		case EHoudiniOutputType::Landscape:
		{
			const bool bResult = BakeLandscape(
				OutputIdx,
				Output,
				InBakedOutputs[OutputIdx].BakedOutputObjects,
				bInReplaceActors,
				bInReplaceAssets,
				InBakeFolder.Path,
				InHoudiniAssetName,
				OutBakeStats);
		}
		break;

		case EHoudiniOutputType::Skeletal:
			break;

		case EHoudiniOutputType::Curve:
		{
			FHoudiniEngineBakeUtils::BakeHoudiniCurveOutputToActors(
				Output,
				InBakedOutputs[OutputIdx].BakedOutputObjects,
				InBakedOutputs,
				InHoudiniAssetName,
				InBakeFolder,
				bInReplaceActors,
				bInReplaceAssets,
				BakedActors,
				InFallbackActor,
				InFallbackWorldOutlinerFolder);
		}
		break;

		case EHoudiniOutputType::Invalid:
			break;
		}

		NumProcessedOutputs++;
	}

	if (bHasAnyInstancers)
	{
		for (int32 OutputIdx = 0; OutputIdx < NumOutputs; ++OutputIdx)
		{
			UHoudiniOutput* Output = InOutputs[OutputIdx];
			if (!Output || Output->IsPendingKill())
			{
				NumProcessedOutputs++;
				continue;
			}

			Msg = FString::Format(*MsgTemplate, { NumProcessedOutputs + 1, NumOutputs });
			FHoudiniEngine::Get().UpdateTaskSlateNotification(FText::FromString(Msg));
			
			if (Output->GetType() == EHoudiniOutputType::Instancer)
			{
				FHoudiniEngineBakeUtils::BakeInstancerOutputToActors(
                    OutputIdx,
                    InOutputs,
                    InBakedOutputs,
                    InParentTransform,
                    InBakeFolder,
                    InTempCookFolder,
                    bInReplaceActors,
                    bInReplaceAssets,
                    BakedActors,
                    OutPackagesToSave,
                    InInstancerComponentTypesToBake,
                    InFallbackActor,
                    InFallbackWorldOutlinerFolder);
			}

			NumProcessedOutputs++;
		}
	}

	OutNewActors.Append(BakedActors);
	
	return true;
}


bool 
FHoudiniEngineBakeUtils::CanHoudiniAssetComponentBakeToFoliage(UHoudiniAssetComponent* HoudiniAssetComponent) 
{
	if (!HoudiniAssetComponent || HoudiniAssetComponent->IsPendingKill())
		return false;

	for (int32 n = 0; n < HoudiniAssetComponent->GetNumOutputs(); ++n) 
	{
		UHoudiniOutput* Output = HoudiniAssetComponent->GetOutputAt(n);
		if (!Output || Output->IsPendingKill())
			continue;

		if (Output->GetType() != EHoudiniOutputType::Instancer)
			continue;

		if (Output->GetInstancedOutputs().Num() > 0)
			return true;
		/*
		// TODO: Is this needed? check we have components to bake?
		for (auto& OutputObjectPair : Output->GetOutputObjects())
		{
			if (OutputObjectPair.Value.OutputCompoent!= nullpt)
				return true;
		}
		*/
	}

	return false;
}

bool 
FHoudiniEngineBakeUtils::BakeHoudiniActorToFoliage(UHoudiniAssetComponent* HoudiniAssetComponent, bool bInReplaceAssets) 
{
	if (!HoudiniAssetComponent || HoudiniAssetComponent->IsPendingKill())
		return false;

	AActor * OwnerActor = HoudiniAssetComponent->GetOwner();
	if (!OwnerActor || OwnerActor->IsPendingKill())
		return false;

	ULevel* DesiredLevel = GWorld->GetCurrentLevel();
	
	AInstancedFoliageActor* InstancedFoliageActor = AInstancedFoliageActor::GetInstancedFoliageActorForLevel(DesiredLevel, true);
	if (!InstancedFoliageActor || InstancedFoliageActor->IsPendingKill())
		return false;

	int32 BakedCount = 0;
	TArray<UPackage*> PackagesToSave;

	FTransform HoudiniAssetTransform = HoudiniAssetComponent->GetComponentTransform();

	// Build an array of the outputs so that we can search for meshes/previous baked meshes
	const int32 NumOutputs = HoudiniAssetComponent->GetNumOutputs();
	TArray<UHoudiniOutput*> Outputs;
	Outputs.Reserve(NumOutputs);
	for (int32 OutputIdx = 0; OutputIdx < NumOutputs; OutputIdx++)
	{
		UHoudiniOutput* Output = HoudiniAssetComponent->GetOutputAt(OutputIdx);
		if (!Output || Output->IsPendingKill())
			continue;

		Outputs.Add(Output);
	}

	// Get the previous bake outputs and match the output array size
	TArray<FHoudiniBakedOutput>& BakedOutputs = HoudiniAssetComponent->GetBakedOutputs();
	if (BakedOutputs.Num() != NumOutputs)
		BakedOutputs.SetNum(NumOutputs);
	
	// Map storing original and baked Static Meshes
	TMap< const UStaticMesh*, UStaticMesh* > OriginalToBakedMesh;
	for (int32 OutputIdx = 0; OutputIdx < NumOutputs; OutputIdx++)
	{
		UHoudiniOutput* Output = HoudiniAssetComponent->GetOutputAt(OutputIdx);
		if (!Output || Output->IsPendingKill())
			continue;

		if (Output->GetType() != EHoudiniOutputType::Instancer)
			continue;

		// TODO: No need to use the instanced outputs for this
		// We should simply iterate on the Output Objects instead!
		TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& OutputObjects = Output->GetOutputObjects();
		TMap<FHoudiniOutputObjectIdentifier, FHoudiniInstancedOutput>& InstancedOutputs = Output->GetInstancedOutputs();
		for (auto & Pair : InstancedOutputs)
		{
			FString InstanceName = OwnerActor->GetName();

			// // See if we have a bake name for that output
			// FHoudiniOutputObject* OutputObj = OutputObjects.Find(Pair.Key);
			// if (OutputObj && OutputObj->BakeName.IsEmpty())
			// 	InstanceName = OutputObj->BakeName;

			FHoudiniInstancedOutput& InstancedOutput = Pair.Value;
			for (int32 VariarionIdx = 0; VariarionIdx < InstancedOutput.VariationObjects.Num(); ++VariarionIdx)
			{
				// TODO: !!! what if the instanced object/var is not a static mesh!!!!!!
				UObject* CurrentVariationObject = InstancedOutput.VariationObjects[VariarionIdx].Get();
				UStaticMesh* InstancedStaticMesh = Cast<UStaticMesh>(CurrentVariationObject);
				if (!InstancedStaticMesh)
				{
					if (CurrentVariationObject)
					{
						HOUDINI_LOG_ERROR(TEXT("Failed to bake the instances of %s to Foliage"), *CurrentVariationObject->GetName());
					}
					continue;
				}

				// Check if we have already handled this mesh (already baked it from a previous variation), if so,
				// use that
				UStaticMesh* OutStaticMesh = nullptr;
				bool bCreateNewType = true;
				if (OriginalToBakedMesh.Contains(InstancedStaticMesh))
				{
					OutStaticMesh = OriginalToBakedMesh.FindChecked(InstancedStaticMesh);
					bCreateNewType = false;
				}

				if (!IsValid(OutStaticMesh))
				{
					// Find the output object and identifier for the mesh and previous bake of the mesh (if it exists)
					FString ObjectName;
					int32 MeshOutputIdx = INDEX_NONE;
					FHoudiniOutputObjectIdentifier MeshOutputIdentifier;
					UStaticMesh* PreviousBakeMesh = nullptr;
					FHoudiniBakedOutputObject* BakedOutputObject = nullptr;
					if (FindOutputObject(InstancedStaticMesh, Outputs, MeshOutputIdx, MeshOutputIdentifier))
					{
						GetTemporaryOutputObjectBakeName(InstancedStaticMesh, Outputs, ObjectName);

						BakedOutputObject = &BakedOutputs[MeshOutputIdx].BakedOutputObjects.FindOrAdd(MeshOutputIdentifier);
						if (BakedOutputObject)
						{
							PreviousBakeMesh = Cast<UStaticMesh>(BakedOutputObject->GetBakedObjectIfValid());
						}
					}
					else
					{
						ObjectName = FHoudiniPackageParams::GetPackageNameExcludingGUID(InstancedStaticMesh);
					}

					// If the instanced static mesh is still a temporary Houdini created Static Mesh
					// we will duplicate/bake it first before baking to foliage
					FHoudiniPackageParams PackageParams;
					// Set the replace mode based on if we are doing a replacement or incremental asset bake
					const EPackageReplaceMode AssetPackageReplaceMode = bInReplaceAssets ?
                        EPackageReplaceMode::ReplaceExistingAssets : EPackageReplaceMode::CreateNewAssets;
					FHoudiniEngineUtils::FillInPackageParamsForBakingOutput(
						PackageParams,
						MeshOutputIdentifier,
						HoudiniAssetComponent->BakeFolder.Path,
						ObjectName,
						OwnerActor->GetName(),
						AssetPackageReplaceMode);

					// DuplicateStaticMeshAndCreatePackageIfNeeded uses baked results to find a baked version of
					// InstancedStaticMesh in the current bake results, but since we are already using
					// OriginalToBakedMesh we don't have to populate BakedResults
					const TArray<FHoudiniEngineBakedActor> BakedResults;
					OutStaticMesh = FHoudiniEngineBakeUtils::DuplicateStaticMeshAndCreatePackageIfNeeded(
                        InstancedStaticMesh,
                        PreviousBakeMesh,
                        PackageParams,
                        Outputs,
                        BakedResults,
                        HoudiniAssetComponent->TemporaryCookFolder.Path,
                        PackagesToSave);
					OriginalToBakedMesh.Add(InstancedStaticMesh, OutStaticMesh);

					// Update our tracked baked output
					if (BakedOutputObject)
						BakedOutputObject->BakedObject = FSoftObjectPath(OutStaticMesh).ToString();

					bCreateNewType = true;
				}

				// See if we already have a FoliageType for that static mesh
				UFoliageType *FoliageType = InstancedFoliageActor->GetLocalFoliageTypeForSource(OutStaticMesh);
				if (!FoliageType || FoliageType->IsPendingKill()) 
				{
					// We need to create a new FoliageType for this Static Mesh
					// TODO: Add foliage default settings
					InstancedFoliageActor->AddMesh(OutStaticMesh, &FoliageType);
					bCreateNewType = true;
				}

				// If we are baking in replace mode, remove the foliage type if it already exists
				// and a create a new one
				if (bInReplaceAssets && bCreateNewType && IsValid(FoliageType))
					InstancedFoliageActor->RemoveFoliageType(&FoliageType, 1);
				
				// Get the FoliageMeshInfo for this Foliage type so we can add the instance to it
				FFoliageInfo* FoliageInfo = InstancedFoliageActor->FindOrAddMesh(FoliageType);
				if (!FoliageInfo)
					continue;

				// Apply the transform offset on the transforms for this variation
				TArray<FTransform> ProcessedTransforms;
				FHoudiniInstanceTranslator::ProcessInstanceTransforms(InstancedOutput, VariarionIdx, ProcessedTransforms);

				FFoliageInstance FoliageInstance;
				int32 CurrentInstanceCount = 0;
				for (auto CurrentTransform : ProcessedTransforms)
				{
					FoliageInstance.Location = HoudiniAssetTransform.TransformPosition(CurrentTransform.GetLocation());
					FoliageInstance.Rotation = HoudiniAssetTransform.TransformRotation(CurrentTransform.GetRotation()).Rotator();
					FoliageInstance.DrawScale3D = CurrentTransform.GetScale3D() * HoudiniAssetTransform.GetScale3D();

					FoliageInfo->AddInstance(InstancedFoliageActor, FoliageType, FoliageInstance);
					CurrentInstanceCount++;
				}

				// TODO: This was due to a bug in UE4.22-20, check if still needed! 
				if (FoliageInfo->GetComponent())
					FoliageInfo->GetComponent()->BuildTreeIfOutdated(true, true);

				// Notify the user that we succesfully bake the instances to foliage
				FString Notification = TEXT("Successfully baked ") + FString::FromInt(CurrentInstanceCount) + TEXT(" instances of ") + OutStaticMesh->GetName() + TEXT(" to Foliage");
				FHoudiniEngineUtils::CreateSlateNotification(Notification);

				BakedCount += ProcessedTransforms.Num();
			}
		}
	}

	InstancedFoliageActor->RegisterAllComponents();

	// Update / repopulate the foliage editor mode's mesh list
	FHoudiniEngineUtils::RepopulateFoliageTypeListInUI();
	
	if (BakedCount > 0)
	{
		FHoudiniEngineBakeUtils::SaveBakedPackages(PackagesToSave);
		return true;
	}

	return false;
}


bool
FHoudiniEngineBakeUtils::BakeInstancerOutputToActors(
	int32 InOutputIndex,
	const TArray<UHoudiniOutput*>& InAllOutputs,
	TArray<FHoudiniBakedOutput>& InBakedOutputs,
	const FTransform& InTransform,
	const FDirectoryPath& InBakeFolder,
	const FDirectoryPath& InTempCookFolder,
	bool bInReplaceActors,
	bool bInReplaceAssets,
	TArray<FHoudiniEngineBakedActor>& OutActors,
	TArray<UPackage*>& OutPackagesToSave,
	TArray<EHoudiniInstancerComponentType> const* InInstancerComponentTypesToBake,
	AActor* InFallbackActor,
	const FString& InFallbackWorldOutlinerFolder)
{
	if (!InAllOutputs.IsValidIndex(InOutputIndex))
		return false;

	UHoudiniOutput* InOutput = InAllOutputs[InOutputIndex];	
	if (!InOutput || InOutput->IsPendingKill())
		return false;

	TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& OutputObjects = InOutput->GetOutputObjects();

	// Ensure we have the same number of baked outputs and asset outputs
	if (InBakedOutputs.Num() != InAllOutputs.Num())
		InBakedOutputs.SetNum(InAllOutputs.Num());

	// Iterate on the output objects, baking their object/component as we go
	for (auto& Pair : OutputObjects)
	{
		FHoudiniOutputObject& CurrentOutputObject = Pair.Value;
		FHoudiniBakedOutputObject& BakedOutputObject = InBakedOutputs[InOutputIndex].BakedOutputObjects.FindOrAdd(Pair.Key);

		if (CurrentOutputObject.bProxyIsCurrent)
		{
			// TODO: we need to refine the SM first!
			// ?? 
		}

		if (!CurrentOutputObject.OutputComponent || CurrentOutputObject.OutputComponent->IsPendingKill())
			continue;

		if (CurrentOutputObject.OutputComponent->IsA<UFoliageInstancedStaticMeshComponent>())
		{
			// TODO: Baking foliage instancer to actors it not supported currently
		}
		else if (CurrentOutputObject.OutputComponent->IsA<UInstancedStaticMeshComponent>()
			&& (!InInstancerComponentTypesToBake || InInstancerComponentTypesToBake->Contains(EHoudiniInstancerComponentType::InstancedStaticMeshComponent)))
		{
			BakeInstancerOutputToActors_ISMC(
				InOutputIndex,
				InAllOutputs,
				// InBakedOutputs,
				Pair.Key, 
				CurrentOutputObject, 
				BakedOutputObject,
				InTransform,
				InBakeFolder,
				InTempCookFolder,
				bInReplaceActors, 
				bInReplaceAssets,
				OutActors,
				OutPackagesToSave,
				InFallbackActor,
				InFallbackWorldOutlinerFolder);
		}
		else if (CurrentOutputObject.OutputComponent->IsA<UHoudiniInstancedActorComponent>()
				&& (!InInstancerComponentTypesToBake || InInstancerComponentTypesToBake->Contains(EHoudiniInstancerComponentType::InstancedActorComponent)))
		{
			BakeInstancerOutputToActors_IAC(
				InOutputIndex,
				Pair.Key, 
				CurrentOutputObject, 
				BakedOutputObject,
				InBakeFolder,
				bInReplaceActors, 
				bInReplaceAssets,
				OutActors,
				OutPackagesToSave);
		}
		else if (CurrentOutputObject.OutputComponent->IsA<UHoudiniMeshSplitInstancerComponent>()
		 		 && (!InInstancerComponentTypesToBake || InInstancerComponentTypesToBake->Contains(EHoudiniInstancerComponentType::MeshSplitInstancerComponent)))
		{
			BakeInstancerOutputToActors_MSIC(
				InOutputIndex,
				InAllOutputs,
				// InBakedOutputs,
				Pair.Key, 
				CurrentOutputObject, 
				BakedOutputObject,
				InTransform,
				InBakeFolder,
				InTempCookFolder,
				bInReplaceActors, 
				bInReplaceAssets,
				OutActors,
				OutPackagesToSave,
				InFallbackActor,
				InFallbackWorldOutlinerFolder);
		}
		else if (CurrentOutputObject.OutputComponent->IsA<UStaticMeshComponent>()
	  			 && (!InInstancerComponentTypesToBake || InInstancerComponentTypesToBake->Contains(EHoudiniInstancerComponentType::StaticMeshComponent)))
		{
			BakeInstancerOutputToActors_SMC(
				InOutputIndex,
				InAllOutputs,
				// InBakedOutputs,
				Pair.Key, 
				CurrentOutputObject, 
				BakedOutputObject, 
				InBakeFolder,
				InTempCookFolder,
				bInReplaceActors, 
				bInReplaceAssets,
				OutActors,
				OutPackagesToSave,
				InFallbackActor,
				InFallbackWorldOutlinerFolder);
		}
		else
		{
			// Unsupported component!
		}

	}

	return true;
}

bool
FHoudiniEngineBakeUtils::BakeInstancerOutputToActors_ISMC(
	int32 InOutputIndex,
	const TArray<UHoudiniOutput*>& InAllOutputs,
	// const TArray<FHoudiniBakedOutput>& InAllBakedOutputs,
	const FHoudiniOutputObjectIdentifier& InOutputObjectIdentifier,
	const FHoudiniOutputObject& InOutputObject,
	FHoudiniBakedOutputObject& InBakedOutputObject,
	const FTransform& InTransform,
	const FDirectoryPath& InBakeFolder,
	const FDirectoryPath& InTempCookFolder,
	bool bInReplaceActors,
	bool bInReplaceAssets,
	TArray<FHoudiniEngineBakedActor>& OutActors,
	TArray<UPackage*>& OutPackagesToSave,
	AActor* InFallbackActor,
	const FString& InFallbackWorldOutlinerFolder)
{
	UInstancedStaticMeshComponent * InISMC = Cast<UInstancedStaticMeshComponent>(InOutputObject.OutputComponent);
	if (!InISMC || InISMC->IsPendingKill())
		return false;

	AActor * OwnerActor = InISMC->GetOwner();
	if (!OwnerActor || OwnerActor->IsPendingKill())
		return false;

	UStaticMesh * StaticMesh = InISMC->GetStaticMesh();
	if (!StaticMesh || StaticMesh->IsPendingKill())
		return false;

	// Find the incoming mesh in the output (only if its temporary) and get its bake name. If not temporary, get its
	// name from its package.
	FString ObjectName;
	if (!GetTemporaryOutputObjectBakeName(StaticMesh, InAllOutputs, ObjectName))
	{
		// Not found in HDA/temp outputs, use its package name
		ObjectName = FHoudiniPackageParams::GetPackageNameExcludingGUID(StaticMesh);
	}

	// Instancer name adds the split identifier (INSTANCERNUM_VARIATIONNUM)
	const FString BaseName = OwnerActor->GetName();
	const FString InstancerName = ObjectName + "_instancer_" + InOutputObjectIdentifier.SplitIdentifier;
	const FName WorldOutlinerFolderPath = GetOutlinerFolderPath(InOutputObject, FName(InFallbackWorldOutlinerFolder.IsEmpty() ? BaseName : InFallbackWorldOutlinerFolder));

	// See if the instanced static mesh is still a temporary Houdini created Static Mesh
	// If it is, we need to bake the StaticMesh first
	FHoudiniPackageParams PackageParams;
	const EPackageReplaceMode AssetPackageReplaceMode = bInReplaceAssets ?
		EPackageReplaceMode::ReplaceExistingAssets : EPackageReplaceMode::CreateNewAssets;
	FHoudiniEngineUtils::FillInPackageParamsForBakingOutput(
		PackageParams,
		InOutputObjectIdentifier,
		InBakeFolder.Path,
		// ObjectName + "_" + FHoudiniPackageParams::GetPackageNameExcludingGUID(StaticMesh),
		ObjectName,
		OwnerActor->GetName(),
		AssetPackageReplaceMode);

	// This will bake/duplicate the mesh if temporary, or return the input one if it is not
	UStaticMesh* PreviousStaticMesh = Cast<UStaticMesh>(InBakedOutputObject.GetBakedObjectIfValid());
	UStaticMesh* BakedStaticMesh = FHoudiniEngineBakeUtils::DuplicateStaticMeshAndCreatePackageIfNeeded(
		StaticMesh, PreviousStaticMesh, PackageParams, InAllOutputs, OutActors, InTempCookFolder.Path, OutPackagesToSave);

	// Update the baked object
	InBakedOutputObject.BakedObject = FSoftObjectPath(BakedStaticMesh).ToString();
	
	// By default spawn in the current level unless specified via the unreal_level_path attribute
	ULevel* DesiredLevel = GWorld->GetCurrentLevel();
	bool bHasLevelPathAttribute = InOutputObject.CachedAttributes.Contains(HAPI_UNREAL_ATTRIB_LEVEL_PATH);
	if (bHasLevelPathAttribute)
	{
		UWorld* DesiredWorld = OwnerActor ? OwnerActor->GetWorld() : GWorld;

		// Access some of the attribute that were cached on the output object
		FHoudiniAttributeResolver Resolver;
		{
			TMap<FString, FString> CachedAttributes = InOutputObject.CachedAttributes;
			TMap<FString, FString> Tokens = InOutputObject.CachedTokens;
			PackageParams.UpdateTokensFromParams(DesiredWorld, Tokens);
			Resolver.SetCachedAttributes(CachedAttributes);
			Resolver.SetTokensFromStringMap(Tokens);
		}

		// Get the package path from the unreal_level_apth attribute
		FString LevelPackagePath = Resolver.ResolveFullLevelPath();

		bool bCreatedPackage = false;
		if (!FHoudiniEngineBakeUtils::FindOrCreateDesiredLevelFromLevelPath(
			LevelPackagePath,
			DesiredLevel,
			DesiredWorld,
			bCreatedPackage))
		{
			// TODO: LOG ERROR IF NO LEVEL
			return false;
		}

		// If we have created a new level, add it to the packages to save
		// TODO: ? always add?
		if (bCreatedPackage && DesiredLevel)
		{
			// We can now save the package again, and unload it.
			OutPackagesToSave.Add(DesiredLevel->GetOutermost());
		}
	}

	if(!DesiredLevel)
		return false;

	// Try to find the unreal_bake_actor, if specified, or fallback to the default named actor
	FName BakeActorName;
	AActor* FoundActor = nullptr;
	bool bHasBakeActorName = false;
	if (!FindUnrealBakeActor(InOutputObject, InBakedOutputObject, OutActors, DesiredLevel, *InstancerName, bInReplaceActors, InFallbackActor, FoundActor, bHasBakeActorName, BakeActorName))
		return false;

	/*
	// TODO: Get the bake name!
	// Bake override, the output name
	// The bake name override has priority
	FString InstancerName = InOutputObject.BakeName;
	if (InstancerName.IsEmpty())
	{
		// .. then use the output name
		InstancerName = Resolver.ResolveOutputName();
	}
	*/

	// Should we create one actor with an ISMC or multiple actors with one SMC?
	bool bSpawnMultipleSMC = false;
	if (bSpawnMultipleSMC)
	{
		// TODO: Double check, Has a crash here!

		// Get the StaticMesh ActorFactory
		UActorFactory* SMFactory = nullptr;

		if (!FoundActor)
		{
			SMFactory = GEditor ? GEditor->FindActorFactoryByClass(UActorFactoryStaticMesh::StaticClass()) : nullptr;
			if (!SMFactory)
				return false;
		}

		// Split the instances to multiple StaticMeshActors
		for (int32 InstanceIdx = 0; InstanceIdx < InISMC->GetInstanceCount(); InstanceIdx++)
		{
			FTransform InstanceTransform;
			InISMC->GetInstanceTransform(InstanceIdx, InstanceTransform, true);

			if (!FoundActor)
			{
				FoundActor = SMFactory->CreateActor(BakedStaticMesh, DesiredLevel, InstanceTransform, RF_Transactional);
				if (!FoundActor || FoundActor->IsPendingKill())
					continue;
			}

			FName NewName = MakeUniqueObjectNameIfNeeded(DesiredLevel, SMFactory->NewActorClass, BakeActorName, FoundActor);
			// FoundActor->Rename(*NewName.ToString());
			// FoundActor->SetActorLabel(NewName.ToString());
			RenameAndRelabelActor(FoundActor, NewName.ToString(), false);

			// The folder is named after the original actor and contains all generated actors
			SetOutlinerFolderPath(FoundActor, InOutputObject, WorldOutlinerFolderPath);

			AStaticMeshActor* SMActor = Cast<AStaticMeshActor>(FoundActor);
			if (!SMActor || SMActor->IsPendingKill())
				continue;

			// Copy properties from the existing component
			CopyPropertyToNewActorAndComponent(FoundActor, SMActor->GetStaticMeshComponent(), InISMC);

			OutActors.Add(FHoudiniEngineBakedActor(
				FoundActor,
				BakeActorName,
				WorldOutlinerFolderPath,
				InOutputIndex,
				InOutputObjectIdentifier,
				BakedStaticMesh,
				StaticMesh));
		}
	}
	else
	{
		bool bSpawnedActor = false;
		if (!FoundActor)
		{
			// Only create one actor
			FActorSpawnParameters SpawnInfo;
			SpawnInfo.OverrideLevel = DesiredLevel;
			SpawnInfo.ObjectFlags = RF_Transactional;
			SpawnInfo.Name = MakeUniqueObjectNameIfNeeded(DesiredLevel, AActor::StaticClass(), BakeActorName);
			SpawnInfo.bDeferConstruction = true;

			// Spawn the new Actor
			FoundActor = DesiredLevel->OwningWorld->SpawnActor<AActor>(SpawnInfo);
			if (!FoundActor || FoundActor->IsPendingKill())
				return false;
			bSpawnedActor = true;

			FoundActor->SetActorLabel(FoundActor->GetName());
			FoundActor->SetActorHiddenInGame(InISMC->bHiddenInGame);
		}
		else
		{
			// If there is a previously baked component, and we are in replace mode, remove it
			if (bInReplaceAssets)
			{
				USceneComponent* InPrevComponent = Cast<USceneComponent>(InBakedOutputObject.GetBakedComponentIfValid());
				if (IsValid(InPrevComponent) && InPrevComponent->GetOwner() == FoundActor)
					RemovePreviouslyBakedComponent(InPrevComponent);
			}
			
			const FName UniqueActorName = MakeUniqueObjectNameIfNeeded(DesiredLevel, AActor::StaticClass(), BakeActorName, FoundActor);
			RenameAndRelabelActor(FoundActor, UniqueActorName.ToString(), false);
		}
		
		// The folder is named after the original actor and contains all generated actors
		SetOutlinerFolderPath(FoundActor, InOutputObject, WorldOutlinerFolderPath);

		// Get/create the actor's root component
		const bool bCreateIfMissing = true;
		USceneComponent* RootComponent = GetActorRootComponent(FoundActor, bCreateIfMissing);
		if (bSpawnedActor && IsValid(RootComponent))
			RootComponent->SetWorldTransform(InTransform);
		
		// Duplicate the instancer component, create a Hierarchical ISMC if needed
		UInstancedStaticMeshComponent* NewISMC = nullptr;
		UHierarchicalInstancedStaticMeshComponent* InHISMC = Cast<UHierarchicalInstancedStaticMeshComponent>(InISMC);
		if (InHISMC)
		{
			NewISMC = DuplicateObject<UHierarchicalInstancedStaticMeshComponent>(
				InHISMC,
				FoundActor,
				MakeUniqueObjectNameIfNeeded(FoundActor, InHISMC->GetClass(), InISMC->GetFName()));
		}
		else
		{
			NewISMC = DuplicateObject<UInstancedStaticMeshComponent>(
				InISMC,
				FoundActor,
				MakeUniqueObjectNameIfNeeded(FoundActor, InISMC->GetClass(), InISMC->GetFName()));
		}

		if (!NewISMC)
		{
			//DesiredLevel->OwningWorld->
			return false;
		}

		InBakedOutputObject.BakedComponent = FSoftObjectPath(NewISMC).ToString();

		NewISMC->RegisterComponent();
		// NewISMC->SetupAttachment(nullptr);
		NewISMC->SetStaticMesh(BakedStaticMesh);
		FoundActor->AddInstanceComponent(NewISMC);
		// NewActor->SetRootComponent(NewISMC);
		if (IsValid(RootComponent))
			NewISMC->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
		NewISMC->SetWorldTransform(InISMC->GetComponentTransform());

		// TODO: do we need to copy properties here, we duplicated the component
		// // Copy properties from the existing component
		// CopyPropertyToNewActorAndComponent(FoundActor, NewISMC, InISMC);

		if (bSpawnedActor)
			FoundActor->FinishSpawning(InTransform);

		InBakedOutputObject.Actor = FSoftObjectPath(FoundActor).ToString();
		OutActors.Add(FHoudiniEngineBakedActor(
			FoundActor,
			BakeActorName,
			WorldOutlinerFolderPath,
			InOutputIndex,
			InOutputObjectIdentifier,
			BakedStaticMesh,
			StaticMesh));

		FoundActor->InvalidateLightingCache();
		FoundActor->PostEditMove(true);
		FoundActor->MarkPackageDirty();
	}

	// If we are baking in replace mode, remove previously baked components/instancers
	if (bInReplaceActors && bInReplaceAssets)
	{
		const bool bInDestroyBakedComponent = false;
		const bool bInDestroyBakedInstancedActors = true;
		const bool bInDestroyBakedInstancedComponents = true;
		DestroyPreviousBakeOutput(
			InBakedOutputObject, bInDestroyBakedComponent, bInDestroyBakedInstancedActors, bInDestroyBakedInstancedComponents);
	}
	
	return true;
}

bool
FHoudiniEngineBakeUtils::BakeInstancerOutputToActors_SMC(
	int32 InOutputIndex,
	const TArray<UHoudiniOutput*>& InAllOutputs,
	// const TArray<FHoudiniBakedOutput>& InAllBakedOutputs,
	const FHoudiniOutputObjectIdentifier& InOutputObjectIdentifier,
	const FHoudiniOutputObject& InOutputObject,
	FHoudiniBakedOutputObject& InBakedOutputObject,
	const FDirectoryPath& InBakeFolder,
	const FDirectoryPath& InTempCookFolder,
	bool bInReplaceActors,
	bool bInReplaceAssets,
	TArray<FHoudiniEngineBakedActor>& OutActors,
	TArray<UPackage*>& OutPackagesToSave,
	AActor* InFallbackActor,
	const FString& InFallbackWorldOutlinerFolder)
{
	UStaticMeshComponent* InSMC = Cast<UStaticMeshComponent>(InOutputObject.OutputComponent);
	if (!InSMC || InSMC->IsPendingKill())
		return false;

	AActor* OwnerActor = InSMC->GetOwner();
	if (!OwnerActor || OwnerActor->IsPendingKill())
		return false;

	UStaticMesh* StaticMesh = InSMC->GetStaticMesh();
	if (!StaticMesh || StaticMesh->IsPendingKill())
		return false;

	// Find the incoming mesh in the output (only if its temporary) and get its bake name. If not temporary, get its
	// name from its package.
	FString ObjectName;
	if (!GetTemporaryOutputObjectBakeName(StaticMesh, InAllOutputs, ObjectName))
	{
		// Not found in HDA/temp outputs, use its package name
		ObjectName = FHoudiniPackageParams::GetPackageNameExcludingGUID(StaticMesh);
	}

	// BaseName holds the Actor / HDA name
	// Instancer name adds the split identifier (INSTANCERNUM_VARIATIONNUM)
	const FString BaseName = OwnerActor->GetName();
	const FString InstancerName = ObjectName + "_instancer_" + InOutputObjectIdentifier.SplitIdentifier;
	const FName WorldOutlinerFolderPath = GetOutlinerFolderPath(InOutputObject, FName(InFallbackWorldOutlinerFolder.IsEmpty() ? BaseName : InFallbackWorldOutlinerFolder));

	// See if the instanced static mesh is still a temporary Houdini created Static Mesh
	// If it is, we need to bake the StaticMesh first
	FHoudiniPackageParams PackageParams;
	const EPackageReplaceMode AssetPackageReplaceMode = bInReplaceAssets ?
		EPackageReplaceMode::ReplaceExistingAssets : EPackageReplaceMode::CreateNewAssets;
	FHoudiniEngineUtils::FillInPackageParamsForBakingOutput(
		PackageParams,
		InOutputObjectIdentifier,
		InBakeFolder.Path,
		// BaseName + "_" + FHoudiniPackageParams::GetPackageNameExcludingGUID(StaticMesh),
		ObjectName,
		OwnerActor->GetName(),
		AssetPackageReplaceMode);

	// This will bake/duplicate the mesh if temporary, or return the input one if it is not
	UStaticMesh* BakedStaticMesh = FHoudiniEngineBakeUtils::DuplicateStaticMeshAndCreatePackageIfNeeded(
		StaticMesh, Cast<UStaticMesh>(InBakedOutputObject.GetBakedObjectIfValid()), PackageParams, InAllOutputs,
		OutActors, InTempCookFolder.Path, OutPackagesToSave);

	// Update the previous baked object
	InBakedOutputObject.BakedObject = FSoftObjectPath(BakedStaticMesh).ToString();

	// By default spawn in the current level unless specified via the unreal_level_path attribute
	ULevel* DesiredLevel = GWorld->GetCurrentLevel();
	bool bHasLevelPathAttribute = InOutputObject.CachedAttributes.Contains(HAPI_UNREAL_ATTRIB_LEVEL_PATH);
	if (bHasLevelPathAttribute)
	{
		UWorld* DesiredWorld = OwnerActor ? OwnerActor->GetWorld() : GWorld;

		// Access some of the attribute that were cached on the output object
		FHoudiniAttributeResolver Resolver;
		{
			TMap<FString, FString> CachedAttributes = InOutputObject.CachedAttributes;
			TMap<FString, FString> Tokens = InOutputObject.CachedTokens;
			PackageParams.UpdateTokensFromParams(DesiredWorld, Tokens);
			Resolver.SetCachedAttributes(CachedAttributes);
			Resolver.SetTokensFromStringMap(Tokens);
		}

		// Get the package path from the unreal_level_apth attribute
		FString LevelPackagePath = Resolver.ResolveFullLevelPath();

		bool bCreatedPackage = false;
		if (!FHoudiniEngineBakeUtils::FindOrCreateDesiredLevelFromLevelPath(
			LevelPackagePath,
			DesiredLevel,
			DesiredWorld,
			bCreatedPackage))
		{
			// TODO: LOG ERROR IF NO LEVEL
			return false;
		}

		// If we have created a level, add it to the packages to save
		// TODO: ? always add?
		if (bCreatedPackage && DesiredLevel)
		{
			// We can now save the package again, and unload it.
			OutPackagesToSave.Add(DesiredLevel->GetOutermost());
		}
	}

	if (!DesiredLevel)
		return false;

	// Try to find the unreal_bake_actor, if specified
	FName BakeActorName;
	AActor* FoundActor = nullptr;
	bool bHasBakeActorName = false;
	if (!FindUnrealBakeActor(InOutputObject, InBakedOutputObject, OutActors, DesiredLevel, *InstancerName, bInReplaceActors, InFallbackActor, FoundActor, bHasBakeActorName, BakeActorName))
		return false;

	UStaticMeshComponent* StaticMeshComponent = nullptr;
	// Create an actor if we didn't find one
	if (!FoundActor)
	{
		// Get the StaticMesh ActorFactory
		UActorFactory* SMFactory = GEditor ? GEditor->FindActorFactoryByClass(UActorFactoryStaticMesh::StaticClass()) : nullptr;
		if (!SMFactory)
			return false;

		FoundActor = SMFactory->CreateActor(BakedStaticMesh, DesiredLevel, InSMC->GetComponentTransform(), RF_Transactional);
		if (!FoundActor || FoundActor->IsPendingKill())
			return false;

		AStaticMeshActor* SMActor = Cast<AStaticMeshActor>(FoundActor);
		if (!SMActor || SMActor->IsPendingKill())
			return false;

		StaticMeshComponent = SMActor->GetStaticMeshComponent();
	}
	else
	{
		USceneComponent* RootComponent = GetActorRootComponent(FoundActor);
		if (!IsValid(RootComponent))
			return false;

		if (bInReplaceAssets)
		{
			// Check if we have a previous bake component and that it belongs to FoundActor, if so, reuse it
			UStaticMeshComponent* PrevSMC = Cast<UStaticMeshComponent>(InBakedOutputObject.GetBakedComponentIfValid());
			if (IsValid(PrevSMC) && (PrevSMC->GetOwner() == FoundActor))
			{
				StaticMeshComponent = PrevSMC;
			}
		}
		
		if (!IsValid(StaticMeshComponent))
		{
			// Create a new static mesh component
			StaticMeshComponent = NewObject<UStaticMeshComponent>(FoundActor, NAME_None, RF_Transactional);

			FoundActor->AddInstanceComponent(StaticMeshComponent);
			StaticMeshComponent->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
			StaticMeshComponent->RegisterComponent();
		}
	}

	FName NewName = MakeUniqueObjectNameIfNeeded(DesiredLevel, FoundActor->GetClass(), BakeActorName, FoundActor);
	// FoundActor->Rename(*NewName.ToString());
	// FoundActor->SetActorLabel(NewName.ToString());
	RenameAndRelabelActor(FoundActor, NewName.ToString(), false);

	// The folder is named after the original actor and contains all generated actors
	SetOutlinerFolderPath(FoundActor, InOutputObject, WorldOutlinerFolderPath);

	// Update the previous baked component
	InBakedOutputObject.BakedComponent = FSoftObjectPath(StaticMeshComponent).ToString();
	
	if (!IsValid(StaticMeshComponent))
		return false;
	
	// Copy properties from the existing component
	CopyPropertyToNewActorAndComponent(FoundActor, StaticMeshComponent, InSMC);
	StaticMeshComponent->SetStaticMesh(BakedStaticMesh);
	
	InBakedOutputObject.Actor = FSoftObjectPath(FoundActor).ToString();
	OutActors.Add(FHoudiniEngineBakedActor(
		FoundActor,
		BakeActorName,
		WorldOutlinerFolderPath,
		InOutputIndex,
		InOutputObjectIdentifier,
		BakedStaticMesh,
		StaticMesh));

	// If we are baking in replace mode, remove previously baked components/instancers
	if (bInReplaceActors && bInReplaceAssets)
	{
		const bool bInDestroyBakedComponent = false;
		const bool bInDestroyBakedInstancedActors = true;
		const bool bInDestroyBakedInstancedComponents = true;
		DestroyPreviousBakeOutput(
			InBakedOutputObject, bInDestroyBakedComponent, bInDestroyBakedInstancedActors, bInDestroyBakedInstancedComponents);
	}

	return true;
}

bool
FHoudiniEngineBakeUtils::BakeInstancerOutputToActors_IAC(
	int32 InOutputIndex,
	const FHoudiniOutputObjectIdentifier& InOutputObjectIdentifier,
	const FHoudiniOutputObject& InOutputObject,
	FHoudiniBakedOutputObject& InBakedOutputObject,
	const FDirectoryPath& InBakeFolder,
	bool bInReplaceActors,
	bool bInReplaceAssets,
	TArray<FHoudiniEngineBakedActor>& OutActors,
	TArray<UPackage*>& OutPackagesToSave)
{
	UHoudiniInstancedActorComponent* InIAC = Cast<UHoudiniInstancedActorComponent>(InOutputObject.OutputComponent);
	if (!InIAC || InIAC->IsPendingKill())
		return false;

	AActor * OwnerActor = InIAC->GetOwner();
	if (!OwnerActor || OwnerActor->IsPendingKill())
		return false;

	// BaseName holds the Actor / HDA name
	const FName BaseName = FName(OwnerActor->GetName());

	// Get the object instanced by this IAC
	UObject* InstancedObject = InIAC->GetInstancedObject();
	if (!InstancedObject || InstancedObject->IsPendingKill())
		return false;

	FHoudiniPackageParams PackageParams;
	const EPackageReplaceMode AssetPackageReplaceMode = bInReplaceAssets ?
		EPackageReplaceMode::ReplaceExistingAssets : EPackageReplaceMode::CreateNewAssets;
	FHoudiniEngineUtils::FillInPackageParamsForBakingOutput(
		PackageParams,
		InOutputObjectIdentifier,
		InBakeFolder.Path,
		BaseName.ToString(),
		OwnerActor->GetName(),
		AssetPackageReplaceMode);

	// By default spawn in the current level unless specified via the unreal_level_path attribute
	UWorld* DesiredWorld = OwnerActor ? OwnerActor->GetWorld() : GWorld;
	ULevel* DesiredLevel = GWorld->GetCurrentLevel();

	bool bHasLevelPathAttribute = InOutputObject.CachedAttributes.Contains(HAPI_UNREAL_ATTRIB_LEVEL_PATH);
	if (bHasLevelPathAttribute)
	{
		// Access some of the attribute that were cached on the output object
		FHoudiniAttributeResolver Resolver;
		{
			TMap<FString, FString> CachedAttributes = InOutputObject.CachedAttributes;
			TMap<FString, FString> Tokens = InOutputObject.CachedTokens;
			PackageParams.UpdateTokensFromParams(DesiredWorld, Tokens);
			Resolver.SetCachedAttributes(CachedAttributes);
			Resolver.SetTokensFromStringMap(Tokens);
		}

		// Get the package path from the unreal_level_apth attribute
		FString LevelPackagePath = Resolver.ResolveFullLevelPath();

		bool bCreatedPackage = false;
		if (!FHoudiniEngineBakeUtils::FindOrCreateDesiredLevelFromLevelPath(
			LevelPackagePath,
			DesiredLevel,
			DesiredWorld,
			bCreatedPackage))
		{
			// TODO: LOG ERROR IF NO LEVEL
			return false;
		}

		// If we have created a level, add it to the packages to save
		// TODO: ? always add?
		if (bCreatedPackage && DesiredLevel)
		{
			// We can now save the package again, and unload it.
			OutPackagesToSave.Add(DesiredLevel->GetOutermost());
		}
	}

	if (!DesiredLevel)
		return false;

	// If we are baking in actor replacement mode, remove any previously baked instanced actors for this output
	if (bInReplaceActors && InBakedOutputObject.InstancedActors.Num() > 0)
	{
		UWorld* LevelWorld = DesiredLevel->GetWorld();
		if (IsValid(LevelWorld))
		{
			for (const FString& ActorPathStr : InBakedOutputObject.InstancedActors)
			{
				const FSoftObjectPath ActorPath(ActorPathStr);

				if (!ActorPath.IsValid())
					continue;
				
				AActor* Actor = Cast<AActor>(ActorPath.TryLoad());
				// Destroy Actor if it is valid and part of DesiredLevel
				if (IsValid(Actor) && Actor->GetLevel() == DesiredLevel)
				{
#if WITH_EDITOR
					LevelWorld->EditorDestroyActor(Actor, true);
#else
					LevelWorld->DestroyActor(Actor);
#endif
				}
			}
		}
	}

	// Empty and reserve enough space for new instanced actors
	InBakedOutputObject.InstancedActors.Empty(InIAC->GetInstancedActors().Num());

	// Iterates on all the instances of the IAC
	for (AActor* CurrentInstancedActor : InIAC->GetInstancedActors())
	{
		if (!CurrentInstancedActor || CurrentInstancedActor->IsPendingKill())
			continue;

		FName NewInstanceName = MakeUniqueObjectNameIfNeeded(DesiredLevel, InstancedObject->StaticClass(), BaseName);
		FString NewNameStr = NewInstanceName.ToString();

		FTransform CurrentTransform = CurrentInstancedActor->GetTransform();
		AActor* NewActor = FHoudiniInstanceTranslator::SpawnInstanceActor(CurrentTransform, DesiredLevel, InIAC);
		if (!NewActor || NewActor->IsPendingKill())
			continue;

		const auto CopyOptions = (EditorUtilities::ECopyOptions::Type)
			(EditorUtilities::ECopyOptions::OnlyCopyEditOrInterpProperties |
				EditorUtilities::ECopyOptions::PropagateChangesToArchetypeInstances);

		EditorUtilities::CopyActorProperties(CurrentInstancedActor, NewActor);

		const FName WorldOutlinerFolderPath = GetOutlinerFolderPath(InOutputObject, BaseName);

		NewActor->SetActorLabel(NewNameStr);
		SetOutlinerFolderPath(NewActor, InOutputObject, WorldOutlinerFolderPath);
		NewActor->SetActorTransform(CurrentTransform);

		InBakedOutputObject.InstancedActors.Add(FSoftObjectPath(NewActor).ToString());
		
		OutActors.Add(FHoudiniEngineBakedActor(
			NewActor,
			BaseName,
			WorldOutlinerFolderPath,
			InOutputIndex,
			InOutputObjectIdentifier,
			nullptr,
			InstancedObject));
	}

	// TODO:
	// Move Actors to DesiredLevel if needed??

	// If we are baking in replace mode, remove previously baked components/instancers
	if (bInReplaceActors && bInReplaceAssets)
	{
		const bool bInDestroyBakedComponent = true;
		const bool bInDestroyBakedInstancedActors = false;
		const bool bInDestroyBakedInstancedComponents = true;
		DestroyPreviousBakeOutput(
			InBakedOutputObject, bInDestroyBakedComponent, bInDestroyBakedInstancedActors, bInDestroyBakedInstancedComponents);
	}

	return true;
}

bool
FHoudiniEngineBakeUtils::BakeInstancerOutputToActors_MSIC(
	int32 InOutputIndex,
	const TArray<UHoudiniOutput*>& InAllOutputs,
	// const TArray<FHoudiniBakedOutput>& InAllBakedOutputs,
	const FHoudiniOutputObjectIdentifier& InOutputObjectIdentifier,
	const FHoudiniOutputObject& InOutputObject,
	FHoudiniBakedOutputObject& InBakedOutputObject,
	const FTransform& InTransform,
	const FDirectoryPath& InBakeFolder,
	const FDirectoryPath& InTempCookFolder,
	bool bInReplaceActors,
	bool bInReplaceAssets,
	TArray<FHoudiniEngineBakedActor>& OutActors,
	TArray<UPackage*>& OutPackagesToSave,
	AActor* InFallbackActor,
	const FString& InFallbackWorldOutlinerFolder)
{
	UHoudiniMeshSplitInstancerComponent * InMSIC = Cast<UHoudiniMeshSplitInstancerComponent>(InOutputObject.OutputComponent);
	if (!InMSIC || InMSIC->IsPendingKill())
		return false;

	AActor * OwnerActor = InMSIC->GetOwner();
	if (!OwnerActor || OwnerActor->IsPendingKill())
		return false;

	UStaticMesh * StaticMesh = InMSIC->GetStaticMesh();
	if (!StaticMesh || StaticMesh->IsPendingKill())
		return false;

	// Find the incoming mesh in the output (only if its temporary) and get its bake name. If not temporary, get its
	// name from its package.
	FString ObjectName;
	if (!GetTemporaryOutputObjectBakeName(StaticMesh, InAllOutputs, ObjectName))
	{
		// Not found in HDA/temp outputs, use its package name
		ObjectName = FHoudiniPackageParams::GetPackageNameExcludingGUID(StaticMesh);
	}

	// Instancer name adds the split identifier (INSTANCERNUM_VARIATIONNUM)
	const FString BaseName = OwnerActor->GetName();
	const FString InstancerName = ObjectName + "_instancer_" + InOutputObjectIdentifier.SplitIdentifier;
	const FName WorldOutlinerFolderPath = GetOutlinerFolderPath(InOutputObject, FName(InFallbackWorldOutlinerFolder.IsEmpty() ? BaseName : InFallbackWorldOutlinerFolder));

	// See if the instanced static mesh is still a temporary Houdini created Static Mesh
	// If it is, we need to bake the StaticMesh first
	FHoudiniPackageParams PackageParams;
	const EPackageReplaceMode AssetPackageReplaceMode = bInReplaceAssets ?
		EPackageReplaceMode::ReplaceExistingAssets : EPackageReplaceMode::CreateNewAssets;
	FHoudiniEngineUtils::FillInPackageParamsForBakingOutput(
		PackageParams,
		InOutputObjectIdentifier,
		InBakeFolder.Path,
		// BaseName + "_" + FHoudiniPackageParams::GetPackageNameExcludingGUID(StaticMesh),
		ObjectName,
		OwnerActor->GetName(),
		AssetPackageReplaceMode);

	// This will bake/duplicate the mesh if temporary, or return the input one if it is not
	UStaticMesh* BakedStaticMesh = FHoudiniEngineBakeUtils::DuplicateStaticMeshAndCreatePackageIfNeeded(
		StaticMesh, Cast<UStaticMesh>(InBakedOutputObject.GetBakedObjectIfValid()), PackageParams, InAllOutputs,
		OutActors, InTempCookFolder.Path, OutPackagesToSave);

	// Update the baked output
	InBakedOutputObject.BakedObject = FSoftObjectPath(BakedStaticMesh).ToString();
	
	// By default spawn in the current level unless specified via the unreal_level_path attribute
	ULevel* DesiredLevel = GWorld->GetCurrentLevel();

	bool bHasLevelPathAttribute = InOutputObject.CachedAttributes.Contains(HAPI_UNREAL_ATTRIB_LEVEL_PATH);
	if (bHasLevelPathAttribute)
	{
		UWorld* DesiredWorld = OwnerActor ? OwnerActor->GetWorld() : GWorld;

		// Get the level specified by attribute
		// Access some of the attributes that were cached on the output object
		FHoudiniAttributeResolver Resolver;
		{
			TMap<FString, FString> CachedAttributes = InOutputObject.CachedAttributes;
			TMap<FString, FString> Tokens = InOutputObject.CachedTokens;
			PackageParams.UpdateTokensFromParams(DesiredWorld, Tokens);
			Resolver.SetCachedAttributes(CachedAttributes);
			Resolver.SetTokensFromStringMap(Tokens);
		}

		// Get the package path from the unreal_level_path attribute
		FString LevelPackagePath = Resolver.ResolveFullLevelPath();

		bool bCreatedPackage = false;
		if (!FHoudiniEngineBakeUtils::FindOrCreateDesiredLevelFromLevelPath(
			LevelPackagePath,
			DesiredLevel,
			DesiredWorld,
			bCreatedPackage))
		{
			// TODO: LOG ERROR IF NO LEVEL
			return false;
		}

		// If we have created a level, add it to the packages to save
		// TODO: ? always add?
		if (bCreatedPackage && DesiredLevel)
		{
			// We can now save the package again, and unload it.
			OutPackagesToSave.Add(DesiredLevel->GetOutermost());
		}
	}

	if (!DesiredLevel)
		return false;

	// Try to find the unreal_bake_actor, if specified
	FName BakeActorName;
	AActor* FoundActor = nullptr;
	bool bHasBakeActorName = false;
	bool bSpawnedActor = false;
	if (!FindUnrealBakeActor(InOutputObject, InBakedOutputObject, OutActors, DesiredLevel, *InstancerName, bInReplaceActors, InFallbackActor, FoundActor, bHasBakeActorName, BakeActorName))
		return false;

	if (!FoundActor)
	{
		// This is a split mesh instancer component - we will create a generic AActor with a bunch of SMC
		FActorSpawnParameters SpawnInfo;
		SpawnInfo.OverrideLevel = DesiredLevel;
		SpawnInfo.ObjectFlags = RF_Transactional;
		SpawnInfo.Name = MakeUniqueObjectNameIfNeeded(DesiredLevel, AActor::StaticClass(), BakeActorName);
		SpawnInfo.bDeferConstruction = true;

		// Spawn the new Actor
		FoundActor = DesiredLevel->OwningWorld->SpawnActor<AActor>(SpawnInfo);
		if (!FoundActor || FoundActor->IsPendingKill())
			return false;
		bSpawnedActor = true;

		FoundActor->SetActorLabel(FoundActor->GetName());
		FoundActor->SetActorHiddenInGame(InMSIC->bHiddenInGame);
	}
	else
	{
		// If we are baking in replacement mode, remove the previous components (if they belong to FoundActor)
		for (const FString& PrevComponentPathStr : InBakedOutputObject.InstancedComponents)
		{
			const FSoftObjectPath PrevComponentPath(PrevComponentPathStr);

			if (!PrevComponentPath.IsValid())
				continue;
			
			UActorComponent* PrevComponent = Cast<UActorComponent>(PrevComponentPath.TryLoad());
			if (!IsValid(PrevComponent) || PrevComponent->GetOwner() != FoundActor)
				continue;

			RemovePreviouslyBakedComponent(PrevComponent);
		}

		const FName UniqueActorName = MakeUniqueObjectNameIfNeeded(DesiredLevel, AActor::StaticClass(), BakeActorName, FoundActor);
		RenameAndRelabelActor(FoundActor, UniqueActorName.ToString(), false);
	}
	// The folder is named after the original actor and contains all generated actors
	SetOutlinerFolderPath(FoundActor, InOutputObject, WorldOutlinerFolderPath);

	// Get/create the actor's root component
	const bool bCreateIfMissing = true;
	USceneComponent* RootComponent = GetActorRootComponent(FoundActor, bCreateIfMissing);
	if (bSpawnedActor && IsValid(RootComponent))
		RootComponent->SetWorldTransform(InTransform);

	// Empty and reserve enough space in the baked components array for the new components
	InBakedOutputObject.InstancedComponents.Empty(InMSIC->GetInstances().Num());
	
	// Now add s SMC component for each of the SMC's instance
	for (UStaticMeshComponent* CurrentSMC : InMSIC->GetInstances())
	{
		if (!CurrentSMC || CurrentSMC->IsPendingKill())
			continue;

		UStaticMeshComponent* NewSMC = DuplicateObject<UStaticMeshComponent>(
			CurrentSMC,
			FoundActor,
			MakeUniqueObjectNameIfNeeded(FoundActor, CurrentSMC->GetClass(), CurrentSMC->GetFName()));
		if (!NewSMC || NewSMC->IsPendingKill())
			continue;

		InBakedOutputObject.InstancedComponents.Add(FSoftObjectPath(NewSMC).ToString());
		
		NewSMC->RegisterComponent();
		// NewSMC->SetupAttachment(nullptr);
		NewSMC->SetStaticMesh(BakedStaticMesh);
		FoundActor->AddInstanceComponent(NewSMC);
		NewSMC->SetWorldTransform(CurrentSMC->GetComponentTransform());
		if (IsValid(RootComponent))
			NewSMC->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepWorldTransform);

		// TODO: Do we need to copy properties here, we duplicated the component
		// // Copy properties from the existing component
		// CopyPropertyToNewActorAndComponent(FoundActor, NewSMC, CurrentSMC);
	}

	if (bSpawnedActor)
		FoundActor->FinishSpawning(InTransform);

	InBakedOutputObject.Actor = FSoftObjectPath(FoundActor).ToString();
	OutActors.Add(FHoudiniEngineBakedActor(
		FoundActor,
		BakeActorName,
		WorldOutlinerFolderPath,
		InOutputIndex,
		InOutputObjectIdentifier,
		BakedStaticMesh,
		StaticMesh));

	FoundActor->InvalidateLightingCache();
	FoundActor->PostEditMove(true);
	FoundActor->MarkPackageDirty();

	// If we are baking in replace mode, remove previously baked components/instancers
	if (bInReplaceActors && bInReplaceAssets)
	{
		const bool bInDestroyBakedComponent = true;
		const bool bInDestroyBakedInstancedActors = true;
		const bool bInDestroyBakedInstancedComponents = false;
		DestroyPreviousBakeOutput(
			InBakedOutputObject, bInDestroyBakedComponent, bInDestroyBakedInstancedActors, bInDestroyBakedInstancedComponents);
	}

	return true;
}

bool
FHoudiniEngineBakeUtils::FindHGPO(
	const FHoudiniOutputObjectIdentifier& InIdentifier,
	const TArray<FHoudiniGeoPartObject>& InHGPOs,
	FHoudiniGeoPartObject const*& OutHGPO)
{
	// Find the HGPO that matches this output identifier
	const FHoudiniGeoPartObject* FoundHGPO = nullptr;
	for (auto & NextHGPO : InHGPOs) 
	{
		// We use Matches() here as it handles the case where the HDA was loaded,
		// which likely means that the the obj/geo/part ids dont match the output identifier
		if(InIdentifier.Matches(NextHGPO))
		{
			FoundHGPO = &NextHGPO;
			break;
		}
	}

	OutHGPO = FoundHGPO;
	return !OutHGPO;
}

void
FHoudiniEngineBakeUtils::GetTemporaryOutputObjectBakeName(
	const UObject* InObject,
	const FHoudiniOutputObject& InMeshOutputObject,
	FString& OutBakeName)
{
	// The bake name override has priority
	OutBakeName = InMeshOutputObject.BakeName;
	if (OutBakeName.IsEmpty())
	{
		FHoudiniAttributeResolver Resolver;
		Resolver.SetCachedAttributes(InMeshOutputObject.CachedAttributes);
		Resolver.SetTokensFromStringMap(InMeshOutputObject.CachedTokens);
		const FString DefaultObjectName = FHoudiniPackageParams::GetPackageNameExcludingGUID(InObject);
		// The default output name (if not set via attributes) is {object_name}, which look for an object_name
		// key-value token
		if (!Resolver.GetCachedTokens().Contains(TEXT("object_name")))
			Resolver.SetToken(TEXT("object_name"), DefaultObjectName);
		OutBakeName = Resolver.ResolveOutputName();
		// const TArray<FHoudiniGeoPartObject>& HGPOs = InAllOutputs[MeshOutputIdx]->GetHoudiniGeoPartObjects();
		// const FHoudiniGeoPartObject* FoundHGPO = nullptr;
		// FindHGPO(MeshIdentifier, HGPOs, FoundHGPO);
		// // ... finally the part name
		// if (FoundHGPO && FoundHGPO->bHasCustomPartName)
		// 	OutBakeName = FoundHGPO->PartName;
		if (OutBakeName.IsEmpty())
			OutBakeName = DefaultObjectName;
	}
}

bool
FHoudiniEngineBakeUtils::GetTemporaryOutputObjectBakeName(
	const UObject* InObject,
	const TArray<UHoudiniOutput*>& InAllOutputs,
	FString& OutBakeName)
{
	if (!IsValid(InObject))
		return false;
	
	OutBakeName.Empty();
	
	int32 MeshOutputIdx = INDEX_NONE;
	FHoudiniOutputObjectIdentifier MeshIdentifier;
	if (FindOutputObject(InObject, InAllOutputs, MeshOutputIdx, MeshIdentifier))
	{
		// Found the mesh, get its name
		const FHoudiniOutputObject& MeshOutputObject = InAllOutputs[MeshOutputIdx]->GetOutputObjects().FindChecked(MeshIdentifier);
		GetTemporaryOutputObjectBakeName(InObject, MeshOutputObject, OutBakeName);
		
		return true;
	}

	return false;
}

bool 
FHoudiniEngineBakeUtils::BakeStaticMeshOutputToActors(
	int32 InOutputIndex,
	const TArray<UHoudiniOutput*>& InAllOutputs,
	TArray<FHoudiniBakedOutput>& InBakedOutputs,
	const FString& InHoudiniAssetName,
	const FDirectoryPath& InBakeFolder,
	const FDirectoryPath& InTempCookFolder,
	bool bInReplaceActors,
	bool bInReplaceAssets,
	TArray<FHoudiniEngineBakedActor>& OutActors,
	TArray<UPackage*>& OutPackagesToSave,
	AActor* InFallbackActor,
	const FString& InFallbackWorldOutlinerFolder)
{
	if (!InAllOutputs.IsValidIndex(InOutputIndex))
		return false;

	UHoudiniOutput* InOutput = InAllOutputs[InOutputIndex];
	if (!InOutput || InOutput->IsPendingKill())
		return false;

	UActorFactory* Factory = GEditor ? GEditor->FindActorFactoryByClass(UActorFactoryStaticMesh::StaticClass()) : nullptr;
	if (!Factory)
		return false;

	TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& OutputObjects = InOutput->GetOutputObjects();
const TArray<FHoudiniGeoPartObject>& HGPOs = InOutput->GetHoudiniGeoPartObjects();

	// Get the previous bake objects
	if (InOutputIndex >= 0 && !InBakedOutputs.IsValidIndex(InOutputIndex))
		InBakedOutputs.SetNum(InOutputIndex + 1);
	TMap<FHoudiniOutputObjectIdentifier, FHoudiniBakedOutputObject>& BakedOutputObjects = InBakedOutputs[InOutputIndex].BakedOutputObjects;

	for (auto& Pair : OutputObjects)
	{
		const FHoudiniOutputObjectIdentifier& Identifier = Pair.Key;
		const FHoudiniOutputObject& OutputObject = Pair.Value;

		// Fetch previous bake output
		FHoudiniBakedOutputObject& BakedOutputObject = BakedOutputObjects.FindOrAdd(Identifier);

		UStaticMesh* StaticMesh = Cast<UStaticMesh>(OutputObject.OutputObject);
		if (!StaticMesh || StaticMesh->IsPendingKill())
			continue;

		UStaticMeshComponent* InSMC = Cast<UStaticMeshComponent>(OutputObject.OutputComponent);
		if (!InSMC || InSMC->IsPendingKill())
			continue;

		// Find the HGPO that matches this output identifier
		const FHoudiniGeoPartObject* FoundHGPO = nullptr;
		FindHGPO(Identifier, HGPOs, FoundHGPO);

		// We do not bake templated geos
		if (FoundHGPO && FoundHGPO->bIsTemplated)
			continue;

		FHoudiniAttributeResolver Resolver;
		Resolver.SetCachedAttributes(OutputObject.CachedAttributes);
		Resolver.SetTokensFromStringMap(OutputObject.CachedTokens);
		const FString DefaultObjectName = FHoudiniPackageParams::GetPackageNameExcludingGUID(StaticMesh);
		// The default output name (if not set via attributes) is {object_name}, which look for an object_name
		// key-value token
		if (!Resolver.GetCachedTokens().Contains(TEXT("object_name")))
			Resolver.SetToken(TEXT("object_name"), DefaultObjectName);

		// The bake name override has priority
		FString SMName = OutputObject.BakeName;
		if (SMName.IsEmpty())
		{
			// // ... finally the part name
			// if (FoundHGPO && FoundHGPO->bHasCustomPartName)
			// 	SMName = FoundHGPO->PartName;
			// else
			SMName = Resolver.ResolveOutputName();
			if (SMName.IsEmpty())
				SMName = DefaultObjectName;
		}

		FHoudiniPackageParams PackageParams;
		// Set the replace mode based on if we are doing a replacement or incremental asset bake
		const EPackageReplaceMode AssetPackageReplaceMode = bInReplaceAssets ?
			EPackageReplaceMode::ReplaceExistingAssets : EPackageReplaceMode::CreateNewAssets;
		FHoudiniEngineUtils::FillInPackageParamsForBakingOutput(
			PackageParams, Identifier, InBakeFolder.Path, SMName,
			InHoudiniAssetName, AssetPackageReplaceMode);

		const FName WorldOutlinerFolderPath = GetOutlinerFolderPath(OutputObject, FName(InFallbackWorldOutlinerFolder.IsEmpty() ? InHoudiniAssetName : InFallbackWorldOutlinerFolder));

		UWorld* DesiredWorld = InOutput ? InOutput->GetWorld() : GWorld;
		ULevel* DesiredLevel = GWorld->GetCurrentLevel();

		// See if this output object has an unreal_level_path attribute specified
		// In which case, we need to create/find the desired level for baking instead of using the current one
		bool bHasLevelPathAttribute = OutputObject.CachedAttributes.Contains(HAPI_UNREAL_ATTRIB_LEVEL_PATH);
		if (bHasLevelPathAttribute)
		{
			// Access some of the attribute that were cached on the output object
			// FHoudiniAttributeResolver Resolver;
			// const TMap<FString, FString>& CachedAttributes = OutputObject.CachedAttributes;
			TMap<FString, FString> Tokens = OutputObject.CachedTokens;
			PackageParams.UpdateTokensFromParams(DesiredWorld, Tokens);
			// Resolver.SetCachedAttributes(CachedAttributes);
			Resolver.SetTokensFromStringMap(Tokens);

			// Get the package path from the unreal_level_apth attribute
			FString LevelPackagePath = Resolver.ResolveFullLevelPath();

			bool bCreatedPackage = false;
			if (!FHoudiniEngineBakeUtils::FindOrCreateDesiredLevelFromLevelPath(
				LevelPackagePath,
				DesiredLevel,
				DesiredWorld,
				bCreatedPackage))
			{
				// TODO: LOG ERROR IF NO LEVEL
				continue;
			}

			// If we have created a level, add it to the packages to save
			// TODO: ? always add the level to the packages to save?
			if (bCreatedPackage && DesiredLevel)
			{
				// We can now save the package again, and unload it.
				OutPackagesToSave.Add(DesiredLevel->GetOutermost());
			}
		}

		// Bake the static mesh if it is still temporary
		UStaticMesh* BakedSM = FHoudiniEngineBakeUtils::DuplicateStaticMeshAndCreatePackageIfNeeded(
			StaticMesh,
			Cast<UStaticMesh>(BakedOutputObject.GetBakedObjectIfValid()),
			PackageParams,
			InAllOutputs,
			OutActors,
			InTempCookFolder.Path,
			OutPackagesToSave);

		if (!BakedSM || BakedSM->IsPendingKill())
			continue;

		// Record the baked object
		BakedOutputObject.BakedObject = FSoftObjectPath(BakedSM).ToString();

		// Make sure we have a level to spawn to
		if (!DesiredLevel || DesiredLevel->IsPendingKill())
			continue;

		// Try to find the unreal_bake_actor, if specified
		FName BakeActorName;
		AActor* FoundActor = nullptr;
		bool bHasBakeActorName = false;
		if (!FindUnrealBakeActor(OutputObject, BakedOutputObject, OutActors, DesiredLevel, *(PackageParams.ObjectName), bInReplaceActors, InFallbackActor, FoundActor, bHasBakeActorName, BakeActorName))
			return false;

		UStaticMeshComponent* SMC = nullptr;
		if (!FoundActor)
		{
			// Spawn the new actor
			FoundActor = Factory->CreateActor(BakedSM, DesiredLevel, InSMC->GetComponentTransform(), RF_Transactional);
			if (!FoundActor || FoundActor->IsPendingKill())
				continue;

			// Copy properties to new actor
			AStaticMeshActor* SMActor = Cast<AStaticMeshActor>(FoundActor);
			if (!SMActor || SMActor->IsPendingKill())
				continue;

			SMC = SMActor->GetStaticMeshComponent();
		}
		else
		{
			if (bInReplaceAssets)
			{
				// Check if we have a previous bake component and that it belongs to FoundActor, if so, reuse it
				UStaticMeshComponent* PrevSMC = Cast<UStaticMeshComponent>(BakedOutputObject.GetBakedComponentIfValid());
				if (IsValid(PrevSMC) && (PrevSMC->GetOwner() == FoundActor))
				{
					SMC = PrevSMC;
				}
			}

			const bool bCreateIfMissing = true;
			USceneComponent* RootComponent = GetActorRootComponent(FoundActor, bCreateIfMissing);

			if (!IsValid(SMC))
			{
				// Create a new static mesh component on the existing actor
				SMC = NewObject<UStaticMeshComponent>(FoundActor, NAME_None, RF_Transactional);

				FoundActor->AddInstanceComponent(SMC);
				if (IsValid(RootComponent))
					SMC->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
				else
					FoundActor->SetRootComponent(SMC);
				SMC->RegisterComponent();
			}
		}

		// We need to make a unique name for the actor, renaming an object on top of another is a fatal error
		const FName NewName = MakeUniqueObjectNameIfNeeded(DesiredLevel, Factory->NewActorClass, BakeActorName, FoundActor);
		const FString NewNameStr = NewName.ToString();
		// FoundActor->Rename(*NewNameStr);
		// FoundActor->SetActorLabel(NewNameStr);
		RenameAndRelabelActor(FoundActor, NewNameStr, false);
		SetOutlinerFolderPath(FoundActor, OutputObject, WorldOutlinerFolderPath);

		if (IsValid(SMC))
		{
			CopyPropertyToNewActorAndComponent(FoundActor, SMC, InSMC);
			SMC->SetStaticMesh(BakedSM);
			BakedOutputObject.BakedComponent = FSoftObjectPath(SMC).ToString();
		}
		
		BakedOutputObject.Actor = FSoftObjectPath(FoundActor).ToString();
		OutActors.Add(FHoudiniEngineBakedActor(
			FoundActor, BakeActorName, WorldOutlinerFolderPath, InOutputIndex, Identifier, BakedSM, StaticMesh));

		// If we are baking in replace mode, remove previously baked components/instancers
		if (bInReplaceActors && bInReplaceAssets)
		{
			const bool bInDestroyBakedComponent = false;
			const bool bInDestroyBakedInstancedActors = true;
			const bool bInDestroyBakedInstancedComponents = true;
			DestroyPreviousBakeOutput(
				BakedOutputObject, bInDestroyBakedComponent, bInDestroyBakedInstancedActors, bInDestroyBakedInstancedComponents);
		}
	}

	return true;
}

bool
FHoudiniEngineBakeUtils::BakeHoudiniCurveOutputToActors(
	UHoudiniOutput* Output,
	TMap<FHoudiniOutputObjectIdentifier, FHoudiniBakedOutputObject>& InBakedOutputObjects,
	const TArray<FHoudiniBakedOutput>& InAllBakedOutputs,
	const FString& InHoudiniAssetName,
	const FDirectoryPath& InBakeFolder,
	bool bInReplaceActors,
	bool bInReplaceAssets,
	TArray<FHoudiniEngineBakedActor>& OutActors,
	AActor* InFallbackActor,
	const FString& InFallbackWorldOutlinerFolder) 
{
	if (!Output || Output->IsPendingKill())
		return false;

	TArray<UPackage*> PackagesToSave;
	
	TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& OutputObjects = Output->GetOutputObjects();
	const TArray<FHoudiniGeoPartObject> & HGPOs = Output->GetHoudiniGeoPartObjects();

	for (auto & Pair : OutputObjects) 
	{
		FHoudiniOutputObject& OutputObject = Pair.Value;
		USplineComponent* SplineComponent = Cast<USplineComponent>(OutputObject.OutputComponent);
		if (!SplineComponent || SplineComponent->IsPendingKill())
			continue;
		
		FHoudiniOutputObjectIdentifier & Identifier = Pair.Key;
		FHoudiniBakedOutputObject& BakedOutputObject = InBakedOutputObjects.FindOrAdd(Identifier);

		// TODO: FIX ME!! May not work 100%
		const FHoudiniGeoPartObject* FoundHGPO = nullptr;
		for (auto & NextHGPO : HGPOs)
		{
			if (Identifier.GeoId == NextHGPO.GeoId &&
				Identifier.ObjectId == NextHGPO.ObjectId &&
				Identifier.PartId == NextHGPO.PartId)
			{
				FoundHGPO = &NextHGPO;
				break;
			}
		}

		if (!FoundHGPO)
			continue;

		FString CurveName = Pair.Value.BakeName;
		if (CurveName.IsEmpty())
		{
			if (FoundHGPO->bHasCustomPartName)
				CurveName = FoundHGPO->PartName;
			else
				CurveName = InHoudiniAssetName + "_" + SplineComponent->GetName();
		}		

		FHoudiniPackageParams PackageParams;
		// Set the replace mode based on if we are doing a replacement or incremental asset bake
		const EPackageReplaceMode AssetPackageReplaceMode = bInReplaceAssets ?
			EPackageReplaceMode::ReplaceExistingAssets : EPackageReplaceMode::CreateNewAssets;
		FHoudiniEngineUtils::FillInPackageParamsForBakingOutput(
			PackageParams, Identifier, InBakeFolder.Path, CurveName,
			InHoudiniAssetName, AssetPackageReplaceMode);

		BakeCurve(OutputObject, BakedOutputObject, PackageParams, bInReplaceActors, bInReplaceAssets, OutActors,
			PackagesToSave, InFallbackActor, InFallbackWorldOutlinerFolder);
	}

	SaveBakedPackages(PackagesToSave);

	return true;
}

bool 
FHoudiniEngineBakeUtils::CopyActorContentsToBlueprint(AActor * InActor, UBlueprint * OutBlueprint) 
{
	if (!InActor || InActor->IsPendingKill())
		return false;

	if (!OutBlueprint || OutBlueprint->IsPendingKill())
		return false;

	if (InActor->GetInstanceComponents().Num() > 0)
		FKismetEditorUtilities::AddComponentsToBlueprint(
			OutBlueprint,
			InActor->GetInstanceComponents());

	if (OutBlueprint->GeneratedClass)
	{
		AActor * CDO = Cast< AActor >(OutBlueprint->GeneratedClass->GetDefaultObject());
		if (!CDO || CDO->IsPendingKill())
			return false;

		const auto CopyOptions = (EditorUtilities::ECopyOptions::Type)
			(EditorUtilities::ECopyOptions::OnlyCopyEditOrInterpProperties |
				EditorUtilities::ECopyOptions::PropagateChangesToArchetypeInstances);

		EditorUtilities::CopyActorProperties(InActor, CDO, CopyOptions);

		USceneComponent * Scene = CDO->GetRootComponent();
		if (Scene && !Scene->IsPendingKill())
		{
			Scene->SetRelativeLocation(FVector::ZeroVector);
			Scene->SetRelativeRotation(FRotator::ZeroRotator);

			// Clear out the attachment info after having copied the properties from the source actor
			Scene->SetupAttachment(nullptr);
			while (true)
			{
				const int32 ChildCount = Scene->GetAttachChildren().Num();
				if (ChildCount < 1)
					break;

				USceneComponent * Component = Scene->GetAttachChildren()[ChildCount - 1];
				if (Component && !Component->IsPendingKill())
					Component->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
			}
			check(Scene->GetAttachChildren().Num() == 0);

			// Ensure the light mass information is cleaned up
			Scene->InvalidateLightingCache();

			// Copy relative scale from source to target.
			if (USceneComponent* SrcSceneRoot = InActor->GetRootComponent())
			{
				Scene->SetRelativeScale3D_Direct(SrcSceneRoot->GetRelativeScale3D());
			}
		}
	}

	// Compile our blueprint and notify asset system about blueprint.
	//FKismetEditorUtilities::CompileBlueprint(OutBlueprint);
	//FAssetRegistryModule::AssetCreated(OutBlueprint);

	return true;
}

bool 
FHoudiniEngineBakeUtils::BakeBlueprints(UHoudiniAssetComponent* HoudiniAssetComponent, bool bInReplaceAssets) 
{
	FHoudiniEngineOutputStats BakeStats;
	TArray<UPackage*> PackagesToSave;
	TArray<UBlueprint*> Blueprints;
	const bool bSuccess = BakeBlueprints(HoudiniAssetComponent, bInReplaceAssets, BakeStats, Blueprints, PackagesToSave);
	if (!bSuccess)
	{
		// TODO: ?
		HOUDINI_LOG_WARNING(TEXT("Errors while baking to blueprints."));
	}

	FHoudiniEngineBakeUtils::SaveBakedPackages(PackagesToSave);

	// Sync the CB to the baked objects
	if(GEditor && Blueprints.Num() > 0)
	{
		TArray<UObject*> Assets;
		Assets.Reserve(Blueprints.Num());
		for (UBlueprint* Blueprint : Blueprints)
		{
			Assets.Add(Blueprint);
		}
		GEditor->SyncBrowserToObjects(Assets);
	}

	{
		const FString FinishedTemplate = TEXT("Baking finished. Created {0} packages. Updated {1} packages.");
		FString Msg = FString::Format(*FinishedTemplate, { BakeStats.NumPackagesCreated, BakeStats.NumPackagesUpdated } );
		FHoudiniEngine::Get().FinishTaskSlateNotification( FText::FromString(Msg) );
	}
	
	TryCollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);

	return bSuccess;
}

bool 
FHoudiniEngineBakeUtils::BakeBlueprints(
	UHoudiniAssetComponent* HoudiniAssetComponent,
	bool bInReplaceAssets,
	FHoudiniEngineOutputStats& InBakeStats,
	TArray<UBlueprint*>& OutBlueprints,
	TArray<UPackage*>& OutPackagesToSave)
{
	if (!HoudiniAssetComponent || HoudiniAssetComponent->IsPendingKill())
		return false;

	AActor* OwnerActor = HoudiniAssetComponent->GetOwner();
	const bool bIsOwnerActorValid = IsValid(OwnerActor);
	
	TArray<FHoudiniEngineBakedActor> Actors;

	// Don't process outputs that are not supported in blueprints
	TArray<EHoudiniOutputType> OutputsToBake = {
		EHoudiniOutputType::Mesh,
		EHoudiniOutputType::Instancer,
		EHoudiniOutputType::Curve
	};
	TArray<EHoudiniInstancerComponentType> InstancerComponentTypesToBake = {
		EHoudiniInstancerComponentType::StaticMeshComponent,
		EHoudiniInstancerComponentType::InstancedStaticMeshComponent,
		EHoudiniInstancerComponentType::MeshSplitInstancerComponent,
	};
	// When baking blueprints we always create new actors since they are deleted from the world once copied into the
	// blueprint
	const bool bReplaceActors = false;
	bool bBakeSuccess = BakeHoudiniActorToActors(
		HoudiniAssetComponent,
		bReplaceActors,
		bInReplaceAssets,
		Actors,
		OutPackagesToSave,
		InBakeStats,
		&OutputsToBake,
		&InstancerComponentTypesToBake);
	if (!bBakeSuccess)
	{
		HOUDINI_LOG_ERROR(TEXT("Could not create output actors for baking to blueprint."));
		return false;
	}

	// Get the previous baked outputs
	TArray<FHoudiniBakedOutput>& BakedOutputs = HoudiniAssetComponent->GetBakedOutputs();

	bBakeSuccess = BakeBlueprintsFromBakedActors(
		Actors,
		HoudiniAssetComponent->bRecenterBakedActors,
		bInReplaceAssets,
		bIsOwnerActorValid ? OwnerActor->GetName() : FString(),
		HoudiniAssetComponent->BakeFolder,
		&BakedOutputs,
		nullptr,
		OutBlueprints,
		OutPackagesToSave);

	return bBakeSuccess;
}

UStaticMesh* 
FHoudiniEngineBakeUtils::BakeStaticMesh(
	UStaticMesh * StaticMesh,
	const FHoudiniPackageParams& PackageParams,
	const TArray<UHoudiniOutput*>& InAllOutputs,
	const FDirectoryPath& InTempCookFolder) 
{
	if (!StaticMesh || StaticMesh->IsPendingKill())
		return nullptr;

	TArray<UPackage*> PackagesToSave;
	TArray<UHoudiniOutput*> Outputs;
	const TArray<FHoudiniEngineBakedActor> BakedResults;
	UStaticMesh* BakedStaticMesh = FHoudiniEngineBakeUtils::DuplicateStaticMeshAndCreatePackageIfNeeded(
		StaticMesh, nullptr, PackageParams, InAllOutputs, BakedResults, InTempCookFolder.Path, PackagesToSave);

	if (BakedStaticMesh) 
	{
		FHoudiniEngineBakeUtils::SaveBakedPackages(PackagesToSave);

		// Sync the CB to the baked objects
		if(GEditor)
		{
			TArray<UObject*> Objects;
			Objects.Add(BakedStaticMesh);
			GEditor->SyncBrowserToObjects(Objects);
		}
	}

	return BakedStaticMesh;
}

bool
FHoudiniEngineBakeUtils::BakeLandscape(
	int32 InOutputIndex,
	UHoudiniOutput* InOutput,
	TMap<FHoudiniOutputObjectIdentifier, FHoudiniBakedOutputObject>& InBakedOutputObjects,
	bool bInReplaceActors,
	bool bInReplaceAssets,
	FString BakePath,
	FString HoudiniAssetName,
	FHoudiniEngineOutputStats& BakeStats
	)
{
	if (!IsValid(InOutput))
		return false;
	
	TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& OutputObjects = InOutput->GetOutputObjects();
	TArray<UPackage*> PackagesToSave;
	TArray<UWorld*> LandscapeWorldsToUpdate;

	FHoudiniPackageParams PackageParams;
	
	for (auto& Elem : OutputObjects)
	{
		const FHoudiniOutputObjectIdentifier& ObjectIdentifier = Elem.Key;
		FHoudiniOutputObject& OutputObject = Elem.Value;
		FHoudiniBakedOutputObject& BakedOutputObject = InBakedOutputObjects.FindOrAdd(ObjectIdentifier);
		
		// Populate the package params for baking this output object.
		if (!IsValid(OutputObject.OutputObject))
			continue;

		if (!OutputObject.OutputObject->IsA<UHoudiniLandscapePtr>())
			continue;

		UHoudiniLandscapePtr* LandscapePtr = Cast<UHoudiniLandscapePtr>(OutputObject.OutputObject);
		ALandscapeProxy* Landscape = LandscapePtr->GetRawPtr();
		if (!IsValid(Landscape))
			continue;

		FString ObjectName = Landscape->GetName();

		// Set the replace mode based on if we are doing a replacement or incremental asset bake
		const EPackageReplaceMode AssetPackageReplaceMode = bInReplaceAssets ?
			EPackageReplaceMode::ReplaceExistingAssets : EPackageReplaceMode::CreateNewAssets;
		FHoudiniEngineUtils::FillInPackageParamsForBakingOutput(
			PackageParams,
			ObjectIdentifier,
			BakePath,
			ObjectName,
			HoudiniAssetName,
			AssetPackageReplaceMode
		);

		BakeLandscapeObject(OutputObject, BakedOutputObject, bInReplaceActors, bInReplaceAssets,
			PackageParams, LandscapeWorldsToUpdate, PackagesToSave, BakeStats);
	}

	if (PackagesToSave.Num() > 0)
	{
		FEditorFileUtils::PromptForCheckoutAndSave(PackagesToSave, true, false);
	}

	for(UWorld* LandscapeWorld : LandscapeWorldsToUpdate)
	{
		if (!LandscapeWorld)
			continue;
		FHoudiniEngineUtils::RescanWorldPath(LandscapeWorld);
		ULandscapeInfo::RecreateLandscapeInfo(LandscapeWorld, true);
		if (LandscapeWorld->WorldComposition)
		{
			UWorldComposition::WorldCompositionChangedEvent.Broadcast(LandscapeWorld);
		}
	}

	if (PackagesToSave.Num() > 0)
	{
		// These packages were either created during the Bake process or they weren't
		// loaded in the first place so be sure to unload them again to preserve their "state".
		
		TArray<UPackage*> PackagesToUnload;
		for (UPackage* Package : PackagesToSave)
		{
			if (!Package->IsDirty())
				PackagesToUnload.Add(Package);
		}
		UPackageTools::UnloadPackages(PackagesToUnload);
	}

#if WITH_EDITOR
	FEditorDelegates::RefreshLevelBrowser.Broadcast();
	FEditorDelegates::RefreshAllBrowsers.Broadcast();
#endif

	return true;
}

bool
FHoudiniEngineBakeUtils::BakeLandscapeObject(
	FHoudiniOutputObject& InOutputObject,
	FHoudiniBakedOutputObject& InBakedOutputObject,
	bool bInReplaceActors,
	bool bInReplaceAssets,
	FHoudiniPackageParams& PackageParams,
	TArray<UWorld*>& WorldsToUpdate,
	TArray<UPackage*>& OutPackagesToSave,
	FHoudiniEngineOutputStats& BakeStats)
{
	UHoudiniLandscapePtr* LandscapePointer = Cast<UHoudiniLandscapePtr>(InOutputObject.OutputObject);
	if (!LandscapePointer)
		return false;
	
	ALandscapeProxy* TileActor = LandscapePointer->GetRawPtr();
	if (!TileActor)
		return false;

	// Fetch the previous bake's pointer and proxy (if available)
	ALandscapeProxy* PreviousTileActor = Cast<ALandscapeProxy>(InBakedOutputObject.GetBakedObjectIfValid());
	
	UWorld* TileWorld = TileActor->GetWorld();
	ULevel* TileLevel = TileActor->GetLevel();

	ULandscapeInfo::RecreateLandscapeInfo(TileWorld, true);

	// At this point we reconstruct the resolver using cached attributes and tokens
	// and just update certain tokens (output paths) for bake mode.
	FHoudiniAttributeResolver Resolver;
	{
		TMap<FString,FString> Tokens = InOutputObject.CachedTokens;
		// PackageParams.UpdateOutputPathTokens(EPackageMode::Bake, Tokens);
		PackageParams.UpdateTokensFromParams(TileWorld, Tokens);
		Resolver.SetCachedAttributes(InOutputObject.CachedAttributes);
		Resolver.SetTokensFromStringMap(Tokens);
	}

	// If this actor has a shared landscape, ensure the shared landscape gets detached from the HAC
	// and has the appropriate name.
	ALandscape* SharedLandscapeActor = TileActor->GetLandscapeActor();
	check(SharedLandscapeActor);

	// Fetch the previous bake's shared landscape actor (if available)
	ALandscape* PreviousSharedLandscapeActor = nullptr;
	if (IsValid(PreviousTileActor))
		PreviousSharedLandscapeActor = PreviousTileActor->GetLandscapeActor();
	
	const bool bHasSharedLandscape = SharedLandscapeActor != TileActor;
	const bool bHasPreviousSharedLandscape = PreviousSharedLandscapeActor && PreviousSharedLandscapeActor != PreviousTileActor;
	bool bLandscapeReplaced = false;
	if (bHasSharedLandscape)
	{
		// If we are baking in replace mode and we have a previous shared landscape actor, use the name of that
		// actor
		const FString DesiredSharedLandscapeName = bHasPreviousSharedLandscape && bInReplaceActors
			? PreviousSharedLandscapeActor->GetName()
			: Resolver.ResolveAttribute(
				HAPI_UNREAL_ATTRIB_LANDSCAPE_SHARED_ACTOR_NAME,
				SharedLandscapeActor->GetName());

		// If we are not baking in replacement mode, create a unique name if the name is already in use
		const FString SharedLandscapeName = !bInReplaceActors
			? MakeUniqueObjectNameIfNeeded(SharedLandscapeActor->GetOuter(), SharedLandscapeActor->GetClass(), *DesiredSharedLandscapeName).ToString()
			: DesiredSharedLandscapeName;
		
		if (SharedLandscapeActor->GetName() != SharedLandscapeName)
		{
			AActor* FoundActor = nullptr;
			ALandscape* ExistingLandscape = FHoudiniEngineUtils::FindOrRenameInvalidActor<ALandscape>(TileWorld, SharedLandscapeName, FoundActor);
			if (ExistingLandscape && bInReplaceActors)
			{
				// Even though we found an existing landscape with the desired type, we're just going to destroy/replace
				// it for now.
				FHoudiniEngineUtils::RenameToUniqueActor(ExistingLandscape, SharedLandscapeName+"_0");
				ExistingLandscape->Destroy();
				bLandscapeReplaced = true;
			}

			// Fix name of shared landscape
			FHoudiniEngineUtils::SafeRenameActor(SharedLandscapeActor, *SharedLandscapeName);
		}
		
		SharedLandscapeActor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		WorldsToUpdate.AddUnique(SharedLandscapeActor->GetWorld());
	}

	// Find the world where the landscape tile should be placed.

	TArray<ALandscapeProxy*> ValidLandscapes;

	FString ActorName = Resolver.ResolveOutputName();

	// If the unreal_level_path was not specified, then fallback to the tile world's package
	FString PackagePath = TileWorld->GetPackage() ? TileWorld->GetPackage()->GetPathName() : FString();
	bool bHasLevelPathAttribute = InOutputObject.CachedAttributes.Contains(HAPI_UNREAL_ATTRIB_LEVEL_PATH);
	if (bHasLevelPathAttribute)
		PackagePath = Resolver.ResolveFullLevelPath();
	
	if (bInReplaceActors)
	{
		// If we are baking in replace mode: get the previous baked actor (if available) name, but only if it is in the
		// same target level
		if (IsValid(PreviousTileActor))
		{
			UPackage* PreviousPackage = PreviousTileActor->GetPackage();
			if (IsValid(PreviousPackage) && PreviousPackage->GetPathName() == PackagePath)
			{
				ActorName = PreviousTileActor->GetName();
			}
		}
	}

	bool bCreatedPackage = false;
	UWorld* TargetWorld = nullptr;
	ULevel* TargetLevel = nullptr;
	ALandscapeProxy* TargetActor = FHoudiniLandscapeTranslator::FindExistingLandscapeActor_Bake(
		TileActor->GetWorld(),
		nullptr, //unused in bake mode
		ValidLandscapes,//unused in bake mode
		-1, //unused in bake mode
		-1, //unused in bake mode
		ActorName,
		PackagePath,
		TargetWorld,
		TargetLevel,
		bCreatedPackage
		);

	check(TargetLevel)
	check(TargetWorld)
	
	if (TargetActor && TargetActor != TileActor)
	{
		if (bInReplaceActors && (!PreviousTileActor || PreviousTileActor == TargetActor))
		{
			// We found an target matching the name that we want. For now, rename it and then nuke it, so that
			// at the very least we can spawn a new actor with the desired name. At a later stage we'll implement
			// a content update, if possible.
			FHoudiniEngineUtils::RenameToUniqueActor(TargetActor, ActorName + TEXT("_0"));
			TargetActor->Destroy();
		}
		else
		{
			// incremental, keep existing actor and create a unique name for the new one
			ActorName = MakeUniqueObjectName(TargetActor->GetOuter(), TargetActor->GetClass(), *ActorName).ToString();
		}
		TargetActor = nullptr;
	}

	if (TargetLevel != TileActor->GetLevel())
	{
		bool bLevelInWorld = TileWorld->ContainsLevel(TargetLevel);
		ALandscape* SharedLandscape = TileActor->GetLandscapeActor();
		ULandscapeInfo* LandscapeInfo = TileActor->GetLandscapeInfo();
		
		check(LandscapeInfo);
		
		// We can now move the current landscape to the new world / level
		// if (TileActor->GetClass()->IsChildOf<ALandscapeStreamingProxy>())
		{
			// We can only move streaming proxies to sublevels for now.
			TArray<AActor*> ActorsToMove = {TileActor};

			ALandscapeProxy* NewLandscapeProxy = LandscapeInfo->MoveComponentsToLevel(TileActor->LandscapeComponents, TargetLevel);
			// We have now moved the landscape components into the new level. We can (hopefully) safely delete the
			// old tile actor.
			TileActor->Destroy();

			TargetLevel->MarkPackageDirty();

			TileActor = NewLandscapeProxy;
		}
	}
	else
	{
		// Ensure the landscape actor is detached.
		TileActor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	}

	// Ensure the tile actor has the desired name.
	FHoudiniEngineUtils::SafeRenameActor(TileActor, ActorName);

	if (TileActor->GetClass()->IsChildOf(ALandscape::StaticClass()))
	{
		// This is not a shared landscape. Be sure to update this landscape's world when
		// baking is done.
		WorldsToUpdate.AddUnique(TileActor->GetWorld());
	}

	if (bCreatedPackage)
	{
		// We can now save the package again, and unload it.		
		OutPackagesToSave.Add(TargetLevel->GetOutermost());
	}

	// Record the landscape in the baked output object via a new UHoudiniLandscapePtr
	// UHoudiniLandscapePtr* BakedLandscapePtr = NewObject<UHoudiniLandscapePtr>(LandscapePointer->GetOuter());
	// if (IsValid(BakedLandscapePtr))
	// {
	// 	BakedLandscapePtr->SetSoftPtr(TileActor);
		InBakedOutputObject.BakedObject = FSoftObjectPath(TileActor).ToString();
	// }
	// else
	// {
	// 	InBakedOutputObject.BakedObject = nullptr;
	// }

	// Remove the landscape from the InOutputObject since it should no longer be used/reused/updated by temp cooks
	InOutputObject.OutputObject = nullptr;
	
	DestroyPreviousBakeOutput(InBakedOutputObject, true, true, true);

	// ----------------------------------------------------
	// Collect baking stats
	// ----------------------------------------------------
	if (bLandscapeReplaced)
		BakeStats.NotifyObjectsReplaced(EHoudiniOutputType::Landscape, 1);
	else
		BakeStats.NotifyObjectsCreated(EHoudiniOutputType::Landscape, 1);

	if (bCreatedPackage)
		BakeStats.NotifyPackageCreated(1);
	else
		if (TileLevel != TargetLevel)
			BakeStats.NotifyPackageUpdated(1);

	return true;
}

UStaticMesh * 
FHoudiniEngineBakeUtils::DuplicateStaticMeshAndCreatePackageIfNeeded(
	UStaticMesh * InStaticMesh,
	UStaticMesh * InPreviousBakeStaticMesh,
	const FHoudiniPackageParams &PackageParams,
	const TArray<UHoudiniOutput*>& InParentOutputs, 
	const TArray<FHoudiniEngineBakedActor>& InCurrentBakedActors,
	const FString& InTemporaryCookFolder,
	TArray<UPackage*> & OutCreatedPackages) 
{
	if (!InStaticMesh || InStaticMesh->IsPendingKill())
		return nullptr;

	bool bIsTemporaryStaticMesh = IsObjectTemporary(InStaticMesh, InParentOutputs, InTemporaryCookFolder);
	if (!bIsTemporaryStaticMesh)
	{
		// The Static Mesh is not a temporary one/already baked, we can simply reuse it
		// instead of duplicating it
		return InStaticMesh;
	}

	// Look for InStaticMesh as the SourceObject in InCurrentBakedActors (it could have already been baked along with
	// a previous output: instancers etc)
	for (const FHoudiniEngineBakedActor& BakedActor : InCurrentBakedActors)
	{
		if (BakedActor.SourceObject == InStaticMesh && IsValid(BakedActor.BakedObject)
			&& BakedActor.BakedObject->IsA(InStaticMesh->GetClass()))
		{
			// We have found a bake result where InStaticMesh was the source object and we have a valid BakedObject
			// of a compatible class
			return Cast<UStaticMesh>(BakedActor.BakedObject);
		}
	}

	// InStaticMesh is temporary and we didn't find a baked version of it in our current bake output, we need to bake it
	
	// If we have a previously baked static mesh, get the bake counter from it so that both replace and increment
	// is consistent with the bake counter
	int32 BakeCounter = 0;
	bool bPreviousBakeStaticMeshValid = IsValid(InPreviousBakeStaticMesh);
	TArray<FStaticMaterial> PreviousBakeMaterials;
	if (bPreviousBakeStaticMeshValid)
	{
		bPreviousBakeStaticMeshValid = PackageParams.MatchesPackagePathNameExcludingBakeCounter(InPreviousBakeStaticMesh);
		if (bPreviousBakeStaticMeshValid)
		{
			PackageParams.GetBakeCounterFromBakedAsset(InPreviousBakeStaticMesh, BakeCounter);
			PreviousBakeMaterials = InPreviousBakeStaticMesh->StaticMaterials;//GetStaticMaterials();
		}
	}
	FString CreatedPackageName;
	UPackage* MeshPackage = PackageParams.CreatePackageForObject(CreatedPackageName, BakeCounter);
	if (!MeshPackage || MeshPackage->IsPendingKill())
		return nullptr;

	OutCreatedPackages.Add(MeshPackage);

	// We need to be sure the package has been fully loaded before calling DuplicateObject
	if (!MeshPackage->IsFullyLoaded())
	{
		FlushAsyncLoading();
		if (!MeshPackage->GetOuter())
		{
			MeshPackage->FullyLoad();
		}
		else
		{
			MeshPackage->GetOutermost()->FullyLoad();
		}
	}

	// If the a UStaticMesh with that name already exists then detach it from all of its components before replacing
	// it so that its render resources can be safely replaced/updated, and then reattach it
	UStaticMesh * DuplicatedStaticMesh = nullptr;
	UStaticMesh* ExistingMesh = FindObject<UStaticMesh>(MeshPackage, *CreatedPackageName);
	if (IsValid(ExistingMesh))
	{
		FStaticMeshComponentRecreateRenderStateContext SMRecreateContext(ExistingMesh);	
		DuplicatedStaticMesh = DuplicateObject<UStaticMesh>(InStaticMesh, MeshPackage, *CreatedPackageName);
	}
	else
	{
		DuplicatedStaticMesh = DuplicateObject<UStaticMesh>(InStaticMesh, MeshPackage, *CreatedPackageName);
	}
	
	if (!DuplicatedStaticMesh || DuplicatedStaticMesh->IsPendingKill())
		return nullptr;

	// Add meta information.
	FHoudiniEngineBakeUtils::AddHoudiniMetaInformationToPackage(
		MeshPackage, DuplicatedStaticMesh,
		HAPI_UNREAL_PACKAGE_META_GENERATED_OBJECT, TEXT("true"));
	FHoudiniEngineBakeUtils::AddHoudiniMetaInformationToPackage(
		MeshPackage, DuplicatedStaticMesh,
		HAPI_UNREAL_PACKAGE_META_GENERATED_NAME, *CreatedPackageName);

	// See if we need to duplicate materials and textures.
	TArray<FStaticMaterial>DuplicatedMaterials;
	TArray<FStaticMaterial>& Materials = DuplicatedStaticMesh->StaticMaterials;
	for (int32 MaterialIdx = 0; MaterialIdx < Materials.Num(); ++MaterialIdx)
	{
		UMaterialInterface* MaterialInterface = Materials[MaterialIdx].MaterialInterface;
		if (!MaterialInterface || MaterialInterface->IsPendingKill())
			continue;

		// Only duplicate the material if it is temporary
		if (IsObjectTemporary(MaterialInterface, InParentOutputs, InTemporaryCookFolder))
		{
			UPackage * MaterialPackage = Cast<UPackage>(MaterialInterface->GetOuter());
			if (MaterialPackage && !MaterialPackage->IsPendingKill())
			{
				FString MaterialName;
				if (FHoudiniEngineBakeUtils::GetHoudiniGeneratedNameFromMetaInformation(
					MeshPackage, DuplicatedStaticMesh, MaterialName))
				{
					MaterialName = MaterialName + "_Material" + FString::FromInt(MaterialIdx + 1);

					// We only deal with materials.
					UMaterial * Material = Cast< UMaterial >(MaterialInterface);
					if (Material && !Material->IsPendingKill())
					{
						// Look for a previous bake material at this index
						UMaterial* PreviousBakeMaterial = nullptr;
						if (bPreviousBakeStaticMeshValid && PreviousBakeMaterials.IsValidIndex(MaterialIdx))
						{
							PreviousBakeMaterial = Cast<UMaterial>(PreviousBakeMaterials[MaterialIdx].MaterialInterface);
						}
						// Duplicate material resource.
						UMaterial * DuplicatedMaterial = FHoudiniEngineBakeUtils::DuplicateMaterialAndCreatePackage(
							Material, PreviousBakeMaterial, MaterialName, PackageParams, OutCreatedPackages);

						if (!DuplicatedMaterial || DuplicatedMaterial->IsPendingKill())
							continue;

						// Store duplicated material.
						FStaticMaterial DupeStaticMaterial = Materials[MaterialIdx];
						DupeStaticMaterial.MaterialInterface = DuplicatedMaterial;
						DuplicatedMaterials.Add(DupeStaticMaterial);
						continue;
					}
				}
			}
		}
		
		// We can simply reuse the source material
		DuplicatedMaterials.Add(Materials[MaterialIdx]);
	}
		
	// Assign duplicated materials.
	DuplicatedStaticMesh->StaticMaterials = DuplicatedMaterials;

	// Notify registry that we have created a new duplicate mesh.
	FAssetRegistryModule::AssetCreated(DuplicatedStaticMesh);

	// Dirty the static mesh package.
	DuplicatedStaticMesh->MarkPackageDirty();

	return DuplicatedStaticMesh;
}

ALandscapeProxy* 
FHoudiniEngineBakeUtils::BakeHeightfield(
	ALandscapeProxy * InLandscapeProxy,
	const FHoudiniPackageParams & PackageParams,
	const EHoudiniLandscapeOutputBakeType & LandscapeOutputBakeType)
{
	if (!InLandscapeProxy || InLandscapeProxy->IsPendingKill())
		return nullptr;

	const FString & BakeFolder = PackageParams.BakeFolder;
	const FString & AssetName = PackageParams.HoudiniAssetName;

	switch (LandscapeOutputBakeType) 
	{
		case EHoudiniLandscapeOutputBakeType::Detachment:
		{
			// Detach the landscape from the Houdini Asset Actor
			InLandscapeProxy->DetachFromActor(FDetachmentTransformRules::KeepRelativeTransform);
		}
		break;
		case EHoudiniLandscapeOutputBakeType::BakeToImage:
		{
			// Create heightmap image to the bake folder
			ULandscapeInfo * InLandscapeInfo = InLandscapeProxy->GetLandscapeInfo();
			if (!InLandscapeInfo || InLandscapeInfo->IsPendingKill())
				return nullptr;
		
			// bake to image must use absoluate path, 
			// and the file name has a file extension (.png)
			FString BakeFolderInFullPath = BakeFolder;

			// Figure absolute path,
			if (!BakeFolderInFullPath.EndsWith("/"))
				BakeFolderInFullPath += "/";

			if (BakeFolderInFullPath.StartsWith("/Game"))
				BakeFolderInFullPath = BakeFolderInFullPath.Mid(5, BakeFolderInFullPath.Len() - 5);

			if (BakeFolderInFullPath.StartsWith("/"))
				BakeFolderInFullPath = BakeFolderInFullPath.Mid(1, BakeFolderInFullPath.Len() - 1);

			FString FullPath = FPaths::ProjectContentDir() + BakeFolderInFullPath + AssetName + "_" + InLandscapeProxy->GetName() + ".png";

			InLandscapeInfo->ExportHeightmap(FullPath);

			// TODO:
			// We should update this to have an asset/package..
		}
		break;
		case EHoudiniLandscapeOutputBakeType::BakeToWorld:
		{
			ULandscapeInfo * InLandscapeInfo = InLandscapeProxy->GetLandscapeInfo();
			if (!InLandscapeInfo || InLandscapeInfo->IsPendingKill())
				return nullptr;

			// 0.  Get Landscape Data //
			
			// Extract landscape height data
			TArray<uint16> InLandscapeHeightData;
			int32 XSize, YSize;
			FVector Min, Max;
			if (!FUnrealLandscapeTranslator::GetLandscapeData(InLandscapeProxy, InLandscapeHeightData, XSize, YSize, Min, Max))
				return nullptr;

			// Extract landscape Layers data
			TArray<FLandscapeImportLayerInfo> InLandscapeImportLayerInfos;
			for (int32 n = 0; n < InLandscapeInfo->Layers.Num(); ++n) 
			{
				TArray<uint8> CurrentLayerIntData;
				FLinearColor LayerUsageDebugColor;
				FString LayerName;
				if (!FUnrealLandscapeTranslator::GetLandscapeLayerData(InLandscapeInfo, n, CurrentLayerIntData, LayerUsageDebugColor, LayerName))
					continue;

				FLandscapeImportLayerInfo CurrentLayerInfo;
				CurrentLayerInfo.LayerName = FName(LayerName);
				CurrentLayerInfo.LayerInfo = InLandscapeInfo->Layers[n].LayerInfoObj;
				CurrentLayerInfo.LayerData = CurrentLayerIntData;

				CurrentLayerInfo.LayerInfo->LayerUsageDebugColor = LayerUsageDebugColor;

				InLandscapeImportLayerInfos.Add(CurrentLayerInfo);
			}

			// 1. Create package  //

			FString PackagePath = PackageParams.GetPackagePath();
			FString PackageName = PackageParams.GetPackageName();

			UPackage *CreatedPackage = nullptr;
			FString CreatedPackageName;

			CreatedPackage = PackageParams.CreatePackageForObject(CreatedPackageName);

			if (!CreatedPackage)
				return nullptr;

			// 2. Create a new world asset with dialog //
			UWorldFactory* Factory = NewObject<UWorldFactory>();
			FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");

			UObject* Asset = AssetToolsModule.Get().CreateAssetWithDialog(
				PackageName, PackagePath,
				UWorld::StaticClass(), Factory, FName("ContentBrowserNewAsset"));


			UWorld* NewWorld = Cast<UWorld>(Asset);
			if (!NewWorld)
				return nullptr;

			NewWorld->SetCurrentLevel(NewWorld->PersistentLevel);

			// 4. Spawn a landscape proxy actor in the created world
			ALandscapeStreamingProxy * BakedLandscapeProxy = NewWorld->SpawnActor<ALandscapeStreamingProxy>();
			if (!BakedLandscapeProxy)
				return nullptr;

			// Create a new GUID
			FGuid currentGUID = FGuid::NewGuid();
			BakedLandscapeProxy->SetLandscapeGuid(currentGUID);

			// Deactivate CastStaticShadow on the landscape to avoid "grid shadow" issue
			BakedLandscapeProxy->bCastStaticShadow = false;
			

			// 5. Import data to the created landscape proxy
			TMap<FGuid, TArray<uint16>> HeightmapDataPerLayers;
			TMap<FGuid, TArray<FLandscapeImportLayerInfo>> MaterialLayerDataPerLayer;

			HeightmapDataPerLayers.Add(FGuid(), InLandscapeHeightData);
			MaterialLayerDataPerLayer.Add(FGuid(), InLandscapeImportLayerInfos);

			ELandscapeImportAlphamapType ImportLayerType = ELandscapeImportAlphamapType::Additive;

			BakedLandscapeProxy->Import(
				currentGUID,
				0, 0, XSize-1, YSize-1,
				InLandscapeInfo->ComponentNumSubsections, InLandscapeInfo->SubsectionSizeQuads,
				HeightmapDataPerLayers, NULL,
				MaterialLayerDataPerLayer, ImportLayerType);

			BakedLandscapeProxy->StaticLightingLOD = FMath::DivideAndRoundUp(FMath::CeilLogTwo((XSize * YSize) / (2048 * 2048) + 1), (uint32)2);

	
			if (BakedLandscapeProxy->LandscapeMaterial)
				BakedLandscapeProxy->LandscapeMaterial = InLandscapeProxy->LandscapeMaterial;

			if (BakedLandscapeProxy->LandscapeHoleMaterial)
				BakedLandscapeProxy->LandscapeHoleMaterial = InLandscapeProxy->LandscapeHoleMaterial;

			// 6. Register all the landscape components, and set landscape actor transform
			BakedLandscapeProxy->RegisterAllComponents();
			BakedLandscapeProxy->SetActorTransform(InLandscapeProxy->GetTransform());

			// 7. Save Package
			TArray<UPackage*> PackagesToSave;
			PackagesToSave.Add(CreatedPackage);
			FHoudiniEngineBakeUtils::SaveBakedPackages(PackagesToSave);

			// Sync the CB to the baked objects
			if(GEditor)
			{
				TArray<UObject*> Objects;
				Objects.Add(NewWorld);
				GEditor->SyncBrowserToObjects(Objects);
			}
		}
		break;
	}

	return InLandscapeProxy;
}

bool
FHoudiniEngineBakeUtils::BakeCurve(
	USplineComponent* InSplineComponent,
	ULevel* InLevel,
	const FHoudiniPackageParams &PackageParams,
	AActor*& OutActor,
	USplineComponent*& OutSplineComponent,
	FName InOverrideFolderPath,
	AActor* InActor)
{
	if (!IsValid(InActor))
	{
		UActorFactory* Factory = GEditor ? GEditor->FindActorFactoryByClass(UActorFactoryEmptyActor::StaticClass()) : nullptr;
		if (!Factory)
			return false;

		OutActor = Factory->CreateActor(nullptr, InLevel, InSplineComponent->GetComponentTransform(), RF_Transactional);
	}
	else
	{
		OutActor = InActor;
	}

	// The default name will be based on the static mesh package, we would prefer it to be based on the Houdini asset
	const FName BaseActorName(PackageParams.ObjectName);
	const FName NewName = MakeUniqueObjectNameIfNeeded(InLevel, OutActor->GetClass(), BaseActorName, OutActor);
	const FString NewNameStr = NewName.ToString();
	// OutActor->Rename(*NewNameStr);
	// OutActor->SetActorLabel(NewNameStr);
	RenameAndRelabelActor(OutActor, NewNameStr, false);
	OutActor->SetFolderPath(InOverrideFolderPath.IsNone() ? FName(PackageParams.HoudiniAssetName) : InOverrideFolderPath);

	USplineComponent* DuplicatedSplineComponent = DuplicateObject<USplineComponent>(
		InSplineComponent,
		OutActor,
		MakeUniqueObjectNameIfNeeded(OutActor, InSplineComponent->GetClass(), FName(PackageParams.ObjectName)));
	OutActor->AddInstanceComponent(DuplicatedSplineComponent);
	const bool bCreateIfMissing = true;
	USceneComponent* RootComponent = GetActorRootComponent(OutActor, bCreateIfMissing);
	DuplicatedSplineComponent->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
	
	FAssetRegistryModule::AssetCreated(DuplicatedSplineComponent);
	DuplicatedSplineComponent->RegisterComponent();

	OutSplineComponent = DuplicatedSplineComponent;
	return true;
}

bool 
FHoudiniEngineBakeUtils::BakeCurve(
	const FHoudiniOutputObject& InOutputObject,
	FHoudiniBakedOutputObject& InBakedOutputObject,
	// const TArray<FHoudiniBakedOutput>& InAllBakedOutputs,
	const FHoudiniPackageParams &PackageParams,
	bool bInReplaceActors,
	bool bInReplaceAssets,
	TArray<FHoudiniEngineBakedActor>& OutActors,
	TArray<UPackage*>& OutPackagesToSave,
	AActor* InFallbackActor,
	const FString& InFallbackWorldOutlinerFolder)
{
	USplineComponent* SplineComponent = Cast<USplineComponent>(InOutputObject.OutputComponent);
	if (!IsValid(SplineComponent))
		return false;

	// By default spawn in the current level unless specified via the unreal_level_path attribute
	ULevel* DesiredLevel = GWorld->GetCurrentLevel();
	bool bHasLevelPathAttribute = InOutputObject.CachedAttributes.Contains(HAPI_UNREAL_ATTRIB_LEVEL_PATH);
	if (bHasLevelPathAttribute)
	{
		UWorld* DesiredWorld = SplineComponent ? SplineComponent->GetWorld() : GWorld;

		// Access some of the attribute that were cached on the output object
		FHoudiniAttributeResolver Resolver;
		{
			TMap<FString, FString> CachedAttributes = InOutputObject.CachedAttributes;
			TMap<FString, FString> Tokens = InOutputObject.CachedTokens;
			PackageParams.UpdateTokensFromParams(DesiredWorld, Tokens);
			Resolver.SetCachedAttributes(CachedAttributes);
			Resolver.SetTokensFromStringMap(Tokens);
		}

		// Get the package path from the unreal_level_apth attribute
		FString LevelPackagePath = Resolver.ResolveFullLevelPath();

		bool bCreatedPackage = false;
		if (!FHoudiniEngineBakeUtils::FindOrCreateDesiredLevelFromLevelPath(
			LevelPackagePath,
			DesiredLevel,
			DesiredWorld,
			bCreatedPackage))
		{
			// TODO: LOG ERROR IF NO LEVEL
			return false;
		}

		// If we have created a new level, add it to the packages to save
		// TODO: ? always add?
		if (bCreatedPackage && DesiredLevel)
		{
			// We can now save the package again, and unload it.
			OutPackagesToSave.Add(DesiredLevel->GetOutermost());
		}
	}

	if(!DesiredLevel)
		return false;

	// Try to find the unreal_bake_actor, if specified, or fallback to the default named actor
	FName BakeActorName;
	AActor* FoundActor = nullptr;
	bool bHasBakeActorName = false;
	if (!FindUnrealBakeActor(InOutputObject, InBakedOutputObject, OutActors, DesiredLevel, *(PackageParams.ObjectName), bInReplaceActors, InFallbackActor, FoundActor, bHasBakeActorName, BakeActorName))
		return false;

	// If we are baking in replace mode, remove the previous bake component
	if (bInReplaceAssets && !InBakedOutputObject.BakedComponent.IsEmpty())
	{
		UActorComponent* PrevComponent = Cast<UActorComponent>(InBakedOutputObject.GetBakedComponentIfValid());
		if (PrevComponent && PrevComponent->GetOwner() == FoundActor)
		{
			RemovePreviouslyBakedComponent(PrevComponent);
		}
	}
	
	FHoudiniPackageParams CurvePackageParams = PackageParams;
	CurvePackageParams.ObjectName = BakeActorName.ToString();
	USplineComponent* NewSplineComponent = nullptr;
	const FName OutlinerFolderPath = GetOutlinerFolderPath(InOutputObject, *(CurvePackageParams.HoudiniAssetName));
	if (!BakeCurve(SplineComponent, DesiredLevel, CurvePackageParams, FoundActor, NewSplineComponent, OutlinerFolderPath, FoundActor))
		return false;

	InBakedOutputObject.Actor = FSoftObjectPath(FoundActor).ToString();
	InBakedOutputObject.BakedComponent = FSoftObjectPath(NewSplineComponent).ToString();

	// If we are baking in replace mode, remove previously baked components/instancers
	if (bInReplaceActors && bInReplaceAssets)
	{
		const bool bInDestroyBakedComponent = false;
		const bool bInDestroyBakedInstancedActors = true;
		const bool bInDestroyBakedInstancedComponents = true;
		DestroyPreviousBakeOutput(
			InBakedOutputObject, bInDestroyBakedComponent, bInDestroyBakedInstancedActors, bInDestroyBakedInstancedComponents);
	}

	FHoudiniEngineBakedActor Result;
	Result.Actor = FoundActor;
	Result.ActorBakeName = BakeActorName;
	OutActors.Add(Result);

	return true;
}

AActor*
FHoudiniEngineBakeUtils::BakeInputHoudiniCurveToActor(
	UHoudiniSplineComponent * InHoudiniSplineComponent,
	const FHoudiniPackageParams & PackageParams,
	UWorld* WorldToSpawn,
	const FTransform & SpawnTransform) 
{
	if (!InHoudiniSplineComponent || InHoudiniSplineComponent->IsPendingKill())
		return nullptr;

	TArray<FVector> & DisplayPoints = InHoudiniSplineComponent->DisplayPoints;
	if (DisplayPoints.Num() < 2)
		return nullptr;

	ULevel* DesiredLevel = GWorld->GetCurrentLevel();

	UActorFactory* Factory = GEditor ? GEditor->FindActorFactoryByClass(UActorFactoryEmptyActor::StaticClass()) : nullptr;
	if (!Factory)
		return nullptr;

	// Remove the actor if it exists
	for (auto & Actor : DesiredLevel->Actors)
	{
		if (!Actor)
			continue;

		if (Actor->GetName() == PackageParams.ObjectName)
		{
			UWorld* World = Actor->GetWorld();
			if (!World)
				World = GWorld;

			Actor->RemoveFromRoot();
			Actor->ConditionalBeginDestroy();
			World->EditorDestroyActor(Actor, true);

			break;
		}
	}

	AActor* NewActor = Factory->CreateActor(nullptr, DesiredLevel, InHoudiniSplineComponent->GetComponentTransform(), RF_Transactional);

	USplineComponent* BakedUnrealSplineComponent = NewObject<USplineComponent>(NewActor);
	if (!BakedUnrealSplineComponent)
		return nullptr;

	// add display points to created unreal spline component
	for (int32 n = 0; n < DisplayPoints.Num(); ++n) 
	{
		FVector & NextPoint = DisplayPoints[n];
		BakedUnrealSplineComponent->AddSplinePoint(NextPoint, ESplineCoordinateSpace::Local);
		// Set the curve point type to be linear, since we are using display points
		BakedUnrealSplineComponent->SetSplinePointType(n, ESplinePointType::Linear);
	}
	NewActor->AddInstanceComponent(BakedUnrealSplineComponent);

	BakedUnrealSplineComponent->AttachToComponent(NewActor->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);

	FAssetRegistryModule::AssetCreated(NewActor);
	FAssetRegistryModule::AssetCreated(BakedUnrealSplineComponent);
	BakedUnrealSplineComponent->RegisterComponent();

	// The default name will be based on the static mesh package, we would prefer it to be based on the Houdini asset
	const FName NewName = MakeUniqueObjectNameIfNeeded(DesiredLevel, Factory->NewActorClass, *(PackageParams.ObjectName), NewActor);
	const FString NewNameStr = NewName.ToString();
	// NewActor->Rename(*NewNameStr);
	// NewActor->SetActorLabel(NewNameStr);
	RenameAndRelabelActor(NewActor, NewNameStr, false);
	NewActor->SetFolderPath(FName(PackageParams.HoudiniAssetName));

	return NewActor;
}

UBlueprint* 
FHoudiniEngineBakeUtils::BakeInputHoudiniCurveToBlueprint(
	UHoudiniSplineComponent * InHoudiniSplineComponent,
	const FHoudiniPackageParams & PackageParams,
	UWorld* WorldToSpawn,
	const FTransform & SpawnTransform) 
{
	if (!InHoudiniSplineComponent || InHoudiniSplineComponent->IsPendingKill())
		return nullptr;

	FGuid BakeGUID = FGuid::NewGuid();

	if (!BakeGUID.IsValid())
		BakeGUID = FGuid::NewGuid();

	// We only want half of generated guid string.
	FString BakeGUIDString = BakeGUID.ToString().Left(FHoudiniEngineUtils::PackageGUIDItemNameLength);

	// Generate Blueprint name.
	FString BlueprintName = PackageParams.ObjectName + "_BP";

	// Generate unique package name.
	FString PackageName = PackageParams.BakeFolder + "/" + BlueprintName;
	PackageName = UPackageTools::SanitizePackageName(PackageName);

	// See if package exists, if it does, we need to regenerate the name.
	UPackage * Package = FindPackage(nullptr, *PackageName);

	if (Package && !Package->IsPendingKill())
	{
		// Package does exist, there's a collision, we need to generate a new name.
		BakeGUID.Invalidate();
	}
	else
	{
		// Create actual package.
		Package = CreatePackage(*PackageName);
	}

	AActor * CreatedHoudiniSplineActor = FHoudiniEngineBakeUtils::BakeInputHoudiniCurveToActor(
		InHoudiniSplineComponent, PackageParams, WorldToSpawn, SpawnTransform);

	TArray<UPackage*> PackagesToSave;

	UBlueprint * Blueprint = nullptr;
	if (CreatedHoudiniSplineActor && !CreatedHoudiniSplineActor->IsPendingKill())
	{

		UObject* Asset = nullptr;

		Asset = StaticFindObjectFast(UObject::StaticClass(), Package, FName(*BlueprintName));
		if (!Asset)
		{
			UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();

			FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");

			Asset = AssetToolsModule.Get().CreateAsset(
				BlueprintName, PackageParams.BakeFolder,
				UBlueprint::StaticClass(), Factory, FName("ContentBrowserNewAsset"));
		}

		TArray<UActorComponent*> Components;
		for (auto & Next : CreatedHoudiniSplineActor->GetComponents())
		{
			Components.Add(Next);
		}

		Blueprint = Cast<UBlueprint>(Asset);

		// Clear old Blueprint Node tree
		USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;

		int32 NodeSize = SCS->GetAllNodes().Num();
		for (int32 n = NodeSize - 1; n >= 0; --n)
			SCS->RemoveNode(SCS->GetAllNodes()[n]);

		FKismetEditorUtilities::AddComponentsToBlueprint(Blueprint, Components);

		CreatedHoudiniSplineActor->RemoveFromRoot();
		CreatedHoudiniSplineActor->ConditionalBeginDestroy();

		GWorld->EditorDestroyActor(CreatedHoudiniSplineActor, true);

		Package->MarkPackageDirty();
		PackagesToSave.Add(Package);
	}

	// Save the created BP package.
	FHoudiniEngineBakeUtils::SaveBakedPackages
	(PackagesToSave);

	return Blueprint;
}


void
FHoudiniEngineBakeUtils::AddHoudiniMetaInformationToPackage(
	UPackage * Package, UObject * Object, const TCHAR * Key,
	const TCHAR * Value)
{
	if (!Package || Package->IsPendingKill())
		return;

	UMetaData * MetaData = Package->GetMetaData();
	if (MetaData && !MetaData->IsPendingKill())
		MetaData->SetValue(Object, Key, Value);
}


bool
FHoudiniEngineBakeUtils::
GetHoudiniGeneratedNameFromMetaInformation(
	UPackage * Package, UObject * Object, FString & HoudiniName)
{
	if (!Package || Package->IsPendingKill())
		return false;

	UMetaData * MetaData = Package->GetMetaData();
	if (!MetaData || MetaData->IsPendingKill())
		return false;

	if (MetaData->HasValue(Object, HAPI_UNREAL_PACKAGE_META_GENERATED_OBJECT))
	{
		// Retrieve name used for package generation.
		const FString NameFull = MetaData->GetValue(Object, HAPI_UNREAL_PACKAGE_META_GENERATED_NAME);

		//HoudiniName = NameFull.Left(FMath::Min(NameFull.Len(), FHoudiniEngineUtils::PackageGUIDItemNameLength));
		HoudiniName = NameFull;
		return true;
	}

	return false;
}

UMaterial *
FHoudiniEngineBakeUtils::DuplicateMaterialAndCreatePackage(
	UMaterial * Material, UMaterial* PreviousBakeMaterial, const FString & MaterialName, const FHoudiniPackageParams& ObjectPackageParams,
	TArray<UPackage*> & OutGeneratedPackages)
{
	UMaterial * DuplicatedMaterial = nullptr;

	FString CreatedMaterialName;
	// Create material package.  Use the same package params as static mesh, but with the material's name
	FHoudiniPackageParams MaterialPackageParams = ObjectPackageParams;
	MaterialPackageParams.ObjectName = MaterialName;

	// Check if there is a valid previous material. If so, get the bake counter for consistency in
	// replace or iterative package naming
	bool bIsPreviousBakeMaterialValid = IsValid(PreviousBakeMaterial);
	int32 BakeCounter = 0;
	TArray<UMaterialExpression*> PreviousBakeMaterialExpressions;
	if (bIsPreviousBakeMaterialValid)
	{
		bIsPreviousBakeMaterialValid = MaterialPackageParams.MatchesPackagePathNameExcludingBakeCounter(PreviousBakeMaterial);
		if (bIsPreviousBakeMaterialValid)
		{
			MaterialPackageParams.GetBakeCounterFromBakedAsset(PreviousBakeMaterial, BakeCounter);
			PreviousBakeMaterialExpressions = PreviousBakeMaterial->Expressions;
		}
	}
	
	UPackage * MaterialPackage = MaterialPackageParams.CreatePackageForObject(CreatedMaterialName, BakeCounter);

	if (!MaterialPackage || MaterialPackage->IsPendingKill())
		return nullptr;

	// Clone material.
	DuplicatedMaterial = DuplicateObject< UMaterial >(Material, MaterialPackage, *CreatedMaterialName);
	if (!DuplicatedMaterial || DuplicatedMaterial->IsPendingKill())
		return nullptr;

	// Add meta information.
	FHoudiniEngineBakeUtils::AddHoudiniMetaInformationToPackage(
		MaterialPackage, DuplicatedMaterial,
		HAPI_UNREAL_PACKAGE_META_GENERATED_OBJECT, TEXT("true"));
	FHoudiniEngineBakeUtils::AddHoudiniMetaInformationToPackage(
		MaterialPackage, DuplicatedMaterial,
		HAPI_UNREAL_PACKAGE_META_GENERATED_NAME, *CreatedMaterialName);

	// Retrieve and check various sampling expressions. If they contain textures, duplicate (and bake) them.
	const int32 NumExpressions = DuplicatedMaterial->Expressions.Num();
	for (int32 ExpressionIdx = 0; ExpressionIdx < NumExpressions; ++ExpressionIdx)
	{
		UMaterialExpression* Expression = DuplicatedMaterial->Expressions[ExpressionIdx];
		UMaterialExpression* PreviousBakeExpression = nullptr;
		if (bIsPreviousBakeMaterialValid && PreviousBakeMaterialExpressions.IsValidIndex(ExpressionIdx))
		{
			PreviousBakeExpression = PreviousBakeMaterialExpressions[ExpressionIdx];
		}
		FHoudiniEngineBakeUtils::ReplaceDuplicatedMaterialTextureSample(
			Expression, PreviousBakeExpression, MaterialPackageParams, OutGeneratedPackages);
	}

	// Notify registry that we have created a new duplicate material.
	FAssetRegistryModule::AssetCreated(DuplicatedMaterial);

	// Dirty the material package.
	DuplicatedMaterial->MarkPackageDirty();

	// Recompile the baked material
	// DuplicatedMaterial->ForceRecompileForRendering();
	// Use UMaterialEditingLibrary::RecompileMaterial since it correctly updates texture references in the material
	// which ForceRecompileForRendering does not do
	UMaterialEditingLibrary::RecompileMaterial(DuplicatedMaterial);

	OutGeneratedPackages.Add(MaterialPackage);

	return DuplicatedMaterial;
}

void
FHoudiniEngineBakeUtils::ReplaceDuplicatedMaterialTextureSample(
	UMaterialExpression * MaterialExpression, UMaterialExpression* PreviousBakeMaterialExpression,
	const FHoudiniPackageParams& PackageParams, TArray<UPackage*> & OutCreatedPackages)
{
	UMaterialExpressionTextureSample * TextureSample = Cast< UMaterialExpressionTextureSample >(MaterialExpression);
	if (!TextureSample || TextureSample->IsPendingKill())
		return;

	UTexture2D * Texture = Cast< UTexture2D >(TextureSample->Texture);
	if (!Texture || Texture->IsPendingKill())
		return;

	UPackage * TexturePackage = Cast< UPackage >(Texture->GetOuter());
	if (!TexturePackage || TexturePackage->IsPendingKill())
		return;

	// Try to get the previous bake's texture
	UTexture2D* PreviousBakeTexture = nullptr;
	if (IsValid(PreviousBakeMaterialExpression))
	{
		UMaterialExpressionTextureSample* PreviousBakeTextureSample = Cast< UMaterialExpressionTextureSample >(PreviousBakeMaterialExpression);
		if (IsValid(PreviousBakeTextureSample))
			PreviousBakeTexture = Cast< UTexture2D >(PreviousBakeTextureSample->Texture);
	}

	FString GeneratedTextureName;
	if (FHoudiniEngineBakeUtils::GetHoudiniGeneratedNameFromMetaInformation(
		TexturePackage, Texture, GeneratedTextureName))
	{
		// Duplicate texture.
		UTexture2D * DuplicatedTexture = FHoudiniEngineBakeUtils::DuplicateTextureAndCreatePackage(
			Texture, PreviousBakeTexture, GeneratedTextureName, PackageParams, OutCreatedPackages);

		// Re-assign generated texture.
		TextureSample->Texture = DuplicatedTexture;
	}
}

UTexture2D *
FHoudiniEngineBakeUtils::DuplicateTextureAndCreatePackage(
	UTexture2D * Texture, UTexture2D* PreviousBakeTexture, const FString & SubTextureName, const FHoudiniPackageParams& PackageParams,
	TArray<UPackage*> & OutCreatedPackages)
{
	UTexture2D* DuplicatedTexture = nullptr;
#if WITH_EDITOR
	// Retrieve original package of this texture.
	UPackage * TexturePackage = Cast< UPackage >(Texture->GetOuter());
	if (!TexturePackage || TexturePackage->IsPendingKill())
		return nullptr;

	FString GeneratedTextureName;
	if (FHoudiniEngineBakeUtils::GetHoudiniGeneratedNameFromMetaInformation(TexturePackage, Texture, GeneratedTextureName))
	{
		UMetaData * MetaData = TexturePackage->GetMetaData();
		if (!MetaData || MetaData->IsPendingKill())
			return nullptr;

		// Retrieve texture type.
		const FString & TextureType =
			MetaData->GetValue(Texture, HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_TYPE);

		FString CreatedTextureName;

		// Create texture package. Use the same package params as material's, but with object name appended by generated texture's name
		FHoudiniPackageParams TexturePackageParams = PackageParams;
		TexturePackageParams.ObjectName = TexturePackageParams.ObjectName + "_" + GeneratedTextureName;

		// Determine the bake counter of the previous bake's texture (if exists/valid) for naming consistency when
		// replacing/iterating
		bool bIsPreviousBakeTextureValid = IsValid(PreviousBakeTexture);
		int32 BakeCounter = 0;
		if (bIsPreviousBakeTextureValid)
		{
			bIsPreviousBakeTextureValid = TexturePackageParams.MatchesPackagePathNameExcludingBakeCounter(PreviousBakeTexture);
			if (bIsPreviousBakeTextureValid)
			{
				TexturePackageParams.GetBakeCounterFromBakedAsset(PreviousBakeTexture, BakeCounter);
			}
		}

		UPackage * NewTexturePackage = TexturePackageParams.CreatePackageForObject(CreatedTextureName, BakeCounter);

		if (!NewTexturePackage || NewTexturePackage->IsPendingKill())
			return nullptr;
		
		// Clone texture.
		DuplicatedTexture = DuplicateObject< UTexture2D >(Texture, NewTexturePackage, *CreatedTextureName);
		if (!DuplicatedTexture || DuplicatedTexture->IsPendingKill())
			return nullptr;

		// Add meta information.
		FHoudiniEngineBakeUtils::AddHoudiniMetaInformationToPackage(
			NewTexturePackage, DuplicatedTexture,
			HAPI_UNREAL_PACKAGE_META_GENERATED_OBJECT, TEXT("true"));
		FHoudiniEngineBakeUtils::AddHoudiniMetaInformationToPackage(
			NewTexturePackage, DuplicatedTexture,
			HAPI_UNREAL_PACKAGE_META_GENERATED_NAME, *CreatedTextureName);
		FHoudiniEngineBakeUtils::AddHoudiniMetaInformationToPackage(
			NewTexturePackage, DuplicatedTexture,
			HAPI_UNREAL_PACKAGE_META_GENERATED_TEXTURE_TYPE, *TextureType);

		// Notify registry that we have created a new duplicate texture.
		FAssetRegistryModule::AssetCreated(DuplicatedTexture);
		
		// Dirty the texture package.
		DuplicatedTexture->MarkPackageDirty();

		OutCreatedPackages.Add(NewTexturePackage);
	}
#endif
	return DuplicatedTexture;
}


bool 
FHoudiniEngineBakeUtils::DeleteBakedHoudiniAssetActor(UHoudiniAssetComponent* HoudiniAssetComponent) 
{
	if (!HoudiniAssetComponent || HoudiniAssetComponent->IsPendingKill())
		return false;

	AActor * ActorOwner = HoudiniAssetComponent->GetOwner();

	if (!ActorOwner || ActorOwner->IsPendingKill())
		return false;

	UWorld* World = ActorOwner->GetWorld();
	if (!World)
		World = GWorld;

	World->EditorDestroyActor(ActorOwner, false);

	return true;
}


void 
FHoudiniEngineBakeUtils::SaveBakedPackages(TArray<UPackage*> & PackagesToSave, bool bSaveCurrentWorld) 
{
	UWorld * CurrentWorld = nullptr;
	if (bSaveCurrentWorld && GEditor)
		CurrentWorld = GEditor->GetEditorWorldContext().World();

	if (CurrentWorld)
	{
		// Save the current map
		FString CurrentWorldPath = FPaths::GetBaseFilename(CurrentWorld->GetPathName(), false);
		UPackage* CurrentWorldPackage = CreatePackage(*CurrentWorldPath);

		if (CurrentWorldPackage)
		{
			CurrentWorldPackage->MarkPackageDirty();
			PackagesToSave.Add(CurrentWorldPackage);
		}
	}

	FEditorFileUtils::PromptForCheckoutAndSave(PackagesToSave, true, false);
}

bool
FHoudiniEngineBakeUtils::FindOutputObject(
	const UObject* InObjectToFind, const TArray<UHoudiniOutput*> InOutputs, int32& OutOutputIndex, FHoudiniOutputObjectIdentifier &OutIdentifier)
{
	if (!InObjectToFind || InObjectToFind->IsPendingKill())
		return false;

	const int32 NumOutputs = InOutputs.Num();
	for (int32 OutputIdx = 0; OutputIdx < NumOutputs; ++OutputIdx)
	{
		const UHoudiniOutput* CurOutput = InOutputs[OutputIdx];
		if (!IsValid(CurOutput))
			continue;
		
		for (const auto& CurOutputObject : CurOutput->GetOutputObjects())
		{
			if (CurOutputObject.Value.OutputObject == InObjectToFind
				|| CurOutputObject.Value.OutputComponent == InObjectToFind
				|| CurOutputObject.Value.ProxyObject == InObjectToFind
				|| CurOutputObject.Value.ProxyComponent == InObjectToFind)
			{
				OutOutputIndex = OutputIdx;
				OutIdentifier = CurOutputObject.Key;
				return true;
			}
		}
	}

	return false;
}

bool
FHoudiniEngineBakeUtils::IsObjectTemporary(UObject* InObject, UHoudiniAssetComponent* InHAC)
{
	if (!InObject || InObject->IsPendingKill())
		return false;

	FString TempPath = FString();

	// TODO: Get the HAC outputs in a better way?
	TArray<UHoudiniOutput*> Outputs;
	if (InHAC && !InHAC->IsPendingKill())
	{
		const int32 NumOutputs = InHAC->GetNumOutputs();
		Outputs.Reserve(NumOutputs);
		for (int32 OutputIdx = 0; OutputIdx < NumOutputs; ++OutputIdx)
		{
			Outputs.Add(InHAC->GetOutputAt(OutputIdx));
		}

		TempPath = InHAC->TemporaryCookFolder.Path;
	}

	return IsObjectTemporary(InObject, Outputs, TempPath);
}

bool FHoudiniEngineBakeUtils::IsObjectTemporary(
	UObject* InObject, const TArray<UHoudiniOutput*>& InParentOutputs, const FString& InTemporaryCookFolder)
{
	if (!InObject || InObject->IsPendingKill())
		return false;

	int32 ParentOutputIndex = -1;
	FHoudiniOutputObjectIdentifier Identifier;
	if (FindOutputObject(InObject, InParentOutputs, ParentOutputIndex, Identifier))
		return true;
	
	// Check the package path for this object
	// If it is in the HAC temp directory, assume it is temporary, and will need to be duplicated
	UPackage* ObjectPackage = InObject->GetOutermost();
	if (ObjectPackage && !ObjectPackage->IsPendingKill())
	{
		const FString PathName = ObjectPackage->GetPathName();
		if (PathName.StartsWith(InTemporaryCookFolder))
			return true;

		// Also check the default temp folder
		const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault<UHoudiniRuntimeSettings>();
		if (PathName.StartsWith(HoudiniRuntimeSettings->DefaultTemporaryCookFolder))
			return true;
		
		/*
		// TODO: this just indicates that the object was generated by H
		// it could as well have been baked before... 
		// we should probably add a "temp" metadata
		// Look in the meta info as well??
		UMetaData * MetaData = ObjectPackage->GetMetaData();
		if (!MetaData || MetaData->IsPendingKill())
			return false;

		if (MetaData->HasValue(InObject, HAPI_UNREAL_PACKAGE_META_GENERATED_OBJECT))
			return true;
		*/
	}

	return false;	
}

void 
FHoudiniEngineBakeUtils::CopyPropertyToNewActorAndComponent(AActor* NewActor, UStaticMeshComponent* NewSMC, UStaticMeshComponent* InSMC)
{
	if (!NewSMC || NewSMC->IsPendingKill())
		return;

	if (!InSMC || InSMC->IsPendingKill())
		return;

	// Copy properties to new actor
	//UStaticMeshComponent* OtherSMC_NonConst = const_cast<UStaticMeshComponent*>(StaticMeshComponent);
	NewSMC->SetCollisionProfileName(InSMC->GetCollisionProfileName());
	NewSMC->SetCollisionEnabled(InSMC->GetCollisionEnabled());
	NewSMC->LightmassSettings = InSMC->LightmassSettings;
	NewSMC->CastShadow = InSMC->CastShadow;
	NewSMC->SetMobility(InSMC->Mobility);

	UBodySetup* InBodySetup = InSMC->GetBodySetup();
	UBodySetup* NewBodySetup = NewSMC->GetBodySetup();
	if (InBodySetup && NewBodySetup)
	{
		// Copy the BodySetup
		NewBodySetup->CopyBodyPropertiesFrom(InBodySetup);

		// We need to recreate the physics mesh for the new body setup
		NewBodySetup->InvalidatePhysicsData();
		NewBodySetup->CreatePhysicsMeshes();

		// Only copy the physical material if it's different from the default one,
		// As this was causing crashes on BakeToActors in some cases
		if (GEngine != NULL && NewBodySetup->GetPhysMaterial() != GEngine->DefaultPhysMaterial)
			NewSMC->SetPhysMaterialOverride(InBodySetup->GetPhysMaterial());
	}

	if (NewActor && !NewActor->IsPendingKill())
		NewActor->SetActorHiddenInGame(InSMC->bHiddenInGame);

	NewSMC->SetVisibility(InSMC->IsVisible());

	// TODO:
	// // Reapply the uproperties modified by attributes on the new component
	// FHoudiniEngineUtils::UpdateAllPropertyAttributesOnObject(InSMC, InHGPO);

	// The below code is from EditorUtilities::CopyActorProperties and modified to only copy from one component to another
	UClass* ComponentClass = InSMC->GetClass();
	if (ComponentClass != NewSMC->GetClass())
	{
		HOUDINI_LOG_WARNING(
			TEXT("Incompatible component classes in CopyPropertyToNewActorAndComponent: %s vs %s"),
			*(ComponentClass->GetName()),
			*(NewSMC->GetClass()->GetName()));

		NewSMC->PostEditChange();
		return;
	}

	TSet<const FProperty*> SourceUCSModifiedProperties;
	InSMC->GetUCSModifiedProperties(SourceUCSModifiedProperties);

	AActor* SourceActor = InSMC->GetOwner();
	if (!IsValid(SourceActor))
	{
		NewSMC->PostEditChange();
		return;
	}

	TArray<UObject*> ModifiedObjects;
	const EditorUtilities::FCopyOptions Options(EditorUtilities::ECopyOptions::CallPostEditChangeProperty);
	// Copy component properties
	for( FProperty* Property = ComponentClass->PropertyLink; Property != nullptr; Property = Property->PropertyLinkNext )
	{
		const bool bIsTransient = !!( Property->PropertyFlags & CPF_Transient );
		const bool bIsIdentical = Property->Identical_InContainer( InSMC, NewSMC );
		const bool bIsComponent = !!( Property->PropertyFlags & ( CPF_InstancedReference | CPF_ContainsInstancedReference ) );
		const bool bIsTransform =
			Property->GetFName() == USceneComponent::GetRelativeScale3DPropertyName() ||
			Property->GetFName() == USceneComponent::GetRelativeLocationPropertyName() ||
			Property->GetFName() == USceneComponent::GetRelativeRotationPropertyName();

		// auto SourceComponentIsRoot = [&]()
		// {
		// 	USceneComponent* RootComponent = SourceActor->GetRootComponent();
		// 	if (InSMC == RootComponent)
		// 	{
		// 		return true;
		// 	}
		// 	return false;
		// };

		// if( !bIsTransient && !bIsIdentical && !bIsComponent && !SourceUCSModifiedProperties.Contains(Property)
		// 	&& ( !bIsTransform || !SourceComponentIsRoot() ) )
		if( !bIsTransient && !bIsIdentical && !bIsComponent && !SourceUCSModifiedProperties.Contains(Property)
			&& !bIsTransform )
		{
			// const bool bIsSafeToCopy = (!(Options.Flags & EditorUtilities::ECopyOptions::OnlyCopyEditOrInterpProperties) || (Property->HasAnyPropertyFlags(CPF_Edit | CPF_Interp)))
			// 							&& (!(Options.Flags & EditorUtilities::ECopyOptions::SkipInstanceOnlyProperties) || (!Property->HasAllPropertyFlags(CPF_DisableEditOnTemplate)));
			const bool bIsSafeToCopy = true;
			if( bIsSafeToCopy )
			{
				if (!Options.CanCopyProperty(*Property, *SourceActor))
				{
					continue;
				}
					
				if( !ModifiedObjects.Contains(NewSMC) )
				{
					NewSMC->SetFlags(RF_Transactional);
					NewSMC->Modify();
					ModifiedObjects.Add(NewSMC);
				}

				if (Options.Flags & EditorUtilities::ECopyOptions::CallPostEditChangeProperty)
				{
					// @todo simulate: Should we be calling this on the component instead?
					NewActor->PreEditChange( Property );
				}

				EditorUtilities::CopySingleProperty(InSMC, NewSMC, Property);

				if (Options.Flags & EditorUtilities::ECopyOptions::CallPostEditChangeProperty)
				{
					FPropertyChangedEvent PropertyChangedEvent( Property );
					NewActor->PostEditChangeProperty( PropertyChangedEvent );
				}
			}
		}
	}

	NewSMC->PostEditChange();
};

bool
FHoudiniEngineBakeUtils::RemovePreviouslyBakedActor(
	AActor* InNewBakedActor,
	ULevel* InLevel,
	const FHoudiniPackageParams& InPackageParams)
{
	// Remove a previous bake actor if it exists
	for (auto & Actor : InLevel->Actors)
	{
		if (!Actor)
			continue;

		if (Actor != InNewBakedActor && Actor->GetName() == InPackageParams.ObjectName)
		{
			UWorld* World = Actor->GetWorld();
			if (!World)
				World = GWorld;

			Actor->RemoveFromRoot();
			Actor->ConditionalBeginDestroy();
			World->EditorDestroyActor(Actor, true);

			return true;
		}
	}

	return false;
}

bool
FHoudiniEngineBakeUtils::RemovePreviouslyBakedComponent(UActorComponent* InComponent)
{
	if (!IsValid(InComponent))
		return false;

	// Remove from its actor first
	if (InComponent->GetOwner())
		InComponent->GetOwner()->RemoveOwnedComponent(InComponent);

	// Detach from its parent component if attached
	USceneComponent* SceneComponent = Cast<USceneComponent>(InComponent);
	if (IsValid(SceneComponent))
		SceneComponent->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
	InComponent->UnregisterComponent();
	InComponent->DestroyComponent();

	return true;
}

FName
FHoudiniEngineBakeUtils::GetOutputFolderPath(UObject* InOutputOwner)
{
	// Get an output folder path for PDG outputs generated by InOutputOwner.
	// The folder path is: <InOutputOwner's folder path (if it is an actor)>/<InOutputOwner's name>
	FString FolderName;
	FName FolderDirName;
	AActor* OuterActor = Cast<AActor>(InOutputOwner);
	if (OuterActor)
	{
		FolderName = OuterActor->GetActorLabel();
		FolderDirName = OuterActor->GetFolderPath();	
	}
	else
	{
		FolderName = InOutputOwner->GetName();
	}
	if (!FolderDirName.IsNone())		
		return FName(FString::Printf(TEXT("%s/%s"), *FolderDirName.ToString(), *FolderName));
	else
		return FName(FolderName);
}

void
FHoudiniEngineBakeUtils::RenameAsset(UObject* InAsset, const FString& InNewName, bool bMakeUniqueIfNotUnique)
{
	FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));

	const FSoftObjectPath OldPath = FSoftObjectPath(InAsset);

	FString NewName;
	if (bMakeUniqueIfNotUnique)
		NewName = MakeUniqueObjectNameIfNeeded(InAsset->GetPackage(), InAsset->GetClass(), FName(InNewName), InAsset).ToString();
	else
		NewName = InNewName;

	InAsset->Rename(*NewName);
	
	const FSoftObjectPath NewPath = FSoftObjectPath(InAsset);
	if (OldPath != NewPath)
	{
		TArray<FAssetRenameData> RenameData;
		RenameData.Add(FAssetRenameData(OldPath, NewPath, true));
		AssetToolsModule.Get().RenameAssets(RenameData);
	}
}

void
FHoudiniEngineBakeUtils::RenameAndRelabelActor(AActor* InActor, const FString& InNewName, bool bMakeUniqueIfNotUnique)
{
	if (!IsValid(InActor))
		return;
	
	FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));

	const FSoftObjectPath OldPath = FSoftObjectPath(InActor);

	FString NewName;
	if (bMakeUniqueIfNotUnique)
		NewName = MakeUniqueObjectNameIfNeeded(InActor->GetOuter(), InActor->GetClass(), FName(InNewName), InActor).ToString();
	else
		NewName = InNewName;
	
	InActor->Rename(*NewName);
	InActor->SetActorLabel(NewName);
	
	const FSoftObjectPath NewPath = FSoftObjectPath(InActor);
	if (OldPath != NewPath)
	{
		TArray<FAssetRenameData> RenameData;
		RenameData.Add(FAssetRenameData(OldPath, NewPath, true));
		AssetToolsModule.Get().RenameAssets(RenameData);
	}
}

bool
FHoudiniEngineBakeUtils::DetachAndRenameBakedPDGOutputActor(
	AActor* InActor,
	const FString& InNewName,
	const FName& InFolderPath)
{
	if (!IsValid(InActor))
	{
		HOUDINI_LOG_WARNING(TEXT("[FHoudiniEngineUtils::DetachAndRenameBakedPDGOutputActor]: InActor is null."));
		return false;
	}

	if (InNewName.TrimStartAndEnd().IsEmpty())
	{
		HOUDINI_LOG_WARNING(TEXT("[FHoudiniEngineUtils::DetachAndRenameBakedPDGOutputActor]: A valid actor name must be specified."));
		return false;
	}

	// Detach from parent
	InActor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	// Rename
	// InActor->Rename(*MakeUniqueObjectNameIfNeeded(InActor->GetOuter(), InActor->GetClass(), FName(InNewName)).ToString());
	// InActor->SetActorLabel(InNewName);
	const bool bMakeUniqueIfNotUnique = true;
	RenameAndRelabelActor(InActor, InNewName, bMakeUniqueIfNotUnique);

	InActor->SetFolderPath(InFolderPath);

	return true;
}

bool
FHoudiniEngineBakeUtils::BakePDGWorkResultObject(
	UHoudiniPDGAssetLink* InPDGAssetLink,
	UTOPNode* InNode,
	int32 InWorkResultIndex,
	int32 InWorkResultObjectIndex,
	bool bInReplaceActors,
	bool bInReplaceAssets,
	bool bInBakeToWorkResultActor,
	TArray<FHoudiniEngineBakedActor>& OutBakedActors,
	TArray<UPackage*>& OutPackagesToSave,
	FHoudiniEngineOutputStats& OutBakeStats,
	TArray<EHoudiniOutputType> const* InOutputTypesToBake,
	TArray<EHoudiniInstancerComponentType> const* InInstancerComponentTypesToBake,
	const FString& InFallbackWorldOutlinerFolder)
{
	if (!IsValid(InPDGAssetLink))
		return false;

	if (!IsValid(InNode))
		return false;

	if (!InNode->WorkResult.IsValidIndex(InWorkResultIndex))
		return false;

	FTOPWorkResult& WorkResult = InNode->WorkResult[InWorkResultIndex];
	if (!WorkResult.ResultObjects.IsValidIndex(InWorkResultObjectIndex))
		return false;
	
	FTOPWorkResultObject& WorkResultObject = WorkResult.ResultObjects[InWorkResultObjectIndex];
	TArray<UHoudiniOutput*>& Outputs = WorkResultObject.GetResultOutputs();
	if (Outputs.Num() == 0)
		return true;

	AActor* WorkResultObjectActor = WorkResultObject.GetOutputActorOwner().GetOutputActor();
	if (!IsValid(WorkResultObjectActor))
	{
		HOUDINI_LOG_WARNING(TEXT("[FHoudiniEngineBakeUtils::BakePDGTOPNodeOutputsKeepActors]: WorkResultObjectActor (%s) is null (unexpected since # Outputs > 0)"), *WorkResultObject.Name);
		return false;
	}

	// BakedActorsForWorkResultObject contains each actor that contains baked PDG results. Actors may
	// appear in the array more than once if they have more than one baked result/component associated with
	// them
	TArray<FHoudiniEngineBakedActor> BakedActorsForWorkResultObject;
	const FString HoudiniAssetName(WorkResultObject.Name);

	// Find the previous bake output for this work result object
	FString Key;
	InNode->GetBakedWorkResultObjectOutputsKey(InWorkResultIndex, InWorkResultObjectIndex, Key);
	FHoudiniPDGWorkResultObjectBakedOutput& BakedOutputContainer = InNode->GetBakedWorkResultObjectsOutputs().FindOrAdd(Key);
	
	BakeHoudiniOutputsToActors(
		Outputs,
		BakedOutputContainer.BakedOutputs,
		HoudiniAssetName,
		WorkResultObjectActor->GetActorTransform(),
		InPDGAssetLink->BakeFolder,
		InPDGAssetLink->GetTemporaryCookFolder(),
		bInReplaceActors,
		bInReplaceAssets,
		BakedActorsForWorkResultObject, 
		OutPackagesToSave,
		OutBakeStats,
		InOutputTypesToBake,
		InInstancerComponentTypesToBake,
		bInBakeToWorkResultActor ? WorkResultObjectActor : nullptr,
		InFallbackWorldOutlinerFolder);

	// Set the PDG indices on the output baked actor entries
	if (BakedActorsForWorkResultObject.Num() > 0)
	{
		for (FHoudiniEngineBakedActor& BakedActorEntry : BakedActorsForWorkResultObject)
		{
			BakedActorEntry.PDGWorkResultIndex = InWorkResultIndex;
			BakedActorEntry.PDGWorkResultObjectIndex = InWorkResultObjectIndex;
		}
	}

	// If anything was baked to WorkResultObjectActor, detach it from its parent
	if (bInBakeToWorkResultActor)
	{
		FOutputActorOwner& OutputActorOwner = WorkResultObject.GetOutputActorOwner();
		// if we re-used the temp actor as a bake actor, then remove its temp outputs
		WorkResultObject.DestroyResultOutputs();
		AActor* WROActor = OutputActorOwner.GetOutputActor();
		if (WROActor)
		{
			const FHoudiniEngineBakedActor* BakedActorEntry = BakedActorsForWorkResultObject.FindByPredicate([WROActor](const FHoudiniEngineBakedActor& Entry)
			{
				return Entry.Actor == WROActor;
			});
			if (BakedActorEntry)
			{
				OutputActorOwner.SetOutputActor(nullptr);
				const FString OldActorPath = FSoftObjectPath(WROActor).ToString();
				DetachAndRenameBakedPDGOutputActor(
					WROActor, BakedActorEntry->ActorBakeName.ToString(), BakedActorEntry->WorldOutlinerFolder);
				const FString NewActorPath = FSoftObjectPath(WROActor).ToString();
				if (OldActorPath != NewActorPath)
				{
					// Fix cached string reference in baked outputs to WROActor
					for (FHoudiniBakedOutput& BakedOutput : BakedOutputContainer.BakedOutputs)
					{
						for (auto& Entry : BakedOutput.BakedOutputObjects)
						{
							if (Entry.Value.Actor == OldActorPath)
								Entry.Value.Actor = NewActorPath;
						}
					}
				}
			}
			else
			{
				OutputActorOwner.DestroyOutputActor();
			}
		}
	}
	OutBakedActors.Append(BakedActorsForWorkResultObject);
	return true;
}


bool
FHoudiniEngineBakeUtils::BakePDGWorkResultObject(
	UHoudiniPDGAssetLink* InPDGAssetLink,
	UTOPNode* InNode,
	int32 InWorkResultId,
	const FString& InWorkResultObjectName)
{
	if (!IsValid(InPDGAssetLink))
		return false;

	if (!IsValid(InNode))
		return false;

	// Find the work result index and work result object index
	const int32 WorkResultIndex = InNode->WorkResult.IndexOfByPredicate([InWorkResultId](const FTOPWorkResult& Entry)
	{
		return Entry.WorkItemID == InWorkResultId;
	});
	if (!InNode->WorkResult.IsValidIndex(WorkResultIndex))
		return false;
	FTOPWorkResult& WorkResult = InNode->WorkResult[WorkResultIndex];
	const int32 WorkResultObjectIndex = WorkResult.ResultObjects.IndexOfByPredicate([InWorkResultObjectName](const FTOPWorkResultObject& Entry)
	{
		return Entry.Name.Equals(InWorkResultObjectName);
	});
	if (!WorkResult.ResultObjects.IsValidIndex(WorkResultObjectIndex))
		return false;
	
	// Determine the output world outliner folder path via the PDG asset link's
	// owner's folder path and name
	UObject* PDGOwner = InPDGAssetLink->GetOwnerActor();
	if (!PDGOwner)
		PDGOwner = InPDGAssetLink->GetOuter();
	const FName& FallbackWorldOutlinerFolderPath = GetOutputFolderPath(PDGOwner);

	// Determine the actor/package replacement settings
	const bool bBakeBlueprints = InPDGAssetLink->HoudiniEngineBakeOption == EHoudiniEngineBakeOption::ToBlueprint;
	const bool bReplaceActors = !bBakeBlueprints && InPDGAssetLink->PDGBakePackageReplaceMode == EPDGBakePackageReplaceModeOption::ReplaceExistingAssets;
	const bool bReplaceAssets = InPDGAssetLink->PDGBakePackageReplaceMode == EPDGBakePackageReplaceModeOption::ReplaceExistingAssets;

	// Determine the output types to bake: don't bake landscapes in blueprint baking mode
	TArray<EHoudiniOutputType> OutputTypesToBake;
	TArray<EHoudiniInstancerComponentType> InstancerComponentTypesToBake;
	if (bBakeBlueprints)
	{
		OutputTypesToBake.Add(EHoudiniOutputType::Mesh);
		OutputTypesToBake.Add(EHoudiniOutputType::Instancer);
		OutputTypesToBake.Add(EHoudiniOutputType::Curve);

		InstancerComponentTypesToBake.Add(EHoudiniInstancerComponentType::StaticMeshComponent);
		InstancerComponentTypesToBake.Add(EHoudiniInstancerComponentType::InstancedStaticMeshComponent);
		InstancerComponentTypesToBake.Add(EHoudiniInstancerComponentType::MeshSplitInstancerComponent);
	}

	TArray<UPackage*> PackagesToSave;
	FHoudiniEngineOutputStats BakeStats;
	TArray<FHoudiniEngineBakedActor> BakedActors;

	bool bSuccess = BakePDGWorkResultObject(
		InPDGAssetLink,
		InNode,
		WorkResultIndex,
		WorkResultObjectIndex,
		bReplaceActors,
		bReplaceAssets,
		!bBakeBlueprints,
		BakedActors,
		PackagesToSave,
		BakeStats,
		OutputTypesToBake.Num() > 0 ? &OutputTypesToBake : nullptr,
		InstancerComponentTypesToBake.Num() > 0 ? &InstancerComponentTypesToBake : nullptr,
		FallbackWorldOutlinerFolderPath.ToString()
	); 

	// Recenter and select the baked actors
	if (GEditor && BakedActors.Num() > 0)
		GEditor->SelectNone(false, true);
	
	for (const FHoudiniEngineBakedActor& Entry : BakedActors)
	{
		if (!IsValid(Entry.Actor))
			continue;
		
		if (InPDGAssetLink->bRecenterBakedActors)
			CenterActorToBoundingBoxCenter(Entry.Actor);

		if (GEditor)
			GEditor->SelectActor(Entry.Actor, true, false);
	}
	
	if (GEditor && BakedActors.Num() > 0)
		GEditor->NoteSelectionChange();

	if (bBakeBlueprints && bSuccess)
	{
		TArray<UBlueprint*> Blueprints;
		bSuccess = BakeBlueprintsFromBakedActors(
			BakedActors,
			InPDGAssetLink->bRecenterBakedActors,
			bReplaceAssets,
			InPDGAssetLink->AssetName,
			InPDGAssetLink->BakeFolder,
			nullptr,
			&InNode->GetBakedWorkResultObjectsOutputs(),
			Blueprints,
			PackagesToSave);

		// Sync the CB to the baked objects
		if(GEditor && Blueprints.Num() > 0)
		{
			TArray<UObject*> Assets;
			Assets.Reserve(Blueprints.Num());
			for (UBlueprint* Blueprint : Blueprints)
			{
				Assets.Add(Blueprint);
			}
			GEditor->SyncBrowserToObjects(Assets);
		}
	}

	SaveBakedPackages(PackagesToSave);

	{
		const FString FinishedTemplate = TEXT("Baking finished. Created {0} packages. Updated {1} packages.");
		FString Msg = FString::Format(*FinishedTemplate, { BakeStats.NumPackagesCreated, BakeStats.NumPackagesUpdated } );
		FHoudiniEngine::Get().FinishTaskSlateNotification( FText::FromString(Msg) );
	}

	return bSuccess;
}

void
FHoudiniEngineBakeUtils::AutoBakePDGWorkResultObject(
	UHoudiniPDGAssetLink* InPDGAssetLink,
	UTOPNode* InNode,
	int32 InWorkResultId,
	const FString& InWorkResultObjectName)
{
	if (!IsValid(InPDGAssetLink))
		return;

	if (!InPDGAssetLink->bBakeAfterWorkResultObjectLoaded)
		return;

	BakePDGWorkResultObject(
		InPDGAssetLink,
		InNode,
		InWorkResultId,
		InWorkResultObjectName);
}

bool
FHoudiniEngineBakeUtils::BakePDGTOPNodeOutputsKeepActors(
	UHoudiniPDGAssetLink* InPDGAssetLink,
	UTOPNode* InNode,
	bool bInBakeForBlueprint,
	TArray<FHoudiniEngineBakedActor>& OutBakedActors,
	TArray<UPackage*>& OutPackagesToSave,
	FHoudiniEngineOutputStats& OutBakeStats) 
{
	if (!InPDGAssetLink || InPDGAssetLink->IsPendingKill())
		return false;

	if (!IsValid(InNode))
		return false;

	// Determine the output world outliner folder path via the PDG asset link's
	// owner's folder path and name
	UObject* PDGOwner = InPDGAssetLink->GetOwnerActor();
	if (!PDGOwner)
		PDGOwner = InPDGAssetLink->GetOuter();
	const FName& FallbackWorldOutlinerFolderPath = GetOutputFolderPath(PDGOwner);

	// Determine the actor/package replacement settings
	const bool bReplaceActors = !bInBakeForBlueprint && InPDGAssetLink->PDGBakePackageReplaceMode == EPDGBakePackageReplaceModeOption::ReplaceExistingAssets;
	const bool bReplaceAssets = InPDGAssetLink->PDGBakePackageReplaceMode == EPDGBakePackageReplaceModeOption::ReplaceExistingAssets;

	// Determine the output types to bake: don't bake landscapes in blueprint baking mode
	TArray<EHoudiniOutputType> OutputTypesToBake;
	TArray<EHoudiniInstancerComponentType> InstancerComponentTypesToBake;
	if (bInBakeForBlueprint)
	{
		OutputTypesToBake.Add(EHoudiniOutputType::Mesh);
		OutputTypesToBake.Add(EHoudiniOutputType::Instancer);
		OutputTypesToBake.Add(EHoudiniOutputType::Curve);

		InstancerComponentTypesToBake.Add(EHoudiniInstancerComponentType::StaticMeshComponent);
		InstancerComponentTypesToBake.Add(EHoudiniInstancerComponentType::InstancedStaticMeshComponent);
		InstancerComponentTypesToBake.Add(EHoudiniInstancerComponentType::MeshSplitInstancerComponent);
	}

	const int32 NumWorkResults = InNode->WorkResult.Num();
	for (int32 WorkResultIdx = 0; WorkResultIdx < NumWorkResults; ++WorkResultIdx)
	{
		FTOPWorkResult& WorkResult = InNode->WorkResult[WorkResultIdx];
		const int32 NumWorkResultObjects = WorkResult.ResultObjects.Num();
		for (int32 WorkResultObjectIdx = 0; WorkResultObjectIdx < NumWorkResultObjects; ++WorkResultObjectIdx)
		{
			BakePDGWorkResultObject(
				InPDGAssetLink,
				InNode,
				WorkResultIdx,
				WorkResultObjectIdx,
				bReplaceActors,
				bReplaceAssets,
				!bInBakeForBlueprint,
				OutBakedActors,
				OutPackagesToSave,
				OutBakeStats,
				OutputTypesToBake.Num() > 0 ? &OutputTypesToBake : nullptr,
				InstancerComponentTypesToBake.Num() > 0 ? &InstancerComponentTypesToBake : nullptr,
				FallbackWorldOutlinerFolderPath.ToString()
			);
		}
	}

	return true;
}

bool
FHoudiniEngineBakeUtils::BakePDGTOPNetworkOutputsKeepActors(
	UHoudiniPDGAssetLink* InPDGAssetLink,
	UTOPNetwork* InNetwork,
	bool bInBakeForBlueprint,
	TArray<FHoudiniEngineBakedActor>& BakedActors,
	TArray<UPackage*>& OutPackagesToSave,
	FHoudiniEngineOutputStats& OutBakeStats)
{
	if (!InPDGAssetLink || InPDGAssetLink->IsPendingKill())
		return false;

	if (!IsValid(InNetwork))
		return false;

	bool bSuccess = true;
	for (UTOPNode* Node : InNetwork->AllTOPNodes)
	{
		if (!IsValid(Node))
			continue;

		bSuccess &= BakePDGTOPNodeOutputsKeepActors(InPDGAssetLink, Node, bInBakeForBlueprint, BakedActors, OutPackagesToSave, OutBakeStats);
	}

	return bSuccess;
}

bool
FHoudiniEngineBakeUtils::BakePDGAssetLinkOutputsKeepActors(UHoudiniPDGAssetLink* InPDGAssetLink)
{
	if (!InPDGAssetLink || InPDGAssetLink->IsPendingKill())
		return false;

	TArray<UPackage*> PackagesToSave;
	FHoudiniEngineOutputStats BakeStats;
	TArray<FHoudiniEngineBakedActor> BakedActors;

	const bool bBakeBlueprints = false;

	bool bSuccess = true;
	switch(InPDGAssetLink->PDGBakeSelectionOption)
	{
		case EPDGBakeSelectionOption::All:
			for (UTOPNetwork* Network : InPDGAssetLink->AllTOPNetworks)
			{
				if (!IsValid(Network))
					continue;
				
				for (UTOPNode* Node : Network->AllTOPNodes)
				{
					if (!IsValid(Node))
						continue;
					
					bSuccess &= BakePDGTOPNodeOutputsKeepActors(InPDGAssetLink, Node, bBakeBlueprints, BakedActors, PackagesToSave, BakeStats);
				}
			}
			break;
		case EPDGBakeSelectionOption::SelectedNetwork:
			bSuccess = BakePDGTOPNetworkOutputsKeepActors(InPDGAssetLink, InPDGAssetLink->GetSelectedTOPNetwork(), bBakeBlueprints, BakedActors, PackagesToSave, BakeStats);
		case EPDGBakeSelectionOption::SelectedNode:
			bSuccess = BakePDGTOPNodeOutputsKeepActors(InPDGAssetLink, InPDGAssetLink->GetSelectedTOPNode(), bBakeBlueprints, BakedActors, PackagesToSave, BakeStats);
	}

	SaveBakedPackages(PackagesToSave);

	// Recenter and select the baked actors
	if (GEditor && BakedActors.Num() > 0)
		GEditor->SelectNone(false, true);
	
	for (const FHoudiniEngineBakedActor& Entry : BakedActors)
	{
		if (!IsValid(Entry.Actor))
			continue;
		
		if (InPDGAssetLink->bRecenterBakedActors)
			CenterActorToBoundingBoxCenter(Entry.Actor);

		if (GEditor)
			GEditor->SelectActor(Entry.Actor, true, false);
	}
	
	if (GEditor && BakedActors.Num() > 0)
		GEditor->NoteSelectionChange();

	{
		const FString FinishedTemplate = TEXT("Baking finished. Created {0} packages. Updated {1} packages.");
		FString Msg = FString::Format(*FinishedTemplate, { BakeStats.NumPackagesCreated, BakeStats.NumPackagesUpdated } );
		FHoudiniEngine::Get().FinishTaskSlateNotification( FText::FromString(Msg) );
	}

	return bSuccess;
}

bool
FHoudiniEngineBakeUtils::BakeBlueprintsFromBakedActors(
	const TArray<FHoudiniEngineBakedActor>& InBakedActors, 
	bool bInRecenterBakedActors,
	bool bInReplaceAssets,
	const FString& InAssetName,
	const FDirectoryPath& InBakeFolder,
	TArray<FHoudiniBakedOutput>* const InNonPDGBakedOuputs,
	TMap<FString, FHoudiniPDGWorkResultObjectBakedOutput>* const InPDGBakedOutputs,
	TArray<UBlueprint*>& OutBlueprints,
	TArray<UPackage*>& OutPackagesToSave)
{
	// // Clear selection
	// if (GEditor)
	// {
	// 	GEditor->SelectNone(false, true);
	// 	GEditor->NoteSelectionChange();
	// }

	// Iterate over the baked actors. An actor might appear multiple times if multiple OutputComponents were
	// baked to the same actor, so keep track of actors we have already processed in BakedActorSet
	UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
	const bool bIsAssetEditorSubsystemValid = IsValid(AssetEditorSubsystem);
	TArray<UObject*> AssetsToReOpenEditors;
	TSet<AActor*> BakedActorSet;

	for (const FHoudiniEngineBakedActor& Entry : InBakedActors)
	{
		AActor *Actor = Entry.Actor;
		
		if (!Actor || Actor->IsPendingKill())
			continue;

		if (BakedActorSet.Contains(Actor))
			continue;

		BakedActorSet.Add(Actor);

		UObject* Asset = nullptr;

		// Recenter the actor to its bounding box center
		if (bInRecenterBakedActors)
			CenterActorToBoundingBoxCenter(Actor);

		// Create package for out Blueprint
		FString BlueprintName;

		FHoudiniPackageParams PackageParams;
		// Set the replace mode based on if we are doing a replacement or incremental asset bake
		const EPackageReplaceMode AssetPackageReplaceMode = bInReplaceAssets ?
            EPackageReplaceMode::ReplaceExistingAssets : EPackageReplaceMode::CreateNewAssets;
		FHoudiniEngineUtils::FillInPackageParamsForBakingOutput(
            PackageParams,
            FHoudiniOutputObjectIdentifier(),
            InBakeFolder.Path,
            Entry.ActorBakeName.ToString() + "_BP",
            InAssetName,
            AssetPackageReplaceMode);
		
		// If we have a previously baked a blueprint, get the bake counter from it so that both replace and increment
		// is consistent with the bake counter
        int32 BakeCounter = 0;
		UBlueprint* InPreviousBlueprint = nullptr;
		FHoudiniBakedOutputObject* BakedOutputObject = nullptr;
		FHoudiniPDGWorkResultObjectBakedOutput* WorkResultObjectBakedOutput = nullptr;
		// Get the baked output object
		if (Entry.PDGWorkResultIndex >= 0 && Entry.PDGWorkResultObjectIndex >= 0 && InPDGBakedOutputs)
		{
			const FString Key = UTOPNode::GetBakedWorkResultObjectOutputsKey(Entry.PDGWorkResultIndex, Entry.PDGWorkResultObjectIndex);
			WorkResultObjectBakedOutput = InPDGBakedOutputs->Find(Key);
			if (WorkResultObjectBakedOutput)
			{
				if (Entry.OutputIndex >= 0 && WorkResultObjectBakedOutput->BakedOutputs.IsValidIndex(Entry.OutputIndex))
				{
					BakedOutputObject = WorkResultObjectBakedOutput->BakedOutputs[Entry.OutputIndex].BakedOutputObjects.Find(Entry.OutputObjectIdentifier);
				}
			}
		}
		else if (Entry.OutputIndex >= 0 && InNonPDGBakedOuputs)
		{
			if (Entry.OutputIndex >= 0 && InNonPDGBakedOuputs->IsValidIndex(Entry.OutputIndex))
			{
				BakedOutputObject = (*InNonPDGBakedOuputs)[Entry.OutputIndex].BakedOutputObjects.Find(Entry.OutputObjectIdentifier);
			}
		}
		if (BakedOutputObject)
		{
			InPreviousBlueprint = BakedOutputObject->GetBlueprintIfValid();
			if (IsValid(InPreviousBlueprint))
			{
				if (PackageParams.MatchesPackagePathNameExcludingBakeCounter(InPreviousBlueprint))
				{
					PackageParams.GetBakeCounterFromBakedAsset(InPreviousBlueprint, BakeCounter);
				}
			}
		}

		UPackage* Package = PackageParams.CreatePackageForObject(BlueprintName, BakeCounter);
		
		if (!Package || Package->IsPendingKill())
		{
			HOUDINI_LOG_WARNING(TEXT("Could not find or create a package for the blueprint of %s"), *(Actor->GetPathName()));
			continue;
		}

		if (!Package->IsFullyLoaded())
			Package->FullyLoad();

		//Blueprint = FKismetEditorUtilities::CreateBlueprintFromActor(*BlueprintName, Package, Actor, false);
		// Find existing asset first first (only relevant if we are in replacement mode). If the existing asset has a
		// different base class than the incoming actor, we reparent the blueprint to the new base class before
		// clearing the SCS graph and repopulating it from the temp actor.
		Asset = StaticFindObjectFast(UBlueprint::StaticClass(), Package, FName(*BlueprintName));
		if (IsValid(Asset))
		{
			UBlueprint* Blueprint = Cast<UBlueprint>(Asset);
			if (IsValid(Blueprint))
			{
				if (Blueprint->GeneratedClass && Blueprint->GeneratedClass != Actor->GetClass())
				{
					// Close editors opened on existing asset if applicable
					if (Asset && bIsAssetEditorSubsystemValid && AssetEditorSubsystem->FindEditorForAsset(Asset, false) != nullptr)
					{
						AssetEditorSubsystem->CloseAllEditorsForAsset(Asset);
						AssetsToReOpenEditors.Add(Asset);
					}

					Blueprint->ParentClass = Actor->GetClass();

					FBlueprintEditorUtils::RefreshAllNodes(Blueprint);
					FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
					FKismetEditorUtilities::CompileBlueprint(Blueprint);
				}
			}
		}
		else if (Asset && Asset->IsPendingKill())
		{
			// Rename to pending kill so that we can use the desired name
			const FString AssetPendingKillName(BlueprintName + "_PENDING_KILL");
			// Asset->Rename(*MakeUniqueObjectNameIfNeeded(Package, UBlueprint::StaticClass(), AssetPendingKillName).ToString());
			RenameAsset(Asset, AssetPendingKillName, true);
			Asset = nullptr;
		}

		if (!Asset)
		{
			UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();
			Factory->ParentClass = Actor->GetClass();

			FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");

			Asset = AssetToolsModule.Get().CreateAsset(
                BlueprintName, InBakeFolder.Path,
                UBlueprint::StaticClass(), Factory, FName("ContentBrowserNewAsset"));
		}

		UBlueprint* Blueprint = Cast<UBlueprint>(Asset);

		if (!Blueprint || Blueprint->IsPendingKill())
		{
			HOUDINI_LOG_WARNING(
				TEXT("Found an asset at %s/%s, but it was not a blueprint or was pending kill."),
				*(InBakeFolder.Path), *BlueprintName);
			
			continue;
		}

		// Close editors opened on existing asset if applicable
		if (Blueprint && bIsAssetEditorSubsystemValid && AssetEditorSubsystem->FindEditorForAsset(Blueprint, false) != nullptr)
		{
			AssetEditorSubsystem->CloseAllEditorsForAsset(Blueprint);
			AssetsToReOpenEditors.Add(Blueprint);
		}
		
		// Record the blueprint as the previous bake blueprint
		if (BakedOutputObject)
			BakedOutputObject->Blueprint = FSoftObjectPath(Blueprint).ToString();
		
		OutBlueprints.Add(Blueprint);

		// Clear old Blueprint Node tree
		{
			USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;

			int32 NodeSize = SCS->GetAllNodes().Num();
			for (int32 n = NodeSize - 1; n >= 0; --n)
				SCS->RemoveNode(SCS->GetAllNodes()[n]);
		}

		FHoudiniEngineBakeUtils::CopyActorContentsToBlueprint(Actor, Blueprint);

		UWorld* World = Actor->GetWorld();
		if (!World)
			World = GWorld;

		World->EditorDestroyActor(Actor, true);

		// Save the created BP package.
		Package->MarkPackageDirty();
		OutPackagesToSave.Add(Package);
	}

	// Re-open asset editors for updated blueprints that were open in editors
	if (bIsAssetEditorSubsystemValid && AssetsToReOpenEditors.Num() > 0)
	{
		for (UObject* Asset : AssetsToReOpenEditors)
		{
			if (IsValid(Asset))
			{
				AssetEditorSubsystem->OpenEditorForAsset(Asset);
			}
		}
	}

	return true;
}

bool
FHoudiniEngineBakeUtils::BakePDGTOPNodeBlueprints(
	UHoudiniPDGAssetLink* InPDGAssetLink,
	UTOPNode* InNode,
	TArray<UBlueprint*>& OutBlueprints,
	TArray<UPackage*>& OutPackagesToSave,
	FHoudiniEngineOutputStats& OutBakeStats)
{
	TArray<AActor*> BPActors;

	if (!IsValid(InPDGAssetLink))
	{
		HOUDINI_LOG_WARNING(TEXT("[FHoudiniEngineBakeUtils::BakePDGBlueprint]: InPDGAssetLink is null"));
		return false;
	}
		
	if (!IsValid(InNode))
	{
		HOUDINI_LOG_WARNING(TEXT("[FHoudiniEngineBakeUtils::BakePDGBlueprint]: InNode is null"));
		return false;
	}

	const bool bReplaceAssets = InPDGAssetLink->PDGBakePackageReplaceMode == EPDGBakePackageReplaceModeOption::ReplaceExistingAssets; 
	
	// Bake PDG output to new actors
	// bInBakeForBlueprint == true will skip landscapes and instanced actor components
	const bool bInBakeForBlueprint = true;
	TArray<FHoudiniEngineBakedActor> BakedActors;
	bool bSuccess = BakePDGTOPNodeOutputsKeepActors(
		InPDGAssetLink,
		InNode,
		bInBakeForBlueprint,
		BakedActors,
		OutPackagesToSave,
		OutBakeStats
	);

	if (bSuccess)
	{
		bSuccess = BakeBlueprintsFromBakedActors(
			BakedActors,
			InPDGAssetLink->bRecenterBakedActors,
			bReplaceAssets,
			InPDGAssetLink->AssetName,
			InPDGAssetLink->BakeFolder,
			nullptr,
			&InNode->GetBakedWorkResultObjectsOutputs(),
			OutBlueprints,
			OutPackagesToSave);
	}
	
	return bSuccess;
}

bool
FHoudiniEngineBakeUtils::BakePDGTOPNetworkBlueprints(
	UHoudiniPDGAssetLink* InPDGAssetLink,
	UTOPNetwork* InNetwork,
	TArray<UBlueprint*>& OutBlueprints,
	TArray<UPackage*>& OutPackagesToSave,
	FHoudiniEngineOutputStats& OutBakeStats)
{
	if (!InPDGAssetLink || InPDGAssetLink->IsPendingKill())
		return false;

	if (!IsValid(InNetwork))
		return false;

	bool bSuccess = true;
	for (UTOPNode* Node : InNetwork->AllTOPNodes)
	{
		if (!IsValid(Node))
			continue;
		
		bSuccess &= BakePDGTOPNodeBlueprints(InPDGAssetLink, Node, OutBlueprints, OutPackagesToSave, OutBakeStats);
	}

	return bSuccess;
}

bool
FHoudiniEngineBakeUtils::BakePDGAssetLinkBlueprints(UHoudiniPDGAssetLink* InPDGAssetLink)
{
	TArray<UBlueprint*> Blueprints;
	TArray<UPackage*> PackagesToSave;
	FHoudiniEngineOutputStats BakeStats;
	
	if (!InPDGAssetLink || InPDGAssetLink->IsPendingKill())
		return false;

	bool bSuccess = true;
	switch(InPDGAssetLink->PDGBakeSelectionOption)
	{
		case EPDGBakeSelectionOption::All:
			for (UTOPNetwork* Network : InPDGAssetLink->AllTOPNetworks)
			{
				if (!IsValid(Network))
					continue;
				
				for (UTOPNode* Node : Network->AllTOPNodes)
				{
					if (!IsValid(Node))
						continue;
					
					bSuccess &= BakePDGTOPNodeBlueprints(InPDGAssetLink, Node, Blueprints, PackagesToSave, BakeStats);
				}
			}
			break;
		case EPDGBakeSelectionOption::SelectedNetwork:
			bSuccess &= BakePDGTOPNetworkBlueprints(
				InPDGAssetLink,
				InPDGAssetLink->GetSelectedTOPNetwork(),
			Blueprints,
			PackagesToSave,
			BakeStats);
		case EPDGBakeSelectionOption::SelectedNode:
			bSuccess &= BakePDGTOPNodeBlueprints(
				InPDGAssetLink,
				InPDGAssetLink->GetSelectedTOPNode(),
				Blueprints,
				PackagesToSave,
				BakeStats);
	}

	FHoudiniEngineBakeUtils::SaveBakedPackages(PackagesToSave);

	// Sync the CB to the baked objects
	if(GEditor && Blueprints.Num() > 0)
	{
		TArray<UObject*> Assets;
		Assets.Reserve(Blueprints.Num());
		for (UBlueprint* Blueprint : Blueprints)
		{
			Assets.Add(Blueprint);
		}
		GEditor->SyncBrowserToObjects(Assets);
	}

	{
		const FString FinishedTemplate = TEXT("Baking finished. Created {0} packages. Updated {1} packages.");
		FString Msg = FString::Format(*FinishedTemplate, { BakeStats.NumPackagesCreated, BakeStats.NumPackagesUpdated } );
		FHoudiniEngine::Get().FinishTaskSlateNotification( FText::FromString(Msg) );
	}
	
	TryCollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);

	return bSuccess;
}

bool
FHoudiniEngineBakeUtils::FindOrCreateDesiredLevelFromLevelPath(
	const FString& InLevelPath,
	ULevel*& OutDesiredLevel,
	UWorld*& OutDesiredWorld,
	bool& OutCreatedPackage)
{
	OutDesiredLevel = nullptr;
	OutDesiredWorld = nullptr;
	if (InLevelPath.IsEmpty())
	{
		OutDesiredWorld = GWorld;
		OutDesiredLevel = GWorld->GetCurrentLevel();
	}
	else
	{
		OutCreatedPackage = false;

		UWorld* FoundWorld = nullptr;
		ULevel* FoundLevel = nullptr;		
		bool bActorInWorld = false;
		if (FHoudiniEngineUtils::FindWorldAndLevelForSpawning(
			GWorld,
			InLevelPath,
			true,
			FoundWorld,
			FoundLevel,
			OutCreatedPackage,
			bActorInWorld))
		{
			OutDesiredLevel = FoundLevel;
			OutDesiredWorld = FoundWorld;
		}
	}

	return ((OutDesiredWorld != nullptr) && (OutDesiredLevel != nullptr));
}


bool
FHoudiniEngineBakeUtils::FindDesiredBakeActorFromBakeActorName(
	const FString& InBakeActorName,
	ULevel* InLevel,
	AActor*& OutActor,
	bool bInNoPendingKillActors,
	bool bRenamePendingKillActor)
{
	OutActor = nullptr;
	
	if (!IsValid(InLevel))
		return false;

	UWorld* const World = InLevel->GetWorld();
	if (!IsValid(World))
		return false;

	// Look for an actor with the given name in the world
	const FName BakeActorFName(InBakeActorName); 
	AActor* FoundActor = Cast<AActor>(StaticFindObjectFast(AActor::StaticClass(), InLevel, BakeActorFName));
	// for (TActorIterator<AActor> Iter(World, AActor::StaticClass(), EActorIteratorFlags::AllActors); Iter; ++Iter)
	// {
	// 	AActor* const Actor = *Iter;
	// 	if (Actor->GetFName() == BakeActorFName && Actor->GetLevel() == InLevel)
	// 	{
	// 		// Found the actor
	// 		FoundActor = Actor;
	// 		break;
	// 	}
	// }

	// If we found an actor and it is pending kill, rename it and don't use it
	if (FoundActor)
	{
		if (FoundActor->IsPendingKill())
		{
			if (bRenamePendingKillActor)
			{
				// FoundActor->Rename(
    //                 *MakeUniqueObjectNameIfNeeded(
    //                     FoundActor->GetOuter(),
    //                     FoundActor->GetClass(),
    //                     FName(FoundActor->GetName() + "_Pending_Kill")).ToString());
				RenameAndRelabelActor(
					FoundActor,
                    *MakeUniqueObjectNameIfNeeded(
                        FoundActor->GetOuter(),
                        FoundActor->GetClass(),
                        FName(FoundActor->GetName() + "_Pending_Kill"),
                        FoundActor).ToString(),
                    false);
			}
			if (bInNoPendingKillActors)
				FoundActor = nullptr;
			else
				OutActor = FoundActor;
		}
		else
		{
			OutActor = FoundActor;
		}
	}

	return true;
}

bool FHoudiniEngineBakeUtils::FindUnrealBakeActor(
	const FHoudiniOutputObject& InOutputObject,
	const FHoudiniBakedOutputObject& InBakedOutputObject,
	const TArray<FHoudiniEngineBakedActor>& InAllBakedActors,
	ULevel* InLevel,
	FName InDefaultActorName,
	bool bInReplaceActorBakeMode,
	AActor* InFallbackActor,
	AActor*& OutFoundActor,
	bool& bOutHasBakeActorName,
	FName& OutBakeActorName)
{
	// Determine desired actor name via unreal_output_actor, fallback to InDefaultActorName
	OutBakeActorName = NAME_None;
	OutFoundActor = nullptr;
	bOutHasBakeActorName = InOutputObject.CachedAttributes.Contains(HAPI_UNREAL_ATTRIB_BAKE_ACTOR);
	if (bOutHasBakeActorName)
	{
		const FString& BakeActorNameStr = InOutputObject.CachedAttributes[HAPI_UNREAL_ATTRIB_BAKE_ACTOR];
		if (BakeActorNameStr.IsEmpty())
		{
			OutBakeActorName = NAME_None;
			bOutHasBakeActorName = false;
		}
		else
		{
			OutBakeActorName = *BakeActorNameStr;
			// We have a bake actor name, look for the actor
			AActor* BakeNameActor = nullptr;
			if (FindDesiredBakeActorFromBakeActorName(BakeActorNameStr, InLevel, BakeNameActor))
			{
				// Found an actor with that name, check that we "own" it (we created in during baking previously)
				AActor* IncrementedBakedActor = nullptr;
				for (const FHoudiniEngineBakedActor& BakedActor : InAllBakedActors)
				{
					if (!IsValid(BakedActor.Actor))
						continue;
					if (BakedActor.Actor == BakeNameActor)
					{
						OutFoundActor = BakeNameActor;
						break;
					}
					else if (!IncrementedBakedActor && BakedActor.ActorBakeName == OutBakeActorName)
					{
						// Found an actor we have baked named OutBakeActorName_# (incremental version of our desired name)
						IncrementedBakedActor = BakedActor.Actor;
					}
				}
				if (!OutFoundActor && IncrementedBakedActor)
					OutFoundActor = IncrementedBakedActor;
			}
		}
	}

	// If unreal_actor_name is not set, or is blank, fallback to InDefaultActorName
	if (!bOutHasBakeActorName || (OutBakeActorName.IsNone() || OutBakeActorName.ToString().TrimStartAndEnd().IsEmpty()))
		OutBakeActorName = InDefaultActorName;

	if (!OutFoundActor)
	{
		// If in replace mode, use previous bake actor if valid and in InLevel
		if (bInReplaceActorBakeMode)
		{
			const FSoftObjectPath PrevActorPath(InBakedOutputObject.Actor);
			const FString ActorPath = PrevActorPath.IsSubobject()
                ? PrevActorPath.GetAssetPathString() + ":" + PrevActorPath.GetSubPathString()
                : PrevActorPath.GetAssetPathString();
			const FString LevelPath = IsValid(InLevel) ? InLevel->GetPathName() : "";
			if (PrevActorPath.IsValid() && (LevelPath.IsEmpty() || ActorPath.StartsWith(LevelPath)))
				OutFoundActor = InBakedOutputObject.GetActorIfValid();
		}

		// Fallback to InFallbackActor if valid and in InLevel
		if (!OutFoundActor && IsValid(InFallbackActor) && (!InLevel || InFallbackActor->GetLevel() == InLevel))
			OutFoundActor = InFallbackActor;
	}

	return true;
}

AActor*
FHoudiniEngineBakeUtils::FindExistingActor_Bake(
	UWorld* InWorld,
	UHoudiniOutput* InOutput,	
	const FString& InActorName,
	const FString& InPackagePath,
	UWorld*& OutWorld,
	ULevel*& OutLevel,
	bool& bCreatedPackage)
{
	bCreatedPackage = false;

	// Try to Locate a previous actor
	AActor* FoundActor = FHoudiniEngineUtils::FindOrRenameInvalidActor<AActor>(InWorld, InActorName, FoundActor);
	if (FoundActor)
		FoundActor->Destroy(); // nuke it!

	if (FoundActor)
	{
		// TODO: make sure that the found is actor is actually assigned to the level defined by package path.
		//       If the found actor is not from that level, it should be moved there.

		OutWorld = FoundActor->GetWorld();
		OutLevel = FoundActor->GetLevel();
	}
	else
	{
		// Actor is not present, BUT target package might be loaded. Find the appropriate World and Level for spawning. 
		bool bActorInWorld = false;
		const bool bResult = FHoudiniEngineUtils::FindWorldAndLevelForSpawning(
			InWorld,
			InPackagePath,
			true,
			OutWorld,
			OutLevel,
			bCreatedPackage,
			bActorInWorld);

		if (!bResult)
		{
			return nullptr;
		}

		if (!bActorInWorld)
		{
			// The OutLevel is not present in the current world which means we might
			// still find the tile actor in OutWorld.
			FoundActor = FHoudiniEngineUtils::FindActorInWorld<AActor>(OutWorld, FName(InActorName));
		}
	}

	return FoundActor;
}

bool
FHoudiniEngineBakeUtils::CheckForAndRefineHoudiniProxyMesh(
	UHoudiniAssetComponent* InHoudiniAssetComponent,
	bool bInReplacePreviousBake,
	EHoudiniEngineBakeOption InBakeOption,
	bool bInRemoveHACOutputOnSuccess,
	bool& bOutNeedsReCook)
{
	if (!IsValid(InHoudiniAssetComponent))
	{
		return false;
	}
		
	// Handle proxies: if the output has any current proxies, first refine them
	bOutNeedsReCook = false;
	if (InHoudiniAssetComponent->HasAnyCurrentProxyOutput())
	{
		bool bNeedsRebuildOrDelete;
		bool bInvalidState;
		const bool bCookedDataAvailable = InHoudiniAssetComponent->IsHoudiniCookedDataAvailable(bNeedsRebuildOrDelete, bInvalidState);

		if (bCookedDataAvailable)
		{
			// Cook data is available, refine the mesh
			AHoudiniAssetActor* HoudiniActor = Cast<AHoudiniAssetActor>(InHoudiniAssetComponent->GetOwner());
			if (IsValid(HoudiniActor))
			{
				FHoudiniEngineCommands::RefineHoudiniProxyMeshActorArrayToStaticMeshes({ HoudiniActor });
			}
		}
		else if (!bNeedsRebuildOrDelete && !bInvalidState)
		{
			// A cook is needed: request the cook, but with no proxy and with a bake after cook
			InHoudiniAssetComponent->SetNoProxyMeshNextCookRequested(true);
			// Only
			if (!InHoudiniAssetComponent->IsBakeAfterNextCookEnabled() || !InHoudiniAssetComponent->GetOnPostCookBakeDelegate().IsBound())
			{
				InHoudiniAssetComponent->GetOnPostCookBakeDelegate().BindLambda([bInReplacePreviousBake, InBakeOption, bInRemoveHACOutputOnSuccess](UHoudiniAssetComponent* InHAC) {
                    return FHoudiniEngineBakeUtils::BakeHoudiniAssetComponent(InHAC, bInReplacePreviousBake, InBakeOption, bInRemoveHACOutputOnSuccess);
                });
			}
			InHoudiniAssetComponent->MarkAsNeedCook();

			bOutNeedsReCook = true;

			// The cook has to complete first (asynchronously) before the bake can happen
			// The SetBakeAfterNextCookEnabled flag will result in a bake after cook
			return false;
		}
		else
		{
			// The HAC is in an unsupported state
			const EHoudiniAssetState AssetState = InHoudiniAssetComponent->GetAssetState();
			HOUDINI_LOG_ERROR(TEXT("Could not refine (in order to bake) %s, the asset is in an unsupported state: %s"), *(InHoudiniAssetComponent->GetPathName()), *(UEnum::GetValueAsString(AssetState)));
			return false;
		}
	}

	return true;
}

void
FHoudiniEngineBakeUtils::CenterActorToBoundingBoxCenter(AActor* InActor)
{
	if (!IsValid(InActor))
		return;

	USceneComponent * const RootComponent = InActor->GetRootComponent();
	if (!IsValid(RootComponent))
		return;

	// If the root component does not have any child components, then there is nothing to recenter
	if (RootComponent->GetNumChildrenComponents() <= 0)
		return;

	const bool bOnlyCollidingComponents = false;
	const bool bIncludeFromChildActors = true;
	FVector Origin;
	FVector BoxExtent;
	InActor->GetActorBounds(bOnlyCollidingComponents, Origin, BoxExtent, bIncludeFromChildActors);

	const FVector Delta = Origin - RootComponent->GetComponentLocation();
	// Actor->SetActorLocation(Origin);
	RootComponent->SetWorldLocation(Origin);

	for (USceneComponent* SceneComponent : RootComponent->GetAttachChildren())
	{
		if (!IsValid(SceneComponent))
			continue;
		
		SceneComponent->SetWorldLocation(SceneComponent->GetComponentLocation() - Delta);
	}
}

void
FHoudiniEngineBakeUtils::CenterActorsToBoundingBoxCenter(const TArray<AActor*>& InActors)
{
	for (AActor* Actor : InActors)
	{
		if (!IsValid(Actor))
			continue;

		CenterActorToBoundingBoxCenter(Actor);
	}
}

USceneComponent*
FHoudiniEngineBakeUtils::GetActorRootComponent(AActor* InActor, bool bCreateIfMissing, EComponentMobility::Type InMobilityIfCreated)
{
	USceneComponent* RootComponent = InActor->GetRootComponent();
	if (!IsValid(RootComponent))
	{
		RootComponent = NewObject<USceneComponent>(InActor, USceneComponent::GetDefaultSceneRootVariableName(), RF_Transactional);

		// Change the creation method so the component is listed in the details panels
		InActor->SetRootComponent(RootComponent);
		InActor->AddInstanceComponent(RootComponent);
		RootComponent->RegisterComponent();
		RootComponent->SetMobility(InMobilityIfCreated);
	}

	return RootComponent;
}

FName
FHoudiniEngineBakeUtils::MakeUniqueObjectNameIfNeeded(UObject* InOuter, const UClass* InClass, FName InName, UObject* InObjectThatWouldBeRenamed)
{
	if (IsValid(InObjectThatWouldBeRenamed))
	{
		const FName CurrentName = InObjectThatWouldBeRenamed->GetFName();
		if (CurrentName == InName)
			return InName;

		// Check if the prefix matches (without counter suffix) the new name
		const FString CurrentNamePlainStr = CurrentName.GetPlainNameString();
		if (CurrentNamePlainStr == InName.ToString())
			return CurrentName;
	}

	UObject* ExistingObject = nullptr;
	if (InOuter == ANY_PACKAGE)
	{
		ExistingObject = StaticFindObject(nullptr, ANY_PACKAGE, *InName.ToString());
	}
	else
	{
		ExistingObject = StaticFindObjectFast(nullptr, InOuter, InName);
	}

	if (ExistingObject)
		return MakeUniqueObjectName(InOuter, InClass, InName);
	return InName;
}

FName
FHoudiniEngineBakeUtils::GetOutlinerFolderPath(const FHoudiniOutputObject& InOutputObject, FName InDefaultFolder)
{
	const FString* FolderPathPtr = InOutputObject.CachedAttributes.Find(HAPI_UNREAL_ATTRIB_BAKE_OUTLINER_FOLDER);
	if (FolderPathPtr && !FolderPathPtr->IsEmpty())
		return FName(*FolderPathPtr);
	else
		return InDefaultFolder;
}

bool
FHoudiniEngineBakeUtils::SetOutlinerFolderPath(AActor* InActor, const FHoudiniOutputObject& InOutputObject, FName InDefaultFolder)
{
	if (!IsValid(InActor))
		return false;

	InActor->SetFolderPath(GetOutlinerFolderPath(InOutputObject, InDefaultFolder));
	return true;
}

uint32
FHoudiniEngineBakeUtils::DestroyPreviousBakeOutput(
	FHoudiniBakedOutputObject& InBakedOutputObject,
	bool bInDestroyBakedComponent,
	bool bInDestroyBakedInstancedActors,
	bool bInDestroyBakedInstancedComponents)
{
	uint32 NumDeleted = 0;

	if (bInDestroyBakedComponent)
	{
		UActorComponent* Component = Cast<UActorComponent>(InBakedOutputObject.GetBakedComponentIfValid());
		if (Component)
		{
			if (RemovePreviouslyBakedComponent(Component))
			{
				InBakedOutputObject.BakedComponent = nullptr;
				NumDeleted++;
			}
		}
	}

	if (bInDestroyBakedInstancedActors)
	{
		for (const FString& ActorPathStr : InBakedOutputObject.InstancedActors)
		{
			const FSoftObjectPath ActorPath(ActorPathStr);

			if (!ActorPath.IsValid())
				continue;

			AActor* Actor = Cast<AActor>(ActorPath.TryLoad());
			if (IsValid(Actor))
			{
				UWorld* World = Actor->GetWorld();
				if (IsValid(World))
				{
#if WITH_EDITOR
					World->EditorDestroyActor(Actor, true);
#else
					World->DestroyActor(Actor);
#endif
					NumDeleted++;
				}
			}
		}
		InBakedOutputObject.InstancedActors.Empty();
	}

	if (bInDestroyBakedInstancedComponents)
	{
		for (const FString& ComponentPathStr : InBakedOutputObject.InstancedComponents)
		{
			const FSoftObjectPath ComponentPath(ComponentPathStr);

			if (!ComponentPath.IsValid())
				continue;

			UActorComponent* Component = Cast<UActorComponent>(ComponentPath.TryLoad());
			if (IsValid(Component))
			{
				if (RemovePreviouslyBakedComponent(Component))
					NumDeleted++;
			}
		}
		InBakedOutputObject.InstancedComponents.Empty();
	}
	
	return NumDeleted;
}

#undef LOCTEXT_NAMESPACE