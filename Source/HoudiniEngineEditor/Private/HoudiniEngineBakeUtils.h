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

#include "HoudiniPDGAssetLink.h"
#include "HoudiniOutput.h"

class UHoudiniAssetComponent;
class UHoudiniOutput;
class ALandscapeProxy;
class UStaticMesh;
class USplineComponent;
class UPackage;
class UWorld;
class AActor;
class UHoudiniSplineComponent;
class UStaticMeshComponent;

class UHoudiniPDGAssetLink;
class FHoudiniPackageParams;
struct FHoudiniGeoPartObject;
struct FHoudiniOutputObject;
struct FHoudiniOutputObjectIdentifier;
struct FTOPNetwork;
struct FTOPNode;
struct FHoudiniEngineOutputStats;

enum class EHoudiniLandscapeOutputBakeType : uint8;

struct HOUDINIENGINEEDITOR_API FHoudiniEngineBakeUtils
{
public:

	/** Bake static mesh. **/

	/*static UStaticMesh * BakeStaticMesh(
		UHoudiniAssetComponent * HoudiniAssetComponent,
		UStaticMesh * InStaticMesh,
		const FHoudiniPackageParams &PackageParams);*/

	static ALandscapeProxy* BakeHeightfield(
		ALandscapeProxy * InLandscapeProxy,
		const FHoudiniPackageParams &PackageParams,
		const EHoudiniLandscapeOutputBakeType & LandscapeOutputBakeType);

	static AActor* BakeCurve(
		USplineComponent* InSplineComponent,
		const FHoudiniPackageParams &PackageParams);

	static AActor* BakeInputHoudiniCurveToActor(
		UHoudiniSplineComponent * InHoudiniSplineComponent,
		const FHoudiniPackageParams & PakcageParams,
		UWorld* WorldToSpawn,
		const FTransform & SpawnTransform);

	static UBlueprint* BakeInputHoudiniCurveToBlueprint(
		UHoudiniSplineComponent * InHoudiniSplineComponent,
		const FHoudiniPackageParams & PakcageParams,
		UWorld* WorldToSpawn,
		const FTransform & SpawnTransform);

	static UStaticMesh* BakeStaticMesh(
		UStaticMesh * StaticMesh,
		const FHoudiniPackageParams & PackageParams);

	static TArray<AActor*> BakeInstancerOutputToActors(UHoudiniOutput * InOutput, TArray<UPackage*>& OutPackagesToSave);

	static TArray<AActor*> BakeInstancerOutputToActors_ISMC(
		const FHoudiniOutputObjectIdentifier& InOutputObjectIdentifier,
		const FHoudiniOutputObject& InOutputObject,
		UHoudiniAssetComponent* InHAC,
		TArray<UPackage*>& OutPackagesToSave);

	static TArray<AActor*> BakeInstancerOutputToActors_IAC(
		const FHoudiniOutputObjectIdentifier& InOutputObjectIdentifier,
		const FHoudiniOutputObject& InOutputObject,
		UHoudiniAssetComponent* InHAC);

	static TArray<AActor*> BakeInstancerOutputToActors_MSIC(
		const FHoudiniOutputObjectIdentifier& InOutputObjectIdentifier,
		const FHoudiniOutputObject& InOutputObject,
		UHoudiniAssetComponent* InHAC,
		TArray<UPackage*>& OutPackagesToSave);

	static TArray<AActor*> BakeInstancerOutputToActors_SMC(
		const FHoudiniOutputObjectIdentifier& InOutputObjectIdentifier,
		const FHoudiniOutputObject& InOutputObject,
		UHoudiniAssetComponent* InHAC,
		TArray<UPackage*>& OutPackagesToSave);

	static UStaticMesh * DuplicateStaticMeshAndCreatePackageIfNeeded(
		UStaticMesh * InStaticMesh,
		const FHoudiniPackageParams &PackageParams,
		UHoudiniAssetComponent* InHAC,
		TArray<UPackage*> & OutCreatedPackages);

	static UStaticMesh * DuplicateStaticMeshAndCreatePackageIfNeeded(
		UStaticMesh * InStaticMesh,
		const FHoudiniPackageParams &PackageParams,
		const TArray<UHoudiniOutput*>& InParentOutputs,
		const FString& InTemporaryCookFolder,
		TArray<UPackage*> & OutCreatedPackages);

	static UMaterial * DuplicateMaterialAndCreatePackage(
		UMaterial * Material,
		const FString & SubMaterialName,
		const FHoudiniPackageParams& ObjectPackageParams,
		TArray<UPackage*> & OutCreatedPackages);

	static void ReplaceDuplicatedMaterialTextureSample(
		UMaterialExpression * MaterialExpression, 
		const FHoudiniPackageParams& PackageParams,
		TArray<UPackage*> & OutCreatedPackages);
	
	static UTexture2D * DuplicateTextureAndCreatePackage(
		UTexture2D * Texture,
		const FString & SubTextureName,
		const FHoudiniPackageParams& PackageParams,
		TArray<UPackage*> & OutCreatedPackages);

	static AActor * CloneComponentsAndCreateActor(UHoudiniAssetComponent* HoudiniAssetComponent, TArray<UPackage*> & OutCreatedPackages);

	static bool BakeHoudiniActorToActors(UHoudiniAssetComponent* HoudiniAssetComponent);

	static bool BakeHoudiniActorToActors(UHoudiniAssetComponent* HoudiniAssetComponent, TArray<AActor*>& OutNewActors, TArray<UPackage*>& OutPackagesToSave, FHoudiniEngineOutputStats& OutBakeStats);

	static bool ReplaceHoudiniActorWithActors(UHoudiniAssetComponent* HoudiniAssetComponent);

	static bool CanHoudiniAssetComponentBakeToFoliage(UHoudiniAssetComponent* HoudiniAssetComponent);

	static bool BakeHoudiniActorToFoliage(UHoudiniAssetComponent* HoudiniAssetComponent);

	static bool ReplaceHoudiniActorWithFoliage(UHoudiniAssetComponent* HoudiniAssetComponent);

	static TArray<AActor*> BakeStaticMeshOutputToActors(UHoudiniOutput* Output, TArray<UPackage*>& OutPackagesToSave);

	static TArray<AActor*> BakeHoudiniCurveOutputToActors(UHoudiniOutput* Output);

	static AActor* ReplaceWithBlueprint(UHoudiniAssetComponent* HoudiniAssetComponent);

	static UBlueprint * BakeBlueprint(UHoudiniAssetComponent* HoudiniAssetComponent);

	static bool CopyActorContentsToBlueprint(AActor * InActor, UBlueprint * OutBlueprint);

	static UPackage* BakeCreateBlueprintPackageForComponent(
		UHoudiniAssetComponent* HoudiniAssetComponent,
		FString & BlueprintName,
		bool & bFoundPackage);

	static void AddHoudiniMetaInformationToPackage(
			UPackage * Package, UObject * Object, const TCHAR * Key,
			const TCHAR * Value);

	static bool GetHoudiniGeneratedNameFromMetaInformation(
		UPackage * Package, UObject * Object, FString & HoudiniName);

	static bool DeleteBakedHoudiniAssetActor(UHoudiniAssetComponent* HoudiniAssetComponent);

	static void SaveBakedPackages(TArray<UPackage*> & PackagesToSave, bool bSaveCurrentWorld = false);

	static bool IsObjectTemporary(UObject* InObject, UHoudiniAssetComponent* InHAC);

	static bool IsObjectTemporary(
		UObject* InObject, const TArray<UHoudiniOutput*>& InParentOutputs, const FString& InTemporaryCookFolder);

	// Function used to copy properties from the source Static Mesh Component to the new (baked) one
	static void CopyPropertyToNewActorAndComponent(
		AActor* NewActor, UStaticMeshComponent* NewSMC, UStaticMeshComponent* InSMC);

	// Finds an existing package for a blueprint asset, or creates a new one.
	// Adds the _BP suffix to InBaseName when constructing the asset name.
	static UPackage* FindOrCreateBlueprintPackage(
		const FString& InBaseName,
		const FDirectoryPath& InBakeFolder,
		FString& OutBlueprintName,
		bool &bOutFoundPackage,
		EPDGBakePackageReplaceModeOption InReplaceMode = EPDGBakePackageReplaceModeOption::ReplaceExistingAssets);

	// Finds the world/level indicated by the package path.
	// If the level doesn't exists, it will be created.
	// If InLevelPath is empty, outputs the editor world and current level
	// Returns true if the world/level were found, false otherwise
	static bool FindOrCreateDesiredLevelFromLevelPath(
		const FString& InLevelPath,
		ULevel*& OutDesiredLevel,
		UWorld*& OutDesiredWorld,
		bool& OutCreatedPackage);

	// Try to find an actor that we can use for baking.
	// If the requested actor could not be found, then `OutWorld` and `OutLevel`
	// should be used to spawn the new bake actor.
	// @returns AActor* if found. Otherwise, returns nullptr.
	static AActor* FindExistingActor_Bake(
		UWorld* InWorld,
		UHoudiniOutput* InOutput,
		const FString& InActorName,
		const FString& InPackagePath,
		UWorld*& OutWorld,
		ULevel*& OutLevel,
		bool& bCreatedPackage);

	// Start: PDG Baking

	// Remove a previously baked actor
	static bool RemovePreviouslyBakedActor(
		AActor* InNewBakedActor,
		ULevel* InLevel,
		const FHoudiniPackageParams& InPackageParams);

	// Get the world outliner folder path for output generated by InOutputOwner
	static FName GetOutputFolderPath(UObject* InOutputOwner);
	
	// Detach InActor from its parent, and rename to InNewName (attaches a numeric suffix to make it unique via
	// MakeUniqueObjectName). Place it in the world outliner folder InFolderPath.
	static bool DetachAndRenameBakedPDGOutputActor(AActor* InActor, const FString& InNewName, const FName& InFolderPath);
	
	// Bake static mesh from PDG output. Uses BakePDGStaticMeshOutputObject on each static mesh output object.
	static bool BakePDGStaticMeshOutput(
		UHoudiniPDGAssetLink* InPDGAssetLink,
		AActor* InOutputActor,
		const FString& InHoudiniAssetName,
		UHoudiniOutput* InOutput,
		const TArray<UHoudiniOutput*>& InParentOutputs,
		const FName& InWorldOutlinerFolderPath);

	// Bake an instancer PDG output. Keep the output actor, but detach if from PDG.
	// For instancers that use components that inherit from UStaticMeshComponent BakePDGStaticMeshOutputObject is called
	// otherwise BakePDGInstancerOutputKeepActors_IAC or BakePDGInstancerOutputKeepActors_MSIC are used.
	// For blueprint baking UHoudiniInstancedActorComponent is not supported, so bSkipIAC can be used to skip baking
	// UHoudiniInstancedActorComponent instancers, bOutSkippedIAC is true if there were any IACs that were skipped.
	// Bakes any used StaticMeshes that are temporary and updates the components to use the baked mesh.
	static TArray<AActor*> BakePDGInstancerOutputKeepActors(
		UHoudiniPDGAssetLink* InPDGAssetLink,
		const TArray<UHoudiniOutput*>& InAllOutputs,
		AActor* InOutputActor,
		UHoudiniOutput* InOutput,
		const FString& InHoudiniAssetName,
		bool bSkipIAC,
		bool& bOutSkippedIAC);

	// Bake UHoudiniInstancedActorComponent instancer. Creates a new actor for in InOutputFolderPath for instance and
	// destroys the UHoudiniInstancedActorComponent component.
	static TArray<AActor*> BakePDGInstancerOutputKeepActors_IAC(
		FHoudiniOutputObject& InOutputObject, const FName& InOutputFolderPath);

	// Bake UHoudiniMeshSplitInstancerComponent instancer. Duplicates the static mesh components to the PDG output
	// actor and detaches it and places it in InOutputFolderPath in the world outliner. Destroys the
	// UHoudiniMeshSplitInstancerComponent. Bakes any used StaticMeshes that are temporary and updates the components
	// to use the baked mesh.
	static bool BakePDGInstancerOutputKeepActors_MSIC(
		const FHoudiniOutputObjectIdentifier& InOutputObjectIdentifier,
		FHoudiniOutputObject& InOutputObject,
		const FName& InOutputFolderPath,
		const TArray<UHoudiniOutput*>& InParentOutputs,
		AActor* InActor,
		const FDirectoryPath& InTemporaryCookFolder,
		const FDirectoryPath& InBakeFolder,
		const EPDGBakePackageReplaceModeOption& InReplaceMode);

	// Bakes a static mesh output object. Keeps the output actor and component, but detach it from PDG. Updates the
	// component to use the baked static mesh.
	static bool BakePDGStaticMeshOutputObject(
		const FHoudiniOutputObjectIdentifier& InOutputObjectIdentifier,
		FHoudiniOutputObject& InOutputObject,
		const FName& InOutputFolderPath,
		const TArray<UHoudiniOutput*>& InParentOutputs,
		UHoudiniOutput* InOutput,
		AActor* InActor,
		const FString& InHoudiniAssetName,
		const FDirectoryPath& InTemporaryCookFolder,
		const FDirectoryPath& InBakeFolder,
		const EPDGBakePackageReplaceModeOption& InReplaceMode);
	
	// Bake PDG output. This bakes all assets from all work items in the specified InNode (FTOPNode).
	// It uses the existing output actors in the level, but breaks any links from these actors to the PDG link and
	// moves the actors out of the parent Folder/ detaches from the parent PDG output actor.
	static TArray<AActor*> BakePDGTOPNodeOutputsKeepActors(
		UHoudiniPDGAssetLink* InPDGAssetLink,
		FTOPNode* InNode,
		bool bInUseWorkResultActor=true,
		bool bInBakeForBlueprint=false);

	// Bake PDG output. This bakes all assets from all work items in the specified TOP network.
	// It uses the existing output actors in the level, but breaks any links
	// from these actors to the PDG link and moves the actors out of the parent Folder/ detaches from the parent
	// PDG output actor.
	static bool BakePDGTOPNetworkOutputsKeepActors(UHoudiniPDGAssetLink* InPDGAssetLink, FTOPNetwork* InNetwork);

	// Bake PDG output. This bakes assets from TOP networks and nodes according to
	// InPDGAssetLink->PDGBakeSelectionOption. It uses the existing output actors in the level, but breaks any links
	// from these actors to the PDG link and moves the actors out of the parent Folder/ detaches from the parent
	// PDG output actor.
	static bool BakePDGAssetLinkOutputsKeepActors(UHoudiniPDGAssetLink* InPDGAssetLink);

	// Bake PDG output. This bakes all supported assets from all work items in the specified InNode (FTOPNode).
	// It duplicates the output actors and bakes them to blueprints. Assets that were baked are removed from
	// PDG output actors.
	static TArray<AActor*> BakePDGTOPNodeBlueprints(UHoudiniPDGAssetLink* InPDGAssetLink, FTOPNode* InNode);
	
	// Bake PDG output. This bakes all supported assets from all work items in the specified TOP network.
	// It duplicates the output actors and bakes them to blueprints. Assets that were baked are removed from
	// PDG output actors.
	static TArray<AActor*> BakePDGTOPNetworkBlueprints(UHoudiniPDGAssetLink* InPDGAssetLink, FTOPNetwork* InNetwork);

	// Bake PDG output. This bakes assets from TOP networks and nodes according to
	// InPDGAssetLink->PDGBakeSelectionOption. It duplicates the output actors and bakes them to blueprints. Assets
	// that were baked are removed from PDG output actors.
	static TArray<AActor*> BakePDGAssetLinkBlueprints(UHoudiniPDGAssetLink* InPDGAssetLink);

	// End: PDG Baking
};
