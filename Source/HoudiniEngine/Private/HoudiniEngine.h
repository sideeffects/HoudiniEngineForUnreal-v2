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

#include "HAPI/HAPI_Common.h"
#include "HoudiniEnginePrivatePCH.h"
#include "HoudiniEngineTaskInfo.h"
#include "HoudiniRuntimeSettings.h"

#include "Modules/ModuleInterface.h"

class FRunnableThread;
class FHoudiniEngineScheduler;
class FHoudiniEngineManager;
class UHoudiniAssetComponent;
class UStaticMesh;
class UMaterial;

struct FSlateDynamicImageBrush;

// Not using the IHoudiniEngine interface for now
class HOUDINIENGINE_API FHoudiniEngine : public IModuleInterface
{
	public:

		FHoudiniEngine();

		virtual void StartupModule() override;
		virtual void ShutdownModule() override;

		// Return singleton instance of Houdini Engine, used internally.
		static FHoudiniEngine & Get();
		// Return true if singleton instance has been created.
		static bool IsInitialized();

		// Return the location of the currently loaded LibHAPI
		virtual const FString & GetLibHAPILocation() const;

		// Session accessor
		virtual const HAPI_Session* GetSession() const;
		// Creates a new session
		bool StartSession(
			HAPI_Session*& SessionPtr,
			const bool& StartAutomaticServer,
			const float& AutomaticServerTimeout,
			const EHoudiniRuntimeSettingsSessionType& SessionType,
			const FString& ServerPipeName,
			const int32& ServerPort,
			const FString& ServerHost);
		// Stop the current session if it is valid
		bool StopSession(HAPI_Session*& SessionPtr);


		// Stops the current session
		bool StopSession();
		// Stops, then creates a new session
		bool RestartSession();
		// Creates a session, start HARS
		bool CreateSession(const EHoudiniRuntimeSettingsSessionType& SessionType);
		// Connect to an existing HE session
		bool ConnectSession(const EHoudiniRuntimeSettingsSessionType& SessionType);

		// Initialize HAPI
		bool InitializeHAPISession();

		// Indicate to the plugin that the session is now invalid (HAPI has likely crashed...)
		void OnSessionLost();

		bool CreateTaskSlateNotification(
			const FText& InText,
			const bool& bForceNow = false,
			const float& NotificationExpire = HAPI_UNREAL_NOTIFICATION_EXPIRE,
			const float& NotificationFadeOut = HAPI_UNREAL_NOTIFICATION_FADEOUT);

		bool UpdateTaskSlateNotification(const FText& InText);
		bool FinishTaskSlateNotification(const FText& InText);

		void SetHapiNotificationStartedTime(const double& InTime) { HapiNotificationStarted = InTime; };

		// Register task for execution.
		virtual void AddTask(const FHoudiniEngineTask & InTask);
		// Register task info.
		virtual void AddTaskInfo(const FGuid& InHapiGUID, const FHoudiniEngineTaskInfo & InTaskInfo);
		// Remove task info.
		virtual void RemoveTaskInfo(const FGuid& InHapiGUID);
		// Remove task info.
		virtual bool RetrieveTaskInfo(const FGuid& InHapiGUID, FHoudiniEngineTaskInfo & OutTaskInfo);
		// Register asset to the manager
		//virtual void AddHoudiniAssetComponent(UHoudiniAssetComponent* HAC);

		// Indicates whether or not cooking is currently enabled
		bool IsCookingEnabled() const;
		// Sets whether or not cooking is currently enabled
		void SetCookingEnabled(const bool& bInEnableCooking);

		// Indicates whether or not the first attempt to create a Houdini session was made
		bool GetFirstSessionCreated() const;
		// Sets whether or not the first attempt to create a Houdini session was made
		void SetFirstSessionCreated(const bool& bInStarted) { bFirstSessionCreated = bInStarted; };

		bool IsTwoWayDebuggerEnabled() const { return bEnableTwoWayHEngineDebugger; };

		// Returns the default Houdini Logo Static Mesh
		virtual TWeakObjectPtr<UStaticMesh> GetHoudiniLogoStaticMesh() const { return HoudiniLogoStaticMesh; };
		// Returns the default Houdini material
		virtual TWeakObjectPtr<UMaterial> GetHoudiniDefaultMaterial() const { return HoudiniDefaultMaterial; };
		// Returns a shared Ptr to the houdini logo
		TSharedPtr<FSlateDynamicImageBrush> GetHoudiniLogoBrush() const { return HoudiniLogoBrush; };

		const HAPI_License GetLicenseType() const { return LicenseType; };

		const bool IsLicenseIndie() const { return (LicenseType == HAPI_LICENSE_HOUDINI_ENGINE_INDIE || LicenseType == HAPI_LICENSE_HOUDINI_INDIE); };

	private:

		// Singleton instance of Houdini Engine.
		static FHoudiniEngine * HoudiniEngineInstance;

		// Location of libHAPI binary. 
		FString LibHAPILocation;

		// The Houdini Engine session. 
		HAPI_Session Session;

		// The type of HE license used by the current session
		HAPI_License LicenseType;

		// Synchronization primitive.
		FCriticalSection CriticalSection;
		
		// Map of task statuses.
		TMap<FGuid, FHoudiniEngineTaskInfo> TaskInfos;

		// Thread used to execute the scheduler.
		FRunnableThread * HoudiniEngineSchedulerThread;
		// Scheduler used to schedule HAPI instantiation and cook tasks. 
		FHoudiniEngineScheduler * HoudiniEngineScheduler;

		// Thread used to execute the manager.
		FRunnableThread * HoudiniEngineManagerThread;
		// Scheduler used to monitor and process Houdini Asset Components
		FHoudiniEngineManager * HoudiniEngineManager;

		// Is set to true when mismatch between defined and running HAPI versions is detected. 
		//bool bHAPIVersionMismatch;

		// Global cooking flag, used to pause HEngine while using the editor 
		bool bEnableCookingGlobal;

		// Indicates that the first attempt to create a session has been done
		// This is to delay the first "automatic" session creation for the first cook 
		// or instantiation rather than when the module started.
		bool bFirstSessionCreated;

		// Indicates if we want to automatically push changes made in a HE debugger session to UE4
		bool bEnableTwoWayHEngineDebugger;

		// Static mesh used for Houdini logo rendering.
		TWeakObjectPtr<UStaticMesh> HoudiniLogoStaticMesh;

		// Material used as default material.
		TWeakObjectPtr<UMaterial> HoudiniDefaultMaterial;

		// Houdini logo brush.
		TSharedPtr<FSlateDynamicImageBrush> HoudiniLogoBrush;

#if WITH_EDITOR
		/** Notification used by this component. **/
		TWeakPtr<class SNotificationItem> NotificationPtr;
		/** Used to delay notification updates for HAPI asynchronous work. **/
		double HapiNotificationStarted;
#endif
};