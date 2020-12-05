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

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Engine/EngineTypes.h"

#include "HoudiniRuntimeSettings.generated.h"

UENUM()
enum EHoudiniRuntimeSettingsSessionType
{
	// In process session.
	HRSST_InProcess UMETA(Hidden),

	// TCP socket connection to Houdini Engine server.
	HRSST_Socket UMETA(DisplayName = "TCP socket"),

	// Connection to Houdini Engine server via pipe connection.
	HRSST_NamedPipe UMETA(DisplayName = "Named pipe or domain socket"),

	// No session, prevents license/Engine cook
	HRSST_None UMETA(DisplayName = "None"),

	HRSST_MAX
};

UCLASS(config = Engine, defaultconfig)
class HOUDINIENGINERUNTIME_API UHoudiniRuntimeSettings : public UObject
{
	GENERATED_UCLASS_BODY()

	public:

		// Destructor.
		virtual ~UHoudiniRuntimeSettings();

		// 
		virtual void PostInitProperties() override;

#if WITH_EDITOR
		virtual void PostEditChangeProperty(FPropertyChangedEvent & PropertyChangedEvent) override;
#endif

protected:

		// Locate property of this class by name.
		FProperty * LocateProperty(const FString & PropertyName) const;

		// Make specified property read only.
		void SetPropertyReadOnly(const FString & PropertyName, bool bReadOnly = true);

#if WITH_EDITOR
		// Update session ui elements.
		void UpdateSessionUI();
#endif

	public:

		//-------------------------------------------------------------------------------------------------------------
		// Session options.		
		//-------------------------------------------------------------------------------------------------------------
		UPROPERTY(GlobalConfig, EditAnywhere, Category = Session)
		TEnumAsByte<enum EHoudiniRuntimeSettingsSessionType> SessionType;

		UPROPERTY(GlobalConfig, EditAnywhere, Category = Session)
		FString ServerHost;

		UPROPERTY(GlobalConfig, EditAnywhere, Category = Session)
		int32 ServerPort;

		UPROPERTY(GlobalConfig, EditAnywhere, Category = Session)
		FString ServerPipeName;

		// Whether to automatically start a HARS process
		UPROPERTY(GlobalConfig, EditAnywhere, Category = Session)
		bool bStartAutomaticServer;

		UPROPERTY(GlobalConfig, EditAnywhere, Category = Session)
		float AutomaticServerTimeout;

		// If enabled, changes made in Houdini, when connected to Houdini running in Session Sync mode will be automatically be pushed to Unreal.
		UPROPERTY(GlobalConfig, EditAnywhere, AdvancedDisplay, Category = Session)
		bool bSyncWithHoudiniCook;
			
		// If enabled, the Houdini Timeline time will be used to cook assets.
		UPROPERTY(GlobalConfig, EditAnywhere, AdvancedDisplay, Category = Session)
		bool bCookUsingHoudiniTime;

		// Enable when wanting to sync the Houdini and Unreal viewport when using Session Sync.
		UPROPERTY(GlobalConfig, EditAnywhere, AdvancedDisplay, Category = Session)
		bool bSyncViewport;

		// If enabled, Houdini's viewport will be synchronized to Unreal's when using Session Sync.
		UPROPERTY(GlobalConfig, EditAnywhere, AdvancedDisplay, Category = Session, meta = (DisplayName = "Sync the Houdini Viewport to Unreal's viewport.", EditCondition = "bSyncViewport"))
		bool bSyncHoudiniViewport;
		
		// If enabled, Unreal's viewport will be synchronized to Houdini's when using Session Sync.
		UPROPERTY(GlobalConfig, EditAnywhere, AdvancedDisplay, Category = Session, meta = (DisplayName = "Sync the Unreal Viewport to Houdini's viewport", EditCondition = "bSyncViewport"))
		bool bSyncUnrealViewport;
		
		//-------------------------------------------------------------------------------------------------------------
		// Instantiating options.
		//-------------------------------------------------------------------------------------------------------------

		// Whether to ask user to select an asset when instantiating an HDA with multiple assets inside. If disabled, will always instantiate first asset.
		// TODO: PORT THE DIALOG!!
		UPROPERTY(GlobalConfig, EditAnywhere, Category = Instantiating)
		bool bShowMultiAssetDialog;

		//-------------------------------------------------------------------------------------------------------------
		// Cooking options.		
		//-------------------------------------------------------------------------------------------------------------

		// Whether houdini engine cooking is paused or not upon initializing the plugin
		UPROPERTY(GlobalConfig, EditAnywhere, Category = Cooking)
		bool bPauseCookingOnStart;

		// Whether to display instantiation and cooking Slate notifications.
		UPROPERTY(GlobalConfig, EditAnywhere, Category = Cooking)
		bool bDisplaySlateCookingNotifications;

		// Default content folder storing all the temporary cook data (Static meshes, materials, textures, landscape layer infos...)
		UPROPERTY(GlobalConfig, EditAnywhere, Category = Cooking)
		FString DefaultTemporaryCookFolder;

		// Default content folder used when baking houdini asset data to native unreal objects
		UPROPERTY(GlobalConfig, EditAnywhere, Category = Cooking)
		FString DefaultBakeFolder;

		//-------------------------------------------------------------------------------------------------------------
		// Parameter options.		
		//-------------------------------------------------------------------------------------------------------------

		/* Deprecated!
		// Forces the treatment of ramp parameters as multiparms.
		UPROPERTY(GlobalConfig, EditAnywhere, Category = Parameters)
		bool bTreatRampParametersAsMultiparms;
		*/

		//-------------------------------------------------------------------------------------------------------------
		// Geometry Marshalling
		//-------------------------------------------------------------------------------------------------------------

		// If true, generated Landscapes will be marshalled using default unreal scaling. 
		// Generated landscape will loose a lot of precision on the Z axis but will use the same transforms
		// as Unreal's default landscape
		UPROPERTY(GlobalConfig, EditAnywhere, Category = GeometryMarshalling)
		bool MarshallingLandscapesUseDefaultUnrealScaling;
		// If true, generated Landscapes will be using full precision for their ZAxis, 
		// allowing for more precision but preventing them from being sculpted higher/lower than their min/max.
		UPROPERTY(GlobalConfig, EditAnywhere, Category = GeometryMarshalling)
		bool MarshallingLandscapesUseFullResolution;
		// If true, the min/max values used to convert heightfields to landscape will be forced values
		// This is usefull when importing multiple landscapes from different HDAs
		UPROPERTY(GlobalConfig, EditAnywhere, Category = GeometryMarshalling)
		bool MarshallingLandscapesForceMinMaxValues;
		// The minimum value to be used for Landscape conversion when MarshallingLandscapesForceMinMaxValues is enabled
		UPROPERTY(GlobalConfig, EditAnywhere, Category = GeometryMarshalling)
		float MarshallingLandscapesForcedMinValue;
		// The maximum value to be used for Landscape conversion when MarshallingLandscapesForceMinMaxValues is enabled
		UPROPERTY(GlobalConfig, EditAnywhere, Category = GeometryMarshalling)
		float MarshallingLandscapesForcedMaxValue;

		// Default resolution used when converting Unreal Spline Components to Houdini Curves (step in cm between control points, 0 only send the control points)
		UPROPERTY(GlobalConfig, EditAnywhere, Category = GeometryMarshalling)
		float MarshallingSplineResolution;

		//-------------------------------------------------------------------------------------------------------------
		// Static Mesh Options
		//-------------------------------------------------------------------------------------------------------------

		// For StaticMesh outputs: should a fast proxy be created first?
		UPROPERTY(GlobalConfig, EditAnywhere, Category = "Static Mesh", meta = (DisplayName = "Enable Proxy Static Mesh"))
		bool bEnableProxyStaticMesh;

		// For static mesh outputs and socket actors: should spawn a default actor if the reference is invalid?
		UPROPERTY(GlobalConfig, EditAnywhere, Category = "Static Mesh", meta = (DisplayName = "Show Default Mesh"))
		bool bShowDefaultMesh;

		// If fast proxy meshes are being created, must it be baked as a StaticMesh after a period of no updates?
		UPROPERTY(GlobalConfig, EditAnywhere, AdvancedDisplay, Category = "Static Mesh", meta = (DisplayName = "Refine Proxy Static Meshes After a Timeout", EditCondition = "bEnableProxyStaticMesh"))
		bool bEnableProxyStaticMeshRefinementByTimer;

		// If the option to automatically refine the proxy mesh via a timer has been selected, this controls the timeout in seconds.
		UPROPERTY(GlobalConfig, EditAnywhere, AdvancedDisplay, Category = "Static Mesh", meta = (DisplayName = "Proxy Mesh Auto Refine Timeout Seconds", EditCondition = "bEnableProxyStaticMesh && bEnableProxyStaticMeshRefinementByTimer"))
		float ProxyMeshAutoRefineTimeoutSeconds;

		// Automatically refine proxy meshes to UStaticMesh before the map is saved
		UPROPERTY(GlobalConfig, EditAnywhere, AdvancedDisplay, Category = "Static Mesh", meta = (DisplayName = "Refine Proxy Static Meshes When Saving a Map", EditCondition = "bEnableProxyStaticMesh"))
		bool bEnableProxyStaticMeshRefinementOnPreSaveWorld;

		// Automatically refine proxy meshes to UStaticMesh before starting a play in editor session
		UPROPERTY(GlobalConfig, EditAnywhere, AdvancedDisplay, Category = "Static Mesh", meta = (DisplayName = "Refine Proxy Static Meshes On PIE", EditCondition = "bEnableProxyStaticMesh"))
		bool bEnableProxyStaticMeshRefinementOnPreBeginPIE;

		//-------------------------------------------------------------------------------------------------------------
		// Legacy
		//-------------------------------------------------------------------------------------------------------------
		// Whether to enable backward compatibility
		UPROPERTY(GlobalConfig, EditAnywhere, Category = "Legacy", Meta = (DisplayName = "Enable backward compatibility with Version 1"))
		bool bEnableBackwardCompatibility;

		// Automatically rebuild legacy HAC
		UPROPERTY(GlobalConfig, EditAnywhere, Category = "Legacy", meta = (DisplayName = "Automatically rebuild legacy Houdini Asset Components", EditCondition = "bEnableBackwardCompatibility"))
		bool bAutomaticLegacyHDARebuild;

		//-------------------------------------------------------------------------------------------------------------
		// Custom Houdini Location
		//-------------------------------------------------------------------------------------------------------------
		// Whether to use custom Houdini location.
		UPROPERTY(GlobalConfig, EditAnywhere, Category = HoudiniLocation, Meta = (DisplayName = "Use custom Houdini location (requires restart)"))
		bool bUseCustomHoudiniLocation;

		// Custom Houdini location (where HAPI library is located).
		UPROPERTY(GlobalConfig, EditAnywhere, Category = HoudiniLocation, Meta = (DisplayName = "Custom Houdini location"))
		FDirectoryPath CustomHoudiniLocation;

		//-------------------------------------------------------------------------------------------------------------
		// HAPI_Initialize
		//-------------------------------------------------------------------------------------------------------------
		// Evaluation thread stack size in bytes.  -1 for default 
		UPROPERTY(GlobalConfig, EditAnywhere, Category = HoudiniEngineInitialization)
		int32 CookingThreadStackSize;

		// List of paths to Houdini-compatible .env files (; separated on Windows, : otherwise)
		UPROPERTY(GlobalConfig, EditAnywhere, Category = HoudiniEngineInitialization)
		FString HoudiniEnvironmentFiles;

		// Path to find other OTL/HDA files
		UPROPERTY(GlobalConfig, EditAnywhere, Category = HoudiniEngineInitialization)
		FString OtlSearchPath;

		// Sets HOUDINI_DSO_PATH
		UPROPERTY(GlobalConfig, EditAnywhere, Category = HoudiniEngineInitialization)
		FString DsoSearchPath;

		// Sets HOUDINI_IMAGE_DSO_PATH
		UPROPERTY(GlobalConfig, EditAnywhere, Category = HoudiniEngineInitialization)
		FString ImageDsoSearchPath;

		// Sets HOUDINI_AUDIO_DSO_PATH
		UPROPERTY(GlobalConfig, EditAnywhere, Category = HoudiniEngineInitialization)
		FString AudioDsoSearchPath;

		//-------------------------------------------------------------------------------------------------------------
		// PDG Commandlet import
		//-------------------------------------------------------------------------------------------------------------
		// Is the PDG commandlet enabled? 
		UPROPERTY(GlobalConfig, EditAnywhere, Category = "PDG Settings", Meta=(DisplayName="Async Importer Enabled"))
		bool bPDGAsyncCommandletImportEnabled;
};