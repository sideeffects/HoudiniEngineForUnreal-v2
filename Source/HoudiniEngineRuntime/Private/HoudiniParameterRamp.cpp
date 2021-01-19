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

#include "HoudiniParameterRamp.h"
#include "HoudiniParameter.h"
#include "HoudiniParameterFloat.h"
#include "HoudiniParameterColor.h"
#include "HoudiniParameterChoice.h"

#include "UObject/UnrealType.h"


void 
UHoudiniParameterRampFloatPoint::SetPosition(const float InPosition)
{
	if (PositionParentParm && PositionParentParm->GetNumberOfValues() > 0)
	{
		Position = InPosition;
		PositionParentParm->SetValueAt(InPosition, 0);
		PositionParentParm->SetIsChildOfRamp();
	}
};

void 
UHoudiniParameterRampFloatPoint::SetValue(const float InValue) 
{
	if (ValueParentParm && PositionParentParm->GetNumberOfValues() > 0)
	{
		Value = InValue;
		ValueParentParm->SetValueAt(InValue, 0);
		ValueParentParm->SetIsChildOfRamp();
	}
};

void
UHoudiniParameterRampFloatPoint::SetInterpolation(const EHoudiniRampInterpolationType InInterpolation)
{
	if (InterpolationParentParm)
	{
		Interpolation = InInterpolation;
		InterpolationParentParm->SetIntValue((int32)InInterpolation);
		InterpolationParentParm->UpdateStringValueFromInt();
		InterpolationParentParm->SetIsChildOfRamp();
	}
}

UHoudiniParameterRampFloatPoint*
UHoudiniParameterRampFloatPoint::DuplicateAndCopyState(UObject* DestOuter, EObjectFlags InClearFlags, EObjectFlags InSetFlags)
{
	UHoudiniParameterRampFloatPoint* NewPoint = Cast<UHoudiniParameterRampFloatPoint>(StaticDuplicateObject(this, DestOuter));

	NewPoint->CopyStateFrom(this, false, InClearFlags, InSetFlags);

	return NewPoint;
}

void
UHoudiniParameterRampFloatPoint::CopyStateFrom(UHoudiniParameterRampFloatPoint * InParameter, bool bCopyAllProperties, EObjectFlags InClearFlags, EObjectFlags InSetFlags)
{
	if (bCopyAllProperties)
	{
		UEngine::FCopyPropertiesForUnrelatedObjectsParams Params;
		Params.bDoDelta = false; // Perform a deep copy
		Params.bClearReferences = false; // References will be replaced afterwards.
		UEngine::CopyPropertiesForUnrelatedObjects(InParameter, this, Params);
	}

	// Ensure this object's flags match the desired flags.
	if (InClearFlags != RF_NoFlags)
		ClearFlags( InClearFlags );
	if (InSetFlags != RF_NoFlags)
		SetFlags(InSetFlags);
}

void
UHoudiniParameterRampFloatPoint::RemapParameters(
	const TMap<UHoudiniParameter*, UHoudiniParameter*>& ParameterMapping)
{
	if (ParameterMapping.Contains(PositionParentParm))
	{
		PositionParentParm = CastChecked<UHoudiniParameterFloat>(ParameterMapping.FindChecked(PositionParentParm));
	}
	else
	{
		PositionParentParm = nullptr;
	}

	if (ParameterMapping.Contains(ValueParentParm))
	{
		ValueParentParm = CastChecked<UHoudiniParameterFloat>(ParameterMapping.FindChecked(ValueParentParm));
	}
	else
	{
		ValueParentParm = nullptr;
	}

	if (ParameterMapping.Contains(InterpolationParentParm))
	{
		InterpolationParentParm = CastChecked<UHoudiniParameterChoice>(ParameterMapping.FindChecked(InterpolationParentParm));
	}
	else
	{
		InterpolationParentParm = nullptr;
	}
};


void
UHoudiniParameterRampColorPoint::SetPosition(const float InPosition)
{
	if (PositionParentParm && PositionParentParm->GetNumberOfValues() > 0)
	{
		Position = InPosition;
		PositionParentParm->SetValueAt(InPosition, 0);
		PositionParentParm->SetIsChildOfRamp();
	}
};

void
UHoudiniParameterRampColorPoint::SetValue(const FLinearColor InValue) 
{
	if (!ValueParentParm)
		return;

	Value = InValue;
	ValueParentParm->SetColorValue(InValue);
	ValueParentParm->SetIsChildOfRamp();
};

void
UHoudiniParameterRampColorPoint::SetInterpolation(const EHoudiniRampInterpolationType InInterpolation) 
{
	if (!InterpolationParentParm)
		return;

	Interpolation = InInterpolation;
	InterpolationParentParm->SetIntValue((int32)InInterpolation);
	InterpolationParentParm->UpdateStringValueFromInt();
	InterpolationParentParm->SetIsChildOfRamp();
}
UHoudiniParameterRampColorPoint * UHoudiniParameterRampColorPoint::DuplicateAndCopyState(UObject * DestOuter, EObjectFlags InClearFlags, EObjectFlags InSetFlags)
{

	UHoudiniParameterRampColorPoint* NewPoint = Cast<UHoudiniParameterRampColorPoint>(StaticDuplicateObject(this, DestOuter));

	NewPoint->CopyStateFrom(this, false, InClearFlags, InSetFlags);

	return NewPoint;
	
}
void UHoudiniParameterRampColorPoint::CopyStateFrom(UHoudiniParameterRampColorPoint * InParameter, bool bCopyAllProperties, EObjectFlags InClearFlags, EObjectFlags InSetFlags)
{
#if WITH_EDITOR
	PreEditChange(nullptr);
#endif
	if (bCopyAllProperties)
	{
		UEngine::FCopyPropertiesForUnrelatedObjectsParams Params;
		Params.bDoDelta = false; // Perform a deep copy
		Params.bClearReferences = false; // References will be replaced afterwards.
		UEngine::CopyPropertiesForUnrelatedObjects(InParameter, this, Params);
	}

	// Ensure this object's flags match the desired flags.
	if (InClearFlags != RF_NoFlags)
		ClearFlags( InClearFlags );
	if (InSetFlags != RF_NoFlags)
		SetFlags(InSetFlags);
#if WITH_EDITOR
	PostEditChange();
#endif
}

void UHoudiniParameterRampColorPoint::RemapParameters(const TMap<UHoudiniParameter*, UHoudiniParameter*>& ParameterMapping)
{
	if (ParameterMapping.Contains(PositionParentParm))
	{
		PositionParentParm = CastChecked<UHoudiniParameterFloat>(ParameterMapping.FindChecked(PositionParentParm));
	}
	else
	{
		PositionParentParm = nullptr;
	}

	if (ParameterMapping.Contains(ValueParentParm))
	{
		ValueParentParm = CastChecked<UHoudiniParameterColor>(ParameterMapping.FindChecked(ValueParentParm));
	}
	else
	{
		ValueParentParm = nullptr;
	}

	if (ParameterMapping.Contains(InterpolationParentParm))
	{
		InterpolationParentParm = CastChecked<UHoudiniParameterChoice>(ParameterMapping.FindChecked(InterpolationParentParm));
	}
	else
	{
		InterpolationParentParm = nullptr;
	}
}


UHoudiniParameterRampFloat::UHoudiniParameterRampFloat(const FObjectInitializer & ObjectInitializer)
	: Super(ObjectInitializer),
	NumDefaultPoints(-1),
	bCaching(false)
{
	ParmType = EHoudiniParameterType::FloatRamp;
}

void
UHoudiniParameterRampFloat::OnPreCook()
{
	if (bCaching)
	{
		SyncCachedPoints();
		bCaching = false;
	}
}

UHoudiniParameterRampFloat *
UHoudiniParameterRampFloat::Create(
	UObject* InOuter,
	const FString& InParamName)
{
	FString ParamNameStr = "HoudiniParameterRamp_" + InParamName;
	FName ParamName = MakeUniqueObjectName(InOuter, UHoudiniParameterRampFloat::StaticClass(), *ParamNameStr);

	// We need to create a new parameter
	UHoudiniParameterRampFloat * HoudiniParameter = NewObject< UHoudiniParameterRampFloat >(
		InOuter, UHoudiniParameterRampFloat::StaticClass(), ParamName, RF_Public | RF_Transactional);

	HoudiniParameter->SetParameterType(EHoudiniParameterType::FloatRamp);

	HoudiniParameter->NumDefaultPoints = -1;

	HoudiniParameter->bCaching = false;

	return HoudiniParameter;
}

void UHoudiniParameterRampFloat::CopyStateFrom(UHoudiniParameter* InParameter, bool bCopyAllProperties, EObjectFlags InClearFlags, EObjectFlags InSetFlags)
{
#if WITH_EDITOR
	PreEditChange(nullptr);
#endif

	UHoudiniParameterRampFloat* FromParameter = Cast<UHoudiniParameterRampFloat>(InParameter);
	check(FromParameter);

	TArray<UHoudiniParameterRampFloatPoint*> PrevCachedPoints = CachedPoints;
	TArray<UHoudiniParameterRampFloatPoint*> PrevPoints = Points;
	
	Super::CopyStateFrom(InParameter, bCopyAllProperties, InClearFlags, InSetFlags);

	CachedPoints = PrevCachedPoints;
	Points = PrevPoints;
	
	
	auto CopyPointsStateFn = [InClearFlags, InSetFlags] (TArray<UHoudiniParameterRampFloatPoint*>& FromArray, TArray<UHoudiniParameterRampFloatPoint*>& ToArray, UObject* NewOuter)
	{
		const int32 NumPts = FromArray.Num();

		ToArray.SetNum(NumPts);

		for(int32 i = 0; i < NumPts; ++i)
		{
			UHoudiniParameterRampFloatPoint* FromPoint = FromArray[i];
			UHoudiniParameterRampFloatPoint* ToPoint = ToArray[i];

			check(FromPoint);

			if (ToPoint)
			{
				// Ensure the destination point is outered to this parameter
				bool bIsValid = ToPoint->GetOuter() == NewOuter; 
				if (!bIsValid)
					ToPoint = nullptr;
			}

			if (!ToPoint)
			{
				// Duplicate a new copy using FromPoint
				ToPoint = FromPoint->DuplicateAndCopyState(NewOuter, InClearFlags, InSetFlags);
			}
			else
			{
				// We have a valid point that we can reuse. Simply copy state.
				ToPoint->CopyStateFrom(FromPoint, true, InClearFlags, InSetFlags);
			}

			ToArray[i] = ToPoint;
		}
	};

	CopyPointsStateFn(FromParameter->CachedPoints, CachedPoints, this);
	CopyPointsStateFn(FromParameter->Points, Points, this);

#if WITH_EDITOR
	PostEditChange();
#endif
}

void
UHoudiniParameterRampFloat::RemapParameters(const TMap<UHoudiniParameter*, UHoudiniParameter*>& ParameterMapping)
{
	Super::RemapParameters(ParameterMapping);

	AActor* OuterActor = GetTypedOuter<AActor>();


	for(UHoudiniParameterRampFloatPoint* CurrentPoint : Points)
	{
		CurrentPoint->RemapParameters(ParameterMapping);
	}

	for(UHoudiniParameterRampFloatPoint* CurrentPoint : CachedPoints)
	{
		CurrentPoint->RemapParameters(ParameterMapping);
	}
}

void
UHoudiniParameterRampFloat::SyncCachedPoints()
{
	int32 Idx = 0;

	while (Idx < CachedPoints.Num() && Idx < Points.Num())
	{
		UHoudiniParameterRampFloatPoint* &CachedPoint = CachedPoints[Idx];
		UHoudiniParameterRampFloatPoint* &CurrentPoint = Points[Idx];

		if (!CachedPoint || !CurrentPoint)
			continue;

		if (CachedPoint->GetPosition() != CurrentPoint->GetPosition()) 
		{
			if (CurrentPoint->PositionParentParm) 
			{
				CurrentPoint->SetPosition(CachedPoint->GetPosition());
				CurrentPoint->PositionParentParm->MarkChanged(true);
			}
		}

		if (CachedPoint->GetValue() != CurrentPoint->GetValue()) 
		{
			if (CurrentPoint->ValueParentParm) 
			{
				CurrentPoint->SetValue(CachedPoint->GetValue());
				CurrentPoint->ValueParentParm->MarkChanged(true);
			}
		}

		if (CachedPoint->GetInterpolation() != CurrentPoint->GetInterpolation()) 
		{
			if (CurrentPoint->InterpolationParentParm) 
			{
				CurrentPoint->SetInterpolation(CachedPoint->GetInterpolation());
				CurrentPoint->InterpolationParentParm->MarkChanged(true);
			}
		}

		Idx += 1;
	}

	// Insert points
	for (int32 IdxCachedPointLeft = Idx; IdxCachedPointLeft < CachedPoints.Num(); ++IdxCachedPointLeft) 
	{
		UHoudiniParameterRampFloatPoint *&CachedPoint = CachedPoints[IdxCachedPointLeft];
		if (!CachedPoint)
			continue;

		CreateInsertEvent(CachedPoint->Position, CachedPoint->Value, CachedPoint->Interpolation);

		MarkChanged(true);
	}

	// Remove points
	for (int32 IdxCurrentPointLeft = Idx; IdxCurrentPointLeft < Points.Num(); ++IdxCurrentPointLeft) 
	{
		RemoveElement(IdxCurrentPointLeft);

		UHoudiniParameterRampFloatPoint* Point = Points[IdxCurrentPointLeft];

		if (!Point)
			continue;

		CreateDeleteEvent(Point->InstanceIndex);

		MarkChanged(true);
	}
}


void 
UHoudiniParameterRampFloat::CreateInsertEvent(const float& InPosition, const float& InValue, const EHoudiniRampInterpolationType &InInterp) 
{
	UHoudiniParameterRampModificationEvent* InsertEvent = NewObject<UHoudiniParameterRampModificationEvent>(
		this, UHoudiniParameterRampModificationEvent::StaticClass());

	if (!InsertEvent)
		return;

	InsertEvent->SetFloatRampEvent();
	InsertEvent->SetInsertEvent();
	InsertEvent->InsertPosition = InPosition;
	InsertEvent->InsertFloat = InValue;
	InsertEvent->InsertInterpolation = InInterp;

	ModificationEvents.Add(InsertEvent);
}

void 
UHoudiniParameterRampFloat::CreateDeleteEvent(const int32 &InDeleteIndex) 
{
	UHoudiniParameterRampModificationEvent* DeleteEvent = NewObject<UHoudiniParameterRampModificationEvent>(
		this, UHoudiniParameterRampModificationEvent::StaticClass());

	if (!DeleteEvent)
		return;

	DeleteEvent->SetFloatRampEvent();
	DeleteEvent->SetDeleteEvent();
	DeleteEvent->DeleteInstanceIndex = InDeleteIndex;

	ModificationEvents.Add(DeleteEvent);
}


UHoudiniParameterRampColor::UHoudiniParameterRampColor(const FObjectInitializer & ObjectInitializer)
	: Super(ObjectInitializer),	
	bCaching(false),
	NumDefaultPoints(-1)
{
	ParmType = EHoudiniParameterType::ColorRamp;
}


UHoudiniParameterRampColor *
UHoudiniParameterRampColor::Create(
	UObject* InOuter,
	const FString& InParamName)
{
	FString ParamNameStr = "HoudiniParameterRamp_" + InParamName;
	FName ParamName = MakeUniqueObjectName(InOuter, UHoudiniParameterRampColor::StaticClass(), *ParamNameStr);

	// We need to create a new parameter
	UHoudiniParameterRampColor * HoudiniParameter = NewObject< UHoudiniParameterRampColor >(
		InOuter, UHoudiniParameterRampColor::StaticClass(), ParamName, RF_Public | RF_Transactional);

	HoudiniParameter->SetParameterType(EHoudiniParameterType::ColorRamp);

	HoudiniParameter->NumDefaultPoints = -1;

	HoudiniParameter->bCaching = false;

	return HoudiniParameter;
}


bool 
UHoudiniParameterRampFloat::IsDefault() const 
{
	if (NumDefaultPoints < 0)
		return true;

	if (NumDefaultPoints != Points.Num())
	{
		return false;
	} 

	TArray<float> Positions = DefaultPositions;
	TArray<float> Values = DefaultValues;
	TArray<int32> Choices = DefaultChoices;

	for (auto & NextPt : Points) 
	{
		if (!NextPt)
			return false;

		bool bFoundMatch = false;
		for (int32 DefaultIdx = 0; DefaultIdx < Positions.Num(); ++DefaultIdx)
		{
			if (Positions[DefaultIdx] == NextPt->Position &&
				Values[DefaultIdx] == NextPt->Value &&
				Choices[DefaultIdx] == (int32)NextPt->Interpolation) 
			{
				Positions.RemoveAt(DefaultIdx);
				Values.RemoveAt(DefaultIdx);
				Choices.RemoveAt(DefaultIdx);
				bFoundMatch = true;
			}
		}

		if (!bFoundMatch)
			return false;
	}

	if (Positions.Num() > 0)
		return false;

	return true;
}

void UHoudiniParameterRampColor::CopyStateFrom(UHoudiniParameter * InParameter, bool bCopyAllProperties, EObjectFlags InClearFlags, EObjectFlags InSetFlags)
{
	UHoudiniParameterRampColor* FromParameter = Cast<UHoudiniParameterRampColor>(InParameter);
	check(FromParameter);

	TArray<UHoudiniParameterRampColorPoint*> PrevCachedPoints = CachedPoints;
	TArray<UHoudiniParameterRampColorPoint*> PrevPoints = Points;
	
	Super::CopyStateFrom(InParameter, bCopyAllProperties, InClearFlags, InSetFlags);

	CachedPoints = PrevCachedPoints;
	Points = PrevPoints;
	
	
	auto CopyPointsStateFn = [InClearFlags, InSetFlags] (TArray<UHoudiniParameterRampColorPoint*>& FromArray, TArray<UHoudiniParameterRampColorPoint*>& ToArray, UObject* NewOuter)
	{
		const int32 NumPts = FromArray.Num();

		ToArray.SetNum(NumPts);

		for(int32 i = 0; i < NumPts; ++i)
		{
			UHoudiniParameterRampColorPoint* FromPoint = FromArray[i];
			UHoudiniParameterRampColorPoint* ToPoint = ToArray[i];

			check(FromPoint);

			if (ToPoint)
			{
				// Ensure the destination point is outered to this parameter
				bool bIsValid = ToPoint->GetOuter() == NewOuter; 
				if (!bIsValid)
					ToPoint = nullptr;
			}

			if (!ToPoint)
			{
				// Duplicate a new copy using FromPoint
				ToPoint = FromPoint->DuplicateAndCopyState(NewOuter, InClearFlags, InSetFlags);
			}
			else
			{
				// We have a valid point that we can reuse. Simply copy state.
				ToPoint->CopyStateFrom(FromPoint, true, InClearFlags, InSetFlags);
			}

			ToArray[i] = ToPoint;
		}
	};

	CopyPointsStateFn(FromParameter->CachedPoints, CachedPoints, this);
	CopyPointsStateFn(FromParameter->Points, Points, this);

	if (InClearFlags != RF_NoFlags)
		ClearFlags( InClearFlags );
	if (InSetFlags != RF_NoFlags)
		SetFlags(InSetFlags);
}

void UHoudiniParameterRampColor::RemapParameters(const TMap<UHoudiniParameter*, UHoudiniParameter*>& ParameterMapping)
{
	Super::RemapParameters(ParameterMapping);

	AActor* OuterActor = GetTypedOuter<AActor>();


	for(UHoudiniParameterRampColorPoint* CurrentPoint : Points)
	{
		CurrentPoint->RemapParameters(ParameterMapping);
	}

	for(UHoudiniParameterRampColorPoint* CurrentPoint : CachedPoints)
	{
		CurrentPoint->RemapParameters(ParameterMapping);
	}
}

bool
UHoudiniParameterRampColor::IsDefault() const 
{
	if (NumDefaultPoints < 0)
		return true;

	if (NumDefaultPoints != Points.Num())
		return false;

	TArray<float> Positions = DefaultPositions;
	TArray<FLinearColor> Values = DefaultValues;
	TArray<int32> Choices = DefaultChoices;

	for (auto & NextPt : Points)
	{
		if (!NextPt)
			return false;

		bool bFoundMatch = false;
		for (int32 DefaultIdx = 0; DefaultIdx < Positions.Num(); ++DefaultIdx)
		{
			if (Positions[DefaultIdx] == NextPt->Position &&
				Values[DefaultIdx] == NextPt->Value &&
				Choices[DefaultIdx] == (int32)NextPt->Interpolation)
			{
				Positions.RemoveAt(DefaultIdx);
				Values.RemoveAt(DefaultIdx);
				Choices.RemoveAt(DefaultIdx);
				bFoundMatch = true;
			}
		}

		if (!bFoundMatch)
			return false;
	}

	if (Positions.Num() > 0)
		return false;

	return true;
}


void
UHoudiniParameterRampColor::SetDefaultValues() 
{
	if (NumDefaultPoints >= 0)
		return;


	if (DefaultPositions.Num() > 0)
		return;

	DefaultPositions.Empty();
	DefaultValues.Empty();
	DefaultChoices.Empty();

	for (auto & NextPoint : Points)
	{
		if (!NextPoint)
			continue;

		DefaultPositions.Add(NextPoint->Position);
		DefaultValues.Add(NextPoint->Value);
		DefaultChoices.Add((int32)NextPoint->Interpolation);

	}

	NumDefaultPoints = Points.Num();
}

void
UHoudiniParameterRampFloat::SetDefaultValues()
{
	if (DefaultPositions.Num() > 0)
		return;

	DefaultPositions.Empty();
	DefaultValues.Empty();
	DefaultChoices.Empty();

	for (auto & NextPoint : Points) 
	{
		if (!NextPoint)
			continue;

		DefaultPositions.Add(NextPoint->Position);
		DefaultValues.Add(NextPoint->Value);
		DefaultChoices.Add((int32)NextPoint->Interpolation);

	}

	NumDefaultPoints = Points.Num();

}