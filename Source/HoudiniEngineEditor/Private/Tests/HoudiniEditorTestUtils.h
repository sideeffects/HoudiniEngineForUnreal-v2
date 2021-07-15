#pragma once

#if WITH_DEV_AUTOMATION_TESTS

#include "CoreMinimal.h"

class UHoudiniAssetComponent;

class FHoudiniEditorTestUtils
{
public:
	enum EEditorScreenshotType
	{
		ENTIRE_EDITOR,
                ACTIVE_WINDOW, // Gets the active window. Probably never use this.
                DETAILS_WINDOW,
                VIEWPORT
        };

	static void InitializeTests(FAutomationTestBase* Test);
	
	static UObject* FindAssetUObject(FAutomationTestBase* Test, const FName AssetUObjectPath);

	static UHoudiniAssetComponent* InstantiateAsset(FAutomationTestBase* Test, const FName AssetUObjectPath, TFunction<void(UHoudiniAssetComponent*, const bool)> OnFinishInstantiate, const bool ErrorOnFail = true);

	static void TakeScreenshotEditor(FAutomationTestBase* Test, const FString ScreenshotName, const EEditorScreenshotType EditorScreenshotType, const FVector2D Size);

	static void TakeScreenshotViewport(FAutomationTestBase* Test, const FString ScreenshotName);
	
	static void SetUseLessCPUInTheBackground();

	static TSharedPtr<SWindow> GetMainFrameWindow();

	static TSharedPtr<SWindow> GetActiveTopLevelWindow();

	static TSharedPtr<SWindow> CreateNewDetailsWindow();

	static TSharedPtr<SWindow> CreateViewportWindow();

	static const FVector2D GDefaultEditorSize;

private:
	static void WaitForScreenshotAndCopy(FAutomationTestBase* Test, FString BaseName, TFunction<void(FAutomationTestBase*, FString)> OnScreenshotGenerated);

	static void CopyScreenshotToTestFolder(FAutomationTestBase* Test, FString BaseName);

	static FString GetTestDirectory();

	static FString GetUnrealTestDirectory();

	static FString FormatScreenshotOutputName(FString BaseName);

	static void ForceRefreshViewport();
};

#endif