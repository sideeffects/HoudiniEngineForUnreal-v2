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


#include "HoudiniAssetBlueprintComponent.h"
#include "HoudiniOutput.h"
#include "HoudiniEngineRuntime.h"
#include "HoudiniEngineRuntimeUtils.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "UObject/Object.h"

#include "HoudiniParameterFloat.h"
#include "HoudiniParameterToggle.h"
#include "HoudiniInput.h"

#if WITH_EDITOR
	#include "Editor.h"
	#include "Kismet2/BlueprintEditorUtils.h"
	#include "Kismet2/KismetEditorUtilities.h"
	#include "Toolkits/AssetEditorManager.h"
	#include "Kismet2/ComponentEditorUtils.h"
	#include "ComponentAssetBroker.h"
#endif

UHoudiniAssetBlueprintComponent::UHoudiniAssetBlueprintComponent(const FObjectInitializer & ObjectInitializer)
	: Super(ObjectInitializer)
{
	/*
	AActor* Owner = GetOwner();
	if (Owner)
	{

		UWorld* OwnerWorld = Owner->GetWorld();
		if (OwnerWorld)
		{

		}
	}
	*/
	//if (IsTemplate())
	//{

	//	// We want template components to be registered with the HER as well.
	//	FHoudiniEngineRuntime::Get().RegisterHoudiniComponent(this, true);
	//}

#if WITH_EDITOR
	if (IsTemplate())
	{
		// CachedAssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
		//GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OnAssetEditorRequestClose().AddUObject( this, &UHoudiniAssetBlueprintComponent::ReceivedAssetEditorRequestCloseEvent );
	}
#endif

	bForceNeedUpdate = false;
	bHoudiniAssetChanged = false;
	bIsInBlueprintEditor = false;
	bCanDeleteHoudiniNodes = false;

	// AssetState will be updated by changes to the HoudiniAsset
	// or parameter changes on the Component template.
	AssetState = EHoudiniAssetState::None;
	bHasRegisteredComponentTemplate = false;
	bHasBeenLoaded = false;

	// Disable proxy mesh by default (unsupported for now)
	bOverrideGlobalProxyStaticMeshSettings = true;
	bEnableProxyStaticMeshOverride = false;
	bEnableProxyStaticMeshRefinementByTimerOverride = false;
	bEnableProxyStaticMeshRefinementOnPreSaveWorldOverride = false;
	bEnableProxyStaticMeshRefinementOnPreBeginPIEOverride = false;
	StaticMeshMethod = EHoudiniStaticMeshMethod::RawMesh;

	// Set default mobility to Movable
	Mobility = EComponentMobility::Movable;
}

#if WITH_EDITOR
void
UHoudiniAssetBlueprintComponent::CopyStateToTemplateComponent()
{
	// We need to propagate changes made here back to the corresponding component in 
	// the Blueprint Generated Class ("<COMPONENT_NAME>_GEN_VARIABLE"). The reason being that
	// the Blueprint editor works directly with the GEN_VARIABLE component (all 
	// PostEditChange() calls, Details Customizations, etc will receive the GEN_VARIABLE instance) BUT
	// when the Editor runs the construction script it uses a different component instance, so all changes
	// made to that instance won't write back to the Blueprint definition.
	// To Summarize:
	// Be sure to sync the Parameters array (and any other relevant properties) back
	// to the corresponding component on the Blueprint Generated class otherwise these wont be 
	// accessible in the Details Customization callbacks.

	// This should never be called by component templates. 
	check(!IsTemplate());

	if (!CachedTemplateComponent.IsValid())
		return;

	USimpleConstructionScript* SCS = CachedBlueprint->SimpleConstructionScript;
	check(SCS);

	/*
	USCS_Node* SCSNodeForInstance = FindSCSNodeForInstanceComponent(SCS, this);
	if (SCSNodeForInstance)
	{
	
	}
	else 
	{
	
	}
	*/

	//// If we don't have an SCS node for this preview instance, we need to create one, regardless
	//// of whether output updates are required.
	//if (!CachedTemplateComponent->bOutputsRequireUpdate && SCSNodeForInstance != nullptr)
	//	return;

	// TODO: If the blueprint editor is NOT open, then we shouldn't attempting 
	// to copy state back to the BPGC at all!
	FBlueprintEditor* BlueprintEditor = FHoudiniEngineRuntimeUtils::GetBlueprintEditor(this);
	check(BlueprintEditor);

	TSharedPtr<SSCSEditor> SCSEditor = BlueprintEditor->GetSCSEditor();
	check(SCSEditor);	
	
	USCS_Node* SCSHACNode = FindSCSNodeForTemplateComponentInClassHierarchy(CachedTemplateComponent.Get());
	// check(SCSHACNode);
	
	// This is the actor instance that is being used for component editing.
	AActor* PreviewActor = GetPreviewActor();
	check(PreviewActor);
	
	// NOTE: Inputs are only from component templates to instances, not the other way around ... I think.

	// -----------------------------------------------------
	// Copy outputs to component template
	// -----------------------------------------------------

	// Populate / update the outputs for the template from the preview / instance.
	// TODO: Wrap the Blueprint manipulation in a transaction
	TArray<UHoudiniOutput*>& TemplateOutputs = CachedTemplateComponent->Outputs;
	TSet<UHoudiniOutput*> StaleTemplateOutputs(TemplateOutputs);

	TemplateOutputs.SetNum(Outputs.Num());
	CachedOutputNodes.Empty();

	bool bBlueprintStructureModified = false;
	for (int i = 0; i < Outputs.Num(); i++)
	{
		// Find a output on the template that corresponds to this output from the instance.
		UHoudiniOutput* TemplateOutput = nullptr;
		UHoudiniOutput* InstanceOutput = nullptr;
		InstanceOutput = Outputs[i];
		
		check(InstanceOutput)
		// Ensure that instance outputs won't delete houdini content. 
		// Houdini content should only be allowed to be deleted from 
		// the component template.
		InstanceOutput->SetCanDeleteHoudiniNodes(false);
		
		TemplateOutput = TemplateOutputs[i];

		if (TemplateOutput)
		{
			check(TemplateOutput->GetOuter() == CachedTemplateComponent.Get());
			StaleTemplateOutputs.Remove(TemplateOutput);
		}

		
		if (TemplateOutput)
		{
			// Copy properties from the current instance component while preserving output objects
			// and instanced outputs.
			TemplateOutput->CopyPropertiesFrom(InstanceOutput, true);
		}
		else
		{
			// NOTE: If the template output is NULL it means that the HDA spawned a new component / output in the transient world
			// and the new output object needs to be copied back to the BPGC.

			// Corresponding template output could not be found. Create one by duplicating the instance output.
			TemplateOutput = InstanceOutput->DuplicateAndCopyProperties(CachedTemplateComponent.Get(), FName(*InstanceOutput->GetName()));
			// Treat these the same one would components created by CreateDefaultSubObject.
			// NOTE: CreateDefaultSubobject performs lots of checks, and unfortunately we can't use it directly (it is 
			// only allowed to be used in a constructor). Not sure whether we need to either. For now, we just set the
			// object flags to be similar to components created by CreateDefaultSubobject.
			TemplateOutput->SetFlags(RF_Public|RF_ArchetypeObject|RF_DefaultSubObject);
			TemplateOutputs[i] = TemplateOutput;
		}
		
		check(TemplateOutput);
		TemplateOutput->SetCanDeleteHoudiniNodes(false);

		// Keep track of potential stale output objects on the template component, for this output.
		TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& TemplateOutputObjects = TemplateOutput->GetOutputObjects();
		TArray<FHoudiniOutputObjectIdentifier> StaleTemplateObjects;
		TemplateOutputObjects.GetKeys(StaleTemplateObjects);

		for (auto& Entry : InstanceOutput->GetOutputObjects())
		{
		
			// Prepare the FHoudiniOutputObject for the template component
			const FHoudiniOutputObject& InstanceObj = Entry.Value;
			FHoudiniOutputObject TemplateObj;

			// Any output present in the Instance Outputs should be
			// transferred to the template.
			// Remove this output object from stale outputs list.
			StaleTemplateObjects.Remove(Entry.Key);

			if (TemplateOutputObjects.Contains(Entry.Key))
			{
				// Reuse the existing template object
				TemplateObj = TemplateOutputObjects.FindChecked(Entry.Key);
			}
			else 
			{
				// Create a new template output object object by duplicating the instance object.
				// Keep the output object, but clear the output component since we have to 
				// create a new component template.
				TemplateObj = InstanceObj;
				TemplateObj.ProxyComponent = nullptr;
				TemplateObj.OutputComponent = nullptr;
				TemplateObj.ProxyObject = nullptr;
			}

			USceneComponent* ComponentInstance = Cast<USceneComponent>(InstanceObj.OutputComponent);
			USceneComponent* ComponentTemplate = Cast<USceneComponent>(TemplateObj.OutputComponent);
			UObject* OutputObject = InstanceObj.OutputObject;

			if (ComponentInstance)
			{
				// The translation process has either constructed new components, or it is 
				// reusing existing components, or changed an output (or all or none of the aforementioned). 
				// Carefully inspect the SCS graph to determine whether there is a corresponding 
				// (and compatible) node for this output. If not, create a new node and remove unusable node, if any.

				USCS_Node* ComponentNode = nullptr;
				{
					// Check whether the current OutputComponent being referenced by the template is still valid.
					// Even if it was removed in the editor, it doesn't have any associated destroyed / pendingkill state. 
					// Instead we're going to check for validity by finding an SCS node with a matching template component. 
					bool bValidComponentTemplate = (ComponentTemplate != nullptr);
					if (ComponentTemplate)
					{
					
						ComponentNode = FindSCSNodeForTemplateComponentInClassHierarchy(ComponentTemplate);
						bValidComponentTemplate = bValidComponentTemplate && (ComponentNode != nullptr);
					}

					if (!bValidComponentTemplate)
					{
						// Either this component was removed from the editor or it doesn't exist yet.
						// Ensure the references are cleared
						TemplateObj.OutputComponent = nullptr;
						ComponentTemplate = nullptr;
					}
				}

				// NOTE: we can't use the component instance name directly due to the Blueprint compiler performing an internal checking
				// using FComponentEditorUtils::IsValidVariableNameString(), which will return false if the name looks like an autogenerated name...
				//FString ComponentName = ComponentInstance->GetName();
				FString ComponentName = FBlueprintEditorUtils::GetClassNameWithoutSuffix(ComponentInstance->GetClass());
				FName ComponentFName  = FName(*ComponentName);			

				const auto ComponentCopyOptions = ( EditorUtilities::ECopyOptions::Type )(
						EditorUtilities::ECopyOptions::PropagateChangesToArchetypeInstances |
						EditorUtilities::ECopyOptions::CallPostEditChangeProperty |
						EditorUtilities::ECopyOptions::CallPostEditMove);
				
				if (IsValid(ComponentNode))
				{
					// Check if we have an existing and compatible SCS node containing a USceneComponent as a template component.
					bool bComponentNodeIsValid = true;
					
					ComponentTemplate = Cast<USceneComponent>(ComponentNode->ComponentTemplate);

					bComponentNodeIsValid = bComponentNodeIsValid && ComponentInstance->GetClass() == ComponentNode->ComponentClass;
					bComponentNodeIsValid = bComponentNodeIsValid && ComponentTemplate != nullptr;
					// TODO: Do we need to perform any other compatibility checks?

					if (!bComponentNodeIsValid)
					{
						// Component template is not compatible. We can't reuse it.
					
						SCSHACNode->RemoveChildNode(ComponentNode);
						SCS->RemoveNode(ComponentNode);
						ComponentNode = nullptr;
						ComponentTemplate = nullptr;
						bBlueprintStructureModified = true;
					}
				}

				if (ComponentNode)
				{
					// We found a reusable SCS node. Just copy the component instance
					// properties over to the existing template.
					check(ComponentNode->ComponentTemplate);				

					// UEngine::FCopyPropertiesForUnrelatedObjectsParams Params;
					// //Params.bReplaceObjectClassReferences = false;
					// Params.bDoDelta = false; // Perform a deep copy
					// Params.bClearReferences = false;
					// UEngine::CopyPropertiesForUnrelatedObjects(ComponentInstance, ComponentNode->ComponentTemplate, Params);
					
					FHoudiniEngineRuntimeUtils::CopyComponentProperties(ComponentInstance, ComponentNode->ComponentTemplate, ComponentCopyOptions);
					bBlueprintStructureModified = true;

					ComponentNode->ComponentTemplate->CreationMethod = EComponentCreationMethod::Native;
				}
				else
				{
					// We couldn't find a reusable SCS node. 
					// Duplicate the instance component and create a new corresponding SCS node
					ComponentNode = SCS->CreateNode(ComponentInstance->GetClass(), ComponentFName);
					
					UEditorEngine::FCopyPropertiesForUnrelatedObjectsParams Params;
					Params.bDoDelta = false; // We need a deep copy of parameters here so the CDO values get copied as well
					UEditorEngine::CopyPropertiesForUnrelatedObjects(ComponentInstance, ComponentNode->ComponentTemplate, Params);
					// FHoudiniEngineRuntimeUtils::CopyComponentProperties(ComponentInstance, ComponentNode->ComponentTemplate, ComponentCopyOptions);

					// {
					// 	UInstancedStaticMeshComponent* Component = Cast<UInstancedStaticMeshComponent>(ComponentNode->ComponentTemplate);
					// 	if (Component)
					// 	{
					// 	}
					// }
					
					// NOTE: The EComponentCreationMethod here is currently set to be the same as a component that was 
					// created manually in the editor.
					ComponentNode->ComponentTemplate->CreationMethod = EComponentCreationMethod::Native; 
					
					// Add this node to the SCS root set.

					// Attach the new node the HAC SCS node
					// NOTE: This will add the node to the SCS->AllNodes list too but it won't update
					// the nodename map. We can't forcibly update the Node/Name map either since the 
					// relevant functions have not been exported.
					SCSHACNode->AddChildNode(ComponentNode);

					// Set the output component.
					TemplateObj.OutputComponent = ComponentNode->ComponentTemplate;

					bBlueprintStructureModified = true;
				}

				// Cache the mapping between the output and the SCS node.
				check(ComponentNode);
				CachedOutputNodes.Add(Entry.Key, ComponentNode->GetVariableName());
			} // if (ComponentInstance)
			/*
			else if (InstanceObj.OutputObject)
			{
			
			}
			*/

			// Add the updated output object to the template output
			TemplateOutputObjects.Add(Entry.Key, TemplateObj);
		}

		// Cleanup stale objects for this template output.
		for (const auto& StaleId : StaleTemplateObjects)
		{
			FHoudiniOutputObject& OutputObj = TemplateOutputObjects.FindChecked(StaleId);

			// Ensure the component template is no longer referencing this output.
			TemplateOutputObjects.Remove(StaleId);

			USceneComponent* TemplateComponent = Cast<USceneComponent>(OutputObj.OutputComponent);

			if (TemplateComponent)
			{			
				USCS_Node* StaleNode = FindSCSNodeForTemplateComponentInClassHierarchy(TemplateComponent);
				if (StaleNode)
				{
				
					SCS->RemoveNode(StaleNode);
					bBlueprintStructureModified = true;
				}
				/*
				else
				{
				
				}
				*/
			}
			/*
			else
			{
			
			}
			*/
		}
	} //for (int i = 0; i < Outputs.Num(); i++)

	// Clean up stale outputs on the component template.
	for (UHoudiniOutput* StaleOutput : StaleTemplateOutputs)
	{
		if (!StaleOutput)
			continue;

		// Remove any components contained in this output from the SCS graph
		for (auto& Entry : StaleOutput->GetOutputObjects())
		{
			FHoudiniOutputObject& StaleObject = Entry.Value;
			USceneComponent* OutputComponent = Cast<USceneComponent>(StaleObject.OutputComponent);

			if (OutputComponent)
			{
			
				USCS_Node* StaleNode = FindSCSNodeForTemplateComponentInClassHierarchy(OutputComponent);
				if (StaleNode)
				{
				
					SCS->RemoveNode(StaleNode);
					bBlueprintStructureModified = true;
				}
				/*
				else
				{
				
				}
				*/
			}
		}

		TemplateOutputs.Remove(StaleOutput);
		//StaleOutput->ConditionalBeginDestroy();
	}

	SCS->ValidateSceneRootNodes();

	// Copy parameters from this component to the template component.
	// NOTE: We need to do this since the preview component will be cooking the HDA and get populated with
	// all the parameters. This data needs to be sent back to the component template.
	CachedTemplateComponent->CopyDetailsFromComponent(
		this, 
		/* CopyInputs */ true, 
		/* ClearChanged */ false, 
		/* bCanDeleteHoudiniNodes */ false,
		/* SetFlags */ RF_Public|RF_DefaultSubObject|RF_ArchetypeObject);

	// Copy the cached output nodes back to the template so that 
	// reconstructed actors can correctly update output objects
	// with newly constructed components during ApplyComponentInstanceData() calls. 
	CachedTemplateComponent->CachedOutputNodes = CachedOutputNodes;

	CachedTemplateComponent->MarkPackageDirty();

	if (bBlueprintStructureModified)
	{
		// We are about to recompile the blueprint. This will reconstruct the preview actor so we need to ensure
		// that the old actor won't release the houdini nodes.
		SetCanDeleteHoudiniNodes(false);
		
		CachedBlueprint->Modify();
		SCSEditor->SaveSCSCurrentState(SCS);
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified( CachedBlueprint.Get() );
		SCSEditor->UpdateTree(true);
	}

	CachedTemplateComponent->AssetId = AssetId;
	CachedTemplateComponent->HapiGUID = HapiGUID;
	CachedTemplateComponent->AssetCookCount = AssetCookCount;
	CachedTemplateComponent->AssetStateResult = AssetStateResult;
	CachedTemplateComponent->bLastCookSuccess = bLastCookSuccess;
}
#endif

#if WITH_EDITOR
void
UHoudiniAssetBlueprintComponent::CopyStateFromTemplateComponent(UHoudiniAssetBlueprintComponent* FromComponent)
{
	// This should never be called by component templates. 
	check(!IsTemplate());

	// Make sure all TransientDuplicate properties from the Template Component needed by this transient component
	// gets copied.

	ComponentGUID = FromComponent->ComponentGUID;

	/*
	{
		const TArray<USceneComponent*> Children = GetAttachChildren();
		for (USceneComponent* Child : Children) 
		{
			if (!Child)
				continue;		
		}
	}
	*/

	// AssetState = FromComponent->PreviewAssetState;
	
	// This state should not be shared between template / instance components.
	//bFullyLoaded = FromComponent->bFullyLoaded;

	bNoProxyMeshNextCookRequested = FromComponent->bNoProxyMeshNextCookRequested;
	
	// Reconstruct outputs and update them to point to component instances as opposed to templates.
	UObject* TemplateOuter = CachedTemplateComponent->GetOuter();
	
	USimpleConstructionScript* SCS = CachedBlueprint->SimpleConstructionScript;
	check(SCS);
	
	// NOTE: We can find the SCS node for the HoudiniAssetComponent from either the template component or the instance (editor preview) component.
	USCS_Node* SCSHACNode = FindSCSNodeForTemplateComponentInClassHierarchy(CachedTemplateComponent.Get());
	check(SCSHACNode);

	// -----------------------------------------------------
	// Copy outputs to component template
	// -----------------------------------------------------

	TArray<UHoudiniOutput*>& TemplateOutputs = CachedTemplateComponent->Outputs;
	
	TSet<UHoudiniOutput*> StaleInstanceOutputs(Outputs);
	
	Outputs.SetNum(TemplateOutputs.Num());

	for (int i = 0; i < TemplateOutputs.Num(); i++)
	{
		UHoudiniOutput* TemplateOutput = TemplateOutputs[i];
		if (!IsValid(TemplateOutput))
			continue;

		UHoudiniOutput* InstanceOutput = Outputs[i];
		if (!(InstanceOutput->GetOuter() == this))
			InstanceOutput = nullptr;

		if (InstanceOutput)
		{
			StaleInstanceOutputs.Remove(InstanceOutput);
		}

		if (InstanceOutput)
		{
			// Copy properties from the current instance component while preserving output objects
			// and instanced outputs.
			InstanceOutput->CopyPropertiesFrom(TemplateOutput, true);
		}
		else
		{
			InstanceOutput = TemplateOutput->DuplicateAndCopyProperties(this, FName(*TemplateOutput->GetName()));
			InstanceOutput->ClearFlags(RF_ArchetypeObject|RF_DefaultSubObject);
		}

		InstanceOutput->SetCanDeleteHoudiniNodes(false);
		Outputs[i] = InstanceOutput;

		TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& TemplateOutputObjects = TemplateOutput->GetOutputObjects();
		TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& InstanceOutputObjects = InstanceOutput->GetOutputObjects();
		TArray<FHoudiniOutputObjectIdentifier> StaleOutputObjects;
		InstanceOutputObjects.GetKeys(StaleOutputObjects);

		for (auto& Entry : TemplateOutputObjects)
		{
			const FHoudiniOutputObject& TemplateObj = Entry.Value;
			FHoudiniOutputObject InstanceObj = TemplateObj;

			if (!InstanceOutputObjects.Contains(Entry.Key))
				continue;

			StaleOutputObjects.Remove(Entry.Key);
			InstanceObj = InstanceOutputObjects.FindChecked(Entry.Key);

			// Is there anything else we need to do here or will the preview actor already have the outputs correctly set up?

			//if (TemplateObj.OutputComponent)
			//{
			//	// This output contains a component. Find / create a component on the component instance.
			//	USCS_Node* ComponentNode = nullptr;
			//	// Find the component node that corresponds to the output object from the template.
			//	if (TemplateObj.OutputComponent)
			//	{
		
			//		ComponentNode = FindSCSNodeForTemplateComponent(SCS, Cast<UActorComponent>(TemplateObj.OutputComponent));
			//	}

			//	if (ComponentNode)
			//	{

			//
			//		// We need to find the Editor Component that corresponds to the Output Component Template (TemplateObj.OutputComponent).
			//		USceneComponent* EditorComponent = ComponentNode->EditorComponentInstance.Get();

			//		if (!EditorComponent)
			//		{
		
			//			// Maybe this happens because this is not visible in a viewport?
			//			AActor* EditorActorInstance = SCS->GetComponentEditorActorInstance();
			//			if (EditorActorInstance)
			//			{
			//				EditorComponent = FindActorComponentByName(EditorActorInstance, ComponentNode->GetVariableName());
			//				if (!EditorComponent)
			//				{
		
			//				}
			//			}
			//		}

			//		// TODO: What should we do if we couldn't find a valid editor component for this output object? 
			//		// Ignore this output or just make the component null?

			//		if (EditorComponent)
			//		{
		
			//		}
			//		else
			//		{
		
			//		}
			//	
			//		// Assign the respective editor component instance to the instance output object.
			//		InstanceObj.OutputComponent = EditorComponent;
			//	}
			//	else
			//	{
			//		InstanceObj.OutputComponent = nullptr;
			//	}
			//}
			
			//InstanceOutputObjects.Add(Entry.Key, InstanceObj);
		} // for (auto& Entry : TemplateOutputObjects)
		
		// Cleanup stale output objects for this output.
		for (const auto& StaleId : StaleOutputObjects)
		{
			//TemplateOutput
			//check(TemplateOutputs);

			FHoudiniOutputObject& OutputObj = InstanceOutputObjects.FindChecked(StaleId);

			InstanceOutputObjects.Remove(StaleId);
			if (OutputObj.OutputComponent)
			{
				//OutputObj.OutputComponent->ConditionalBeginDestroy();
				OutputObj.OutputComponent = nullptr;
			}
		}
	} // for (int i = 0; i < TemplateOutputs.Num(); i++)

	// Cleanup any stale outputs found on the component instance.
	for (UHoudiniOutput* StaleOutput : StaleInstanceOutputs)
	{
		if (!StaleOutput)
			continue;

		if (!(StaleOutput->GetOuter() == this))
			continue;
	
		// We don't want to clear stale outputs on components instances. Only on template components.
		StaleOutput->SetCanDeleteHoudiniNodes(false);
	}

	// Copy parameters from the component template to the instance.
	CopyDetailsFromComponent(FromComponent, 
		/* bCopyInputs */ true, 
		/* bClearChanged */ false, 
		/* bCanDeleteHoudiniNodes */ false, 
		/*SetFlags*/ RF_NoFlags, 
		/*ClearFlags*/ RF_DefaultSubObject|RF_ArchetypeObject);

	/*
	{
		const TArray<USceneComponent*> Children = GetAttachChildren();
		for (USceneComponent* Child : Children) 
		{
			if (!Child)
				continue;
		
		}
	}
	*/
}
#endif

#if WITH_EDITOR
void 
UHoudiniAssetBlueprintComponent::CopyDetailsFromComponent(
	UHoudiniAssetBlueprintComponent* FromComponent,
	bool bCopyInputs,
	bool bClearChanged,
	bool bInCanDeleteHoudiniNodes,
	EObjectFlags SetFlags,
	EObjectFlags ClearFlags)
{
	check(FromComponent);

	/*
	if (!FromComponent->HoudiniAsset)
	{	
		return;
	}
	*/

	// TODO: Try to reuse objects here when we're able.
	//// Copy UHoudiniOutput state from instance to template
	//UEngine::FCopyPropertiesForUnrelatedObjectsParams Params;
	////Params.bReplaceObjectClassReferences = false;
	////Params.bClearReferences = false;
	//Params.bDoDelta = true;
	//UEngine::CopyPropertiesForUnrelatedObjects(ComponentInstance, ComponentNode->ComponentTemplate, Params);

	// Record input remapping that will need to take place when duplicating parameters.
	TMap<UHoudiniInput*, UHoudiniInput*> InputMapping;

	// -----------------------------------------------------
	// Copy inputs
	// -----------------------------------------------------

	// TODO: Add support for input components
	{
		TArray<UHoudiniInput*>& FromInputs = FromComponent->Inputs;
		TSet<UHoudiniInput*> StaleInputs(Inputs);
		
		Inputs.SetNum(FromInputs.Num());
		for (int i = 0; i < FromInputs.Num(); i++)
		{
			UHoudiniInput* FromInput = nullptr;
			UHoudiniInput* ToInput = nullptr;
			FromInput = FromInputs[i];

			check(FromInput);

			ToInput = Inputs[i];

			if (ToInput)
			{
				// Check whether the instance and template input objects are compatible.
				bool bIsValid = true;
				bIsValid = bIsValid && ToInput->Matches(*FromInput);
				bIsValid = bIsValid && ToInput->GetOuter() == this; 

				if (!bIsValid)
				{
					ToInput = nullptr;
				}
			}

			// NOTE: The CopyStateFrom() / DuplicateAndCopyState() will copy/duplicate/cleanup internal inputs to 
			// ensure that there aren't any shared instances between the ToInput/FromInput.
			if (ToInput)
			{
				// We have a compatible input that we can reuse.
				StaleInputs.Remove(ToInput);
				ToInput->CopyStateFrom(FromInput, true, bInCanDeleteHoudiniNodes);
			}
			else
			{
			
				// We don't have an existing / compatible input. Create a new one.
				ToInput = FromInput->DuplicateAndCopyState(this, bInCanDeleteHoudiniNodes);
				if (SetFlags != RF_NoFlags)
					ToInput->SetFlags(SetFlags);
				if (ClearFlags != RF_NoFlags)
					ToInput->ClearFlags( ClearFlags );
			}

			check(ToInput);

			Inputs[i] = ToInput;
			InputMapping.Add(FromInput, ToInput);

			if (bClearChanged)
			{
				// Clear the changed flags on the FromInput so that it doesn't trigger 
				// another update. The ToInput will now be carrying to changed/update flags.
				FromInput->MarkChanged(false);
				FromInput->MarkAllInputObjectsChanged(false);
			}
		}

		// Cleanup any stale inputs from this component.
		// NOTE: We would typically only have stale inputs when copying state from
		// the component instance to the component template. Garbage collection
		// eventually picks up the input objects and removes the content
		// but until such time we are stuck with those nodes as inputs in the Houdini session
		// so we get rid of those nodes immediately here to avoid some user confusion.
		for (UHoudiniInput* StaleInput : StaleInputs)
		{
			if (!IsValid(StaleInput))
				continue;
		
			check(StaleInput->GetOuter() == this);			
		
			if (StaleInput->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad))
				continue;
		
			StaleInput->ConditionalBeginDestroy();
		}
	}


	// -----------------------------------------------------
	// Copy parameters (and optionally remap inputs).
	// -----------------------------------------------------
	TMap<UHoudiniParameter*, UHoudiniParameter*> ParameterMapping;
	
	TArray<UHoudiniParameter*>& FromParameters = FromComponent->Parameters;
	Parameters.SetNum(FromParameters.Num());

	for (int i = 0; i < FromParameters.Num(); i++)
	{
		UHoudiniParameter* FromParameter = nullptr;
		UHoudiniParameter* ToParameter = nullptr;

		FromParameter = FromParameters[i];

		check(FromParameter);

		if (Parameters.IsValidIndex(i))
		{
			ToParameter = Parameters[i];
		}

		if (ToParameter)
		{
			bool bIsValid = true;
			// Check whether To/From parameters are compatible
			bIsValid = bIsValid && ToParameter->Matches(*FromParameter);
			bIsValid = bIsValid && ToParameter->GetOuter() == this;
			
			if (!bIsValid)
				ToParameter = nullptr;
		}

		if (ToParameter)
		{
			// Parameter already exists. Simply sync the state.
			ToParameter->CopyStateFrom(FromParameter, true);
		}
		else
		{
			// TODO: Check whether parameters are the same to avoid recreating them.
			ToParameter = FromParameter->DuplicateAndCopyState(this);
			Parameters[i] = ToParameter;
			if (SetFlags != RF_NoFlags)
				ToParameter->SetFlags(SetFlags);
			if (ClearFlags != RF_NoFlags)
				ToParameter->ClearFlags( ClearFlags );
		
		}
		
		check(ToParameter);
		ParameterMapping.Add(FromParameter, ToParameter);
		
		if (bClearChanged)
		{
			// We clear the Changed flag on the FromParameter (most likely on the component template)
			// since the template parameter state has now been transfered to the preview component and
			// will resume processing from there.
			FromParameter->MarkChanged(false);
		}
	}

	// Apply remappings on the new parameters
	for (UHoudiniParameter* ToParameter : Parameters)
	{
		ToParameter->RemapParameters(ParameterMapping);
		//if (bCopyInputs)
		{
			ToParameter->RemapInputs(InputMapping);
		}
	}

	bEnableCooking = FromComponent->bEnableCooking;
	bRecookRequested = FromComponent->bRecookRequested;
	bRebuildRequested = FromComponent->bRebuildRequested;
}
#endif

#if WITH_EDITOR
bool
UHoudiniAssetBlueprintComponent::HasOpenEditor() const
{
	if (IsTemplate())
	{ 
		IAssetEditorInstance* EditorInstance = FindEditorInstance();

		return EditorInstance != nullptr;
	}

	return false;
}
#endif

#if WITH_EDITOR
IAssetEditorInstance*
UHoudiniAssetBlueprintComponent::FindEditorInstance() const
{
	UClass* BPGC = Cast<UClass>(GetOuter());
	if (!IsValid(BPGC))
		return nullptr;
	UBlueprint* Blueprint = Cast<UBlueprint>(BPGC->ClassGeneratedBy);
	if (!IsValid(Blueprint))
		return nullptr;
	//if (!CachedAssetEditorSubsystem.IsValid())
	//	return nullptr;
	//IAssetEditorInstance* EditorInstance = CachedAssetEditorSubsystem->FindEditorForAsset(Blueprint, false);

	IAssetEditorInstance* EditorInstance = FAssetEditorManager::Get().FindEditorForAsset(Blueprint, false);

	return EditorInstance;
}
#endif

#if WITH_EDITOR
AActor* 
UHoudiniAssetBlueprintComponent::GetPreviewActor() const
{
	FBlueprintEditor* BlueprintEditor = FHoudiniEngineRuntimeUtils::GetBlueprintEditor(this);
	if (BlueprintEditor)
	{
		return BlueprintEditor->GetPreviewActor();
	}
	return nullptr;
}
#endif

UHoudiniAssetComponent*
UHoudiniAssetBlueprintComponent::GetCachedTemplate() const
{
	return CachedTemplateComponent.Get();
}

//bool 
//UHoudiniAssetBlueprintComponent::CanUpdateParameters() const
//{
//	return IsTemplate();
//}
//
//bool 
//UHoudiniAssetBlueprintComponent::CanUpdateInputs() const
//{
//	return !IsTemplate();
//}
//
//bool
//UHoudiniAssetBlueprintComponent::CanUpdateOutputs() const
//{
//	return !IsTemplate();
//}
//
//bool 
//UHoudiniAssetBlueprintComponent::CanProcessOutputs() const
//{
//	return !IsTemplate();
//}

//bool 
//UHoudiniAssetBlueprintComponent::CanInstantiateAsset() const
//{
//	// If this is a preview component, it should not trigger an asset instantiation. It should wait 
//	// for the BPGC template component to finish the cook, get the synced data and then translate.
//
//	if (IsPreview())
//		return false;
//
//	return true;
//}
//
//// Check whether the HAC can translate Houdini outputs at all
//bool
//UHoudiniAssetBlueprintComponent::CanTranslateFromHoudini() const
//{
//	// Template components can't translate Houdini output since they typically do not exist in a world.
//	if (IsTemplate())
//		return false;
//	// Preview components and normally instanced actors can translate Houdini outputs.
//	return true;
//}
//
//// Check whether the HAC can translate a specific output type.
//bool
//UHoudiniAssetBlueprintComponent::CanTranslateFromHoudini(EHoudiniOutputType OutputType) const
//{
//	// Blueprint components have limited translation support, for now.
//	if (OutputType == EHoudiniOutputType::Mesh)
//		return true;
//
//	return false;
//}
//
bool 
UHoudiniAssetBlueprintComponent::CanDeleteHoudiniNodes() const
{
	return bCanDeleteHoudiniNodes;
}

void
UHoudiniAssetBlueprintComponent::SetCanDeleteHoudiniNodes(bool bInCanDeleteNodes)
{
	bCanDeleteHoudiniNodes = bInCanDeleteNodes;
	
	for (UHoudiniInput* Input : Inputs)
	{
		Input->SetCanDeleteHoudiniNodes(bInCanDeleteNodes);
	}

	for (UHoudiniOutput* Output : Outputs)
	{
		Output->SetCanDeleteHoudiniNodes(bInCanDeleteNodes);
	}
}

bool 
UHoudiniAssetBlueprintComponent::IsValidComponent() const
{
	if (!Super::IsValidComponent())
		return false;
	
	if (IsTemplate())
	{
		UObject* Outer = this->GetOuter();
		if (!IsValid(Outer))
			return false;
		UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(Outer);
		if (!BPGC)
			return false;
		// Ensure this component is still in the SCS
		USimpleConstructionScript* SCS = BPGC->SimpleConstructionScript;
		if (!SCS)
			return false;
		USCS_Node* SCSNode = FindSCSNodeForTemplateComponentInClassHierarchy(this);
		if (!SCSNode)
			return false;
		/*UClass* OwnerClass = Outer->GetClass();
		if (!IsValid(OwnerClass))
			return false;*/
		/*UBlueprint* Blueprint = Cast<UBlueprint>(Outhe);
		if (Blueprint)
		{
			
		}*/
	}
	
#if WITH_EDITOR
	if (!IsTemplate())
	{ 
		if (!GetOwner())
		{
			// If it's not a template, it needs an owner!
			return false;
		}

		USimpleConstructionScript* SCS = GetSCS();
		if (SCS)
		{
			// We're dealing with a Blueprint related component.
			AActor* PreviewActor = GetPreviewActor();
			AActor* OwningActor  = GetOwner();
			if (!OwningActor)
				return false;
			if (OwningActor != PreviewActor)
			{
				return false;
			}
		}

	}

	if (IsPreview() && false)
	{
		USimpleConstructionScript* SCS = GetSCS();
		if (!SCS)
			return false; // Preview components should have an SCS.

		// We want to specifically detect whether an editor component is still being previewed. We do this 
		// by checking whether the owning actor is still the active editor actor in the SCS.
		AActor* PreviewActor = GetPreviewActor();
		if (!PreviewActor)
		{
			return false;
		}

		// Ensure this component still belongs the to the current preview actor.
		if (PreviewActor != GetOwner())
		{
			return false;
		}

		/*
		AActor* EditorActor = SCS->GetComponentEditorActorInstance();
		if (GetOwner() != EditorActor)
		{
			return false;
		}
		*/
	}
#endif

	return true;
}

bool 
UHoudiniAssetBlueprintComponent::IsInputTypeSupported(EHoudiniInputType InType) const
{
	switch (InType)
	{
		case EHoudiniInputType::Geometry:
			return true;
			break;
		default:
			break;
	}
	return false;
}

bool 
UHoudiniAssetBlueprintComponent::IsOutputTypeSupported(EHoudiniOutputType InType) const
{
	switch (InType)
	{
		case EHoudiniOutputType::Mesh:
		case EHoudiniOutputType::Instancer:
			return true;
			break;
		default:
			break;
	}
	return false;
}

bool 
UHoudiniAssetBlueprintComponent::IsProxyStaticMeshEnabled() const
{
	// TODO: Investigate adding support for proxy meshes in BP
	// Disabled for now
	return false;
}

//void 
//UHoudiniAssetBlueprintComponent::BroadcastPreAssetCook()
//{
//	// ------------------------------------------------
//	// NOTE: This code will run on TEMPLATE components
//	// ------------------------------------------------
//
//	// The HoudiniAsset is about to be recooked. This flag will indicate to
//	// the transient components that output processing needs to be baked 
//	// back to the BP definition.
//	bOutputsRequireUpdate = true;
//
//	Super::BroadcastPreAssetCook();
//}

void 
UHoudiniAssetBlueprintComponent::OnPrePreCook()
{
	check(IsPreview());
	
	Super::OnPrePreCook();

	// We need to allow deleting houdini nodes 
	SetCanDeleteHoudiniNodes(true);
}

void 
UHoudiniAssetBlueprintComponent::OnPostPreCook()
{
	check(IsPreview());
	
	Super::OnPostPreCook();

	// Ensure the houdini nodes can be deleted during the translation process.
	SetCanDeleteHoudiniNodes(false);
}

void 
UHoudiniAssetBlueprintComponent::OnPreOutputProcessing()
{
	check(IsPreview());
	
	Super::OnPreOutputProcessing();

	// Ensure the houdini nodes can be deleted during the translation process.
	SetCanDeleteHoudiniNodes(true);
}

void 
UHoudiniAssetBlueprintComponent::OnPostOutputProcessing()
{
	/*
	{
		const TArray<USceneComponent*> Children = GetAttachChildren();
		for (USceneComponent* Child : Children) 
		{
			if (!Child)
				continue;
		
		}
	}
	*/

	Super::OnPostOutputProcessing();

	// ------------------------------------------------
	// NOTE: 
	// In Blueprint editor mode, this code will run on PREVIEW components
	// In Map editor mode, this code will run on component instances.
	// ------------------------------------------------
	if (IsPreview())
	{
		// Ensure all the inputs / outputs belonging to the 
		// preview actor won't be deleted by PreviewActor destruction.
		SetCanDeleteHoudiniNodes(false);

#if WITH_EDITOR
		CopyStateToTemplateComponent();
#endif

		{
			const TArray<USceneComponent*> Children = GetAttachChildren();
			for (USceneComponent* Child : Children) 
			{
				if (!Child)
					continue;			
			}
			// DEBUG: We're trying to detect whether the CopyStateToTemplateComponent() is losing objects. 
		}
	}
}

void 
UHoudiniAssetBlueprintComponent::NotifyHoudiniRegisterCompleted()
{
	if (IsTemplate())
	{
		// TODO: Do we need to set any status flags or clear stuff to ensure 
		// the BP HAC will cook properly when the BP is opened again...

		// If the template is being registered, we need to invalidate the AssetId here since it likely 
		// contains a stale asset id from its last cook.
		AssetId = -1;
		// Template component's have very limited update requirements / capabilities.
		// Mostly just cache parameters and cook state.
		AssetState = EHoudiniAssetState::ProcessTemplate; 
	}

	Super::NotifyHoudiniRegisterCompleted();
}

void 
UHoudiniAssetBlueprintComponent::NotifyHoudiniPreUnregister()
{
	if (IsTemplate())
	{
		// Templates can delete Houdini nodes when they get deregistered.
		SetCanDeleteHoudiniNodes(true);
	}
	Super::NotifyHoudiniPreUnregister();
}

void 
UHoudiniAssetBlueprintComponent::NotifyHoudiniPostUnregister()
{
	InvalidateData();
	
	Super::NotifyHoudiniPostUnregister();

	if (IsTemplate())
	{		
		SetCanDeleteHoudiniNodes(false);
	}
}

#if WITH_EDITOR
void 
UHoudiniAssetBlueprintComponent::OnComponentCreated()
{
	Super::OnComponentCreated();

	CachePreviewState();

	if (IsPreview())
	{
		// Don't set an initial AssetState here. Preview components should only cook when template's 
		// Houdini Asset or HDA parameters have changed.

		// Clear these to ensure that we're not sharing references with the component template (otherwise
		// the shared objects will get deleted when the component instance gets destroyed).
		// These objects will be properly duplicated when copying state from the component template.
		Inputs.Empty();
		Parameters.Empty();
	}

	// Wait until InitializeComponent() for blueprint construction to complete before we start caching blueprint data.

	return;

	//CacheBlueprintData();

	//if (CachedTemplateComponent.IsValid())
	//{
	//	AActor* ComponentOwner = this->GetOwner();
	//	if (!IsValid(ComponentOwner))
	//		return;
	//	UClass* OwnerClass = ComponentOwner->GetClass();
	//	if (!IsValid(OwnerClass))
	//		return;
	//	UBlueprint* Blueprint = Cast<UBlueprint>(OwnerClass->ClassGeneratedBy);
	//	if (!IsValid(Blueprint))
	//		return;
	//	check(Blueprint);
	
	//
	//	USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
	//	check(SCS);

	//	check(CachedTemplateComponent.Get());
	//
	//	// NOTE: We can find the SCS node for the HoudiniAssetComponent from either the template component or the instance (editor preview) component.
	//	USCS_Node* SCSHACNode = FindSCSNodeForTemplateComponent(SCS, CachedTemplateComponent.Get());
	//	check(SCSHACNode);

	//	// Copy parameters from Template component node
	//	CopyParametersFromComponent( CachedTemplateComponent.Get() );

	//	CachedAssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
	//	if ( CachedAssetEditorSubsystem->FindEditorForAsset(CachedBlueprint.Get(), false) != nullptr)
	//	{
	//		// Ensure that the Template component has been registered since it needs to be processed by the HE manager.
	//		// NOTE: Deregistration takes place in the HoudiniEngineManager::Tick() when the template doesn't have an
	//		// open editor.


	//		FHoudiniEngineRuntime::Get().RegisterHoudiniComponent(CachedTemplateComponent.Get(), true);
	//	}

	//}

	//return;

	//// If this is a preview component and the Blueprint is open in the editor, 
	//// be sure to register the corresponding component from the BPGC with the HoudiniEngineRuntime.
	//if (!IsPreview())
	//	return;

	//CacheBlueprintData();
	//
	//if (!(CachedBlueprint.IsValid() && CachedTemplateComponent.IsValid()))
	//	return;

	//// Subscribe to template component notifications

	//// Preview components need to be notified when the Template component finished cooking
	////CachedTemplateComponent->OnCookCompleted.AddUObject(this, &UHoudiniAssetBlueprintComponent::OnPostCookHandler);

	//CachedAssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
	//if ( CachedAssetEditorSubsystem->FindEditorForAsset(CachedBlueprint.Get(), false) != nullptr)
	//{
	//	// Ensure that the Template component has been registered since it needs to be processed for parameter updates by the HE manager.


	//	FHoudiniEngineRuntime::Get().RegisterHoudiniComponent(CachedTemplateComponent.Get(), true);
	//}
}
#endif


void 
UHoudiniAssetBlueprintComponent::OnRegister()
{
	Super::OnRegister();

	// We run our Blueprint caching functions here since this the last hook that we have before 
	// entering HoudiniEngineTick();

	CacheBlueprintData();
	CachePreviewState();

	if (IsPreview())
	{
		check(CachedTemplateComponent.Get());
		// Ensure that the component template has been registered since it needs to be processed for parameter updates by the HE manager.
		if (!FHoudiniEngineRuntime::Get().IsComponentRegistered(CachedTemplateComponent.Get()))
		{
			// The template component has not been registered yet, which means that we're probably busy opening a Blueprint editor and this
			// preview component will need to be updated.
		
			FHoudiniEngineRuntime::Get().RegisterHoudiniComponent(CachedTemplateComponent.Get(), true);
			CachedTemplateComponent->SetCanDeleteHoudiniNodes(false);
			// Since we're likely opening a fresh blueprint editor, we'll need to instantiate the HDA.
			bHasRegisteredComponentTemplate = true;
		}
	}

	if (IsTemplate())
	{
		// We're initializing the asset id for HAC template here since it doesn't get unloaded
		// from memory, for example, between Blueprint Editor open/close so we need to make sure
		// that the AssetId has indeed been reset between registrations.
		AssetId = -1;
	}

	//TickInitialization();
}

void 
UHoudiniAssetBlueprintComponent::BeginDestroy()
{
	/*
	for (UHoudiniInput* Input : Inputs)
	{
		if (!Input)
			continue;
	}

	for (UHoudiniOutput* Output : Outputs)
	{
		if (!Output)
			continue;
	}
	*/

	Super::BeginDestroy();
}

void 
UHoudiniAssetBlueprintComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
	/*
	for (UHoudiniInput* Input : Inputs)
	{
		if (!Input)
			continue;
	}

	for (UHoudiniOutput* Output : Outputs)
	{
		if (!Output)
			continue;
	}
	*/

	Super::OnComponentDestroyed(bDestroyingHierarchy);
}

TStructOnScope<FActorComponentInstanceData> 
UHoudiniAssetBlueprintComponent::GetComponentInstanceData() const
{
	TStructOnScope<FActorComponentInstanceData> ComponentInstanceData = MakeStructOnScope<FActorComponentInstanceData, FHoudiniAssetBlueprintInstanceData>(this);
	FHoudiniAssetBlueprintInstanceData* InstanceData = ComponentInstanceData.Cast<FHoudiniAssetBlueprintInstanceData>();

	InstanceData->AssetId = AssetId;
	InstanceData->AssetState = AssetState;
	InstanceData->SubAssetIndex = SubAssetIndex;
	InstanceData->ComponentGUID = ComponentGUID;
	InstanceData->HapiGUID = HapiGUID;
	InstanceData->HoudiniAsset = HoudiniAsset;
	InstanceData->SourceName = GetPathName();
	InstanceData->AssetCookCount = AssetCookCount;
	InstanceData->bHasBeenLoaded = bHasBeenLoaded;
	InstanceData->bHasBeenDuplicated = bHasBeenDuplicated;
	InstanceData->bPendingDelete = bPendingDelete;
	InstanceData->bRecookRequested = bRecookRequested;
	InstanceData->bEnableCooking = bEnableCooking;
	InstanceData->bForceNeedUpdate = bForceNeedUpdate;
	InstanceData->bLastCookSuccess = bLastCookSuccess;
	InstanceData->bRegisteredComponentTemplate = bHasRegisteredComponentTemplate;

	InstanceData->Inputs.Empty();

	for (UHoudiniInput* Input : Inputs)
	{
		if (!Input)
			continue;
		UHoudiniInput* TransientInput = Input->DuplicateAndCopyState(GetTransientPackage(), false);
		InstanceData->Inputs.Add(TransientInput);
	}

	// Cache the current outputs
	InstanceData->Outputs.Empty();
	int OutputIndex = 0;
	for(UHoudiniOutput* Output : Outputs)
	{
		if (!Output)
			continue;

		TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject> OutputObjects = Output->GetOutputObjects();
		for (auto& Entry : OutputObjects)
		{
			FHoudiniAssetBlueprintOutput OutputObjectData;
			OutputObjectData.OutputIndex = OutputIndex;
			OutputObjectData.OutputObject = Entry.Value;
			InstanceData->Outputs.Add(Entry.Key, OutputObjectData);
		}

		++OutputIndex;
	}

	return ComponentInstanceData;
}

void 
UHoudiniAssetBlueprintComponent::ApplyComponentInstanceData(FHoudiniAssetBlueprintInstanceData* InstanceData, const bool bPostUCS)
{
	check(InstanceData);

	if (!bPostUCS)
	{
		// Initialize the component before the User Construction Script runs		
		USimpleConstructionScript* SCS = GetSCS();
		check(SCS);

		TArray<UHoudiniInput*> StaleInputs(Inputs);

		// We need to update references inputs / ouputs to point to new .
		const int32 NumInputs = InstanceData->Inputs.Num();
		Inputs.SetNum(NumInputs);
		for (int i = 0; i < NumInputs; ++i)
		{
			UHoudiniInput* FromInput = InstanceData->Inputs[i];
			UHoudiniInput* ToInput = Inputs[i];

			if (ToInput)
			{
				bool bIsValid = true;
				bIsValid = bIsValid && ToInput->Matches(*FromInput);
				bIsValid = bIsValid && ToInput->GetOuter() == this;
				if (!bIsValid)
				{
					ToInput = nullptr;
				}
			}

			if (ToInput)
			{
				// Reuse input
				StaleInputs.Remove(ToInput);
				ToInput->CopyStateFrom(FromInput, true, false);
			}
			else
			{
				// Create new input
				ToInput = FromInput->DuplicateAndCopyState(this, false);
			}

			Inputs[i] = ToInput;
		}

		// We need to update FHoudiniOutputObject SceneComponent references to
		// the newly created components. Since we cached a map of Output Object IDs to
		// SCSNodes (during CopyStateToTemplateComponent), we can the SCSNode that corresponds to this output objects and find
		// the SceneComponent that matches the SCSNode's variable name.
		// It is important to note that it is safe to do it this way since we're in the pre-UCS
		// phase so that current components should match the SCS graph exactly (no user construction script
		// interference here yet).

		for (auto& Entry : InstanceData->Outputs)
		{
			FHoudiniOutputObjectIdentifier& ObjectId = Entry.Key;
			FHoudiniAssetBlueprintOutput& OutputData = Entry.Value;

			// NOTE: Output objects are going to be empty here since they dissapear during actor reconstruction.
			// We'll need to repopulate from the instance data.

			check(Outputs.IsValidIndex(OutputData.OutputIndex));
			UHoudiniOutput* Output = Outputs[OutputData.OutputIndex];
			check(Output);
			TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& OutputObjects = Output->GetOutputObjects();
			FHoudiniOutputObject NewObject = OutputData.OutputObject;

			if (OutputData.OutputObject.OutputComponent)
			{
				// Update the output component reference.
				check(CachedOutputNodes.Contains(ObjectId))
				const FName VariableName = CachedOutputNodes.FindChecked(ObjectId);

				// Find the component that corresponds to the SCS node.
				USceneComponent* SceneComponent = FindActorComponentByName(GetOwner(), VariableName);
				NewObject.OutputComponent = SceneComponent;
			}

			OutputObjects.Add(ObjectId, NewObject);
		}

		if (CachedTemplateComponent.IsValid())
		{
#if WITH_EDITOR
			CopyStateFromTemplateComponent( CachedTemplateComponent.Get() );
#endif
		}
	
		AssetId = InstanceData->AssetId;
		SubAssetIndex = InstanceData->SubAssetIndex;
		ComponentGUID = InstanceData->ComponentGUID;
		HapiGUID = InstanceData->HapiGUID;
	
		// Apply the previous HoudiniAsset to the component
		// so that we can compare it against the template during CopyStateFromTemplate() calls to see whether it changed.
		HoudiniAsset = InstanceData->HoudiniAsset;

		AssetCookCount = InstanceData->AssetCookCount;
		bHasBeenLoaded = InstanceData->bHasBeenLoaded;
		bHasBeenDuplicated = InstanceData->bHasBeenDuplicated;
		bPendingDelete = InstanceData->bPendingDelete;
		bRecookRequested = InstanceData->bRecookRequested;
		bEnableCooking = InstanceData->bEnableCooking;
		bForceNeedUpdate = InstanceData->bForceNeedUpdate;
		bLastCookSuccess = InstanceData->bLastCookSuccess;
		bHasRegisteredComponentTemplate = InstanceData->bRegisteredComponentTemplate;

		AssetState = InstanceData->AssetState;
		
		SetCanDeleteHoudiniNodes(false);
	}   // if (!bPostUCS)
	/*
	else
	{
		// PostUCS

	}
	*/
}


void 
UHoudiniAssetBlueprintComponent::HoudiniEngineTick()
{
	if (!IsFullyLoaded())
	{
		USimpleConstructionScript* SCS = GetSCS();
		if (SCS == nullptr)
		{		
			OnFullyLoaded();
		}
		else if (IsPreview())
		{
			AActor* OwningActor = GetOwner();
			check(OwningActor);

			// If this is a *preview component*, it is important to wait for the template component to be fully loaded
			// since it needs to be initialized so that the component instance can copy initial values from the template.
			check(CachedTemplateComponent.Get());

			if (CachedTemplateComponent->IsFullyLoaded())
			{
#if WITH_EDITOR
				if(SCS->IsConstructingEditorComponents())
				{
					// We're stuck in an editor blueprint construction / preview actor update. Wait some more.				
				}
				else 
				{
					OnFullyLoaded();
				}
#else
				OnFullyLoaded();
#endif
			}
		}
		else
		{
			// Anything else can go onto being fully loaded at this point.
			OnFullyLoaded();
		}
	}
}

void 
UHoudiniAssetBlueprintComponent::OnFullyLoaded()
{
	Super::OnFullyLoaded();

	// Check whether this component is inside a Blueprint editor. If this object lives outside the blueprint editor (in , then we need to ensure that this 
	// component won't be influencing the Blueprint asset.
	
	if (!IsTemplate())
	{
#if WITH_EDITOR
		AActor* PreviewActor = GetPreviewActor();
#else
		AActor* PreviewActor = nullptr;
#endif
		AActor* OwningActor = GetOwner();
	
		if (!PreviewActor)
		{
			bIsInBlueprintEditor = false;
			AssetState = EHoudiniAssetState::None;
			return;
		}

		if (OwningActor && PreviewActor != OwningActor)  
		{
			bIsInBlueprintEditor = false;
			AssetState = EHoudiniAssetState::None;
			return;
		}
	}

	bIsInBlueprintEditor = true;

	CachePreviewState();
	CacheBlueprintData();

	/*
	for (UHoudiniOutput* Output : Outputs)
	{
		if (!Output)
			continue;
	}
	*/

	if (IsTemplate())
	{	
		AssetId = -1;
		AssetState = EHoudiniAssetState::ProcessTemplate;
	}

	if (IsPreview()) 
	{
		check(CachedTemplateComponent.Get());
	
		// If this is a preview actor, sync initial settings from the component template
#if WITH_EDITOR
		CopyStateFromTemplateComponent(CachedTemplateComponent.Get());
#endif

		CachedTemplateComponent->OnParametersChangedEvent.AddUObject(this, &UHoudiniAssetBlueprintComponent::OnTemplateParametersChangedHandler);
		if (bHoudiniAssetChanged)
		{
		
			// The HoudiniAsset has changed, so we need to force the PreviewInstance to re-instantiate
			AssetState = EHoudiniAssetState::NeedInstantiation;
			bForceNeedUpdate = true;
			bHoudiniAssetChanged = false;
			// TODO: Make this better?
			CachedTemplateComponent->bHoudiniAssetChanged = false;
		}

		if (bHasRegisteredComponentTemplate)
		{
			// We have a newly registered component template. One of two things happened to cause this:
			// 1. A new HoudiniAssetBlueprintComponent was created and registered.
			// 2. The Blueprint Editor was closed / opened.
			// The problem that arises in the #2 is that the template component was never fully unloaded
			// from memory (it was deregistered but not destroyed). After deregistration we had the
			// opportunity to invalidate asset/node ids but now that it has reregistered (without going
			// through the normal initialization process) we will have to force a call to MarkAsNeedInstantiation
			// during the next OnTemplateParametersChangedHandler() invocation.
			bHasBeenLoaded = true;
		}
	}
}

void 
UHoudiniAssetBlueprintComponent::OnTemplateParametersChanged()
{
	OnParametersChangedEvent.Broadcast(this);
}

void 
UHoudiniAssetBlueprintComponent::OnHoudiniAssetChanged()
{
	if (IsTemplate())
	{
		// Invalidate data associated with this component since we're about to change and reinstantiate the Houdini Asset.
		SetCanDeleteHoudiniNodes(true);
		InvalidateData();
		SetCanDeleteHoudiniNodes(false);
		Parameters.Empty();
		Inputs.Empty();
	}

	Super::OnHoudiniAssetChanged();

	// Set on template components, then copied to preview components, and 
	// then used (and reset) during OnFullyLoaded.
	bHoudiniAssetChanged = true;
}

void 
UHoudiniAssetBlueprintComponent::RegisterHoudiniComponent(UHoudiniAssetComponent *InComponent)
{
	// We only want to register this component if it is the preview actor for the Blueprint editor. 
#if WITH_EDITOR
	AActor* PreviewActor = GetPreviewActor();
#else
	AActor* PreviewActor = nullptr;
#endif
	AActor* OwningActor = GetOwner();
	if (!OwningActor)
		return;

	if (PreviewActor != OwningActor)
		return;

	Super::RegisterHoudiniComponent(InComponent);
}


//bool UHoudiniAssetBlueprintComponent::TickInitialization()
//{
//	return true;
//
//	if (IsFullyLoaded())
//		return true;
//
//	bool bHasFinishedLoading = Super::TickInitialization();
//
//	if (!bHasFinishedLoading)
//		return false;
//
//	if (!IsTemplate())
//	{
//
//		if (CachedTemplateComponent.Get())
//		{
//			// Now that that SCS has finished constructing editor components, we can continue.
//			// Copy the current state from the template component, in case there is something that can be processed.
//			CopyStateFromTemplateComponent(CachedTemplateComponent.Get());
//		}
//
//		AssetStateResult = EHoudiniAssetStateResult::None;
//		AssetState = EHoudiniAssetState::None;
//	
//		bForceNeedUpdate = true;
//		AssetState = EHoudiniAssetState::PostCook;
//	}
//
//	return true;
//}

template<typename ParamT, typename ValueT>
inline void 
UHoudiniAssetBlueprintComponent::SetTypedValueAt(const FString& Name, ValueT& Value, int Index)
{
	ParamT* Parameter = Cast<ParamT>(FindParameterByName(Name));
	if (!Parameter)
		return;

	Parameter->SetValueAt(Value, Index);
}

bool
UHoudiniAssetBlueprintComponent::HasParameter(FString Name)
{
	return FindParameterByName(Name) != nullptr;
}

void 
UHoudiniAssetBlueprintComponent::SetFloatParameter(FString Name, float Value, int Index)
{
	SetTypedValueAt<UHoudiniParameterFloat>(Name, Value, Index);
}

void 
UHoudiniAssetBlueprintComponent::SetToggleValueAt(FString Name, bool Value, int Index)
{
	UHoudiniParameterToggle* Parameter = Cast<UHoudiniParameterToggle>(FindParameterByName(Name));
	if (!Parameter)
		return;

	Parameter->SetValueAt(Value, Index);
}

//void UHoudiniAssetBlueprintComponent::OnPostCookHandler(UHoudiniAssetComponent* InComponent)
//{
//
//	// Before this component handles any translation, we need to make sure that it still belongs to the editor actor.
//	// When a blueprint gets recompiled, the editor actor gets replaced with a new one but the old actor has not yet 
//	// been ftroyed / garbage collected so its components still receive cook events from the template.
//
//	UHoudiniAssetBlueprintComponent* ComponentTemplate = Cast<UHoudiniAssetBlueprintComponent>(InComponent);
//	if (!IsValid(ComponentTemplate))
//		return;
//
//	CopyStateFromTemplateComponent(ComponentTemplate);
//	bForceNeedUpdate = true;
//}

void 
UHoudiniAssetBlueprintComponent::OnTemplateParametersChangedHandler(UHoudiniAssetComponent* InComponentTemplate)
{
	if (!IsValidComponent())
	{
		return;
	}

	UHoudiniAssetBlueprintComponent* ComponentTemplate = Cast<UHoudiniAssetBlueprintComponent>(InComponentTemplate);
	if (!ComponentTemplate)
		return;

	// The component instance needs to copy values from the template.
#if WITH_EDITOR
	CopyDetailsFromComponent(ComponentTemplate, /* CopyInputs */ true, /* ClearChanged */ true, false, RF_NoFlags, RF_ClassDefaultObject|RF_ArchetypeObject);
#endif

	SetCanDeleteHoudiniNodes(false);

	if (bHasRegisteredComponentTemplate)
	{
		// NOTE: It is very important to call this *after* CopyDetailsFromComponent(), since CopyDetailsFromComponent
		// will clobber the inputs and parameter states on this component.

		// If we already have a valid asset id, keep it.
		if (AssetId >= 0)
		{
			MarkAsNeedCook();
		}
		else
		{
			MarkAsNeedInstantiation();
		}
		
		bHasRegisteredComponentTemplate = false;
		bFullyLoaded = true; // MarkAsNeedInstantiation sets this to false. Force to true.
		// While MarkAsNeedInstantiation() sets ParametersChanged to true, it does not
		// set the 'NeedToTriggerUpdate' flag (both of which needs to be true in order
		// to trigger an HDA update) so we are going to force NeedUpdate() to return true
		// in order to get an initial cook.
		bForceNeedUpdate = true;
	}
}

void 
UHoudiniAssetBlueprintComponent::InvalidateData()
{
	if (IsTemplate())
	{
		// Ensure transient properties are invalidated/released for parameters, inputs and outputs as if the
		// the object was undergoing destruction since the template component will likely be reregistered
		// without being destroyed.
		for(UHoudiniParameter* Param : Parameters)
		{
			Param->InvalidateData();
		}
		
		for(UHoudiniInput* Input : Inputs)
		{
			Input->InvalidateData();
		}

		FHoudiniEngineRuntime::Get().MarkNodeIdAsPendingDelete(AssetId, true);
		AssetId = -1;
	}
}

//void UHoudiniAssetBlueprintComponent::OnTemplateHoudiniAssetChangedHandler(UHoudiniAssetComponent* InComponentTemplate)
//{
//
//	UHoudiniAssetBlueprintComponent* ComponentTemplate = Cast<UHoudiniAssetBlueprintComponent>(InComponentTemplate);
//	if (!ComponentTemplate)
//		return;
//	check(IsPreview());
//
//	// The Houdini Asset was changed on the template. We need to recook.
//	AssetState = EHoudiniAssetState::NeedInstantiation;
//
//}

USceneComponent* 
UHoudiniAssetBlueprintComponent::FindOwnerComponentByName(FName ComponentName) const
{
	AActor* Owner = GetOwner();
	if (!Owner)
		return nullptr;

	return FindActorComponentByName(Owner, ComponentName);
}

USceneComponent*
UHoudiniAssetBlueprintComponent::FindActorComponentByName(AActor* InActor, FName ComponentName) const
{
	const TSet<UActorComponent*>& Components = InActor->GetComponents();

	for (UActorComponent* Component : Components)
	{
		USceneComponent* SceneComponent = Cast<USceneComponent>(Component);
		if (!IsValid(SceneComponent))
			continue;
		if (FName(*SceneComponent->GetName()) == ComponentName)
			return SceneComponent;
	}

	return nullptr;
}

USCS_Node* 
UHoudiniAssetBlueprintComponent::FindSCSNodeForTemplateComponent(USimpleConstructionScript* SCS, const UActorComponent* InComponent) const
{
	const TArray<USCS_Node*>& AllNodes = SCS->GetAllNodes();

	for (USCS_Node* Node : AllNodes)
	{
		if (!Node)
			continue;

		if (Node->ComponentTemplate == InComponent)
			return Node;
	}

	return nullptr;
}

USCS_Node* 
UHoudiniAssetBlueprintComponent::FindSCSNodeForTemplateComponentInClassHierarchy(
	const UActorComponent* InComponent) const
{
	UObject* Outer = this->GetOuter();
	if (!IsValid(Outer))
		return nullptr;
	UBlueprintGeneratedClass* MainBPGC;
	if (IsTemplate())
	{
		MainBPGC = Cast<UBlueprintGeneratedClass>(Outer);
	}
	else
	{
		AActor* OwningActor = GetOwner();
		MainBPGC = Cast<UBlueprintGeneratedClass>(OwningActor->GetClass());
	}

	check(MainBPGC);
	TArray<const UBlueprintGeneratedClass*> BPGCStack;
	UBlueprintGeneratedClass::GetGeneratedClassesHierarchy(MainBPGC, BPGCStack);
	for(const UBlueprintGeneratedClass* BPGC : BPGCStack)
	{
		USimpleConstructionScript* SCS = BPGC->SimpleConstructionScript;
		if (!SCS)
			return nullptr;
		USCS_Node* SCSNode = FindSCSNodeForTemplateComponent(SCS, InComponent);
		SCSNode = SCS->FindSCSNode(InComponent->GetFName());
		if (SCSNode)
			return SCSNode;		
	}

	return nullptr;
}

#if WITH_EDITOR
USCS_Node*
UHoudiniAssetBlueprintComponent::FindSCSNodeForInstanceComponent(USimpleConstructionScript* SCS, const UActorComponent* InComponent) const
{
	const TArray<USCS_Node*>& AllNodes = SCS->GetAllNodes();

	if (!InComponent)
		return nullptr;

	for (USCS_Node* Node : AllNodes)
	{
		if (!Node)
			continue;
		if (Node->EditorComponentInstance == InComponent)
			return Node;
	}

	return nullptr;
}
#endif

//void UHoudiniAssetBlueprintComponent::OnOutputProcessingCompletedHandler(UHoudiniAssetComponent* InComponent)
//{
//
//	UHoudiniAssetBlueprintComponent* TemplateComponent = Cast<UHoudiniAssetBlueprintComponent>(InComponent);
//	if (!IsValid(TemplateComponent))
//		return;
//
//	CopyStateFromComponent(TemplateComponent);
//	bForceNeedUpdate = true;
//}

//#if WITH_EDITOR
//void UHoudiniAssetBlueprintComponent::ReceivedAssetEditorRequestCloseEvent(UObject* Asset, EAssetEditorCloseReason CloseReason)
//{
//	
//	if (CachedBlueprint.Get())
//	{
//	}
//
//	if (Asset)
//	{
//
//	}
//	
//}
//#endif

void
UHoudiniAssetBlueprintComponent::CachePreviewState()
{
	bCachedIsPreview = false;

#if WITH_EDITOR
	AActor* ComponentOwner = GetOwner();
	if (!IsValid(ComponentOwner))
		return;

	USimpleConstructionScript* SCS = GetSCS();
	if (SCS == nullptr)
		return;

	// Get the preview actor directly from the BlueprintEditor.
	FBlueprintEditor* BlueprintEditor = FHoudiniEngineRuntimeUtils::GetBlueprintEditor(this);
	if (BlueprintEditor)
	{
		AActor* PreviewActor = BlueprintEditor->GetPreviewActor();
		if (PreviewActor == ComponentOwner)
		{
			bCachedIsPreview = true;
			return;
		}
	}
#endif
}

void
UHoudiniAssetBlueprintComponent::CacheBlueprintData()
{
	CachedBlueprint = nullptr;
	CachedActorCDO = nullptr;
	CachedTemplateComponent = nullptr;
#if WITH_EDITOR
	//CachedAssetEditorManager = &FAssetEditorManager::Get();
#endif

	if (CreationMethod != EComponentCreationMethod::SimpleConstructionScript)
		return;

	AActor* ComponentOwner = this->GetOwner();
	if (!IsValid(ComponentOwner))
		return;

	UClass* OwnerClass = ComponentOwner->GetClass();
	if (!IsValid(OwnerClass))
		return;

	CachedBlueprint = Cast<UBlueprint>(OwnerClass->ClassGeneratedBy);
	if (!CachedBlueprint.IsValid())
		return;

	UBlueprintGeneratedClass* MainBPGC = Cast<UBlueprintGeneratedClass>(OwnerClass);
	USCS_Node* SCSNode = FindSCSNodeForTemplateComponentInClassHierarchy(this);
	if (SCSNode)
		CachedTemplateComponent = Cast<UHoudiniAssetBlueprintComponent>(SCSNode->ComponentTemplate); 

	// if (MainBPGC)
	// {
	// 	check(MainBPGC);
	// 	TArray<const UBlueprintGeneratedClass*> BPGCStack;
	// 	UBlueprintGeneratedClass::GetGeneratedClassesHierarchy(MainBPGC, BPGCStack);
	// 	for(const UBlueprintGeneratedClass* BPGC : BPGCStack)
	// 	{
	// 		USimpleConstructionScript* SCS = BPGC->SimpleConstructionScript;
	// 		if (!SCS)
	// 			continue;
	// 		USCS_Node* SCSNode = FindSCSNodeForTemplateComponent(SCS, this);
	// 		SCSNode = SCS->FindSCSNode(this->GetFName());
	// 		if (SCSNode)
	// 		{
	// 			CachedTemplateComponent = Cast<UHoudiniAssetBlueprintComponent>(SCSNode->ComponentTemplate);
	// 		}
	// 	}
	// }

	// CachedActorCDO = Cast< AActor >(CachedBlueprint->GeneratedClass->GetDefaultObject());
	// if (!CachedActorCDO.IsValid() || (CachedActorCDO.Get() == ComponentOwner))
	// 	return;
	//
	// UActorComponent* TargetComponent = EditorUtilities::FindMatchingComponentInstance(this, CachedActorCDO.Get());
	// CachedTemplateComponent = Cast<UHoudiniAssetBlueprintComponent>(TargetComponent);

}

USimpleConstructionScript*
UHoudiniAssetBlueprintComponent::GetSCS() const
{
	if (!CachedBlueprint.Get())
		return nullptr;

	return CachedBlueprint->SimpleConstructionScript;
	/*
	AActor* ComponentOwner = this->GetOwner();
	if (!IsValid(ComponentOwner))
		return nullptr;
	UClass* OwnerClass = ComponentOwner->GetClass();
	if (!IsValid(OwnerClass))
		return nullptr;
	UBlueprint* Blueprint = Cast<UBlueprint>(OwnerClass->ClassGeneratedBy);
	if (!IsValid(Blueprint))
		return nullptr;
	return Blueprint->SimpleConstructionScript;
	*/
}

//------------------------------------------------------------------------------------------------
// FHoudiniAssetBlueprintInstanceData
//------------------------------------------------------------------------------------------------

FHoudiniAssetBlueprintInstanceData::FHoudiniAssetBlueprintInstanceData()
{

}

FHoudiniAssetBlueprintInstanceData::FHoudiniAssetBlueprintInstanceData(const UHoudiniAssetBlueprintComponent* SourceComponent)
	: FActorComponentInstanceData(SourceComponent)
{

}

void 
FHoudiniAssetBlueprintInstanceData::AddReferencedObjects(FReferenceCollector & Collector)
{
	Super::AddReferencedObjects(Collector);
	// TODO: Do we need to add references to output objects here?
	// Any other references?
	// What are the implications?
}
