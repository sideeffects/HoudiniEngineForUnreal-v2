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
#include "HoudiniLandscapeTranslator.h"

#include "HoudiniAssetComponent.h"
#include "HoudiniGeoPartObject.h"
#include "HoudiniEngineString.h"
#include "HoudiniApi.h"
#include "HoudiniEngine.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniEngineRuntime.h"
#include "HoudiniRuntimeSettings.h"
#include "HoudiniEnginePrivatePCH.h"
#include "HoudiniGenericAttribute.h"
#include "HoudiniPackageParams.h"

#include "ObjectTools.h"
#include "FileHelpers.h"
#include "Editor.h"
#include "LandscapeLayerInfoObject.h"
#include "LandscapeStreamingProxy.h"
#include "LandscapeInfo.h"
#include "LandscapeEdit.h"
#include "AssetRegistryModule.h"
#include "PackageTools.h"

#include "GameFramework/WorldSettings.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/Paths.h"
#include "Engine/LevelStreamingDynamic.h"
#include "Modules/ModuleManager.h"
#include "AssetToolsModule.h"
#include "Factories/WorldFactory.h"

#if WITH_EDITOR
	#include "LandscapeEditorModule.h"
	#include "LandscapeFileFormatInterface.h"
#endif


bool
FHoudiniLandscapeTranslator::CreateAllLandscapesFromHoudiniOutput(
	UHoudiniOutput* InOutput,
	TArray<ALandscapeProxy *>& InputLandscapesToUpdate,
	TArray<ALandscapeProxy *>& ValidLandscapes,
	float fInGlobalMin,
	float fInGlobalMax,
	bool bWorldComposition,
	FHoudiniPackageParams InPackageParams)
{
	float fGlobalMin = fInGlobalMin;
	float fGlobalMax = fInGlobalMax;

	if (!InOutput || InOutput->IsPendingKill())
		return false;

	//  Get the height map.
	const FHoudiniGeoPartObject* Heightfield = GetHoudiniHeightFieldFromOutput(InOutput);
	if (!Heightfield)
		return false;

	// Get layer min max.
	TMap<FString, float> GlobalMinimums;
	TMap<FString, float> GlobalMaximums;
	FHoudiniLandscapeTranslator::CalcHeightfieldsArrayGlobalZMinZMax(InOutput->GetHoudiniGeoPartObjects(), GlobalMinimums, GlobalMaximums);

	if (Heightfield->Type != EHoudiniPartType::Volume)
		return false;

	HAPI_NodeId GeoId = Heightfield->GeoId;
	HAPI_PartId PartId = Heightfield->PartId;

	// Get the identifier of the Heightfield GeoPart
	FHoudiniOutputObjectIdentifier Identifier(Heightfield->ObjectId, GeoId, PartId, "Heightfield");
	Identifier.PartName = Heightfield->PartName;

	// Look for all the layers/masks corresponding to the current heightfield.
	TArray< const FHoudiniGeoPartObject* > FoundLayers;
	FHoudiniLandscapeTranslator::GetHeightfieldsLayersFromOutput(InOutput, *Heightfield, FoundLayers);

	// Look for the unreal_landscape_streaming_proxy attribute.
	bool bCreateLandscapeStreamingProxy = false;
	TArray<int> IntData;
	HAPI_AttributeInfo AttributeInfo;
	FHoudiniApi::AttributeInfo_Init(&AttributeInfo);

	if (FHoudiniEngineUtils::HapiGetAttributeDataAsInteger(
	     GeoId, PartId,
		"unreal_landscape_streaming_proxy", AttributeInfo, IntData, 1))
	{
		if (IntData.Num() > 0 && IntData[0] != 0)
			bCreateLandscapeStreamingProxy = true;
	}

	// See if the current heightfield has an unreal_material or unreal_hole_material assigned to it
	UMaterialInterface* LandscapeMaterial = nullptr;
	UMaterialInterface* LandscapeHoleMaterial = nullptr;
	FHoudiniLandscapeTranslator::GetHeightFieldLandscapeMaterials(*Heightfield, LandscapeMaterial, LandscapeHoleMaterial);

	// Extract the float data from the Heightfield.
	const FHoudiniVolumeInfo &VolumeInfo = Heightfield->VolumeInfo;
	TArray<float> FloatValues;
	float FloatMin, FloatMax;
	if (!GetHoudiniHeightfieldFloatData(Heightfield, FloatValues, FloatMin, FloatMax))
		return false;

	if (fGlobalMin != fGlobalMax)
	{	
		FloatMin = fGlobalMin;
		FloatMax = fGlobalMax;
	}

	// Get the Unreal landscape size 
	int32 HoudiniHeightfieldXSize = VolumeInfo.YLength;
	int32 HoudiniHeightfieldYSize = VolumeInfo.XLength;
	int32 UnrealLandscapeSizeX = -1;
	int32 UnrealLandscapeSizeY = -1;
	int32 NumSectionPerLandscapeComponent = -1;
	int32 NumQuadsPerLandscapeSection = -1;

	if (!FHoudiniLandscapeTranslator::CalcLandscapeSizeFromHeightfieldSize(
		HoudiniHeightfieldXSize, HoudiniHeightfieldYSize,
		UnrealLandscapeSizeX, UnrealLandscapeSizeY,
		NumSectionPerLandscapeComponent, NumQuadsPerLandscapeSection))
	{
		return false;
	}

	// See if the output is attached to HAC
	bool bIsOuterHAC = false;
	if (InOutput->GetOuter() && InOutput->GetOuter()->IsA<UHoudiniAssetComponent>())
		bIsOuterHAC = true;

	// See if we can find a landscape to update or if we need to create a new one
	bool bNeedToCreateNewLandscape = true;
	bool bNeedToCreateNewMap = bWorldComposition;
	ALandscapeProxy* FoundLandscapeProxy = nullptr;

	// Start by looking in the input landscape that can be updated.
	// Try to see if we have an input landscape that matches the size of the current HGPO	
	for (int nIdx = 0; nIdx < InputLandscapesToUpdate.Num(); nIdx++)
	{
		ALandscapeProxy* CurrentInputLandscape = InputLandscapesToUpdate[nIdx];
		if (!CurrentInputLandscape)
			continue;

		ULandscapeInfo* CurrentInfo = CurrentInputLandscape->GetLandscapeInfo();
		if (!CurrentInfo)
			continue;

		int32 InputMinX = 0;
		int32 InputMinY = 0;
		int32 InputMaxX = 0;
		int32 InputMaxY = 0;
		CurrentInfo->GetLandscapeExtent(InputMinX, InputMinY, InputMaxX, InputMaxY);

		// If the full size matches, we'll update that input landscape
		bool SizeMatch = false;
		if ((InputMaxX - InputMinX + 1) == UnrealLandscapeSizeX && (InputMaxY - InputMinY + 1) == UnrealLandscapeSizeY)
			SizeMatch = true;

		// HF and landscape don't match, try another one
		if (!SizeMatch)
			continue;

		// Replace FoundLandscape by that input landscape
		FoundLandscapeProxy = CurrentInputLandscape;

		// We've found a valid input landscape, remove it from the input array so we dont try to update it twice
		InputLandscapesToUpdate.RemoveAt(nIdx);
		bNeedToCreateNewLandscape = false;
		break;
	}
	
	// Keep track of the previous cook's landscapes
	TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& OldOutputObjects = InOutput->GetOutputObjects();
	//Try to see if we can reuse a previously cooked a landscape.
	bool bReusingPreviousLandscape = false;
	if (bNeedToCreateNewLandscape)
	{		
		for (auto& CurrentLandscape : OldOutputObjects)
		{
			UHoudiniLandscapePtr* LandscapePtr = Cast<UHoudiniLandscapePtr>(CurrentLandscape.Value.OutputObject);
			if (!LandscapePtr)
				continue;

			FoundLandscapeProxy = LandscapePtr->GetRawPtr();
			if (!FoundLandscapeProxy)
			{
				// We may need to manually load the object
				//OldLandscapeProxy = LandscapePtr->GetSoftPtr().LoadSynchronous();
				FoundLandscapeProxy = LandscapePtr->LandscapeSoftPtr.LoadSynchronous();
			}

			if (!FoundLandscapeProxy)
				continue;

			// If we were updating the input landscape before, but arent anymore,
			// we could still find it here in the output, ignore them now as we're only looking for previous output
			if (ValidLandscapes.Contains(FoundLandscapeProxy))
			{
				FoundLandscapeProxy = nullptr;
				continue;
			}

			// If we found a possible candidate, make sure that its size matches ours
			// as we can only update a landscape of the same size
			ULandscapeInfo* PreviousInfo = FoundLandscapeProxy->GetLandscapeInfo();
			if (PreviousInfo)
			{
				int32 PrevMinX = 0;
				int32 PrevMinY = 0;
				int32 PrevMaxX = 0;
				int32 PrevMaxY = 0;
				PreviousInfo->GetLandscapeExtent(PrevMinX, PrevMinY, PrevMaxX, PrevMaxY);

				if ((PrevMaxX - PrevMinX + 1) == UnrealLandscapeSizeX && (PrevMaxY - PrevMinY + 1) == UnrealLandscapeSizeY)
				{
					// The size matches, we can reuse the old landscape.
					bNeedToCreateNewLandscape = false;
				}
			}

			if (bNeedToCreateNewLandscape)
				FoundLandscapeProxy = nullptr;

			if (!FoundLandscapeProxy)
				continue;

			bReusingPreviousLandscape = true;

			break;
		}
	}

	// No need to create a new world composition map if a valid landscape proxy has been found
	if (!bNeedToCreateNewLandscape)
		bNeedToCreateNewMap = false;

	// Convert Houdini's heightfield data to Unreal's landscape data
	TArray<uint16> IntHeightData;
	FTransform LandscapeTransform;
	if (!FHoudiniLandscapeTranslator::ConvertHeightfieldDataToLandscapeData(
		FloatValues, VolumeInfo,
		UnrealLandscapeSizeX, UnrealLandscapeSizeY,
		FloatMin, FloatMax,
		IntHeightData, LandscapeTransform))
		return false;

	// Create a new map for this output's current landscape
	TMap<FHoudiniOutputObjectIdentifier, UObject*> NewLandscapes;
	ALandscapeProxy* LandscapeProxy = nullptr;
	if (bNeedToCreateNewLandscape)
	{
		// Spawn a new Landscape
		UWorld* SpawnWorld = nullptr;
		if (!SpawnWorld) 
		{
			// Get the world we'll spawn the landscape in
			// We want to create the landscape in the landscape editor mode's world
			FWorldContext& EditorWorldContext = GEditor->GetEditorWorldContext();
			SpawnWorld = EditorWorldContext.World();
		}

		TArray<FLandscapeImportLayerInfo> ImportLayerInfos;
		if (!CreateOrUpdateLandscapeLayers(FoundLayers, *Heightfield, UnrealLandscapeSizeX, UnrealLandscapeSizeY, 
			GlobalMinimums, GlobalMaximums, ImportLayerInfos, false, InPackageParams))
			return false;

		// Create a PackageParam for this tile
		FHoudiniPackageParams TilePackageParams = InPackageParams;
		if(bWorldComposition && Heightfield->VolumeTileIndex >= 0)
			TilePackageParams.ObjectName += "_Tile" + FString::FromInt(Heightfield->VolumeTileIndex);

		// Create the Landscape
		ALandscapeProxy * CreatedLandscape = FHoudiniLandscapeTranslator::CreateLandscape(
			IntHeightData, ImportLayerInfos, LandscapeTransform, 
			UnrealLandscapeSizeX, UnrealLandscapeSizeY,
			NumSectionPerLandscapeComponent, NumQuadsPerLandscapeSection,
			LandscapeMaterial, LandscapeHoleMaterial,
			bCreateLandscapeStreamingProxy, bNeedToCreateNewMap,
			SpawnWorld, TilePackageParams);

		if (!CreatedLandscape || !CreatedLandscape->IsValidLowLevel())
			return false;

		// Update the visibility mask / layer if we have any
		for (auto CurrLayerInfo : ImportLayerInfos)
		{
			if (CurrLayerInfo.LayerInfo && CurrLayerInfo.LayerName.ToString().Equals(TEXT("Visibility"), ESearchCase::IgnoreCase))
			{
				CreatedLandscape->VisibilityLayer = CurrLayerInfo.LayerInfo;
				CreatedLandscape->VisibilityLayer->bNoWeightBlend = true;
				CreatedLandscape->VisibilityLayer->AddToRoot();
			}
		}

		LandscapeProxy = Cast<ALandscapeProxy>(CreatedLandscape);
	}
	else 
	{
		// Reuse an existing one
		ULandscapeInfo* PreviousInfo = FoundLandscapeProxy->GetLandscapeInfo();
		if (!PreviousInfo)
				return false;

		FLandscapeEditDataInterface LandscapeEdit(PreviousInfo);

		// Update height if it has been changed.
		if (Heightfield->bHasGeoChanged)
		{
			LandscapeEdit.SetHeightData(0, 0, UnrealLandscapeSizeX - 1, UnrealLandscapeSizeY - 1, IntHeightData.GetData(), 0, true);
			FoundLandscapeProxy->SetActorRelativeTransform(LandscapeTransform);
		}

		// Get the updated layers.
		TArray<FLandscapeImportLayerInfo> UpdatedLayerInfos;
		if (!CreateOrUpdateLandscapeLayers(FoundLayers, *Heightfield, UnrealLandscapeSizeX, UnrealLandscapeSizeY, 
			GlobalMinimums, GlobalMaximums, UpdatedLayerInfos, true, InPackageParams))
			return false;

		// Update the layers on the landscape.
		for (FLandscapeImportLayerInfo &NextUpdatedLayerInfo : UpdatedLayerInfos) 
		{
			LandscapeEdit.SetAlphaData(NextUpdatedLayerInfo.LayerInfo, 0, 0, UnrealLandscapeSizeX - 1, UnrealLandscapeSizeY - 1, NextUpdatedLayerInfo.LayerData.GetData(), 0);

			if (NextUpdatedLayerInfo.LayerInfo && NextUpdatedLayerInfo.LayerName.ToString().Equals(TEXT("Visibility"), ESearchCase::IgnoreCase))
			{
				FoundLandscapeProxy->VisibilityLayer = NextUpdatedLayerInfo.LayerInfo;
				FoundLandscapeProxy->VisibilityLayer->bNoWeightBlend = true;
				FoundLandscapeProxy->VisibilityLayer->AddToRoot();
			}
		}
		
		// Update the materials if they have changed
		if (FoundLandscapeProxy->GetLandscapeMaterial() != LandscapeMaterial)
			FoundLandscapeProxy->LandscapeMaterial = LandscapeMaterial;

		if (FoundLandscapeProxy->GetLandscapeHoleMaterial() != LandscapeHoleMaterial)
			FoundLandscapeProxy->LandscapeHoleMaterial = LandscapeHoleMaterial;

		LandscapeProxy = FoundLandscapeProxy;
	}

	// See if we have unreal_tag_ attribute
	TArray<FName> Tags;
	if (LandscapeProxy && FHoudiniEngineUtils::GetUnrealTagAttributes(GeoId, PartId, Tags)) 
	{
		LandscapeProxy->Tags = Tags;
	}

	// Update the landscape's collisions
	LandscapeProxy->RecreateCollisionComponents();

	/*
	// TODO: Useless for now as we cant get attribute names on volumes... (HAPI fix?)
	// Extract/update the generic attributes
	// TODO: Handle prim index instead of defaulting to 0
	TArray<FHoudiniGenericAttribute> AllPropertyAttributes;
	if (GetGenericPropertiesAttributes(GeoId, PartId, 0,  AllPropertyAttributes))
	{
		// Apply generic attributes if we have any		
		UpdateGenericPropertiesAttributes(LandscapeProxy, AllPropertyAttributes);
	}
	*/

	// Now destroy previous landscape unless they have been reused or are input landscapes
	bool bCleanUpOld = false;
	for (auto& OldPair : OldOutputObjects)
	{
		UHoudiniLandscapePtr* OldLandscapePtr = Cast<UHoudiniLandscapePtr>(OldPair.Value.OutputObject);
		ALandscapeProxy* OldLandscapeProxy = OldLandscapePtr->GetRawPtr();
		if (!OldLandscapeProxy)
		{
			// We may need to manually load the object
			OldLandscapeProxy = OldLandscapePtr->LandscapeSoftPtr.LoadSynchronous();
		}

		// No need to destroy a reused landscape proxy
		if (LandscapeProxy == OldLandscapeProxy)
			continue;

		// We shouldnt destroy any input landscape
		if (ValidLandscapes.Contains(OldLandscapeProxy))
			continue;

		if (OldLandscapeProxy)
		{
			// Destroy the old landscape as it's no longer used
			bCleanUpOld = true;
			OldLandscapeProxy->UnregisterAllComponents();
			OldLandscapeProxy->Destroy();
		}
	}

	if (bCleanUpOld)
	{
		InOutput->GetOutputObjects().Empty();
	}

	// If our output's outer is the transient package, then don't bother 
	// creating a HoudiniLandscapePtr, since it means that we're used by the
	// GeoImporter and not a HoudiniAssetComponent
	if (bIsOuterHAC)
	{
		// Build a HoudiniLandscapePtr object
		TSoftObjectPtr<ALandscapeProxy> CreatedSoftLandscapePtr = LandscapeProxy;
		UHoudiniLandscapePtr* LandscapePtr = NewObject<UHoudiniLandscapePtr>(InOutput);
		LandscapePtr->SetSoftPtr(CreatedSoftLandscapePtr);
		LandscapePtr->SetIsWorldCompositionLandscape(bWorldComposition);

		// Add the new landscape to the output object
		FHoudiniOutputObject& OutputObj = InOutput->GetOutputObjects().Add(Identifier);
		OutputObj.OutputObject = LandscapePtr;
	}
	else
	{
		// For GeoImporter, simply had the landscapeproxy? landscape package? world?
		UPackage* LandscapePackage = LandscapeProxy->GetOutermost();
		FHoudiniOutputObject& OutputObj = InOutput->GetOutputObjects().Add(Identifier);
		OutputObj.OutputObject = LandscapePackage;
	}

	return true;
}


bool
FHoudiniLandscapeTranslator::ConvertHeightfieldDataToLandscapeData(
	const TArray< float >& HeightfieldFloatValues,
	const FHoudiniVolumeInfo& HeightfieldVolumeInfo,
	const int32& FinalXSize, const int32& FinalYSize,
	float FloatMin, float FloatMax,
	TArray< uint16 >& IntHeightData,
	FTransform& LandscapeTransform,
	const bool& NoResize)
{
	IntHeightData.Empty();
	LandscapeTransform.SetIdentity();

	// HF sizes needs an X/Y swap
	// NOPE.. not anymore
	int32 HoudiniXSize = HeightfieldVolumeInfo.YLength;
	int32 HoudiniYSize = HeightfieldVolumeInfo.XLength;
	int32 SizeInPoints = HoudiniXSize * HoudiniYSize;
	if ((HoudiniXSize < 2) || (HoudiniYSize < 2))
		return false;

	// Test for potential special cases...
	// Just print a warning for now
	if (HeightfieldVolumeInfo.MinX != 0)
		HOUDINI_LOG_WARNING(TEXT("Converting Landscape: heightfield's min X is not zero."));

	if (HeightfieldVolumeInfo.MinY != 0)
		HOUDINI_LOG_WARNING(TEXT("Converting Landscape: heightfield's min Y is not zero."));

	//--------------------------------------------------------------------------------------------------
	// 1. Convert values to uint16 using doubles to get the maximum precision during the conversion
	//--------------------------------------------------------------------------------------------------

	FTransform CurrentVolumeTransform = HeightfieldVolumeInfo.Transform;

	// The ZRange in Houdini (in m)
	double MeterZRange = (double)(FloatMax - FloatMin);

	// The corresponding unreal digit range (as unreal uses uint16, max is 65535)
	// We may want to not use the full range in order to be able to sculpt the landscape past the min/max values after.
	const double dUINT16_MAX = (double)UINT16_MAX;
	double DigitZRange = 49152.0;
	const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault< UHoudiniRuntimeSettings >();
	if (HoudiniRuntimeSettings && HoudiniRuntimeSettings->MarshallingLandscapesUseFullResolution)
		DigitZRange = dUINT16_MAX - 1.0;
	
	// If we  are not using the full range, we need to center the digit values so the terrain can be edited up and down
	double DigitCenterOffset = FMath::FloorToDouble((dUINT16_MAX - DigitZRange) / 2.0);

	// The factor used to convert from Houdini's ZRange to the desired digit range
	double ZSpacing = (MeterZRange != 0.0) ? (DigitZRange / MeterZRange) : 0.0;

	// Changes these values if the user wants to loose a lot of precision
	// just to keep the same transform as the landscape input
	bool bUseDefaultUE4Scaling = false;
	if (HoudiniRuntimeSettings && HoudiniRuntimeSettings->MarshallingLandscapesUseDefaultUnrealScaling)
		bUseDefaultUE4Scaling = HoudiniRuntimeSettings->MarshallingLandscapesUseDefaultUnrealScaling;

	if (bUseDefaultUE4Scaling)
	{
		//Check that our values are compatible with UE4's default scale values
		if (FloatMin < -256.0f || FloatMin > 256.0f || FloatMax < -256.0f || FloatMax > 256.0f)
		{
			// Warn the user that the landscape conversion will have issues 
			// invite him to change that setting
			HOUDINI_LOG_WARNING(
				TEXT("The heightfield's min and max height values are too large for being used with the \"Use Default UE4 scaling\" option.\n \
                      The generated Heightfield will likely be incorrectly converted to landscape unless you disable that option in the project settings and recook the asset."));
		}

		DigitZRange = dUINT16_MAX - 1.0;
		DigitCenterOffset = 0;

		// Default unreal landscape scaling is -256m:256m at Scale = 100
		// We need to apply the scale back to
		FloatMin = -256.0f * CurrentVolumeTransform.GetScale3D().Z * 2.0f;
		FloatMax = 256.0f * CurrentVolumeTransform.GetScale3D().Z * 2.0f;
		MeterZRange = (double)(FloatMax - FloatMin);

		ZSpacing = ((double)DigitZRange) / MeterZRange;
	}

	// Converting the data from Houdini to Unreal
	// For correct orientation in unreal, the point matrix has to be transposed.
	IntHeightData.SetNumUninitialized(SizeInPoints);

	int32 nUnreal = 0;
	for (int32 nY = 0; nY < HoudiniYSize; nY++)
	{
		for (int32 nX = 0; nX < HoudiniXSize; nX++)
		{
			// Copying values X then Y in Unreal but reading them Y then X in Houdini due to swapped X/Y
			int32 nHoudini = nY + nX * HoudiniYSize;

			// Get the double values in [0 - ZRange]
			double DoubleValue = (double)HeightfieldFloatValues[nHoudini] - (double)FloatMin;

			// Then convert it to [0 - DesiredRange] and center it 
			DoubleValue = DoubleValue * ZSpacing + DigitCenterOffset;
			IntHeightData[nUnreal++] = FMath::RoundToInt(DoubleValue);
		}
	}

	//--------------------------------------------------------------------------------------------------
	// 2. Resample / Pad the int data so that if fits unreal size requirements
	//--------------------------------------------------------------------------------------------------

	// UE has specific size requirements for landscape,
	// so we might need to pad/resample the heightfield data
	FVector LandscapeResizeFactor = FVector::OneVector;
	FVector LandscapePositionOffsetInPixels = FVector::ZeroVector;
	if (!NoResize)
	{
		// Try to resize the data
		if (!FHoudiniLandscapeTranslator::ResizeHeightDataForLandscape(
			IntHeightData,
			HoudiniXSize, HoudiniYSize, FinalXSize, FinalYSize,
			LandscapeResizeFactor, LandscapePositionOffsetInPixels))
			return false;
	}

	//--------------------------------------------------------------------------------------------------
	// 3. Calculating the proper transform for the landscape to be sized and positionned properly
	//--------------------------------------------------------------------------------------------------

	// Scale:
	// Calculating the equivalent scale to match Houdini's Terrain Size in Unreal
	FVector LandscapeScale;

	// Unreal has a X/Y resolution of 1m per point while Houdini is dependant on the heighfield's grid spacing
	LandscapeScale.X = CurrentVolumeTransform.GetScale3D().X * 2.0f;
	LandscapeScale.Y = CurrentVolumeTransform.GetScale3D().Y * 2.0f;

	// Calculating the Z Scale so that the Z values in Unreal are the same as in Houdini
	// Unreal has a default Z range is 512m for a scale of a 100%
	LandscapeScale.Z = (float)((double)(dUINT16_MAX / DigitZRange) * MeterZRange / 512.0);
	if (bUseDefaultUE4Scaling)
		LandscapeScale.Z = CurrentVolumeTransform.GetScale3D().Z * 2.0f;
	LandscapeScale *= 100.f;

	// If the data was resized and not expanded, we need to modify the landscape's scale
	LandscapeScale *= LandscapeResizeFactor;

	// Don't allow a zero scale, as this results in divide by 0 operations in FMatrix::InverseFast in the landscape component.
	if (FMath::IsNearlyZero(LandscapeScale.Z))
		LandscapeScale.Z = 1.0f;

	// We'll use the position from Houdini, but we will need to offset the Z Position to center the 
	// values properly as the data has been offset by the conversion to uint16
	FVector LandscapePosition = CurrentVolumeTransform.GetLocation();
	//LandscapePosition.Z = 0.0f;

	// We need to calculate the position offset so that Houdini and Unreal have the same Zero position
	// In Unreal, zero has a height value of 32768.
	// These values are then divided by 128 internally, and then multiplied by the Landscape's Z scale
	// ( DIGIT - 32768 ) / 128 * ZScale = ZOffset

	// We need the Digit (Unreal) value of Houdini's zero for the scale calculation
	// ( float and int32 are used for this because 0 might be out of the landscape Z range!
	// when using the full range, this would cause an overflow for a uint16!! )
	float HoudiniZeroValueInDigit = (float)FMath::RoundToInt((0.0 - (double)FloatMin) * ZSpacing + DigitCenterOffset);
	float ZOffset = -(HoudiniZeroValueInDigit - 32768.0f) / 128.0f * LandscapeScale.Z;

	LandscapePosition.Z += ZOffset;

	// If we have padded the data when resizing the landscape, we need to offset the position because of
	// the added values on the topLeft Corner of the Landscape
	if (LandscapePositionOffsetInPixels != FVector::ZeroVector)
	{
		FVector LandscapeOffset = LandscapePositionOffsetInPixels * LandscapeScale;
		LandscapeOffset.Z = 0.0f;

		LandscapePosition += LandscapeOffset;
	}

	// Landscape rotation
	//FRotator LandscapeRotation( 0.0, -90.0, 0.0 );
	//Landscape->SetActorRelativeRotation( LandscapeRotation );

	// We can now set the Landscape position
	LandscapeTransform.SetLocation(LandscapePosition);
	LandscapeTransform.SetScale3D(LandscapeScale);

	return true;
}

template<typename T>
TArray<T> ResampleData(const TArray<T>& Data, int32 OldWidth, int32 OldHeight, int32 NewWidth, int32 NewHeight)
{
	TArray<T> Result;
	Result.Empty(NewWidth * NewHeight);
	Result.AddUninitialized(NewWidth * NewHeight);

	const float XScale = (float)(OldWidth - 1) / (NewWidth - 1);
	const float YScale = (float)(OldHeight - 1) / (NewHeight - 1);
	for (int32 Y = 0; Y < NewHeight; ++Y)
	{
		for (int32 X = 0; X < NewWidth; ++X)
		{
			const float OldY = Y * YScale;
			const float OldX = X * XScale;
			const int32 X0 = FMath::FloorToInt(OldX);
			const int32 X1 = FMath::Min(FMath::FloorToInt(OldX) + 1, OldWidth - 1);
			const int32 Y0 = FMath::FloorToInt(OldY);
			const int32 Y1 = FMath::Min(FMath::FloorToInt(OldY) + 1, OldHeight - 1);
			const T& Original00 = Data[Y0 * OldWidth + X0];
			const T& Original10 = Data[Y0 * OldWidth + X1];
			const T& Original01 = Data[Y1 * OldWidth + X0];
			const T& Original11 = Data[Y1 * OldWidth + X1];
			Result[Y * NewWidth + X] = FMath::BiLerp(Original00, Original10, Original01, Original11, FMath::Fractional(OldX), FMath::Fractional(OldY));
		}
	}

	return Result;
}

template<typename T>
void ExpandData(T* OutData, const T* InData,
	int32 OldMinX, int32 OldMinY, int32 OldMaxX, int32 OldMaxY,
	int32 NewMinX, int32 NewMinY, int32 NewMaxX, int32 NewMaxY)
{
	const int32 OldWidth = OldMaxX - OldMinX + 1;
	const int32 OldHeight = OldMaxY - OldMinY + 1;
	const int32 NewWidth = NewMaxX - NewMinX + 1;
	const int32 NewHeight = NewMaxY - NewMinY + 1;
	const int32 OffsetX = NewMinX - OldMinX;
	const int32 OffsetY = NewMinY - OldMinY;

	for (int32 Y = 0; Y < NewHeight; ++Y)
	{
		const int32 OldY = FMath::Clamp<int32>(Y + OffsetY, 0, OldHeight - 1);

		// Pad anything to the left
		const T PadLeft = InData[OldY * OldWidth + 0];
		for (int32 X = 0; X < -OffsetX; ++X)
		{
			OutData[Y * NewWidth + X] = PadLeft;
		}

		// Copy one row of the old data
		{
			const int32 X = FMath::Max(0, -OffsetX);
			const int32 OldX = FMath::Clamp<int32>(X + OffsetX, 0, OldWidth - 1);
			FMemory::Memcpy(&OutData[Y * NewWidth + X], &InData[OldY * OldWidth + OldX], FMath::Min<int32>(OldWidth, NewWidth) * sizeof(T));
		}

		const T PadRight = InData[OldY * OldWidth + OldWidth - 1];
		for (int32 X = -OffsetX + OldWidth; X < NewWidth; ++X)
		{
			OutData[Y * NewWidth + X] = PadRight;
		}
	}
}

template<typename T>
TArray<T> ExpandData(const TArray<T>& Data,
	int32 OldMinX, int32 OldMinY, int32 OldMaxX, int32 OldMaxY,
	int32 NewMinX, int32 NewMinY, int32 NewMaxX, int32 NewMaxY,
	int32* PadOffsetX = nullptr, int32* PadOffsetY = nullptr)
{
	const int32 NewWidth = NewMaxX - NewMinX + 1;
	const int32 NewHeight = NewMaxY - NewMinY + 1;

	TArray<T> Result;
	Result.Empty(NewWidth * NewHeight);
	Result.AddUninitialized(NewWidth * NewHeight);

	ExpandData(Result.GetData(), Data.GetData(),
		OldMinX, OldMinY, OldMaxX, OldMaxY,
		NewMinX, NewMinY, NewMaxX, NewMaxY);

	// Return the padding so we can offset the terrain position after
	if (PadOffsetX)
		*PadOffsetX = NewMinX;

	if (PadOffsetY)
		*PadOffsetY = NewMinY;

	return Result;
}

bool
FHoudiniLandscapeTranslator::ResizeHeightDataForLandscape(
	TArray<uint16>& HeightData,
	const int32& SizeX, const int32& SizeY,
	const int32& NewSizeX, const int32& NewSizeY,
	FVector& LandscapeResizeFactor,
	FVector& LandscapePositionOffset)
{
	LandscapeResizeFactor = FVector::OneVector;
	LandscapePositionOffset = FVector::ZeroVector;

	if (HeightData.Num() <= 4)
		return false;

	if ((SizeX < 2) || (SizeY < 2))
		return false;

	// No need to resize anything
	if (SizeX == NewSizeX && SizeY == NewSizeY)
		return true;

	// Do we need to resize/expand the data to the new size?
	bool bForceResample = false;
	bool bResample = bForceResample ? true : ((NewSizeX <= SizeX) && (NewSizeY <= SizeY));

	TArray<uint16> NewData;
	if (!bResample)
	{
		// Expanding the data by padding
		NewData.SetNumUninitialized(NewSizeX * NewSizeY);

		const int32 OffsetX = (int32)(NewSizeX - SizeX) / 2;
		const int32 OffsetY = (int32)(NewSizeY - SizeY) / 2;

		// Store the offset in pixel due to the padding
		int32 PadOffsetX = 0;
		int32 PadOffsetY = 0;

		// Expanding the Data
		NewData = ExpandData(
			HeightData, 0, 0, SizeX - 1, SizeY - 1,
			-OffsetX, -OffsetY, NewSizeX - OffsetX - 1, NewSizeY - OffsetY - 1,
			&PadOffsetX, &PadOffsetY);

		// We will need to offset the landscape position due to the value added by the padding
		LandscapePositionOffset.X = (float)PadOffsetX;
		LandscapePositionOffset.Y = (float)PadOffsetY;

		// Notify the user that the data was padded
		HOUDINI_LOG_WARNING(
			TEXT("Landscape data was padded from ( %d x %d ) to ( %d x %d )."),
			SizeX, SizeY, NewSizeX, NewSizeY);
	}
	else
	{
		// Resampling the data
		NewData.SetNumUninitialized(NewSizeX * NewSizeY);
		NewData = ResampleData(HeightData, SizeX, SizeY, NewSizeX, NewSizeY);

		// The landscape has been resized, we'll need to take that into account when sizing it
		LandscapeResizeFactor.X = (float)SizeX / (float)NewSizeX;
		LandscapeResizeFactor.Y = (float)SizeY / (float)NewSizeY;
		LandscapeResizeFactor.Z = 1.0f;

		// Notify the user if the heightfield data was resized
		HOUDINI_LOG_WARNING(
			TEXT("Landscape data was resized from ( %d x %d ) to ( %d x %d )."),
			SizeX, SizeY, NewSizeX, NewSizeY);
	}

	// Replaces Old data with the new one
	HeightData = NewData;

	return true;
}


bool 
FHoudiniLandscapeTranslator::CalcLandscapeSizeFromHeightfieldSize(
	const int32& HoudiniSizeX, const int32& HoudiniSizeY, 
	int32& UnrealSizeX, int32& UnrealSizeY, 
	int32& NumSectionsPerComponent, int32& NumQuadsPerSection)
{
	if ((HoudiniSizeX < 2) || (HoudiniSizeY < 2))
		return false;

	NumSectionsPerComponent = 1;
	NumQuadsPerSection = 1;
	UnrealSizeX = -1;
	UnrealSizeY = -1;

	// Unreal's default sizes
	int32 SectionSizes[] = { 7, 15, 31, 63, 127, 255 };
	int32 NumSections[] = { 1, 2 };

	// Component count used to calculate the final size of the landscape
	int32 ComponentsCountX = 1;
	int32 ComponentsCountY = 1;

	// Lambda for clamping the number of component in X/Y
	auto ClampLandscapeSize = [&]()
	{
		// Max size is either whole components below 8192 verts, or 32 components
		ComponentsCountX = FMath::Clamp(ComponentsCountX, 1, FMath::Min(32, FMath::FloorToInt(8191 / (NumSectionsPerComponent * NumQuadsPerSection))));
		ComponentsCountY = FMath::Clamp(ComponentsCountY, 1, FMath::Min(32, FMath::FloorToInt(8191 / (NumSectionsPerComponent * NumQuadsPerSection))));
	};

	// Try to find a section size and number of sections that exactly matches the dimensions of the heightfield
	bool bFoundMatch = false;
	for (int32 SectionSizesIdx = ARRAY_COUNT(SectionSizes) - 1; SectionSizesIdx >= 0; SectionSizesIdx--)
	{
		for (int32 NumSectionsIdx = ARRAY_COUNT(NumSections) - 1; NumSectionsIdx >= 0; NumSectionsIdx--)
		{
			int32 ss = SectionSizes[SectionSizesIdx];
			int32 ns = NumSections[NumSectionsIdx];

			if (((HoudiniSizeX - 1) % (ss * ns)) == 0 && ((HoudiniSizeX - 1) / (ss * ns)) <= 32 &&
				((HoudiniSizeY - 1) % (ss * ns)) == 0 && ((HoudiniSizeY - 1) / (ss * ns)) <= 32)
			{
				bFoundMatch = true;
				NumQuadsPerSection = ss;
				NumSectionsPerComponent = ns;
				ComponentsCountX = (HoudiniSizeX - 1) / (ss * ns);
				ComponentsCountY = (HoudiniSizeY - 1) / (ss * ns);
				ClampLandscapeSize();
				break;
			}
		}
		if (bFoundMatch)
		{
			break;
		}
	}

	if (!bFoundMatch)
	{
		// if there was no exact match, try increasing the section size until we encompass the whole heightmap
		const int32 CurrentSectionSize = NumQuadsPerSection;
		const int32 CurrentNumSections = NumSectionsPerComponent;
		for (int32 SectionSizesIdx = 0; SectionSizesIdx < ARRAY_COUNT(SectionSizes); SectionSizesIdx++)
		{
			if (SectionSizes[SectionSizesIdx] < CurrentSectionSize)
			{
				continue;
			}

			const int32 ComponentsX = FMath::DivideAndRoundUp((HoudiniSizeX - 1), SectionSizes[SectionSizesIdx] * CurrentNumSections);
			const int32 ComponentsY = FMath::DivideAndRoundUp((HoudiniSizeY - 1), SectionSizes[SectionSizesIdx] * CurrentNumSections);
			if (ComponentsX <= 32 && ComponentsY <= 32)
			{
				bFoundMatch = true;
				NumQuadsPerSection = SectionSizes[SectionSizesIdx];
				ComponentsCountX = ComponentsX;
				ComponentsCountY = ComponentsY;
				ClampLandscapeSize();
				break;
			}
		}
	}

	if (!bFoundMatch)
	{
		// if the heightmap is very large, fall back to using the largest values we support
		const int32 MaxSectionSize = SectionSizes[ARRAY_COUNT(SectionSizes) - 1];
		const int32 MaxNumSubSections = NumSections[ARRAY_COUNT(NumSections) - 1];
		const int32 ComponentsX = FMath::DivideAndRoundUp((HoudiniSizeX - 1), MaxSectionSize * MaxNumSubSections);
		const int32 ComponentsY = FMath::DivideAndRoundUp((HoudiniSizeY - 1), MaxSectionSize * MaxNumSubSections);

		bFoundMatch = true;
		NumQuadsPerSection = MaxSectionSize;
		NumSectionsPerComponent = MaxNumSubSections;
		ComponentsCountX = ComponentsX;
		ComponentsCountY = ComponentsY;
		ClampLandscapeSize();
	}

	if (!bFoundMatch)
	{
		// Using default size just to not crash..
		UnrealSizeX = 512;
		UnrealSizeY = 512;
		NumSectionsPerComponent = 1;
		NumQuadsPerSection = 511;
		ComponentsCountX = 1;
		ComponentsCountY = 1;
	}
	else
	{
		// Calculating the desired size
		int32 QuadsPerComponent = NumSectionsPerComponent * NumQuadsPerSection;

		UnrealSizeX = ComponentsCountX * QuadsPerComponent + 1;
		UnrealSizeY = ComponentsCountY * QuadsPerComponent + 1;
	}

	return bFoundMatch;
}

const FHoudiniGeoPartObject*
FHoudiniLandscapeTranslator::GetHoudiniHeightFieldFromOutput(UHoudiniOutput* InOutput) 
{
	if (!InOutput || InOutput->IsPendingKill())
		return nullptr;

	if (InOutput->GetHoudiniGeoPartObjects().Num() == 0)
		return nullptr;

	const FHoudiniGeoPartObject & HGPO = InOutput->GetHoudiniGeoPartObjects()[0];

	if (HGPO.Type != EHoudiniPartType::Volume)
		return nullptr;

	FHoudiniVolumeInfo CurVolumeInfo = HGPO.VolumeInfo;
		
	if (!CurVolumeInfo.Name.Contains("height"))
		return nullptr;

	// We're only handling single values for now
	if (CurVolumeInfo.TupleSize != 1)
		return nullptr;

	// Terrains always have a ZSize of 1.
	if (CurVolumeInfo.ZLength != 1)
		return nullptr;

	// Values should be float
	if (!CurVolumeInfo.bIsFloat)
		return nullptr;

	return &HGPO;

}

void 
FHoudiniLandscapeTranslator::GetHeightfieldsLayersFromOutput(const UHoudiniOutput* InOutput, const FHoudiniGeoPartObject& Heightfield, TArray< const FHoudiniGeoPartObject* >& FoundLayers) 
{
	FoundLayers.Empty();

	// Get node id
	HAPI_NodeId HeightFieldNodeId = Heightfield.GeoId;

	// We need the tile attribute if the height has it
	bool bParentHeightfieldHasTile = false;
	int32 HeightFieldTile = -1;
	{
		HAPI_AttributeInfo AttribInfoTile;
		FHoudiniApi::AttributeInfo_Init(&AttribInfoTile);
		TArray< int32 > TileValues;

		FHoudiniEngineUtils::HapiGetAttributeDataAsInteger(
			HeightFieldNodeId, Heightfield.PartId, "tile", AttribInfoTile, TileValues);

		if (AttribInfoTile.exists && AttribInfoTile.owner == HAPI_ATTROWNER_PRIM && TileValues.Num() > 0)
		{
			HeightFieldTile = TileValues[0];
			bParentHeightfieldHasTile = true;
		}
	}

	for (TArray< FHoudiniGeoPartObject >::TConstIterator IterLayers(InOutput->GetHoudiniGeoPartObjects()); IterLayers; ++IterLayers)
	{
		const FHoudiniGeoPartObject & HoudiniGeoPartObject = *IterLayers;

		HAPI_NodeId NodeId = HoudiniGeoPartObject.GeoId;
		if (NodeId == -1 || NodeId != HeightFieldNodeId)
			continue;

		if (bParentHeightfieldHasTile) 
		{
			int32 CurrentTile = -1;

			HAPI_AttributeInfo AttribInfoTile;
			FHoudiniApi::AttributeInfo_Init(&AttribInfoTile);
			TArray<int32> TileValues;

			FHoudiniEngineUtils::HapiGetAttributeDataAsInteger(
				HoudiniGeoPartObject.GeoId, HoudiniGeoPartObject.PartId, "tile", AttribInfoTile, TileValues);


			if (AttribInfoTile.exists && AttribInfoTile.owner == HAPI_ATTROWNER_PRIM && TileValues.Num() > 0)
			{
				CurrentTile = TileValues[0];
			}

			// Does this layer come from the same tile as the height?
			if ((CurrentTile != HeightFieldTile) || (CurrentTile == -1))
				continue;
		}

		// Retrieve the VolumeInfo
		HAPI_VolumeInfo CurrentVolumeInfo;
		FHoudiniApi::VolumeInfo_Init(&CurrentVolumeInfo);
		if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetVolumeInfo(
			FHoudiniEngine::Get().GetSession(),
			NodeId, HoudiniGeoPartObject.PartId,
			&CurrentVolumeInfo))
			continue;

		// We're interesting in anything but height data
		FString CurrentVolumeName;
		FHoudiniEngineString(CurrentVolumeInfo.nameSH).ToFString(CurrentVolumeName);
		if (CurrentVolumeName.Contains("height"))
			continue;

		// We're only handling single values for now
		if (CurrentVolumeInfo.tupleSize != 1)
			continue;

		// Terrains always have a ZSize of 1.
		if (CurrentVolumeInfo.zLength != 1)
			continue;

		// Values should be float
		if (CurrentVolumeInfo.storage != HAPI_STORAGETYPE_FLOAT)
			continue;

		FoundLayers.Add(&HoudiniGeoPartObject);
	}
}

bool 
FHoudiniLandscapeTranslator::GetHoudiniHeightfieldFloatData(const FHoudiniGeoPartObject* HGPO, TArray<float> &OutFloatArr, float &OutFloatMin, float &OutFloatMax) 
{
	OutFloatArr.Empty();
	OutFloatMin = 0.f;
	OutFloatMax = 0.f;

	if (HGPO->Type != EHoudiniPartType::Volume)
		return false;

	FHoudiniVolumeInfo VolumeInfo = HGPO->VolumeInfo;

	// We're only handling single values for now
	if (VolumeInfo.TupleSize != 1)
		return false;

	// Terrains always have a ZSize of 1.
	if (VolumeInfo.ZLength != 1)
		return false;

	// Values must be float
	if (!VolumeInfo.bIsFloat)
		return false;

	if ((VolumeInfo.XLength < 2) || (VolumeInfo.YLength < 2))
		return false;
	
	int32 SizeInPoints = VolumeInfo.XLength *  VolumeInfo.YLength;

	OutFloatArr.SetNumUninitialized(SizeInPoints);
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetHeightFieldData(
		FHoudiniEngine::Get().GetSession(),
		HGPO->GeoId, HGPO->PartId,
		OutFloatArr.GetData(),
		0, SizeInPoints), false);
	
	OutFloatMin = OutFloatArr[0];
	OutFloatMax = OutFloatMin;

	for (float NextFloatVal : OutFloatArr) 
	{
		if (NextFloatVal > OutFloatMax)
		{
			OutFloatMax = NextFloatVal;
		}
		else if (NextFloatVal < OutFloatMin)
			OutFloatMin = NextFloatVal;
	}

	return true;
}

bool
FHoudiniLandscapeTranslator::GetNonWeightBlendedLayerNames(const FHoudiniGeoPartObject& HeightfieldGeoPartObject, TArray<FString>& NonWeightBlendedLayerNames)
{
	// See if we can find the NonWeightBlendedLayer prim attribute on the heightfield
	HAPI_NodeId HeightfieldNodeId = HeightfieldGeoPartObject.GeoId;

	HAPI_PartInfo PartInfo;
	FHoudiniApi::PartInfo_Init(&PartInfo);
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetPartInfo(
		FHoudiniEngine::Get().GetSession(), 
		HeightfieldGeoPartObject.GeoId,
		HeightfieldGeoPartObject.PartId, &PartInfo), false);
		
	HAPI_PartId PartId = HeightfieldGeoPartObject.PartId;

	// Get All attribute names for that part
	int32 nAttribCount = PartInfo.attributeCounts[HAPI_ATTROWNER_PRIM];

	TArray<HAPI_StringHandle> AttribNameSHArray;
	AttribNameSHArray.SetNum(nAttribCount);

	if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetAttributeNames(
		FHoudiniEngine::Get().GetSession(),
		HeightfieldNodeId, PartInfo.id, HAPI_ATTROWNER_PRIM,
		AttribNameSHArray.GetData(), nAttribCount))
		return false;

	// Looking for all the attributes that starts with unreal_landscape_layer_nonweightblended
	for (int32 Idx = 0; Idx < AttribNameSHArray.Num(); ++Idx)
	{
		FString HapiString = TEXT("");
		FHoudiniEngineString HoudiniEngineString(AttribNameSHArray[Idx]);
		HoudiniEngineString.ToFString(HapiString);

		if (!HapiString.StartsWith("unreal_landscape_layer_nonweightblended", ESearchCase::IgnoreCase))
			continue;

		// Get the Attribute Info
		HAPI_AttributeInfo AttribInfo;
		FHoudiniApi::AttributeInfo_Init(&AttribInfo);
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetAttributeInfo(
			FHoudiniEngine::Get().GetSession(),
			HeightfieldNodeId, PartId, TCHAR_TO_UTF8(*HapiString),
			HAPI_ATTROWNER_PRIM, &AttribInfo), false);

		if (AttribInfo.storage != HAPI_STORAGETYPE_STRING)
			break;

		// Initialize a string handle array
		TArray< HAPI_StringHandle > HapiSHArray;
		HapiSHArray.SetNumZeroed(AttribInfo.count * AttribInfo.tupleSize);

		// Get the string handle(s)
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetAttributeStringData(
			FHoudiniEngine::Get().GetSession(),
			HeightfieldNodeId, PartId, TCHAR_TO_UTF8(*HapiString), &AttribInfo,
			HapiSHArray.GetData(), 0, AttribInfo.count), false);

		// Convert them to FString
		for (int32 IdxSH = 0; IdxSH < HapiSHArray.Num(); IdxSH++)
		{
			FString CurrentString;
			FHoudiniEngineString HEngineString(HapiSHArray[IdxSH]);
			HEngineString.ToFString(CurrentString);

			TArray<FString> Tokens;
			CurrentString.ParseIntoArray(Tokens, TEXT(" "), true);

			for (int32 n = 0; n < Tokens.Num(); n++)
				NonWeightBlendedLayerNames.Add(Tokens[n]);
		}

		// We found the attribute, exit
		break;
	}



	return true;
}

bool
FHoudiniLandscapeTranslator::IsUnitLandscapeLayer(const FHoudiniGeoPartObject& LayerGeoPartObject)
{
	// Check the attribute exists on primitive or detail
	HAPI_AttributeOwner Owner = HAPI_ATTROWNER_INVALID;
	if (FHoudiniEngineUtils::HapiCheckAttributeExists(LayerGeoPartObject.GeoId, LayerGeoPartObject.PartId, "unreal_unit_landscape_layer", HAPI_ATTROWNER_PRIM))
		Owner = HAPI_ATTROWNER_PRIM;
	else if (FHoudiniEngineUtils::HapiCheckAttributeExists(LayerGeoPartObject.GeoId, LayerGeoPartObject.PartId, "unreal_unit_landscape_layer", HAPI_ATTROWNER_DETAIL))
		Owner = HAPI_ATTROWNER_DETAIL;
	else
		return false;

	// Check the value
	HAPI_AttributeInfo AttribInfoUnitLayer;
	FHoudiniApi::AttributeInfo_Init(&AttribInfoUnitLayer);
	TArray< int32 > AttribValues;
	FHoudiniEngineUtils::HapiGetAttributeDataAsInteger(
		LayerGeoPartObject.GeoId, LayerGeoPartObject.PartId, "unreal_unit_landscape_layer", AttribInfoUnitLayer, AttribValues, 1, Owner);

	if (AttribValues.Num() > 0 && AttribValues[0] == 1)
		return true;

	return false;
}

bool
FHoudiniLandscapeTranslator::CreateOrUpdateLandscapeLayers(
	const TArray<const FHoudiniGeoPartObject*>& FoundLayers,
	const FHoudiniGeoPartObject& Heightfield,
	const int32& LandscapeXSize, const int32& LandscapeYSize,
	TMap<FString, float>& GlobalMinimums,
	TMap<FString, float>& GlobalMaximums,
	TArray<FLandscapeImportLayerInfo>& OutLayerInfos,
	bool bIsUpdate,
	const FHoudiniPackageParams& InPackageParams)
{
	OutLayerInfos.Empty();

	// Get the names of all non weight blended layers
	TArray<FString> NonWeightBlendedLayerNames;
	FHoudiniLandscapeTranslator::GetNonWeightBlendedLayerNames(Heightfield, NonWeightBlendedLayerNames);

	FHoudiniPackageParams LayerPackageParams = InPackageParams;

	// Try to create all the layers
	TArray<UPackage*> CreatedLandscapeLayerPackage;
	ELandscapeImportAlphamapType ImportLayerType = ELandscapeImportAlphamapType::Additive;
	for (TArray<const FHoudiniGeoPartObject *>::TConstIterator IterLayers(FoundLayers); IterLayers; ++IterLayers)
	{
		const FHoudiniGeoPartObject * LayerGeoPartObject = *IterLayers;
		if (!LayerGeoPartObject)
			continue;

		if (!LayerGeoPartObject->IsValid())
			continue;

		if (!FHoudiniEngineUtils::IsHoudiniNodeValid(LayerGeoPartObject->AssetId))
			continue;

		if (bIsUpdate && !LayerGeoPartObject->bHasGeoChanged)
			continue;

		TArray< float > FloatLayerData;
		float LayerMin = 0;
		float LayerMax = 0;
		if (!FHoudiniLandscapeTranslator::GetHoudiniHeightfieldFloatData(LayerGeoPartObject, FloatLayerData, LayerMin, LayerMax))
			continue;

		// No need to create flat layers as Unreal will remove them afterwards..
		if (LayerMin == LayerMax)
			continue;

		const FHoudiniVolumeInfo& LayerVolumeInfo = LayerGeoPartObject->VolumeInfo;

		// Get the layer's name
		FString LayerString = LayerVolumeInfo.Name;

		// Check if that landscape layer has been marked as unit (range in [0-1]
		if (IsUnitLandscapeLayer(*LayerGeoPartObject))
		{
			LayerMin = 0.0f;
			LayerMax = 1.0f;
		}
		else
		{
			// We want to convert the layer using the global Min/Max
			if (GlobalMaximums.Contains(LayerString))
				LayerMax = GlobalMaximums[LayerString];

			if (GlobalMinimums.Contains(LayerString))
				LayerMin = GlobalMinimums[LayerString];
		}

		// Creating the ImportLayerInfo and LayerInfo objects
		ObjectTools::SanitizeObjectName(LayerString);
		FName LayerName(*LayerString);
		FLandscapeImportLayerInfo currentLayerInfo(LayerName);
		
		// Get the layer package path
		FString LayerNameString = FString::Printf(TEXT("%s_%d"), LayerString.GetCharArray().GetData(), (int32)LayerGeoPartObject->PartId);
		LayerNameString = UPackageTools::SanitizePackageName(LayerNameString);

		// Build an object name for the current layer
		LayerPackageParams.SplitStr = LayerNameString;

		UPackage * Package = nullptr;
		currentLayerInfo.LayerInfo = CreateLandscapeLayerInfoObject(LayerString, LayerPackageParams.GetPackagePath(), LayerPackageParams.GetPackageName(), Package);
		if (!currentLayerInfo.LayerInfo || !Package)
			continue;
		
		// Convert the float data to uint8
		// HF masks need their X/Y sizes swapped
		if (!FHoudiniLandscapeTranslator::ConvertHeightfieldLayerToLandscapeLayer(
			FloatLayerData, LayerVolumeInfo.YLength, LayerVolumeInfo.XLength,
			LayerMin, LayerMax,
			LandscapeXSize, LandscapeYSize,
			currentLayerInfo.LayerData))
			continue;
		
		// We will store the data used to convert from Houdini values to int in the DebugColor
		// This is the only way we'll be able to reconvert those values back to their houdini equivalent afterwards...
		// R = Min, G = Max, B = Spacing, A = ?
		currentLayerInfo.LayerInfo->LayerUsageDebugColor.R = LayerMin;
		currentLayerInfo.LayerInfo->LayerUsageDebugColor.G = LayerMax;
		currentLayerInfo.LayerInfo->LayerUsageDebugColor.B = (LayerMax - LayerMin) / 255.0f;
		currentLayerInfo.LayerInfo->LayerUsageDebugColor.A = PI;

		// Visibility are by default non weight blended
		if (NonWeightBlendedLayerNames.Contains(LayerString)
			|| LayerString.Equals(TEXT("visibility"), ESearchCase::IgnoreCase))
			currentLayerInfo.LayerInfo->bNoWeightBlend = true;
		else
			currentLayerInfo.LayerInfo->bNoWeightBlend = false;

		if (!bIsUpdate)
		{
			// Mark the package dirty...
			Package->MarkPackageDirty();
			CreatedLandscapeLayerPackage.Add(Package);
		}

		OutLayerInfos.Add(currentLayerInfo);
	}

	// Autosaving the layers prevents them for being deleted with the Asset
	// Save the packages created for the LayerInfos
	// Do this only for when creating layers.
	if (!bIsUpdate)
		FEditorFileUtils::PromptForCheckoutAndSave(CreatedLandscapeLayerPackage, true, false);

	return true;
}

void
FHoudiniLandscapeTranslator::CalcHeightfieldsArrayGlobalZMinZMax(
	const TArray< FHoudiniGeoPartObject > & InHeightfieldArray,
	TMap<FString, float>& GlobalMinimums,
	TMap<FString, float>& GlobalMaximums)
{
	GlobalMinimums.Empty();
	GlobalMaximums.Empty();

	for (const FHoudiniGeoPartObject& CurrentHeightfield: InHeightfieldArray)
	{
		// Get the current Heightfield GeoPartObject
		if ( CurrentHeightfield.VolumeInfo.TupleSize != 1)
			continue;

		// Retrieve node id from geo part.
		HAPI_NodeId NodeId = CurrentHeightfield.GeoId;
		if (NodeId == -1)
			continue;

		// Retrieve the VolumeInfo
		HAPI_VolumeInfo CurrentVolumeInfo;
		FHoudiniApi::VolumeInfo_Init(&CurrentVolumeInfo);
		if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetVolumeInfo(
			FHoudiniEngine::Get().GetSession(),
			NodeId, CurrentHeightfield.PartId,
			&CurrentVolumeInfo))
			continue;

		// Unreal's Z values are Y in Houdini
		float ymin, ymax;
		if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetVolumeBounds(FHoudiniEngine::Get().GetSession(),
			NodeId, CurrentHeightfield.PartId,
			nullptr, &ymin, nullptr,
			nullptr, &ymax, nullptr,
			nullptr, nullptr, nullptr))
			continue;

		// Retrieve the volume name.
		FString VolumeName;
		FHoudiniEngineString HoudiniEngineStringPartName(CurrentVolumeInfo.nameSH);
		HoudiniEngineStringPartName.ToFString(VolumeName);

		// Read the global min value for this volume
		if (!GlobalMinimums.Contains(VolumeName))
		{
			GlobalMinimums.Add(VolumeName, ymin);
		}
		else
		{
			// Update the min if necessary
			if (ymin < GlobalMinimums[VolumeName])
				GlobalMinimums[VolumeName] = ymin;
		}

		// Read the global max value for this volume
		if (!GlobalMaximums.Contains(VolumeName))
		{
			GlobalMaximums.Add(VolumeName, ymax);
		}
		else
		{
			// Update the max if necessary
			if (ymax > GlobalMaximums[VolumeName])
				GlobalMaximums[VolumeName] = ymax;
		}
	}
}

bool 
FHoudiniLandscapeTranslator::ConvertHeightfieldLayerToLandscapeLayer(
	const TArray<float>& FloatLayerData,
	const int32& HoudiniXSize, const int32& HoudiniYSize,
	const float& LayerMin, const float& LayerMax,
	const int32& LandscapeXSize, const int32& LandscapeYSize,
	TArray<uint8>& LayerData, const bool& NoResize)
{
	// Convert the float data to uint8
	LayerData.SetNumUninitialized(HoudiniXSize * HoudiniYSize);

	// Calculating the factor used to convert from Houdini's ZRange to [0 255]
	double LayerZRange = (LayerMax - LayerMin);
	double LayerZSpacing = (LayerZRange != 0.0) ? (255.0 / (double)(LayerZRange)) : 0.0;

	int32 nUnrealIndex = 0;
	for (int32 nY = 0; nY < HoudiniYSize; nY++)
	{
		for (int32 nX = 0; nX < HoudiniXSize; nX++)
		{
			// Copying values X then Y in Unreal but reading them Y then X in Houdini due to swapped X/Y
			int32 nHoudini = nY + nX * HoudiniYSize;

			// Get the double values in [0 - ZRange]
			double DoubleValue = (double)FMath::Clamp(FloatLayerData[nHoudini], LayerMin, LayerMax) - (double)LayerMin;

			// Then convert it to [0 - 255]
			DoubleValue *= LayerZSpacing;

			LayerData[nUnrealIndex++] = FMath::RoundToInt(DoubleValue);
		}
	}

	// Finally, resize the data to fit with the new landscape size if needed
	if (NoResize)
		return true;

	return FHoudiniLandscapeTranslator::ResizeLayerDataForLandscape(
		LayerData, HoudiniXSize, HoudiniYSize,
		LandscapeXSize, LandscapeYSize);
}

bool
FHoudiniLandscapeTranslator::ResizeLayerDataForLandscape(
	TArray< uint8 >& LayerData,
	const int32& SizeX, const int32& SizeY,
	const int32& NewSizeX, const int32& NewSizeY)
{
	if ((NewSizeX == SizeX) && (NewSizeY == SizeY))
		return true;

	bool bForceResample = false;
	bool bResample = bForceResample ? true : ((NewSizeX <= SizeX) && (NewSizeY <= SizeY));

	TArray<uint8> NewData;
	if (!bResample)
	{
		NewData.SetNumUninitialized(NewSizeX * NewSizeY);

		const int32 OffsetX = (int32)(NewSizeX - SizeX) / 2;
		const int32 OffsetY = (int32)(NewSizeY - SizeY) / 2;

		// Expanding the Data
		NewData = ExpandData(
			LayerData,
			0, 0, SizeX - 1, SizeY - 1,
			-OffsetX, -OffsetY, NewSizeX - OffsetX - 1, NewSizeY - OffsetY - 1);
	}
	else
	{
		// Resampling the data
		NewData.SetNumUninitialized(NewSizeX * NewSizeY);
		NewData = ResampleData(LayerData, SizeX, SizeY, NewSizeX, NewSizeY);
	}

	LayerData = NewData;

	return true;
}

ALandscapeProxy *
FHoudiniLandscapeTranslator::CreateLandscape(
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
	FHoudiniPackageParams InPackageParams)
{
	if ((XSize < 2) || (YSize < 2))
		return nullptr;

	if (IntHeightData.Num() != (XSize * YSize))
		return nullptr;

	if (!GEditor)
		return nullptr;

	ALandscapeProxy* LandscapeProxy = nullptr;
	UPackage *CreatedPackage = nullptr;
	
	//... Create landscape ...//
	if (bNeedCreateNewWorld)
	{
		// Create Package
		FString PackagePath = InPackageParams.GetPackagePath();
		FString PackageName = InPackageParams.GetPackageName();

		FString CreatedPackageName;
		CreatedPackage = InPackageParams.CreatePackageForObject(CreatedPackageName);

		if (!CreatedPackage)
			return nullptr;

		// Create a new world asset 
		UWorldFactory* Factory = NewObject<UWorldFactory>();		
		FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
		UObject* Asset = AssetToolsModule.Get().CreateAsset(
			PackageName, PackagePath,
			UWorld::StaticClass(), Factory, FName("ContentBrowserNewAsset"));
	
		UWorld* NewWorld = Cast<UWorld>(Asset);		
		if (!NewWorld)
			return nullptr;
		
		NewWorld->SetCurrentLevel(NewWorld->PersistentLevel);
		LandscapeProxy = NewWorld->SpawnActor<ALandscapeStreamingProxy>();
		
		if (LandscapeProxy)
			FAssetRegistryModule::AssetCreated(LandscapeProxy);
	}
	else 
	{
		if (!SpawnWorld)
			return nullptr;

		// We need to create the landscape now and assign it a new GUID so we can create the LayerInfos
		if (CreateLandscapeStreamingProxy)
			LandscapeProxy = SpawnWorld->SpawnActor<ALandscapeStreamingProxy>();
		else
			LandscapeProxy = SpawnWorld->SpawnActor<ALandscape>();
	}

	if (!LandscapeProxy)
		return nullptr;

	//LandscapeProxy->Rename(nullptr, HAC);

	// Create a new GUID
	FGuid currentGUID = FGuid::NewGuid();
	LandscapeProxy->SetLandscapeGuid(currentGUID);

	// Set the landscape Transform
	LandscapeProxy->SetActorTransform(LandscapeTransform);

	// Autosaving the layers prevents them for being deleted with the Asset
	// Save the packages created for the LayerInfos
	//if ( CreatedLayerInfoPackage.Num() > 0 )
	//   FEditorFileUtils::PromptForCheckoutAndSave( CreatedLayerInfoPackage, true, false );

	// Import the landscape data

	// Deactivate CastStaticShadow on the landscape to avoid "grid shadow" issue
	LandscapeProxy->bCastStaticShadow = false;

	if (LandscapeMaterial)
		LandscapeProxy->LandscapeMaterial = LandscapeMaterial;

	if (LandscapeHoleMaterial)
		LandscapeProxy->LandscapeHoleMaterial = LandscapeHoleMaterial;

	// Setting the layer type here.
	ELandscapeImportAlphamapType ImportLayerType = ELandscapeImportAlphamapType::Additive;

	TMap<FGuid, TArray<uint16>> HeightmapDataPerLayers;
	TMap<FGuid, TArray<FLandscapeImportLayerInfo>> MaterialLayerDataPerLayer;
	HeightmapDataPerLayers.Add(FGuid(), IntHeightData);
	MaterialLayerDataPerLayer.Add(FGuid(), ImportLayerInfos);

	// Import the data
	LandscapeProxy->Import(
		currentGUID,
		0, 0, XSize - 1, YSize - 1,
		NumSectionPerLandscapeComponent, NumQuadsPerLandscapeSection,
		HeightmapDataPerLayers, NULL,
		MaterialLayerDataPerLayer, ImportLayerType);

	// Copied straight from UE source code to avoid crash after importing the landscape:
	// automatically calculate a lighting LOD that won't crash lightmass (hopefully)
	// < 2048x2048 -> LOD0,  >=2048x2048 -> LOD1,  >= 4096x4096 -> LOD2,  >= 8192x8192 -> LOD3
	LandscapeProxy->StaticLightingLOD = FMath::DivideAndRoundUp(FMath::CeilLogTwo((XSize * YSize) / (2048 * 2048) + 1), (uint32)2);

	// Register all the landscape components
	LandscapeProxy->RegisterAllComponents();
	
	if (CreatedPackage) 
	{
		CreatedPackage->MarkPackageDirty();
		TArray<UPackage*> PackageToSave;
		PackageToSave.Add(CreatedPackage);

		// Save the created map
		FEditorFileUtils::PromptForCheckoutAndSave(PackageToSave, true, false);

		//CreatedPackage->RemoveFromRoot();
	}
	
	return LandscapeProxy;
}

void 
FHoudiniLandscapeTranslator::GetHeightFieldLandscapeMaterials(
	const FHoudiniGeoPartObject& Heightfield,
	UMaterialInterface*& LandscapeMaterial,
	UMaterialInterface*& LandscapeHoleMaterial)
{
	LandscapeMaterial = nullptr;
	LandscapeHoleMaterial = nullptr;

	if (Heightfield.Type != EHoudiniPartType::Volume)
		return;

	std::string MarshallingAttributeNameMaterial = HAPI_UNREAL_ATTRIB_MATERIAL;
	std::string MarshallingAttributeNameMaterialInstance = HAPI_UNREAL_ATTRIB_MATERIAL_INSTANCE;
	std::string MarshallingAttributeNameMaterialHole = HAPI_UNREAL_ATTRIB_MATERIAL_HOLE;
	std::string MarshallingAttributeNameMaterialHoleInstance = HAPI_UNREAL_ATTRIB_MATERIAL_HOLE_INSTANCE;

	TArray< FString > Materials;
	HAPI_AttributeInfo AttribMaterials;
	FHoudiniApi::AttributeInfo_Init(&AttribMaterials);

	// First, look for landscape material
	{
		FHoudiniEngineUtils::HapiGetAttributeDataAsString(
			Heightfield.GeoId, Heightfield.PartId, MarshallingAttributeNameMaterial.c_str(),
			AttribMaterials, Materials);

		// If the material attribute was not found, check the material instance attribute.
		if (!AttribMaterials.exists)
		{
			Materials.Empty();
			FHoudiniEngineUtils::HapiGetAttributeDataAsString(
				Heightfield.GeoId, Heightfield.PartId, MarshallingAttributeNameMaterialInstance.c_str(),
				AttribMaterials, Materials);
		}

		if (AttribMaterials.exists && AttribMaterials.owner != HAPI_ATTROWNER_PRIM && AttribMaterials.owner != HAPI_ATTROWNER_DETAIL)
		{
			HOUDINI_LOG_WARNING(TEXT("Landscape:  unreal_material must be a primitive or detail attribute, ignoring attribute."));
			AttribMaterials.exists = false;
			Materials.Empty();
		}

		if (AttribMaterials.exists && Materials.Num() > 0)
		{
			// Load the material
			LandscapeMaterial = Cast< UMaterialInterface >(StaticLoadObject(
				UMaterialInterface::StaticClass(),
				nullptr, *(Materials[0]), nullptr, LOAD_NoWarn, nullptr));
		}
	}

	Materials.Empty();
	FHoudiniApi::AttributeInfo_Init(&AttribMaterials);
	//FMemory::Memset< HAPI_AttributeInfo >( AttribMaterials, 0 );

	// Then, for the hole_material
	{
		FHoudiniEngineUtils::HapiGetAttributeDataAsString(
			Heightfield.GeoId, Heightfield.PartId, MarshallingAttributeNameMaterialHole.c_str(),
			AttribMaterials, Materials);

		// If the material attribute was not found, check the material instance attribute.
		if (!AttribMaterials.exists)
		{
			Materials.Empty();
			FHoudiniEngineUtils::HapiGetAttributeDataAsString(
				Heightfield.GeoId, Heightfield.PartId, MarshallingAttributeNameMaterialHoleInstance.c_str(),
				AttribMaterials, Materials);
		}

		if (AttribMaterials.exists && AttribMaterials.owner != HAPI_ATTROWNER_PRIM && AttribMaterials.owner != HAPI_ATTROWNER_DETAIL)
		{
			HOUDINI_LOG_WARNING(TEXT("Landscape:  unreal_material must be a primitive or detail attribute, ignoring attribute."));
			AttribMaterials.exists = false;
			Materials.Empty();
		}

		if (AttribMaterials.exists && Materials.Num() > 0)
		{
			// Load the material
			LandscapeHoleMaterial = Cast< UMaterialInterface >(StaticLoadObject(
				UMaterialInterface::StaticClass(),
				nullptr, *(Materials[0]), nullptr, LOAD_NoWarn, nullptr));
		}
	}
}

// Read the landscape component extent attribute from a heightfield
bool
FHoudiniLandscapeTranslator::GetLandscapeComponentExtentAttributes(
		const FHoudiniGeoPartObject& HoudiniGeoPartObject,
		int32& MinX, int32& MaxX,
		int32& MinY, int32& MaxY)
{
	// If we dont have minX, we likely dont have the others too
	if (!FHoudiniEngineUtils::HapiCheckAttributeExists(
		HoudiniGeoPartObject.GeoId, HoudiniGeoPartObject.PartId, "landscape_component_min_X", HAPI_ATTROWNER_PRIM))
		return false;

	// Create an AttributeInfo
	HAPI_AttributeInfo AttributeInfo;
	FHoudiniApi::AttributeInfo_Init(&AttributeInfo);
	//FMemory::Memzero< HAPI_AttributeInfo >(AttributeInfo);

	// Get MinX
	TArray<int32> IntData;
	if (!FHoudiniEngineUtils::HapiGetAttributeDataAsInteger(
		HoudiniGeoPartObject.GeoId, HoudiniGeoPartObject.PartId, "landscape_component_min_X", AttributeInfo, IntData, 1, HAPI_ATTROWNER_PRIM))
		return false;

	if (IntData.Num() > 0)
		MinX = IntData[0];

	// Get MaxX
	if (!FHoudiniEngineUtils::HapiGetAttributeDataAsInteger(
		HoudiniGeoPartObject.GeoId, HoudiniGeoPartObject.PartId, "landscape_component_max_X", AttributeInfo, IntData, 1, HAPI_ATTROWNER_PRIM))
		return false;

	if (IntData.Num() > 0)
		MaxX = IntData[0];

	// Get MinY
	if (!FHoudiniEngineUtils::HapiGetAttributeDataAsInteger(
		HoudiniGeoPartObject.GeoId, HoudiniGeoPartObject.PartId, "landscape_component_min_Y", AttributeInfo, IntData, 1, HAPI_ATTROWNER_PRIM))
		return false;

	if (IntData.Num() > 0)
		MinY = IntData[0];

	// Get MaxX
	if (!FHoudiniEngineUtils::HapiGetAttributeDataAsInteger(
		HoudiniGeoPartObject.GeoId, HoudiniGeoPartObject.PartId, "landscape_component_max_Y", AttributeInfo, IntData, 1, HAPI_ATTROWNER_PRIM))
		return false;

	if (IntData.Num() > 0)
		MaxY = IntData[0];

	return true;
}

ULandscapeLayerInfoObject *
FHoudiniLandscapeTranslator::CreateLandscapeLayerInfoObject(const FString& InLayerName, const FString& InPackagePath, const FString& InPackageName, UPackage*& OutPackage)
{
	FString PackageFullName = InPackagePath + TEXT("/") + InPackageName;

	// See if package exists, if it does, reuse it
	bool bCreatedPackage = false;
	OutPackage = FindPackage(nullptr, *PackageFullName);
	if (!OutPackage || OutPackage->IsPendingKill())
	{
		// We need to create a new package
		OutPackage = CreatePackage(nullptr, *PackageFullName);
		bCreatedPackage = true;
	}

	if (!OutPackage || OutPackage->IsPendingKill())
		return nullptr;

	if (!OutPackage->IsFullyLoaded())
		OutPackage->FullyLoad();

	ULandscapeLayerInfoObject* LayerInfo = nullptr;
	if (!bCreatedPackage)
	{
		// See if we can load the layer info instead of creating a new one
		LayerInfo = (ULandscapeLayerInfoObject*)StaticFindObjectFast(ULandscapeLayerInfoObject::StaticClass(), OutPackage, FName(*InPackageName));
	}

	if (!LayerInfo || LayerInfo->IsPendingKill())
	{
		// Create a new LandscapeLayerInfoObject in the package
		LayerInfo = NewObject<ULandscapeLayerInfoObject>(OutPackage, FName(*InPackageName), RF_Public | RF_Standalone /*| RF_Transactional*/);

		// Notify the asset registry
		FAssetRegistryModule::AssetCreated(LayerInfo);
	}

	if (LayerInfo && !LayerInfo->IsPendingKill())
	{
		LayerInfo->LayerName = FName(*InLayerName);

		// Trigger update of the Layer Info
		LayerInfo->PreEditChange(nullptr);
		LayerInfo->PostEditChange();
		LayerInfo->MarkPackageDirty();

		// Mark the package dirty...
		OutPackage->MarkPackageDirty();
	}

	return LayerInfo;
}

bool 
FHoudiniLandscapeTranslator::CalcHeightGlobalZminZMax(
	const TArray<UHoudiniOutput*>& AllOutputs, float& OutGlobalMin, float& OutGlobalMax) 
{
	OutGlobalMin = 0.f;
	OutGlobalMax = 0.f;

	for (const auto& CurrentOutput : AllOutputs)
	{
		if (!CurrentOutput)
			continue;

		if (CurrentOutput->GetType() != EHoudiniOutputType::Landscape)
			continue;

		const TArray<FHoudiniGeoPartObject>& HGPOs = CurrentOutput->GetHoudiniGeoPartObjects();
		for (const FHoudiniGeoPartObject& CurrentHGPO : HGPOs) 
		{
			if (CurrentHGPO.Type != EHoudiniPartType::Volume)
				continue;

			if (!CurrentHGPO.VolumeInfo.Name.Contains("height"))
				continue;

			// We're only handling single values for now
			if (CurrentHGPO.VolumeInfo.TupleSize != 1)
				continue;

			// Terrains always have a ZSize of 1.
			if (CurrentHGPO.VolumeInfo.ZLength != 1)
				continue;

			// Values should be float
			if (!CurrentHGPO.VolumeInfo.bIsFloat)
				continue;

			if (!FHoudiniEngineUtils::IsHoudiniNodeValid(CurrentHGPO.GeoId))
				continue;

			// Retrieve the VolumeInfo
			HAPI_VolumeInfo CurrentVolumeInfo;
			FHoudiniApi::VolumeInfo_Init(&CurrentVolumeInfo);
			if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetVolumeInfo(
				FHoudiniEngine::Get().GetSession(),
				CurrentHGPO.GeoId, CurrentHGPO.PartId, &CurrentVolumeInfo))
				continue;

			// Unreal's Z values are Y in Houdini
			float yMin = OutGlobalMin, yMax = OutGlobalMax;
			if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetVolumeBounds(FHoudiniEngine::Get().GetSession(),
				CurrentHGPO.GeoId, CurrentHGPO.PartId,
				nullptr, &yMin, nullptr,
				nullptr, &yMax, nullptr,
				nullptr, nullptr, nullptr))
				continue;

			if (yMin < OutGlobalMin)
				OutGlobalMin = yMin;

			if (yMax > OutGlobalMax)
				OutGlobalMax = yMax;
		}

		if (OutGlobalMin > OutGlobalMax)
		{
			OutGlobalMin = 0.f;
			OutGlobalMax = 0.f;
		}
	}

	return true;
}


bool
FHoudiniLandscapeTranslator::EnableWorldComposition()
{
	// Get the world
	UWorld* MyWorld = nullptr;
	{
		// We want to create the landscape in the landscape editor mode's world
		FWorldContext& EditorWorldContext = GEditor->GetEditorWorldContext();
		MyWorld = EditorWorldContext.World();
	}

	if (!MyWorld)
		return false;

	ULevel* CurrentLevel = MyWorld->GetCurrentLevel();

	if (!CurrentLevel)
		return false;

	AWorldSettings* WorldSettings = CurrentLevel->GetWorldSettings();
	if (!WorldSettings)
		return false;
	
	// Enable world composition in WorldSettings
	WorldSettings->bEnableWorldComposition = true;

	CurrentLevel->PostEditChange();
	
	return true;
}


bool
FHoudiniLandscapeTranslator::GetGenericPropertiesAttributes(
	const HAPI_NodeId& InGeoNodeId, const HAPI_PartId& InPartId,
	const int32& InPrimIndex, TArray<FHoudiniGenericAttribute>& OutPropertyAttributes)
{
	// List all the generic property detail attributes ...
	int32 FoundCount = FHoudiniEngineUtils::GetGenericAttributeList(
		InGeoNodeId, InPartId, HAPI_UNREAL_ATTRIB_GENERIC_UPROP_PREFIX, OutPropertyAttributes, HAPI_ATTROWNER_DETAIL);

	// .. then the primitive property attributes
	// Volumes apparently dont have prim attributes because they're converted to pointmeshes somehow...
	//FoundCount += FHoudiniEngineUtils::GetGenericAttributeList(
	//	InGeoNodeId, InPartId, HAPI_UNREAL_ATTRIB_GENERIC_UPROP_PREFIX, OutPropertyAttributes, HAPI_ATTROWNER_PRIM, InPrimIndex);

	// .. then the point property attributes
	FoundCount += FHoudiniEngineUtils::GetGenericAttributeList(
		InGeoNodeId, InPartId, HAPI_UNREAL_ATTRIB_GENERIC_UPROP_PREFIX, OutPropertyAttributes, HAPI_ATTROWNER_POINT, InPrimIndex);

	return FoundCount > 0;
}


bool
FHoudiniLandscapeTranslator::UpdateGenericPropertiesAttributes(
	UObject* InObject, const TArray<FHoudiniGenericAttribute>& InAllPropertyAttributes)
{
	if (!InObject || InObject->IsPendingKill())
		return false;

	// Iterate over the found Property attributes
	int32 NumSuccess = 0;
	for (auto CurrentPropAttribute : InAllPropertyAttributes)
	{
		// Update the current Property Attribute
		if (!FHoudiniGenericAttribute::UpdatePropertyAttributeOnObject(InObject, CurrentPropAttribute))
			continue;

		// Success!
		NumSuccess++;
		FString ClassName = InObject->GetClass() ? InObject->GetClass()->GetName() : TEXT("Object");
		FString ObjectName = InObject->GetName();
		HOUDINI_LOG_MESSAGE(TEXT("Modified UProperty %s on %s named %s"), *CurrentPropAttribute.AttributeName, *ClassName, *ObjectName);
	}

	return (NumSuccess > 0);
}


bool
FHoudiniLandscapeTranslator::BackupLandscapeToImageFiles(const FString& BaseName, ALandscapeProxy* Landscape)
{
	// We need to cache the input landscape to a file    
	if (!Landscape)
		return false;

	ULandscapeInfo* LandscapeInfo = Landscape->GetLandscapeInfo();
	if (!LandscapeInfo)
		return false;

	// Save Height data to file
	//FString HeightSave = TEXT("/Game/HoudiniEngine/Temp/HeightCache.png");    
	FString HeightSave = BaseName + TEXT("_height.png");
	LandscapeInfo->ExportHeightmap(HeightSave);
	Landscape->ReimportHeightmapFilePath = HeightSave;

	// Save each layer to a file
	for (int LayerIndex = 0; LayerIndex < LandscapeInfo->Layers.Num(); LayerIndex++)
	{
		FName CurrentLayerName = LandscapeInfo->Layers[LayerIndex].GetLayerName();
		//ULandscapeLayerInfoObject* CurrentLayerInfo = LandscapeInfo->GetLayerInfoByName(CurrentLayerName, Landscape);
		ULandscapeLayerInfoObject* CurrentLayerInfo = LandscapeInfo->Layers[LayerIndex].LayerInfoObj;
		if (!CurrentLayerInfo || CurrentLayerInfo->IsPendingKill())
			continue;

		FString LayerSave = BaseName + CurrentLayerName.ToString() + TEXT(".png");
		LandscapeInfo->ExportLayer(CurrentLayerInfo, LayerSave);

		// Update the file reimport path on the input landscape for this layer
		LandscapeInfo->GetLayerEditorSettings(CurrentLayerInfo).ReimportLayerFilePath = LayerSave;
	}

	return true;
}


bool
FHoudiniLandscapeTranslator::RestoreLandscapeFromImageFiles(ALandscapeProxy* LandscapeProxy)
{
	if (!LandscapeProxy)
		return false;

	ULandscapeInfo* LandscapeInfo = LandscapeProxy->GetLandscapeInfo();
	if (!LandscapeInfo)
		return false;

	// Restore Height data from the backup file
	FString ReimportFile = LandscapeProxy->ReimportHeightmapFilePath;
	if (!FHoudiniLandscapeTranslator::ImportLandscapeData(LandscapeInfo, ReimportFile, TEXT("height")))
		HOUDINI_LOG_ERROR(TEXT("Could not restore the landscape actor's source height data."));

	// Restore each layer from the backup file
	TArray< ULandscapeLayerInfoObject* > SourceLayers;
	for (int LayerIndex = 0; LayerIndex < LandscapeProxy->EditorLayerSettings.Num(); LayerIndex++)
	{
		ULandscapeLayerInfoObject* CurrentLayerInfo = LandscapeProxy->EditorLayerSettings[LayerIndex].LayerInfoObj;
		if (!CurrentLayerInfo || CurrentLayerInfo->IsPendingKill())
			continue;

		FString CurrentLayerName = CurrentLayerInfo->LayerName.ToString();
		ReimportFile = LandscapeProxy->EditorLayerSettings[LayerIndex].ReimportLayerFilePath;

		if (!FHoudiniLandscapeTranslator::ImportLandscapeData(LandscapeInfo, ReimportFile, CurrentLayerName, CurrentLayerInfo))
			HOUDINI_LOG_ERROR(TEXT("Could not restore the landscape actor's source height data."));

		SourceLayers.Add(CurrentLayerInfo);
	}

	// Iterate on the landscape info's layer to remove any layer that could have been added by Houdini
	for (int LayerIndex = 0; LayerIndex < LandscapeInfo->Layers.Num(); LayerIndex++)
	{
		ULandscapeLayerInfoObject* CurrentLayerInfo = LandscapeInfo->Layers[LayerIndex].LayerInfoObj;
		if (SourceLayers.Contains(CurrentLayerInfo))
			continue;

		// Delete the added layer
		FName LayerName = LandscapeInfo->Layers[LayerIndex].LayerName;
		LandscapeInfo->DeleteLayer(CurrentLayerInfo, LayerName);
	}

	return true;
}


bool
FHoudiniLandscapeTranslator::ImportLandscapeData(
	ULandscapeInfo* LandscapeInfo, const FString& Filename, const FString& LayerName, ULandscapeLayerInfoObject* LayerInfoObject)
{
	//
	// Code copied/edited from FEdModeLandscape::ImportData as we cannot access that function
	//
	if (!LandscapeInfo)
		return false;

	bool IsHeight = LayerName.Equals(TEXT("height"), ESearchCase::IgnoreCase);

	int32 MinX, MinY, MaxX, MaxY;
	if (LandscapeInfo && LandscapeInfo->GetLandscapeExtent(MinX, MinY, MaxX, MaxY))
	{
		const FLandscapeFileResolution LandscapeResolution = { (uint32)(1 + MaxX - MinX), (uint32)(1 + MaxY - MinY) };

		ILandscapeEditorModule& LandscapeEditorModule = FModuleManager::GetModuleChecked<ILandscapeEditorModule>("LandscapeEditor");

		if (IsHeight)
		{
			const ILandscapeHeightmapFileFormat* HeightmapFormat = LandscapeEditorModule.GetHeightmapFormatByExtension(*FPaths::GetExtension(Filename, true));

			if (!HeightmapFormat)
			{
				HOUDINI_LOG_ERROR(TEXT("Could not reimport the input heightfield's source data for %s, File type not recognised"), *LayerName);
				return false;
			}

			FLandscapeFileResolution ImportResolution = { 0, 0 };

			const FLandscapeHeightmapInfo HeightmapInfo = HeightmapFormat->Validate(*Filename);

			// display error message if there is one, and abort the import
			if (HeightmapInfo.ResultCode == ELandscapeImportResult::Error)
			{
				HOUDINI_LOG_ERROR(TEXT("Could not reimport the input heightfield's source data for %s, %s"), *LayerName, *(HeightmapInfo.ErrorMessage.ToString()));
				return false;
			}

			// if the file is a raw format with multiple possibly resolutions, only attempt import if one matches the current landscape
			if (HeightmapInfo.PossibleResolutions.Num() > 1)
			{
				if (!HeightmapInfo.PossibleResolutions.Contains(LandscapeResolution))
				{
					HOUDINI_LOG_ERROR(TEXT("Could not reimport the input heightfield's source data for %s. The heightmap file does not match the Landscape extent and its exact resolution could not be determined"));
					return false;
				}
				else
				{
					ImportResolution = LandscapeResolution;
				}
			}

			// display warning message if there is one and allow user to cancel
			if (HeightmapInfo.ResultCode == ELandscapeImportResult::Warning)
				HOUDINI_LOG_WARNING(TEXT("When reimporting the input heightfield's source data for %s. %s"), *LayerName, *(HeightmapInfo.ErrorMessage.ToString()));

			// if the file is a format with resolution information, warn the user if the resolution doesn't match the current landscape
			// unlike for raw this is only a warning as we can pad/clip the data if we know what resolution it is
			if (HeightmapInfo.PossibleResolutions.Num() == 1)
			{
				ImportResolution = HeightmapInfo.PossibleResolutions[0];
				if (ImportResolution != LandscapeResolution)
					HOUDINI_LOG_WARNING(TEXT("When reimporting the input heightfield's source data for %s. The heightmap file's size does not match the current Landscape extent, data will be padded/clipped"), *LayerName);
			}

			FLandscapeHeightmapImportData ImportData = HeightmapFormat->Import(*Filename, ImportResolution);
			if (ImportData.ResultCode == ELandscapeImportResult::Error)
			{
				HOUDINI_LOG_ERROR(TEXT("Could not reimport the input heightfield's source data for %s. %s"), *LayerName, *(ImportData.ErrorMessage.ToString()));
				return false;
			}

			TArray<uint16> Data;
			if (ImportResolution != LandscapeResolution)
			{
				// Cloned from FLandscapeEditorDetailCustomization_NewLandscape.OnCreateButtonClicked
				// so that reimports behave the same as the initial import :)

				const int32 OffsetX = (int32)(LandscapeResolution.Width - ImportResolution.Width) / 2;
				const int32 OffsetY = (int32)(LandscapeResolution.Height - ImportResolution.Height) / 2;

				Data.SetNumUninitialized(LandscapeResolution.Width * LandscapeResolution.Height * sizeof(uint16));

				ExpandData<uint16>(Data.GetData(), ImportData.Data.GetData(),
					0, 0, ImportResolution.Width - 1, ImportResolution.Height - 1,
					-OffsetX, -OffsetY, LandscapeResolution.Width - OffsetX - 1, LandscapeResolution.Height - OffsetY - 1);
			}
			else
			{
				Data = MoveTemp(ImportData.Data);
			}

			//FScopedTransaction Transaction(TEXT("Undo_ImportHeightmap", "Importing Landscape Heightmap"));

			FHeightmapAccessor<false> HeightmapAccessor(LandscapeInfo);
			HeightmapAccessor.SetData(MinX, MinY, MaxX, MaxY, Data.GetData());
		}
		else
		{
			// We're importing a Landscape layer
			if (!LayerInfoObject || LayerInfoObject->IsPendingKill())
				return false;

			const ILandscapeWeightmapFileFormat* WeightmapFormat = LandscapeEditorModule.GetWeightmapFormatByExtension(*FPaths::GetExtension(Filename, true));
			if (!WeightmapFormat)
			{
				HOUDINI_LOG_ERROR(TEXT("Could not reimport the input heightfield's source data for %s, File type not recognised"), *LayerName);
				return false;
			}

			FLandscapeFileResolution ImportResolution = { 0, 0 };

			const FLandscapeWeightmapInfo WeightmapInfo = WeightmapFormat->Validate(*Filename, FName(*LayerName));

			// display error message if there is one, and abort the import
			if (WeightmapInfo.ResultCode == ELandscapeImportResult::Error)
			{
				HOUDINI_LOG_ERROR(TEXT("Could not reimport the input heightfield's source data for %s, %s"), *LayerName, *(WeightmapInfo.ErrorMessage.ToString()));
				return false;
			}

			// if the file is a raw format with multiple possibly resolutions, only attempt import if one matches the current landscape
			if (WeightmapInfo.PossibleResolutions.Num() > 1)
			{
				if (!WeightmapInfo.PossibleResolutions.Contains(LandscapeResolution))
				{
					HOUDINI_LOG_ERROR(TEXT("Could not reimport the input heightfield's source data for %s. The weightmap file does not match the Landscape extent and its exact resolution could not be determined"));
					return false;
				}
				else
				{
					ImportResolution = LandscapeResolution;
				}
			}

			// display warning message if there is one and allow user to cancel
			if (WeightmapInfo.ResultCode == ELandscapeImportResult::Warning)
				HOUDINI_LOG_WARNING(TEXT("When reimporting the input heightfield's source data for %s. %s"), *LayerName, *(WeightmapInfo.ErrorMessage.ToString()));

			// if the file is a format with resolution information, warn the user if the resolution doesn't match the current landscape
			// unlike for raw this is only a warning as we can pad/clip the data if we know what resolution it is
			if (WeightmapInfo.PossibleResolutions.Num() == 1)
			{
				ImportResolution = WeightmapInfo.PossibleResolutions[0];
				if (ImportResolution != LandscapeResolution)
					HOUDINI_LOG_WARNING(TEXT("When reimporting the input heightfield's source data for %s. The heightmap file's size does not match the current Landscape extent, data will be padded/clipped"), *LayerName);
			}

			FLandscapeWeightmapImportData ImportData = WeightmapFormat->Import(*Filename, FName(*LayerName), ImportResolution);

			if (ImportData.ResultCode == ELandscapeImportResult::Error)
			{
				HOUDINI_LOG_ERROR(TEXT("Could not reimport the input heightfield's source data for %s. %s"), *LayerName, *(ImportData.ErrorMessage.ToString()));
				return false;
			}

			TArray<uint8> Data;
			if (ImportResolution != LandscapeResolution)
			{
				// Cloned from FLandscapeEditorDetailCustomization_NewLandscape.OnCreateButtonClicked
				// so that reimports behave the same as the initial import :)
				const int32 OffsetX = (int32)(LandscapeResolution.Width - ImportResolution.Width) / 2;
				const int32 OffsetY = (int32)(LandscapeResolution.Height - ImportResolution.Height) / 2;

				Data.SetNumUninitialized(LandscapeResolution.Width * LandscapeResolution.Height * sizeof(uint8));

				ExpandData<uint8>(Data.GetData(), ImportData.Data.GetData(),
					0, 0, ImportResolution.Width - 1, ImportResolution.Height - 1,
					-OffsetX, -OffsetY, LandscapeResolution.Width - OffsetX - 1, LandscapeResolution.Height - OffsetY - 1);
			}
			else
			{
				Data = MoveTemp(ImportData.Data);
			}

			//FScopedTransaction Transaction(LOCTEXT("Undo_ImportWeightmap", "Importing Landscape Layer"));
			FAlphamapAccessor<false, false> AlphamapAccessor(LandscapeInfo, LayerInfoObject);
			AlphamapAccessor.SetData(MinX, MinY, MaxX, MaxY, Data.GetData(), ELandscapeLayerPaintingRestriction::None);
		}
	}

	return true;
}
