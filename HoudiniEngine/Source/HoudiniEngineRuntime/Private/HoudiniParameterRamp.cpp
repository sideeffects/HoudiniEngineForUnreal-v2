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

#include "HoudiniParameterRamp.h"
#include "HoudiniParameter.h"
#include "HoudiniParameterFloat.h"
#include "HoudiniParameterColor.h"
#include "HoudiniParameterChoice.h"


void 
UHoudiniParameterRampFloatPoint::SetPosition(const float InPosition) {
	if (PositionParentParm && PositionParentParm->GetNumberOfValues() > 0)
	{
		Position = InPosition;
		PositionParentParm->SetValueAt(InPosition, 0);
	}
};

void 
UHoudiniParameterRampFloatPoint::SetValue(const float InValue) {
	if (ValueParentParm && PositionParentParm->GetNumberOfValues() > 0)
	{
		Value = InValue;
		ValueParentParm->SetValueAt(InValue, 0);
	}
};

void
UHoudiniParameterRampFloatPoint::SetInterpolation(const EHoudiniRampInterpolationType InInterpolation) {
	if (InterpolationParentParm)
	{
		Interpolation = InInterpolation;
		InterpolationParentParm->SetIntValue((int32)InInterpolation);
		InterpolationParentParm->UpdateStringValueFromInt();
	}
};


void
UHoudiniParameterRampColorPoint::SetPosition(const float InPosition) {
	if (PositionParentParm && PositionParentParm->GetNumberOfValues() > 0)
	{
		Position = InPosition;
		PositionParentParm->SetValueAt(InPosition, 0);
	}
};

void
UHoudiniParameterRampColorPoint::SetValue(const FLinearColor InValue) {
	if (!ValueParentParm)
		return;

	Value = InValue;
	ValueParentParm->SetColorValue(InValue);
};

void
UHoudiniParameterRampColorPoint::SetInterpolation(const EHoudiniRampInterpolationType InInterpolation) {
	if (!InterpolationParentParm)
		return;

	Interpolation = InInterpolation;
	InterpolationParentParm->SetIntValue((int32)InInterpolation);
	InterpolationParentParm->UpdateStringValueFromInt();
};



UHoudiniParameterRampFloat::UHoudiniParameterRampFloat(const FObjectInitializer & ObjectInitializer)
	: Super(ObjectInitializer)
{}


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

	return HoudiniParameter;
}


UHoudiniParameterRampColor::UHoudiniParameterRampColor(const FObjectInitializer & ObjectInitializer)
	: Super(ObjectInitializer)
{}


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

	return HoudiniParameter;
}
