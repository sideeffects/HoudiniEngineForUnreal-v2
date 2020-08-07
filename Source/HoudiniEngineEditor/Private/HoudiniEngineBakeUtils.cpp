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
#include "HoudiniPDGAssetLink.h"
#include "HoudiniStringResolver.h"

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
#include "Editor/EditorEngine.h"
#include "Factories/BlueprintFactory.h"
#include "Engine/SimpleConstructionScript.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "LandscapeEdit.h"
#include "Containers/UnrealString.h"
#include "Components/AudioComponent.h"
#include "Particles/ParticleSystemComponent.h"
#include "Sound/SoundBase.h"

#define LOCTEXT_NAMESPACE HOUDINI_LOCTEXT_NAMESPACE

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
		NewObject<USceneComponent>(Actor, USceneComponent::GetDefaultSceneRootVariableName(), RF_Transactional);

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

					// We do not bake templated geos
					if (FoundHGPO->bIsTemplated)
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
					FHoudiniEngineUtils::FillInPackageParamsForBakingOutput(
						PackageParams, Identifier, HoudiniAssetComponent->BakeFolder.Path, SMName, AssetActor->GetName());

					UStaticMesh* OutStaticMesh = FHoudiniEngineBakeUtils::DuplicateStaticMeshAndCreatePackageIfNeeded(
						StaticMesh, PackageParams, HoudiniAssetComponent, OutCreatedPackages);
					if (!OutStaticMesh || OutStaticMesh->IsPendingKill())
						continue;

					// Duplicate the source static mesh component for baked mesh.
					UStaticMeshComponent * DuplicatedComponent =
						DuplicateObject<UStaticMeshComponent>(StaticMeshComponent, Actor, *StaticMeshComponent->GetName());
						//NewObject<UStaticMeshComponent>(Actor, UStaticMeshComponent::StaticClass(), NAME_None);//, RF_Transactional );						

					if (!DuplicatedComponent || DuplicatedComponent->IsPendingKill())
						continue;

					Actor->AddInstanceComponent(DuplicatedComponent);

					DuplicatedComponent->SetStaticMesh(OutStaticMesh);
					DuplicatedComponent->SetVisibility(true);
					//DuplicatedComponent->SetRelativeTransform(HoudiniGeoPartObject.TransformMatrix);

					// If this is a collision geo, we need to make it invisible.
					//if (HoudiniGeoPartObject.IsCollidable())
					//{
					//	DuplicatedComponent->SetVisibility(false);
					//	DuplicatedComponent->SetHiddenInGame(true);
					//	DuplicatedComponent->SetCollisionProfileName(FName(TEXT("InvisibleWall")));
					//}	

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
				// Iterate on the output objects, baking their object/component as we go
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

					// Instanced actors are treted separately from other instancers
					bool bIsInstancedActor = CurrentOutputObject.OutputComponent->IsA<UHoudiniInstancedActorComponent>();
					if (!bIsInstancedActor)
					{
						UStaticMesh* InstancedMesh = nullptr;
						UInstancedStaticMeshComponent* ISMC = nullptr;
						UHoudiniMeshSplitInstancerComponent* MSIC = nullptr;
						UStaticMeshComponent* SMC = nullptr;
						if (CurrentOutputObject.OutputComponent->IsA<UInstancedStaticMeshComponent>())
						{
							ISMC = Cast<UInstancedStaticMeshComponent>(CurrentOutputObject.OutputComponent);
							InstancedMesh = (ISMC && !ISMC->IsPendingKill()) ? ISMC->GetStaticMesh() : nullptr;
						}
						else if (CurrentOutputObject.OutputComponent->IsA<UHoudiniMeshSplitInstancerComponent>())
						{
							MSIC = Cast<UHoudiniMeshSplitInstancerComponent>(CurrentOutputObject.OutputComponent);
							InstancedMesh = (MSIC && !MSIC->IsPendingKill()) ? MSIC->GetStaticMesh() : nullptr;
						}
						else if (CurrentOutputObject.OutputComponent->IsA<UStaticMeshComponent>())
						{
							SMC = Cast<UStaticMeshComponent>(CurrentOutputObject.OutputComponent);
							InstancedMesh = (SMC && !SMC->IsPendingKill()) ? SMC->GetStaticMesh() : nullptr;
						}

						// 
						FHoudiniPackageParams PackageParams;
						FHoudiniEngineUtils::FillInPackageParamsForBakingOutput(
							PackageParams, 
							Pair.Key, 
							HoudiniAssetComponent->BakeFolder.Path,
							AssetActor->GetName() + "_" + InstancedMesh->GetName(),
							AssetActor->GetName());

						// We will only duplicate the SM if it is temporary
						UStaticMesh* BakedStaticMesh = FHoudiniEngineBakeUtils::DuplicateStaticMeshAndCreatePackageIfNeeded(
							InstancedMesh, PackageParams, HoudiniAssetComponent, OutCreatedPackages);

						if (ISMC && !ISMC->IsPendingKill())
						{
							// Do we need to create a Hierarchical ISMC ?
							UHierarchicalInstancedStaticMeshComponent * HISMC = Cast<UHierarchicalInstancedStaticMeshComponent>(CurrentOutputObject.OutputComponent);
							UInstancedStaticMeshComponent* NewISMC = nullptr;
							if (HISMC && !HISMC->IsPendingKill())
							{
								NewISMC = DuplicateObject<UHierarchicalInstancedStaticMeshComponent>(HISMC, Actor, *HISMC->GetName());
							}
							else
							{
								NewISMC = DuplicateObject<UInstancedStaticMeshComponent>(ISMC, Actor, *ISMC->GetName());
							}

							if (NewISMC && !NewISMC->IsPendingKill())
							{
								//NewISMC->SetupAttachment(nullptr);
								NewISMC->SetStaticMesh(BakedStaticMesh);
								Actor->AddInstanceComponent(NewISMC);

								// Reapply the uproperties modified by attributes on the duplicated component
								//FHoudiniEngineUtils::UpdateUPropertyAttributesOnObject(DuplicatedComponent, HoudiniGeoPartObject);

								//NewISMC->SetWorldTransform(ISMC->GetComponentTransform());
								NewISMC->SetComponentToWorld(ISMC->GetComponentTransform());

								// Copy properties from the existing component
								CopyPropertyToNewActorAndComponent(nullptr, NewISMC, ISMC);

								if (RootComponent && !RootComponent->IsPendingKill())
									NewISMC->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);

								NewISMC->RegisterComponent();

							}
						}
						else if (MSIC && !MSIC->IsPendingKill())
						{
							// Add a SMC component for each of the SMC's instance
							for (UStaticMeshComponent* CurrentSMC : MSIC->GetInstances())
							{
								if (!CurrentSMC || CurrentSMC->IsPendingKill())
									continue;

								UStaticMeshComponent* NewSMC = DuplicateObject<UStaticMeshComponent>(CurrentSMC, Actor, *CurrentSMC->GetName());
								if (!NewSMC || NewSMC->IsPendingKill())
									continue;

								if (RootComponent && !RootComponent->IsPendingKill())
									NewSMC->SetupAttachment(RootComponent);
								//NewSMC->SetupAttachment(nullptr);

								NewSMC->SetStaticMesh(BakedStaticMesh);
								Actor->AddInstanceComponent(NewSMC);
								//NewSMC->SetWorldTransform(CurrentSMC->GetComponentTransform());
								NewSMC->SetComponentToWorld(CurrentSMC->GetComponentTransform());

								// Copy properties from the existing component
								CopyPropertyToNewActorAndComponent(nullptr, NewSMC, CurrentSMC);

								NewSMC->RegisterComponent();
							}
						}
						else if (SMC && !SMC->IsPendingKill())
						{
							UStaticMeshComponent* NewSMC = DuplicateObject<UStaticMeshComponent>(SMC, Actor, *SMC->GetName());
							if (NewSMC && !NewSMC->IsPendingKill())
							{
								if (RootComponent && !RootComponent->IsPendingKill())
									NewSMC->SetupAttachment(RootComponent);
								//NewSMC->SetupAttachment(nullptr);

								NewSMC->SetStaticMesh(BakedStaticMesh);
								Actor->AddInstanceComponent(NewSMC);
								//NewSMC->SetWorldTransform(SMC->GetComponentTransform());
								NewSMC->SetComponentToWorld(SMC->GetComponentTransform());

								// Copy properties from the existing component
								CopyPropertyToNewActorAndComponent(nullptr, NewSMC, SMC);

								NewSMC->RegisterComponent();
							}
						}
					}
					else
					{
						UHoudiniInstancedActorComponent* IAC = Cast<UHoudiniInstancedActorComponent>(CurrentOutputObject.OutputComponent);
						if (!IAC || IAC->IsPendingKill())
							continue;

						UObject* InstancedObject = IAC->GetInstancedObject();
						if (!InstancedObject || InstancedObject->IsPendingKill())
							continue;

						UClass* ObjectClass = InstancedObject->GetClass();
						if (!ObjectClass || ObjectClass->IsPendingKill())
							continue;

						TSubclassOf<AActor> ActorClass;
						if (ObjectClass->IsChildOf<AActor>())
						{
							ActorClass = ObjectClass;
						}
						else if (ObjectClass->IsChildOf<UBlueprint>())
						{
							UBlueprint* BlueprintObj = StaticCast<UBlueprint*>(InstancedObject);
							if (BlueprintObj && !BlueprintObj->IsPendingKill())
								ActorClass = *BlueprintObj->GeneratedClass;
						}

						if (*ActorClass)
						{
							for (AActor* InstancedActor : IAC->GetInstancedActors())
							{
								if (!InstancedActor || InstancedActor->IsPendingKill())
									continue;

								UChildActorComponent* CAC = NewObject<UChildActorComponent>(Actor, UChildActorComponent::StaticClass(), NAME_None, RF_Public);
								if (!CAC || CAC->IsPendingKill())
									continue;

								Actor->AddInstanceComponent(CAC);

								CAC->SetChildActorClass(ActorClass);
								CAC->RegisterComponent();
								CAC->SetWorldTransform(InstancedActor->GetTransform());
								if (RootComponent && !RootComponent->IsPendingKill())
									CAC->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepWorldTransform);
							}
						}
						else if (ObjectClass->IsChildOf<UParticleSystem>())
						{
							for (AActor* InstancedActor : IAC->GetInstancedActors())
							{
								if (InstancedActor && !InstancedActor->IsPendingKill())
								{
									UParticleSystemComponent* PSC = NewObject<UParticleSystemComponent>(Actor, UParticleSystemComponent::StaticClass(), NAME_None, RF_Public);
									if (!PSC || PSC->IsPendingKill())
										continue;

									Actor->AddInstanceComponent(PSC);
									PSC->SetTemplate(StaticCast<UParticleSystem*>(InstancedObject));
									PSC->RegisterComponent();
									PSC->SetWorldTransform(InstancedActor->GetTransform());

									if (RootComponent && !RootComponent->IsPendingKill())
										PSC->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepWorldTransform);
								}
							}
						}
						else if (ObjectClass->IsChildOf<USoundBase>())
						{
							for (AActor* InstancedActor : IAC->GetInstancedActors())
							{
								if (InstancedActor && !InstancedActor->IsPendingKill())
								{
									UAudioComponent* AC = NewObject<UAudioComponent>(Actor, UAudioComponent::StaticClass(), NAME_None, RF_Public);
									if (!AC || AC->IsPendingKill())
										continue;

									Actor->AddInstanceComponent(AC);
									AC->SetSound(StaticCast<USoundBase*>(InstancedObject));
									AC->RegisterComponent();
									AC->SetWorldTransform(InstancedActor->GetTransform());

									if (RootComponent && !RootComponent->IsPendingKill())
										AC->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepWorldTransform);
								}
							}
						}
						else
						{
							// Oh no, the asset is not something we know. We will need to handle each asset type case by case.
							// for example we could create a bunch of ParticleSystemComponent if given an emitter asset
							HOUDINI_LOG_ERROR(TEXT("Can not bake instanced actor component for asset type %s"), *ObjectClass->GetName());
						}
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
	TArray<UPackage*> PackagesToSave;
	FHoudiniEngineOutputStats BakeStats;

	if (!FHoudiniEngineBakeUtils::BakeHoudiniActorToActors(
		HoudiniAssetComponent, NewActors, PackagesToSave, BakeStats))
	{
		// TODO ?
		HOUDINI_LOG_WARNING(TEXT("Errors when baking"));
	}

	// Save the created packages
	FHoudiniEngineBakeUtils::SaveBakedPackages(PackagesToSave);

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
	TArray<AActor*>& OutNewActors, 
	TArray<UPackage*>& OutPackagesToSave,
	FHoudiniEngineOutputStats& OutBakeStats)
{
	if (!HoudiniAssetComponent || HoudiniAssetComponent->IsPendingKill())
		return false;

	int32 NumOutputs = HoudiniAssetComponent->GetNumOutputs();
	
	const FString MsgTemplate = TEXT("Baking output: {0}/{1}.");
	FString Msg = FString::Format(*MsgTemplate, { 0, NumOutputs });
	FHoudiniEngine::Get().CreateTaskSlateNotification(FText::FromString(Msg));

	for (int32 n = 0; n < NumOutputs; ++n)
	{
		UHoudiniOutput* Output = HoudiniAssetComponent->GetOutputAt(n);
		if (!Output || Output->IsPendingKill())
			continue;

		Msg = FString::Format(*MsgTemplate, { n, NumOutputs });
		FHoudiniEngine::Get().UpdateTaskSlateNotification(FText::FromString(Msg));

		switch (Output->GetType())
		{
		case EHoudiniOutputType::Mesh:
		{
			OutNewActors.Append(FHoudiniEngineBakeUtils::BakeStaticMeshOutputToActors(Output, OutPackagesToSave));
		}
		break;

		case EHoudiniOutputType::Instancer:
		{
			OutNewActors.Append(FHoudiniEngineBakeUtils::BakeInstancerOutputToActors(Output, OutPackagesToSave));
		}
		break;

		case EHoudiniOutputType::Landscape:
		{
			UWorld* WorldContext = HoudiniAssetComponent->GetWorld();
			const FString AssetName = HoudiniAssetComponent->GetOwner()->GetName();
			const FString BakeFolder = HoudiniAssetComponent->BakeFolder.Path;
			const bool bResult = FHoudiniLandscapeTranslator::BakeLandscape(WorldContext, Output, BakeFolder, AssetName, OutNewActors, OutBakeStats);
		}
		break;

		case EHoudiniOutputType::Skeletal:
			break;

		case EHoudiniOutputType::Curve:
		{
			OutNewActors.Append(FHoudiniEngineBakeUtils::BakeHoudiniCurveOutputToActors(Output));
		}
		break;

		case EHoudiniOutputType::Invalid:
			break;
		}
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
				UStaticMesh* InstancedStaticMesh = Cast<UStaticMesh>(CurrentVariationObject);
				if (!InstancedStaticMesh)
				{
					if (CurrentVariationObject)
					{
						HOUDINI_LOG_ERROR(TEXT("Failed to bake the instances of %s to Foliage"), *CurrentVariationObject->GetName());
					}
					continue;
				}

				// If the instanced static mesh is still a temporary Houdini created Static Mesh
				// we will duplicate/bake it first before baking to foliage
				FHoudiniPackageParams PackageParams;
				FHoudiniEngineUtils::FillInPackageParamsForBakingOutput(
					PackageParams,
					Pair.Key,
					HoudiniAssetComponent->BakeFolder.Path,
					InstanceName + "_" + InstancedStaticMesh->GetName(),
					OwnerActor->GetName());

				UStaticMesh* OutStaticMesh = FHoudiniEngineBakeUtils::DuplicateStaticMeshAndCreatePackageIfNeeded(
					InstancedStaticMesh, PackageParams, HoudiniAssetComponent, PackagesToSave);

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
FHoudiniEngineBakeUtils::BakeInstancerOutputToActors(
	UHoudiniOutput * InOutput,
	TArray<UPackage*>& OutPackagesToSave)
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
			NewActors.Append(BakeInstancerOutputToActors_ISMC(Pair.Key, CurrentOutputObject, HoudiniAssetComponent, OutPackagesToSave));
		}
		else if (CurrentOutputObject.OutputComponent->IsA<UHoudiniInstancedActorComponent>())
		{
			NewActors.Append(BakeInstancerOutputToActors_IAC(Pair.Key, CurrentOutputObject, HoudiniAssetComponent, OutPackagesToSave));
		}
		else if (CurrentOutputObject.OutputComponent->IsA<UHoudiniMeshSplitInstancerComponent>())
		{
			NewActors.Append(BakeInstancerOutputToActors_MSIC(Pair.Key, CurrentOutputObject, HoudiniAssetComponent, OutPackagesToSave));
		}
		else if (CurrentOutputObject.OutputComponent->IsA<UStaticMeshComponent>())
		{
			NewActors.Append(BakeInstancerOutputToActors_SMC(Pair.Key, CurrentOutputObject, HoudiniAssetComponent, OutPackagesToSave));
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
	UHoudiniAssetComponent* InHAC,
	TArray<UPackage*>& OutPackagesToSave)
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
	
	// See if the instanced static mesh is still a temporary Houdini created Static Mesh
	// If it is, we need to bake the StaticMesh first
	FHoudiniPackageParams PackageParams;
	FHoudiniEngineUtils::FillInPackageParamsForBakingOutput(
		PackageParams,
		InOutputObjectIdentifier,
		InHAC->BakeFolder.Path,
		BaseName + "_" + StaticMesh->GetName(),
		OwnerActor->GetName());

	// This will bake/duplicate the mesh if temporary, or return the input one if it is not
	UStaticMesh* BakedStaticMesh = FHoudiniEngineBakeUtils::DuplicateStaticMeshAndCreatePackageIfNeeded(
		StaticMesh, PackageParams, InHAC, OutPackagesToSave);

	// By default spawn in the current level unless specified via the unreal_level_path attribute
	ULevel* DesiredLevel = GWorld->GetCurrentLevel();
	bool bHasLevelPathAttribute = InOutputObject.CachedAttributes.Contains(HAPI_UNREAL_ATTRIB_LEVEL_PATH);
	if (bHasLevelPathAttribute)
	{
		UWorld* DesiredWorld = InHAC ? InHAC->GetWorld() : GWorld;

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
			return NewActors;
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
		return NewActors;

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

	return NewActors;
}

TArray<AActor*>
FHoudiniEngineBakeUtils::BakeInstancerOutputToActors_SMC(
	const FHoudiniOutputObjectIdentifier& InOutputObjectIdentifier,
	const FHoudiniOutputObject& InOutputObject,
	UHoudiniAssetComponent* InHAC,
	TArray<UPackage*>& OutPackagesToSave)
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
	FHoudiniPackageParams PackageParams;
	FHoudiniEngineUtils::FillInPackageParamsForBakingOutput(
		PackageParams,
		InOutputObjectIdentifier,
		InHAC->BakeFolder.Path,
		BaseName + "_" + StaticMesh->GetName(),
		OwnerActor->GetName());

	// This will bake/duplicate the mesh if temporary, or return the input one if it is not
	UStaticMesh* BakedStaticMesh = FHoudiniEngineBakeUtils::DuplicateStaticMeshAndCreatePackageIfNeeded(
		StaticMesh, PackageParams, InHAC, OutPackagesToSave);

	// By default spawn in the current level unless specified via the unreal_level_path attribute
	ULevel* DesiredLevel = GWorld->GetCurrentLevel();
	bool bHasLevelPathAttribute = InOutputObject.CachedAttributes.Contains(HAPI_UNREAL_ATTRIB_LEVEL_PATH);
	if (bHasLevelPathAttribute)
	{
		UWorld* DesiredWorld = InHAC ? InHAC->GetWorld() : GWorld;

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
			return NewActors;
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

	return NewActors;
}

TArray<AActor*>
FHoudiniEngineBakeUtils::BakeInstancerOutputToActors_IAC(
	const FHoudiniOutputObjectIdentifier& InOutputObjectIdentifier,
	const FHoudiniOutputObject& InOutputObject,
	UHoudiniAssetComponent* InHAC,
	TArray<UPackage*>& OutPackagesToSave)
{
	TArray<AActor*> NewActors;
	if (!InHAC || InHAC->IsPendingKill())
		return NewActors;

	UHoudiniInstancedActorComponent* InIAC = Cast<UHoudiniInstancedActorComponent>(InOutputObject.OutputComponent);
	if (!InIAC || InIAC->IsPendingKill())
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

	FHoudiniPackageParams PackageParams;
	FHoudiniEngineUtils::FillInPackageParamsForBakingOutput(
		PackageParams, InOutputObjectIdentifier, InHAC->BakeFolder.Path, BaseName.ToString(), OwnerActor->GetName());

	// By default spawn in the current level unless specified via the unreal_level_path attribute
	UWorld* DesiredWorld = InHAC ? InHAC->GetWorld() : GWorld;
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
			return NewActors;
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

	// TODO:
	// Move Actors to DesiredLevel if needed??

	return NewActors;
}

TArray<AActor*>
FHoudiniEngineBakeUtils::BakeInstancerOutputToActors_MSIC(
	const FHoudiniOutputObjectIdentifier& InOutputObjectIdentifier,
	const FHoudiniOutputObject& InOutputObject,
	UHoudiniAssetComponent* InHAC,
	TArray<UPackage*>& OutPackagesToSave)
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

	// See if the instanced static mesh is still a temporary Houdini created Static Mesh
	// If it is, we need to bake the StaticMesh first
	FHoudiniPackageParams PackageParams;
	FHoudiniEngineUtils::FillInPackageParamsForBakingOutput(
		PackageParams,
		InOutputObjectIdentifier,
		InHAC->BakeFolder.Path,
		BaseName + "_" + StaticMesh->GetName(),
		OwnerActor->GetName());

	// This will bake/duplicate the mesh if temporary, or return the input one if it is not
	UStaticMesh* BakedStaticMesh = FHoudiniEngineBakeUtils::DuplicateStaticMeshAndCreatePackageIfNeeded(
		StaticMesh, PackageParams, InHAC, OutPackagesToSave);

	// By default spawn in the current level unless specified via the unreal_level_path attribute
	ULevel* DesiredLevel = GWorld->GetCurrentLevel();

	bool bHasLevelPathAttribute = InOutputObject.CachedAttributes.Contains(HAPI_UNREAL_ATTRIB_LEVEL_PATH);
	if (bHasLevelPathAttribute)
	{
		UWorld* DesiredWorld = InHAC ? InHAC->GetWorld() : GWorld;

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
			return NewActors;
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

	return NewActors;
}


TArray<AActor*> 
FHoudiniEngineBakeUtils::BakeStaticMeshOutputToActors(
	UHoudiniOutput* Output, TArray<UPackage*>& OutPackagesToSave)
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
	
	UActorFactory* Factory = GEditor ? GEditor->FindActorFactoryByClass(UActorFactoryStaticMesh::StaticClass()) : nullptr;
	if (!Factory)
		return NewActors;

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

		// We do not bake templated geos
		if (FoundHGPO && FoundHGPO->bIsTemplated)
			continue;

		// The bake name override has priority
		FString SMName = Pair.Value.BakeName;
		if (SMName.IsEmpty())
		{
			// ... finally the part name
			if (FoundHGPO && FoundHGPO->bHasCustomPartName)
				SMName = FoundHGPO->PartName;
			else
				SMName = StaticMesh->GetName();
		}

		FHoudiniPackageParams PackageParams;
		FHoudiniEngineUtils::FillInPackageParamsForBakingOutput(
			PackageParams, Identifier, HAC->BakeFolder.Path, SMName, OwnerActor->GetName());

		UWorld* DesiredWorld = HAC ? HAC->GetWorld() : GWorld;
		ULevel* DesiredLevel = GWorld->GetCurrentLevel();

		// See if this output object has an unreal_level_path attribute specified
		// In which case, we need to create/find the desired level for baking instead of using the current one
		bool bHasLevelPathAttribute = Pair.Value.CachedAttributes.Contains(HAPI_UNREAL_ATTRIB_LEVEL_PATH);
		if (bHasLevelPathAttribute)
		{
			// Access some of the attribute that were cached on the output object
			FHoudiniAttributeResolver Resolver;
			TMap<FString, FString> CachedAttributes = Pair.Value.CachedAttributes;
			TMap<FString, FString> Tokens = Pair.Value.CachedTokens;
			PackageParams.UpdateTokensFromParams(DesiredWorld, Tokens);
			Resolver.SetCachedAttributes(CachedAttributes);
			Resolver.SetTokensFromStringMap(Tokens);

			// Get the package path from the unreal_level_apth attribute
			FString LevelPackagePath = LevelPackagePath = Resolver.ResolveFullLevelPath();

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
			StaticMesh, PackageParams, HAC, OutPackagesToSave);

		if (!BakedSM || BakedSM->IsPendingKill())
			continue;

		// Make sure we have a level to spawn to
		if (!DesiredLevel || DesiredLevel->IsPendingKill())
			continue;

		// Remove a previous bake actor if it exists
		FName BaseName(*(PackageParams.ObjectName));		
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

		// Spawn the new actor
		AActor* NewActor = Factory->CreateActor(BakedSM, DesiredLevel, InSMC->GetComponentTransform(), RF_Transactional);
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
		FHoudiniEngineUtils::FillInPackageParamsForBakingOutput(
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
		FKismetEditorUtilities::AddComponentsToBlueprint(
			OutBlueprint,
			InActor->GetInstanceComponents());

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
	bFoundPackage = false;
	BlueprintName = "";
	
	if (!HoudiniAssetComponent || HoudiniAssetComponent->IsPendingKill())
		return nullptr;

	AActor * OwnerActor = HoudiniAssetComponent->GetOwner();
	if (!OwnerActor || OwnerActor->IsPendingKill())
		return nullptr;

	return FindOrCreateBlueprintPackage(
		OwnerActor->GetName(),
		HoudiniAssetComponent->BakeFolder,
		BlueprintName,
		bFoundPackage);
}

UStaticMesh* 
FHoudiniEngineBakeUtils::BakeStaticMesh(
	UStaticMesh * StaticMesh,
	const FHoudiniPackageParams& PackageParams) 
{
	if (!StaticMesh || StaticMesh->IsPendingKill())
		return nullptr;

	TArray<UPackage*> PackagesToSave;
	TArray<UHoudiniOutput*> Outputs;
	UStaticMesh* BakedStaticMesh = FHoudiniEngineBakeUtils::DuplicateStaticMeshAndCreatePackageIfNeeded(StaticMesh, PackageParams, nullptr, PackagesToSave);

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
FHoudiniEngineBakeUtils::DuplicateStaticMeshAndCreatePackageIfNeeded(
	UStaticMesh * InStaticMesh,
	const FHoudiniPackageParams &PackageParams,
	UHoudiniAssetComponent* InHAC,
	TArray<UPackage*> & OutCreatedPackages)
{
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

	return DuplicateStaticMeshAndCreatePackageIfNeeded(InStaticMesh, PackageParams, Outputs, TempPath, OutCreatedPackages);
}

UStaticMesh * 
FHoudiniEngineBakeUtils::DuplicateStaticMeshAndCreatePackageIfNeeded(
	UStaticMesh * InStaticMesh,
	const FHoudiniPackageParams &PackageParams,
	const TArray<UHoudiniOutput*>& InParentOutputs, 
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

	AActor* NewActor = Factory->CreateActor(nullptr, DesiredLevel, InSplineComponent->GetComponentTransform(), RF_Transactional, FName(PackageParams.ObjectName));
	
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

	AActor* NewActor = Factory->CreateActor(nullptr, DesiredLevel, InHoudiniSplineComponent->GetComponentTransform(), RF_Transactional);
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

	for (UHoudiniOutput* CurOutput : InParentOutputs)
	{
		for (const auto& CurOutputObject : CurOutput->GetOutputObjects())
		{
			if (CurOutputObject.Value.OutputObject == InObject)
				return true;
			else if (CurOutputObject.Value.OutputComponent == InObject)
				return true;
			else if (CurOutputObject.Value.ProxyObject == InObject)
				return true;
			else if (CurOutputObject.Value.ProxyComponent == InObject)
				return true;
		}
	}

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
	// Reapply the uproperties modified by attributes on the new component
	//FHoudiniEngineUtils::UpdateUPropertyAttributesOnObject(SMC, HoudiniGeoPartObject);

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
	InActor->Rename(*MakeUniqueObjectName(InActor->GetOuter(), InActor->GetClass(), FName(InNewName)).ToString());
	InActor->SetActorLabel(InNewName);

	InActor->SetFolderPath(InFolderPath);

	return true;
}

bool
FHoudiniEngineBakeUtils::BakePDGStaticMeshOutput(
	UHoudiniPDGAssetLink* InPDGAssetLink,
	AActor* InOutputActor,
	const FString& InHoudiniAssetName,
	UHoudiniOutput* InOutput,
	const TArray<UHoudiniOutput*>& InParentOutputs,
	const FName& InWorldOutlinerFolderPath)
{
	if (!InOutput || InOutput->IsPendingKill())
		return false;

	if (!InOutputActor || InOutputActor->IsPendingKill())
		return false;

	ULevel* DesiredLevel = GWorld->GetCurrentLevel();
	if (!DesiredLevel || DesiredLevel->IsPendingKill())
		return false;

	TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& OutputObjects = InOutput->GetOutputObjects();
	// TArray<FHoudiniOutputObjectIdentifier> BakedObjectsIdentifiers;
	for (auto& Pair : OutputObjects)
	{
		FHoudiniOutputObjectIdentifier& Identifier = Pair.Key;
		FHoudiniOutputObject& OutputObject = Pair.Value;

		if (!BakePDGStaticMeshOutputObject(
                Identifier,
                OutputObject,
                InWorldOutlinerFolderPath,
                InParentOutputs,
                InOutput,
                InOutputActor,
                InHoudiniAssetName,
                InPDGAssetLink->GetTemporaryCookFolder(),
                InPDGAssetLink->BakeFolder,
                InPDGAssetLink->PDGBakePackageReplaceMode))
		{
			return false;
		}
	}

	// // Remove output objects which have been baked
	// for (FHoudiniOutputObjectIdentifier& Identifier : BakedObjectsIdentifiers)
	// {
	// 	OutputObjects.Remove(Identifier);
	// }

	return true;
}

TArray<AActor*> 
FHoudiniEngineBakeUtils::BakePDGTOPNodeOutputsKeepActors(
	UHoudiniPDGAssetLink* InPDGAssetLink,
	FTOPNode* InNode,
	bool bInUseWorkResultActor,
	bool bInBakeForBlueprint) 
{
	TArray<AActor*> BakedActors;
	
	if (!InPDGAssetLink || InPDGAssetLink->IsPendingKill())
		return BakedActors;

	if (!InNode)
		return BakedActors;

	UActorFactory* Factory = GEditor ? GEditor->FindActorFactoryByClass(UActorFactoryEmptyActor::StaticClass()) : nullptr;
	if (!Factory)
		return BakedActors;

	ULevel* DesiredLevel = GWorld->GetCurrentLevel();
	if (!IsValid(DesiredLevel))
	{
		HOUDINI_LOG_ERROR(TEXT("[FHoudiniEngineBakeUtils::BakePDGTOPNodeOutputsKeepActors]: Could not find current level."));
		return BakedActors;
	}

	// Determine the output world outliner folder path via the PDG asset link's
	// owner's folder path and name
	UObject* PDGOwner = InPDGAssetLink->GetOwnerActor();
	if (!PDGOwner)
		PDGOwner = InPDGAssetLink->GetOuter();
	const FName& WorldOutlinerFolderPath = GetOutputFolderPath(PDGOwner);

	for (FTOPWorkResult& WorkResult : InNode->WorkResult)
	{
		for (FTOPWorkResultObject& WorkResultObject : WorkResult.ResultObjects)
		{
			TArray<UHoudiniOutput*>& Outputs = WorkResultObject.GetResultOutputs();
			if (Outputs.Num() == 0)
				continue;

			AActor* WorkResultObjectActor = WorkResultObject.GetOutputActor();
			if (!IsValid(WorkResultObjectActor))
			{
				HOUDINI_LOG_WARNING(TEXT("[FHoudiniEngineBakeUtils::BakePDGTOPNodeOutputsKeepActors]: WorkResultObjectActor (%s) is null (unexpected since # Outputs > 0)"), *WorkResultObject.Name);
				continue;
			}

			// BakedActorsForWorkResultObject contains each actor that contains baked PDG results. Actors may
			// appear in the array more than once if they have more than one baked result/component associated with
			// them
			TArray<AActor*> BakedActorsForWorkResultObject;
			// We can either re-use the PDG output actor or create a new actor and delete the PDG output actor if it
			// has no more temporary outputs after the baking process (determined by bInUseWorkResultActor). We
			// typically create a new actor when baking to blueprints
			AActor* OutputActor = nullptr;
			if (!bInUseWorkResultActor)
			{
				OutputActor = Factory->CreateActor(
					nullptr,
					DesiredLevel,
					WorkResultObjectActor->GetActorTransform(),
					RF_Transactional,
					MakeUniqueObjectName(DesiredLevel, AActor::StaticClass(), FName(WorkResultObjectActor->GetName())));

				if (OutputActor->HasValidRootComponent() && WorkResultObjectActor && WorkResultObjectActor->HasValidRootComponent())
					OutputActor->GetRootComponent()->SetMobility(WorkResultObjectActor->GetRootComponent()->Mobility);
			}
			else
			{
				OutputActor = WorkResultObjectActor;
			}

			const FString InHoudiniAssetName(WorkResultObject.Name);
			// bKeepWorkResultActor is set to true if we have remaining temporary output after the baking process
			// that is still parented to/associated with the work result output actor. This is only applicable in when
			// we create a separate output actor (bInUseWorkResultActor == false)
			bool bKeepWorkResultActor = false;
			for (UHoudiniOutput* Output : Outputs)
			{
				if (!Output || Output->IsPendingKill())
					continue;

				const EHoudiniOutputType OutputType = Output->GetType();
				switch (OutputType) 
				{
					case EHoudiniOutputType::Mesh:
					{
						if (BakePDGStaticMeshOutput(
								InPDGAssetLink,
								OutputActor,
								InHoudiniAssetName,
								Output,
								Outputs,
								WorldOutlinerFolderPath))
						{
							BakedActorsForWorkResultObject.Add(OutputActor);
						}
					}
					break;

					case EHoudiniOutputType::Instancer:
					{
						BakedActorsForWorkResultObject.Append(BakePDGInstancerOutputKeepActors(
								InPDGAssetLink,
								WorkResultObject.GetResultOutputs(),
								OutputActor,
								Output,
								InHoudiniAssetName,
								bInBakeForBlueprint,
								bKeepWorkResultActor
								));
					}
					break;

					case EHoudiniOutputType::Landscape:
					{
						if (!bInBakeForBlueprint && OutputActor == WorkResultObjectActor)
						{
							// Just ensure that the WorkResultObject->GetOutputActor is detached from its parent
							// and moved to the appropriate folder, the attached landscapes will follow it
							const FString Name = OutputActor->GetName();
							DetachAndRenameBakedPDGOutputActor(OutputActor, Name, WorldOutlinerFolderPath);
							BakedActorsForWorkResultObject.Add(OutputActor);

							UWorld* WorldContext = InPDGAssetLink->GetWorld();
							FString AssetName;
							if (InPDGAssetLink->GetOwnerActor())
								AssetName = InPDGAssetLink->GetOwnerActor()->GetName();
							else
								AssetName = TEXT("UnknownAssetName");
							const FString BakeFolder = InPDGAssetLink->BakeFolder.Path;

							TArray<AActor*> NewActors;
							FHoudiniEngineOutputStats BakeStats;
							const bool bResult = FHoudiniLandscapeTranslator::BakeLandscape(WorldContext, Output, BakeFolder, AssetName, NewActors, BakeStats);

							// Set the OutputObjects to null so that it does not get destroyed by PDG later
							TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& OutputObjects = Output->GetOutputObjects();
							for (auto& CurrentOutputObj : OutputObjects)
							{
								ALandscapeProxy* Landscape = Cast<ALandscapeProxy>(CurrentOutputObj.Value.OutputObject);
								if (!Landscape)
								{
									UHoudiniLandscapePtr* LandscapePointer = Cast<UHoudiniLandscapePtr>(
										CurrentOutputObj.Value.OutputObject);
									if (!LandscapePointer)
										continue;
								}
								CurrentOutputObj.Value.OutputObject = nullptr;
								CurrentOutputObj.Value.OutputComponent = nullptr;
							}
						}
						else
						{
							bKeepWorkResultActor = true;
						}
					}
					break;

					case EHoudiniOutputType::Curve:
					{
						// TODO: Support curves in PDG
						HOUDINI_LOG_WARNING(TEXT("[FHoudiniEngineBakeUtils::BakePDGTOPNodeOutputsKeepActors] Curve output not yet supported."));
						// BakedActors.Append(BakePDGCurveOutputKeepActors(Output));
					}
					break;

					case EHoudiniOutputType::Skeletal:
					case EHoudiniOutputType::Invalid:
					default:
					{
						HOUDINI_LOG_WARNING(
							TEXT("[FHoudiniEngineBakeUtils::BakePDGTOPNodeOutputsKeepActors] Unsupported output type: %s"),
							*UHoudiniOutput::OutputTypeToString(OutputType));
					}
				}
				
			}

			// If OutputActor is not re-used (not in BakedActorsForWorkResultObject) then destroy it, otherwise
			// set it to null on the WorkResultObject
			if (!bInBakeForBlueprint || !bKeepWorkResultActor)
			{
				if (BakedActorsForWorkResultObject.Contains(WorkResultObject.GetOutputActor()))
				{
					WorkResultObject.SetOutputActor(nullptr);
				}
				else
				{
					WorkResultObject.DestroyOutputActor();
				}
			}
			BakedActors.Append(BakedActorsForWorkResultObject);

			// If we created a new OutputActor but ultimately did not use it, then destroy it
			// TODO: delay creation of OutputActor until we know we need it?
			if (BakedActorsForWorkResultObject.Num() == 0 && !bInUseWorkResultActor && IsValid(OutputActor))
			{
				OutputActor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
				OutputActor->Destroy();
				OutputActor = nullptr;
			}
		}			
	}
	
	// Select the baked actors
	if (GEditor && BakedActors.Num() > 0)
	{
		GEditor->SelectNone(false, true);
		for (AActor* BakedActor : BakedActors)
		{
			if (BakedActor && !BakedActor->IsPendingKill())
				GEditor->SelectActor(BakedActor, true, false);
		}
		GEditor->NoteSelectionChange();
	}

	return BakedActors;
}

bool
FHoudiniEngineBakeUtils::BakePDGTOPNetworkOutputsKeepActors(UHoudiniPDGAssetLink* InPDGAssetLink, FTOPNetwork* InNetwork)
{
	if (!InPDGAssetLink || InPDGAssetLink->IsPendingKill())
		return false;

	if (!InNetwork)
		return false;

	for (FTOPNode& Node : InNetwork->AllTOPNodes)
	{
		BakePDGTOPNodeOutputsKeepActors(InPDGAssetLink, &Node);
	}

	return true;
}

bool
FHoudiniEngineBakeUtils::BakePDGAssetLinkOutputsKeepActors(UHoudiniPDGAssetLink* InPDGAssetLink)
{
	if (!InPDGAssetLink || InPDGAssetLink->IsPendingKill())
		return false;

	switch(InPDGAssetLink->PDGBakeSelectionOption)
	{
		case EPDGBakeSelectionOption::All:
			for (FTOPNetwork& Network : InPDGAssetLink->AllTOPNetworks)
			{
				for (FTOPNode& Node : Network.AllTOPNodes)
				{
					BakePDGTOPNodeOutputsKeepActors(InPDGAssetLink, &Node);
				}
			}
			break;
		case EPDGBakeSelectionOption::SelectedNetwork:
			return BakePDGTOPNetworkOutputsKeepActors(InPDGAssetLink, InPDGAssetLink->GetSelectedTOPNetwork());
		case EPDGBakeSelectionOption::SelectedNode:
			return BakePDGTOPNodeOutputsKeepActors(InPDGAssetLink, InPDGAssetLink->GetSelectedTOPNode()).Num() != 0;
	}

	return true;
}

TArray<AActor*>
FHoudiniEngineBakeUtils::BakePDGInstancerOutputKeepActors(
	UHoudiniPDGAssetLink* InPDGAssetLink,
	const TArray<UHoudiniOutput*>& InAllOutputs,
	AActor* InOutputActor,
	UHoudiniOutput* InOutput,
	const FString& InHoudiniAssetName,
	bool bSkipIAC,
	bool& bOutSkippedIAC)
{
	TArray<AActor*> BakedActors;
	bOutSkippedIAC = false;
	if (!InOutput || InOutput->IsPendingKill())
		return BakedActors;

	if (!InPDGAssetLink || InPDGAssetLink->IsPendingKill())
		return BakedActors;

	if (!InOutputActor || InOutputActor->IsPendingKill())
		return BakedActors;

	// Determine the output world outliner folder path via the PDG asset link's
	// owner's folder path and name
	UObject* PDGOwner = InPDGAssetLink->GetOwnerActor();
	if (!PDGOwner)
		PDGOwner = InPDGAssetLink->GetOuter();
	const FName OutputFolderPath = GetOutputFolderPath(PDGOwner);
	
	const FDirectoryPath TempCookFolder = InPDGAssetLink->GetTemporaryCookFolder();
	
	TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& OutputObjects = InOutput->GetOutputObjects();

	// Iterate on the output objects, baking their object/component as we go
	for (auto& Pair : OutputObjects)
	{
		const FHoudiniOutputObjectIdentifier& Identifier = Pair.Key;
		FHoudiniOutputObject& CurrentOutputObject = Pair.Value;

		if (CurrentOutputObject.bProxyIsCurrent)
		{
			// TODO: we need to refine the SM first!
			// ?? 
		}

		if (!CurrentOutputObject.OutputComponent || CurrentOutputObject.OutputComponent->IsPendingKill())
			continue;

		if (CurrentOutputObject.OutputComponent->IsA<UHoudiniInstancedActorComponent>())
		{
			if (bSkipIAC)
				bOutSkippedIAC = true;
			else
				BakedActors.Append(BakePDGInstancerOutputKeepActors_IAC(CurrentOutputObject, OutputFolderPath));
		}
		else if (CurrentOutputObject.OutputComponent->IsA<UHoudiniMeshSplitInstancerComponent>())
		{
			if (BakePDGInstancerOutputKeepActors_MSIC(
					Identifier,
					CurrentOutputObject,
					OutputFolderPath,
					InAllOutputs,
					InOutputActor,
					TempCookFolder,
					InPDGAssetLink->BakeFolder,
					InPDGAssetLink->PDGBakePackageReplaceMode))
			{
				BakedActors.Add(InOutputActor);
			}
		}
		else if (CurrentOutputObject.OutputComponent->IsA<UStaticMeshComponent>())
		{
			// This also handles UInstancedStaticMeshComponent
			if (BakePDGStaticMeshOutputObject(
					Identifier,
					CurrentOutputObject,
					OutputFolderPath,
					InAllOutputs,
					InOutput,
					InOutputActor,
					InHoudiniAssetName,
					TempCookFolder,
					InPDGAssetLink->BakeFolder,
					InPDGAssetLink->PDGBakePackageReplaceMode))
			{
				BakedActors.Add(InOutputActor);
			}
		}
		else
		{
			// Unsupported component!
			HOUDINI_LOG_WARNING(
				TEXT("[FHoudiniEngineBakeUtils::BakePDGInstancerOutputToActors] Unsupported component type %s"),
				*CurrentOutputObject.OutputComponent->GetClass()->GetName());
		}

	}

	return BakedActors;
}

bool
FHoudiniEngineBakeUtils::BakePDGStaticMeshOutputObject(
	const FHoudiniOutputObjectIdentifier& InOutputObjectIdentifier,
	FHoudiniOutputObject& InOutputObject,
	const FName& InWorldOutlinerFolderPath,
	const TArray<UHoudiniOutput*>& InParentOutputs,
	UHoudiniOutput* InOutput,
	AActor* InOutputActor,
	const FString& InHoudiniAssetName,
	const FDirectoryPath& InTemporaryCookFolder,
	const FDirectoryPath& InBakeFolder,
	const EPDGBakePackageReplaceModeOption& InReplaceMode)
{
	UStaticMeshComponent* InSMC = Cast<UStaticMeshComponent>(InOutputObject.OutputComponent);
	if (!InSMC || InSMC->IsPendingKill())
		return false;

	UStaticMesh* StaticMesh = InSMC->GetStaticMesh();
	if (!IsValid(StaticMesh))
		StaticMesh = Cast<UStaticMesh>(InOutputObject.OutputObject);
	if (!StaticMesh || StaticMesh->IsPendingKill())
		return false;

	// Find the HGPO that matches this output identifier
	const FHoudiniGeoPartObject* FoundHGPO = nullptr;
	for (auto & NextHGPO : InOutput->GetHoudiniGeoPartObjects()) 
	{
		// We use Matches() here as it handles the case where the HDA was loaded,
		// which likely means that the the obj/geo/part ids dont match the output identifier
		if(InOutputObjectIdentifier.Matches(NextHGPO))
		{
			FoundHGPO = &NextHGPO;
			break;
		}
	}

	FString SMName = InOutputObject.BakeName;
	if (SMName.IsEmpty())
	{
		if (FoundHGPO && FoundHGPO->bHasCustomPartName)
			SMName = FoundHGPO->PartName;
		else
			SMName = StaticMesh->GetName();
	}
	

	// See if the static mesh is still a temporary Houdini created Static Mesh
	// If it is, we need to bake the StaticMesh first
	FHoudiniPackageParams PackageParams;
	FHoudiniEngineUtils::FillInPackageParamsForBakingOutput(
        PackageParams,
        InOutputObjectIdentifier,
        InBakeFolder.Path,
        SMName,
        InHoudiniAssetName);
	PackageParams.ReplaceMode = FHoudiniEngineEditor::Get().PDGBakePackageReplaceModeToPackageReplaceMode(InReplaceMode);

	// This will bake/duplicate the mesh if temporary, or return the input one if it is not
	TArray<UPackage*> PackagesToSave;
	UStaticMesh* BakedSM = FHoudiniEngineBakeUtils::DuplicateStaticMeshAndCreatePackageIfNeeded(
		StaticMesh, PackageParams, InParentOutputs, InTemporaryCookFolder.Path, PackagesToSave);

	if (!BakedSM || BakedSM->IsPendingKill())
		return false;

	bool bCreatedNewComponent = false;
	if (InOutputActor != InSMC->GetOwner())
	{
		// New actor, duplicate component
		UStaticMeshComponent *NewSMC = DuplicateObject<UStaticMeshComponent>(InSMC, InOutputActor, *InSMC->GetName());
		if (!NewSMC || NewSMC->IsPendingKill())
			return false;

		bCreatedNewComponent = true;
		
		NewSMC->SetupAttachment(nullptr);
		// Set the static mesh on the static mesh component
		NewSMC->SetStaticMesh(BakedSM);
		InOutputActor->AddInstanceComponent(NewSMC);
		NewSMC->SetWorldTransform(InSMC->GetComponentTransform());

		// Copy properties from the existing component
		CopyPropertyToNewActorAndComponent(InOutputActor, NewSMC, InSMC);

		NewSMC->RegisterComponent();

		NewSMC->AttachToComponent(InOutputActor->GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);

		// Destroy the old component
		InSMC->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
		InSMC->UnregisterComponent();
		InSMC->DestroyComponent();
	}
	else
	{
		// Set the static mesh on the static mesh component
		InSMC->SetStaticMesh(BakedSM);
	}

	bool bWasTemporaryStaticMesh = StaticMesh != BakedSM;
	if (bWasTemporaryStaticMesh || bCreatedNewComponent)
	{
		InOutputActor->InvalidateLightingCache();
		InOutputActor->PostEditMove(true);
		InOutputActor->MarkPackageDirty();
	}
	
	// We don't want to delete the static mesh component, since we are reusing it
	// Don't set output object to null since that could be reusable if PDG cooks again,
	// and will be cleaned up with PDG
	InOutputObject.OutputComponent = nullptr;

	// TODO: should we check if any material overrides on the SMC use temp materials?

	// TODO: we are keeping just one actor with multiple components (the same structure as we get from
	// PDG, one actor per work item, with potentially multiple components per work item, so the OutputActor
	// here is the same for iteration of this loop. Should we indeed consider spawning an actor per component?
	
	// Detach from parent
	DetachAndRenameBakedPDGOutputActor(InOutputActor, PackageParams.GetPackageName(), InWorldOutlinerFolderPath);

	FHoudiniEngineBakeUtils::SaveBakedPackages(PackagesToSave);

	return true;
}

TArray<AActor*>
FHoudiniEngineBakeUtils::BakePDGInstancerOutputKeepActors_IAC(
	FHoudiniOutputObject& InOutputObject,
	const FName& InOutputFolderPath)
{
	TArray<AActor*> NewActors;
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
	const FName BaseName = FName(OwnerActor->GetName());

	// Get the object instanced by this IAC
	UObject* InstancedObject = InIAC->GetInstancedObject();
	if (!InstancedObject || InstancedObject->IsPendingKill())
		return NewActors;

	// Iterates on all the instances of the IAC and spawns an actor for each instance
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
		NewActor->SetActorTransform(CurrentTransform);

		NewActor->SetFolderPath(InOutputFolderPath);

		NewActors.Add(NewActor);
	}

	InIAC->ClearAllInstances();

	// Destroy the component, since we have created new actors for all instances, and we don't want to keep a duplicate
	// on the PDG link
	InIAC->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
	InIAC->UnregisterComponent();
	InIAC->DestroyComponent();
	InOutputObject.OutputComponent = nullptr;

	return NewActors;
}

bool
FHoudiniEngineBakeUtils::BakePDGInstancerOutputKeepActors_MSIC(
	const FHoudiniOutputObjectIdentifier& InOutputObjectIdentifier,
	FHoudiniOutputObject& InOutputObject,
	const FName& InOutputFolderPath,
	const TArray<UHoudiniOutput*>& InParentOutputs,
	AActor* InActor,
	const FDirectoryPath& InTemporaryCookFolder,
	const FDirectoryPath& InBakeFolder,
	const EPDGBakePackageReplaceModeOption& InReplaceMode)
{
	if (!InActor || InActor->IsPendingKill())
		return false;

	UHoudiniMeshSplitInstancerComponent * InMSIC = Cast<UHoudiniMeshSplitInstancerComponent>(InOutputObject.OutputComponent);
	if (!InMSIC || InMSIC->IsPendingKill())
		return false;

	AActor * OwnerActor = InMSIC->GetOwner();
	if (!OwnerActor || OwnerActor->IsPendingKill())
		return false;

	// BaseName holds the Actor / HDA name
	// Instancer name adds the split identifier (INSTANCERNUM_VARIATIONNUM)
	const FString BaseName = OwnerActor->GetName();
	const FString InstancerName = BaseName + "_instancer_" + InOutputObjectIdentifier.SplitIdentifier;

	UStaticMesh * StaticMesh = InMSIC->GetStaticMesh();
	if (!StaticMesh || StaticMesh->IsPendingKill())
		return false;

	TArray<UPackage*> PackagesToSave;

	// If the instanced static mesh is still a temporary Houdini created Static Mesh
	// If it is, we need to bake the StaticMesh first
	FHoudiniPackageParams PackageParams;
	FHoudiniEngineUtils::FillInPackageParamsForBakingOutput(
		PackageParams,
		InOutputObjectIdentifier,
		InBakeFolder.Path,
		BaseName + "_" + StaticMesh->GetName(),
		OwnerActor->GetName());
	PackageParams.ReplaceMode = FHoudiniEngineEditor::Get().PDGBakePackageReplaceModeToPackageReplaceMode(InReplaceMode);

	// This will bake/duplicate the mesh if temporary, or return the input one if it is not
	UStaticMesh* BakedStaticMesh = FHoudiniEngineBakeUtils::DuplicateStaticMeshAndCreatePackageIfNeeded(
		StaticMesh, PackageParams, InParentOutputs, InTemporaryCookFolder.Path, PackagesToSave);

	ULevel* DesiredLevel = GWorld->GetCurrentLevel();
	if (!DesiredLevel)
		return false;

	InActor->SetActorHiddenInGame(InMSIC->bHiddenInGame);

	if (InMSIC->GetOwner() == InActor)
	{
		// Just move the SMCs to InActor's root component and update the static mesh
		for (UStaticMeshComponent* CurrentSMC : InMSIC->GetInstances())
		{
			if (!CurrentSMC || CurrentSMC->IsPendingKill())
				continue;

			CurrentSMC->AttachToComponent(InActor->GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);
			CurrentSMC->SetStaticMesh(BakedStaticMesh);
		}
		// Empty the instances array on the MSIC to prevent destruction of re-used components
		InMSIC->GetInstancesForWrite().Empty();
	}
	else
	{
		// Duplicate the SMCs to InActor, we'll delete the MSIC at the end of the function
		for (UStaticMeshComponent* CurrentSMC : InMSIC->GetInstances())
		{
			if (!CurrentSMC || CurrentSMC->IsPendingKill())
				continue;

			UStaticMeshComponent* NewSMC = DuplicateObject<UStaticMeshComponent>(CurrentSMC, InActor, *CurrentSMC->GetName());
			if (!NewSMC || NewSMC->IsPendingKill())
				continue;

			NewSMC->SetupAttachment(nullptr);
			NewSMC->SetStaticMesh(BakedStaticMesh);
			InActor->AddInstanceComponent(NewSMC);
			NewSMC->SetWorldTransform(CurrentSMC->GetComponentTransform());

			// Copy properties from the existing component
			CopyPropertyToNewActorAndComponent(InActor, NewSMC, CurrentSMC);

			NewSMC->RegisterComponent();

			NewSMC->AttachToComponent(InActor->GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);
		}
	}

	const FName NewActorName = MakeUniqueObjectName(DesiredLevel, AActor::StaticClass(), FName(InstancerName));
	DetachAndRenameBakedPDGOutputActor(InActor, NewActorName.ToString(), InOutputFolderPath);
	
	InActor->InvalidateLightingCache();
	InActor->PostEditMove(true);
	InActor->MarkPackageDirty();

	FHoudiniEngineBakeUtils::SaveBakedPackages(PackagesToSave);

	// Destroy the component, since we have created new actors for all instances, and we don't want to keep a duplicate
	// on the PDG link
	InMSIC->ClearInstances(0);
	InMSIC->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
	InMSIC->UnregisterComponent();
	InMSIC->DestroyComponent();
	InOutputObject.OutputComponent = nullptr;

	return true;
}

TArray<AActor*>
FHoudiniEngineBakeUtils::BakePDGTOPNodeBlueprints(UHoudiniPDGAssetLink* InPDGAssetLink, FTOPNode* InNode) 
{
	TArray<AActor*> BPActors;

	if (!IsValid(InPDGAssetLink))
	{
		HOUDINI_LOG_WARNING(TEXT("[FHoudiniEngineBakeUtils::BakePDGBlueprint]: InPDGAssetLink is null"));
		return BPActors;
	}
		
	if (!InNode)
	{
		HOUDINI_LOG_WARNING(TEXT("[FHoudiniEngineBakeUtils::BakePDGBlueprint]: InNode is null"));
		return BPActors;
	}

	// Bake PDG output to new actors (any PDG output actors that no longer have unbaked outputs will be destroyed,
	// those that still have temporary outputs will be kept)
	// bInBakeForBlueprint == true will skip landscapes and instanced actor components
	const bool bInUseWorkResultActor = false;
	const bool bInBakeForBlueprint = true;
	TArray<AActor*> BakedActors = BakePDGTOPNodeOutputsKeepActors(
		InPDGAssetLink,
		InNode,
		bInUseWorkResultActor,
		bInBakeForBlueprint
	);

	// // Clear selection
	// if (GEditor)
	// {
	// 	GEditor->SelectNone(false, true);
	// 	GEditor->NoteSelectionChange();
	// }

	// Iterate over the baked actors. An actor might appear multiple times if multiple OutputComponents were
	// baked to the same actor, so keep track of actors we have already processed in BakedActorSet
	TSet<AActor*> BakedActorSet;
	TArray<UPackage*> PackagesToSave;	
	for (AActor *Actor : BakedActors)
	{
		if (!IsValid(Actor))
			continue;

		if (BakedActorSet.Contains(Actor))
			continue;

		bool bFoundPackage = false;
		FString BlueprintName;
		UPackage* Package = FindOrCreateBlueprintPackage(
			Actor->GetActorLabel(),
			InPDGAssetLink->BakeFolder,
			BlueprintName,
			bFoundPackage,
			InPDGAssetLink->PDGBakePackageReplaceMode);
		
		if (!Package || Package->IsPendingKill())
			continue;

		BakedActorSet.Add(Actor);
		
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
                BlueprintName, InPDGAssetLink->BakeFolder.Path,
                UBlueprint::StaticClass(), Factory, FName("ContentBrowserNewAsset"));
		}

		UBlueprint* Blueprint = Cast<UBlueprint>(Asset);
		if (!Blueprint || Blueprint->IsPendingKill())
			continue;

		// Clear old Blueprint Node tree
		{
			USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;

			const int32 NodeSize = SCS->GetAllNodes().Num();
			for (int32 n = NodeSize - 1; n >= 0; --n)
				SCS->RemoveNode(SCS->GetAllNodes()[n]);
		}

		CopyActorContentsToBlueprint(Actor, Blueprint);

		// Retrieve actor transform.
		FVector Location = Actor->GetActorLocation();
		FRotator Rotator = Actor->GetActorRotation();

		// Replace cloned actor with Blueprint instance.
		{
			TArray<AActor*> Actors;
			Actors.Add(Actor);

			const FName FolderPath = Actor->GetFolderPath();
			Actor->RemoveFromRoot();
			AActor* NewActor = FKismetEditorUtilities::CreateBlueprintInstanceFromSelection(Blueprint, Actors, Location, Rotator);
			NewActor->SetFolderPath(FolderPath);

			if (NewActor)
			{
				NewActor->SetActorScale3D(Actor->GetActorScale3D());
			}

			BPActors.Add(NewActor);
		}

		// Save the created BP package.
		Package->MarkPackageDirty();
		PackagesToSave.Add(Package);
	}

	SaveBakedPackages(PackagesToSave);

	// Select the baked actors
	if (GEditor && BPActors.Num() > 0)
	{
		GEditor->SelectNone(false, true);
		for (AActor* BPActor : BPActors)
		{
			if (BPActor && !BPActor->IsPendingKill())
				GEditor->SelectActor(BPActor, true, false);
		}
		GEditor->NoteSelectionChange();
	}

	return BPActors;
}

TArray<AActor*>
FHoudiniEngineBakeUtils::BakePDGTOPNetworkBlueprints(UHoudiniPDGAssetLink* InPDGAssetLink, FTOPNetwork* InNetwork)
{
	TArray<AActor*> BakedBPs;
	if (!InPDGAssetLink || InPDGAssetLink->IsPendingKill())
		return BakedBPs;

	if (!InNetwork)
		return BakedBPs;

	for (FTOPNode& Node : InNetwork->AllTOPNodes)
	{
		BakedBPs.Append(BakePDGTOPNodeBlueprints(InPDGAssetLink, &Node));
	}

	return BakedBPs;
}

TArray<AActor*>
FHoudiniEngineBakeUtils::BakePDGAssetLinkBlueprints(UHoudiniPDGAssetLink* InPDGAssetLink)
{
	TArray<AActor*> BakedBPs;
	if (!InPDGAssetLink || InPDGAssetLink->IsPendingKill())
		return BakedBPs;

	switch(InPDGAssetLink->PDGBakeSelectionOption)
	{
		case EPDGBakeSelectionOption::All:
			for (FTOPNetwork& Network : InPDGAssetLink->AllTOPNetworks)
			{
				for (FTOPNode& Node : Network.AllTOPNodes)
				{
					BakedBPs.Append(BakePDGTOPNodeBlueprints(InPDGAssetLink, &Node));
				}
			}
			break;
		case EPDGBakeSelectionOption::SelectedNetwork:
			BakedBPs.Append(BakePDGTOPNetworkBlueprints(InPDGAssetLink, InPDGAssetLink->GetSelectedTOPNetwork()));
		case EPDGBakeSelectionOption::SelectedNode:
			BakedBPs.Append(BakePDGTOPNodeBlueprints(InPDGAssetLink, InPDGAssetLink->GetSelectedTOPNode()));
	}

	return BakedBPs;	
}

UPackage* 
FHoudiniEngineBakeUtils::FindOrCreateBlueprintPackage(
	const FString& InBaseName,
	const FDirectoryPath& InBakeFolder,
	FString& OutBlueprintName,
	bool &bOutFoundPackage,
	EPDGBakePackageReplaceModeOption InReplaceMode) 
{
	UPackage* Package = nullptr;

	int32 BakeCounter = 0;
	// FGuid BakeGUID = FGuid::NewGuid();

	// // We only want half of generated guid string.
	// FString BakeGUIDString = BakeGUID.ToString().Left(FHoudiniEngineUtils::PackageGUIDItemNameLength);

	while (true)
	{
		// Generate Blueprint name.
		OutBlueprintName = InBaseName + "_BP";
		if (BakeCounter > 0)
			OutBlueprintName += TEXT("_") + FString::FromInt(BakeCounter);

		// Generate unique package name.
		FString PackageName = InBakeFolder.Path + TEXT("/") + OutBlueprintName;
		PackageName = UPackageTools::SanitizePackageName(PackageName);

		// See if package exists, if it does, then we need to regenerate the name if InReplace mode is CreateNewAssets
		Package = FindPackage(nullptr, *PackageName);

		if (Package && !Package->IsPendingKill())
		{
			if (InReplaceMode == EPDGBakePackageReplaceModeOption::CreateNewAssets)
			{
				// we need to generate a new name for it
				BakeCounter++;
				continue;
			}

			bOutFoundPackage = true;
			break;
		}

		bOutFoundPackage = false;
		// Create actual package.
		Package = CreatePackage(nullptr, *PackageName);
		break;
	}

	return Package;
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

#undef LOCTEXT_NAMESPACE