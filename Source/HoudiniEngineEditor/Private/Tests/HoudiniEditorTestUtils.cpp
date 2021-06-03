

#if WITH_DEV_AUTOMATION_TESTS
#include "HoudiniEditorTestUtils.h"
#include "IAssetViewport.h"
#include "Slate/SceneViewport.h"
#include "Widgets/SViewport.h"
#include "FileHelpers.h"
#include "HoudiniAsset.h"
#include "HoudiniAssetComponent.h"
#include "HoudiniEngineEditor.h"
#include "HoudiniEngineEditorUtils.h"
#include "LevelEditor.h"
#include "AssetRegistryModule.h"
#include "Core/Public/HAL/FileManager.h"
#include "Core/Public/HAL/PlatformFilemanager.h"
#include "Editor/EditorPerformanceSettings.h"
#include "Engine/Selection.h"
#include "Interfaces/IMainFrameModule.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"

const FVector2D FHoudiniEditorTestUtils::GDefaultEditorSize = FVector2D(1280, 720);

void FHoudiniEditorTestUtils::InitializeTests(FAutomationTestBase* Test)
{
	FHoudiniEditorTestUtils::GetMainFrameWindow()->Resize(GDefaultEditorSize);
	FEditorFileUtils::LoadMap(TEXT("/Game/TestLevel"), false, false);
}

UObject* FHoudiniEditorTestUtils::FindAssetUObject(FAutomationTestBase* Test, const FName AssetUObjectPath)
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>( "AssetRegistry" );
	TArray<FAssetData> AssetData;
	AssetRegistryModule.Get().GetAssetsByPackageName( AssetUObjectPath, AssetData );
	if( AssetData.Num() > 0 )
	{
		return AssetData[ 0 ].GetAsset();
	}

	Test->AddError(FString::Printf(TEXT("Could not find UObject: %s"), *AssetUObjectPath.ToString()));
	return nullptr;
}

UHoudiniAssetComponent* FHoudiniEditorTestUtils::InstantiateAsset(FAutomationTestBase* Test,
	const FName AssetUObjectPath, TFunction<void(UHoudiniAssetComponent*, const bool)> OnFinishInstantiate, const bool ErrorOnFail)
{
	SetUseLessCPUInTheBackground();
	
	UHoudiniAsset * HoudiniAsset = Cast<UHoudiniAsset>(FindAssetUObject(Test, AssetUObjectPath));

	if (!HoudiniAsset)
	{
		Test->AddError(FString::Printf(TEXT("Could not find UObject: %s"), *AssetUObjectPath.ToString()));
		return nullptr;
	}
	

	FHoudiniEngineEditorUtils::InstantiateHoudiniAssetAt(HoudiniAsset, FTransform::Identity);

	USelection* SelectedActors = GEditor->GetSelectedActors();
	TArray<AActor*> Actors;
	TArray<ULevel*> UniqueLevels;
	for (FSelectionIterator Iter(*SelectedActors); Iter; ++Iter)
	{
		AActor* Actor = Cast<AActor>(*Iter);
		if (Actor)
		{
			Actors.Add(Actor);
		}
	}
	Test->TestEqual(TEXT("Only one actor should be selected"), Actors.Num(), 1);

	AActor* TheActor = Actors[0];
	UHoudiniAssetComponent * HoudiniComponent = TheActor->FindComponentByClass<UHoudiniAssetComponent>();


	// Need to allocate on heap otherwise it will be garbage collected.
	bool * FinishedCook = new bool(false);
	bool * CookSuccessful = new bool(false);
	FDelegateHandle * PostCookDelegateHandle = new FDelegateHandle();

	auto OnPostCookLambda = [=](UHoudiniAssetComponent* HAC, bool IsSuccess)
	{
		if (FinishedCook != nullptr && CookSuccessful != nullptr)
		{
			*FinishedCook = true;
			*CookSuccessful = IsSuccess;
			if (PostCookDelegateHandle != nullptr)
				HoudiniComponent->GetOnPostCookDelegate().Remove(*PostCookDelegateHandle);
		}

	};
	
	*PostCookDelegateHandle = HoudiniComponent->GetOnPostCookDelegate().AddLambda(OnPostCookLambda);

	Test->AddCommand(new FFunctionLatentCommand([=]()
	{
		const bool FinishedCookResult = *FinishedCook;
		const bool CookSuccessfulResult = *CookSuccessful;

		if (FinishedCookResult == true && HoudiniComponent->GetAssetState() == EHoudiniAssetState::None)
		{
			if (ErrorOnFail && CookSuccessfulResult == false)
			{
				Test->AddError(FString::Printf(TEXT("Cook was unsuccessful: %s"), *AssetUObjectPath.ToString()));
			}

			ForceRefreshViewport();

			OnFinishInstantiate(HoudiniComponent, CookSuccessfulResult);
			delete FinishedCook;
			delete CookSuccessful;
			delete PostCookDelegateHandle;

			return true;
		}
	 
		return false;
	}
	));
	
	return HoudiniComponent;
}

void FHoudiniEditorTestUtils::SetUseLessCPUInTheBackground()
{
	// Equivalent of setting Edit > Editor Preferences > General > Performance > "Use less CPU when in background" is OFF
	// This ensures that objects are rendered even in the background
	UEditorPerformanceSettings* Settings = GetMutableDefault<UEditorPerformanceSettings>();
	Settings->bThrottleCPUWhenNotForeground = false;
	Settings->bMonitorEditorPerformance = false;
	Settings->PostEditChange();
}

TSharedPtr<SWindow> FHoudiniEditorTestUtils::GetMainFrameWindow()
{
			
	if (FModuleManager::Get().IsModuleLoaded("MainFrame"))
	{
		// Check if the main frame is loaded. When using the old main frame it may not be.
		IMainFrameModule& MainFrame = FModuleManager::LoadModuleChecked<IMainFrameModule>("MainFrame");
		return MainFrame.GetParentWindow();
	}

	return nullptr;
}

TSharedPtr<SWindow> FHoudiniEditorTestUtils::GetActiveTopLevelWindow()
{
	return FSlateApplication::Get().GetActiveTopLevelWindow();
}

static bool ShouldShowProperty(const FPropertyAndParent& PropertyAndParent, bool bHaveTemplate)
{
	const FProperty& Property = PropertyAndParent.Property;

	if ( bHaveTemplate )
	{
		const UClass* PropertyOwnerClass = Property.GetOwner<const UClass>();
		const bool bDisableEditOnTemplate = PropertyOwnerClass 
			&& PropertyOwnerClass->IsNative()
			&& Property.HasAnyPropertyFlags(CPF_DisableEditOnTemplate);
		if(bDisableEditOnTemplate)
		{
			return false;
		}
	}
	return true;
}

TSharedPtr<SWindow> FHoudiniEditorTestUtils::CreateNewDetailsWindow()
{
	if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
	{
		// Check if the main frame is loaded. When using the old main frame it may not be.
		FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

		USelection* SelectedActors = GEditor->GetSelectedActors();
		TArray<UObject*> Actors;
		for (FSelectionIterator Iter(*SelectedActors); Iter; ++Iter)
		{
			AActor* Actor = Cast<AActor>(*Iter);
			if (Actor)
			{
			    Actors.Add(Actor);
			}
		}
		
		TSharedRef<SWindow> Details = PropertyEditorModule.CreateFloatingDetailsView(Actors, false);//

		return Details;
	}

	return nullptr;
}

TSharedPtr<SWindow> FHoudiniEditorTestUtils::CreateViewportWindow()
{
	if (!FModuleManager::Get().IsModuleLoaded("LevelEditor"))
	{
		return nullptr;
	}
	
	TSharedRef<SWindow> NewSlateWindow = SNew(SWindow)
	.Title( NSLOCTEXT("ViewportEditor", "WindowTitle", "Viewport Editor") )
	.ClientSize( FVector2D(400, 550) );

	// If the main frame exists parent the window to it
	TSharedPtr< SWindow > ParentWindow;
	if( FModuleManager::Get().IsModuleLoaded( "MainFrame" ) )
	{
		IMainFrameModule& MainFrame = FModuleManager::GetModuleChecked<IMainFrameModule>( "MainFrame" );
		ParentWindow = MainFrame.GetParentWindow();
	}

	if( ParentWindow.IsValid() )
	{
		// Parent the window to the main frame 
		FSlateApplication::Get().AddWindowAsNativeChild( NewSlateWindow, ParentWindow.ToSharedRef() );
	}
	else
	{
		FSlateApplication::Get().AddWindow( NewSlateWindow );
	}

	FLevelEditorModule& LevelEditor = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
	TSharedPtr<class IAssetViewport> Viewport = LevelEditor.GetFirstActiveViewport();

	NewSlateWindow->SetContent(
	SNew(SBorder)
	[
		Viewport->AsWidget()
	]
	);

	return NewSlateWindow;
}

void FHoudiniEditorTestUtils::TakeScreenshotEditor(FAutomationTestBase* Test, const FString ScreenshotName, const EEditorScreenshotType EditorScreenshotType, const FVector2D Size)
{
	// Wait one frame just in case for pending redraws
	Test->AddCommand(new FDelayedFunctionLatentCommand( [=]()
	{
		TSharedPtr<SWindow> ScreenshotWindow;

		bool DestroyWindowOnEnd = false;

		switch (EditorScreenshotType)
		{
			case EEditorScreenshotType::ENTIRE_EDITOR:
				ScreenshotWindow = GetMainFrameWindow();
				break;
			case EEditorScreenshotType::ACTIVE_WINDOW:
				ScreenshotWindow = GetActiveTopLevelWindow();
				break;
			case EEditorScreenshotType::DETAILS_WINDOW:
				ScreenshotWindow = CreateNewDetailsWindow();
				DestroyWindowOnEnd = true;
				break;
			case EEditorScreenshotType::VIEWPORT:
				ScreenshotWindow = CreateViewportWindow();
				DestroyWindowOnEnd = true;
				break;
			default:
				break;
		}

		if (!ScreenshotWindow)
		{
			return;
		}

		WindowScreenshotParameters ScreenshotParameters;
		ScreenshotParameters.ScreenshotName = ScreenshotName;
		ScreenshotParameters.CurrentWindow = ScreenshotWindow;
		
		// Creates a file in Engine\Saved\Automation\Tmp
		TSharedRef<SWidget> WidgetToFind = ScreenshotWindow.ToSharedRef();

		bool ScreenshotSuccessful;

		TArray<FColor> OutImageData;
		FIntVector OutImageSize;

		bool bRenderOffScreen = FParse::Param(FCommandLine::Get(), TEXT("RenderOffScreen"));
		
		if (!bRenderOffScreen)
		{
			// Take a screenshot like a normal person
			// Note that this sizing method is slightly different than the offscreen one, so DO NOT copy the result to SVN
			ScreenshotWindow->Resize(Size);
			ScreenshotSuccessful = FSlateApplication::Get().TakeScreenshot(WidgetToFind, OutImageData, OutImageSize);
		}
		else
		{
			// Rendering offscreen results in a slightly different render pipeline.
			// Resizing as usual doesn't seem to work unless we do it in this very specific way
			// Mostly copied from  FSlateApplication::Get().TakeScreenshot(WindowRef, OutImageData, OutImageSize) , but forces size
			FWidgetPath WidgetPath;
			FSlateApplication::Get().GeneratePathToWidgetChecked(WidgetToFind, WidgetPath);

			FArrangedWidget ArrangedWidget = WidgetPath.FindArrangedWidget(WidgetToFind).Get(FArrangedWidget::GetNullWidget());
			FVector2D Position = ArrangedWidget.Geometry.AbsolutePosition;
			FVector2D WindowPosition = ScreenshotWindow->GetPositionInScreen();

			FIntRect ScreenshotRect = FIntRect(0, 0, (int32)Size.X, (int32)Size.Y);

			ScreenshotRect.Min.X += ( Position.X - WindowPosition.X );
			ScreenshotRect.Min.Y += ( Position.Y - WindowPosition.Y );
			ScreenshotRect.Max.X += ( Position.X - WindowPosition.X );
			ScreenshotRect.Max.Y += ( Position.Y - WindowPosition.Y );

			FSlateApplication::Get().GetRenderer()->RequestResize(ScreenshotWindow, Size.X, Size.Y);
			ScreenshotWindow->Resize(Size);
			FSlateApplication::Get().ForceRedrawWindow(ScreenshotWindow.ToSharedRef());

			FSlateApplication::Get().GetRenderer()->PrepareToTakeScreenshot(ScreenshotRect, &OutImageData, ScreenshotWindow.Get());
			FSlateApplication::Get().ForceRedrawWindow(ScreenshotWindow.ToSharedRef());
			ScreenshotSuccessful = (ScreenshotRect.Size().X != 0 && ScreenshotRect.Size().Y != 0 && OutImageData.Num() >= ScreenshotRect.Size().X * ScreenshotRect.Size().Y);
			OutImageSize.X = ScreenshotRect.Size().X;
			OutImageSize.Y = ScreenshotRect.Size().Y;
		}
		
		if (!ScreenshotSuccessful)
		{
			Test->AddError("Taking screenshot not successful!");
			return;
		}

		FAutomationScreenshotData Data;
		Data.Width = OutImageSize.X;
		Data.Height = OutImageSize.Y;
		Data.ScreenshotName = ScreenshotName;
		
		FAutomationTestFramework::Get().OnScreenshotCaptured().ExecuteIfBound(OutImageData, Data);

		WaitForScreenshotAndCopy(Test, ScreenshotName, [=] (FAutomationTestBase* AutomationTest, FString BaseName)
		{
			CopyScreenshotToTestFolder(AutomationTest, BaseName);

			if (DestroyWindowOnEnd)
			{
				ScreenshotWindow->RequestDestroyWindow();
			}
		});

	}, 0.1f));
}

void FHoudiniEditorTestUtils::TakeScreenshotViewport(FAutomationTestBase* Test, const FString ScreenshotName)
{
	Test->AddCommand(new FDelayedFunctionLatentCommand([=]()
	{
		const FString BaseName = ScreenshotName;
		const FString ScreenshotPath = GetUnrealTestDirectory() + BaseName;
		FScreenshotRequest::RequestScreenshot(ScreenshotPath, false, false);
		ForceRefreshViewport();
		
		WaitForScreenshotAndCopy(Test, BaseName, CopyScreenshotToTestFolder);
	}, 0.1f));
}

void FHoudiniEditorTestUtils::WaitForScreenshotAndCopy(FAutomationTestBase* Test, FString BaseName, TFunction<void(FAutomationTestBase*, FString)> OnScreenshotGenerated)
{
	const FString TestDirectory = GetUnrealTestDirectory();
	const FString FileName = TestDirectory + BaseName;
	
	// Wait for screenshot to finish generating, and then copy to $RT/hapi/unreal/
	Test->AddCommand(new FFunctionLatentCommand([=]()
	{
		IPlatformFile& FileManager = FPlatformFileManager::Get().GetPlatformFile();
		if (FileManager.FileExists(*FileName))
		{
			OnScreenshotGenerated(Test, BaseName);
			return true;
		}
		else
		{
			ForceRefreshViewport();
			return false;
		}
	}));
}

void FHoudiniEditorTestUtils::CopyScreenshotToTestFolder(FAutomationTestBase* Test, FString BaseName)
{
	const FString TestDirectory = GetUnrealTestDirectory();
	const FString FileName = TestDirectory + BaseName;
	FString DestFileName = GetTestDirectory();
	if (!DestFileName.IsEmpty())
	{
		DestFileName += FormatScreenshotOutputName(BaseName);
	}
	
	IPlatformFile& FileManager = FPlatformFileManager::Get().GetPlatformFile();

	// Copy output file to our test directory, if it exists.
	if (!DestFileName.IsEmpty())
	{
		UE_LOG(LogTemp, Verbose, TEXT("Copied file to: %s"), *DestFileName);
		FileManager.CopyFile(*DestFileName, *FileName);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Unable to copy file!"));
	}

	Test->AddCommand(new FFunctionLatentCommand([=]()
	{
		IPlatformFile& CopyFileManager = FPlatformFileManager::Get().GetPlatformFile();
		if (CopyFileManager.FileExists(*FileName))
		{
			return true;
		}
		else
		{
			return false;
		}
	}));
	
}

FString FHoudiniEditorTestUtils::GetTestDirectory()
{
	return FPlatformMisc::GetEnvironmentVariable(TEXT("TEST_OUTPUT_DIR")) + FPlatformMisc::GetDefaultPathSeparator();
}

FString FHoudiniEditorTestUtils::GetUnrealTestDirectory()
{
	//return FPaths::AutomationDir() + "/Incoming/"; // 4.25
	return FPaths::AutomationTransientDir(); // 4.26
}

FString FHoudiniEditorTestUtils::FormatScreenshotOutputName(FString BaseName)
{
	const FString Prefix = FPlatformMisc::GetEnvironmentVariable(TEXT("TEST_PREFIX"));
	return FString::Printf(TEXT("%s_%s"), *Prefix, *BaseName);
}

void FHoudiniEditorTestUtils::ForceRefreshViewport()
{
	// Force redraws viewport even if not in focus to prevent hanging
	FLevelEditorModule &PropertyEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
	if (FModuleManager::Get().IsModuleLoaded("LevelEditor"))
	{
		PropertyEditorModule.BroadcastRedrawViewports(false);
	}
}

#endif
