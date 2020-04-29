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

#include "Curves/CurveFloat.h"
#include "Curves/CurveLinearColor.h"
#include "HoudiniParameterMultiParm.h"
#include "HoudiniParameterRamp.generated.h"

class UHoudiniParameterRampFloat;
class UHoudiniParameterRampColor;
class UHoudiniParameter;
class UHoudiniParameterFloat;
class UHoudiniParameterChoice;
class UHoudiniParameterColor;

UENUM()
enum class EHoudiniRampPointConstructStatus : uint8
{
	None,

	INITIALIZED,
	POSITION_INSERTED,
	VALUE_INSERTED,
	INTERPTYPE_INSERTED
};

UCLASS()
class HOUDINIENGINERUNTIME_API UHoudiniParameterRampModificationEvent : public UObject 
{
	GENERATED_BODY()
public:
	FORCEINLINE
	void SetInsertEvent() { bIsInsertEvent = true; };

	FORCEINLINE
	void SetDeleteEvent() { bIsInsertEvent = false; };

	FORCEINLINE
	void SetFloatRampEvent() { bIsFloatRamp = true; };

	FORCEINLINE
	void SetColorRampEvent() { bIsFloatRamp = false; };

	FORCEINLINE
	bool IsInsertEvent() const { return bIsInsertEvent; };

	FORCEINLINE
	bool IsDeleteEvent() const { return !bIsInsertEvent; };

	FORCEINLINE
	bool IsFloatRampEvent() { return bIsFloatRamp; };

	FORCEINLINE
	bool IsColorRampEvent() { return !bIsFloatRamp; };


private:
	UPROPERTY()
	bool bIsInsertEvent = false;

	UPROPERTY()
	bool bIsFloatRamp = false;

public:
	UPROPERTY()
	int32 DeleteInstanceIndex = -1;

	UPROPERTY()
	float InsertPosition;

	UPROPERTY()
	float InsertFloat;

	UPROPERTY()
	FLinearColor InsertColor;

	UPROPERTY()
	EHoudiniRampInterpolationType InsertInterpolation;
};

UCLASS()
class HOUDINIENGINERUNTIME_API UHoudiniParameterRampFloatPoint : public UObject
{
	GENERATED_BODY()

public:

	UPROPERTY()
	float Position;

	UPROPERTY()
	float Value;

	UPROPERTY()
	EHoudiniRampInterpolationType Interpolation = EHoudiniRampInterpolationType::InValid;

	UPROPERTY()
	int32 InstanceIndex = -1;

	UPROPERTY()
	UHoudiniParameterFloat* PositionParentParm = nullptr;

	UPROPERTY()
	UHoudiniParameterFloat* ValueParentParm = nullptr;

	UPROPERTY()
	UHoudiniParameterChoice* InterpolationParentParm = nullptr;

	FORCEINLINE
	float GetPosition() const { return Position; };
	
	void SetPosition(const float InPosition);

	FORCEINLINE
	float GetValue() const { return Value; };
	
	void SetValue(const float InValue);

	FORCEINLINE
	EHoudiniRampInterpolationType GetInterpolation() const { return Interpolation; };

	void SetInterpolation(const EHoudiniRampInterpolationType InInterpolation);
		
};

UCLASS()
class HOUDINIENGINERUNTIME_API UHoudiniParameterRampColorPoint : public UObject
{
	GENERATED_BODY()

public:

	UPROPERTY()
	float Position;

	UPROPERTY()
	FLinearColor Value;

	UPROPERTY()
	EHoudiniRampInterpolationType Interpolation = EHoudiniRampInterpolationType::InValid;

	UPROPERTY()
	int32 InstanceIndex = -1;

	UPROPERTY()
	UHoudiniParameterFloat * PositionParentParm = nullptr;

	UPROPERTY()
	UHoudiniParameterColor* ValueParentParm = nullptr;

	UPROPERTY()
	UHoudiniParameterChoice* InterpolationParentParm = nullptr;

	FORCEINLINE
	float GetPosition() const { return Position; };

	void SetPosition(const float InPosition);
	FORCEINLINE
	FLinearColor GetValue() const { return Value; };

	void SetValue(const FLinearColor InValue);

	FORCEINLINE
	EHoudiniRampInterpolationType GetInterpolation() const { return Interpolation; };

	void SetInterpolation(const EHoudiniRampInterpolationType InInterpolation);
};


UCLASS()
class HOUDINIENGINERUNTIME_API UHoudiniParameterRampFloat : public UHoudiniParameterMultiParm
{
	GENERATED_UCLASS_BODY()

public:

	// Create instance of this class.
	static UHoudiniParameterRampFloat * Create(
		UObject* Outer,
		const FString& ParamName);

	FORCEINLINE
	bool IsCaching() const { return bCaching; };

	FORCEINLINE
	void SetCaching(const bool bInCaching) { bCaching = bInCaching; };


	UPROPERTY()
	TArray<UHoudiniParameterRampFloatPoint*> Points;

	UPROPERTY()
	TArray<UHoudiniParameterRampFloatPoint*> CachedPoints;

	UPROPERTY()
	int32 NumChild = 0;

	UPROPERTY()
	bool bCaching = false;

	UPROPERTY()
	TArray<UHoudiniParameterRampModificationEvent*> ModificationEvents;

};


UCLASS()
class HOUDINIENGINERUNTIME_API UHoudiniParameterRampColor : public UHoudiniParameterMultiParm
{
	GENERATED_UCLASS_BODY()

public:

	// Create instance of this class.
	static UHoudiniParameterRampColor * Create(
		UObject* Outer,
		const FString& ParamName);

	UPROPERTY()
	TArray<UHoudiniParameterRampColorPoint*> Points;

	UPROPERTY()
	int NumChild = 0;

	UPROPERTY()
	bool bCaching = false;

	UPROPERTY()
	TArray<UHoudiniParameterRampColorPoint*> CachedPoints;

	UPROPERTY()
	TArray<UHoudiniParameterRampModificationEvent*> ModificationEvents;

};