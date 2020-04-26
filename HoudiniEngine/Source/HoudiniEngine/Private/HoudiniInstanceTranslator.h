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

//#include "HAPI/HAPI_Common.h"
#include "HoudiniOutput.h"

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"

class UStaticMesh;
struct FHoudiniGenericAttribute;
class UHoudiniStaticMesh;

struct HOUDINIENGINE_API FHoudiniInstanceTranslator
{
	public:

		static bool CreateAllInstancersFromHoudiniOutput(
			UHoudiniOutput* InOutput,
			const TArray<UHoudiniOutput*>& InAllOutputs,
			UObject* InOuterComponent);

		static bool GetInstancerObjectsAndTransforms(
			const FHoudiniGeoPartObject& InHGPO,
			const TArray<UHoudiniOutput*>& InAllOutputs,
			TArray<UObject*>& OutInstancedObjects,
			TArray<TArray<FTransform>>& OutInstancedTransforms);

		static bool GetPackedPrimitiveInstancerHGPOsAndTransforms(
			const FHoudiniGeoPartObject& InHGPO,
			TArray<FHoudiniGeoPartObject>& OutInstancedHGPO,
			TArray<TArray<FTransform>>& OutInstancedTransforms);

		static bool GetAttributeInstancerObjectsAndTransforms(
			const FHoudiniGeoPartObject& InHGPO,
			TArray<UObject*>& OutInstancedObjects,
			TArray<TArray<FTransform>>& OutInstancedTransforms);

		static bool GetOldSchoolAttributeInstancerHGPOsAndTransforms(
			const FHoudiniGeoPartObject& InHGPO,
			const TArray<UHoudiniOutput*>& InAllOutputs,
			TArray<FHoudiniGeoPartObject>& OutInstancedHGPO,
			TArray<TArray<FTransform>>& OutInstancedTransforms);

		static bool GetObjectInstancerHGPOsAndTransforms(
			const FHoudiniGeoPartObject& InHGPO,
			const TArray<UHoudiniOutput*>& InAllOutputs,
			TArray<FHoudiniGeoPartObject>& OutInstancedHGPO,
			TArray<TArray<FTransform>>& OutInstancedTransforms);

		// Updates the variations array using the instanced outputs
		static void UpdateInstanceVariationObjects(
			const FHoudiniOutputObjectIdentifier& InOutputIdentifier,
			const TArray<UObject*>& InOriginalObjects,
			const TArray<TArray<FTransform>>& InOriginalTransforms,
			TMap<FHoudiniOutputObjectIdentifier, FHoudiniInstancedOutput>& InstancedOutputs,
			TArray<TSoftObjectPtr<UObject>>& OutVariationsInstancedObjects,
			TArray<TArray<FTransform>>& OutVariationsInstancedTransforms,
			TArray<int32>& OutVariationOriginalObjectIdx,
			TArray<int32>& OutVariationIndices);

		// Recreates the components after an instanced outputs has been changed
		static bool UpdateChangedInstancedOutput(
			FHoudiniInstancedOutput& InInstancedOutput,
			const FHoudiniOutputObjectIdentifier& OutputIdentifier,
			UHoudiniOutput* InParentOutput,
			USceneComponent* InParentComponent);

		// Recomputes the variation assignements for a given instanced output
		static void UpdateVariationAssignements(FHoudiniInstancedOutput& InstancedOutput);

		// Extracts the final transforms (with the transform offset applied) for a given variation
		static void ProcessInstanceTransforms(
			FHoudiniInstancedOutput& InstancedOutput, const int32& VariationIdx, TArray<FTransform>& OutProcessedTransforms);

		// Creates a new component or updates the previous one if possible
		static bool CreateOrUpdateInstanceComponent(
			UObject* InstancedObject,
			const TArray<FTransform>& InstancedObjectTransforms,
			const TArray<FHoudiniGenericAttribute>& AllPropertyAttributes,
			const FHoudiniGeoPartObject& InstancerGeoPartObject,			
			USceneComponent* ParentComponent,
			USceneComponent* OldComponent,
			USceneComponent*& NewComponent,
			const bool& InIsSplitMeshInstancer,
			const TArray<UMaterialInterface *>& InstancerMaterials,
			const int32& InstancerObjectIdx = 0);

		// Create or update an ISMC / HISMC
		static bool CreateOrUpdateInstancedStaticMeshComponent(
			UStaticMesh* InstancedStaticMesh,
			const TArray<FTransform>& InstancedObjectTransforms,
			const TArray<FHoudiniGenericAttribute>& AllPropertyAttributes,
			const FHoudiniGeoPartObject& InstancerGeoPartObject,
			USceneComponent* ParentComponent,
			USceneComponent*& CreatedInstancedComponent,
			UMaterialInterface * InstancerMaterial = nullptr);

		// Create or update an IAC
		static bool CreateOrUpdateInstancedActorComponent(
			UObject* InstancedObject,
			const TArray<FTransform>& InstancedObjectTransforms,
			const TArray<FHoudiniGenericAttribute>& AllPropertyAttributes,
			USceneComponent* ParentComponent,
			USceneComponent*& CreatedInstancedComponent);

		// Create or update a MeshSplitInstancer
		static bool CreateOrUpdateMeshSplitInstancerComponent(
			UStaticMesh* InstancedStaticMesh,
			const TArray<FTransform>& InstancedObjectTransforms,
			const TArray<FHoudiniGenericAttribute>& AllPropertyAttributes,
			const FHoudiniGeoPartObject& InstancerGeoPartObject,
			USceneComponent* ParentComponent,
			USceneComponent*& CreatedInstancedComponent,
			const TArray<UMaterialInterface *>& InstancerMaterials);

		// Create or update a StaticMeshComponent (when we have only one instance)
		static bool CreateOrUpdateStaticMeshComponent(
			UStaticMesh* InstancedStaticMesh,
			const TArray<FTransform>& InstancedObjectTransforms,
			const TArray<FHoudiniGenericAttribute>& AllPropertyAttributes,
			const FHoudiniGeoPartObject& InstancerGeoPartObject,
			USceneComponent* ParentComponent,
			USceneComponent*& CreatedInstancedComponent,
			UMaterialInterface * InstancerMaterial = nullptr);

		// Create or update a HoudiniStaticMeshComponent (when we have only one instance)
		static bool CreateOrUpdateHoudiniStaticMeshComponent(
			UHoudiniStaticMesh* InstancedProxyStaticMesh,
			const TArray<FTransform>& InstancedObjectTransforms,
			const TArray<FHoudiniGenericAttribute>& AllPropertyAttributes,
			const FHoudiniGeoPartObject& InstancerGeoPartObject,
			USceneComponent* ParentComponent,
			USceneComponent*& CreatedInstancedComponent,
			UMaterialInterface * InstancerMaterial = nullptr);

		// Helper fumction to properly remove/destroy a component
		static bool RemoveAndDestroyComponent(UObject* InComponent);

		// Utility function
		// Fetches instance transforms and convert them to ue4 coordinates
		static bool HapiGetInstanceTransforms(
			const FHoudiniGeoPartObject& InHGPO,
			TArray<FTransform>& OutInstancerUnrealTransforms);

		// Helper functions for generic property attributes
		static bool GetGenericPropertiesAttributes(
			const int32& InGeoNodeId, const int32& InPartId,
			TArray<FHoudiniGenericAttribute>& OutPropertyAttributes);

		static bool UpdateGenericPropertiesAttributes(
			UObject* InObject, const TArray<FHoudiniGenericAttribute>& InAllPropertyAttributes, const int32& AtIndex);

		static bool GetMaterialOverridesFromAttributes(
			const int32& InGeoNodeId, const int32& InPartId, TArray<FString>& OutMaterialAttributes);

		static bool GetInstancerMaterials(
			const int32& InGeoNodeId, const int32& InPartId, TArray<UMaterialInterface*>& OutInstancerMaterials);

		static bool GetVariationMaterials(
			FHoudiniInstancedOutput* InInstancedOutput, const int32& InVariationIndex,
			const TArray<UMaterialInterface*>& InInstancerMaterials, TArray<UMaterialInterface*>& OutVariationMaterials);

		static bool IsSplitInstancer(const int32& InGeoId, const int32& InPartId);

};