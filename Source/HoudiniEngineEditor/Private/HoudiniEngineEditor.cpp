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

#include "HoudiniEngineEditor.h"
#include "HoudiniEngineEditorPrivatePCH.h"
#include "HoudiniEngineEditorUtils.h"
#include "HoudiniEngineStyle.h"

#include "HoudiniEngine.h"
#include "HoudiniAsset.h"
#include "HoudiniAssetBroker.h"
#include "HoudiniAssetActorFactory.h"
#include "HoudiniAssetActor.h"
#include "HoudiniAssetComponent.h"
#include "HoudiniAssetComponentDetails.h"
#include "HoudiniInput.h"
#include "HoudiniOutput.h"
#include "HoudiniParameter.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniEngineCommands.h"
#include "HoudiniRuntimeSettingsDetails.h"
#include "HoudiniSplineComponentVisualizer.h"
#include "HoudiniHandleComponentVisualizer.h"
#include "AssetTypeActions_HoudiniAsset.h"
#include "HoudiniGeoPartObject.h"

#include "Modules/ModuleManager.h"
#include "Interfaces/IPluginManager.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/Paths.h"
#include "AssetRegistryModule.h"
#include "PropertyEditorModule.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "LevelEditor.h"
#include "Templates/SharedPointer.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/ConsoleManager.h"

#include "Editor/UnrealEdEngine.h"
#include "Editor.h"
#include "UnrealEdGlobals.h"

#define LOCTEXT_NAMESPACE HOUDINI_LOCTEXT_NAMESPACE 

IMPLEMENT_MODULE(FHoudiniEngineEditor, HoudiniEngineEditor);
DEFINE_LOG_CATEGORY(LogHoudiniEngineEditor);

FHoudiniEngineEditor *
FHoudiniEngineEditor::HoudiniEngineEditorInstance = nullptr;

FHoudiniEngineEditor &
FHoudiniEngineEditor::Get()
{
	return *HoudiniEngineEditorInstance;
}

bool
FHoudiniEngineEditor::IsInitialized()
{
	return FHoudiniEngineEditor::HoudiniEngineEditorInstance != nullptr;
}

FHoudiniEngineEditor::FHoudiniEngineEditor()
{
}

void FHoudiniEngineEditor::StartupModule()
{
	HOUDINI_LOG_MESSAGE(TEXT("Starting the Houdini Engine Editor module."));

	// Create style set.
	FHoudiniEngineStyle::Initialize();

	// Initilizes various resources used by our editor UI widgets
	InitializeWidgetResource();

	// Register asset type actions.
	RegisterAssetTypeActions();

	// Register asset brokers.
	RegisterAssetBrokers();

	// Register component visualizers.
	RegisterComponentVisualizers();

	// Register detail presenters.
	RegisterDetails();

	// Register actor factories.
	RegisterActorFactories();

	// Extends the file menu.
	ExtendMenu();

	// Extend the World Outliner Menu
	AddLevelViewportMenuExtender();

	// Adds the custom console commands
	RegisterConsoleCommands();

	// Register global undo / redo callbacks.
	//RegisterForUndo();

	//RegisterPlacementModeExtensions();

	// Register for any FEditorDelegates that we are interested in, such as
	// PreSaveWorld and PreBeginPIE, for HoudiniStaticMesh -> UStaticMesh builds
	RegisterEditorDelegates();

	// Store the instance.
	FHoudiniEngineEditor::HoudiniEngineEditorInstance = this;

	HOUDINI_LOG_MESSAGE(TEXT("Houdini Engine Editor module startup complete."));
}

void FHoudiniEngineEditor::ShutdownModule()
{
	HOUDINI_LOG_MESSAGE(TEXT("Shutting down the Houdini Engine Editor module."));

	// Deregister editor delegates
	UnregisterEditorDelegates();

	// Deregister console commands
	UnregisterConsoleCommands();

	// Remove the level viewport Menu extender
	RemoveLevelViewportMenuExtender();

	// Unregister asset type actions.
	UnregisterAssetTypeActions();

	// Unregister asset brokers.
	//UnregisterAssetBrokers();

	// Unregister detail presenters.
	UnregisterDetails();

	// Unregister our component visualizers.
	//UnregisterComponentVisualizers();

	// Unregister global undo / redo callbacks.
	//UnregisterForUndo();

	//UnregisterPlacementModeExtensions();

	// Unregister the styleset
	FHoudiniEngineStyle::Shutdown();

	HOUDINI_LOG_MESSAGE(TEXT("Houdini Engine Editor module shutdown complete."));
}

FString
FHoudiniEngineEditor::GetHoudiniEnginePluginDir()
{
	FString EnginePluginDir = FPaths::EnginePluginsDir() / TEXT("Runtime/HoudiniEngine");
	if (FPaths::DirectoryExists(EnginePluginDir))
		return EnginePluginDir;

	FString ProjectPluginDir = FPaths::ProjectPluginsDir() / TEXT("Runtime/HoudiniEngine");
	if (FPaths::DirectoryExists(ProjectPluginDir))
		return ProjectPluginDir;

	TSharedPtr<IPlugin> HoudiniPlugin = IPluginManager::Get().FindPlugin(TEXT("HoudiniEngine"));
	FString PluginBaseDir = HoudiniPlugin.IsValid() ? HoudiniPlugin->GetBaseDir() : EnginePluginDir;
	if (FPaths::DirectoryExists(PluginBaseDir))
		return PluginBaseDir;

	HOUDINI_LOG_WARNING(TEXT("Could not find the Houdini Engine plugin's directory"));

	return EnginePluginDir;
}

void
FHoudiniEngineEditor::RegisterDetails()
{
	FPropertyEditorModule & PropertyModule = FModuleManager::LoadModuleChecked< FPropertyEditorModule >("PropertyEditor");

	// Register details presenter for our component type and runtime settings.
	PropertyModule.RegisterCustomClassLayout(
		TEXT("HoudiniAssetComponent"),
		FOnGetDetailCustomizationInstance::CreateStatic(&FHoudiniAssetComponentDetails::MakeInstance));

	PropertyModule.RegisterCustomClassLayout(
		TEXT("HoudiniRuntimeSettings"),
		FOnGetDetailCustomizationInstance::CreateStatic(&FHoudiniRuntimeSettingsDetails::MakeInstance));
}

void
FHoudiniEngineEditor::UnregisterDetails()
{
	if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
	{
		FPropertyEditorModule & PropertyModule =
			FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

		PropertyModule.UnregisterCustomClassLayout(TEXT("HoudiniAssetComponent"));
		PropertyModule.UnregisterCustomClassLayout(TEXT("HoudiniRuntimeSettings"));
	}
}

void
FHoudiniEngineEditor::RegisterComponentVisualizers()
{
	if (GUnrealEd) 
	{
		// Register Houdini spline visualizer
		SplineComponentVisualizer = MakeShareable<FComponentVisualizer>(new FHoudiniSplineComponentVisualizer);
		if (SplineComponentVisualizer.IsValid()) 
		{
			GUnrealEd->RegisterComponentVisualizer(UHoudiniSplineComponent::StaticClass()->GetFName(), SplineComponentVisualizer);
			SplineComponentVisualizer->OnRegister();
		}

		// Register Houdini handle visualizer
		HandleComponentVisualizer = MakeShareable<FComponentVisualizer>(new FHoudiniHandleComponentVisualizer);
		if (HandleComponentVisualizer.IsValid())
		{
			GUnrealEd->RegisterComponentVisualizer(UHoudiniHandleComponent::StaticClass()->GetFName(), HandleComponentVisualizer);
			HandleComponentVisualizer->OnRegister();
		}
	}
}

void
FHoudiniEngineEditor::UnregisterComponentVisualizers()
{
	if (GUnrealEd) 
	{
		// Unregister Houdini spline visualizer
		if(SplineComponentVisualizer.IsValid())
			GUnrealEd->UnregisterComponentVisualizer(UHoudiniSplineComponent::StaticClass()->GetFName());

		// Unregister Houdini handle visualizer
		if (HandleComponentVisualizer.IsValid())
			GUnrealEd->UnregisterComponentVisualizer(UHoudiniHandleComponent::StaticClass()->GetFName());
	}
}

void
FHoudiniEngineEditor::RegisterAssetTypeAction(IAssetTools & AssetTools, TSharedRef< IAssetTypeActions > Action)
{
	AssetTools.RegisterAssetTypeActions(Action);
	AssetTypeActions.Add(Action);
}

void
FHoudiniEngineEditor::RegisterAssetTypeActions()
{
	// Create and register asset type actions for Houdini asset.
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked< FAssetToolsModule >("AssetTools").Get();
	RegisterAssetTypeAction(AssetTools, MakeShareable(new FAssetTypeActions_HoudiniAsset()));
}

void
FHoudiniEngineEditor::UnregisterAssetTypeActions()
{
	// Unregister asset type actions we have previously registered.
	if (FModuleManager::Get().IsModuleLoaded("AssetTools"))
	{
		IAssetTools & AssetTools = FModuleManager::GetModuleChecked< FAssetToolsModule >("AssetTools").Get();

		for (int32 Index = 0; Index < AssetTypeActions.Num(); ++Index)
			AssetTools.UnregisterAssetTypeActions(AssetTypeActions[Index].ToSharedRef());

		AssetTypeActions.Empty();
	}
}

void
FHoudiniEngineEditor::RegisterAssetBrokers()
{
	// Create and register broker for Houdini asset.
	HoudiniAssetBroker = MakeShareable(new FHoudiniAssetBroker());
	FComponentAssetBrokerage::RegisterBroker( HoudiniAssetBroker, UHoudiniAssetComponent::StaticClass(), true, true );
}

void
FHoudiniEngineEditor::UnregisterAssetBrokers()
{
	if (UObjectInitialized())
	{
		// Unregister broker.
		FComponentAssetBrokerage::UnregisterBroker( HoudiniAssetBroker );
	}
}

void
FHoudiniEngineEditor::RegisterActorFactories()
{
	if (GEditor)
	{
		UHoudiniAssetActorFactory * HoudiniAssetActorFactory =
			NewObject< UHoudiniAssetActorFactory >(GetTransientPackage(), UHoudiniAssetActorFactory::StaticClass());

		GEditor->ActorFactories.Add(HoudiniAssetActorFactory);
	}
}

void
FHoudiniEngineEditor::BindMenuCommands()
{
	HEngineCommands = MakeShareable(new FUICommandList);

	FHoudiniEngineCommands::Register();
	const FHoudiniEngineCommands& Commands = FHoudiniEngineCommands::Get();
	
	// Session 

	HEngineCommands->MapAction(
		Commands._CreateSession,
		FExecuteAction::CreateLambda([]() { return FHoudiniEngineCommands::CreateSession(); }),
		FCanExecuteAction::CreateLambda([]() { return !FHoudiniEngineCommands::IsSessionValid(); }));

	HEngineCommands->MapAction(
		Commands._ConnectSession,
		FExecuteAction::CreateLambda([]() { return FHoudiniEngineCommands::ConnectSession(); }),
		FCanExecuteAction::CreateLambda([]() { return !FHoudiniEngineCommands::IsSessionValid(); }));

	HEngineCommands->MapAction(
		Commands._StopSession,
		FExecuteAction::CreateLambda([]() { return FHoudiniEngineCommands::StopSession(); }),
		FCanExecuteAction::CreateLambda([]() { return FHoudiniEngineCommands::IsSessionValid(); }));

	HEngineCommands->MapAction(
		Commands._RestartSession,
		FExecuteAction::CreateLambda([]() { return FHoudiniEngineCommands::RestartSession(); }),
		FCanExecuteAction::CreateLambda([]() { return true; }));

	// Plugin

	HEngineCommands->MapAction(
		Commands._InstallInfo,
		FExecuteAction::CreateLambda([]() { return FHoudiniEngineCommands::ShowInstallInfo(); }),
		FCanExecuteAction::CreateLambda([]() { return false; }));

	HEngineCommands->MapAction(
		Commands._PluginSettings,
		FExecuteAction::CreateLambda([]() { return FHoudiniEngineCommands::ShowPluginSettings(); }),
		FCanExecuteAction::CreateLambda([]() { return true; }));

	// Files

	HEngineCommands->MapAction(
		Commands._OpenInHoudini,
		FExecuteAction::CreateLambda([](){ return FHoudiniEngineCommands::OpenInHoudini(); }),
		FCanExecuteAction::CreateLambda([]() { return FHoudiniEngineCommands::IsSessionValid(); }));

	HEngineCommands->MapAction(
		Commands._SaveHIPFile,
		FExecuteAction::CreateLambda([](){ return FHoudiniEngineCommands::SaveHIPFile(); }),
		FCanExecuteAction::CreateLambda([]() { return FHoudiniEngineCommands::IsSessionValid(); }));

	HEngineCommands->MapAction(
		Commands._CleanUpTempFolder,
		FExecuteAction::CreateLambda([]() { return FHoudiniEngineCommands::CleanUpTempFolder(); }),
		FCanExecuteAction::CreateLambda([]() { return true; }));

	// Help and support

	HEngineCommands->MapAction(
		Commands._ReportBug,
		FExecuteAction::CreateLambda([](){ return FHoudiniEngineCommands::ReportBug(); }),
		FCanExecuteAction::CreateLambda([]() { return true; }));

	HEngineCommands->MapAction(
		Commands._OnlineDoc,
		FExecuteAction::CreateLambda([]() { return FHoudiniEngineCommands::OnlineDocumentation(); }),
		FCanExecuteAction::CreateLambda([]() { return true; }));

	HEngineCommands->MapAction(
		Commands._OnlineForum,
		FExecuteAction::CreateLambda([]() { return FHoudiniEngineCommands::OnlineForum(); }),
		FCanExecuteAction::CreateLambda([]() { return true; }));

	// Actions

	HEngineCommands->MapAction(
		Commands._CookAll,
		FExecuteAction::CreateLambda([]() { return FHoudiniEngineCommands::RecookAllAssets(); }),
		FCanExecuteAction::CreateLambda([]() { return FHoudiniEngineCommands::IsSessionValid(); }));

	HEngineCommands->MapAction(
		Commands._CookSelected,
		FExecuteAction::CreateLambda([]() { return FHoudiniEngineCommands::RecookSelection(); }),
		FCanExecuteAction::CreateLambda([]() { return FHoudiniEngineCommands::IsSessionValid(); }));

	HEngineCommands->MapAction(
		Commands._RebuildAll,
		FExecuteAction::CreateLambda([]() { return FHoudiniEngineCommands::RebuildAllAssets(); }),
		FCanExecuteAction::CreateLambda([]() { return FHoudiniEngineCommands::IsSessionValid(); }));

	HEngineCommands->MapAction(
		Commands._RebuildSelected,
		FExecuteAction::CreateLambda([]() { return FHoudiniEngineCommands::RebuildSelection(); }),
		FCanExecuteAction::CreateLambda([]() { return FHoudiniEngineCommands::IsSessionValid(); }));

	HEngineCommands->MapAction(
		Commands._BakeAll,
		FExecuteAction::CreateLambda([](){ return FHoudiniEngineCommands::BakeAllAssets(); }),
		FCanExecuteAction::CreateLambda([](){ return true; }));

	HEngineCommands->MapAction(
		Commands._BakeSelected,
		FExecuteAction::CreateLambda([]() { return FHoudiniEngineCommands::BakeSelection(); }),
		FCanExecuteAction::CreateLambda([]() { return true; }));

	HEngineCommands->MapAction(
		Commands._RefineAll,
		FExecuteAction::CreateLambda([]() { return FHoudiniEngineCommands::RefineHoudiniProxyMeshesToStaticMeshes(false); }),
		FCanExecuteAction::CreateLambda([]() { return true; }));

	HEngineCommands->MapAction(
		Commands._RefineSelected,
		FExecuteAction::CreateLambda([]() { return FHoudiniEngineCommands::RefineHoudiniProxyMeshesToStaticMeshes(true); }),
		FCanExecuteAction::CreateLambda([]() { return true; }));

	HEngineCommands->MapAction(
		Commands._PauseAssetCooking,
		FExecuteAction::CreateLambda([](){ return FHoudiniEngineCommands::PauseAssetCooking(); }),
		FCanExecuteAction::CreateLambda([](){ return FHoudiniEngineCommands::IsSessionValid(); }),
		FIsActionChecked::CreateLambda([](){ return FHoudiniEngineCommands::IsAssetCookingPaused(); }));

	// Non menu command (used for shortcuts only)

	// Append the command to the editor module
	FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked< FLevelEditorModule >("LevelEditor");
	LevelEditorModule.GetGlobalLevelEditorActions()->Append(HEngineCommands.ToSharedRef());
}

void
FHoudiniEngineEditor::ExtendMenu()
{
	if (IsRunningCommandlet())
		return;

	// We need to add/bind the UI Commands to their functions first
	BindMenuCommands();

	MainMenuExtender = MakeShareable(new FExtender);

	// Extend File menu, we will add Houdini section.
	MainMenuExtender->AddMenuExtension(
		"FileLoadAndSave", 
		EExtensionHook::After,
		HEngineCommands,
		FMenuExtensionDelegate::CreateRaw(this, &FHoudiniEngineEditor::AddHoudiniFileMenuExtension));
		
	MainMenuExtender->AddMenuBarExtension(
		"Edit",
		EExtensionHook::After,
		HEngineCommands,
		FMenuBarExtensionDelegate::CreateRaw(this, &FHoudiniEngineEditor::AddHoudiniEditorMenu));

	// Add our menu extender
	FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
	LevelEditorModule.GetMenuExtensibilityManager()->AddExtender(MainMenuExtender);
}

void
FHoudiniEngineEditor::AddHoudiniFileMenuExtension(FMenuBuilder & MenuBuilder)
{
	MenuBuilder.BeginSection("Houdini", LOCTEXT("HoudiniLabel", "Houdini Engine"));

	// Icons used by the commands are defined in the HoudiniEngineStyle
	MenuBuilder.AddMenuEntry(FHoudiniEngineCommands::Get()._OpenInHoudini);
	MenuBuilder.AddMenuEntry(FHoudiniEngineCommands::Get()._SaveHIPFile);
	//MenuBuilder.AddMenuEntry(FHoudiniEngineCommands::Get()._ReportBug);
	MenuBuilder.AddMenuEntry(FHoudiniEngineCommands::Get()._CleanUpTempFolder);
	//MenuBuilder.AddMenuEntry(FHoudiniEngineCommands::Get()._BakeAll);
	//MenuBuilder.AddMenuEntry(FHoudiniEngineCommands::Get()._PauseAssetCooking);
	//MenuBuilder.AddMenuEntry(FHoudiniEngineCommands::Get()._RestartSession);
	//MenuBuilder.AddMenuEntry(FHoudiniEngineCommands::Get()._RefineAll);

	MenuBuilder.EndSection();
}

void
FHoudiniEngineEditor::AddHoudiniEditorMenu(FMenuBarBuilder& MenuBarBuilder)
{
	// View
	MenuBarBuilder.AddPullDownMenu(
		LOCTEXT("HoudiniLabel", "Houdini Engine"),
		LOCTEXT("HoudiniMenu_ToolTip", "Open the Houdini Engine menu"),
		FNewMenuDelegate::CreateRaw(this, &FHoudiniEngineEditor::AddHoudiniMainMenuExtension),
		"View");
}

void
FHoudiniEngineEditor::AddHoudiniMainMenuExtension(FMenuBuilder & MenuBuilder)
{
	/*
	MenuBuilder.BeginSection("Houdini", LOCTEXT("HoudiniLabel", "Houdini Engine"));
	// Icons used by the commands are defined in the HoudiniEngineStyle
	MenuBuilder.AddMenuEntry(FHoudiniEngineCommands::Get()._OpenInHoudini);
	MenuBuilder.AddMenuEntry(FHoudiniEngineCommands::Get()._SaveHIPFile);
	MenuBuilder.AddMenuEntry(FHoudiniEngineCommands::Get()._ReportBug);
	MenuBuilder.AddMenuEntry(FHoudiniEngineCommands::Get()._CleanUpTempFolder);
	MenuBuilder.AddMenuEntry(FHoudiniEngineCommands::Get()._BakeAll);
	MenuBuilder.AddMenuEntry(FHoudiniEngineCommands::Get()._PauseAssetCooking);
	MenuBuilder.AddMenuEntry(FHoudiniEngineCommands::Get()._RestartSession);
	MenuBuilder.AddMenuEntry(FHoudiniEngineCommands::Get()._RefineAll);
	MenuBuilder.EndSection();
	*/

	MenuBuilder.BeginSection("Session", LOCTEXT("SessionLabel", "Session"));
	MenuBuilder.AddMenuEntry(FHoudiniEngineCommands::Get()._CreateSession);
	MenuBuilder.AddMenuEntry(FHoudiniEngineCommands::Get()._ConnectSession);
	MenuBuilder.AddMenuEntry(FHoudiniEngineCommands::Get()._StopSession);
	MenuBuilder.AddMenuEntry(FHoudiniEngineCommands::Get()._RestartSession);
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("Plugin", LOCTEXT("PluginLabel", "Plugin"));
	MenuBuilder.AddMenuEntry(FHoudiniEngineCommands::Get()._InstallInfo);
	MenuBuilder.AddMenuEntry(FHoudiniEngineCommands::Get()._PluginSettings);
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("File", LOCTEXT("FileLabel", "File"));
	MenuBuilder.AddMenuEntry(FHoudiniEngineCommands::Get()._OpenInHoudini);
	MenuBuilder.AddMenuEntry(FHoudiniEngineCommands::Get()._SaveHIPFile);
	MenuBuilder.AddMenuEntry(FHoudiniEngineCommands::Get()._CleanUpTempFolder);
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("Help", LOCTEXT("HelpLabel", "Help And Support"));
	MenuBuilder.AddMenuEntry(FHoudiniEngineCommands::Get()._OnlineDoc);
	MenuBuilder.AddMenuEntry(FHoudiniEngineCommands::Get()._OnlineForum);
	MenuBuilder.AddMenuEntry(FHoudiniEngineCommands::Get()._ReportBug);
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("Actions", LOCTEXT("ActionsLabel", "Actions"));
	MenuBuilder.AddMenuEntry(FHoudiniEngineCommands::Get()._CookAll);
	MenuBuilder.AddMenuEntry(FHoudiniEngineCommands::Get()._CookSelected);
	MenuBuilder.AddMenuEntry(FHoudiniEngineCommands::Get()._RebuildAll);
	MenuBuilder.AddMenuEntry(FHoudiniEngineCommands::Get()._RebuildSelected);
	MenuBuilder.AddMenuEntry(FHoudiniEngineCommands::Get()._BakeAll);
	MenuBuilder.AddMenuEntry(FHoudiniEngineCommands::Get()._BakeSelected);
	MenuBuilder.AddMenuEntry(FHoudiniEngineCommands::Get()._RefineAll);
	MenuBuilder.AddMenuEntry(FHoudiniEngineCommands::Get()._RefineSelected);
	MenuBuilder.AddMenuEntry(FHoudiniEngineCommands::Get()._PauseAssetCooking);

	
	MenuBuilder.EndSection();
}

void
FHoudiniEngineEditor::RegisterForUndo()
{
	/*
	if (GUnrealEd)
		GUnrealEd->RegisterForUndo(this);
	*/
}

void
FHoudiniEngineEditor::UnregisterForUndo()
{
	/*
	if (GUnrealEd)
		GUnrealEd->UnregisterForUndo(this);
	*/
}

void
FHoudiniEngineEditor::RegisterPlacementModeExtensions()
{
	// Load custom houdini tools
	/*
	const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault< UHoudiniRuntimeSettings >();
	check(HoudiniRuntimeSettings);

	if (HoudiniRuntimeSettings->bHidePlacementModeHoudiniTools)
		return;

	FPlacementCategoryInfo Info(
		LOCTEXT("HoudiniCategoryName", "Houdini Engine"),
		"HoudiniEngine",
		TEXT("PMHoudiniEngine"),
		25
	);
	Info.CustomGenerator = []() -> TSharedRef<SWidget> { return SNew(SHoudiniToolPalette); };

	IPlacementModeModule::Get().RegisterPlacementCategory(Info);
	*/
}

void
FHoudiniEngineEditor::UnregisterPlacementModeExtensions()
{
	/*
	if (IPlacementModeModule::IsAvailable())
	{
		IPlacementModeModule::Get().UnregisterPlacementCategory("HoudiniEngine");
	}

	HoudiniTools.Empty();
	*/
}

void 
FHoudiniEngineEditor::InitializeWidgetResource()
{
	// Choice labels for all the input types
	//TArray<TSharedPtr<FString>> InputTypeChoiceLabels;
	InputTypeChoiceLabels.Reset();
	InputTypeChoiceLabels.Add(MakeShareable(new FString(UHoudiniInput::InputTypeToString(EHoudiniInputType::Geometry))));
	InputTypeChoiceLabels.Add(MakeShareable(new FString(UHoudiniInput::InputTypeToString(EHoudiniInputType::Curve))));
	InputTypeChoiceLabels.Add(MakeShareable(new FString(UHoudiniInput::InputTypeToString(EHoudiniInputType::Asset))));
	InputTypeChoiceLabels.Add(MakeShareable(new FString(UHoudiniInput::InputTypeToString(EHoudiniInputType::Landscape))));
	InputTypeChoiceLabels.Add(MakeShareable(new FString(UHoudiniInput::InputTypeToString(EHoudiniInputType::World))));
	InputTypeChoiceLabels.Add(MakeShareable(new FString(UHoudiniInput::InputTypeToString(EHoudiniInputType::Skeletal))));

	// Choice labels for all Houdini curve types
	HoudiniCurveTypeChoiceLabels.Reset();
	HoudiniCurveTypeChoiceLabels.Add(MakeShareable(new FString(FHoudiniEngineEditorUtils::HoudiniCurveTypeToString(EHoudiniCurveType::Linear))));
	HoudiniCurveTypeChoiceLabels.Add(MakeShareable(new FString(FHoudiniEngineEditorUtils::HoudiniCurveTypeToString(EHoudiniCurveType::Bezier))));
	HoudiniCurveTypeChoiceLabels.Add(MakeShareable(new FString(FHoudiniEngineEditorUtils::HoudiniCurveTypeToString(EHoudiniCurveType::Nurbs))));

	// Choice labels for all Houdini curve methods
	HoudiniCurveMethodChoiceLabels.Reset();
	HoudiniCurveMethodChoiceLabels.Add(MakeShareable(new FString(FHoudiniEngineEditorUtils::HoudiniCurveMethodToString(EHoudiniCurveMethod::CVs))));
	HoudiniCurveMethodChoiceLabels.Add(MakeShareable(new FString(FHoudiniEngineEditorUtils::HoudiniCurveMethodToString(EHoudiniCurveMethod::Breakpoints))));
	HoudiniCurveMethodChoiceLabels.Add(MakeShareable(new FString(FHoudiniEngineEditorUtils::HoudiniCurveMethodToString(EHoudiniCurveMethod::Freehand))));

	// Choice labels for all Houdini ramp parameter interpolation methods
	HoudiniParameterRampInterpolationLabels.Reset();
	HoudiniParameterRampInterpolationLabels.Add(MakeShareable(new FString(UHoudiniParameter::GetStringFromHoudiniInterpMethod(EHoudiniRampInterpolationType::CONSTANT))));
	HoudiniParameterRampInterpolationLabels.Add(MakeShareable(new FString(UHoudiniParameter::GetStringFromHoudiniInterpMethod(EHoudiniRampInterpolationType::LINEAR))));
	HoudiniParameterRampInterpolationLabels.Add(MakeShareable(new FString(UHoudiniParameter::GetStringFromHoudiniInterpMethod(EHoudiniRampInterpolationType::CATMULL_ROM))));
	HoudiniParameterRampInterpolationLabels.Add(MakeShareable(new FString(UHoudiniParameter::GetStringFromHoudiniInterpMethod(EHoudiniRampInterpolationType::MONOTONE_CUBIC))));
	HoudiniParameterRampInterpolationLabels.Add(MakeShareable(new FString(UHoudiniParameter::GetStringFromHoudiniInterpMethod(EHoudiniRampInterpolationType::BEZIER))));
	HoudiniParameterRampInterpolationLabels.Add(MakeShareable(new FString(UHoudiniParameter::GetStringFromHoudiniInterpMethod(EHoudiniRampInterpolationType::BSPLINE))));
	HoudiniParameterRampInterpolationLabels.Add(MakeShareable(new FString(UHoudiniParameter::GetStringFromHoudiniInterpMethod(EHoudiniRampInterpolationType::HERMITE))));

	// Choice labels for all Houdini curve output export types
	HoudiniCurveOutputExportTypeLabels.Reset();
	HoudiniCurveOutputExportTypeLabels.Add(MakeShareable(new FString(TEXT("Unreal Spline"))));
	HoudiniCurveOutputExportTypeLabels.Add(MakeShareable(new FString(TEXT("Houdini Spline"))));

	// Choice labels for all Unreal curve output curve types 
	//(for temporary, we need to figure out a way to access the output curve's info later)
	UnrealCurveOutputCurveTypeLabels.Reset();
	UnrealCurveOutputCurveTypeLabels.Add(MakeShareable(new FString(TEXT("Linear"))));
	UnrealCurveOutputCurveTypeLabels.Add(MakeShareable(new FString(TEXT("Curve"))));

	// Option labels for all landscape outputs bake options
	HoudiniLandscapeOutputBakeOptionLabels.Reset();
	HoudiniLandscapeOutputBakeOptionLabels.Add(MakeShareable(new FString(TEXT("To Current Level"))));
	HoudiniLandscapeOutputBakeOptionLabels.Add(MakeShareable(new FString(TEXT("To Image"))));
	HoudiniLandscapeOutputBakeOptionLabels.Add(MakeShareable(new FString(TEXT("To New World"))));

	// Option labels for Houdini Engine bake options
	HoudiniEngineBakeTypeOptionLabels.Reset();
	HoudiniEngineBakeTypeOptionLabels.Add(MakeShareable(new FString(FHoudiniEngineEditor::GetStringFromHoudiniEngineBakeOption(EHoudiniEngineBakeOption::ToActor))));
	HoudiniEngineBakeTypeOptionLabels.Add(MakeShareable(new FString(FHoudiniEngineEditor::GetStringFromHoudiniEngineBakeOption(EHoudiniEngineBakeOption::ToBlueprint))));
	HoudiniEngineBakeTypeOptionLabels.Add(MakeShareable(new FString(FHoudiniEngineEditor::GetStringFromHoudiniEngineBakeOption(EHoudiniEngineBakeOption::ToFoliage))));
	HoudiniEngineBakeTypeOptionLabels.Add(MakeShareable(new FString(FHoudiniEngineEditor::GetStringFromHoudiniEngineBakeOption(EHoudiniEngineBakeOption::ToWorldOutliner))));

	// Houdini Logo Brush
	FString Icon128FilePath = GetHoudiniEnginePluginDir() / TEXT("Resources/Icon128.png");
	if (FPlatformFileManager::Get().GetPlatformFile().FileExists(*Icon128FilePath))
	{
		const FName BrushName(*Icon128FilePath);
		const FIntPoint Size = FSlateApplication::Get().GetRenderer()->GenerateDynamicImageResource(BrushName);
		if (Size.X > 0 && Size.Y > 0)
		{
			static const int32 ProgressIconSize = 32;
			HoudiniLogoBrush = MakeShareable(new FSlateDynamicImageBrush(
				BrushName, FVector2D(ProgressIconSize, ProgressIconSize)));
		}
	}

	// Houdini Engine UI Icon Brush
	FString HoudiniEngineUIIconFilePath = GetHoudiniEnginePluginDir() / TEXT("Resources/hengine_banner_d.png");
	if (FPlatformFileManager::Get().GetPlatformFile().FileExists(*HoudiniEngineUIIconFilePath))
	{
		const FName BrushName(*HoudiniEngineUIIconFilePath);
		const FIntPoint Size = FSlateApplication::Get().GetRenderer()->GenerateDynamicImageResource(BrushName);
		if (Size.X > 0 && Size.Y > 0)
		{
			static const int32 ProgressIconSize = 32;
			HoudiniEngineUIIconBrush = MakeShareable(new FSlateDynamicImageBrush(
				BrushName, FVector2D(ProgressIconSize, ProgressIconSize)));
		}
	}

	// Houdini Engine UI Rebuild Icon Brush
	FString HoudiniEngineUIRebuildIconFilePath = GetHoudiniEnginePluginDir() / TEXT("Resources/hengine_reload_icon.png");
	if (FPlatformFileManager::Get().GetPlatformFile().FileExists(*HoudiniEngineUIRebuildIconFilePath))
	{
		const FName BrushName(*HoudiniEngineUIRebuildIconFilePath);
		const FIntPoint Size = FSlateApplication::Get().GetRenderer()->GenerateDynamicImageResource(BrushName);
		if (Size.X > 0 && Size.Y > 0)
		{
			static const int32 ProgressIconSize = 32;
			HoudiniEngineUIRebuildIconBrush = MakeShareable(new FSlateDynamicImageBrush(
				BrushName, FVector2D(ProgressIconSize, ProgressIconSize)));
		}
	}

	// Houdini Engine UI Recook Icon Brush
	FString HoudiniEngineUIRecookIconFilePath = GetHoudiniEnginePluginDir() / TEXT("Resources/hengine_recook_icon.png");
	if (FPlatformFileManager::Get().GetPlatformFile().FileExists(*HoudiniEngineUIRecookIconFilePath))
	{
		const FName BrushName(*HoudiniEngineUIRecookIconFilePath);
		const FIntPoint Size = FSlateApplication::Get().GetRenderer()->GenerateDynamicImageResource(BrushName);
		if (Size.X > 0 && Size.Y > 0)
		{
			static const int32 ProgressIconSize = 32;
			HoudiniEngineUIRecookIconBrush = MakeShareable(new FSlateDynamicImageBrush(
				BrushName, FVector2D(ProgressIconSize, ProgressIconSize)));
		}
	}

	// Houdini Engine UI Reset Parameters Icon Brush
	FString HoudiniEngineUIResetParametersIconFilePath = GetHoudiniEnginePluginDir() / TEXT("Resources/hengine_resetparameters_icon.png");
	if (FPlatformFileManager::Get().GetPlatformFile().FileExists(*HoudiniEngineUIResetParametersIconFilePath))
	{
		const FName BrushName(*HoudiniEngineUIResetParametersIconFilePath);
		const FIntPoint Size = FSlateApplication::Get().GetRenderer()->GenerateDynamicImageResource(BrushName);
		if (Size.X > 0 && Size.Y > 0)
		{
			static const int32 ProgressIconSize = 32;
			HoudiniEngineUIResetParametersIconBrush = MakeShareable(new FSlateDynamicImageBrush(
				BrushName, FVector2D(ProgressIconSize, ProgressIconSize)));
		}
	}
}

void
FHoudiniEngineEditor::AddLevelViewportMenuExtender()
{
	FLevelEditorModule& LevelEditorModule = FModuleManager::Get().LoadModuleChecked<FLevelEditorModule>("LevelEditor");
	auto& MenuExtenders = LevelEditorModule.GetAllLevelViewportContextMenuExtenders();

	MenuExtenders.Add(FLevelEditorModule::FLevelViewportMenuExtender_SelectedActors::CreateRaw(this, &FHoudiniEngineEditor::GetLevelViewportContextMenuExtender));
	LevelViewportExtenderHandle = MenuExtenders.Last().GetHandle();
}

void
FHoudiniEngineEditor::RemoveLevelViewportMenuExtender()
{
	if (LevelViewportExtenderHandle.IsValid())
	{
		FLevelEditorModule* LevelEditorModule = FModuleManager::Get().GetModulePtr<FLevelEditorModule>("LevelEditor");
		if (LevelEditorModule)
		{
			typedef FLevelEditorModule::FLevelViewportMenuExtender_SelectedActors DelegateType;
			LevelEditorModule->GetAllLevelViewportContextMenuExtenders().RemoveAll(
				[=](const DelegateType& In) { return In.GetHandle() == LevelViewportExtenderHandle; });
		}
	}
}

TSharedRef<FExtender>
FHoudiniEngineEditor::GetLevelViewportContextMenuExtender(const TSharedRef<FUICommandList> CommandList, const TArray<AActor*> InActors)
{
	TSharedRef<FExtender> Extender = MakeShareable(new FExtender);

	// Build an array of the HoudiniAssets corresponding to the selected actors
	TArray< TWeakObjectPtr< UHoudiniAsset > > HoudiniAssets;
	TArray< TWeakObjectPtr< AHoudiniAssetActor > > HoudiniAssetActors;
	for (auto CurrentActor : InActors)
	{
		AHoudiniAssetActor * HoudiniAssetActor = Cast<AHoudiniAssetActor>(CurrentActor);
		if (!HoudiniAssetActor || HoudiniAssetActor->IsPendingKill())
			continue;

		HoudiniAssetActors.Add(HoudiniAssetActor);

		UHoudiniAssetComponent* HoudiniAssetComponent = HoudiniAssetActor->GetHoudiniAssetComponent();
		if (!HoudiniAssetComponent || HoudiniAssetComponent->IsPendingKill())
			continue;

		HoudiniAssets.AddUnique(HoudiniAssetComponent->GetHoudiniAsset());
	}

	if (HoudiniAssets.Num() > 0)
	{
		// Add the Asset menu extension
		if (AssetTypeActions.Num() > 0)
		{
			// Add the menu extensions via our HoudiniAssetTypeActions
			FAssetTypeActions_HoudiniAsset * HATA = static_cast<FAssetTypeActions_HoudiniAsset*>(AssetTypeActions[0].Get());
			if (HATA)
				Extender = HATA->AddLevelEditorMenuExtenders(HoudiniAssets);
		}
	}

	if (HoudiniAssetActors.Num() > 0)
	{
		// Add some actor menu extensions
		FLevelEditorModule& LevelEditor = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
		TSharedRef<FUICommandList> LevelEditorCommandBindings = LevelEditor.GetGlobalLevelEditorActions();
		Extender->AddMenuExtension(
			"ActorControl",
			EExtensionHook::After,
			LevelEditorCommandBindings,
			FMenuExtensionDelegate::CreateLambda([this, HoudiniAssetActors](FMenuBuilder& MenuBuilder)
		{
			MenuBuilder.AddMenuEntry(
				NSLOCTEXT("HoudiniAssetLevelViewportContextActions", "HoudiniActor_Recentre", "Recentre selected"),
				NSLOCTEXT("HoudiniAssetLevelViewportContextActions", "HoudiniActor_RecentreTooltip", "Recentres the selected Houdini Asset Actors pivots to their input/cooked static mesh average centre."),
				FSlateIcon(FHoudiniEngineStyle::GetStyleSetName(), "HoudiniEngine.HoudiniEngineLogo"),
				FUIAction(
					FExecuteAction::CreateLambda([]() { return FHoudiniEngineCommands::RecentreSelection(); }),
					FCanExecuteAction::CreateLambda([=] { return (HoudiniAssetActors.Num() > 0); })
				)
			);

			MenuBuilder.AddMenuEntry(
				NSLOCTEXT("HoudiniAssetLevelViewportContextActions", "HoudiniActor_Recook", "Recook selected"),
				NSLOCTEXT("HoudiniAssetLevelViewportContextActions", "HoudiniActor_RecookTooltip", "Forces a recook on the selected Houdini Asset Actors."),
				FSlateIcon(FHoudiniEngineStyle::GetStyleSetName(), "HoudiniEngine.HoudiniEngineLogo"),
				FUIAction(					
					FExecuteAction::CreateLambda([]() { return FHoudiniEngineCommands::RecookSelection(); }),
					FCanExecuteAction::CreateLambda([=] { return (HoudiniAssetActors.Num() > 0); })
				)
			);

			MenuBuilder.AddMenuEntry(
				NSLOCTEXT("HoudiniAssetLevelViewportContextActions", "HoudiniActor_Rebuild", "Rebuild selected"),
				NSLOCTEXT("HoudiniAssetLevelViewportContextActions", "HoudiniActor_RebuildTooltip", "Rebuilds selected Houdini Asset Actors in the current level."),
				FSlateIcon(FHoudiniEngineStyle::GetStyleSetName(), "HoudiniEngine.HoudiniEngineLogo"),
				FUIAction(
					FExecuteAction::CreateLambda([]() { return FHoudiniEngineCommands::RebuildSelection(); }),
					FCanExecuteAction::CreateLambda([=] { return (HoudiniAssetActors.Num() > 0); })
				)
			);

			MenuBuilder.AddMenuEntry(
				NSLOCTEXT("HoudiniAssetLevelViewportContextActions", "HoudiniActor_Refine_ProxyMeshes", "Refine Houdini Proxy Meshes"),
				NSLOCTEXT("HoudiniAssetLevelViewportContextActions", "HoudiniActor_Refine_ProxyMeshesTooltip", "Build and replace Houdini Proxy Meshes with Static Meshes."),
				FSlateIcon(FHoudiniEngineStyle::GetStyleSetName(), "HoudiniEngine.HoudiniEngineLogo"),
				FUIAction(
					FExecuteAction::CreateLambda([]() { return FHoudiniEngineCommands::RefineHoudiniProxyMeshesToStaticMeshes(true); }),
					FCanExecuteAction::CreateLambda([=] { return (HoudiniAssetActors.Num() > 0); })
				)
			);
		})
		);
	}

	return Extender;
}

void
FHoudiniEngineEditor::RegisterConsoleCommands()
{
	IConsoleManager &ConsoleManager = IConsoleManager::Get();
	const TCHAR *CommandName = TEXT("HoudiniEngine.RefineHoudiniProxyMeshesToStaticMeshes");
	IConsoleCommand *Command = ConsoleManager.RegisterConsoleCommand(
		CommandName,
		TEXT("Builds and replaces all Houdini proxy meshes with UStaticMeshes."),
		FConsoleCommandDelegate::CreateLambda([]() { FHoudiniEngineCommands::RefineHoudiniProxyMeshesToStaticMeshes(false); }));
	if (Command)
	{
		ConsoleCommands.Add(Command);
	}
	else
	{
		HOUDINI_LOG_ERROR(TEXT("Failed to register the '%s' console command."), CommandName);
	}
}

void
FHoudiniEngineEditor::UnregisterConsoleCommands()
{
	IConsoleManager &ConsoleManager = IConsoleManager::Get();
	for (IConsoleCommand *Command : ConsoleCommands)
	{
		if (Command)
		{
			ConsoleManager.UnregisterConsoleObject(Command);
		}
	}
	ConsoleCommands.Empty();
}

void
FHoudiniEngineEditor::RegisterEditorDelegates()
{
	PreSaveWorldEditorDelegateHandle = FEditorDelegates::PreSaveWorld.AddLambda([](uint32 SaveFlags, UWorld* World)
	{
		// Skip if this is a game world or an autosave, only refine meshes when the user manually saves
		if (!World->IsGameWorld() && (SaveFlags & ESaveFlags::SAVE_FromAutosave) == 0)
		{
			const bool bSelectedOnly = false;
			const bool bSilent = false;
			const bool bRefineAll = false;
			const bool bOnPreSaveWorld = true;
			UWorld * const OnPreSaveWorld = World;
			const bool bOnPreBeginPIE = false;
			FHoudiniEngineCommands::RefineHoudiniProxyMeshesToStaticMeshes(bSelectedOnly, bSilent, bRefineAll, bOnPreSaveWorld, OnPreSaveWorld, bOnPreBeginPIE);
		}

		if (!World->IsGameWorld())
		{
			UWorld * const OnPreSaveWorld = World;

			// Save all dirty temporary cook package OnPostSaveWorld
			FDelegateHandle PostSaveHandle = FEditorDelegates::PostSaveWorld.AddLambda([PostSaveHandle, OnPreSaveWorld](uint32 InSaveFlags, UWorld* InWorld, bool bInSuccess) 
			{
				if (OnPreSaveWorld && OnPreSaveWorld != InWorld)
					return;

				FHoudiniEngineEditorUtils::SaveAllHoudiniTemporaryCookData(InWorld);

				FEditorDelegates::PostSaveWorld.Remove(PostSaveHandle);
			});
		}
	});

	PreBeginPIEEditorDelegateHandle = FEditorDelegates::PreBeginPIE.AddLambda([](const bool bIsSimulating)
	{
		const bool bSelectedOnly = false;
		const bool bSilent = false;
		const bool bRefineAll = false;
		const bool bOnPreSaveWorld = false;
		UWorld * const OnPreSaveWorld = nullptr;
		const bool bOnPreBeginPIE = true;
		FHoudiniEngineCommands::RefineHoudiniProxyMeshesToStaticMeshes(bSelectedOnly, bSilent, bRefineAll, bOnPreSaveWorld, OnPreSaveWorld, bOnPreBeginPIE);
	});
}

void
FHoudiniEngineEditor::UnregisterEditorDelegates()
{
	if (PreSaveWorldEditorDelegateHandle.IsValid())
		FEditorDelegates::PreSaveWorld.Remove(PreSaveWorldEditorDelegateHandle);

	if (PreBeginPIEEditorDelegateHandle.IsValid())
		FEditorDelegates::PreSaveWorld.Remove(PreBeginPIEEditorDelegateHandle);
}

FString 
FHoudiniEngineEditor::GetStringFromHoudiniEngineBakeOption(const EHoudiniEngineBakeOption & BakeOption) 
{
	FString Str;
	switch (BakeOption) 
	{
	case EHoudiniEngineBakeOption::ToActor:
		Str = "Actor";
		break;

	case EHoudiniEngineBakeOption::ToBlueprint:
		Str = "Blueprint";
		break;

	case EHoudiniEngineBakeOption::ToFoliage:
		Str = "Foliage";
		break;

	case EHoudiniEngineBakeOption::ToWorldOutliner:
		Str = "World Outlinear";
		break;
	}

	return Str;
}

const EHoudiniEngineBakeOption 
FHoudiniEngineEditor::StringToHoudiniEngineBakeOption(const FString & InString) 
{
	if (InString == "Actor")
		return EHoudiniEngineBakeOption::ToActor;

	if (InString == "Blueprint")
		return EHoudiniEngineBakeOption::ToBlueprint;

	if (InString == "Foliage")
		return EHoudiniEngineBakeOption::ToFoliage;

	if (InString == "World Outlinear")
		return EHoudiniEngineBakeOption::ToWorldOutliner;

	return EHoudiniEngineBakeOption::ToActor;
}

#undef LOCTEXT_NAMESPACE
