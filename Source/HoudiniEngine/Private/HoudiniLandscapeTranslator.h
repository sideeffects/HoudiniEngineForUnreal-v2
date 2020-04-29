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

#include "HAPI/HAPI_Common.h"
#include "Landscape.h"
#include "HoudiniGeoPartObject.h"
#include "HoudiniOutput.h"

class UHoudiniAssetComponent;
class FHoudiniPackageParams;
class ULandscapeLayerInfoObject;
struct FHoudiniGenericAttribute;

struct HOUDINIENGINE_API FHoudiniLandscapeTranslator
{
	public:
		static bool CreateAllLandscapesFromHoudiniOutput(
			UHoudiniOutput* InOutput,
			TArray<ALandscapeProxy *>& InputLandscapesToUpdate,
			TArray<ALandscapeProxy *>& ValidLandscapes,
			float fGlobalMin, float fGlobalMax,
			bool bWorldComposition,
			FHoudiniPackageParams InPackageParams);

		static const FHoudiniGeoPartObject* GetHoudiniHeightFieldFromOutput(
			UHoudiniOutput* InOutput);

		static void GetHeightfieldsLayersFromOutput(
			const UHoudiniOutput* InOutput,
			const FHoudiniGeoPartObject& Heightfield,
			TArray< const FHoudiniGeoPartObject* >& FoundLayers);

		static bool GetHoudiniHeightfieldFloatData(
			const FHoudiniGeoPartObject* HGPO,
			TArray<float> &OutFloatArr,
			float &OutFloatMin,
			float &OutFloatMax);

		static bool CalcLandscapeSizeFromHeightfieldSize(
			const int32& HoudiniSizeX,
			const int32& HoudiniSizeY,
			int32& UnrealSizeX,
			int32& UnrealSizeY,
			int32& NumSectionsPerComponent,
			int32& NumQuadsPerSection);

		static bool ConvertHeightfieldDataToLandscapeData(
			const TArray< float >& HeightfieldFloatValues,
			const FHoudiniVolumeInfo& HeightfieldVolumeInfo,
			const int32& FinalXSize,
			const int32& FinalYSize,
			float FloatMin,
			float FloatMax,
			TArray< uint16 >& IntHeightData,
			FTransform& LandscapeTransform,
			const bool& NoResize = false);

		static bool ResizeHeightDataForLandscape(
			TArray<uint16>& HeightData,
			const int32& SizeX,
			const int32& SizeY,
			const int32& NewSizeX,
			const int32& NewSizeY,
			FVector& LandscapeResizeFactor,
			FVector& LandscapePositionOffset);

		static bool CreateOrUpdateLandscapeLayers(
			const TArray<const FHoudiniGeoPartObject*>& FoundLayers,
			const FHoudiniGeoPartObject& HeightField,
			const int32& LandscapeXSize,
			const int32& LandscapeYSize,
			TMap<FString, float> &GlobalMinimums,
			TMap<FString, float> &GlobalMaximums,
			TArray<FLandscapeImportLayerInfo>& OutLayerInfos,
			bool bIsUpdate,
			const FHoudiniPackageParams& InPackageParams);

		static bool GetNonWeightBlendedLayerNames(
			const FHoudiniGeoPartObject& HeightfieldGeoPartObject,
			TArray<FString>& NonWeightBlendedLayerNames);

		static bool IsUnitLandscapeLayer(
			const FHoudiniGeoPartObject& LayerGeoPartObject);

		// Return the height min/max values for all 
		static bool CalcHeightGlobalZminZMax(
			const TArray<UHoudiniOutput*>& AllOutputs,
			float& OutGlobalMin,
			float& OutGlobalMax);

		// Returns the min/max values per layer/volume for an array of volumes/heightfields
		static void CalcHeightfieldsArrayGlobalZMinZMax(
			const TArray< FHoudiniGeoPartObject > & InHeightfieldArray,
			TMap<FString, float>& GlobalMinimums,
			TMap<FString, float>& GlobalMaximums);

		static bool ConvertHeightfieldLayerToLandscapeLayer(
			const TArray<float>& FloatLayerData,
			const int32& HoudiniXSize,
			const int32& HoudiniYSize,
			const float& LayerMin,
			const float& LayerMax,
			const int32& LandscapeXSize,
			const int32& LandscapeYSize,
			TArray<uint8>& LayerData,
			const bool& NoResize = false);

		static bool ResizeLayerDataForLandscape(
			TArray< uint8 >& LayerData,
			const int32& SizeX,
			const int32& SizeY,
			const int32& NewSizeX,
			const int32& NewSizeY);

		static ALandscapeProxy * CreateLandscape(
			const TArray< uint16 >& IntHeightData,
			const TArray< FLandscapeImportLayerInfo >& ImportLayerInfos,
			const FTransform& LandscapeTransform,
			const int32& XSize,
			const int32& YSize,
			const int32& NumSectionPerLandscapeComponent,
			const int32& NumQuadsPerLandscapeSection,
			UMaterialInterface* LandscapeMaterial,
			UMaterialInterface* LandscapeHoleMaterial,
			const bool& CreateLandscapeStreamingProxy,
			bool bNeedCreateNewWorld,
			UWorld* SpawnWorld,
			FHoudiniPackageParams InPackageParams);

		static void GetHeightFieldLandscapeMaterials(
			const FHoudiniGeoPartObject& Heightfield,
			UMaterialInterface*& LandscapeMaterial,
			UMaterialInterface*& LandscapeHoleMaterial);

		static bool GetLandscapeComponentExtentAttributes(
			const FHoudiniGeoPartObject& HoudiniGeoPartObject,
			int32& MinX,
			int32& MaxX,
			int32& MinY,
			int32& MaxY);

		static ULandscapeLayerInfoObject* CreateLandscapeLayerInfoObject(
			const FString& InLayerName,
			const FString& InPackagePath,
			const FString& InPackageName,
			UPackage*& OutPackage);

		static bool EnableWorldComposition();

		static bool GetGenericPropertiesAttributes(
			const HAPI_NodeId& InGeoNodeId,
			const HAPI_PartId& InPartId,
			const int32& InPrimIndex,
			TArray<FHoudiniGenericAttribute>& OutPropertyAttributes);
		
		static bool UpdateGenericPropertiesAttributes(
			UObject* InObject,
			const TArray<FHoudiniGenericAttribute>& InAllPropertyAttributes);

		static bool BackupLandscapeToImageFiles(
			const FString& BaseName, ALandscapeProxy* Landscape);

		static bool RestoreLandscapeFromImageFiles(ALandscapeProxy* LandscapeProxy);

	private:

		static bool ImportLandscapeData(
			ULandscapeInfo* LandscapeInfo,
			const FString& Filename,
			const FString& LayerName,
			ULandscapeLayerInfoObject* LayerInfoObject = nullptr);
};