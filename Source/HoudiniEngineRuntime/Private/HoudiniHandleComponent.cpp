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

#include "HoudiniHandleComponent.h"

#include "HoudiniParameter.h"
#include "HoudiniParameterFloat.h"
#include "HoudiniParameterChoice.h"

UHoudiniHandleParameter::UHoudiniHandleParameter(const FObjectInitializer & ObjectInitializer) 
	:Super(ObjectInitializer)
{};

UHoudiniHandleComponent::UHoudiniHandleComponent(const FObjectInitializer & ObjectInitializer)
	:Super(ObjectInitializer) 
{};


bool 
UHoudiniHandleParameter::Bind(float & OutValue, const char * CmpName, int32 InTupleIdx,
			const FString & HandleParmName, UHoudiniParameter* Parameter) 
{
	if (!Parameter)
		return false;

	if (HandleParmName != CmpName)
		return false;

	UHoudiniParameterFloat* FloatParameter = Cast<UHoudiniParameterFloat>(Parameter);

	if (!FloatParameter)
		return false;

	AssetParameter = Parameter;

	if (FloatParameter) 
	{
		// It is possible that the handle param is bound to a single tuple param.
		// Ignore the preset tuple index if that's the case or we'll crash.
		if (Parameter->GetTupleSize() <= InTupleIdx)
			InTupleIdx = 0;

		auto Optional = FloatParameter->GetValue(InTupleIdx);
		if (Optional.IsSet())
		{
			TupleIndex = InTupleIdx;
			OutValue = Optional.GetValue();
			return true;
		}
	}

	return false;
}

bool 
UHoudiniHandleParameter::Bind(TSharedPtr<FString> & OutValue, const char * CmpName,
			int32 InTupleIdx, const FString & HandleParmName, UHoudiniParameter* Parameter) 
{
	if (!Parameter)
		return false;

	if (HandleParmName != CmpName)
		return false;

	UHoudiniParameterChoice* ChoiceParameter = Cast<UHoudiniParameterChoice>(Parameter);

	if (!ChoiceParameter)
		return false;

	AssetParameter = Parameter;

	if (ChoiceParameter)
	{
		// It is possible that the handle param is bound to a single tuple param.
		// Ignore the preset tuple index if that's the case or we'll crash.
		if (Parameter->GetTupleSize() <= InTupleIdx)
			InTupleIdx = 0;

		auto Optional = ChoiceParameter->GetValue(InTupleIdx);
		if (Optional.IsSet())
		{
			TupleIndex = InTupleIdx;
			OutValue = Optional.GetValue();
			return true;
		}
	}

	return false;
}

TSharedPtr<FString> 
UHoudiniHandleParameter::Get(TSharedPtr<FString> DefaultValue) const 
{
	UHoudiniParameterChoice* ChoiceParameter = Cast<UHoudiniParameterChoice>(AssetParameter);
	if (ChoiceParameter)
	{
		auto Optional = ChoiceParameter->GetValue(TupleIndex);
		if (Optional.IsSet())
			return Optional.GetValue();
	}

	return DefaultValue;
}

UHoudiniHandleParameter & 
UHoudiniHandleParameter::operator=(float Value) 
{
	UHoudiniParameterFloat* FloatParameter = Cast<UHoudiniParameterFloat>(AssetParameter);
	if (FloatParameter)
	{
		FloatParameter->SetValue(Value, TupleIndex);
		FloatParameter->MarkChanged(true);
	}

	return *this;
}

void 
UHoudiniHandleComponent::InitializeHandleParameters() 
{
	if (XformParms.Num() < int32(EXformParameter::COUNT)) 
	{
		XformParms.Empty();
		for (int32 n = 0; n < int32(EXformParameter::COUNT); ++n)
		{
			UHoudiniHandleParameter* XformHandle = NewObject<UHoudiniHandleParameter>(this, UHoudiniHandleParameter::StaticClass());
			XformParms.Add(XformHandle);
		}
	}

	if (!RSTParm) 
	{
		RSTParm = NewObject<UHoudiniHandleParameter>(this, UHoudiniHandleParameter::StaticClass());
	}

	if (!RotOrderParm) 
	{
		RotOrderParm = NewObject<UHoudiniHandleParameter>(this, UHoudiniHandleParameter::StaticClass());
	}
}

bool 
UHoudiniHandleComponent::CheckHandleValid() const
{
	if (XformParms.Num() < int32(EXformParameter::COUNT))
		return false;

	for (auto& XformParm : XformParms) 
	{
		if (!XformParm)
			return false;
	}

	if (!RSTParm)
		return false;

	if (!RotOrderParm)
		return false;

	return true;
}
