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

#include "HoudiniAssetComponent.h"

#include "CoreMinimal.h"

#include "Widgets/Layout/SUniformGridPanel.h"
#include "SCurveEditor.h"
#include "Editor/CurveEditor/Public/CurveEditorSettings.h"
#include "HoudiniParameterTranslator.h"
#include "Curves/CurveFloat.h"
#include "SColorGradientEditor.h"
#include "Curves/CurveLinearColor.h"

#include "HoudiniParameterDetails.generated.h"

class UHoudiniAssetComponent;
class UHoudiniParameter;
class UHoudiniParameterFloat; 
class UHoudiniParameterInt;
class UHoudiniParameterString;
class UHoudiniParameterColor;
class UHoudiniParameterButton;
class UHoudiniParameterLabel;
class UHoudiniParameterToggle;
class UHoudiniParameterFile;
class UHoudiniParameterChoice;
class UHoudiniParameterFolder;
class UHoudiniParameterFolderList;
class UHoudiniParameterMultiParm;
class UHoudiniParameterRampFloat;
class UHoudiniParameterRampColor;
class UHoudiniParameterOperatorPath;

class UHoudiniParameterRampColorPoint;
class UHoudiniParameterRampFloatPoint;

class IDetailCategoryBuilder;
class FDetailWidgetRow;
class SHorizontalBox;
class SHoudiniAssetParameterRampCurveEditor;

enum class EHoudiniRampInterpolationType : int8;


class SHoudiniFloatRampCurveEditor : public SCurveEditor
{
public:
	SLATE_BEGIN_ARGS(SHoudiniFloatRampCurveEditor)
		: _ViewMinInput(0.0f)
		, _ViewMaxInput(10.0f)
		, _ViewMinOutput(0.0f)
		, _ViewMaxOutput(1.0f)
		, _InputSnap(0.1f)
		, _OutputSnap(0.05f)
		, _InputSnappingEnabled(false)
		, _OutputSnappingEnabled(false)
		, _ShowTimeInFrames(false)
		, _TimelineLength(5.0f)
		, _DesiredSize(FVector2D::ZeroVector)
		, _DrawCurve(true)
		, _HideUI(true)
		, _AllowZoomOutput(true)
		, _AlwaysDisplayColorCurves(false)
		, _ZoomToFitVertical(true)
		, _ZoomToFitHorizontal(true)
		, _ShowZoomButtons(true)
		, _XAxisName()
		, _YAxisName()
		, _ShowInputGridNumbers(true)
		, _ShowOutputGridNumbers(true)
		, _ShowCurveSelector(true)
		, _GridColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.3f))
	{
		_Clipping = EWidgetClipping::ClipToBounds;
	}

		SLATE_ATTRIBUTE(float, ViewMinInput)
		SLATE_ATTRIBUTE(float, ViewMaxInput)
		SLATE_ATTRIBUTE(TOptional<float>, DataMinInput)
		SLATE_ATTRIBUTE(TOptional<float>, DataMaxInput)
		SLATE_ATTRIBUTE(float, ViewMinOutput)
		SLATE_ATTRIBUTE(float, ViewMaxOutput)
		SLATE_ATTRIBUTE(float, InputSnap)
		SLATE_ATTRIBUTE(float, OutputSnap)
		SLATE_ATTRIBUTE(bool, InputSnappingEnabled)
		SLATE_ATTRIBUTE(bool, OutputSnappingEnabled)
		SLATE_ATTRIBUTE(bool, ShowTimeInFrames)
		SLATE_ATTRIBUTE(float, TimelineLength)
		SLATE_ATTRIBUTE(FVector2D, DesiredSize)
		SLATE_ATTRIBUTE(bool, AreCurvesVisible)
		SLATE_ARGUMENT(bool, DrawCurve)
		SLATE_ARGUMENT(bool, HideUI)
		SLATE_ARGUMENT(bool, AllowZoomOutput)
		SLATE_ARGUMENT(bool, AlwaysDisplayColorCurves)
		SLATE_ARGUMENT(bool, ZoomToFitVertical)
		SLATE_ARGUMENT(bool, ZoomToFitHorizontal)
		SLATE_ARGUMENT(bool, ShowZoomButtons)
		SLATE_ARGUMENT(TOptional<FString>, XAxisName)
		SLATE_ARGUMENT(TOptional<FString>, YAxisName)
		SLATE_ARGUMENT(bool, ShowInputGridNumbers)
		SLATE_ARGUMENT(bool, ShowOutputGridNumbers)
		SLATE_ARGUMENT(bool, ShowCurveSelector)
		SLATE_ARGUMENT(FLinearColor, GridColor)
		SLATE_EVENT(FOnSetInputViewRange, OnSetInputViewRange)
		SLATE_EVENT(FOnSetOutputViewRange, OnSetOutputViewRange)
		SLATE_EVENT(FOnSetAreCurvesVisible, OnSetAreCurvesVisible)
		SLATE_EVENT(FSimpleDelegate, OnCreateAsset)
		SLATE_END_ARGS()

	public:
		TWeakObjectPtr<UHoudiniFloatRampCurve> HoudiniFloatRampCurve;

		/** Widget construction. **/
		void Construct(const FArguments & InArgs);

		virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

		virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

};


class SHoudiniColorRampCurveEditor : public SColorGradientEditor 
{

public:
	SLATE_BEGIN_ARGS(SHoudiniColorRampCurveEditor)
		: _ViewMinInput(0.0f)
		, _ViewMaxInput(10.0f)
		, _InputSnap(0.1f)
		, _OutputSnap(0.05f)
		, _InputSnappingEnabled(false)
		, _OutputSnappingEnabled(false)
		, _ShowTimeInFrames(false)
		, _TimelineLength(5.0f)
		, _DesiredSize(FVector2D::ZeroVector)
		, _DrawCurve(true)
		, _HideUI(true)
		, _AllowZoomOutput(true)
		, _AlwaysDisplayColorCurves(false)
		, _ZoomToFitVertical(true)
		, _ZoomToFitHorizontal(true)
		, _ShowZoomButtons(true)
		, _XAxisName()
		, _YAxisName()
		, _ShowInputGridNumbers(true)
		, _ShowOutputGridNumbers(true)
		, _ShowCurveSelector(true)
		, _GridColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.3f))
	{
		_Clipping = EWidgetClipping::ClipToBounds;
	}

	SLATE_ATTRIBUTE(float, ViewMinInput)
		SLATE_ATTRIBUTE(float, ViewMaxInput)
		SLATE_ATTRIBUTE(TOptional<float>, DataMinInput)
		SLATE_ATTRIBUTE(TOptional<float>, DataMaxInput)
		SLATE_ATTRIBUTE(float, InputSnap)
		SLATE_ATTRIBUTE(float, OutputSnap)
		SLATE_ATTRIBUTE(bool, InputSnappingEnabled)
		SLATE_ATTRIBUTE(bool, OutputSnappingEnabled)
		SLATE_ATTRIBUTE(bool, ShowTimeInFrames)
		SLATE_ATTRIBUTE(float, TimelineLength)
		SLATE_ATTRIBUTE(FVector2D, DesiredSize)
		SLATE_ATTRIBUTE(bool, AreCurvesVisible)
		SLATE_ARGUMENT(bool, DrawCurve)
		SLATE_ARGUMENT(bool, HideUI)
		SLATE_ARGUMENT(bool, AllowZoomOutput)
		SLATE_ARGUMENT(bool, AlwaysDisplayColorCurves)
		SLATE_ARGUMENT(bool, ZoomToFitVertical)
		SLATE_ARGUMENT(bool, ZoomToFitHorizontal)
		SLATE_ARGUMENT(bool, ShowZoomButtons)
		SLATE_ARGUMENT(TOptional<FString>, XAxisName)
		SLATE_ARGUMENT(TOptional<FString>, YAxisName)
		SLATE_ARGUMENT(bool, ShowInputGridNumbers)
		SLATE_ARGUMENT(bool, ShowOutputGridNumbers)
		SLATE_ARGUMENT(bool, ShowCurveSelector)
		SLATE_ARGUMENT(FLinearColor, GridColor)
		SLATE_EVENT(FOnSetInputViewRange, OnSetInputViewRange)
		SLATE_EVENT(FOnSetOutputViewRange, OnSetOutputViewRange)
		SLATE_EVENT(FOnSetAreCurvesVisible, OnSetAreCurvesVisible)
		SLATE_EVENT(FSimpleDelegate, OnCreateAsset)
		SLATE_END_ARGS()

	public:
		/** Widget construction. **/
		void Construct(const FArguments & InArgs);

		TWeakObjectPtr<UHoudiniColorRampCurve> HoudiniColorRampCurve;

		virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

		virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

		virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
};

UCLASS()
class UHoudiniFloatRampCurve : public UCurveFloat 
{
	GENERATED_BODY()

	public:

		TArray<TWeakObjectPtr<UHoudiniParameterRampFloat>> FloatRampParameters;

		virtual void OnCurveChanged(const TArray<FRichCurveEditInfo>& ChangedCurveEditInfos) override;
};


UCLASS()
class UHoudiniColorRampCurve : public UCurveLinearColor
{
	GENERATED_BODY()

	public:
		bool bEditing = false;

		TArray<TWeakObjectPtr<UHoudiniParameterRampColor>> ColorRampParameters;

		virtual void OnCurveChanged(const TArray< FRichCurveEditInfo > & ChangedCurveEditInfos) override;

		void OnColorRampCurveChanged(bool bModificationOnly = false);
};

// This class is used for parenting CurveEditor of float ramp.
// The parent of the instance of this class is set to the float ramp parameter
// The destructor calls SetCurveOwner() on the curve editor to avoid crash at level switching.
UCLASS()
class  UHoudiniFloatCurveEditorParentClass : public UObject
{
	GENERATED_BODY()

	public:
	TSharedPtr<SHoudiniFloatRampCurveEditor> CurveEditor;

	~UHoudiniFloatCurveEditorParentClass();
};

// This class is used for parenting CurveEditor of color ramp.
// The parent of the instance of this class is set to the color ramp parameter
// The destructor calls SetCurveOwner() on the curve editor to avoid crash at level switching.
UCLASS()
class  UHoudiniColorCurveEditorParentClass : public UObject
{
	GENERATED_BODY()

	public:
	TSharedPtr<SHoudiniColorRampCurveEditor> CurveEditor;

	~UHoudiniColorCurveEditorParentClass();

};

//class FHoudiniParameterDetails : public TSharedFromThis<FHoudiniParameterDetails>, public TNumericUnitTypeInterface<float>, public TNumericUnitTypeInterface<int32>
class FHoudiniParameterDetails : public TSharedFromThis<FHoudiniParameterDetails>
{
	public:
		void CreateWidget(
			IDetailCategoryBuilder & HouParameterCategory,
			TArray<UHoudiniParameter*> &InParams);

		void CreateWidgetInt(
			IDetailCategoryBuilder & HouParameterCategory, TArray<UHoudiniParameter*>& InParams);
		void CreateWidgetFloat(
			IDetailCategoryBuilder & HouParameterCategory, TArray<UHoudiniParameter*>& InParams);
		void CreateWidgetString(
			IDetailCategoryBuilder & HouParameterCategory, TArray<UHoudiniParameter*>& InParams);
		void CreateWidgetColor(
			IDetailCategoryBuilder & HouParameterCategory, TArray<UHoudiniParameter*>& InParams);
		void CreateWidgetButton(
			IDetailCategoryBuilder & HouParameterCategory, TArray<UHoudiniParameter*>& InParams);
		void CreateWidgetLabel(
			IDetailCategoryBuilder & HouParameterCategory, TArray<UHoudiniParameter*> &InParams);
		void CreateWidgetToggle(
			IDetailCategoryBuilder & HouParameterCategory, TArray<UHoudiniParameter*> &InParams);
		void CreateWidgetFile(
			IDetailCategoryBuilder & HouParameterCategory, TArray<UHoudiniParameter*> &InParams);
		void CreateWidgetChoice(
			IDetailCategoryBuilder & HouParameterCategory, TArray<UHoudiniParameter*>& InParams);
		void CreateWidgetSeparator(
			IDetailCategoryBuilder & HouParameterCategory, TArray<UHoudiniParameter*>& InParams, const bool& InIsEnabled);
		void CreateWidgetFolderList(
			IDetailCategoryBuilder & HouParameterCategory, TArray<UHoudiniParameter*> &InParams);
		void CreateWidgetFolder(
			IDetailCategoryBuilder & HouParameterCategory, TArray<UHoudiniParameter*> &InParams);
		void CreateWidgetMultiParm(
			IDetailCategoryBuilder & HouParameterCategory, TArray<UHoudiniParameter*> &InParams);
		void CreateWidgetOperatorPath(
			IDetailCategoryBuilder & HouParameterCategory, TArray<UHoudiniParameter*> &InParams);
		void CreateWidgetFloatRamp(
			IDetailCategoryBuilder & HouParameterCategory, TArray<UHoudiniParameter*> &InParams);
		void CreateWidgetColorRamp(
			IDetailCategoryBuilder & HouParameterCategory, TArray<UHoudiniParameter*> &InParams);

		void HandleUnsupportedParmType(
			IDetailCategoryBuilder & HouParameterCategory, TArray<UHoudiniParameter*> &InParams
		);


		static FText GetParameterTooltip(UHoudiniParameter* InParam);

		static void SyncCachedColorRampPoints(UHoudiniParameterRampColor* ColorRampParameter);

		static void SyncCachedFloatRampPoints(UHoudiniParameterRampFloat* FloatRampParameter);

		// replace the children parameter values of all (multi-selected) float ramp parameters with the main parameter (weak object pointer version)
		static void ReplaceAllFloatRampParameterPointsWithMainParameter(TArray<TWeakObjectPtr<UHoudiniParameterRampFloat>>& FloatRampParameters);
		// raw pointer version
		static void ReplaceAllFloatRampParameterPointsWithMainParameter(TArray<UHoudiniParameterRampFloat*>& FloatRampParameters);
		// helper
		static void ReplaceFloatRampParameterPointsWithMainParameter(UHoudiniParameterRampFloat* Param, UHoudiniParameterRampFloat* MainParam);


		// replace the children parameter values of all (multi-selected) color ramp parameters with the main parameter (weak object pointer version)
		static void ReplaceAllColorRampParameterPointsWithMainParameter(TArray<TWeakObjectPtr<UHoudiniParameterRampColor>>& ColorRampParameters);
		// raw pointer version
		static void ReplaceAllColorRampParameterPointsWithMainParameter(TArray<UHoudiniParameterRampColor*>& ColorRampParameters);
		// helper
		static void ReplaceColorRampParameterPointsWithMainParameter(UHoudiniParameterRampColor* Param, UHoudiniParameterRampColor* MainParam);



		// Create an insert event for a float ramp parameter
		static void CreateFloatRampParameterInsertEvent(UHoudiniParameterRampFloat* InParam, 
			const float& InPosition, const float& InValue, const EHoudiniRampInterpolationType &InInterp);

		// Create an insert event for a color ramp parameter
		static void CreateColorRampParameterInsertEvent(UHoudiniParameterRampColor* InParam, 
			const float& InPosition, const FLinearColor& InColor, const EHoudiniRampInterpolationType &InInterp);

		// Create a delete event for a float ramp parameter
		static void CreateFloatRampParameterDeleteEvent(UHoudiniParameterRampFloat* InParam, const int32 &InDeleteIndex);

		// Create a delete event for a color ramp parameter
		static void CreateColorRampParameterDeleteEvent(UHoudiniParameterRampColor* InParam, const int32 &InDeleteIndex);


	private:

		template< class T >
		static bool CastParameters(
			TArray<UHoudiniParameter*> InParams, TArray<T*>& OutCastedParams);

		//
		// Private helper functions for widget creation
		//

		// Creates the default name widget, the parameter will then fill the value after
		void CreateNameWidget(FDetailWidgetRow* Row, TArray<UHoudiniParameter*> &InParams, bool WithLabel);

		// Creates the default name widget, with an extra checkbox for disabling the the parameter update
		void CreateNameWidgetWithAutoUpdate(FDetailWidgetRow* Row, TArray<UHoudiniParameter*> &InParams, bool WithLabel);

		FDetailWidgetRow* CreateNestedRow(IDetailCategoryBuilder & HouParameterCategory, TArray<UHoudiniParameter*> InParams, bool bDecreaseChildCount = true); //

		void CreateFolderHeaderUI(FDetailWidgetRow* HeaderRow, TArray<UHoudiniParameter*>& InParams); //

		void CreateWidgetTabMenu(IDetailCategoryBuilder & HouParameterCategory, FDetailWidgetRow* OutputRow, TArray<UHoudiniParameter*> &InParams);

		void CreateWidgetTab(UHoudiniParameterFolder* InParam);  //

		void CreateWidgetMultiParmObjectButtons(TSharedPtr<SHorizontalBox> HorizontalBox, TArray<UHoudiniParameter*> InParams); //
	
		// Create the UI for ramp's curve editor.
		FDetailWidgetRow* CreateWidgetRampCurveEditor(IDetailCategoryBuilder & HouParameterCategory, TArray<UHoudiniParameter*> &InParams); //

		// Create the UI for ramp's stop points.
		void CreateWidgetRampPoints(FDetailWidgetRow* Row, UHoudiniParameter* InParameter, TArray<UHoudiniParameter*>& InParams); //

		FString GetIndentationString(UHoudiniParameter* InParam);

		int32 GetIndentationLevel(UHoudiniParameter* InParam);

		void PruneStack(); //


		// The parameter directory is flattened with BFS inside of DFS.
		// When a folderlist is encountered, it goes 'one step' of DFS, otherwise BFS.
		// So that use a Stack<Queue> structure to reconstruct the tree.

		TArray<TArray<bool>> FolderStack;

		TArray<TArray<int32>> FolderChildCounterStack;

		UHoudiniParameterRampFloat* CurrentRampFloat;

		UHoudiniParameterRampColor* CurrentRampColor;

		TArray<UHoudiniParameter*> CurrentRampParameterList;

		TArray<UHoudiniParameterRampFloatPoint*> CurrentRampFloatPointsArray;

		TArray<UHoudiniParameterRampColorPoint*> CurrentRampColorPointsArray;

		UHoudiniColorRampCurve* CurrentRampParameterColorCurve;

		UHoudiniFloatRampCurve* CurrentRampParameterFloatCurve;

		FDetailWidgetRow * CurrentRampRow;


		/* Variables for keeping expansion state after adding multiparm instance*/
		TMap<int32, UHoudiniParameterMultiParm*> AllMultiParms;

		TMap<int32, UHoudiniParameter*> AllFoldersAndTabs;

		/* Variables for keeping expansion state after adding multiparm instance*/

		TMap<int32, int32> MultiParmInstanceIndices;

		TMap<int32, int32> Indentation;

		int32 CurrentFolderListSize = 0;

		TSharedPtr<SHorizontalBox> CurrentTabMenuBox;

		bool bCurrentTabMenu = false;

		UHoudiniParameterFolderList* CurrentTabMenuFolderList;

		TArray<UHoudiniFloatCurveEditorParentClass*> FloatCurveParents;

		TArray<UHoudiniColorCurveEditorParentClass*> ColorCurveParents;

};