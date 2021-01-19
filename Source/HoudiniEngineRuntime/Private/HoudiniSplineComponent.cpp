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

#include "HoudiniSplineComponent.h"

#include "HoudiniEngineRuntimePrivatePCH.h"
#include "HoudiniAssetComponent.h"
#include "HoudiniEngineRuntime.h"
#include "HoudiniEngineRuntimeUtils.h"
#include "HoudiniInput.h"
#include "HoudiniInputObject.h"
#include "HoudiniPluginSerializationVersion.h"

#include "Components/MeshComponent.h"
#include "Algo/Reverse.h"

#include "HoudiniPluginSerializationVersion.h"
#include "HoudiniCompatibilityHelpers.h"

#include "UObject/DevObjectVersion.h"
#include "Serialization/CustomVersion.h"

void
UHoudiniSplineComponent::Serialize(FArchive& Ar)
{
	int64 InitialOffset = Ar.Tell();

	bool bLegacyComponent = false;
	if (Ar.IsLoading())
	{
		int32 Ver = Ar.CustomVer(FHoudiniCustomSerializationVersion::GUID);
		if (Ver < VER_HOUDINI_PLUGIN_SERIALIZATION_VERSION_V2_BASE && Ver >= VER_HOUDINI_PLUGIN_SERIALIZATION_VERSION_BASE)
		{
			bLegacyComponent = true;
		}
	}

	if (bLegacyComponent)
	{
		// Legacy serialization
		// Either try to convert or skip depending on HOUDINI_ENGINE_ENABLE_BACKWARD_COMPATIBILITY 
		const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault<UHoudiniRuntimeSettings>();
		bool bEnableBackwardCompatibility = HoudiniRuntimeSettings->bEnableBackwardCompatibility;
		if (bEnableBackwardCompatibility)
		{
			HOUDINI_LOG_WARNING(TEXT("Loading deprecated version of UHoudiniSplineComponent : converting v1 object to v2."));

			Super::Serialize(Ar);

			UHoudiniSplineComponent_V1* CompatibilitySC = NewObject<UHoudiniSplineComponent_V1>();
			CompatibilitySC->Serialize(Ar);
			CompatibilitySC->UpdateFromLegacyData(this);

			Construct(CompatibilitySC->CurveDisplayPoints);
		}
		else
		{
			HOUDINI_LOG_WARNING(TEXT("Loading deprecated version of UHoudiniSplineComponent : serialization will be skipped."));

			Super::Serialize(Ar);

			// Skip v1 Serialized data
			if (FLinker* Linker = Ar.GetLinker())
			{
				int32 const ExportIndex = this->GetLinkerIndex();
				FObjectExport& Export = Linker->ExportMap[ExportIndex];
				Ar.Seek(InitialOffset + Export.SerialSize);
				return;
			}
		}
	}
	else
	{
		// Normal v2 serialization
		Super::Serialize(Ar);
	}
}

UHoudiniSplineComponent::UHoudiniSplineComponent(const FObjectInitializer & ObjectInitializer)
	: Super(ObjectInitializer)
	, bClosed(false)
	, bReversed(false)
	, bIsHoudiniSplineVisible(true)
	, CurveType(EHoudiniCurveType::Polygon)
	, CurveMethod(EHoudiniCurveMethod::CVs)
	, bHasChanged(false)
	, bNeedsToTriggerUpdate(false)
	, bIsInputCurve(false)
	, bIsEditableOutputCurve(false)
	, NodeId(-1)
{

	// Add two default points to the curve
	FTransform defaultPoint = FTransform::Identity;

	// Set this component to not tick?
	// SetComponentTickEnabled(false);

	// Default curve.
	CurvePoints.Add(defaultPoint);
	DisplayPoints.Add(defaultPoint.GetLocation());

	defaultPoint.SetTranslation(FVector(200.f, 0.f, 0.f));
	CurvePoints.Add(defaultPoint);
	DisplayPoints.Add(defaultPoint.GetLocation());

	bIsOutputCurve = false;
	bCookOnCurveChanged = true;

#if WITH_EDITOR
	bPostUndo = false;
#endif
}

void 
UHoudiniSplineComponent::Construct(TArray<FVector>& InCurveDisplayPoints, int32 InsertedPoint) 
{
	DisplayPoints.Empty();
	DisplayPointIndexDivider.Empty();

	float DisplayPointStepSize;

	// Resample the display points for linear curve.

	if (InCurveDisplayPoints.Num() <= 0)
		return;

	// Add an additional displaypoint to the end for closed curve
	if (bClosed && InCurveDisplayPoints.Num() > 2)
	{
		FVector & FirstPoint = InCurveDisplayPoints[0];
		FVector ClosingPoint;
		ClosingPoint.X = FirstPoint.X;
		ClosingPoint.Y = FirstPoint.Y;
		ClosingPoint.Z = FirstPoint.Z;

		InCurveDisplayPoints.Add(ClosingPoint);
	}
	

	if (CurveType == EHoudiniCurveType::Polygon) 
	{
		FVector Pt1, Pt2;
		Pt1 = InCurveDisplayPoints[0];
		int32 CurrentDisplayPointIndex = 0;

		for (int Index = 1; Index < InCurveDisplayPoints.Num(); ++Index)
		{
			DisplayPointStepSize = 10.f;

			Pt2 = InCurveDisplayPoints[Index];

			FVector Direction = Pt2 - Pt1;

			float SegmentLength = Direction.Size();

			int32 NumOfDisplayPt = SegmentLength / DisplayPointStepSize;

			// Make sure there are at least 20 display points on a segment.
			while( NumOfDisplayPt < 20 && SegmentLength > 0.01) 
			{
				DisplayPointStepSize /= 2.f;
				NumOfDisplayPt = SegmentLength / DisplayPointStepSize;
			}

			Direction.Normalize(0.01f);

			FVector StepVector = Direction * DisplayPointStepSize;

			// Always add the start point of a line segment 
			FVector NextDisplayPt = Pt1;
			if (NumOfDisplayPt == 0) DisplayPoints.Add(NextDisplayPt);

			for (int32 itr = 0; itr < NumOfDisplayPt; ++itr) 
			{
				DisplayPoints.Add(NextDisplayPt);
				NextDisplayPt += StepVector;
				CurrentDisplayPointIndex += 1;
			}

			DisplayPointIndexDivider.Add(CurrentDisplayPointIndex);

			Pt1 = Pt2;
		}

		// Add the ending point
		DisplayPoints.Add(Pt1);
		// Duplicate the last index, to make the DisplaPointyIndexDivider array matches the length of DP array
		DisplayPointIndexDivider.Add(CurrentDisplayPointIndex + 1);
	}
	else if (CurveType == EHoudiniCurveType::Points) 
	{
		// do not add display points for Points curve type, just show the CVs
	}
	else
	{
		// Needs a better algorithm to divide the display points
		// Refined display points does not strictly interpolate the curve points.

		FVector Pt1, Pt2;
		Pt1 = InCurveDisplayPoints[0];

		int Itr = 1;

		int32 ClosestIndex = -1;
		float ClosestDistance = -1.f;

		for (int32 Index = 1; Index < InCurveDisplayPoints.Num(); ++Index) 
		{
			if (Itr >= CurvePoints.Num()) break;

			Pt2 = InCurveDisplayPoints[Index];

			FVector ClosestPointOnSegment = FMath::ClosestPointOnSegment(CurvePoints[Itr].GetLocation(), Pt1, Pt2);
	
			float Distance = (ClosestPointOnSegment - CurvePoints[Itr].GetLocation()).Size();

			if (ClosestDistance < 0.f || Distance < ClosestDistance) 
			{
				ClosestDistance = Distance;
				ClosestIndex = Index;
			}
			else 
			{
				Itr += 1;
				if (Itr >= CurvePoints.Num()) break;

				DisplayPointIndexDivider.Add(Index-1);
				
				ClosestPointOnSegment = FMath::ClosestPointOnSegment(CurvePoints[Itr].GetLocation(), Pt1, Pt2);
				ClosestDistance = (ClosestPointOnSegment - CurvePoints[Itr].GetLocation()).Size();

			}
		}

		DisplayPointIndexDivider.Add(InCurveDisplayPoints.Num());

		DisplayPoints = InCurveDisplayPoints;
	}
}


void 
UHoudiniSplineComponent::CopyHoudiniData(const UHoudiniSplineComponent* OtherHoudiniSplineComponent) 
{
	if (!OtherHoudiniSplineComponent) 
		return;

	CurvePoints = OtherHoudiniSplineComponent->CurvePoints;
	DisplayPoints = OtherHoudiniSplineComponent->DisplayPoints;
	DisplayPointIndexDivider = OtherHoudiniSplineComponent->DisplayPointIndexDivider;
	CurveType = OtherHoudiniSplineComponent->CurveType;
	CurveMethod = OtherHoudiniSplineComponent->CurveMethod;
	bClosed = OtherHoudiniSplineComponent->bClosed;
#if WITH_EDITORONLY_DATA
	bVisualizeComponent = OtherHoudiniSplineComponent->bVisualizeComponent;
#endif
	bReversed = OtherHoudiniSplineComponent->bReversed;
	SetVisibility(OtherHoudiniSplineComponent->IsVisible());

	HoudiniSplineName = OtherHoudiniSplineComponent->HoudiniSplineName;
}

UHoudiniSplineComponent::~UHoudiniSplineComponent()
{}

void 
UHoudiniSplineComponent::AppendPoint(const FTransform& NewPoint)
{
	CurvePoints.Add(NewPoint);
}

void
UHoudiniSplineComponent::InsertPointAtIndex(const FTransform& NewPoint, const int32& Index)
{
	check(Index >= 0 && Index < CurvePoints.Num());
	CurvePoints.Insert(NewPoint, Index);
	bHasChanged = true;
}


void
UHoudiniSplineComponent::RemovePointAtIndex(const int32& Index)
{
	check(Index >= 0 && Index < CurvePoints.Num());
	CurvePoints.RemoveAt(Index);
	bHasChanged = true;
}

void 
UHoudiniSplineComponent::SetReversed(const bool& InReversed) 
{
	// don't need to do anything if the reversed state doesn't change.
	if (InReversed == bReversed)
		return;

	bReversed = InReversed;
	ReverseCurvePoints();
	MarkChanged(true);
}

void
UHoudiniSplineComponent::ReverseCurvePoints() 
{
	if (CurvePoints.Num() < 2)
		return;

	Algo::Reverse(CurvePoints);
}

void 
UHoudiniSplineComponent::EditPointAtindex(const FTransform& NewPoint, const int32& Index) 
{
	if (!CurvePoints.IsValidIndex(Index))
		return;

	CurvePoints[Index] = NewPoint;
	bHasChanged = true;
}

#if WITH_EDITOR
void 
UHoudiniSplineComponent::PostEditChangeProperty(FPropertyChangedEvent& PeopertyChangedEvent) 
{
	Super::PostEditChangeProperty(PeopertyChangedEvent);

	FName PropertyName = (PeopertyChangedEvent.Property != nullptr) ? PeopertyChangedEvent.Property->GetFName() : NAME_None;

	// Responses to the uproperty changes
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UHoudiniSplineComponent, bClosed)) 
	{
		UE_LOG(LogTemp, Warning, TEXT("UPROPERTY Changed : bClosed"));
		MarkChanged(true);
	}

	if (PropertyName == GET_MEMBER_NAME_CHECKED(UHoudiniSplineComponent, bReversed))
	{
		UE_LOG(LogTemp, Warning, TEXT("UPROPERTY Changed : bReversed"));
		ReverseCurvePoints();
		MarkChanged(true);
	}

	if (PropertyName == GET_MEMBER_NAME_CHECKED(UHoudiniSplineComponent, CurveType)) 
	{
		UE_LOG(LogTemp, Warning, TEXT("UPROPERTY Changed : CurveType"));
		MarkChanged(true);
	}

	if (PropertyName == GET_MEMBER_NAME_CHECKED(UHoudiniSplineComponent, CurveMethod)) 
	{
		UE_LOG(LogTemp, Warning, TEXT("UPROPERTY Changed : CurveMethod"));
		MarkChanged(true);
	}
}
#endif

void
UHoudiniSplineComponent::PostLoad() 
{
	Super::PostLoad();
	HOUDINI_LOG_DISPLAY(TEXT("[UHoudiniSplineComponent::PostLoad()] Component: %s"), *GetPathName());

}

TStructOnScope<FActorComponentInstanceData> 
UHoudiniSplineComponent::GetComponentInstanceData() const
{
	HOUDINI_LOG_DISPLAY(TEXT("[UHoudiniSplineComponent::GetComponentInstanceData()] Component: %s"), *GetPathName());
	TStructOnScope<FActorComponentInstanceData> ComponentInstanceData = MakeStructOnScope<FActorComponentInstanceData, FHoudiniSplineComponentInstanceData>(this);
	FHoudiniSplineComponentInstanceData* InstanceData = ComponentInstanceData.Cast<FHoudiniSplineComponentInstanceData>();


	// NOTE: We need to capture these properties here before the component gets torn down
	// since the Spline visualizer changed values on the instance directly and is not present on the
	// template yet.
	/*InstanceData->CurvePoints = CurvePoints;
	InstanceData->DisplayPoints = DisplayPoints;
	InstanceData->DisplayPointIndexDivider = DisplayPointIndexDivider;

#if WITH_EDITOR_DATA
	InstanceData->EditedControlPointsIndexes = EditedControlPointsIndexes;
#endif*/

	return ComponentInstanceData;
}

void 
UHoudiniSplineComponent::ApplyComponentInstanceData(FHoudiniSplineComponentInstanceData* ComponentInstanceData,
                                                         const bool bPostUCS)
{
	HOUDINI_LOG_DISPLAY(TEXT("[UHoudiniSplineComponent::ApplyComponentInstanceData()] Component: %s"), *GetPathName());
	HOUDINI_LOG_DISPLAY(TEXT("[UHoudiniSplineComponent::ApplyComponentInstanceData()] Component: %p"), this);
	HOUDINI_LOG_DISPLAY(TEXT("[UHoudiniSplineComponent::ApplyComponentInstanceData()] IsVisible: %d"), IsVisible());
	HOUDINI_LOG_DISPLAY(TEXT("[UHoudiniSplineComponent::ApplyComponentInstanceData()] bHiddenInGame: %d"), bHiddenInGame);

	check(ComponentInstanceData);
	
	if (!bPostUCS)
	{
		//bHasChanged = ComponentInstanceData->bHasChanged;
		//bNeedsToTriggerUpdate = ComponentInstanceData->bNeedsToTriggerUpdate;
		/*CurvePoints = ComponentInstanceData->CurvePoints;
		DisplayPoints = ComponentInstanceData->DisplayPoints;
		DisplayPointIndexDivider = ComponentInstanceData->DisplayPointIndexDivider;

#if WITH_EDITOR_DATA
		EditedControlPointsIndexes = ComponentInstanceData->EditedControlPointsIndexes;
#endif*/
	}
}

void 
UHoudiniSplineComponent::CopyPropertiesFrom(UObject* FromObject)
{
	// Capture properties that we want to preserve during copy
	const int32 PrevNodeId = NodeId;

	HOUDINI_LOG_DISPLAY(TEXT("[UHoudiniSplineComponent::CopyPropertiesFrom()] BEFORE - IsVisible: %d"), IsVisible());
	HOUDINI_LOG_DISPLAY(TEXT("[UHoudiniSplineComponent::CopyPropertiesFrom()] BEFORE - bHiddenInGame: %d"), bHiddenInGame);

	UActorComponent* FromComponent = Cast<UActorComponent>(FromObject);
	check(FromComponent);

	UEngine::FCopyPropertiesForUnrelatedObjectsParams Params;
	//Params.bDoDelta = false; // Perform a deep copy
	//Params.bClearReferences = false;
	//UEngine::CopyPropertiesForUnrelatedObjects(FromComponent, this, Params);
	
	/*const auto ComponentCopyOptions = ( EditorUtilities::ECopyOptions::Type )(EditorUtilities::ECopyOptions::Default);
	FHoudiniEngineRuntimeUtils::CopyComponentProperties(FromComponent, this, ComponentCopyOptions);*/

	UHoudiniSplineComponent* FromSplineComponent = Cast<UHoudiniSplineComponent>(FromObject);
	if (FromSplineComponent)
	{
		CurvePoints = FromSplineComponent->CurvePoints;
		DisplayPoints = FromSplineComponent->DisplayPoints;
		DisplayPointIndexDivider = FromSplineComponent->DisplayPointIndexDivider;
#if WITH_EDITORONLY_DATA
		EditedControlPointsIndexes = FromSplineComponent->EditedControlPointsIndexes;
#endif

		HoudiniSplineName = FromSplineComponent->HoudiniSplineName;
		bClosed = FromSplineComponent->bClosed;
		bIsHoudiniSplineVisible = FromSplineComponent->bIsHoudiniSplineVisible;
		CurveType = FromSplineComponent->CurveType;
		CurveMethod = FromSplineComponent->CurveMethod;
		bIsInputCurve = FromSplineComponent->bIsInputCurve;
		bIsOutputCurve = FromSplineComponent->bIsOutputCurve;
		bIsEditableOutputCurve = FromSplineComponent->bIsEditableOutputCurve;
		bCookOnCurveChanged = FromSplineComponent->bCookOnCurveChanged;



		bHasChanged = FromSplineComponent->bHasChanged;
		bNeedsToTriggerUpdate = FromSplineComponent->bNeedsToTriggerUpdate;
	}

	HOUDINI_LOG_DISPLAY(TEXT("[UHoudiniSplineComponent::CopyPropertiesFrom()] AFTER - IsVisible: %d"), IsVisible());
	HOUDINI_LOG_DISPLAY(TEXT("[UHoudiniSplineComponent::CopyPropertiesFrom()] AFTER - bHiddenInGame: %d"), bHiddenInGame);

	// Restore properties that we want to preserve
	NodeId = PrevNodeId;
}


void 
UHoudiniSplineComponent::OnUnregister()
{
	Super::OnUnregister();
}

void 
UHoudiniSplineComponent::OnComponentCreated() 
{
	HOUDINI_LOG_DISPLAY(TEXT("[UHoudiniSplineComponent::OnComponentCreated()] Component: %s"), *GetPathName());
	HOUDINI_LOG_DISPLAY(TEXT("[UHoudiniSplineComponent::OnComponentCreated()] bVisible: %d"), IsVisible());
	HOUDINI_LOG_DISPLAY(TEXT("[UHoudiniSplineComponent::OnComponentCreated()] bHiddenInGame: %d"), bHiddenInGame);
	Super::OnComponentCreated();
}

void 
UHoudiniSplineComponent::OnComponentDestroyed(bool bDestroyingHierarchy) 
{
	Super::OnComponentDestroyed(bDestroyingHierarchy);

	if (IsInputCurve())
	{
		// This component can't just come out of nowhere and decide to delete an input object!
		// We have rules and regulations for this sort of thing. Protocols to follow, forms to fill out, in triplicate! 
		
		// InputObject->MarkPendingKill();

		// if(NodeId > -1)
		// 	FHoudiniEngineRuntime::Get().MarkNodeIdAsPendingDelete(NodeId);

		SetNodeId(-1); // Set nodeId to invalid for reconstruct on re-do
	}
}


#if WITH_EDITOR
void 
UHoudiniSplineComponent::PostEditUndo() 
{
	Super::PostEditUndo();
	
	bPostUndo = true;

	// if (bIsInputCurve)
	// {
	// 	UHoudiniInputObject * CurrentInputObject = GetInputObject();
	// 	if (!CurrentInputObject || CurrentInputObject->IsPendingKill())
	// 		return;
	//
	// 	CurrentInputObject->MarkChanged(true);
	// }
	//
	// if (bIsEditableOutputCurve)
	// {
	// 	MarkChanged(true);
	// }

	MarkChanged(true);

}
#endif

void 
UHoudiniSplineComponent::SetOffset(const float& Offset) 
{
	for (int n = 0; n < CurvePoints.Num(); ++n)
		CurvePoints[n].AddToTranslation(FVector(0.f, Offset, 0.f));
	
	for (int n = 0; n < DisplayPoints.Num(); ++n)
		DisplayPoints[n] += FVector(0.f, Offset, 0.f);
}

void 
UHoudiniSplineComponent::ResetCurvePoints() 
{
	CurvePoints.Empty();
}

void
UHoudiniSplineComponent::ResetDisplayPoints() 
{
	DisplayPoints.Empty();
}

void
UHoudiniSplineComponent::AddCurvePoints(const TArray<FTransform>& Points) 
{
	CurvePoints.Append(Points);
}

void 
UHoudiniSplineComponent::AddDisplayPoints(const TArray<FVector>& Points) 
{
	DisplayPoints.Append(Points);
}

bool 
UHoudiniSplineComponent::NeedsToTriggerUpdate() const 
{
	return bNeedsToTriggerUpdate;
	
	// if (bIsInputCurve) 
	// {
	// 	UHoudiniInputObject * CurrentInputObject = GetInputObject();
	// 	if (!CurrentInputObject || CurrentInputObject->IsPendingKill())
	// 		return false;
	//
	// 	return CurrentInputObject->NeedsToTriggerUpdate();
	// }
	//
	// if (bIsEditableOutputCurve) 
	// {
	// 	return bNeedsToTriggerUpdate;
	// }
	//
	// return false;
}

void UHoudiniSplineComponent::SetNeedsToTriggerUpdate(const bool& NeedsToTriggerUpdate)
{
	 bNeedsToTriggerUpdate = NeedsToTriggerUpdate;
}

void UHoudiniSplineComponent::SetCurveType(const EHoudiniCurveType & NewCurveType)
{
	CurveType = NewCurveType;
#if WITH_EDITOR
	//FHoudiniEngineRuntimeUtils::DoPostEditChangeProperty(this, TEXT("CurveType"));
#endif
}

void
UHoudiniSplineComponent::MarkInputObjectChanged() 
{
	// if (bIsInputCurve) 
	// {
	// 	UHoudiniInputObject * CurrentInputObject = GetInputObject();
	// 	if (!CurrentInputObject || CurrentInputObject->IsPendingKill())
	// 		return;
	//
	// 	if (HasChanged())
	// 		CurrentInputObject->MarkChanged(true);
	// }
	//
	// if (bIsEditableOutputCurve) 
	// {
	// 	if (HasChanged())
	// 		MarkChanged(true);
	// }
	
	// NOTE: This component should be trying to push ANY state changes to Input or Output objects. This
	// component should strictly be a data container. Input / Output objects that reference this component should
	// be polling this component's state.
	MarkChanged(true);
}

bool UHoudiniSplineComponent::HasChanged() const
{
	return bHasChanged;
}

void UHoudiniSplineComponent::MarkChanged(const bool& Changed)
{
	bHasChanged = Changed;
	bNeedsToTriggerUpdate = Changed;
}

// UHoudiniAssetComponent* 
// UHoudiniSplineComponent::GetParentHAC() 
// {
// 	UHoudiniAssetComponent* ParentHAC = nullptr;
// 	if (bIsInputCurve) 
// 	{
// 		if (!InputObject)
// 			return nullptr;
//
// 		UHoudiniInput* Input = Cast<UHoudiniInput>(InputObject->GetOuter());
// 		if (!Input)
// 			return nullptr;
//
// 		ParentHAC = Cast<UHoudiniAssetComponent>(Input->GetOuter());
// 	}
// 	else
// 	{
// 		// may do something else if this is not an input curve instead of returning Null.
// 	}
//
// 	return ParentHAC;
//
// }

FHoudiniSplineComponentInstanceData::FHoudiniSplineComponentInstanceData()
{
}

FHoudiniSplineComponentInstanceData::FHoudiniSplineComponentInstanceData(const UHoudiniSplineComponent* SourceComponent)
	: FActorComponentInstanceData(SourceComponent)
{
}


