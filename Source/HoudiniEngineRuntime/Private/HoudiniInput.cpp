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

#include "HoudiniInput.h"

#include "HoudiniEngineRuntime.h"
#include "HoudiniAssetComponent.h"
#include "HoudiniOutput.h"
#include "HoudiniSplineComponent.h"
#include "HoudiniAsset.h"
#include "HoudiniGeoPartObject.h"
#include "HoudiniAssetComponent.h"

#include "EngineUtils.h"
#include "Engine/Brush.h"
#include "Model.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "UObject/UObjectGlobals.h"
#include "Components/SplineComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Landscape.h"


//
UHoudiniInput::UHoudiniInput(const FObjectInitializer & ObjectInitializer)
	: Super(ObjectInitializer)
	, Type(EHoudiniInputType::Invalid)
	, PreviousType(EHoudiniInputType::Invalid)
	, AssetNodeId(-1)
	, InputNodeId(-1)
	, InputIndex(0)
	, ParmId(-1)
	, bIsObjectPathParameter(false)
	, bHasChanged(false)
	, bPackBeforeMerge(false)
	, bExportLODs(false)
	, bExportSockets(false)
	, bExportColliders(false)
	, bCookOnCurveChanged(true)
	, bStaticMeshChanged(false)
	, bInputAssetConnectedInHoudini(false)
	, bSwitchedToCurve(false)
	, DefaultCurveOffset(0.f)
	, bIsWorldInputBoundSelector(false)
	, bWorldInputBoundSelectorAutoUpdate(false)
	, UnrealSplineResolution(0.0f)
	, bUpdateInputLandscape(false)
	, LandscapeExportType(EHoudiniLandscapeExportType::Heightfield)
	, bLandscapeExportSelectionOnly(false)
	, bLandscapeAutoSelectComponent(false)
	, bLandscapeExportMaterials(false)
	, bLandscapeExportLighting(false)
	, bLandscapeExportNormalizedUVs(false)
	, bLandscapeExportTileUVs(false)
{
	Name = TEXT("");
	Label = TEXT("");
	SetFlags(RF_Transactional);

	// Geometry inputs always have one null default object
	GeometryInputObjects.Add(nullptr);
	
	KeepWorldTransform = GetDefaultXTransformType();
}

void
UHoudiniInput::BeginDestroy()
{
	// If valid, mark our input node for deletion
	if (InputNodeId >= 0)
	{
		// .. but if we're an asset input, don't delete the node as InputNodeId
		// is set to the input HDA's node ID!
		if (Type != EHoudiniInputType::Asset)
		{
			FHoudiniEngineRuntime::Get().MarkNodeIdAsPendingDelete(InputNodeId, true);
		}
		
		InputNodeId = -1;
	}

	// DO NOT MANUALLY DESTROY OUR INPUT OBJECTS!
	// This messes up unreal's Garbage collection and would cause crashes on duplication

	// Mark all our input objects for destruction
	GeometryInputObjects.Empty();
	CurveInputObjects.Empty();
	WorldInputObjects.Empty();
	SkeletalInputObjects.Empty();
	LandscapeInputObjects.Empty();
	AssetInputObjects.Empty();

	Super::BeginDestroy();
}

#if WITH_EDITOR
void UHoudiniInput::PostEditUndo() 
{
	 Super::PostEditUndo();
	 TArray<UHoudiniInputObject*>* InputObjectsPtr = GetHoudiniInputObjectArray(Type);
	 if (!InputObjectsPtr)
		 return;
	 
	 if (Type == EHoudiniInputType::Curve) 
	 {
		bool bUndoDelete = false;
		bool bUndoInsert = false;
		bool bUndoDeletedObjArrayEmptied = false;

		TArray< USceneComponent* > childActor;
		UHoudiniAssetComponent* OuterHAC = Cast<UHoudiniAssetComponent>(GetOuter());
		if (OuterHAC && !OuterHAC->IsPendingKill())
			childActor = OuterHAC->GetAttachChildren();

		// Undo delete input objects action
		for (int Index = 0; Index < GetNumberOfInputObjects(); ++Index)
		{
			UHoudiniInputObject* InputObject = (*InputObjectsPtr)[Index];
			if (!InputObject) 
				continue;

			UHoudiniInputHoudiniSplineComponent * HoudiniSplineInputObject = Cast<UHoudiniInputHoudiniSplineComponent>(InputObject);

			if (!HoudiniSplineInputObject) 
				continue;

			UHoudiniSplineComponent* SplineComponent = HoudiniSplineInputObject->GetCurveComponent();

			if (!SplineComponent) 
				continue;

			// If the last change deleted this curve input, recreate this Houdini Spline input.
			if ( !SplineComponent->GetAttachParent() )
			{	 
				bUndoDelete = true;

				if (!bUndoDeletedObjArrayEmptied)   LastUndoDeletedInputs.Empty();

				bUndoDeletedObjArrayEmptied = true;


				UHoudiniSplineComponent * ReconstructedSpline = NewObject<UHoudiniSplineComponent>(
					GetOuter(), UHoudiniSplineComponent::StaticClass());

				ReconstructedSpline->SetFlags(RF_Transactional);
				ReconstructedSpline->CopyHoudiniData(SplineComponent);

				UHoudiniInputObject * ReconstructedInputObject = UHoudiniInputHoudiniSplineComponent::Create(
					ReconstructedSpline, GetOuter(), ReconstructedSpline->GetHoudiniSplineName());
				UHoudiniInputHoudiniSplineComponent *ReconstructedHoudiniSplineInput = (UHoudiniInputHoudiniSplineComponent*)ReconstructedInputObject;
				(*InputObjectsPtr)[Index] = ReconstructedHoudiniSplineInput;

				ReconstructedSpline->RegisterComponent();
				ReconstructedSpline->SetFlags(RF_Transactional);

				CreateHoudiniSplineInput(ReconstructedHoudiniSplineInput);

				// Cast the reconstructed Houdini Spline Input to a generic HoudiniInput object.
				UHoudiniInputObject * ReconstructedHoudiniInput = Cast<UHoudiniInputObject>(ReconstructedHoudiniSplineInput);

				LastUndoDeletedInputs.Add(ReconstructedHoudiniInput);
				// Reset the LastInsertedInputsArray for redoing this undo action.
			 }	
		}

		if (bUndoDelete) 
			return;

		// Undo insert input objects action
		for (int Index = 0; Index < LastInsertedInputs.Num(); ++Index) 
		{
			bUndoInsert = true;
			UHoudiniInputHoudiniSplineComponent* SplineInputComponent = LastInsertedInputs[Index];
			if (!SplineInputComponent) continue;

			UHoudiniSplineComponent* HoudiniSplineComponent = SplineInputComponent->GetCurveComponent();
			if (!HoudiniSplineComponent) continue;

			HoudiniSplineComponent->DestroyComponent();
		}

		if (bUndoInsert) 
			return;

		for (int Index = 0; Index < LastUndoDeletedInputs.Num(); ++Index) 
		{
			UHoudiniInputObject* NextInputObject = LastUndoDeletedInputs[Index];

			UHoudiniInputHoudiniSplineComponent* SplineInputComponent = Cast<UHoudiniInputHoudiniSplineComponent>(NextInputObject);
			if (!SplineInputComponent)
				continue;

			UHoudiniSplineComponent* HoudiniSplineComponent = SplineInputComponent->GetCurveComponent();
			if (!HoudiniSplineComponent) 
				continue;

			FDetachmentTransformRules DetachTransRules(EDetachmentRule::KeepRelative, EDetachmentRule::KeepRelative, EDetachmentRule::KeepRelative, false);
			HoudiniSplineComponent->DetachFromComponent(DetachTransRules);

			HoudiniSplineComponent->DestroyComponent();
		}
	 }
}
#endif

FString
UHoudiniInput::InputTypeToString(const EHoudiniInputType& InInputType)
{
	FString InputTypeStr;
	switch (InInputType)
	{
		case EHoudiniInputType::Geometry:
		{
			InputTypeStr = TEXT("Geometry Input");
		}
		break;

		case EHoudiniInputType::Asset:
		{
			InputTypeStr = TEXT("Asset Input");
		}
		break;

		case EHoudiniInputType::Curve:
		{
			InputTypeStr = TEXT("Curve Input");
		}
		break;

		case EHoudiniInputType::Landscape:
		{
			InputTypeStr = TEXT("Landscape Input");
		}
		break;

		case EHoudiniInputType::World:
		{
			InputTypeStr = TEXT("World Outliner Input");
		}
		break;

		case EHoudiniInputType::Skeletal:
		{
			InputTypeStr = TEXT("Skeletal Mesh Input");
		}
		break;
	}

	return InputTypeStr;
}


EHoudiniInputType
UHoudiniInput::StringToInputType(const FString& InInputTypeString)
{
	if (InInputTypeString.StartsWith(TEXT("Geometry"), ESearchCase::IgnoreCase))
	{
		return EHoudiniInputType::Geometry;
	}
	else if (InInputTypeString.StartsWith(TEXT("Asset"), ESearchCase::IgnoreCase))
	{
		return EHoudiniInputType::Asset;
	}
	else if (InInputTypeString.StartsWith(TEXT("Curve"), ESearchCase::IgnoreCase))
	{
		return EHoudiniInputType::Curve;
	}
	else if (InInputTypeString.StartsWith(TEXT("Landscape"), ESearchCase::IgnoreCase))
	{
		return EHoudiniInputType::Landscape;
	}
	else if (InInputTypeString.StartsWith(TEXT("World"), ESearchCase::IgnoreCase))
	{
		return EHoudiniInputType::World;
	}
	else if (InInputTypeString.StartsWith(TEXT("Skeletal"), ESearchCase::IgnoreCase))
	{
		return EHoudiniInputType::Skeletal;
	}

	return EHoudiniInputType::Invalid;
}


EHoudiniCurveType UHoudiniInput::StringToHoudiniCurveType(const FString& HoudiniCurveTypeString) 
{
	if (HoudiniCurveTypeString.StartsWith(TEXT("Linear"), ESearchCase::IgnoreCase))
	{
		return EHoudiniCurveType::Linear;
	}
	else if (HoudiniCurveTypeString.StartsWith(TEXT("Bezier"), ESearchCase::IgnoreCase))
	{
		return EHoudiniCurveType::Bezier;
	}
	else if (HoudiniCurveTypeString.StartsWith(TEXT("Nurbs"), ESearchCase::IgnoreCase))
	{
		return EHoudiniCurveType::Nurbs;
	}
	
	return EHoudiniCurveType::Invalid;
}

EHoudiniCurveMethod UHoudiniInput::StringToHoudiniCurveMethod(const FString& HoudiniCurveMethodString) 
{
	if (HoudiniCurveMethodString.StartsWith(TEXT("CVs"), ESearchCase::IgnoreCase)) 
	{
		return EHoudiniCurveMethod::CVs;
	}
	else if (HoudiniCurveMethodString.StartsWith(TEXT("Breakpoints"), ESearchCase::IgnoreCase)) 
	{
		return EHoudiniCurveMethod::Breakpoints;
	}

	else if (HoudiniCurveMethodString.StartsWith(TEXT("Freehand"), ESearchCase::IgnoreCase))
	{
		return EHoudiniCurveMethod::Freehand;
	}

	return EHoudiniCurveMethod::Invalid;

}

// 
void 
UHoudiniInput::SetSOPInput(const int32& InInputIndex)
{
	// Set the input index
	InputIndex = InInputIndex;

	// Invalidate objpath parameter
	ParmId = -1;
	bIsObjectPathParameter = false;
}

void
UHoudiniInput::SetObjectPathParameter(const int32& InParmId)
{
	// Set as objpath parameter
	ParmId = InParmId;
	bIsObjectPathParameter = true;

	// Invalidate the geo input
	InputIndex = -1;
}

EHoudiniXformType 
UHoudiniInput::GetDefaultXTransformType() 
{
	switch (Type)
	{
		case EHoudiniInputType::Curve:
		case EHoudiniInputType::Geometry:
		case EHoudiniInputType::Skeletal:
			return EHoudiniXformType::None;
		case EHoudiniInputType::Asset:
		case EHoudiniInputType::Landscape:
		case EHoudiniInputType::World:
			return EHoudiniXformType::IntoThisObject;
	}

	return EHoudiniXformType::Auto;
}

bool
UHoudiniInput::GetKeepWorldTransform() const
{
	bool bReturn = false;
	switch (KeepWorldTransform)
	{
		case EHoudiniXformType::Auto:
		{
			// Return default values corresponding to the input type:
			if (Type == EHoudiniInputType::Curve
				|| Type == EHoudiniInputType::Geometry
				|| Type == EHoudiniInputType::Skeletal )
			{
				// NONE  for Geo, Asset and skeletal mesh IN
				bReturn = false;
			}
			else
			{
				// INTO THIS OBJECT for Asset, Landscape and World IN
				bReturn = true;
			}
			break;
		}
		
		case EHoudiniXformType::None:
		{
			bReturn = false;
			break;
		}

		case EHoudiniXformType::IntoThisObject:
		{
			bReturn = true;
			break;
		}
	}

	return bReturn;
}

void
UHoudiniInput::SetKeepWorldTransform(const bool& bInKeepWorldTransform)
{
	if (bInKeepWorldTransform)
	{
		KeepWorldTransform = EHoudiniXformType::IntoThisObject;
	}
	else
	{
		KeepWorldTransform = EHoudiniXformType::None;
	}
}

void 
UHoudiniInput::SetInputType(const EHoudiniInputType& InInputType) 
{ 
	if (InInputType == Type)
		return;

	SetPreviousInputType(Type);

	// Mark this input as changed
	MarkChanged(true);

	// Check previous input type
	switch (PreviousType) 
	{
		case EHoudiniInputType::Asset:
		{
			break;
		}

		case EHoudiniInputType::Curve:
		{
			// detach the input curves from the asset component
			if (GetNumberOfInputObjects() > 0)
			{
				for (UHoudiniInputObject * NextInput : *GetHoudiniInputObjectArray(Type))
				{
					UHoudiniSplineComponent * HoudiniSplineComponent = NextInput 
						? Cast<UHoudiniSplineComponent>(NextInput->GetObject())
						: nullptr;

					if (!HoudiniSplineComponent)
						continue;

					FDetachmentTransformRules DetachTransRules(EDetachmentRule::KeepRelative, EDetachmentRule::KeepRelative, EDetachmentRule::KeepRelative, false);
					HoudiniSplineComponent->DetachFromComponent(DetachTransRules);

					HoudiniSplineComponent->SetVisibility(false, true);
					HoudiniSplineComponent->SetHoudiniSplineVisible(false);
					HoudiniSplineComponent->SetHiddenInGame(true, true);
					HoudiniSplineComponent->SetNodeId(-1);
				}
			}
			break;
		}

		case EHoudiniInputType::Geometry:
		{
			break;
		}

		case EHoudiniInputType::Landscape:
		{
			TArray<UHoudiniInputObject*>* InputObjectsArray = GetHoudiniInputObjectArray(PreviousType);

			if (!InputObjectsArray)
				break;
		
			for (int32 Idx = 0; Idx < InputObjectsArray->Num(); ++Idx)
			{
				UHoudiniInputObject* NextInputObj = (*InputObjectsArray)[Idx];

				if (!NextInputObj)
					continue;

				UHoudiniInputLandscape* NextInputLandscape = Cast<UHoudiniInputLandscape>(NextInputObj);

				if (!NextInputLandscape)
					continue;

				// do something?
			}
		
			break;
		}

		case EHoudiniInputType::Skeletal:
		{
			break;
		}

		case EHoudiniInputType::World:
		{
			break;
		}

		default:
			break;
	}


	Type = InInputType;

	// TODO: NOPE, not needed
	// Set keep world transform to default w.r.t to new input type.
	//KeepWorldTransform = GetDefaultXTransformType();
	
	// Check current input type
	switch (InInputType) 
	{
	case EHoudiniInputType::Asset:
	{
		UHoudiniAssetComponent* OuterHAC = Cast<UHoudiniAssetComponent>(GetOuter());
		if (OuterHAC) 
		{
			for (auto& NextInput : *GetHoudiniInputObjectArray(Type)) 
			{
				UHoudiniInputHoudiniAsset* HoudiniAssetInput = Cast<UHoudiniInputHoudiniAsset>(NextInput);
				if (!HoudiniAssetInput)
					continue;

				UHoudiniAssetComponent* NextHAC = HoudiniAssetInput->GetHoudiniAssetComponent();
				if (!NextHAC || NextHAC->IsPendingKill())
					continue;

				NextHAC->AddDownstreamHoudiniAsset(OuterHAC);
			}
		}
		break;
	}
	case EHoudiniInputType::Curve:
	{
		if (GetNumberOfInputObjects() > 0)
		{
			for (auto& NextInput : *GetHoudiniInputObjectArray(Type))
			{
				UHoudiniInputHoudiniSplineComponent* SplineInput = Cast< UHoudiniInputHoudiniSplineComponent>(NextInput);
				if (!SplineInput)
					continue;

				UHoudiniSplineComponent * HoudiniSplineComponent = SplineInput->GetCurveComponent();
				if (!HoudiniSplineComponent)
					continue;

				USceneComponent* OuterComponent = Cast<USceneComponent>(GetOuter());

				// Attach the new Houdini spline component to it's owner
				HoudiniSplineComponent->RegisterComponent();
				HoudiniSplineComponent->AttachToComponent(OuterComponent, FAttachmentTransformRules::KeepRelativeTransform);
				HoudiniSplineComponent->SetVisibility(true, true);
				HoudiniSplineComponent->SetHoudiniSplineVisible(true);
				HoudiniSplineComponent->SetHiddenInGame(false, true);
				HoudiniSplineComponent->MarkInputObjectChanged();
			}
		}
		else
		{
			// If there is no input for the selected input type, create a new default houdini curve, and
			// wrapped it in an input object.
			CreateHoudiniSplineInput(nullptr);
		}
		
		break;
	}
	case EHoudiniInputType::Geometry:
	{
		break;
	}
	case EHoudiniInputType::Landscape:
	{
		// Need to do anything on select?
		break;
	}
	case EHoudiniInputType::Skeletal:
	{
		break;
	}
	case EHoudiniInputType::World:
	{
		break;
	}


	default:
		break;
	}

}

void
UHoudiniInput::MarkAllInputObjectsChanged(const bool& bInChanged)
{
	MarkDataUploadNeeded(bInChanged);

	// Mark all the objects from this input has changed so they upload themselves
	TArray<UHoudiniInputObject*>* NewInputObjects = GetHoudiniInputObjectArray(Type);
	if (NewInputObjects)
	{
		for (auto CurInputObject : *NewInputObjects)
		{
			if (CurInputObject && !CurInputObject->IsPendingKill())
				CurInputObject->MarkChanged(bInChanged);
		}
	}
}

UHoudiniInputHoudiniSplineComponent*
UHoudiniInput::CreateHoudiniSplineInput(UHoudiniInputHoudiniSplineComponent * FromHoudiniSplineInputComponent, bool bAppendToInputArray) 
{
	UHoudiniInputHoudiniSplineComponent* HoudiniSplineInput = nullptr;
	UHoudiniSplineComponent* HoudiniSplineComponent = nullptr;
	
	USceneComponent* OuterComp = Cast<USceneComponent>(GetOuter());

	if (!FromHoudiniSplineInputComponent || FromHoudiniSplineInputComponent->IsPendingKill())
	{
		// Create a default Houdini spline input if a null pointer is passed in.
		FName HoudiniSplineName = MakeUniqueObjectName(GetOuter(), UHoudiniSplineComponent::StaticClass(), TEXT("Houdini Spline"));

		// Create a Houdini Input Object.
		UHoudiniInputObject * NewInputObject = UHoudiniInputHoudiniSplineComponent::Create(
			nullptr, this, HoudiniSplineName.ToString());

		HoudiniSplineInput = Cast<UHoudiniInputHoudiniSplineComponent>(NewInputObject);
		if (!HoudiniSplineInput)
			return nullptr;

		HoudiniSplineComponent = NewObject<UHoudiniSplineComponent>(
			HoudiniSplineInput,	UHoudiniSplineComponent::StaticClass());

		if (!HoudiniSplineComponent)
			return nullptr;

		HoudiniSplineInput->Update(HoudiniSplineComponent);

		HoudiniSplineComponent->SetHoudiniSplineName(HoudiniSplineName.ToString());
		HoudiniSplineComponent->SetFlags(RF_Transactional);

		// Set the default position of curve to avoid overlapping.
		HoudiniSplineComponent->SetOffset(DefaultCurveOffset);
		DefaultCurveOffset += 100.f;

		HoudiniSplineComponent->RegisterComponent();

		// Attach the new Houdini spline component to it's owner.		
		HoudiniSplineComponent->AttachToComponent(OuterComp, FAttachmentTransformRules::KeepRelativeTransform);

		//push the new input object to the array for new type.
		if (bAppendToInputArray)
			GetHoudiniInputObjectArray(Type)->Add(NewInputObject);
	}
	else 
	{
		// Otherwise, get the Houdini spline, and Houdini spline input from the argument.
		HoudiniSplineInput = FromHoudiniSplineInputComponent;
		HoudiniSplineComponent = FromHoudiniSplineInputComponent->GetCurveComponent();

		if (!HoudiniSplineComponent)
			return nullptr;

		// Attach the new Houdini spline component to it's owner.
		HoudiniSplineComponent->AttachToComponent(OuterComp, FAttachmentTransformRules::KeepRelativeTransform);
	}

	// Mark the created UHoudiniSplineComponent as an input, and set its InputObject.
	HoudiniSplineComponent->SetIsInputCurve(true);
	HoudiniSplineComponent->SetInputObject(HoudiniSplineInput);

	// Set Houdini Spline Component bHasChanged and bNeedsToTrigerUpdate to true.
	HoudiniSplineComponent->MarkInputObjectChanged();

	return HoudiniSplineInput;
}


TArray<UHoudiniInputObject*>*
UHoudiniInput::GetHoudiniInputObjectArray(const EHoudiniInputType& InType)
{
	switch (InType)
	{
	case EHoudiniInputType::Geometry:
		return &GeometryInputObjects;

	case EHoudiniInputType::Curve:
		return &CurveInputObjects;

	case EHoudiniInputType::Asset:
		return &AssetInputObjects;

	case EHoudiniInputType::Landscape:
		return &LandscapeInputObjects;

	case EHoudiniInputType::World:
		return &WorldInputObjects;

	case EHoudiniInputType::Skeletal:
		return &SkeletalInputObjects;

	default:
	case EHoudiniInputType::Invalid:
		return nullptr;
	}

	return nullptr;
}

TArray<AActor*>*
UHoudiniInput::GetBoundSelectorObjectArray()
{
	return &WorldInputBoundSelectorObjects;
}

const TArray<UHoudiniInputObject*>*
UHoudiniInput::GetHoudiniInputObjectArray(const EHoudiniInputType& InType) const
{
	switch (InType)
	{
		case EHoudiniInputType::Geometry:
			return &GeometryInputObjects;

		case EHoudiniInputType::Curve:
			return &CurveInputObjects;

		case EHoudiniInputType::Asset:
			return &AssetInputObjects;

		case EHoudiniInputType::Landscape:
			return &LandscapeInputObjects;

		case EHoudiniInputType::World:
			return &WorldInputObjects;

		case EHoudiniInputType::Skeletal:
			return &SkeletalInputObjects;

		default:
		case EHoudiniInputType::Invalid:
			return nullptr;
	}

	return nullptr;
}

UHoudiniInputObject*
UHoudiniInput::GetHoudiniInputObjectAt(const int32& AtIndex)
{
	return GetHoudiniInputObjectAt(Type, AtIndex);
}

const UHoudiniInputObject*
UHoudiniInput::GetHoudiniInputObjectAt(const int32& AtIndex) const
{
	const TArray<UHoudiniInputObject*>* InputObjectsArray = GetHoudiniInputObjectArray(Type);
	if (!InputObjectsArray || !InputObjectsArray->IsValidIndex(AtIndex))
		return nullptr;

	return (*InputObjectsArray)[AtIndex];
}

UHoudiniInputObject*
UHoudiniInput::GetHoudiniInputObjectAt(const EHoudiniInputType& InType, const int32& AtIndex)
{
	TArray<UHoudiniInputObject*>* InputObjectsArray = GetHoudiniInputObjectArray(InType);
	if (!InputObjectsArray || !InputObjectsArray->IsValidIndex(AtIndex))
		return nullptr;

	return (*InputObjectsArray)[AtIndex];
}

UObject*
UHoudiniInput::GetInputObjectAt(const int32& AtIndex)
{
	return GetInputObjectAt(Type, AtIndex);
}

AActor*
UHoudiniInput::GetBoundSelectorObjectAt(const int32& AtIndex)
{
	if (!WorldInputBoundSelectorObjects.IsValidIndex(AtIndex))
		return nullptr;

	return WorldInputBoundSelectorObjects[AtIndex];
}

UObject*
UHoudiniInput::GetInputObjectAt(const EHoudiniInputType& InType, const int32& AtIndex)
{
	UHoudiniInputObject* HoudiniInputObject = GetHoudiniInputObjectAt(InType, AtIndex);
	if (!HoudiniInputObject)
		return nullptr;

	return HoudiniInputObject->GetObject();
}

void
UHoudiniInput::InsertInputObjectAt(const int32& AtIndex)
{
	InsertInputObjectAt(Type, AtIndex);
}

void
UHoudiniInput::InsertInputObjectAt(const EHoudiniInputType& InType, const int32& AtIndex)
{
	TArray<UHoudiniInputObject*>* InputObjectsPtr = GetHoudiniInputObjectArray(InType);
	if (!InputObjectsPtr)
		return;

	InputObjectsPtr->InsertDefaulted(AtIndex, 1);
	MarkChanged(true);
}

void
UHoudiniInput::DeleteInputObjectAt(const int32& AtIndex)
{
	DeleteInputObjectAt(Type, AtIndex);
}

void
UHoudiniInput::DeleteInputObjectAt(const EHoudiniInputType& InType, const int32& AtIndex)
{
	TArray<UHoudiniInputObject*>* InputObjectsPtr = GetHoudiniInputObjectArray(InType);
	if (!InputObjectsPtr)
		return;

	if (!InputObjectsPtr->IsValidIndex(AtIndex))
		return;

	if (Type == EHoudiniInputType::Asset)
	{
		// ... TODO operations for removing asset input type
	}
	else if (Type == EHoudiniInputType::Curve)
	{
		UHoudiniInputHoudiniSplineComponent * HoudiniSplineInputComponent = (UHoudiniInputHoudiniSplineComponent*)((*InputObjectsPtr)[AtIndex]);
		if (HoudiniSplineInputComponent)
		{
			UHoudiniSplineComponent * HoudiniSplineComponent = HoudiniSplineInputComponent->GetCurveComponent();
			if (HoudiniSplineComponent)
			{
				// detach the input curves from the asset component
				//FDetachmentTransformRules DetachTransRules(EDetachmentRule::KeepRelative, EDetachmentRule::KeepRelative, EDetachmentRule::KeepRelative, false);
				//HoudiniSplineComponent->DetachFromComponent(DetachTransRules);
				// Destory the Houdini Spline Component
				//InputObjectsPtr->RemoveAt(AtIndex);
				HoudiniSplineComponent->DestroyComponent();
			}
		}
	}
	else if (Type == EHoudiniInputType::Geometry) 
	{
		// ... TODO operations for removing geometry input type
	}
	else if (Type == EHoudiniInputType::Landscape) 
	{
		// ... TODO operations for removing landscape input type
	}
	else if (Type == EHoudiniInputType::Skeletal) 
	{
		// ... TODO operations for removing skeletal input type
	}
	else if (Type == EHoudiniInputType::World) 
	{
		// ... TODO operations for removing world input type
	}
	else 
	{
		// ... invalid input type
	}

	MarkChanged(true);

	UHoudiniInputObject* InputObjectToDelete = (*InputObjectsPtr)[AtIndex];
	if (InputObjectToDelete != nullptr)
	{		
		// Mark the input object's nodes for deletion
		InputObjectToDelete->MarkInputNodesForDeletion();

		// If the deleted object wasnt null, trigger a re upload of the input data
		MarkDataUploadNeeded(true);
	}

	InputObjectsPtr->RemoveAt(AtIndex);

	// Delete the merge node when all the input objects are deleted.
	if (InputObjectsPtr->Num() == 0 && InputNodeId >= 0)
	{
		FHoudiniEngineRuntime::Get().MarkNodeIdAsPendingDelete(InputNodeId);
		InputNodeId = -1;
	}
}

void
UHoudiniInput::DuplicateInputObjectAt(const int32& AtIndex)
{
	DuplicateInputObjectAt(Type, AtIndex);
}

void
UHoudiniInput::DuplicateInputObjectAt(const EHoudiniInputType& InType, const int32& AtIndex)
{
	TArray<UHoudiniInputObject*>* InputObjectsPtr = GetHoudiniInputObjectArray(InType);
	if (!InputObjectsPtr)
		return;

	if (!InputObjectsPtr->IsValidIndex(AtIndex))
		return;

	// If the duplicated object is not null, trigger a re upload of the input data
	bool bTriggerUpload = (*InputObjectsPtr)[AtIndex] != nullptr;

	// TODO: Duplicate the UHoudiniInputObject!!
	UHoudiniInputObject* DuplicateInput = (*InputObjectsPtr)[AtIndex];
	InputObjectsPtr->Insert(DuplicateInput, AtIndex);

	MarkChanged(true);

	if (bTriggerUpload)
		MarkDataUploadNeeded(true);
}

int32
UHoudiniInput::GetNumberOfInputObjects()
{
	return GetNumberOfInputObjects(Type);
}

int32
UHoudiniInput::GetNumberOfInputObjects(const EHoudiniInputType& InType)
{
	TArray<UHoudiniInputObject*>* InputObjectsPtr = GetHoudiniInputObjectArray(InType);
	if (!InputObjectsPtr)
		return 0;
	
	// TODO?
	// If geometry input, and we only have one null object, return 0

	return InputObjectsPtr->Num();
}

int32
UHoudiniInput::GetNumberOfBoundSelectorObjects() const
{
	return WorldInputBoundSelectorObjects.Num();
}

void
UHoudiniInput::SetInputObjectAt(const int32& AtIndex, UObject* InObject)
{
	return SetInputObjectAt(Type, AtIndex, InObject);
}

void
UHoudiniInput::SetInputObjectAt(const EHoudiniInputType& InType, const int32& AtIndex, UObject* InObject)
{
	// Start by making sure we have the proper number of input objects
	int32 NumIntObject = GetNumberOfInputObjects(InType);
	if (NumIntObject <= AtIndex)
	{
		// We need to resize the array
		SetInputObjectsNumber(InType, AtIndex + 1);
	}

	UObject* CurrentInputObject = GetInputObjectAt(InType, AtIndex);
	if (CurrentInputObject == InObject)
	{
		// Nothing to do
		return;
	}

	UHoudiniInputObject* CurrentInputObjectWrapper = GetHoudiniInputObjectAt(InType, AtIndex);
	if (!InObject)
	{
		// We want to set the input object to null
		TArray<UHoudiniInputObject*>* InputObjectsPtr = GetHoudiniInputObjectArray(InType);		
		if (!ensure(InputObjectsPtr != nullptr && InputObjectsPtr->IsValidIndex(AtIndex)))
			return;

		if (CurrentInputObjectWrapper)
		{
			// TODO: Check this case
			// Do not destroy the input object manually! this messes up GC
			//CurrentInputObjectWrapper->ConditionalBeginDestroy();
			MarkDataUploadNeeded(true);
		}

		(*InputObjectsPtr)[AtIndex] = nullptr;
		return;
	}

	// Get the type of the previous and new input objects
	EHoudiniInputObjectType NewObjectType = InObject ? UHoudiniInputObject::GetInputObjectTypeFromObject(InObject) : EHoudiniInputObjectType::Invalid;
	EHoudiniInputObjectType CurrentObjectType = CurrentInputObjectWrapper ? CurrentInputObjectWrapper->Type : EHoudiniInputObjectType::Invalid;

	// See if we can reuse the existing InputObject		
	if (CurrentObjectType == NewObjectType && NewObjectType != EHoudiniInputObjectType::Invalid)
	{
		// The InputObjectTypes match, we can just update the existing object
		CurrentInputObjectWrapper->Update(InObject);
		CurrentInputObjectWrapper->MarkChanged(true);
		return;
	}

	// Destroy the existing input object
	TArray<UHoudiniInputObject*>* InputObjectsPtr = GetHoudiniInputObjectArray(InType);
	if (!ensure(InputObjectsPtr))
		return;

	UHoudiniInputObject* NewInputObject = UHoudiniInputObject::CreateTypedInputObject(InObject, GetOuter(), FString::FromInt(AtIndex + 1));
	if (!ensure(NewInputObject))
		return;

	// Mark that input object as changed so we know we need to update it
	NewInputObject->MarkChanged(true);
	if (CurrentInputObjectWrapper != nullptr)
	{
		// TODO:
		// For some input type, we may have to copy some of the previous object's property before deleting it

		// Delete the previous object
		CurrentInputObjectWrapper->MarkPendingKill();
		(*InputObjectsPtr)[AtIndex] = nullptr;
	}

	// Update the input object array with the newly created input object
	(*InputObjectsPtr)[AtIndex] = NewInputObject;
}

void
UHoudiniInput::SetInputObjectsNumber(const EHoudiniInputType& InType, const int32& InNewCount)
{
	TArray<UHoudiniInputObject*>* InputObjectsPtr = GetHoudiniInputObjectArray(InType);
	if (!InputObjectsPtr)
		return;

	if (InputObjectsPtr->Num() == InNewCount)
	{
		// Nothing to do
		return;
	}

	if (InNewCount > InputObjectsPtr->Num())
	{
		// Simply add new default InputObjects
		InputObjectsPtr->SetNum(InNewCount);
	}
	else
	{
		// TODO: Check this case!
		// Do not destroy the input object themselves manually,
		// destroy the input object's nodes and reduce the array's size  
		for (int32 InObjIdx = InputObjectsPtr->Num() - 1; InObjIdx >= InNewCount; InObjIdx--)
		{
			UHoudiniInputObject* CurrentInputObject = (*InputObjectsPtr)[InObjIdx];
			if (!CurrentInputObject)
				continue;
			
			CurrentInputObject->MarkInputNodesForDeletion();

			/*/
			//FHoudiniInputTranslator::DestroyInput(Inputs[InputIdx]);
			CurrentObject->ConditionalBeginDestroy();
			(*InputObjectsPtr)[InObjIdx] = nullptr;
			*/
		}
		
		// Decrease the input object array size
		InputObjectsPtr->SetNum(InNewCount);
	}

	// Also delete the input's merge node when all the input objects are deleted.
	if (InNewCount == 0 && InputNodeId >= 0)
	{
		FHoudiniEngineRuntime::Get().MarkNodeIdAsPendingDelete(InputNodeId, true);
		InputNodeId = -1;
	}
}

void
UHoudiniInput::SetBoundSelectorObjectsNumber(const int32& InNewCount)
{
	if (WorldInputBoundSelectorObjects.Num() == InNewCount)
	{
		// Nothing to do
		return;
	}

	if (InNewCount > WorldInputBoundSelectorObjects.Num())
	{
		// Simply add new default InputObjects
		WorldInputBoundSelectorObjects.SetNum(InNewCount);
	}
	else
	{
		/*
		// TODO: Not Needed?
		// Do not destroy the input object themselves manually,
		// destroy the input object's nodes and reduce the array's size  
		for (int32 InObjIdx = WorldInputBoundSelectorObjects.Num() - 1; InObjIdx >= InNewCount; InObjIdx--)
		{
			UHoudiniInputObject* CurrentInputObject = WorldInputBoundSelectorObjects[InObjIdx];
			if (!CurrentInputObject)
				continue;

			CurrentInputObject->MarkInputNodesForDeletion();
		}
		*/

		// Decrease the input object array size
		WorldInputBoundSelectorObjects.SetNum(InNewCount);
	}
}

void
UHoudiniInput::SetBoundSelectorObjectAt(const int32& AtIndex, AActor* InActor)
{
	// Start by making sure we have the proper number of objects
	int32 NumIntObject = GetNumberOfBoundSelectorObjects();
	if (NumIntObject <= AtIndex)
	{
		// We need to resize the array
		SetBoundSelectorObjectsNumber(AtIndex + 1);
	}

	AActor* CurrentActor = GetBoundSelectorObjectAt(AtIndex);
	if (CurrentActor == InActor)
	{
		// Nothing to do
		return;
	}

	// Update the array with the new object
	WorldInputBoundSelectorObjects[AtIndex] = InActor;
}

// Helper function indicating what classes are supported by an input type
TArray<const UClass*>
UHoudiniInput::GetAllowedClasses(const EHoudiniInputType& InInputType)
{
	TArray<const UClass*> AllowedClasses;
	switch (InInputType)
	{
		case EHoudiniInputType::Geometry:
			AllowedClasses.Add(UStaticMesh::StaticClass());
			AllowedClasses.Add(USkeletalMesh::StaticClass());
			break;

		case EHoudiniInputType::Curve:
			AllowedClasses.Add(USplineComponent::StaticClass());
			AllowedClasses.Add(UHoudiniSplineComponent::StaticClass());
			break;

		case EHoudiniInputType::Asset:
			AllowedClasses.Add(UHoudiniAssetComponent::StaticClass());
			break;

		case EHoudiniInputType::Landscape:
			AllowedClasses.Add(ALandscapeProxy::StaticClass());
			break;

		case EHoudiniInputType::World:
			AllowedClasses.Add(AActor::StaticClass());
			break;

		case EHoudiniInputType::Skeletal:
			AllowedClasses.Add(USkeletalMesh::StaticClass());
			break;

		default:
			break;
	}

	return AllowedClasses;
}

// Helper function indicating if an object is supported by an input type
bool 
UHoudiniInput::IsObjectAcceptable(const EHoudiniInputType& InInputType, const UObject* InObject)
{
	TArray<const UClass*> AllowedClasses = GetAllowedClasses(InInputType);
	for (auto CurClass : AllowedClasses)
	{
		if (InObject->IsA(CurClass))
			return true;
	}

	return false;
}

bool
UHoudiniInput::IsDataUploadNeeded()
{
	if (bDataUploadNeeded)
		return true;

	return HasChanged();
}

// Indicates if this input has changed and should be updated
bool 
UHoudiniInput::HasChanged()
{
	if (bHasChanged)
		return true;

	TArray<UHoudiniInputObject*>* InputObjectsPtr = GetHoudiniInputObjectArray(Type);
	if (!ensure(InputObjectsPtr))
		return false;

	for (auto CurrentInputObject : (*InputObjectsPtr))
	{
		if (CurrentInputObject && CurrentInputObject->HasChanged())
			return true;
	}

	return false;
}

bool
UHoudiniInput::IsTransformUploadNeeded()
{
	TArray<UHoudiniInputObject*>* InputObjectsPtr = GetHoudiniInputObjectArray(Type);
	if (!ensure(InputObjectsPtr))
		return false;

	for (auto CurrentInputObject : (*InputObjectsPtr))
	{
		if (CurrentInputObject && CurrentInputObject->HasTransformChanged())
			return true;
	}

	return false;
}

// Indicates if this input needs to trigger an update
bool
UHoudiniInput::NeedsToTriggerUpdate()
{
	if (bNeedsToTriggerUpdate)
		return true;

	const TArray<UHoudiniInputObject*>* InputObjectsPtr = GetHoudiniInputObjectArray(Type);
	if (!ensure(InputObjectsPtr))
		return false;

	for (auto CurrentInputObject : (*InputObjectsPtr))
	{
		if (CurrentInputObject && CurrentInputObject->NeedsToTriggerUpdate())
			return true;
	}

	return false;
}

FString 
UHoudiniInput::GetNodeBaseName() const
{
	UHoudiniAssetComponent* HAC = Cast<UHoudiniAssetComponent>(GetOuter());
	FString NodeBaseName = HAC ? HAC->GetDisplayName() : TEXT("HoudiniAsset");

	// Unfortunately CreateInputNode always prefix with input_...
	NodeBaseName += TEXT("_input") + FString::FromInt(GetInputIndex());
	if (IsObjectPathParameter())
		NodeBaseName += TEXT("_") + GetName();		

	return NodeBaseName;
}

void
UHoudiniInput::OnTransformUIExpand(const int32& AtIndex)
{
#if WITH_EDITORONLY_DATA
	if (TransformUIExpanded.IsValidIndex(AtIndex))
	{
		TransformUIExpanded[AtIndex] = !TransformUIExpanded[AtIndex];
	}
	else
	{
		// We need to append values to the expanded array
		for (int32 Index = TransformUIExpanded.Num(); Index <= AtIndex; Index++)
		{
			TransformUIExpanded.Add(Index == AtIndex ? true : false);
		}
	}
#endif
}

bool
UHoudiniInput::IsTransformUIExpanded(const int32& AtIndex)
{
#if WITH_EDITORONLY_DATA
	return TransformUIExpanded.IsValidIndex(AtIndex) ? TransformUIExpanded[AtIndex] : false;
#else
	return false;
#endif
};

FTransform*
UHoudiniInput::GetTransformOffset(const int32& AtIndex) 
{
	UHoudiniInputObject* InObject = GetHoudiniInputObjectAt(AtIndex);
	if (InObject)
		return &(InObject->Transform);

	return nullptr;
}

const FTransform
UHoudiniInput::GetTransformOffset(const int32& AtIndex) const
{
	const UHoudiniInputObject* InObject = GetHoudiniInputObjectAt(AtIndex);
	if (InObject)
		return InObject->Transform;

	return FTransform::Identity;
}

TOptional<float>
UHoudiniInput::GetPositionOffsetX(int32 AtIndex) const
{
	return GetTransformOffset(AtIndex).GetLocation().X;
}

TOptional<float>
UHoudiniInput::GetPositionOffsetY(int32 AtIndex) const
{
	return GetTransformOffset(AtIndex).GetLocation().Y;
}

TOptional<float>
UHoudiniInput::GetPositionOffsetZ(int32 AtIndex) const
{
	return GetTransformOffset(AtIndex).GetLocation().Z;
}

TOptional<float>
UHoudiniInput::GetRotationOffsetRoll(int32 AtIndex) const
{
	return GetTransformOffset(AtIndex).Rotator().Roll;
}

TOptional<float>
UHoudiniInput::GetRotationOffsetPitch(int32 AtIndex) const
{
	return GetTransformOffset(AtIndex).Rotator().Pitch;
}

TOptional<float>
UHoudiniInput::GetRotationOffsetYaw(int32 AtIndex) const
{
	return GetTransformOffset(AtIndex).Rotator().Yaw;
}

TOptional<float>
UHoudiniInput::GetScaleOffsetX(int32 AtIndex) const
{
	return GetTransformOffset(AtIndex).GetScale3D().X;
}

TOptional<float>
UHoudiniInput::GetScaleOffsetY(int32 AtIndex) const
{
	return GetTransformOffset(AtIndex).GetScale3D().Y;
}

TOptional<float>
UHoudiniInput::GetScaleOffsetZ(int32 AtIndex) const
{
	return GetTransformOffset(AtIndex).GetScale3D().Z;
}

bool
UHoudiniInput::SetTransformOffsetAt(const float& Value, const int32& AtIndex, const int32& PosRotScaleIndex, const int32& XYZIndex)
{
	FTransform* Transform = GetTransformOffset(AtIndex);
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
	bStaticMeshChanged = true;

	return true;
}

FText 
UHoudiniInput::GetCurrentSelectionText() const 
{
	FText CurrentSelectionText;
	switch (Type) 
	{
		case EHoudiniInputType::Landscape :
		{
			if (LandscapeInputObjects.Num() > 0) 
			{
				UHoudiniInputObject* InputObject = LandscapeInputObjects[0];

				UHoudiniInputLandscape* InputLandscape = Cast<UHoudiniInputLandscape>(InputObject);
				if (!InputLandscape)
					return CurrentSelectionText;

				ALandscapeProxy* LandscapeProxy = InputLandscape->GetLandscapeProxy();
				if (!LandscapeProxy || LandscapeProxy->IsPendingKill())
					return CurrentSelectionText;

				CurrentSelectionText = FText::FromString(LandscapeProxy->GetName());
			}	
		}
		break;

		case EHoudiniInputType::Asset :
		{
			if (AssetInputObjects.Num() > 0) 
			{
				UHoudiniInputObject* InputObject = AssetInputObjects[0];

				UHoudiniInputHoudiniAsset* HoudiniAssetInput = Cast<UHoudiniInputHoudiniAsset>(InputObject);
				if (!HoudiniAssetInput)
					return CurrentSelectionText;

				UHoudiniAssetComponent* HAC = HoudiniAssetInput->GetHoudiniAssetComponent();
				if (!HAC || HAC->IsPendingKill())
					return CurrentSelectionText;

				UHoudiniAsset* HoudiniAsset = HAC->GetHoudiniAsset();
				if (!HoudiniAsset || HoudiniAsset->IsPendingKill())
					return CurrentSelectionText;

				CurrentSelectionText = FText::FromString(HoudiniAsset->GetName());
			}
		}
		break;

		default:
		break;
	}
	
	return CurrentSelectionText;
}


bool 
UHoudiniInput::HasLandscapeExportTypeChanged () const 
{
	if (Type != EHoudiniInputType::Landscape)
		return false;

	return bLandscapeHasExportTypeChanged;
}

void 
UHoudiniInput::SetHasLandscapeExportTypeChanged(const bool InChanged) 
{
	if (Type != EHoudiniInputType::Landscape)
		return;

	bLandscapeHasExportTypeChanged = InChanged;
}

bool 
UHoudiniInput::GetUpdateInputLandscape() const 
{
	if (Type != EHoudiniInputType::Landscape)
		return false;

	return bUpdateInputLandscape;
}

void 
UHoudiniInput::SetUpdateInputLandscape(const bool bInUpdateInputLandcape)
{
	if (Type != EHoudiniInputType::Landscape)
		return;

	bUpdateInputLandscape = bInUpdateInputLandcape;
}


bool
UHoudiniInput::UpdateWorldSelectionFromBoundSelectors()
{
	// Dont do anything if we're not a World Input
	if (Type != EHoudiniInputType::World)
		return false;

	// Build an array of the current selection's bounds
	TArray<FBox> AllBBox;
	for (auto CurrentActor : WorldInputBoundSelectorObjects)
	{
		if (!CurrentActor || CurrentActor->IsPendingKill())
			continue;

		AllBBox.Add(CurrentActor->GetComponentsBoundingBox(true, true));
	}

	//
	// Select all actors in our bound selectors bounding boxes
	//

	// Get our parent component/actor
	USceneComponent* ParentComponent = Cast<USceneComponent>(GetOuter());
	AActor* ParentActor = ParentComponent ? ParentComponent->GetOwner() : nullptr;

	//UWorld* editorWorld = GEditor->GetEditorWorldContext().World();
	UWorld* MyWorld = GetWorld();
	TArray<AActor*> NewSelectedActors;
	for (TActorIterator<AActor> ActorItr(MyWorld); ActorItr; ++ActorItr)
	{
		AActor *CurrentActor = *ActorItr;
		if (!CurrentActor || CurrentActor->IsPendingKill())
			continue;

		// Check that actor is currently not selected
		if (WorldInputBoundSelectorObjects.Contains(CurrentActor))
			continue;

		// Ignore the SkySpheres?
		FString ClassName = CurrentActor->GetClass() ? CurrentActor->GetClass()->GetName() : FString();
		if (ClassName.Contains("BP_Sky_Sphere"))
			continue;

		// Don't allow selection of ourselves. Bad things happen if we do.
		if (ParentActor && (CurrentActor == ParentActor))
			continue;

		// For BrushActors, both the actor and its brush must be valid
		ABrush* BrushActor = Cast<ABrush>(CurrentActor);
		if (BrushActor)
		{
			if (!BrushActor->Brush || BrushActor->Brush->IsPendingKill())
				continue;
		}

		FBox ActorBounds = CurrentActor->GetComponentsBoundingBox(true);
		for (auto InBounds : AllBBox)
		{
			// Check if both actor's bounds intersects
			if (!ActorBounds.Intersect(InBounds))
				continue;

			NewSelectedActors.Add(CurrentActor);
			break;
		}
	}
	
	return UpdateWorldSelection(NewSelectedActors);
}

bool
UHoudiniInput::UpdateWorldSelection(const TArray<AActor*>& InNewSelection)
{
	TArray<AActor*> NewSelectedActors = InNewSelection;

	// Update our current selection with the new one
	// Keep actors that are still selected, remove the one that are not selected anymore
	bool bHasSelectionChanged = false;
	for (int32 Idx = WorldInputObjects.Num() - 1; Idx >= 0; Idx--)
	{
		UHoudiniInputActor* InputActor = Cast<UHoudiniInputActor>(WorldInputObjects[Idx]);
		AActor* CurActor = InputActor ? InputActor->GetActor() : nullptr;

		if (CurActor && NewSelectedActors.Contains(CurActor))
		{
			// The actor is still selected, remove it from the new selection
			NewSelectedActors.Remove(CurActor);
		}
		else
		{
			// That actor is no longer selected, remove itr from our current selection
			DeleteInputObjectAt(EHoudiniInputType::World, Idx);
			bHasSelectionChanged = true;
		}
	}

	if (NewSelectedActors.Num() > 0)
		bHasSelectionChanged = true;

	// Then add the newly selected Actors
	int32 InputObjectIdx = GetNumberOfInputObjects(EHoudiniInputType::World);
	int32 NewInputObjectNumber = InputObjectIdx + NewSelectedActors.Num();
	SetInputObjectsNumber(EHoudiniInputType::World, NewInputObjectNumber);
	for (const auto& CurActor : NewSelectedActors)
	{
		// Update the input objects from the valid selected actors array
		SetInputObjectAt(InputObjectIdx++, CurActor);
	}

	MarkChanged(bHasSelectionChanged);

	return bHasSelectionChanged;
}


bool
UHoudiniInput::ContainsInputObject(const UObject* InObject, const EHoudiniInputType& InType) const
{
	if (!InObject || InObject->IsPendingKill())
		return false;

	// Returns true if the object is one of our input object for the given type
	const TArray<UHoudiniInputObject*>* ObjectArray = GetHoudiniInputObjectArray(InType);
	if (!ObjectArray)
		return false;

	for (auto& CurrentInputObject : (*ObjectArray))
	{
		if (!CurrentInputObject || CurrentInputObject->IsPendingKill())
			continue;

		if (CurrentInputObject->GetObject() == InObject)
			return true;
	}

	return false;
}