#include "HoudiniEditorTests.h"


#if WITH_DEV_AUTOMATION_TESTS
#include "HoudiniEditorTestUtils.h"

#include "Core/Public/HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "HoudiniAssetComponent.h"


IMPLEMENT_SIMPLE_AUTOMATION_TEST(HoudiniEditorEvergreenTest, "Houdini.Editor.EvergreenScreenshots", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool HoudiniEditorEvergreenTest::RunTest(const FString & Parameters)
{
	// Really force editor size
	// TODO: Move to HoudiniEditorUtils
	FHoudiniEditorTestUtils::InitializeTests(this);

	FHoudiniEditorTestUtils::InstantiateAsset(this, TEXT("/Game/TestHDAs/Evergreen"), 
        [=](UHoudiniAssetComponent * HAC, const bool IsSuccessful)
        {
                FHoudiniEditorTestUtils::TakeScreenshotEditor(this, "EverGreen_EntireEditor.png", FHoudiniEditorTestUtils::ENTIRE_EDITOR, FHoudiniEditorTestUtils::GDefaultEditorSize);
                FHoudiniEditorTestUtils::TakeScreenshotEditor(this, "EverGreen_Details.png", FHoudiniEditorTestUtils::DETAILS_WINDOW, FVector2D(400, 1130));
                FHoudiniEditorTestUtils::TakeScreenshotEditor(this, "EverGreen_EditorViewport.png", FHoudiniEditorTestUtils::VIEWPORT, FVector2D(640, 360));
                //FHoudiniEditorTestUtils::TakeScreenshotViewport(this, "EverGreen_Viewport.png"); // Viewport resolution might be inconsisent
        });
	return true;
}

#endif
