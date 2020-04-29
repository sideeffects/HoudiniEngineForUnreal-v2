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
class UHoudiniAssetComponent;
class UHoudiniOutput;
class ALandscapeProxy;
class UStaticMesh;
class USplineComponent;
class UPackage;
class UWorld;
class AActor;
class UHoudiniSplineComponent;

class FHoudiniPackageParams;
struct FHoudiniGeoPartObject;
struct FHoudiniOutputObjectIdentifier;

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
		const UStaticMesh * StaticMesh,
		const FHoudiniPackageParams & PackageParams);

	static UStaticMesh * DuplicateStaticMeshAndCreatePackage(
		const UStaticMesh * StaticMesh,
		const FHoudiniPackageParams &PackageParams,
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

	static bool ReplaceHoudiniActorWithActors(UHoudiniAssetComponent* HoudiniAssetComponent);

	static bool CanHoudiniAssetComponentBakeToFoliage(UHoudiniAssetComponent* HoudiniAssetComponent);

	static bool BakeHoudiniActorToFoliage(UHoudiniAssetComponent* HoudiniAssetComponent);

	static bool ReplaceHoudiniActorWithFoliage(UHoudiniAssetComponent* HoudiniAssetComponent);

	static TArray<AActor*> BakeHoudiniStaticMeshOutputToActors(UHoudiniOutput* Output);

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

	static void FillInPackageParamsForBakingOutput(
		FHoudiniPackageParams& OutPackageParams,
		const FHoudiniOutputObjectIdentifier& InIdentifier,
		const FString &BakeFolder,
		const FString &HoudiniAssetName,
		const FString &ObjectName);

	static bool DeleteBakedHoudiniAssetActor(UHoudiniAssetComponent* HoudiniAssetComponent);

	static void SaveBakedPackages(TArray<UPackage*> & PackagesToSave, bool bSaveCurrentWorld = false);
};
