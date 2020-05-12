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

#include "HoudiniEngineBakeUtils.h"

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
#include "Kismet2/KismetEditorUtilities.h"
#include "FileHelpers.h"
#include "Editor/EditorEngine.h"
#include "Factories/BlueprintFactory.h"
#include "Engine/SimpleConstructionScript.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "LandscapeEdit.h"


AActor * 
FHoudiniEngineBakeUtils::CloneComponentsAndCreateActor(UHoudiniAssetComponent* HoudiniAssetComponent, TArray<UPackage*> & OutCreatedPackages) 
{
	if (!HoudiniAssetComponent || HoudiniAssetComponent->IsPendingKill())
		return nullptr;

	AActor* AssetActor = HoudiniAssetComponent->GetOwner();
	if (!AssetActor || AssetActor->IsPendingKill())
		return nullptr;

	// Display busy cursor.
	FScopedBusyCursor ScopedBusyCursor;
	
	ULevel* Level = nullptr;
	if (AssetActor && !AssetActor->IsPendingKill())
		Level = AssetActor->GetLevel();
	
	if (!Level)
		Level = GWorld->GetCurrentLevel();

	AActor * Actor = NewObject<AActor>(Level, NAME_None);
	Actor->AddToRoot();

	USceneComponent * RootComponent =
		NewObject< USceneComponent >(Actor, USceneComponent::GetDefaultSceneRootVariableName(), RF_Transactional);

	RootComponent->SetMobility(EComponentMobility::Static);
	RootComponent->bVisualizeComponent = true;

	const FTransform & ComponentWorldTransform = HoudiniAssetComponent->GetComponentTransform();
	RootComponent->SetWorldLocationAndRotation(
		ComponentWorldTransform.GetLocation(),
		ComponentWorldTransform.GetRotation());

	Actor->SetRootComponent(RootComponent);
	Actor->AddInstanceComponent(RootComponent);

	RootComponent->RegisterComponent();

	// Duplicate static mesh components.
	for (int32 n = 0; n < HoudiniAssetComponent->GetNumOutputs(); ++n) 
	{
		UHoudiniOutput* CurrentOutput = HoudiniAssetComponent->GetOutputAt(n);
		if (!CurrentOutput || CurrentOutput->IsPendingKill())
			continue;

		TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& OutputObjects = CurrentOutput->GetOutputObjects();
		switch (CurrentOutput->GetType())
		{
			case EHoudiniOutputType::Mesh: 
			{
				const TArray<FHoudiniGeoPartObject> &HGPOs = CurrentOutput->GetHoudiniGeoPartObjects();
				for (auto& CurrentOutputObject : OutputObjects)
				{
					FHoudiniOutputObjectIdentifier& Identifier = CurrentOutputObject.Key;

					UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(CurrentOutputObject.Value.OutputComponent);
					if (!StaticMeshComponent || StaticMeshComponent->IsPendingKill())
						continue;

					UStaticMesh* StaticMesh = StaticMeshComponent->GetStaticMesh();
					if (!StaticMesh || StaticMesh->IsPendingKill())
						continue;
				
					// Find HGPO
					// TODO: This may not work 100% (after rebuild/reload etc..)
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

					FString SMName = CurrentOutputObject.Value.BakeName;
					if (SMName.IsEmpty())
					{
						if (FoundHGPO->bHasCustomPartName)
							SMName = FoundHGPO->PartName;
						else
							SMName = StaticMesh->GetName();
					}
				
					FHoudiniPackageParams PackageParams;
					FHoudiniEngineBakeUtils::FillInPackageParamsForBakingOutput(
						PackageParams, Identifier, HoudiniAssetComponent->BakeFolder.Path, SMName, AssetActor->GetName());

					UStaticMesh* OutStaticMesh = FHoudiniEngineBakeUtils::DuplicateStaticMeshAndCreatePackage(StaticMesh, PackageParams, OutCreatedPackages);
					if (!OutStaticMesh || OutStaticMesh->IsPendingKill())
						continue;

					FAssetRegistryModule::AssetCreated(OutStaticMesh);

					// Create static mesh component for baked mesh.
					UStaticMeshComponent * DuplicatedComponent =
						NewObject<UStaticMeshComponent>(Actor, UStaticMeshComponent::StaticClass(), NAME_None);//, RF_Transactional );

					if (!DuplicatedComponent || DuplicatedComponent->IsPendingKill())
						continue;

					FAssetRegistryModule::AssetCreated(DuplicatedComponent);

					Actor->AddInstanceComponent(DuplicatedComponent);

					DuplicatedComponent->SetStaticMesh(OutStaticMesh);
					DuplicatedComponent->SetVisibility(true);
					//DuplicatedComponent->SetRelativeTransform(HoudiniGeoPartObject.TransformMatrix);

					// If this is a collision geo, we need to make it invisible.
					/*
					if (HoudiniGeoPartObject.IsCollidable())
					{
						DuplicatedComponent->SetVisibility(false);
						DuplicatedComponent->SetHiddenInGame(true);
						DuplicatedComponent->SetCollisionProfileName(FName(TEXT("InvisibleWall")));
					}
					*/

					// Reapply the uproperties modified by attributes on the duplicated component
					//FHoudiniEngineUtils::UpdateUPropertyAttributesOnObject(DuplicatedComponent, HoudiniGeoPartObject);

					DuplicatedComponent->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
					DuplicatedComponent->RegisterComponent();

				}
			}
			break;

			case EHoudiniOutputType::Curve:
			{
				for (auto& CurrentOutputObject : OutputObjects)
				{
					USplineComponent* SplineComponent = Cast<USplineComponent>(CurrentOutputObject.Value.OutputObject);
					if (!SplineComponent || SplineComponent->IsPendingKill())
						continue;

					FString CurveName = AssetActor->GetName();
					if (CurrentOutputObject.Value.BakeName.IsEmpty())
						CurveName = CurrentOutputObject.Value.BakeName;

					FName BaseName(CurveName);
					USplineComponent* DuplicatedSplineComponent = DuplicateObject<USplineComponent>(SplineComponent, Actor, BaseName);
					DuplicatedSplineComponent->AttachToComponent(Actor->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);

					FAssetRegistryModule::AssetCreated(DuplicatedSplineComponent);
					DuplicatedSplineComponent->RegisterComponent();
				}
			}
			break;

			case EHoudiniOutputType::Landscape:
				break;

			case EHoudiniOutputType::Instancer:
			{
				TMap<FHoudiniOutputObjectIdentifier, FHoudiniInstancedOutput>& InstancedOutputs = CurrentOutput->GetInstancedOutputs();
				int32 InstancedOutputIdx = 0;

				for (auto & Pair : InstancedOutputs)
				{
					FHoudiniInstancedOutput& InstancedOutput = Pair.Value;
					UInstancedStaticMeshComponent * InstancedStaticMeshcomponent = nullptr;

					for (auto & NextOutputObj : OutputObjects)
					{
						if (NextOutputObj.Key.GeoId == Pair.Key.GeoId &&
							NextOutputObj.Key.ObjectId == Pair.Key.ObjectId &&
							NextOutputObj.Key.PartId == Pair.Key.PartId)
						{
							InstancedStaticMeshcomponent = Cast<UInstancedStaticMeshComponent>(NextOutputObj.Value.OutputComponent);
							break;
						}
					}

					if (!InstancedStaticMeshcomponent)
						continue;

					for (int32 VariationIdx = 0; VariationIdx < InstancedOutput.VariationObjects.Num(); ++VariationIdx)
					{
						UObject* CurrentVariationObject = InstancedOutput.VariationObjects[VariationIdx].Get();
						UStaticMesh * OutStaticMesh = Cast<UStaticMesh>(CurrentVariationObject);
						if (!OutStaticMesh)
						{
							if (CurrentVariationObject)
							{
								HOUDINI_LOG_ERROR(TEXT("Failed to bake the instances of %s to Foliage"), *CurrentVariationObject->GetName());
							}
							continue;
						}

						// TODO FIX ME?? Only needed if that original object is from H, if instancing a UE4 asset we want to use the original!
						// If the original static mesh is used (the cooked mesh for HAPI), then we need to bake it to a file
						// If not, we don't need to bake a new mesh as we're using a SM override, so can use the existing unreal asset
						if (InstancedOutput.OriginalObject == OutStaticMesh)
						{
							// Bake the Houdini generated static mesh
							FHoudiniPackageParams PackageParams;
							PackageParams.BakeFolder = HoudiniAssetComponent->BakeFolder.Path;
							FHoudiniEngineBakeUtils::FillInPackageParamsForBakingOutput(PackageParams, Pair.Key, HoudiniAssetComponent->BakeFolder.Path,
								AssetActor->GetName() + "_" + OutStaticMesh->GetName(), AssetActor->GetName());
							FHoudiniEngineBakeUtils::DuplicateStaticMeshAndCreatePackage(OutStaticMesh, PackageParams, OutCreatedPackages);
						}

						// Apply the trasnform offset on the transforms for this variation
						TArray<FTransform> ProcessedTransforms;
						FHoudiniInstanceTranslator::ProcessInstanceTransforms(InstancedOutput, VariationIdx, ProcessedTransforms);

						FString CompName = AssetActor->GetName() + "_instancer_output";
						for (int32 Idx = 0; Idx < ProcessedTransforms.Num(); ++Idx)
						{
							UStaticMeshComponent * DuplicatedComponent =
								NewObject<UStaticMeshComponent>(Actor, UStaticMeshComponent::StaticClass(), FName(CompName + FString::FromInt(InstancedOutputIdx) + "_" + FString::FromInt(Idx)));

							if (!DuplicatedComponent || DuplicatedComponent->IsPendingKill())
								continue;

							FAssetRegistryModule::AssetCreated(DuplicatedComponent);

							Actor->AddInstanceComponent(DuplicatedComponent);

							DuplicatedComponent->SetStaticMesh(OutStaticMesh);
							DuplicatedComponent->SetVisibility(true);

							DuplicatedComponent->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
							DuplicatedComponent->RegisterComponent();

						}

						InstancedOutputIdx += 1;
					}

				}
			}
			break;

			case EHoudiniOutputType::Skeletal:
				break;

			case EHoudiniOutputType::Invalid:
				break;
		}
	}

	return Actor;
}

bool 
FHoudiniEngineBakeUtils::ReplaceHoudiniActorWithActors(UHoudiniAssetComponent* HoudiniAssetComponent) 
{
	if (!HoudiniAssetComponent || HoudiniAssetComponent->IsPendingKill())
		return false;

	if (FHoudiniEngineBakeUtils::BakeHoudiniActorToActors(HoudiniAssetComponent)) 
	{
		return FHoudiniEngineBakeUtils::DeleteBakedHoudiniAssetActor(HoudiniAssetComponent);
	}

	return true;
}

bool 
FHoudiniEngineBakeUtils::BakeHoudiniActorToActors(UHoudiniAssetComponent* HoudiniAssetComponent) 
{
	if (!HoudiniAssetComponent || HoudiniAssetComponent->IsPendingKill())
		return false;

	TArray<AActor*> NewActors;
	for (int32 n = 0; n < HoudiniAssetComponent->GetNumOutputs(); ++n) 
	{
		UHoudiniOutput* Output = HoudiniAssetComponent->GetOutputAt(n);
		if (!Output || Output->IsPendingKill())
			continue;

		switch (Output->GetType()) 
		{
			case EHoudiniOutputType::Mesh:
			{
				NewActors.Append(FHoudiniEngineBakeUtils::BakeHoudiniStaticMeshOutputToActors(Output));
			}
			break;

			case EHoudiniOutputType::Instancer:
			{
				NewActors.Append(FHoudiniEngineBakeUtils::BakeInstancerOutputToActors(Output));
			}
			break;

			case EHoudiniOutputType::Landscape:
			{
				// Simply detach all landscapes from the Houdini Asset Actor, 
				// Do not need to create actors for landscapes
				TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& OutputObjects = Output->GetOutputObjects();
				for (auto& CurrentOutputObj : OutputObjects)
				{
					UHoudiniLandscapePtr* LandscapePointer = Cast<UHoudiniLandscapePtr>(CurrentOutputObj.Value.OutputObject);
					if (!LandscapePointer)
						continue;

					ALandscapeProxy* Landscape = LandscapePointer->GetRawPtr();
					if (Landscape)
					{
						Landscape->DetachFromActor(FDetachmentTransformRules::KeepRelativeTransform);
						NewActors.Add(Landscape);
					}
				}
			}
			break;

			case EHoudiniOutputType::Skeletal:
			break;

			case EHoudiniOutputType::Curve:
			{
				NewActors.Append(FHoudiniEngineBakeUtils::BakeHoudiniCurveOutputToActors(Output));
			}
			break;

			case EHoudiniOutputType::Invalid:
			break;
		}
	}

	// Select the baked actors
	if (GEditor && NewActors.Num() > 0)
	{
		GEditor->SelectNone(false, true);
		for (AActor* NewActor : NewActors)
		{
			if (NewActor && !NewActor->IsPendingKill())
				GEditor->SelectActor(NewActor, true, false);
		}
		GEditor->NoteSelectionChange();
	}

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
FHoudiniEngineBakeUtils::ReplaceHoudiniActorWithFoliage(UHoudiniAssetComponent* HoudiniAssetComponent) 
{
	if (!HoudiniAssetComponent || HoudiniAssetComponent->IsPendingKill())
		return false;

	if (FHoudiniEngineBakeUtils::BakeHoudiniActorToFoliage(HoudiniAssetComponent)) 
	{
		return FHoudiniEngineBakeUtils::DeleteBakedHoudiniAssetActor(HoudiniAssetComponent);
	}

	return false;
}

bool 
FHoudiniEngineBakeUtils::BakeHoudiniActorToFoliage(UHoudiniAssetComponent* HoudiniAssetComponent) 
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

	// Map storing original and baked Static Meshes
	TMap< const UStaticMesh*, UStaticMesh* > OriginalToBakedMesh;
	for (int32 OutputIdx = 0; OutputIdx < HoudiniAssetComponent->GetNumOutputs(); OutputIdx++)
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

			// See if we have a bake name for that output
			FHoudiniOutputObject* OutputObj = OutputObjects.Find(Pair.Key);
			if (OutputObj && OutputObj->BakeName.IsEmpty())
				InstanceName = OutputObj->BakeName;

			FHoudiniInstancedOutput& InstancedOutput = Pair.Value;
			for (int32 VariarionIdx = 0; VariarionIdx < InstancedOutput.VariationObjects.Num(); ++VariarionIdx)
			{
				// TODO: !!! what if the instanced object/var is not a static mesh!!!!!!
				UObject* CurrentVariationObject = InstancedOutput.VariationObjects[VariarionIdx].Get();
				UStaticMesh * OutStaticMesh = Cast<UStaticMesh>(CurrentVariationObject);
				if (!OutStaticMesh)
				{
					if (CurrentVariationObject)
					{
						HOUDINI_LOG_ERROR(TEXT("Failed to bake the instances of %s to Foliage"), *CurrentVariationObject->GetName());
					}
					continue;
				}

				// See if the instanced static mesh is still a temporary Houdini created Static Mesh
				// If it is, we will need to bake the StaticMesh first before baking to foliage
				bool bIsTemporaryStaticMesh = IsObjectTemporary(OutStaticMesh, HoudiniAssetComponent);
				if (bIsTemporaryStaticMesh)
				{
					// Bake the Houdini generated static mesh
					FHoudiniPackageParams PackageParams;
					PackageParams.BakeFolder = HoudiniAssetComponent->BakeFolder.Path;
					FHoudiniEngineBakeUtils::FillInPackageParamsForBakingOutput(PackageParams, Pair.Key, HoudiniAssetComponent->BakeFolder.Path, 
						 InstanceName + "_" + OutStaticMesh->GetName(), OwnerActor->GetName());
					FHoudiniEngineBakeUtils::DuplicateStaticMeshAndCreatePackage(OutStaticMesh, PackageParams, PackagesToSave);
				}

				// See if we already have a FoliageType for that static mesh
				UFoliageType *FoliageType = InstancedFoliageActor->GetLocalFoliageTypeForSource(OutStaticMesh);
				if (!FoliageType || FoliageType->IsPendingKill()) 
				{
					// We need to create a new FoliageType for this Static Mesh
					// TODO: Add foliage default settings
					InstancedFoliageActor->AddMesh(OutStaticMesh, &FoliageType);
				}

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
	
	if (BakedCount > 0)
	{
		FHoudiniEngineBakeUtils::SaveBakedPackages(PackagesToSave);
		return true;
	}

	return false;
}


TArray<AActor*>
FHoudiniEngineBakeUtils::BakeInstancerOutputToActors(UHoudiniOutput * InOutput)
{
	TArray<AActor*> NewActors;
	if (!InOutput || InOutput->IsPendingKill())
		return NewActors;

	UHoudiniAssetComponent * HoudiniAssetComponent = Cast<UHoudiniAssetComponent>(InOutput->GetOuter());
	if (!HoudiniAssetComponent || HoudiniAssetComponent->IsPendingKill())
		return NewActors;

	TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& OutputObjects = InOutput->GetOutputObjects();

	// Iterate on the output obkects, baking their object/component as we go
	for (auto& Pair : OutputObjects)
	{
		FHoudiniOutputObject& CurrentOutputObject = Pair.Value;

		if (CurrentOutputObject.bProxyIsCurrent)
		{
			// TODO: we need to refine the SM first!
			// ?? 
		}

		if (!CurrentOutputObject.OutputComponent || CurrentOutputObject.OutputComponent->IsPendingKill())
			continue;

		if (CurrentOutputObject.OutputComponent->IsA<UInstancedStaticMeshComponent>())
		{
			NewActors.Append(BakeInstancerOutputToActors_ISMC(Pair.Key, CurrentOutputObject, HoudiniAssetComponent));
		}
		else if (CurrentOutputObject.OutputComponent->IsA<UHoudiniInstancedActorComponent>())
		{
			NewActors.Append(BakeInstancerOutputToActors_IAC(Pair.Key, CurrentOutputObject, HoudiniAssetComponent));
		}
		else if (CurrentOutputObject.OutputComponent->IsA<UHoudiniMeshSplitInstancerComponent>())
		{
			NewActors.Append(BakeInstancerOutputToActors_MSIC(Pair.Key, CurrentOutputObject, HoudiniAssetComponent));
		}
		else if (CurrentOutputObject.OutputComponent->IsA<UStaticMeshComponent>())
		{
			NewActors.Append(BakeInstancerOutputToActors_SMC(Pair.Key, CurrentOutputObject, HoudiniAssetComponent));
		}
		else
		{
			// Unsupported component!
		}

	}

	return NewActors;
}

TArray<AActor*>
FHoudiniEngineBakeUtils::BakeInstancerOutputToActors_ISMC(
	const FHoudiniOutputObjectIdentifier& InOutputObjectIdentifier,
	const FHoudiniOutputObject& InOutputObject,
	UHoudiniAssetComponent* InHAC)
{
	TArray<AActor*> NewActors;
	if (!InHAC || InHAC->IsPendingKill())
		return NewActors;

	UInstancedStaticMeshComponent * InISMC = Cast<UInstancedStaticMeshComponent>(InOutputObject.OutputComponent);
	if (!InISMC || InISMC->IsPendingKill())
		return NewActors;

	AActor * OwnerActor = InISMC->GetOwner();
	if (!OwnerActor || OwnerActor->IsPendingKill())
		return NewActors;

	// BaseName holds the Actor / HDA name
	// Instancer name adds the split identifier (INSTANCERNUM_VARIATIONNUM)
	FString BaseName = OwnerActor->GetName();
	FString InstancerName = BaseName + "_instancer_" + InOutputObjectIdentifier.SplitIdentifier;

	UStaticMesh * StaticMesh = InISMC->GetStaticMesh();
	if (!StaticMesh || StaticMesh->IsPendingKill())
		return NewActors;
	
	TArray<UPackage*> PackagesToSave;

	// See if the instanced static mesh is still a temporary Houdini created Static Mesh
	// If it is, we need to bake the StaticMesh first
	bool bIsTemporaryStaticMesh = IsObjectTemporary(StaticMesh, InHAC);

	UStaticMesh* BakedStaticMesh = nullptr;
	if (bIsTemporaryStaticMesh)
	{
		FString BakeFolder = InHAC->BakeFolder.Path;

		// Bake the Houdini generated static mesh
		FHoudiniPackageParams PackageParams;
		PackageParams.BakeFolder = InHAC->BakeFolder.Path;
		FHoudiniEngineBakeUtils::FillInPackageParamsForBakingOutput(
			PackageParams,
			InOutputObjectIdentifier,
			BakeFolder,
			BaseName + "_" + StaticMesh->GetName(),
			OwnerActor->GetName());

		BakedStaticMesh = FHoudiniEngineBakeUtils::DuplicateStaticMeshAndCreatePackage(StaticMesh, PackageParams, PackagesToSave);
	}
	else
	{
		// Static Mesh is not a temporary one /  already baked, we can reuse it
		BakedStaticMesh = StaticMesh;
	}

	ULevel* DesiredLevel = GWorld->GetCurrentLevel();
	if (!DesiredLevel)
		return NewActors;

	// Should we create one actor with an ISMC or multiple actors with one SMC?
	bool bSpawnMultipleSMC = false;
	if (bSpawnMultipleSMC)
	{
		// TODO: Double check, Has a crash here!

		// Get the StaticMesh ActorFactory
		UActorFactory* SMFactory = GEditor ? GEditor->FindActorFactoryByClass(UActorFactoryStaticMesh::StaticClass()) : nullptr;
		if (!SMFactory)
			return NewActors;

		// Split the instances to multiple StaticMeshActors
		for (int32 InstanceIdx = 0; InstanceIdx < InISMC->GetInstanceCount(); InstanceIdx++)
		{
			FTransform InstanceTransform;
			InISMC->GetInstanceTransform(InstanceIdx, InstanceTransform, true);

			AActor* NewActor = SMFactory->CreateActor(BakedStaticMesh, DesiredLevel, InstanceTransform, RF_Transactional);
			if (!NewActor || NewActor->IsPendingKill())
				continue;

			FName NewName = MakeUniqueObjectName(DesiredLevel, SMFactory->NewActorClass, FName(InstancerName));
			NewActor->Rename(*NewName.ToString());
			NewActor->SetActorLabel(NewName.ToString());

			// The folder is named after the original actor and contains all generated actors
			NewActor->SetFolderPath(FName(BaseName));

			AStaticMeshActor* SMActor = Cast<AStaticMeshActor>(NewActor);
			if (!SMActor || SMActor->IsPendingKill())
				continue;

			// Copy properties from the existing component
			CopyPropertyToNewActorAndComponent(NewActor, SMActor->GetStaticMeshComponent(), InISMC);

			NewActors.Add(NewActor);
		}
	}
	else
	{
		// Only create one actor, with a UInstancedStaticMeshComponent root
		FActorSpawnParameters SpawnInfo;
		SpawnInfo.OverrideLevel = DesiredLevel;
		SpawnInfo.ObjectFlags = RF_Transactional;
		SpawnInfo.Name = MakeUniqueObjectName(DesiredLevel, AActor::StaticClass(), FName(InstancerName));
		SpawnInfo.bDeferConstruction = true;

		// Spawn the new Actor
		AActor* NewActor = DesiredLevel->OwningWorld->SpawnActor<AActor>(SpawnInfo);
		if (!NewActor || NewActor->IsPendingKill())
			return NewActors;

		NewActor->SetActorLabel(NewActor->GetName());
		NewActor->SetActorHiddenInGame(InISMC->bHiddenInGame);

		// Duplicate the instancer component, create a Hierarchical ISMC if needed
		UInstancedStaticMeshComponent* NewISMC = nullptr;
		UHierarchicalInstancedStaticMeshComponent* InHISMC = Cast<UHierarchicalInstancedStaticMeshComponent>(InISMC);
		if (InHISMC)
		{
			NewISMC = DuplicateObject<UHierarchicalInstancedStaticMeshComponent>(InHISMC, NewActor, *InISMC->GetName());
		}
		else
		{
			NewISMC = DuplicateObject<UInstancedStaticMeshComponent>(InISMC, NewActor, *InISMC->GetName());
		}

		if (!NewISMC)
		{
			//DesiredLevel->OwningWorld->
			return NewActors;
		}

		NewISMC->SetupAttachment(nullptr);
		NewISMC->SetStaticMesh(BakedStaticMesh);
		NewActor->AddInstanceComponent(NewISMC);
		NewActor->SetRootComponent(NewISMC);
		NewISMC->SetWorldTransform(InISMC->GetComponentTransform());

		// Copy properties from the existing component
		CopyPropertyToNewActorAndComponent(NewActor, NewISMC, InISMC);

		NewISMC->RegisterComponent();

		// The folder is named after the original actor and contains all generated actors
		NewActor->SetFolderPath(FName(BaseName));
		NewActor->FinishSpawning(InISMC->GetComponentTransform());

		NewActors.Add(NewActor);

		NewActor->InvalidateLightingCache();
		NewActor->PostEditMove(true);
		NewActor->MarkPackageDirty();
	}

	FHoudiniEngineBakeUtils::SaveBakedPackages(PackagesToSave);

	return NewActors;
}

TArray<AActor*>
FHoudiniEngineBakeUtils::BakeInstancerOutputToActors_SMC(
	const FHoudiniOutputObjectIdentifier& InOutputObjectIdentifier,
	const FHoudiniOutputObject& InOutputObject,
	UHoudiniAssetComponent* InHAC)
{
	TArray<AActor*> NewActors;
	if (!InHAC || InHAC->IsPendingKill())
		return NewActors;

	UStaticMeshComponent* InSMC = Cast<UStaticMeshComponent>(InOutputObject.OutputComponent);
	if (!InSMC || InSMC->IsPendingKill())
		return NewActors;

	AActor* OwnerActor = InSMC->GetOwner();
	if (!OwnerActor || OwnerActor->IsPendingKill())
		return NewActors;

	// BaseName holds the Actor / HDA name
	// Instancer name adds the split identifier (INSTANCERNUM_VARIATIONNUM)
	FString BaseName = OwnerActor->GetName();
	FString InstancerName = BaseName + "_instancer_" + InOutputObjectIdentifier.SplitIdentifier;

	UStaticMesh* StaticMesh = InSMC->GetStaticMesh();
	if (!StaticMesh || StaticMesh->IsPendingKill())
		return NewActors;
	
	// See if the instanced static mesh is still a temporary Houdini created Static Mesh
	// If it is, we need to bake the StaticMesh first
	bool bIsTemporaryStaticMesh = IsObjectTemporary(StaticMesh, InHAC);

	TArray<UPackage*> PackagesToSave;
	UStaticMesh* BakedStaticMesh = nullptr;
	if (bIsTemporaryStaticMesh)
	{
		FString BakeFolder = InHAC->BakeFolder.Path;

		// Bake the Houdini generated static mesh
		FHoudiniPackageParams PackageParams;
		PackageParams.BakeFolder = InHAC->BakeFolder.Path;
		FHoudiniEngineBakeUtils::FillInPackageParamsForBakingOutput(
			PackageParams,
			InOutputObjectIdentifier,
			BakeFolder,
			BaseName + "_" + StaticMesh->GetName(),
			OwnerActor->GetName());

		BakedStaticMesh = FHoudiniEngineBakeUtils::DuplicateStaticMeshAndCreatePackage(StaticMesh, PackageParams, PackagesToSave);
	}
	else
	{
		// Static Mesh is not a temporary one/already baked, we can reuse it
		BakedStaticMesh = StaticMesh;
	}

	ULevel* DesiredLevel = GWorld->GetCurrentLevel();
	if (!DesiredLevel)
		return NewActors;

	// Get the StaticMesh ActorFactory
	UActorFactory* SMFactory = GEditor ? GEditor->FindActorFactoryByClass(UActorFactoryStaticMesh::StaticClass()) : nullptr;
	if (!SMFactory)
		return NewActors;

	AActor* NewActor = SMFactory->CreateActor(BakedStaticMesh, DesiredLevel, InSMC->GetComponentTransform(), RF_Transactional);
	if (!NewActor || NewActor->IsPendingKill())
		return NewActors;

	FName NewName = MakeUniqueObjectName(DesiredLevel, SMFactory->NewActorClass, FName(InstancerName));
	NewActor->Rename(*NewName.ToString());
	NewActor->SetActorLabel(NewName.ToString());

	// The folder is named after the original actor and contains all generated actors
	NewActor->SetFolderPath(FName(BaseName));

	AStaticMeshActor* SMActor = Cast<AStaticMeshActor>(NewActor);
	if (!SMActor || SMActor->IsPendingKill())
		return NewActors;

	// Copy properties from the existing component
	CopyPropertyToNewActorAndComponent(NewActor, SMActor->GetStaticMeshComponent(), InSMC);

	NewActors.Add(NewActor);
	
	FHoudiniEngineBakeUtils::SaveBakedPackages(PackagesToSave);

	return NewActors;
}

TArray<AActor*>
FHoudiniEngineBakeUtils::BakeInstancerOutputToActors_IAC(
	const FHoudiniOutputObjectIdentifier& InOutputObjectIdentifier,
	const FHoudiniOutputObject& InOutputObject,
	UHoudiniAssetComponent* InHAC)
{
	TArray<AActor*> NewActors;
	if (!InHAC || InHAC->IsPendingKill())
		return NewActors;

	UHoudiniInstancedActorComponent* InIAC = Cast<UHoudiniInstancedActorComponent>(InOutputObject.OutputComponent);
	if (!InIAC || InIAC->IsPendingKill())
		return NewActors;
	
	ULevel* DesiredLevel = GWorld->GetCurrentLevel();
	if (!DesiredLevel)
		return NewActors;

	AActor * OwnerActor = InIAC->GetOwner();
	if (!OwnerActor || OwnerActor->IsPendingKill())
		return NewActors;

	// BaseName holds the Actor / HDA name
	FName BaseName = FName(OwnerActor->GetName());

	// Get the object instanced by this IAC
	UObject* InstancedObject = InIAC->GetInstancedObject();
	if (!InstancedObject || InstancedObject->IsPendingKill())
		return NewActors;

	// Iterates on all the instances of the IAC
	for (AActor* CurrentInstancedActor : InIAC->GetInstancedActors())
	{
		if (!CurrentInstancedActor || CurrentInstancedActor->IsPendingKill())
			continue;

		FName NewInstanceName = MakeUniqueObjectName(DesiredLevel, InstancedObject->StaticClass(), BaseName);
		FString NewNameStr = NewInstanceName.ToString();

		FTransform CurrentTransform = CurrentInstancedActor->GetTransform();
		AActor* NewActor = FHoudiniInstanceTranslator::SpawnInstanceActor(CurrentTransform, DesiredLevel, InIAC);
		if (!NewActor || NewActor->IsPendingKill())
			continue;

		NewActor->SetActorLabel(NewNameStr);
		NewActor->SetFolderPath(BaseName);
		NewActor->SetActorTransform(CurrentTransform);

		NewActors.Add(NewActor);
	}

	return NewActors;
}

TArray<AActor*>
FHoudiniEngineBakeUtils::BakeInstancerOutputToActors_MSIC(
	const FHoudiniOutputObjectIdentifier& InOutputObjectIdentifier,
	const FHoudiniOutputObject& InOutputObject,
	UHoudiniAssetComponent* InHAC)
{
	TArray<AActor*> NewActors;
	if (!InHAC || InHAC->IsPendingKill())
		return NewActors;

	UHoudiniMeshSplitInstancerComponent * InMSIC = Cast<UHoudiniMeshSplitInstancerComponent>(InOutputObject.OutputComponent);
	if (!InMSIC || InMSIC->IsPendingKill())
		return NewActors;

	AActor * OwnerActor = InMSIC->GetOwner();
	if (!OwnerActor || OwnerActor->IsPendingKill())
		return NewActors;

	// BaseName holds the Actor / HDA name
	// Instancer name adds the split identifier (INSTANCERNUM_VARIATIONNUM)
	FString BaseName = OwnerActor->GetName();
	FString InstancerName = BaseName + "_instancer_" + InOutputObjectIdentifier.SplitIdentifier;

	UStaticMesh * StaticMesh = InMSIC->GetStaticMesh();
	if (!StaticMesh || StaticMesh->IsPendingKill())
		return NewActors;

	TArray<UPackage*> PackagesToSave;

	// See if the instanced static mesh is still a temporary Houdini created Static Mesh
	// If it is, we need to bake the StaticMesh first
	bool bIsTemporaryStaticMesh = IsObjectTemporary(StaticMesh, InHAC);

	UStaticMesh* BakedStaticMesh = nullptr;
	if (bIsTemporaryStaticMesh)
	{
		FString BakeFolder = InHAC->BakeFolder.Path;

		// Bake the Houdini generated static mesh
		FHoudiniPackageParams PackageParams;
		PackageParams.BakeFolder = InHAC->BakeFolder.Path;
		FHoudiniEngineBakeUtils::FillInPackageParamsForBakingOutput(
			PackageParams,
			InOutputObjectIdentifier,
			BakeFolder,
			BaseName + "_" + StaticMesh->GetName(),
			OwnerActor->GetName());

		BakedStaticMesh = FHoudiniEngineBakeUtils::DuplicateStaticMeshAndCreatePackage(StaticMesh, PackageParams, PackagesToSave);
	}
	else
	{
		// Static Mesh is not a temporary one /  already baked, we can reuse it
		BakedStaticMesh = StaticMesh;
	}

	ULevel* DesiredLevel = GWorld->GetCurrentLevel();
	if (!DesiredLevel)
		return NewActors;

	// This is a split mesh instancer component - we will create a generic AActor with a bunch of SMC
	FActorSpawnParameters SpawnInfo;
	SpawnInfo.OverrideLevel = DesiredLevel;
	SpawnInfo.ObjectFlags = RF_Transactional;
	SpawnInfo.Name = MakeUniqueObjectName(DesiredLevel, AActor::StaticClass(), FName(InstancerName));
	SpawnInfo.bDeferConstruction = true;

	// Spawn the new Actor
	AActor* NewActor = DesiredLevel->OwningWorld->SpawnActor<AActor>(SpawnInfo);
	if (!NewActor || NewActor->IsPendingKill())
		return NewActors;

	NewActor->SetActorLabel(NewActor->GetName());
	NewActor->SetActorHiddenInGame(InMSIC->bHiddenInGame);

	// Now add s SMC component for each of the SMC's instance
	for (UStaticMeshComponent* CurrentSMC : InMSIC->GetInstances())
	{
		if (!CurrentSMC || CurrentSMC->IsPendingKill())
			continue;

		UStaticMeshComponent* NewSMC = DuplicateObject<UStaticMeshComponent>(CurrentSMC, NewActor, *CurrentSMC->GetName());
		if (!NewSMC || NewSMC->IsPendingKill())
			continue;

		NewSMC->SetupAttachment(nullptr);
		NewSMC->SetStaticMesh(BakedStaticMesh);
		NewActor->AddInstanceComponent(NewSMC);
		NewSMC->SetWorldTransform(CurrentSMC->GetComponentTransform());

		// Copy properties from the existing component
		CopyPropertyToNewActorAndComponent(NewActor, NewSMC, CurrentSMC);

		NewSMC->RegisterComponent();
	}

	// The folder is named after the original actor and contains all generated actors
	NewActor->SetFolderPath(FName(BaseName));
	NewActor->FinishSpawning(InMSIC->GetComponentTransform());

	NewActors.Add(NewActor);

	NewActor->InvalidateLightingCache();
	NewActor->PostEditMove(true);
	NewActor->MarkPackageDirty();

	FHoudiniEngineBakeUtils::SaveBakedPackages(PackagesToSave);

	return NewActors;
}


TArray<AActor*> 
FHoudiniEngineBakeUtils::BakeHoudiniStaticMeshOutputToActors(
	UHoudiniOutput* Output) 
{
	TArray<AActor*> NewActors;
	if (!Output || Output->IsPendingKill())
		return NewActors;

	UHoudiniAssetComponent * HAC = Cast<UHoudiniAssetComponent>(Output->GetOuter());
	if (!HAC || HAC->IsPendingKill())
		return NewActors;

	AActor * OwnerActor = HAC->GetOwner();
	if (!OwnerActor || OwnerActor->IsPendingKill())
		return NewActors;

	ULevel* DesiredLevel = GWorld->GetCurrentLevel();
	if (!DesiredLevel || DesiredLevel->IsPendingKill())
		return NewActors;

	UActorFactory* Factory = GEditor ? GEditor->FindActorFactoryByClass(UActorFactoryStaticMesh::StaticClass()) : nullptr;
	if (!Factory)
		return NewActors;

	TArray<UPackage*> PackagesToSave;
	TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& OutputObjects = Output->GetOutputObjects();
	const TArray<FHoudiniGeoPartObject>& HGPOs = Output->GetHoudiniGeoPartObjects();

	for (auto& Pair : OutputObjects)
	{
		FHoudiniOutputObjectIdentifier& Identifier = Pair.Key;

		UStaticMesh* StaticMesh = Cast<UStaticMesh>(Pair.Value.OutputObject);
		if (!StaticMesh || StaticMesh->IsPendingKill())
			continue;

		UStaticMeshComponent* InSMC = Cast<UStaticMeshComponent>(Pair.Value.OutputComponent);
		if (!InSMC || InSMC->IsPendingKill())
			continue;

		// Find the HGPO that matches this output identifier
		const FHoudiniGeoPartObject* FoundHGPO = nullptr;
		for (auto & NextHGPO : HGPOs) 
		{
			// We use Matches() here as it handles the case where the HDA was loaded,
			// which likely means that the the obj/geo/part ids dont match the output identifier
			if(Identifier.Matches(NextHGPO))
			{
				FoundHGPO = &NextHGPO;
				break;
			}
		}

		FString SMName = Pair.Value.BakeName;
		if (SMName.IsEmpty())
		{
			if (FoundHGPO && FoundHGPO->bHasCustomPartName)
				SMName = FoundHGPO->PartName;
			else
				SMName = StaticMesh->GetName();
		}

		// Bake the static mesh
		FHoudiniPackageParams PackageParams;
		FHoudiniEngineBakeUtils::FillInPackageParamsForBakingOutput(
			PackageParams, Identifier, HAC->BakeFolder.Path, SMName, OwnerActor->GetName());

		UStaticMesh* BakedSM = FHoudiniEngineBakeUtils::DuplicateStaticMeshAndCreatePackage(
			StaticMesh, PackageParams, PackagesToSave);

		if (!BakedSM || BakedSM->IsPendingKill())
			continue;

		// Spawn actor
		FName BaseName(*(PackageParams.ObjectName));

		// Remove a previous bake actor if it exists
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

		AActor* NewActor = Factory->CreateActor(BakedSM, DesiredLevel, InSMC->GetComponentTransform(), RF_Standalone | RF_Public);
		if (!NewActor || NewActor->IsPendingKill())
			continue;

		// FName NewName = MakeUniqueObjectName(DesiredLevel, Factory->NewActorClass, *PackageParams.ObjectName);
		NewActor->Rename(*(PackageParams.ObjectName));
		NewActor->SetActorLabel(PackageParams.ObjectName);
		NewActor->SetFolderPath(FName(OwnerActor->GetName()));

		// Copy properties to new actor
		AStaticMeshActor* SMActor = Cast<AStaticMeshActor>(NewActor);
		if (!SMActor || SMActor->IsPendingKill())
			continue;

		UStaticMeshComponent* NewSMC = SMActor->GetStaticMeshComponent();
		if (!NewSMC || NewSMC->IsPendingKill())
			continue;

		CopyPropertyToNewActorAndComponent(NewActor, NewSMC, InSMC);

		NewActors.Add(NewActor);
	}

	FHoudiniEngineBakeUtils::SaveBakedPackages(PackagesToSave);

	return NewActors;
}

TArray<AActor*> 
FHoudiniEngineBakeUtils::BakeHoudiniCurveOutputToActors(
	UHoudiniOutput* Output) 
{
	TArray<AActor*> CreatedActors;
	if (!Output || Output->IsPendingKill())
		return CreatedActors;

	UHoudiniAssetComponent * HAC = Cast<UHoudiniAssetComponent>(Output->GetOuter());
	if (!HAC || HAC->IsPendingKill())
		return CreatedActors;

	AActor * OwnerActor = HAC->GetOwner();
	if (!OwnerActor || OwnerActor->IsPendingKill())
		return CreatedActors;

	TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& OutputObjects = Output->GetOutputObjects();
	const TArray<FHoudiniGeoPartObject> & HGPOs = Output->GetHoudiniGeoPartObjects();

	for (auto & Pair : OutputObjects) 
	{
		USplineComponent* SplineComponent = Cast<USplineComponent>(Pair.Value.OutputComponent);
		if (!SplineComponent || SplineComponent->IsPendingKill())
			continue;
		
		FHoudiniOutputObjectIdentifier & Identifier = Pair.Key;

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
				CurveName = OwnerActor->GetName() + "_" + SplineComponent->GetName();
		}		

		FHoudiniPackageParams PackageParams;
		FHoudiniEngineBakeUtils::FillInPackageParamsForBakingOutput(
			PackageParams, Identifier, HAC->BakeFolder.Path, CurveName, OwnerActor->GetName());

		AActor *CreatedActor = FHoudiniEngineBakeUtils::BakeCurve( SplineComponent, PackageParams);
		if (CreatedActor && !CreatedActor->IsPendingKill())
			CreatedActors.Add(CreatedActor);
	}

	return CreatedActors;
}

bool 
FHoudiniEngineBakeUtils::CopyActorContentsToBlueprint(AActor * InActor, UBlueprint * OutBlueprint) 
{
	if (!InActor || InActor->IsPendingKill())
		return false;

	if (!OutBlueprint || OutBlueprint->IsPendingKill())
		return false;

	if (InActor->GetInstanceComponents().Num() > 0)
		FKismetEditorUtilities::AddComponentsToBlueprint(OutBlueprint, InActor->GetInstanceComponents());

	if (OutBlueprint->GeneratedClass)
	{
		AActor * CDO = Cast< AActor >(OutBlueprint->GeneratedClass->GetDefaultObject());
		if (!CDO || CDO->IsPendingKill())
			return nullptr;

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
			Scene->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
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
		}
	}

	// Compile our blueprint and notify asset system about blueprint.
	//FKismetEditorUtilities::CompileBlueprint(OutBlueprint);
	//FAssetRegistryModule::AssetCreated(OutBlueprint);

	return true;
}

AActor*
FHoudiniEngineBakeUtils::ReplaceWithBlueprint(UHoudiniAssetComponent* HoudiniAssetComponent) 
{
	if (!HoudiniAssetComponent || HoudiniAssetComponent->IsPendingKill())
		return nullptr;

	AActor * OwnerActor = HoudiniAssetComponent->GetOwner();
	if (!OwnerActor || OwnerActor->IsPendingKill())
		return nullptr;

	TArray<UPackage*> PackagesToSave;

	AActor * Actor = nullptr;

	bool bFoundPackage = false;
	FString BlueprintName;
	UPackage * Package = FHoudiniEngineBakeUtils::BakeCreateBlueprintPackageForComponent(HoudiniAssetComponent, BlueprintName, bFoundPackage);
	if (!Package || Package->IsPendingKill())
		return nullptr;

	if (!Package->IsFullyLoaded())
		Package->FullyLoad();

	// find the BP asset first
	// create new if not found
	UObject* Asset = nullptr;
	Asset = StaticFindObjectFast(UBlueprint::StaticClass(), Package, FName(*BlueprintName));
	if (!Asset)
	{
		UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();

		FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");

		Asset = AssetToolsModule.Get().CreateAsset(
			BlueprintName, HoudiniAssetComponent->BakeFolder.Path,
			UBlueprint::StaticClass(), Factory, FName("ContentBrowserNewAsset"));
	}

	UBlueprint* Blueprint = Cast<UBlueprint>(Asset);
	if (!Blueprint || Blueprint->IsPendingKill())
		return nullptr;

	AActor * ClonedActor = FHoudiniEngineBakeUtils::CloneComponentsAndCreateActor(HoudiniAssetComponent, PackagesToSave);
	if (!ClonedActor || ClonedActor->IsPendingKill())
		return nullptr;

	// Clear old Blueprint Node tree
	{
		USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;

		int32 NodeSize = SCS->GetAllNodes().Num();
		for (int32 n = NodeSize - 1; n >= 0; --n)
			SCS->RemoveNode(SCS->GetAllNodes()[n]);
	}


	FHoudiniEngineBakeUtils::CopyActorContentsToBlueprint(ClonedActor, Blueprint);

	// Retrieve actor transform.
	FVector Location = ClonedActor->GetActorLocation();
	FRotator Rotator = ClonedActor->GetActorRotation();

	// Replace cloned actor with Blueprint instance.
	{
		TArray< AActor * > Actors;
		Actors.Add(ClonedActor);

		ClonedActor->RemoveFromRoot();
		Actor = FKismetEditorUtilities::CreateBlueprintInstanceFromSelection(Blueprint, Actors, Location, Rotator);
		Actor->SetFolderPath(FName(OwnerActor->GetName()));
	}

	// Delete Houdini Actor
	FHoudiniEngineBakeUtils::DeleteBakedHoudiniAssetActor(HoudiniAssetComponent);

	// Save the created BP package.
	Package->MarkPackageDirty();
	PackagesToSave.Add(Package);

	FHoudiniEngineBakeUtils::SaveBakedPackages(PackagesToSave);

	return Actor;
}

UBlueprint * 
FHoudiniEngineBakeUtils::BakeBlueprint(UHoudiniAssetComponent* HoudiniAssetComponent) 
{
	UBlueprint* Blueprint = nullptr;

	if (!HoudiniAssetComponent || HoudiniAssetComponent->IsPendingKill())
		return nullptr;

	TArray<UPackage*> PackagesToSave;
	// Create package for out Blueprint
	FString BlueprintName;

	bool bFoundPackage = false;
	UPackage* Package = FHoudiniEngineBakeUtils::BakeCreateBlueprintPackageForComponent(
		HoudiniAssetComponent, BlueprintName, bFoundPackage);
	
	if (!Package || Package->IsPendingKill())
		return nullptr;

	if (!Package->IsFullyLoaded())
		Package->FullyLoad();

	AActor* Actor = CloneComponentsAndCreateActor(HoudiniAssetComponent, PackagesToSave);

	UObject* Asset = nullptr;
	if (Actor && !Actor->IsPendingKill()) 
	{
		//Blueprint = FKismetEditorUtilities::CreateBlueprintFromActor(*BlueprintName, Package, Actor, false);
		// Find existing asset first,
		// Create new if not existed
		Asset = StaticFindObjectFast(UBlueprint::StaticClass(), Package, FName(*BlueprintName));
		if (!Asset)
		{
			UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();

			FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");

			Asset = AssetToolsModule.Get().CreateAsset(
				BlueprintName, HoudiniAssetComponent->BakeFolder.Path,
				UBlueprint::StaticClass(), Factory, FName("ContentBrowserNewAsset"));
		}

		Blueprint = Cast<UBlueprint>(Asset);

		if (!Blueprint || Blueprint->IsPendingKill())
			return nullptr;

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

		Actor->RemoveFromRoot();
		Actor->ConditionalBeginDestroy();
		World->EditorDestroyActor(Actor, true);
	}

	// Save the created BP package.
	Package->MarkPackageDirty();
	PackagesToSave.Add(Package);
	
	FHoudiniEngineBakeUtils::SaveBakedPackages(PackagesToSave);

	// Sync the CB to the baked objects
	if(GEditor)
	{
		TArray<UObject*> Objects;
		Objects.Add(Asset);
		GEditor->SyncBrowserToObjects(Objects);
	}

	return Blueprint;
}

UPackage* 
FHoudiniEngineBakeUtils::BakeCreateBlueprintPackageForComponent(
	UHoudiniAssetComponent* HoudiniAssetComponent,
	FString & BlueprintName, bool & bFoundPackage) 
{
	UPackage* Package = nullptr;

	if (!HoudiniAssetComponent || HoudiniAssetComponent->IsPendingKill())
		return nullptr;

	AActor * OwnerActor = HoudiniAssetComponent->GetOwner();
	if (!OwnerActor || OwnerActor->IsPendingKill())
		return nullptr;
	

	FGuid BakeGUID = FGuid::NewGuid();

	if (!BakeGUID.IsValid())
		BakeGUID = FGuid::NewGuid();

	// We only want half of generated guid string.
	FString BakeGUIDString = BakeGUID.ToString().Left(FHoudiniEngineUtils::PackageGUIDItemNameLength);

	// Generate Blueprint name.
	BlueprintName = OwnerActor->GetName() + "_BP";

	// Generate unique package name.
	FString PackageName = HoudiniAssetComponent->BakeFolder.Path + TEXT("/") + BlueprintName;
	PackageName = UPackageTools::SanitizePackageName(PackageName);

	// See if package exists, if it does, we need to regenerate the name.
	Package = FindPackage(nullptr, *PackageName);

	if (Package && !Package->IsPendingKill())
	{
		// Package does exist, there's a collision, we need to generate a new name.
		BakeGUID.Invalidate();
		bFoundPackage = true;
	}
	else
	{
		// Create actual package.
		Package = CreatePackage(nullptr, *PackageName);
		bFoundPackage = false;
	}

	return Package;
}

UStaticMesh* 
FHoudiniEngineBakeUtils::BakeStaticMesh(
	const UStaticMesh * StaticMesh,
	const FHoudiniPackageParams & PackageParams) 
{
	if (!StaticMesh || StaticMesh->IsPendingKill())
		return nullptr;

	TArray<UPackage*> PackagesToSave;
	UStaticMesh* BakedStaticMesh = FHoudiniEngineBakeUtils::DuplicateStaticMeshAndCreatePackage(StaticMesh, PackageParams, PackagesToSave);

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

UStaticMesh * 
FHoudiniEngineBakeUtils::DuplicateStaticMeshAndCreatePackage(
	const UStaticMesh * StaticMesh,
	const FHoudiniPackageParams &PackageParams,
	TArray<UPackage*> & OutCreatedPackages) 
{
	if (!StaticMesh || StaticMesh->IsPendingKill())
		return nullptr;

	UStaticMesh * DuplicatedStaticMesh = nullptr;
	
	FString CreatedPackageName;
	UPackage* MeshPackage = PackageParams.CreatePackageForObject(CreatedPackageName);

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

	// Duplicate mesh for this new copied component.
	DuplicatedStaticMesh = DuplicateObject< UStaticMesh >(StaticMesh, MeshPackage, *CreatedPackageName);
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
	TArray< FStaticMaterial > DuplicatedMaterials;
	TArray< FStaticMaterial > & Materials = DuplicatedStaticMesh->StaticMaterials;
		
	for (int32 MaterialIdx = 0; MaterialIdx < Materials.Num(); ++MaterialIdx)
	{
		UMaterialInterface* MaterialInterface = Materials[MaterialIdx].MaterialInterface;
		if (MaterialInterface)
		{
			UPackage * MaterialPackage = Cast< UPackage >(MaterialInterface->GetOuter());
			if (MaterialPackage && !MaterialPackage->IsPendingKill())
			{
				FString MaterialName;
				if (FHoudiniEngineBakeUtils::GetHoudiniGeneratedNameFromMetaInformation(
					MeshPackage, DuplicatedStaticMesh, MaterialName))
				{
					MaterialName = MaterialName + "_Material" + FString::FromInt(MaterialIdx+1);

					// We only deal with materials.
					UMaterial * Material = Cast< UMaterial >(MaterialInterface);
					if (Material && !Material->IsPendingKill())
					{
						// Duplicate material resource.
						UMaterial * DuplicatedMaterial = FHoudiniEngineBakeUtils::DuplicateMaterialAndCreatePackage(
							Material, MaterialName, PackageParams, OutCreatedPackages);
						
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

AActor* 
FHoudiniEngineBakeUtils::BakeCurve(
	USplineComponent* InSplineComponent,
	const FHoudiniPackageParams &PackageParams)
{
	if (!InSplineComponent || InSplineComponent->IsPendingKill())
		return nullptr;

	ULevel* DesiredLevel = GWorld->GetCurrentLevel();

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

	UActorFactory* Factory = GEditor ? GEditor->FindActorFactoryByClass(UActorFactoryEmptyActor::StaticClass()) : nullptr;
	if (!Factory)
		return nullptr;

	AActor* NewActor = Factory->CreateActor(nullptr, DesiredLevel, InSplineComponent->GetComponentTransform(), RF_Standalone | RF_Public, FName(PackageParams.ObjectName));
	
	USplineComponent* DuplicatedSplineComponent = DuplicateObject<USplineComponent>(InSplineComponent, NewActor, FName(PackageParams.ObjectName));
	NewActor->AddInstanceComponent(DuplicatedSplineComponent);
	DuplicatedSplineComponent->AttachToComponent(NewActor->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
	
	FAssetRegistryModule::AssetCreated(DuplicatedSplineComponent);
	DuplicatedSplineComponent->RegisterComponent();

	// The default name will be based on the static mesh package, we would prefer it to be based on the Houdini asset
	//FName NewName = MakeUniqueObjectName(DesiredLevel, Factory->NewActorClass, BaseName);
	//FString NewNameStr = NewName.ToString();
	NewActor->Rename(*(PackageParams.ObjectName));
	NewActor->SetActorLabel(PackageParams.ObjectName);
	NewActor->SetFolderPath(FName(PackageParams.HoudiniAssetName));

	return NewActor;
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

	AActor* NewActor = Factory->CreateActor(nullptr, DesiredLevel, InHoudiniSplineComponent->GetComponentTransform(), RF_Standalone | RF_Public);
	FString Name = NewActor->GetName();

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
	//FName NewName = MakeUniqueObjectName(DesiredLevel, Factory->NewActorClass, BaseName);
	//FString NewNameStr = NewName.ToString();
	NewActor->Rename(*PackageParams.ObjectName);
	NewActor->SetActorLabel(PackageParams.ObjectName);
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
		Package = CreatePackage(nullptr, *PackageName);
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
	UMaterial * Material, const FString & MaterialName, const FHoudiniPackageParams& ObjectPackageParams,
	TArray<UPackage*> & OutGeneratedPackages)
{
	UMaterial * DuplicatedMaterial = nullptr;

	FString CreatedMaterialName;
	// Create material package.  Use the same package params as static mesh, but with the material's name
	FHoudiniPackageParams MaterialPackageParams = ObjectPackageParams;
	MaterialPackageParams.ObjectName = MaterialName;

	UPackage * MaterialPackage = MaterialPackageParams.CreatePackageForObject(CreatedMaterialName);

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

	for (auto& Expression : DuplicatedMaterial->Expressions)
	{
		FHoudiniEngineBakeUtils::ReplaceDuplicatedMaterialTextureSample(
			Expression, MaterialPackageParams, OutGeneratedPackages);
	}

	// Notify registry that we have created a new duplicate material.
	FAssetRegistryModule::AssetCreated(DuplicatedMaterial);

	// Dirty the material package.
	DuplicatedMaterial->MarkPackageDirty();

	// Reset any derived state
	DuplicatedMaterial->ForceRecompileForRendering();

	OutGeneratedPackages.Add(MaterialPackage);

	return DuplicatedMaterial;
}

void
FHoudiniEngineBakeUtils::ReplaceDuplicatedMaterialTextureSample(
	UMaterialExpression * MaterialExpression, const FHoudiniPackageParams& PackageParams, TArray<UPackage*> & OutCreatedPackages)
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

	FString GeneratedTextureName;
	if (FHoudiniEngineBakeUtils::GetHoudiniGeneratedNameFromMetaInformation(
		TexturePackage, Texture, GeneratedTextureName))
	{
		// Duplicate texture.
		UTexture2D * DuplicatedTexture = FHoudiniEngineBakeUtils::DuplicateTextureAndCreatePackage(
			Texture, GeneratedTextureName, PackageParams, OutCreatedPackages);

		// Re-assign generated texture.
		TextureSample->Texture = DuplicatedTexture;
	}
}

UTexture2D *
FHoudiniEngineBakeUtils::DuplicateTextureAndCreatePackage(
	UTexture2D * Texture, const FString & SubTextureName, const FHoudiniPackageParams& PackageParams,
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

		UPackage * NewTexturePackage = TexturePackageParams.CreatePackageForObject(CreatedTextureName);

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


void 
FHoudiniEngineBakeUtils::FillInPackageParamsForBakingOutput(
	FHoudiniPackageParams& OutPackageParams,
	const FHoudiniOutputObjectIdentifier& InIdentifier,
	const FString &BakeFolder,
	const FString &ObjectName,
	const FString &HoudiniAssetName) 
{
	OutPackageParams.GeoId = InIdentifier.GeoId;
	OutPackageParams.ObjectId = InIdentifier.ObjectId;
	OutPackageParams.PartId = InIdentifier.PartId;
	OutPackageParams.BakeFolder = BakeFolder;
	OutPackageParams.PackageMode = EPackageMode::Bake;
	OutPackageParams.HoudiniAssetName = HoudiniAssetName;
	OutPackageParams.ObjectName = ObjectName;
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
		UPackage* CurrentWorldPackage = CreatePackage(nullptr, *CurrentWorldPath);

		if (CurrentWorldPackage)
		{
			CurrentWorldPackage->MarkPackageDirty();
			PackagesToSave.Add(CurrentWorldPackage);
		}
	}

	FEditorFileUtils::PromptForCheckoutAndSave(PackagesToSave, true, false);
}

bool
FHoudiniEngineBakeUtils::IsObjectTemporary(UObject* InObject, UHoudiniAssetComponent* InHAC)
{
	if (!InObject || InObject->IsPendingKill())
		return false;

	if (!InHAC || InHAC->IsPendingKill())
		return false;

	// See if the object has been produced by the HAC
	if (InHAC->HasOutputObject(InObject))
	{
		// This asset has been generated by the parent HAC
		return true;
	}
	else
	{
		// Check the package path for this SM
		// If it is in the HAC temp directory, assume it needs to be baked
		UPackage* SMPackage = InObject->GetOutermost();
		if (SMPackage && !SMPackage->IsPendingKill())
		{
			FString PathName = SMPackage->GetPathName();
			FString TempPath = InHAC->TemporaryCookFolder.Path;
			if (PathName.StartsWith(TempPath))
				return true;
		}
	}

	// TODO: 
	// Look in the meta info as well??

	return false;
}


void 
FHoudiniEngineBakeUtils::CopyPropertyToNewActorAndComponent(AActor* NewActor, UStaticMeshComponent* NewSMC, UStaticMeshComponent* InSMC)
{
	if (!NewActor || NewActor->IsPendingKill())
		return;

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

	NewActor->SetActorHiddenInGame(InSMC->bHiddenInGame);
	NewSMC->SetVisibility(InSMC->IsVisible());

	// TODO:
	// Reapply the uproperties modified by attributes on the new component
	//FHoudiniEngineUtils::UpdateUPropertyAttributesOnObject(SMC, HoudiniGeoPartObject);

	NewSMC->PostEditChange();
};