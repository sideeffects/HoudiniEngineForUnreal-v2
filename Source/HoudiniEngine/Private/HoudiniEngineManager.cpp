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

#include "HoudiniEngineManager.h"

#include "HoudiniEngineRuntimePrivatePCH.h"
#include "HoudiniEngine.h"
#include "HoudiniEngineRuntime.h"
#include "HoudiniAsset.h"
#include "HoudiniAssetComponent.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniParameterTranslator.h"
#include "HoudiniPDGManager.h"
#include "HoudiniInputTranslator.h"
#include "HoudiniOutputTranslator.h"
#include "HoudiniHandletranslator.h"
#include "HoudiniSplineTranslator.h"

#include "Misc/MessageDialog.h"
#include "Misc/ScopedSlowTask.h"

#if WITH_EDITOR
	#include "Editor.h"
#endif

const float
FHoudiniEngineManager::TickTimerDelay = 0.05f;

FHoudiniEngineManager::FHoudiniEngineManager()
	: CurrentIndex(0)
	, ComponentCount(0)
	, bStopping(false)
{

}

FHoudiniEngineManager::~FHoudiniEngineManager()
{

}

void 
FHoudiniEngineManager::StartHoudiniTicking()
{
	// If we have no timer delegate spawned, spawn one.
	if (!TimerDelegateProcess.IsBound() && GEditor)
	{
		TimerDelegateProcess = FTimerDelegate::CreateRaw(this, &FHoudiniEngineManager::Tick);
		GEditor->GetTimerManager()->SetTimer(TimerHandleProcess, TimerDelegateProcess, TickTimerDelay, true);

		// Grab current time for delayed notification.
		FHoudiniEngine::Get().SetHapiNotificationStartedTime(FPlatformTime::Seconds());
	}
}

void 
FHoudiniEngineManager::StopHoudiniTicking()
{
	if (TimerDelegateProcess.IsBound() && GEditor)
	{
		GEditor->GetTimerManager()->ClearTimer(TimerHandleProcess);
		TimerDelegateProcess.Unbind();

		// Reset time for delayed notification.
		FHoudiniEngine::Get().SetHapiNotificationStartedTime(0.0);
	}
}

void
FHoudiniEngineManager::Tick()
{
	// Process the current component if possible
	while (true)
	{
		UHoudiniAssetComponent * CurrentComponent = nullptr;
		if (FHoudiniEngineRuntime::IsInitialized())
		{
			FHoudiniEngineRuntime::Get().CleanUpRegisteredHoudiniComponents();

			//FScopeLock ScopeLock(&CriticalSection);
			ComponentCount = FHoudiniEngineRuntime::Get().GetRegisteredHoudiniComponentCount();

			// No work to be done
			if (ComponentCount <= 0)
				break;

			// Wrap around if needed
			if (CurrentIndex >= ComponentCount)
				CurrentIndex = 0;

			CurrentComponent = FHoudiniEngineRuntime::Get().GetRegisteredHoudiniComponentAt(CurrentIndex);
			CurrentIndex++;
		}

		if (!CurrentComponent || !CurrentComponent->IsValidLowLevelFast())
		{
			// Invalid component, do not process
			break;
		}
		else if (CurrentComponent->IsPendingKill()
			|| CurrentComponent->GetAssetState() == EHoudiniAssetState::Deleting)
		{
			// Component being deleted, do not process
			break;
		}

		// See if we should start the default "first" session
		if(!FHoudiniEngine::Get().GetSession() && !FHoudiniEngine::Get().GetFirstSessionCreated())
		{
			// Only try to start the default session if we have an "active" HAC
			if (CurrentComponent->GetAssetState() == EHoudiniAssetState::PreInstantiation
				|| CurrentComponent->GetAssetState() == EHoudiniAssetState::Instantiating
				|| CurrentComponent->GetAssetState() == EHoudiniAssetState::PreCook
				|| CurrentComponent->GetAssetState() == EHoudiniAssetState::Cooking)
			{
				FString StatusText = TEXT("Initializing Houdini Engine...");
				FHoudiniEngine::Get().CreateTaskSlateNotification(FText::FromString(StatusText), true, 4.0f);

				// We want to yield for a bit.
				//FPlatformProcess::Sleep(0.5f);

				// Indicates that we've tried to start the session once no matter if it failed or succeed
				FHoudiniEngine::Get().SetFirstSessionCreated(true);

				// Attempt to restart the session
				if (!FHoudiniEngine::Get().RestartSession())
				{
					// We failed to start the session
					// Stop ticking until it's manually restarted
					StopHoudiniTicking();

					StatusText = TEXT("Houdini Engine failed to initialize.");
				}
				else
				{
					StatusText = TEXT("Houdini Engine successfully initialized.");
				}

				// Finish the notification and display the results
				FHoudiniEngine::Get().FinishTaskSlateNotification(FText::FromString(StatusText));
			}
		}

		// Process the component
		// try to catch (apache::thrift::transport::TTransportException * e) for session loss?
		ProcessComponent(CurrentComponent);
		break;
	}

	// Handle Asset delete
	if (FHoudiniEngineRuntime::IsInitialized())
	{
		int32 PendingDeleteCount = FHoudiniEngineRuntime::Get().GetNodeIdsPendingDeleteCount();
		for (int32 DeleteIdx = PendingDeleteCount - 1; DeleteIdx >= 0; DeleteIdx--)
		{
			HAPI_NodeId NodeIdToDelete = (HAPI_NodeId)FHoudiniEngineRuntime::Get().GetNodeIdsPendingDeleteAt(DeleteIdx);
			FGuid HapiDeletionGUID;
			bool bShouldDeleteParent = FHoudiniEngineRuntime::Get().IsParentNodePendingDelete(NodeIdToDelete);
			if (StartTaskAssetDelete(NodeIdToDelete, HapiDeletionGUID, bShouldDeleteParent))
			{
				FHoudiniEngineRuntime::Get().RemoveNodeIdPendingDeleteAt(DeleteIdx);
				if (bShouldDeleteParent)
					FHoudiniEngineRuntime::Get().RemoveParentNodePendingDelete(NodeIdToDelete);
			}
		}
	}

	// Update PDG Contexts and asset link if needed
	PDGManager.Update();
}

void
FHoudiniEngineManager::ProcessComponent(UHoudiniAssetComponent* HAC)
{
	if (!HAC || HAC->IsPendingKill())
		return;

	// No need to process component not tied to an asset
	if (!HAC->GetHoudiniAsset())
		return;

	// If cooking is paused, stay in the current state until cooking's resumed
	if (!FHoudiniEngine::Get().IsCookingEnabled())
	{
		// We can only handle output updates
		if (HAC->GetAssetState() == EHoudiniAssetState::None && HAC->NeedOutputUpdate())
		{
			FHoudiniOutputTranslator::UpdateChangedOutputs(HAC);
		}

		// Prevent any other state change to happen
		return;
	}		

	switch (HAC->GetAssetState())
	{
		case EHoudiniAssetState::NeedInstantiation:
		{
			// Do nothing unless the HAC has been updated
			if (HAC->NeedUpdate())
			{
				// Update the HAC's state
				HAC->AssetState = EHoudiniAssetState::PreInstantiation;
			}
			else if (HAC->NeedOutputUpdate())
			{
				// Output updates do not recquire the HDA to be instantiated
				FHoudiniOutputTranslator::UpdateChangedOutputs(HAC);
			}

			// Update world input if we have any
			FHoudiniInputTranslator::UpdateWorldInputs(HAC);

			break;
		}

		case EHoudiniAssetState::PreInstantiation:
		{
			// Only proceed forward if we don't need to wait for our input HoudiniAssets to finish cooking/instantiating
			if (HAC->NeedsToWaitForInputHoudiniAssets())
				break;

			FGuid TaskGuid;
			UHoudiniAsset* HoudiniAsset = HAC->GetHoudiniAsset();
			if (StartTaskAssetInstantiation(HoudiniAsset, HAC->GetDisplayName(), TaskGuid))
			{
				// Update the HAC's state
				HAC->AssetState = EHoudiniAssetState::Instantiating;
				//HAC->AssetStateResult = EHoudiniAssetStateResult::None;

				// Update the Task GUID
				HAC->HapiGUID = TaskGuid;
			}
			else
			{
				// If we couldnt instantiate the asset
				// Change the state to NeedInstantiating
				HAC->AssetState = EHoudiniAssetState::NeedInstantiation;
				//HAC->AssetStateResult = EHoudiniAssetStateResult::None;
			}
			break;
		}

		case EHoudiniAssetState::Instantiating:
		{			
			EHoudiniAssetState NewState = EHoudiniAssetState::Instantiating;
			if (UpdateInstantiating(HAC, NewState))
			{
				// We need to update the HAC's state
				HAC->AssetState = NewState;
			}
			break;
		}

		case EHoudiniAssetState::PreCook:
		{
			// Only proceed forward if we don't need to wait for our input
			// HoudiniAssets to finish cooking/instantiating
			if (HAC->NeedsToWaitForInputHoudiniAssets())
				break;

			// Update all the HAPI nodes, parameters, inputs etc...
			PreCook(HAC);

			// Create a Cooking task only if necessary
			bool bCookStarted = false;
			if (IsCookingEnabledForHoudiniAsset(HAC))
			{
				FGuid TaskGUID = HAC->GetHapiGUID();
				if ( StartTaskAssetCooking(HAC->GetAssetId(), HAC->GetDisplayName(), TaskGUID) )
				{
					// Updates the HAC's state
					HAC->AssetState = EHoudiniAssetState::Cooking;
					HAC->HapiGUID = TaskGUID;
					bCookStarted = true;
				}
			}
			
			if(!bCookStarted)
			{
				// Just refresh editor properties?
				FHoudiniEngineUtils::UpdateEditorProperties(HAC, true);

				// TODO: Check! update state?
				HAC->AssetState = EHoudiniAssetState::None;
			}
			break;
		}

		case EHoudiniAssetState::Cooking:
		{
			EHoudiniAssetState NewState = EHoudiniAssetState::Cooking;
			bool state = UpdateCooking(HAC, NewState);
			if (state)
			{
				// We need to update the HAC's state
				HAC->AssetState = NewState;
			}
			break;
		}

		case EHoudiniAssetState::PostCook:
		{
			// TODO: Remove the postcook state!
			//FHoudiniInputTranslator::UpdateHoudiniInputCurves(HAC);
			break;
		}

		case EHoudiniAssetState::PreProcess:
		{
			StartTaskAssetProcess(HAC);
			break;
		}

		case EHoudiniAssetState::Processing:
		{
			UpdateProcess(HAC);
			break;
		}

		case EHoudiniAssetState::None:
		{
			// Do nothing unless the HAC has been updated
			if (HAC->NeedUpdate())
			{
				// Update the HAC's state
				HAC->AssetState = EHoudiniAssetState::PreCook;
			}
			else if (HAC->NeedTransformUpdate())
			{
				FHoudiniEngineUtils::UploadHACTransform(HAC);
			}
			else if (HAC->NeedOutputUpdate())
			{
				FHoudiniOutputTranslator::UpdateChangedOutputs(HAC);
			}

			// Update world inputs if we have any
			FHoudiniInputTranslator::UpdateWorldInputs(HAC);

			// See if we need an upodate for the 2 way HE Debugger
			if(FHoudiniEngine::Get().IsTwoWayDebuggerEnabled() && HAC->AssetState == EHoudiniAssetState::None)
			{
				int32 CookCount = FHoudiniEngineUtils::HapiGetCookCount(HAC->GetAssetId());
				if (CookCount >= 0 && CookCount != HAC->GetAssetCookCount())
				{
					// Trigger an update if the cook count has change on the Houdini side
					HAC->AssetState = EHoudiniAssetState::PreCook;
				}
			}
			break;
		}

		case EHoudiniAssetState::NeedRebuild:
		{
			StartTaskAssetRebuild(HAC->AssetId, HAC->HapiGUID);

			HAC->MarkAsNeedCook();
			HAC->AssetState = EHoudiniAssetState::PreInstantiation;
			break;
		}

		case EHoudiniAssetState::NeedDelete:
		{
			FGuid HapiDeletionGUID;
			StartTaskAssetDelete(HAC->GetAssetId(), HapiDeletionGUID, true);
				//HAC->AssetId = -1;

			// Update the HAC's state
			HAC->AssetState = EHoudiniAssetState::Deleting;
			break;
		}		

		case EHoudiniAssetState::Deleting:
		{
			break;
		}		
	}
}



bool 
FHoudiniEngineManager::StartTaskAssetInstantiation(UHoudiniAsset* HoudiniAsset, const FString& DisplayName, FGuid& OutTaskGUID)
{
	// Make sure we have a valid session before attempting anything
	if (!FHoudiniEngine::Get().GetSession())
		return false;

	OutTaskGUID.Invalidate();
	
	// Load the HDA file
	if (!HoudiniAsset || HoudiniAsset->IsPendingKill())
	{
		HOUDINI_LOG_ERROR(TEXT("Cancelling asset instantiation - null or invalid Houdini Asset."));
		return false;
	}

	HAPI_AssetLibraryId AssetLibraryId = -1;
	if (!FHoudiniEngineUtils::LoadHoudiniAsset(HoudiniAsset, AssetLibraryId) )
	{
		HOUDINI_LOG_ERROR(TEXT("Cancelling asset instantiation - could not load Houdini Asset."));
		return false;
	}

	// Handle hda files that contain multiple assets
	TArray< HAPI_StringHandle > AssetNames;
	if (!FHoudiniEngineUtils::GetSubAssetNames(AssetLibraryId, AssetNames))
	{
		HOUDINI_LOG_ERROR(TEXT("Cancelling asset instantiation - unable to retrieve asset names."));
		return false;
	}

	// By default, assume we want to load the first Asset
	HAPI_StringHandle PickedAssetName = AssetNames[0];

#if WITH_EDITOR
	// Should we show the multi asset dialog?
	bool bShowMultiAssetDialog = false;

	const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault< UHoudiniRuntimeSettings >();
	if (HoudiniRuntimeSettings && AssetNames.Num() > 1)
		bShowMultiAssetDialog = HoudiniRuntimeSettings->bShowMultiAssetDialog;

	// TODO: Add multi selection dialog
	if (bShowMultiAssetDialog )
	{
		// TODO: Implement
		FHoudiniEngineUtils::OpenSubassetSelectionWindow(AssetNames, PickedAssetName);
	}
#endif

	// Give the HAC a new GUID to identify this request.
	OutTaskGUID = FGuid::NewGuid();

	// Create a new instantiation task
	FHoudiniEngineTask Task(EHoudiniEngineTaskType::AssetInstantiation, OutTaskGUID);
	Task.Asset = HoudiniAsset;
	Task.ActorName = DisplayName;
	//Task.bLoadedComponent = bLocalLoadedComponent;
	Task.AssetLibraryId = AssetLibraryId;
	Task.AssetHapiName = PickedAssetName;

	// Add the task to the stack
	FHoudiniEngine::Get().AddTask(Task);

	return true;
}

bool 
FHoudiniEngineManager::UpdateInstantiating(UHoudiniAssetComponent* HAC, EHoudiniAssetState& NewState )
{
	check(HAC);

	// Will return true if the asset's state need to be updated
	NewState = HAC->GetAssetState();
	bool bUpdateState = false;

	// Get the HAC display name for the logs
	FString DisplayName = HAC->GetDisplayName();

	// Get the current task's progress
	FHoudiniEngineTaskInfo TaskInfo;	
	if ( !UpdateTaskStatus(HAC->HapiGUID, TaskInfo) 
		|| TaskInfo.TaskType != EHoudiniEngineTaskType::AssetInstantiation)
	{
		// Couldnt get a valid task info
		HOUDINI_LOG_ERROR(TEXT("    %s Failed to instantiate - invalid task"), *DisplayName);
		NewState = EHoudiniAssetState::NeedInstantiation;
		bUpdateState = true;
		return bUpdateState;
	}

	bool bSuccess = false;
	bool bFinished = false;
	switch (TaskInfo.TaskState)
	{
		case EHoudiniEngineTaskState::Success:
		{
			bSuccess = true;
			bFinished = true;
			break;
		}

		case EHoudiniEngineTaskState::Aborted:
		case EHoudiniEngineTaskState::FinishedWithError:
		case EHoudiniEngineTaskState::FinishedWithFatalError:
		{
			bSuccess = false;
			bFinished = true;
			break;
		}

		case EHoudiniEngineTaskState::None:
		case EHoudiniEngineTaskState::Working:
		{
			bFinished = false;
			break;
		}
	}

	if ( !bFinished )
	{
		// Task is still in progress, nothing to do for now
		return false;
	}

	if ( bSuccess && (TaskInfo.AssetId < 0) )
	{
		// Task finished successfully but we received an invalid asset ID, error out
		HOUDINI_LOG_ERROR(TEXT("    %s Finished Instantiation but received invalid asset id."), *DisplayName);
		bSuccess = false;
	}

	if ( bSuccess )
	{
		HOUDINI_LOG_MESSAGE(TEXT("    %s FinishedInstantiation."), *DisplayName);

		// Set the new Asset ID
		HAC->AssetId = TaskInfo.AssetId;

		// Assign a unique name to the actor if needed
		FHoudiniEngineUtils::AssignUniqueActorLabelIfNeeded(HAC);

		// TODO: Create default preset buffer.
		/*TArray< char > DefaultPresetBuffer;
		if (!FHoudiniEngineUtils::GetAssetPreset(TaskInfo.AssetId, DefaultPresetBuffer))
			DefaultPresetBuffer.Empty();*/

		// Reset the cook counter.
		HAC->SetAssetCookCount(0);

		// If necessary, set asset transform.
		if (HAC->bUploadTransformsToHoudiniEngine)
		{
			// Retrieve the current component-to-world transform for this component.
			if (!FHoudiniEngineUtils::HapiSetAssetTransform(TaskInfo.AssetId, HAC->GetComponentTransform()))
				HOUDINI_LOG_MESSAGE(TEXT("Failed to upload the initial Transform back to HAPI."));
		}

		// See if this Asset is a PDG Asset
		PDGManager.InitializePDGAssetLink(HAC);

		// Update the HAC's state
		NewState = EHoudiniAssetState::PreCook;
		return true;
	}
	else
	{
		HOUDINI_LOG_ERROR(TEXT("    %s FinishedInstantiationWithErrors."), *DisplayName);

		bool bLicensingIssue = false;
		switch (TaskInfo.Result)
		{
			case HAPI_RESULT_NO_LICENSE_FOUND:
			{
				//FHoudiniEngine::Get().SetHapiState(HAPI_RESULT_NO_LICENSE_FOUND);
				bLicensingIssue = true;
				break;
			}

			case HAPI_RESULT_DISALLOWED_NC_LICENSE_FOUND:
			case HAPI_RESULT_DISALLOWED_NC_ASSET_WITH_C_LICENSE:
			case HAPI_RESULT_DISALLOWED_NC_ASSET_WITH_LC_LICENSE:
			case HAPI_RESULT_DISALLOWED_LC_ASSET_WITH_C_LICENSE:
			{
				bLicensingIssue = true;
				break;
			}

			default:
			{
				break;
			}
		}

		if (bLicensingIssue)
		{
			const FString & StatusMessage = TaskInfo.StatusText.ToString();
			HOUDINI_LOG_MESSAGE(TEXT("%s"), *StatusMessage);

			FString WarningTitle = TEXT("Houdini Engine Plugin Warning");
			FText WarningTitleText = FText::FromString(WarningTitle);
			FString WarningMessage = FString::Printf(TEXT("Houdini License issue - %s."), *StatusMessage);

			FMessageDialog::Debugf(FText::FromString(WarningMessage), &WarningTitleText);
		}

		// Reset the cook counter.
		HAC->SetAssetCookCount(0);

		// Make sure the asset ID is invalid
		HAC->AssetId = -1;

		// Update the HAC's state
		HAC->AssetState = EHoudiniAssetState::NeedInstantiation;
		//HAC->AssetStateResult = EHoudiniAssetStateResult::Success;

		return true;
	}
}

bool
FHoudiniEngineManager::StartTaskAssetCooking(const HAPI_NodeId& AssetId, const FString& DisplayName, FGuid& OutTaskGUID)
{
	// Make sure we have a valid session before attempting anything
	if (!FHoudiniEngine::Get().GetSession())
		return false;

	// Check we have a valid AssetId
	if (AssetId < 0)
		return false;

	// Check this HAC doesn't already have a running task
	if (OutTaskGUID.IsValid())
		return false;

	// Generate a GUID for our new task.
	OutTaskGUID = FGuid::NewGuid();

	// Add a new cook task
	FHoudiniEngineTask Task(EHoudiniEngineTaskType::AssetCooking, OutTaskGUID);
	Task.ActorName = DisplayName;
	Task.AssetId = AssetId;
	FHoudiniEngine::Get().AddTask(Task);

	return true;
}

bool
FHoudiniEngineManager::UpdateCooking(UHoudiniAssetComponent* HAC, EHoudiniAssetState& NewState)
{
	check(HAC);

	// Will return true if the asset's state need to be updated
	NewState = HAC->GetAssetState();
	bool bUpdateState = false;

	// Get the HAC display name for the logs
	FString DisplayName = HAC->GetDisplayName();

	// Get the current task's progress
	FHoudiniEngineTaskInfo TaskInfo;
	if (!UpdateTaskStatus(HAC->HapiGUID, TaskInfo)
		|| TaskInfo.TaskType != EHoudiniEngineTaskType::AssetCooking)
	{
		// Couldnt get a valid task info
		HOUDINI_LOG_ERROR(TEXT("    %s Failed to cook - invalid task"), *DisplayName);
		NewState = EHoudiniAssetState::None;
		bUpdateState = true;
		return bUpdateState;
	}

	bool bSuccess = false;
	switch (TaskInfo.TaskState)
	{
		case EHoudiniEngineTaskState::Success:
		{
			HOUDINI_LOG_MESSAGE(TEXT("   %s FinishedCooking."), *DisplayName);
			bSuccess = true;
			bUpdateState = true;
		}
		break;

		case EHoudiniEngineTaskState::FinishedWithError:
		{
			// We finished with cook error, will still try to process the results
			HOUDINI_LOG_MESSAGE(TEXT("   %s FinishedCooking with errors - will try to process the available results."), *DisplayName);
			bSuccess = true;
			bUpdateState = true;
		}
		break;

		case EHoudiniEngineTaskState::Aborted:
		case EHoudiniEngineTaskState::FinishedWithFatalError:
		{
			HOUDINI_LOG_MESSAGE(TEXT("   %s FinishedCooking with fatal errors - aborting."), *DisplayName);
			bSuccess = false;
			bUpdateState = true;
		}
		break;

		case EHoudiniEngineTaskState::None:
		case EHoudiniEngineTaskState::Working:
		{
			// Task is still in progress, nothing to do for now
			// return false so we do not update the state
			bUpdateState = false;
		}
		break;
	}

	// If the task is still in progress, return now
	if (!bUpdateState)
		return false;
	   
	// Handle PostCook
	if (PostCook(HAC, bSuccess, TaskInfo.AssetId))
	{
		// Cook was successfull, process the results
		NewState = EHoudiniAssetState::PreProcess;
	}
	else
	{
		// Cook failed, skip output processing
		NewState = EHoudiniAssetState::None;
	}

	return true;
}

bool
FHoudiniEngineManager::PreCook(UHoudiniAssetComponent* HAC)
{
	// Handle duplicated HAC
	// We need to clean/duplicate some of the HAC's output data manually here
	if (HAC->HasBeenDuplicated())
	{
		HAC->UpdatePostDuplicate();
	}

	// Upload the changed/parameters back to HAPI
	// If cooking is disabled, we still try to upload parameters
	if (HAC->HasBeenLoaded())
	{
		// Handle loaded parameters
		FHoudiniParameterTranslator::UpdateLoadedParameters(HAC);

		// Handle loaded inputs
		FHoudiniInputTranslator::UpdateLoadedInputs(HAC);

		// Handle loaded outputs
		FHoudiniOutputTranslator::UpdateLoadedOutputs(HAC);

		// TODO: Handle loaded curve
		// TODO: Handle editable node
		// TODO: Restore parameter preset data
	}

	// Try to upload changed parameters
	FHoudiniParameterTranslator::UploadChangedParameters(HAC);

	// Try to upload changed inputs
	FHoudiniInputTranslator::UploadChangedInputs(HAC);

	// Try to upload changed editable nodes
	FHoudiniOutputTranslator::UploadChangedEditableOutput(HAC, false);

	// Upload the asset's transform if needed
	if (HAC->NeedTransformUpdate())
		FHoudiniEngineUtils::UploadHACTransform(HAC);
	
	HAC->ClearRefineMeshesTimer();

	return true;
}

bool
FHoudiniEngineManager::PostCook(UHoudiniAssetComponent* HAC, const bool& bSuccess, const HAPI_NodeId& TaskAssetId)
{
	// Get the HAC display name for the logs
	FString DisplayName = HAC->GetDisplayName();

	bool bCookSuccess = bSuccess;
	if (bCookSuccess && (TaskAssetId < 0))
	{
		// Task finished successfully but we received an invalid asset ID, error out
		HOUDINI_LOG_ERROR(TEXT("    %s received an invalid asset id - aborting."), *DisplayName);
		bCookSuccess = false;
	}

	// Update the asset cook count using the node infos
	int32 CookCount = FHoudiniEngineUtils::HapiGetCookCount(HAC->GetAssetId());
	HAC->SetAssetCookCount(CookCount);
	/*	
	if(CookCount >= 0 )
		HAC->SetAssetCookCount(CookCount);
	else
		HAC->SetAssetCookCount(HAC->GetAssetCookCount()+1);
	*/

	bool bNeedsToTriggerViewportUpdate = false;
	HAC->SetRecookRequested(false);
	if (bCookSuccess)
	{
		FHoudiniEngine::Get().CreateTaskSlateNotification(FText::FromString("Processing outputs..."));

		// Set new asset id.
		HAC->AssetId = TaskAssetId;
		
		FHoudiniParameterTranslator::UpdateParameters(HAC);

		FHoudiniInputTranslator::UpdateInputs(HAC);

		bool bHasHoudiniStaticMeshOutput = false;
		FHoudiniOutputTranslator::UpdateOutputs(HAC, false, bHasHoudiniStaticMeshOutput);
		HAC->SetNoProxyMeshNextCookRequested(false);

		// Handles have to be updated after parameters
		FHoudiniHandleTranslator::UpdateHandles(HAC);  

		// Clear the HasBeenLoaded flag
		if (HAC->HasBeenLoaded())
		{
			HAC->SetHasBeenLoaded(false);
		}

		// Clear the HasBeenDuplicated flag
		if (HAC->HasBeenDuplicated())
		{
			HAC->SetHasBeenDuplicated(false);
		}

		// TODO: Need to update rendering information.
		// UpdateRenderingInformation();

		FHoudiniEngine::Get().FinishTaskSlateNotification(FText::FromString("Finished processing outputs"));

		// Trigger a details panel update
		FHoudiniEngineUtils::UpdateEditorProperties(HAC, true);

		// If any outputs have HoudiniStaticMeshes, and if timer based refinement is enabled on the HAC,
		// set the RefineMeshesTimer and ensure BuildStaticMeshesForAllHoudiniStaticMeshes is bound to
		// the RefineMeshesTimerFired delegate of the HAC
		if (bHasHoudiniStaticMeshOutput && HAC->IsProxyStaticMeshRefinementByTimerEnabled())
		{
			if (!HAC->GetOnRefineMeshesTimerDelegate().IsBoundToObject(this))
				HAC->GetOnRefineMeshesTimerDelegate().AddRaw(this, &FHoudiniEngineManager::BuildStaticMeshesForAllHoudiniStaticMeshes);
			HAC->SetRefineMeshesTimer();
		}

		if (bHasHoudiniStaticMeshOutput)
			bNeedsToTriggerViewportUpdate = true;
	}
	else
	{
		// TODO: Create parameters inputs and handles inputs.
		//CreateParameters();
		//CreateInputs();
		//CreateHandles();
	}

	if (HAC->InputPresets.Num() > 0)
	{
		HAC->ApplyInputPresets();
	}

	// If we have downstream HDAs, we need to tell them we're done cooking
	HAC->NotifyCookedToDownstreamAssets();
	
	// Notify the PDG manager that the HDA is done cooking
	FHoudiniPDGManager::NotifyAssetCooked(HAC->PDGAssetLink, bSuccess);

	if (bNeedsToTriggerViewportUpdate && GEditor)
	{
		// We need to manually update the vieport with HoudiniMeshProxies
		// if not, modification made in H with the two way debugger wont be visible in Unreal until the vieports gets focus
		GEditor->RedrawAllViewports(false);
	}

	return bCookSuccess;
}

bool
FHoudiniEngineManager::StartTaskAssetProcess(UHoudiniAssetComponent* HAC)
{
	HAC->AssetState = EHoudiniAssetState::Processing;

	return true;
}

bool
FHoudiniEngineManager::UpdateProcess(UHoudiniAssetComponent* HAC)
{
	HAC->AssetState = EHoudiniAssetState::None;

	return true;
}

bool
FHoudiniEngineManager::StartTaskAssetRebuild(const HAPI_NodeId& InAssetId, FGuid& OutTaskGUID)
{
	// Check this HAC doesn't already have a running task
	if (OutTaskGUID.IsValid())
		return false;

	if (InAssetId >= 0)
	{
		/* TODO: Handle Asset Preset
		if (!FHoudiniEngineUtils::GetAssetPreset(AssetId, PresetBuffer))
		{
			HOUDINI_LOG_WARNING(TEXT("Failed to get the asset's preset, rebuilt asset may have lost its parameters."));
		}
		*/
		// Delete the asset
		if (!StartTaskAssetDelete(InAssetId, OutTaskGUID, true))
		{
			return false;
		}
	}

	// Create a new task GUID for this asset
	OutTaskGUID = FGuid::NewGuid();

	return true;
}

bool
FHoudiniEngineManager::StartTaskAssetDelete(const HAPI_NodeId& InNodeId, FGuid& OutTaskGUID, bool bShouldDeleteParent)
{
	if (InNodeId < 0)
		return false;

	// Get the Asset's NodeInfo
	HAPI_NodeInfo AssetNodeInfo;
	FHoudiniApi::NodeInfo_Init(&AssetNodeInfo);
	HOUDINI_CHECK_ERROR(FHoudiniApi::GetNodeInfo(
		FHoudiniEngine::Get().GetSession(), InNodeId, &AssetNodeInfo));
	
	HAPI_NodeId OBJNodeToDelete = InNodeId;
	if (AssetNodeInfo.type == HAPI_NODETYPE_SOP)
	{
		// For SOP Asset, we want to delete their parent's OBJ node
		if (bShouldDeleteParent)
		{
			HAPI_NodeId ParentId = FHoudiniEngineUtils::HapiGetParentNodeId(OBJNodeToDelete);
			OBJNodeToDelete = ParentId != -1 ? ParentId : OBJNodeToDelete;
		}
	}

	// Generate GUID for our new task.
	OutTaskGUID = FGuid::NewGuid();

	// Create asset deletion task object and submit it for processing.
	FHoudiniEngineTask Task(EHoudiniEngineTaskType::AssetDeletion, OutTaskGUID);
	Task.AssetId = OBJNodeToDelete;
	FHoudiniEngine::Get().AddTask(Task);

	return true;
}

bool
FHoudiniEngineManager::UpdateTaskStatus(FGuid& OutTaskGUID, FHoudiniEngineTaskInfo& OutTaskInfo)
{
	if (!OutTaskGUID.IsValid())
		return false;
	
	if (!FHoudiniEngine::Get().RetrieveTaskInfo(OutTaskGUID, OutTaskInfo))
	{
		// Task information does not exist
		OutTaskGUID.Invalidate();
		return false;
	}

	// Check whether we want to display Slate cooking and instantiation notifications.
	bool bDisplaySlateCookingNotifications = false;
	const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault<UHoudiniRuntimeSettings>();
	if (HoudiniRuntimeSettings)
		bDisplaySlateCookingNotifications = HoudiniRuntimeSettings->bDisplaySlateCookingNotifications;

	if (EHoudiniEngineTaskState::None != OutTaskInfo.TaskState && bDisplaySlateCookingNotifications)
	{
		FHoudiniEngine::Get().CreateTaskSlateNotification(OutTaskInfo.StatusText);
	}

	switch (OutTaskInfo.TaskState)
	{
		case EHoudiniEngineTaskState::Aborted:
		case EHoudiniEngineTaskState::Success:
		case EHoudiniEngineTaskState::FinishedWithError:
		case EHoudiniEngineTaskState::FinishedWithFatalError:
		{
			// If the current task is finished
			// Terminate the slate notification if they exist and delete/invalidate the task
			if (bDisplaySlateCookingNotifications)
			{
				FHoudiniEngine::Get().FinishTaskSlateNotification(OutTaskInfo.StatusText);
			}

			FHoudiniEngine::Get().RemoveTaskInfo(OutTaskGUID);
			OutTaskGUID.Invalidate();
		}
		break;

		case EHoudiniEngineTaskState::Working:
		{
			// The current task is still running, simply update the current notification
			if (bDisplaySlateCookingNotifications)
			{
				FHoudiniEngine::Get().UpdateTaskSlateNotification(OutTaskInfo.StatusText);
			}
		}
		break;
	
		case EHoudiniEngineTaskState::None:
		default:
		{
			break;
		}
	}

	return true;
}

bool
FHoudiniEngineManager::IsCookingEnabledForHoudiniAsset(UHoudiniAssetComponent* HAC)
{
	bool bManualRecook = false;
	bool bComponentEnable = false;
	if (HAC && !HAC->IsPendingKill())
	{
		bManualRecook = HAC->HasRecookBeenRequested();
		bComponentEnable = HAC->IsCookingEnabled();
	}

	if (bManualRecook)
		return true;

	if (bComponentEnable && FHoudiniEngine::Get().IsCookingEnabled())
		return true;

	return false;
}

void 
FHoudiniEngineManager::BuildStaticMeshesForAllHoudiniStaticMeshes(UHoudiniAssetComponent* HAC)
{
	if (!HAC || HAC->IsPendingKill())
	{
		HOUDINI_LOG_ERROR(TEXT("FHoudiniEngineManager::BuildStaticMeshesForAllHoudiniStaticMeshes called with HAC=nullptr"));
		return;
	}

#if WITH_EDITOR
	AActor *Owner = HAC->GetOwner();
	FString Name = Owner ? Owner->GetName() : HAC->GetName();

	FScopedSlowTask Progress(2.0f, FText::FromString(FString::Printf(TEXT("Refining Proxy Mesh to Static Mesh on %s"), *Name)));
	Progress.MakeDialog();
	Progress.EnterProgressFrame(1.0f);
#endif

	FHoudiniOutputTranslator::BuildStaticMeshesOnHoudiniProxyMeshOutputs(HAC);

#if WITH_EDITOR
	Progress.EnterProgressFrame(1.0f);
#endif
}
