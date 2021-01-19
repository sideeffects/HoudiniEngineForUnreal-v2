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

#include "HoudiniGenericAttribute.h"

#include "HoudiniEngineRuntimePrivatePCH.h"

#include "Engine/StaticMesh.h"
#include "Components/ActorComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"

#include "PhysicsEngine/BodySetup.h"
#include "EditorFramework/AssetImportData.h"
#include "AI/Navigation/NavCollisionBase.h"

double
FHoudiniGenericAttribute::GetDoubleValue(int32 index) const
{
	if ((AttributeType == EAttribStorageType::FLOAT) || (AttributeType == EAttribStorageType::FLOAT64))
	{
		if (DoubleValues.IsValidIndex(index))
			return DoubleValues[index];
	}
	else if ((AttributeType == EAttribStorageType::INT) || (AttributeType == EAttribStorageType::INT64))
	{
		if (IntValues.IsValidIndex(index))
			return (double)IntValues[index];
	}
	else if (AttributeType == EAttribStorageType::STRING)
	{
		if (StringValues.IsValidIndex(index))
			return FCString::Atod(*StringValues[index]);
	}

	return 0.0f;
}

void
FHoudiniGenericAttribute::GetDoubleTuple(TArray<double>& TupleValues, int32 index) const
{
	TupleValues.SetNumZeroed(AttributeTupleSize);

	for (int32 n = 0; n < AttributeTupleSize; n++)
		TupleValues[n] = GetDoubleValue(index * AttributeTupleSize + n);
}

int64
FHoudiniGenericAttribute::GetIntValue(int32 index) const
{
	if ((AttributeType == EAttribStorageType::INT) || (AttributeType == EAttribStorageType::INT64))
	{
		if (IntValues.IsValidIndex(index))
			return IntValues[index];
	}
	else if ((AttributeType == EAttribStorageType::FLOAT) || (AttributeType == EAttribStorageType::FLOAT64))
	{
		if (DoubleValues.IsValidIndex(index))
			return (int64)DoubleValues[index];
	}
	else if (AttributeType == EAttribStorageType::STRING)
	{
		if (StringValues.IsValidIndex(index))
			return FCString::Atoi64(*StringValues[index]);
	}

	return 0;
}

void 
FHoudiniGenericAttribute::GetIntTuple(TArray<int64>& TupleValues, int32 index) const
{
	TupleValues.SetNumZeroed(AttributeTupleSize);

	for (int32 n = 0; n < AttributeTupleSize; n++)
		TupleValues[n] = GetIntValue(index * AttributeTupleSize + n);
}

FString 
FHoudiniGenericAttribute::GetStringValue(int32 index) const
{
	if (AttributeType == EAttribStorageType::STRING)
	{
		if (StringValues.IsValidIndex(index))
			return StringValues[index];
	}
	else if ((AttributeType == EAttribStorageType::INT) || (AttributeType == EAttribStorageType::INT64))
	{
		if (IntValues.IsValidIndex(index))
			return FString::FromInt((int32)IntValues[index]);
	}
	else if ((AttributeType == EAttribStorageType::FLOAT) || (AttributeType == EAttribStorageType::FLOAT64))
	{
		if (DoubleValues.IsValidIndex(index))
			return FString::SanitizeFloat(DoubleValues[index]);
	}

	return FString();
}

void 
FHoudiniGenericAttribute::GetStringTuple(TArray<FString>& TupleValues, int32 index) const
{
	TupleValues.SetNumZeroed(AttributeTupleSize);

	for (int32 n = 0; n < AttributeTupleSize; n++)
		TupleValues[n] = GetStringValue(index * AttributeTupleSize + n);
}

bool
FHoudiniGenericAttribute::GetBoolValue(int32 index) const
{
	if ((AttributeType == EAttribStorageType::FLOAT) || (AttributeType == EAttribStorageType::FLOAT64))
	{
		if (DoubleValues.IsValidIndex(index))
			return DoubleValues[index] == 0.0 ? false : true;
	}
	else if ((AttributeType == EAttribStorageType::INT) || (AttributeType == EAttribStorageType::INT64))
	{
		if (IntValues.IsValidIndex(index))
			return IntValues[index] == 0 ? false : true;
	}
	else if (AttributeType == EAttribStorageType::STRING)
	{
		if (StringValues.IsValidIndex(index))
			return StringValues[index].Equals(TEXT("true"), ESearchCase::IgnoreCase) ? true : false;
	}

	return false;
}

void 
FHoudiniGenericAttribute::GetBoolTuple(TArray<bool>& TupleValues, int32 index) const
{
	TupleValues.SetNumZeroed(AttributeTupleSize);

	for (int32 n = 0; n < AttributeTupleSize; n++)
		TupleValues[n] = GetBoolValue(index * AttributeTupleSize + n);
}

void*
FHoudiniGenericAttribute::GetData()
{
	if (AttributeType == EAttribStorageType::STRING)
	{
		if (StringValues.Num() > 0)
			return StringValues.GetData();
	}
	else if ((AttributeType == EAttribStorageType::INT) || (AttributeType == EAttribStorageType::INT64))
	{
		if (IntValues.Num() > 0)
			return IntValues.GetData();
	}
	else if ((AttributeType == EAttribStorageType::FLOAT) || (AttributeType == EAttribStorageType::FLOAT64))
	{
		if (DoubleValues.Num() > 0)
			return DoubleValues.GetData();
	}

	return nullptr;
}

bool
FHoudiniGenericAttribute::UpdatePropertyAttributeOnObject(
	UObject* InObject, const FHoudiniGenericAttribute& InPropertyAttribute, const int32& AtIndex)
{
	if (!InObject || InObject->IsPendingKill())
		return false;

	// Get the Property name
	const FString& PropertyName = InPropertyAttribute.AttributeName;
	if (PropertyName.IsEmpty())
		return false;

	// Some Properties need to be handle and modified manually...
	if (PropertyName == "CollisionProfileName")
	{
		UPrimitiveComponent* PC = Cast<UPrimitiveComponent>(InObject);
		if (PC && !PC->IsPendingKill())
		{
			FString StringValue = InPropertyAttribute.GetStringValue(AtIndex);
			FName Value = FName(*StringValue);
			PC->SetCollisionProfileName(Value);

			return true;
		}
		return false;
	}

	// Handle Component Tags manually here
	if (PropertyName.Contains("Tags"))
	{
		UActorComponent* AC = Cast< UActorComponent >(InObject);
		if (AC && !AC->IsPendingKill())
		{
			FName NameAttr = FName(*InPropertyAttribute.GetStringValue(AtIndex));
			if (!AC->ComponentTags.Contains(NameAttr))
				AC->ComponentTags.Add(NameAttr);			
			/*
			for (int nIdx = 0; nIdx < InPropertyAttribute.AttributeCount; nIdx++)
			{
				FName NameAttr = FName(*InPropertyAttribute.GetStringValue(nIdx));
				if (!AC->ComponentTags.Contains(NameAttr))
					AC->ComponentTags.Add(NameAttr);
			}
			*/
			return true;
		}
		return false;
	}

	// Try to find the corresponding UProperty
	void* OutContainer = nullptr; 
	FProperty* FoundProperty = nullptr;
	UObject* FoundPropertyObject = nullptr;
	if (!FindPropertyOnObject(InObject, PropertyName, FoundProperty, FoundPropertyObject, OutContainer))
		return false;

	// Modify the Property we found
	if (!ModifyPropertyValueOnObject(FoundPropertyObject, InPropertyAttribute, FoundProperty, OutContainer, AtIndex))
		return false;

	return true;
}


bool
FHoudiniGenericAttribute::FindPropertyOnObject(
	UObject* InObject,
	const FString& InPropertyName,
	FProperty*& OutFoundProperty,
	UObject*& OutFoundPropertyObject,
	void*& OutContainer)
{
#if WITH_EDITOR
	if (!InObject || InObject->IsPendingKill())
		return false;

	if (InPropertyName.IsEmpty())
		return false;

	UClass* ObjectClass = InObject->GetClass();
	if (!ObjectClass || ObjectClass->IsPendingKill())
		return false;

	// Set the result pointer to null
	OutContainer = nullptr;
	OutFoundProperty = nullptr;
	OutFoundPropertyObject = InObject;

	bool bPropertyHasBeenFound = false;
	FHoudiniGenericAttribute::TryToFindProperty(
		InObject,
		ObjectClass,
		InPropertyName,
		OutFoundProperty,
		bPropertyHasBeenFound,
		OutContainer);

	/*
	// TODO: Parsing needs to be made recursively!
	// Iterate manually on the properties, in order to handle StructProperties correctly
	for (TFieldIterator<FProperty> PropIt(ObjectClass, EFieldIteratorFlags::IncludeSuper); PropIt; ++PropIt)
	{
		FProperty* CurrentProperty = *PropIt;
		if (!CurrentProperty)
			continue;

		FString DisplayName = CurrentProperty->GetDisplayNameText().ToString().Replace(TEXT(" "), TEXT(""));
		FString Name = CurrentProperty->GetName();

		// If the property name contains the uprop attribute name, we have a candidate
		if (Name.Contains(InPropertyName) || DisplayName.Contains(InPropertyName))
		{
			OutFoundProperty = CurrentProperty;

			// If it's an equality, we dont need to keep searching
			if ((Name == InPropertyName) || (DisplayName == InPropertyName))
			{
				bPropertyHasBeenFound = true;
				break;
			}
		}

		// StructProperty need to be a nested struct
		//if (UStructProperty* StructProperty = Cast< UStructProperty >(CurrentProperty))
		//	bPropertyHasBeenFound = TryToFindInStructProperty(InObject, InPropertyName, StructProperty, OutFoundProperty, OutStructContainer);
		//else if (UArrayProperty* ArrayProperty = Cast< UArrayProperty >(CurrentProperty))
		//	bPropertyHasBeenFound = TryToFindInArrayProperty(InObject, InPropertyName, ArrayProperty, OutFoundProperty, OutStructContainer);

		// Handle StructProperty
		FStructProperty* StructProperty = CastField<FStructProperty>(CurrentProperty);
		if (StructProperty)
		{
			// Walk the structs' properties and try to find the one we're looking for
			UScriptStruct* Struct = StructProperty->Struct;
			if (!Struct || Struct->IsPendingKill())
				continue;

			for (TFieldIterator<FProperty> It(Struct); It; ++It)
			{
				FProperty* Property = *It;
				if (!Property)
					continue;

				DisplayName = Property->GetDisplayNameText().ToString().Replace(TEXT(" "), TEXT(""));
				Name = Property->GetName();

				// If the property name contains the uprop attribute name, we have a candidate
				if (Name.Contains(InPropertyName) || DisplayName.Contains(InPropertyName))
				{
					// We found the property in the struct property, we need to keep the ValuePtr in the object
					// of the structProp in order to be able to access the property value afterwards...
					OutFoundProperty = Property;
					OutStructContainer = StructProperty->ContainerPtrToValuePtr< void >(InObject, 0);

					// If it's an equality, we dont need to keep searching
					if ((Name == InPropertyName) || (DisplayName == InPropertyName))
					{
						bPropertyHasBeenFound = true;
						break;
					}
				}
			}
		}

		if (bPropertyHasBeenFound)
			break;
	}

	if (bPropertyHasBeenFound)
		return true;
	*/

	// Try with FindField??
	if (!OutFoundProperty)
		OutFoundProperty = FindFProperty<FProperty>(ObjectClass, *InPropertyName);

	// Try with FindPropertyByName ??
	if (!OutFoundProperty)
		OutFoundProperty = ObjectClass->FindPropertyByName(*InPropertyName);

	// We found the Property we were looking for
	if (OutFoundProperty)
		return true;

	// Handle common properties nested in classes
	// Static Meshes
	UStaticMesh* SM = Cast<UStaticMesh>(InObject);
	if (SM && !SM->IsPendingKill())
	{
		if (SM->BodySetup && FindPropertyOnObject(
			SM->BodySetup, InPropertyName, OutFoundProperty, OutFoundPropertyObject, OutContainer))
		{
			return true;
		}

		if (SM->AssetImportData && FindPropertyOnObject(
			SM->AssetImportData, InPropertyName, OutFoundProperty, OutFoundPropertyObject, OutContainer))
		{
			return true;
		}

		if (SM->NavCollision && FindPropertyOnObject(
			SM->NavCollision, InPropertyName, OutFoundProperty, OutFoundPropertyObject, OutContainer))
		{
			return true;
		}
	}

	// For Actors, parse their components
	AActor* Actor = Cast<AActor>(InObject);
	if (Actor && !Actor->IsPendingKill())
	{
		TArray<USceneComponent*> AllComponents;
		Actor->GetComponents<USceneComponent>(AllComponents, true);

		int32 CompIdx = 0;
		for (USceneComponent * SceneComponent : AllComponents)
		{
			if (!SceneComponent || SceneComponent->IsPendingKill())
				continue;

			if (FindPropertyOnObject(
				SceneComponent, InPropertyName, OutFoundProperty, OutFoundPropertyObject, OutContainer))
			{
				return true;
			}
		}
	}

	// We found the Property we were looking for
	if (OutFoundProperty)
		return true;

#endif
	return false;
}


bool
FHoudiniGenericAttribute::TryToFindProperty(
	void* InContainer,
	UStruct* InStruct,
	const FString& InPropertyName,
	FProperty*& OutFoundProperty,
	bool& bOutPropertyHasBeenFound,
	void*& OutContainer)
{
#if WITH_EDITOR
	if (!InContainer)
		return false;
	
	if (!InStruct || InStruct->IsPendingKill())
		return false;

	if (InPropertyName.IsEmpty())
		return false;

	// Iterate manually on the properties, in order to handle StructProperties correctly
	for (TFieldIterator<FProperty> PropIt(InStruct, EFieldIteratorFlags::IncludeSuper); PropIt; ++PropIt)
	{
		FProperty* CurrentProperty = *PropIt;
		if (!CurrentProperty)
			continue;

		FString DisplayName = CurrentProperty->GetDisplayNameText().ToString().Replace(TEXT(" "), TEXT(""));
		FString Name = CurrentProperty->GetName();

		// If the property name contains the uprop attribute name, we have a candidate
		if (Name.Contains(InPropertyName) || DisplayName.Contains(InPropertyName))
		{
			OutFoundProperty = CurrentProperty;
			OutContainer = InContainer;

			// If it's an equality, we dont need to keep searching anymore
			if ((Name == InPropertyName) || (DisplayName == InPropertyName))
			{
				bOutPropertyHasBeenFound = true;
				break;
			}
		}

		// Do a recursive parsing for StructProperties
		FStructProperty* StructProperty = CastField<FStructProperty>(CurrentProperty);
		if (StructProperty)
		{
			// Walk the structs' properties and try to find the one we're looking for
			UScriptStruct* Struct = StructProperty->Struct;
			if (!Struct || Struct->IsPendingKill())
				continue;

			TryToFindProperty(
				StructProperty->ContainerPtrToValuePtr<void>(InContainer, 0),
				Struct,
				InPropertyName,
				OutFoundProperty,
				bOutPropertyHasBeenFound,
				OutContainer);
		}

		if (bOutPropertyHasBeenFound)
			break;
	}

	if (bOutPropertyHasBeenFound)
		return true;

	// We found the Property we were looking for
	if (OutFoundProperty)
		return true;

#endif
	return false;
}


bool
FHoudiniGenericAttribute::ModifyPropertyValueOnObject(
	UObject* InObject,
	FHoudiniGenericAttribute InGenericAttribute,
	FProperty* FoundProperty,
	void* InContainer,
	const int32& InAtIndex)
{
	if (!InObject || InObject->IsPendingKill() || !FoundProperty)
		return false;

	// Determine the container to use (either InContainer if specified, or InObject)
	void* Container = InContainer ? InContainer : InObject;

	// Initialize using the found property
	FProperty* InnerProperty = FoundProperty;

	AActor* InOwner = Cast<AActor>(InObject->GetOuter());
	bool bHasModifiedProperty = false;
	

	auto OnPropertyChanged = [InObject, InOwner, &bHasModifiedProperty](FProperty* InProperty)
	{
#if WITH_EDITOR
		FPropertyChangedEvent Evt(InProperty);
		InObject->PostEditChangeProperty(Evt);
		if (InOwner)
		{
			// If we are setting properties on an Actor component, we want to notify the
			// actor of the changes too since the property change might be handled in the actor's
			// PostEditChange callbacks (one such an example occurs when changing the material for a decal actor).
			InOwner->PostEditChangeProperty(Evt);
		}
#endif
		bHasModifiedProperty = true;
	};

	int32 NumberOfProperties = 1;
	FArrayProperty* ArrayProperty = CastField<FArrayProperty>(FoundProperty);
	if (ArrayProperty)
	{
		InnerProperty = ArrayProperty->Inner;
		NumberOfProperties = ArrayProperty->ArrayDim;

		// Do we need to add values to the array?
		FScriptArrayHelper_InContainer ArrayHelper(ArrayProperty, Container);

		//ArrayHelper.ExpandForIndex( InGenericAttribute.AttributeTupleSize - 1 );
		if (InGenericAttribute.AttributeTupleSize > NumberOfProperties)
		{
			ArrayHelper.Resize(InGenericAttribute.AttributeTupleSize);
			NumberOfProperties = InGenericAttribute.AttributeTupleSize;
		}
	}

	// Get the "proper" AtIndex in the flat array by using the attribute tuple size
	// TODO: fix the issue when changing array of tuple properties!
	int32 AtIndex = InAtIndex * InGenericAttribute.AttributeTupleSize;

	for (int32 nPropIdx = 0; nPropIdx < NumberOfProperties; nPropIdx++)
	{
		if (FFloatProperty* FloatProperty = CastField<FFloatProperty>(InnerProperty))
		{
			// FLOAT PROPERTY
			if (InGenericAttribute.AttributeType == EAttribStorageType::STRING)
			{
				FString Value = InGenericAttribute.GetStringValue(AtIndex + nPropIdx);
				void * ValuePtr = nullptr;
				if (ArrayProperty)
				{
					FScriptArrayHelper_InContainer ArrayHelper(ArrayProperty, Container);
					ValuePtr = ArrayHelper.GetRawPtr(nPropIdx);
				}
				else
				{
					ValuePtr = InnerProperty->ContainerPtrToValuePtr<FString>(Container, nPropIdx);
				}

				if (ValuePtr)
				{
					FloatProperty->SetNumericPropertyValueFromString(ValuePtr, *Value);
					OnPropertyChanged(FloatProperty);
				}
			}
			else
			{
				double Value = InGenericAttribute.GetDoubleValue(AtIndex + nPropIdx);
				void * ValuePtr = nullptr;
				if (ArrayProperty)
				{
					FScriptArrayHelper_InContainer ArrayHelper(ArrayProperty, Container);
					ValuePtr = ArrayHelper.GetRawPtr(nPropIdx);
				}
				else
				{
					ValuePtr = InnerProperty->ContainerPtrToValuePtr<float>(Container, nPropIdx);
				}

				if (ValuePtr)
				{
					FloatProperty->SetFloatingPointPropertyValue(ValuePtr, Value);
					OnPropertyChanged(FloatProperty);
				}
			}
		}
		else if (FIntProperty* IntProperty = CastField<FIntProperty>(InnerProperty))
		{
			// INT PROPERTY
			if (InGenericAttribute.AttributeType == EAttribStorageType::STRING)
			{
				FString Value = InGenericAttribute.GetStringValue(AtIndex + nPropIdx);
				void * ValuePtr = nullptr;
				if (ArrayProperty)
				{
					FScriptArrayHelper_InContainer ArrayHelper(ArrayProperty, Container);
					ValuePtr = ArrayHelper.GetRawPtr(nPropIdx);
				}
				else
				{
					ValuePtr = InnerProperty->ContainerPtrToValuePtr<FString>(Container, nPropIdx);
				}

				if (ValuePtr)
				{
					IntProperty->SetNumericPropertyValueFromString(ValuePtr, *Value);
					OnPropertyChanged(IntProperty);
				}
			}
			else
			{
				int64 Value = InGenericAttribute.GetIntValue(AtIndex + nPropIdx);
				void * ValuePtr = nullptr;
				if (ArrayProperty)
				{
					FScriptArrayHelper_InContainer ArrayHelper(ArrayProperty, Container);
					ValuePtr = ArrayHelper.GetRawPtr(nPropIdx);
				}
				else
				{
					ValuePtr = InnerProperty->ContainerPtrToValuePtr<int64>(Container, nPropIdx);
				}

				if (ValuePtr)
				{
					IntProperty->SetIntPropertyValue(ValuePtr, Value);
					OnPropertyChanged(IntProperty);
				}
			}
		}
		else if (FBoolProperty* BoolProperty = CastField<FBoolProperty>(InnerProperty))
		{
			// BOOL PROPERTY
			bool Value = InGenericAttribute.GetBoolValue(AtIndex + nPropIdx);
				void * ValuePtr = nullptr;
				if (ArrayProperty)
				{
					FScriptArrayHelper_InContainer ArrayHelper(ArrayProperty, Container);
					ValuePtr = ArrayHelper.GetRawPtr(nPropIdx);
				}
				else
				{
					ValuePtr = InnerProperty->ContainerPtrToValuePtr<bool>(Container, nPropIdx);
				}

			if (ValuePtr)
			{
				BoolProperty->SetPropertyValue(ValuePtr, Value);
				OnPropertyChanged(BoolProperty);
			}
		}
		else if (FStrProperty* StringProperty = CastField<FStrProperty>(InnerProperty))
		{
			// STRING PROPERTY
			FString Value = InGenericAttribute.GetStringValue(AtIndex + nPropIdx);
				void * ValuePtr = nullptr;
				if (ArrayProperty)
				{
					FScriptArrayHelper_InContainer ArrayHelper(ArrayProperty, Container);
					ValuePtr = ArrayHelper.GetRawPtr(nPropIdx);
				}
				else
				{
					ValuePtr = InnerProperty->ContainerPtrToValuePtr<FString>(Container, nPropIdx);
				}

			if (ValuePtr)
			{
				StringProperty->SetPropertyValue(ValuePtr, Value);
				OnPropertyChanged(StringProperty);
			}
		}
		else if (FNumericProperty *NumericProperty = CastField<FNumericProperty>(InnerProperty))
		{
			// NUMERIC PROPERTY
			if (InGenericAttribute.AttributeType == EAttribStorageType::STRING)
			{
				FString Value = InGenericAttribute.GetStringValue(AtIndex + nPropIdx);
				void * ValuePtr = nullptr;
				if (ArrayProperty)
				{
					FScriptArrayHelper_InContainer ArrayHelper(ArrayProperty, Container);
					ValuePtr = ArrayHelper.GetRawPtr(nPropIdx);
				}
				else
				{
					ValuePtr = InnerProperty->ContainerPtrToValuePtr<FString>(Container, nPropIdx);
				}

				if (ValuePtr)
				{
					NumericProperty->SetNumericPropertyValueFromString(ValuePtr, *Value);
					OnPropertyChanged(NumericProperty);
				}
			}
			else if (NumericProperty->IsInteger())
			{
				int64 Value = InGenericAttribute.GetIntValue(AtIndex + nPropIdx);
				void * ValuePtr = nullptr;
				if (ArrayProperty)
				{
					FScriptArrayHelper_InContainer ArrayHelper(ArrayProperty, Container);
					ValuePtr = ArrayHelper.GetRawPtr(nPropIdx);
				}
				else
				{
					ValuePtr = InnerProperty->ContainerPtrToValuePtr<int64>(Container, nPropIdx);
				}

				if (ValuePtr)
				{
					NumericProperty->SetIntPropertyValue(ValuePtr, (int64)Value);
					OnPropertyChanged(NumericProperty);
				}
			}
			else if (NumericProperty->IsFloatingPoint())
			{
				double Value = InGenericAttribute.GetDoubleValue(AtIndex + nPropIdx);
				void * ValuePtr = nullptr;
				if (ArrayProperty)
				{
					FScriptArrayHelper_InContainer ArrayHelper(ArrayProperty, Container);
					ValuePtr = ArrayHelper.GetRawPtr(nPropIdx);
				}
				else
				{
					ValuePtr = InnerProperty->ContainerPtrToValuePtr<float>(Container, nPropIdx);
				}

				if (ValuePtr)
				{
					NumericProperty->SetFloatingPointPropertyValue(ValuePtr, Value);
					OnPropertyChanged(NumericProperty);
				}
			}
			else
			{
				// Numeric Property was found, but is of an unsupported type
				HOUDINI_LOG_MESSAGE(TEXT("Unsupported Numeric UProperty"));
			}
		}
		else if (FNameProperty* NameProperty = CastField<FNameProperty>(InnerProperty))
		{
			// NAME PROPERTY
			FString StringValue = InGenericAttribute.GetStringValue(AtIndex + nPropIdx);
			FName Value = FName(*StringValue);

			void * ValuePtr = nullptr;
			if (ArrayProperty)
			{
				FScriptArrayHelper_InContainer ArrayHelper(ArrayProperty, Container);
				ValuePtr = ArrayHelper.GetRawPtr(nPropIdx);
			}
			else
			{
				ValuePtr = InnerProperty->ContainerPtrToValuePtr<FName>(Container, nPropIdx);
			}

			if (ValuePtr)
			{
				NameProperty->SetPropertyValue(ValuePtr, Value);
				OnPropertyChanged(NameProperty);
			}
		}
		else if (FStructProperty* StructProperty = CastField<FStructProperty>(InnerProperty))
		{
			// STRUCT PROPERTY
			const FName PropertyName = StructProperty->Struct->GetFName();
			if (PropertyName == NAME_Vector)
			{
				FVector* PropertyValue = nullptr;
				if (ArrayProperty)
				{
					FScriptArrayHelper_InContainer ArrayHelper(ArrayProperty, Container);
					PropertyValue = reinterpret_cast<FVector*>(ArrayHelper.GetRawPtr(nPropIdx));
				}
				else
				{
					PropertyValue = InnerProperty->ContainerPtrToValuePtr<FVector>(Container, nPropIdx);
				}

				if (PropertyValue)
				{
					// Found a vector property, fill it with the 3 tuple values
					PropertyValue->X = InGenericAttribute.GetDoubleValue(AtIndex + nPropIdx + 0);
					PropertyValue->Y = InGenericAttribute.GetDoubleValue(AtIndex + nPropIdx + 1);
					PropertyValue->Z = InGenericAttribute.GetDoubleValue(AtIndex + nPropIdx + 2);
					OnPropertyChanged(StructProperty);
				}
			}
			else if (PropertyName == NAME_Transform)
			{
				FTransform* PropertyValue = nullptr;
				if (ArrayProperty)
				{
					FScriptArrayHelper_InContainer ArrayHelper(ArrayProperty, Container);
					PropertyValue = reinterpret_cast<FTransform*>(ArrayHelper.GetRawPtr(nPropIdx));
				}
				else
				{
					PropertyValue = InnerProperty->ContainerPtrToValuePtr<FTransform>(Container, nPropIdx);
				}

				if (PropertyValue)
				{
					// Found a transform property fill it with the attribute tuple values
					FVector Translation;
					Translation.X = InGenericAttribute.GetDoubleValue(AtIndex + nPropIdx + 0);
					Translation.Y = InGenericAttribute.GetDoubleValue(AtIndex + nPropIdx + 1);
					Translation.Z = InGenericAttribute.GetDoubleValue(AtIndex + nPropIdx + 2);

					FQuat Rotation;
					Rotation.W = InGenericAttribute.GetDoubleValue(AtIndex + nPropIdx + 3);
					Rotation.X = InGenericAttribute.GetDoubleValue(AtIndex + nPropIdx + 4);
					Rotation.Y = InGenericAttribute.GetDoubleValue(AtIndex + nPropIdx + 5);
					Rotation.Z = InGenericAttribute.GetDoubleValue(AtIndex + nPropIdx + 6);

					FVector Scale;
					Scale.X = InGenericAttribute.GetDoubleValue(AtIndex + nPropIdx + 7);
					Scale.Y = InGenericAttribute.GetDoubleValue(AtIndex + nPropIdx + 8);
					Scale.Z = InGenericAttribute.GetDoubleValue(AtIndex + nPropIdx + 9);

					PropertyValue->SetTranslation(Translation);
					PropertyValue->SetRotation(Rotation);
					PropertyValue->SetScale3D(Scale);

					OnPropertyChanged(StructProperty);
				}
			}
			else if (PropertyName == NAME_Color)
			{
				FColor* PropertyValue = nullptr;
				if (ArrayProperty)
				{
					FScriptArrayHelper_InContainer ArrayHelper(ArrayProperty, Container);
					PropertyValue = reinterpret_cast<FColor*>(ArrayHelper.GetRawPtr(nPropIdx));
				}
				else
				{
					PropertyValue = InnerProperty->ContainerPtrToValuePtr<FColor>(Container, nPropIdx);
				}

				if (PropertyValue)
				{
					PropertyValue->R = (int8)InGenericAttribute.GetIntValue(AtIndex + nPropIdx);
					PropertyValue->G = (int8)InGenericAttribute.GetIntValue(AtIndex + nPropIdx + 1);
					PropertyValue->B = (int8)InGenericAttribute.GetIntValue(AtIndex + nPropIdx + 2);
					if (InGenericAttribute.AttributeTupleSize == 4)
						PropertyValue->A = (int8)InGenericAttribute.GetIntValue(AtIndex + nPropIdx + 3);

					OnPropertyChanged(StructProperty);
				}
			}
			else if (PropertyName == NAME_LinearColor)
			{
				FLinearColor* PropertyValue = nullptr;
				if (ArrayProperty)
				{
					FScriptArrayHelper_InContainer ArrayHelper(ArrayProperty, Container);
					PropertyValue = reinterpret_cast<FLinearColor*>(ArrayHelper.GetRawPtr(nPropIdx));
				}
				else
				{
					PropertyValue = InnerProperty->ContainerPtrToValuePtr<FLinearColor>(Container, nPropIdx);
				}

				if (PropertyValue)
				{
					PropertyValue->R = (float)InGenericAttribute.GetDoubleValue(AtIndex + nPropIdx);
					PropertyValue->G = (float)InGenericAttribute.GetDoubleValue(AtIndex + nPropIdx + 1);
					PropertyValue->B = (float)InGenericAttribute.GetDoubleValue(AtIndex + nPropIdx + 2);
					if (InGenericAttribute.AttributeTupleSize == 4)
						PropertyValue->A = (float)InGenericAttribute.GetDoubleValue(AtIndex + nPropIdx + 3);

					OnPropertyChanged(StructProperty);
				}
			}
			else if (PropertyName == "Int32Interval")
			{
				FInt32Interval* PropertyValue = nullptr;
				if (ArrayProperty)
				{
					FScriptArrayHelper_InContainer ArrayHelper(ArrayProperty, Container);
					PropertyValue = reinterpret_cast<FInt32Interval*>(ArrayHelper.GetRawPtr(nPropIdx));
				}
				else
				{
					PropertyValue = InnerProperty->ContainerPtrToValuePtr<FInt32Interval>(Container, nPropIdx);
				}

				if (PropertyValue)
				{
					PropertyValue->Min = (float)InGenericAttribute.GetIntValue(AtIndex + nPropIdx);
					PropertyValue->Max = (float)InGenericAttribute.GetIntValue(AtIndex + nPropIdx + 1);

					OnPropertyChanged(StructProperty);
				}
			}
			else if (PropertyName == "FloatInterval")
			{
				FFloatInterval* PropertyValue = nullptr;
				if (ArrayProperty)
				{
					FScriptArrayHelper_InContainer ArrayHelper(ArrayProperty, Container);
					PropertyValue = reinterpret_cast<FFloatInterval*>(ArrayHelper.GetRawPtr(nPropIdx));
				}
				else
				{
					PropertyValue = InnerProperty->ContainerPtrToValuePtr<FFloatInterval>(Container, nPropIdx);
				}

				if (PropertyValue)
				{
					PropertyValue->Min = (float)InGenericAttribute.GetDoubleValue(AtIndex + nPropIdx);
					PropertyValue->Max = (float)InGenericAttribute.GetDoubleValue(AtIndex + nPropIdx + 1);

					OnPropertyChanged(StructProperty);
				}
			}
		}
		else if (FObjectProperty* ObjectProperty = CastField<FObjectProperty>(InnerProperty))
		{
			// OBJECT PATH PROPERTY
			FString Value = InGenericAttribute.GetStringValue(AtIndex + nPropIdx);
			void * ValuePtr = nullptr;
			if (ArrayProperty)
			{
				FScriptArrayHelper_InContainer ArrayHelper(ArrayProperty, Container);
				ValuePtr = ArrayHelper.GetRawPtr(nPropIdx);
			}
			else
			{
				ValuePtr = InnerProperty->ContainerPtrToValuePtr<FString>(Container, nPropIdx);
			}

			if (ValuePtr)
			{
				TSoftObjectPtr<UObject> ValueObjectPtr;
				ValueObjectPtr = Value;
				UObject* ValueObject = ValueObjectPtr.LoadSynchronous();

				// Ensure the ObjectProperty class matches the ValueObject that we just loaded
				if (!ValueObject || (ValueObject && ValueObject->IsA(ObjectProperty->PropertyClass)))
				{
					ObjectProperty->SetObjectPropertyValue(ValuePtr, ValueObject);
					OnPropertyChanged(ObjectProperty);
				}
			}
		}
		else
		{
			// Property was found, but is of an unsupported type
			FString PropertyClass = FoundProperty->GetClass() ? FoundProperty->GetClass()->GetName() : TEXT("Unknown");
			HOUDINI_LOG_MESSAGE(TEXT("Unsupported UProperty Class: %s found for uproperty %s"), *PropertyClass, *InGenericAttribute.AttributeName);
			return false;
		}
	}

	if (bHasModifiedProperty)
	{
#if WITH_EDITOR
		InObject->PostEditChange();
		if (InOwner)
		{
			InOwner->PostEditChange();
		}
#endif
	}

	return true;
}

/*
bool
FHoudiniEngineUtils::TryToFindInStructProperty(
	UObject* Object, FString UPropertyNameToFind, UStructProperty* StructProperty, UProperty*& FoundProperty, void*& StructContainer )
{
	if ( !StructProperty || !Object )
		return false;

	// Walk the structs' properties and try to find the one we're looking for
	UScriptStruct* Struct = StructProperty->Struct;
	for (TFieldIterator< UProperty > It(Struct); It; ++It)
	{
		UProperty* Property = *It;
		if ( !Property )
			continue;

		FString DisplayName = It->GetDisplayNameText().ToString().Replace(TEXT(" "), TEXT(""));
		FString Name = It->GetName();

		// If the property name contains the uprop attribute name, we have a candidate
		if ( Name.Contains( UPropertyNameToFind ) || DisplayName.Contains( UPropertyNameToFind ) )
		{
			// We found the property in the struct property, we need to keep the ValuePtr in the object
			// of the structProp in order to be able to access the property value afterwards...
			FoundProperty = Property;
			StructContainer = StructProperty->ContainerPtrToValuePtr< void >( Object, 0);

			// If it's an equality, we dont need to keep searching
			if ( ( Name == UPropertyNameToFind ) || ( DisplayName == UPropertyNameToFind ) )
				return true;
		}

		if ( FoundProperty )
			continue;

		UStructProperty* NestedStruct = Cast<UStructProperty>( Property );
		if ( NestedStruct && TryToFindInStructProperty( Object, UPropertyNameToFind, NestedStruct, FoundProperty, StructContainer ) )
			return true;

		UArrayProperty* ArrayProp = Cast<UArrayProperty>( Property );
		if ( ArrayProp && TryToFindInArrayProperty( Object, UPropertyNameToFind, ArrayProp, FoundProperty, StructContainer ) )
			return true;
	}

	return false;
}

bool
FHoudiniEngineUtils::TryToFindInArrayProperty(
	UObject* Object, FString UPropertyNameToFind, UArrayProperty* ArrayProperty, UProperty*& FoundProperty, void*& StructContainer )
{
	if ( !ArrayProperty || !Object )
		return false;

	UProperty* Property = ArrayProperty->Inner;
	if ( !Property )
		return false;

	FString DisplayName = Property->GetDisplayNameText().ToString().Replace(TEXT(" "), TEXT(""));
	FString Name = Property->GetName();

	// If the property name contains the uprop attribute name, we have a candidate
	if ( Name.Contains( UPropertyNameToFind ) || DisplayName.Contains( UPropertyNameToFind ) )
	{
		// We found the property in the struct property, we need to keep the ValuePtr in the object
		// of the structProp in order to be able to access the property value afterwards...
		FoundProperty = Property;
		StructContainer = ArrayProperty->ContainerPtrToValuePtr< void >( Object, 0);

		// If it's an equality, we dont need to keep searching
		if ( ( Name == UPropertyNameToFind ) || ( DisplayName == UPropertyNameToFind ) )
			return true;
	}

	if ( !FoundProperty )
	{
		UStructProperty* NestedStruct = Cast<UStructProperty>( Property );
		if ( NestedStruct && TryToFindInStructProperty( ArrayProperty, UPropertyNameToFind, NestedStruct, FoundProperty, StructContainer ) )
			return true;

		UArrayProperty* ArrayProp = Cast<UArrayProperty>( Property );
		if ( ArrayProp && TryToFindInArrayProperty( ArrayProperty, UPropertyNameToFind, ArrayProp, FoundProperty, StructContainer ) )
			return true;
	}

	return false;
}
*/