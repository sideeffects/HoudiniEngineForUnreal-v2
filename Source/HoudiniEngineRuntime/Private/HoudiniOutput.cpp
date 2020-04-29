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


#include "HoudiniOutput.h"
#include "HoudiniSplineComponent.h"

#include "Components\SceneComponent.h"
#include "Components/SplineComponent.h"


UHoudiniLandscapePtr::UHoudiniLandscapePtr(class FObjectInitializer const& Initializer) 
{
	bIsWorldCompositionLandscape = false;
	BakeType = EHoudiniLandscapeOutputBakeType::Detachment;
};

uint32
GetTypeHash(const FHoudiniOutputObjectIdentifier& HoudiniOutputObjectIdentifier)
{
	return HoudiniOutputObjectIdentifier.GetTypeHash();
}

void
FHoudiniInstancedOutput::SetVariationObjectAt(const int32& AtIndex, UObject* InObject)
{
	// Resize the array if needed
	if (VariationObjects.Num() <= AtIndex)
		VariationObjects.SetNum(AtIndex + 1);

	if (VariationTransformOffsets.Num() <= AtIndex)
		VariationTransformOffsets.SetNum(AtIndex + 1);

	UObject* CurrentObject = VariationObjects[AtIndex].LoadSynchronous();
	if (CurrentObject == InObject)
		return;

	VariationObjects[AtIndex] = InObject;
}

bool 
FHoudiniInstancedOutput::SetTransformOffsetAt(const float& Value, const int32& AtIndex, const int32& PosRotScaleIndex, const int32& XYZIndex)
{
	FTransform* Transform = VariationTransformOffsets.IsValidIndex(AtIndex) ? &VariationTransformOffsets[AtIndex] : nullptr;
	if (!Transform)
		return false;

	if (PosRotScaleIndex == 0)
	{
		FVector Position = Transform->GetLocation();
		if (Position[XYZIndex] == Value)
			return false;
		Position[XYZIndex] = Value;
		Transform->SetLocation(Position);
	}
	else if (PosRotScaleIndex == 1)
	{
		FRotator Rotator = Transform->Rotator();
		switch (XYZIndex)
		{
		case 0:
		{
			if (Rotator.Roll == Value)
				return false;
			Rotator.Roll = Value;
			break;
		}

		case 1:
		{
			if (Rotator.Pitch == Value)
				return false;
			Rotator.Pitch = Value;
			break;
		}

		case 2:
		{
			if (Rotator.Yaw == Value)
				return false;
			Rotator.Yaw = Value;
			break;
		}
		}
		Transform->SetRotation(Rotator.Quaternion());
	}
	else if (PosRotScaleIndex == 2)
	{
		FVector Scale = Transform->GetScale3D();
		if (Scale[XYZIndex] == Value)
			return false;

		Scale[XYZIndex] = Value;
		Transform->SetScale3D(Scale);
	}

	MarkChanged(true);

	return true;
}

float 
FHoudiniInstancedOutput::GetTransformOffsetAt(const int32& AtIndex, const int32& PosRotScaleIndex, const int32& XYZIndex)
{
	FTransform* Transform = VariationTransformOffsets.IsValidIndex(AtIndex) ? &VariationTransformOffsets[AtIndex] : nullptr;
	if (!Transform)
		return 0.0f;

	if (PosRotScaleIndex == 0)
	{
		FVector Position = Transform->GetLocation();
		return Position[XYZIndex];
	}
	else if (PosRotScaleIndex == 1)
	{
		FRotator Rotator = Transform->Rotator();
		switch (XYZIndex)
		{
			case 0:
			{
				return Rotator.Roll;
			}

			case 1:
			{
				return Rotator.Pitch;
			}

			case 2:
			{
				return Rotator.Yaw;
			}
		}
	}
	else if (PosRotScaleIndex == 2)
	{
		FVector Scale = Transform->GetScale3D();
		return Scale[XYZIndex];
	}

	return 0.0f;
}

FHoudiniOutputObjectIdentifier::FHoudiniOutputObjectIdentifier()
{
	ObjectId = -1;
	GeoId = -1;
	PartId = -1;
	SplitIdentifier = FString();
	PartName = FString();
}

FHoudiniOutputObjectIdentifier::FHoudiniOutputObjectIdentifier(
	const int32& InObjectId, const int32& InGeoId, const int32& InPartId, const FString& InSplitIdentifier)
{
	ObjectId = InObjectId;
	GeoId = InGeoId;
	PartId = InPartId;
	SplitIdentifier = InSplitIdentifier;
}

uint32
FHoudiniOutputObjectIdentifier::GetTypeHash() const
{
	int32 HashBuffer[3] = { ObjectId, GeoId, PartId };
	int32 Hash = FCrc::MemCrc32((void *)&HashBuffer[0], sizeof(HashBuffer));
	return FCrc::StrCrc32(*SplitIdentifier, Hash);
}

bool
FHoudiniOutputObjectIdentifier::operator==(const FHoudiniOutputObjectIdentifier& InOutputObjectIdentifier) const
{
	// Object/Geo/Part IDs must match
	bool bMatchingIds = true;
	if (ObjectId != InOutputObjectIdentifier.ObjectId
		|| GeoId != InOutputObjectIdentifier.GeoId
		|| PartId != InOutputObjectIdentifier.PartId)
		bMatchingIds = false;

	if ((bLoaded && !InOutputObjectIdentifier.bLoaded)
		|| (!bLoaded && InOutputObjectIdentifier.bLoaded))
	{
		// If one of the two identifier is loaded, 
		// we can simply compare the part names
		if (PartName.Equals(InOutputObjectIdentifier.PartName)
			&& SplitIdentifier.Equals(InOutputObjectIdentifier.SplitIdentifier))
			return true;
	}

	if (!bMatchingIds)
	{
		return false;
	}

	// If split ID and name match, we're equal...
	if (SplitIdentifier.Equals(InOutputObjectIdentifier.SplitIdentifier))
		return true;

	// ... if not we're different
	return false;
}

bool
FHoudiniOutputObjectIdentifier::Matches(const FHoudiniGeoPartObject& InHGPO) const
{
	// Object/Geo/Part IDs must match
	bool bMatchingIds = true;
	if (ObjectId != InHGPO.ObjectId
		|| GeoId != InHGPO.GeoId
		|| PartId != InHGPO.PartId)
		bMatchingIds = false;

	if ((bLoaded && !InHGPO.bLoaded) || (!bLoaded && InHGPO.bLoaded))
	{
		// If either the HGPO or the Identifer is nmarked as loaded, 
		// we can simply compare the part names
		if (PartName.Equals(InHGPO.PartName))
			return true;
	}

	if (!bMatchingIds)
	{
		return false;
	}

	// If the HGPO has our split identifier
	//if (InHGPO.SplitGroups.Contains(SplitIdentifier))
	//	return true;

	//
	return true;
}


UHoudiniOutput::UHoudiniOutput(const FObjectInitializer & ObjectInitializer)
	: Super(ObjectInitializer)
	, Type(EHoudiniOutputType::Invalid)
	, StaleCount(0)
	, bLandscapeWorldComposition(false)
	, bIsEditableNode(false)
	, bHasEditableNodeBuilt(false)
{
	
}

UHoudiniOutput::~UHoudiniOutput()
{
	Type = EHoudiniOutputType::Invalid;
	StaleCount = 0;
	bIsUpdating = false;

	HoudiniGeoPartObjects.Empty();
	OutputObjects.Empty();
	InstancedOutputs.Empty();
	AssignementMaterials.Empty();
	ReplacementMaterials.Empty();
}

void
UHoudiniOutput::BeginDestroy()
{
	Super::BeginDestroy();
}

void
UHoudiniOutput::Clear()
{
	StaleCount = 0;

	HoudiniGeoPartObjects.Empty();

	for (auto& CurrentOutputObject : OutputObjects)
	{
		// Clear the output component
		USceneComponent* SceneComp = Cast<USceneComponent>(CurrentOutputObject.Value.OutputComponent);
		if (SceneComp && !SceneComp->IsPendingKill())
		{
			SceneComp->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
			SceneComp->UnregisterComponent();
			SceneComp->DestroyComponent();
		}

		// Also destroy proxy components
		USceneComponent* ProxyComp = Cast<USceneComponent>(CurrentOutputObject.Value.ProxyComponent);
		if (ProxyComp && !ProxyComp->IsPendingKill())
		{
			ProxyComp->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
			ProxyComp->UnregisterComponent();
			ProxyComp->DestroyComponent();
		}

		if (Type == EHoudiniOutputType::Landscape && !bLandscapeWorldComposition)
		{
			UHoudiniLandscapePtr* LandscapePtr = Cast<UHoudiniLandscapePtr>(CurrentOutputObject.Value.OutputObject);
			ALandscapeProxy* LandscapeProxy = LandscapePtr ? LandscapePtr->GetRawPtr() : nullptr;

			if (LandscapeProxy)
			{
				LandscapeProxy->Destroy();
			}
		}
	}

	OutputObjects.Empty();
	InstancedOutputs.Empty();
	AssignementMaterials.Empty();
	ReplacementMaterials.Empty();

	Type = EHoudiniOutputType::Invalid;
}

const bool 
UHoudiniOutput::HasGeoChanged() const
{
	for (auto currentHGPO : HoudiniGeoPartObjects)
	{
		if (currentHGPO.bHasGeoChanged)
			return true;
	}

	return false;
}

const bool
UHoudiniOutput::HasTransformChanged() const
{
	for (auto currentHGPO : HoudiniGeoPartObjects)
	{
		if (currentHGPO.bHasTransformChanged)
			return true;
	}

	return false;
}

const bool
UHoudiniOutput::HasMaterialsChanged() const
{
	for (auto currentHGPO : HoudiniGeoPartObjects)
	{
		if (currentHGPO.bHasMaterialsChanged)
			return true;
	}

	return false;
}

const bool 
UHoudiniOutput::HasHoudiniGeoPartObject(const FHoudiniGeoPartObject& InHGPO) const
{
	return HoudiniGeoPartObjects.Find(InHGPO) != INDEX_NONE;
}

const bool
UHoudiniOutput::HeightfieldMatch(const FHoudiniGeoPartObject& InHGPO) const
{	
	if (InHGPO.Type != EHoudiniPartType::Volume)
		return false;

	if (InHGPO.VolumeName.IsEmpty())
		return false;

	//if (Type != EHoudiniOutputType::Landscape)
	//	return false;

	bool bMatchFound = false;
	for (auto& currentHGPO : HoudiniGeoPartObjects)
	{
		// Asset/Object/Geo IDs should match
		if (currentHGPO.AssetId != InHGPO.AssetId
			|| currentHGPO.ObjectId != InHGPO.ObjectId
			|| currentHGPO.GeoId != InHGPO.GeoId)
		{
			continue;
		}

		// Both type should be volumes
		if (currentHGPO.Type != EHoudiniPartType::Volume)
		{
			continue;
		}

		// Volume tile index should match
		if (currentHGPO.VolumeTileIndex != InHGPO.VolumeTileIndex)
		{
			continue;
		}

		// Volume Names should be different!
		if (InHGPO.VolumeName.Equals(currentHGPO.VolumeName, ESearchCase::IgnoreCase))
		{
			bMatchFound = false;
		}
		else
		{
			bMatchFound = true;
		}
	}

	return bMatchFound;
}

void 
UHoudiniOutput::MarkAllHGPOsAsStale(const bool& bInStale)
{
	// Since objects can only be added to this array,
	// Simply keep track of the current number of HoudiniGeoPartObject
	StaleCount = bInStale ? HoudiniGeoPartObjects.Num() : 0;
}

void 
UHoudiniOutput::DeleteAllStaleHGPOs()
{
	// Simply delete the first "StaleCount" objects and reset the stale marker
	HoudiniGeoPartObjects.RemoveAt(0, StaleCount);
	StaleCount = 0;
}

void 
UHoudiniOutput::AddNewHGPO(const FHoudiniGeoPartObject& InHGPO)
{
	HoudiniGeoPartObjects.Add(InHGPO);
}

void
UHoudiniOutput::UpdateOutputType()
{
	int32 MeshCount = 0;
	int32 CurveCount = 0;
	int32 VolumeCount = 0;
	int32 InstancerCount = 0;
	for (auto& HGPO : HoudiniGeoPartObjects)
	{
		switch (HGPO.Type)
		{
		case EHoudiniPartType::Mesh:
			MeshCount++;
			break;
		case EHoudiniPartType::Curve:
			CurveCount++;
			break;
		case EHoudiniPartType::Volume:
			VolumeCount++;
			break;
		case EHoudiniPartType::Instancer:
			InstancerCount++;
			break;
		default:
		case EHoudiniPartType::Invalid:
			break;
		}
	}
	
	if (VolumeCount > 0)
	{
		// If we have a volume, we're a landscape
		Type = EHoudiniOutputType::Landscape;
	}
	else if (InstancerCount > 0)
	{
		// if we have at least an instancer, we're one
		Type = EHoudiniOutputType::Instancer;
	}
	else if (MeshCount > 0)
	{
		Type = EHoudiniOutputType::Mesh;
	}
	else if (CurveCount > 0)
	{
		Type = EHoudiniOutputType::Curve;
	}
	else
	{
		// No valid HGPO detected...
		Type = EHoudiniOutputType::Invalid;
	}
}

FString
UHoudiniOutput::OutputTypeToString(const EHoudiniOutputType& InOutputType)
{
	FString OutputTypeStr;
	switch (InOutputType)
	{
		case EHoudiniOutputType::Mesh:
			OutputTypeStr = TEXT("Mesh");
			break;
		case EHoudiniOutputType::Instancer:
			OutputTypeStr = TEXT("Instancer");
			break;
		case EHoudiniOutputType::Landscape:
			OutputTypeStr = TEXT("Landscape");
			break;
		case EHoudiniOutputType::Curve:
			OutputTypeStr = TEXT("Curve");
			break;
		case EHoudiniOutputType::Skeletal:
			OutputTypeStr = TEXT("Skeletal");
			break;

		default:
		case EHoudiniOutputType::Invalid:
			OutputTypeStr = TEXT("Invalid");
			break;
	}

	return OutputTypeStr;
}

void
UHoudiniOutput::MarkAsLoaded(const bool& InLoaded)
{
	// Mark all HGPO as loaded
	for (auto& HGPO : HoudiniGeoPartObjects)
	{
		HGPO.bLoaded = InLoaded;
	}

	// Mark all output object's identifier as loaded
	for (auto& Iter : OutputObjects)
	{
		FHoudiniOutputObjectIdentifier& Identifier = Iter.Key;
		Identifier.bLoaded = InLoaded;
	}

	// Instanced outputs
	for (auto& Iter : InstancedOutputs)
	{
		FHoudiniOutputObjectIdentifier& Identifier = Iter.Key;
		Identifier.bLoaded = InLoaded;
	}
}

bool 
UHoudiniOutput::HasCurveExportTypeChanged() const
{
	for (const auto& CurrentOutputObj : OutputObjects) 
	{
		UObject* CurrentOutputComponent = CurrentOutputObj.Value.OutputComponent;
		if (!CurrentOutputComponent || CurrentOutputComponent->IsPendingKill())
			continue;

		const FHoudiniCurveOutputProperties& CurveProperties = CurrentOutputObj.Value.CurveOutputProperty;
		if (CurrentOutputComponent->IsA<USplineComponent>())
		{
			if (CurveProperties.CurveOutputType != EHoudiniCurveOutputType::UnrealSpline)
				return true;

			// TODO: make this actually reflect the info from HAPI
			USplineComponent* UnrealSplineComponent = Cast<USplineComponent>(CurrentOutputComponent);
			if (!UnrealSplineComponent)
				continue;

			if (CurveProperties.CurveType == EHoudiniCurveType::Linear)
			{
				if (UnrealSplineComponent->GetNumberOfSplinePoints() > 0 && UnrealSplineComponent->GetSplinePointType(0) != ESplinePointType::Linear)
					return true;
			}
			else
			{
				if (UnrealSplineComponent->GetNumberOfSplinePoints() > 0 && UnrealSplineComponent->GetSplinePointType(0) == ESplinePointType::Linear)
					return true;
			}

		}
		else if (CurrentOutputComponent->IsA<UHoudiniSplineComponent>())
		{
			if (CurveProperties.CurveOutputType != EHoudiniCurveOutputType::HoudiniSpline)
				return true;

			// TODO?? Check for linear etc... ???
		}
	}

	return false;
}

const bool 
UHoudiniOutput::HasAnyProxy() const
{
	for (const auto& Pair : OutputObjects)
	{
		UObject* FoundProxy = Pair.Value.ProxyObject;
		if (FoundProxy && !FoundProxy->IsPendingKill())
		{
			return true;
		}
	}

	return false;
}

const bool 
UHoudiniOutput::HasProxy(const FHoudiniOutputObjectIdentifier& InIdentifier) const
{
	const FHoudiniOutputObject* FoundOutputObject = OutputObjects.Find(InIdentifier);
	if (!FoundOutputObject)
		return false;

	UObject* FoundProxy = FoundOutputObject->ProxyObject;
	if (!FoundProxy || FoundProxy->IsPendingKill())
		return false;

	return true;
}

const bool
UHoudiniOutput::HasAnyCurrentProxy() const
{
	for (const auto& Pair : OutputObjects)
	{
		UObject* FoundProxy = Pair.Value.ProxyObject;
		if (FoundProxy && !FoundProxy->IsPendingKill())
		{
			if(Pair.Value.bProxyIsCurrent)
			{
				return true;
			}
		}
	}

	return false;
}

const bool 
UHoudiniOutput::IsProxyCurrent(const FHoudiniOutputObjectIdentifier &InIdentifier) const
{
	if (!HasProxy(InIdentifier))
		return false;

	const FHoudiniOutputObject* FoundOutputObject = OutputObjects.Find(InIdentifier);
	if (!FoundOutputObject)
		return false;

	return FoundOutputObject->bProxyIsCurrent;
}
