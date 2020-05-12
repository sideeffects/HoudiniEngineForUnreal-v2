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

#include "HoudiniParameterDetails.h"

#include "HoudiniAssetComponent.h"
#include "HoudiniParameter.h"
#include "HoudiniParameterFloat.h"
#include "HoudiniParameterInt.h"
#include "HoudiniParameterString.h"
#include "HoudiniParameterColor.h"
#include "HoudiniParameterButton.h"
#include "HoudiniParameterButtonStrip.h"
#include "HoudiniParameterLabel.h"
#include "HoudiniParameterToggle.h"
#include "HoudiniParameterFile.h"
#include "HoudiniParameterChoice.h"
#include "HoudiniParameterFolder.h"
#include "HoudiniParameterFolderList.h"
#include "HoudiniParameterMultiParm.h"
#include "HoudiniParameterSeparator.h"
#include "HoudiniParameterRamp.h"
#include "HoudiniParameterOperatorPath.h"
#include "HoudiniInput.h"
#include "HoudiniAsset.h"

#include "HoudiniEngineUtils.h"
#include "HoudiniEngineEditor.h"
#include "HoudiniEnginePrivatePCH.h"
#include "HoudiniEngineEditorPrivatePCH.h"
#include "SNewFilePathPicker.h"

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "IDetailGroup.h"
#include "IDetailCustomization.h"
#include "PropertyCustomizationHelpers.h"
#include "DetailWidgetRow.h"
#include "Math/UnitConversion.h"
#include "ScopedTransaction.h"
#include "EditorDirectories.h"

#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SVectorInputBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Colors/SColorPicker.h"
#include "Widgets/Views/SExpanderArrow.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Input/NumericUnitTypeInterface.inl"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SSeparator.h"
//#include "Widgets/Input/SFilePathPicker.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SSplitter.h"
#include "SCurveEditorView.h"
#include "SAssetDropTarget.h"
#include "AssetThumbnail.h"

#include "HoudiniInputDetails.h"

#include "Framework/SlateDelegates.h"
#include "Templates/SharedPointer.h"


#define LOCTEXT_NAMESPACE HOUDINI_LOCTEXT_NAMESPACE

#define BASE_INDENTATION							 TEXT(" ")

#define MULTIPARM_INDENTATION_LEVEL							15
#define INDENTATION_LEVEL									 9
#define TAB_FOLDER_CHILD_OBJECT_EXTRA_INDENTATION_LEVEL      3
#define TAB_FOLDER_CHILD_FOLDER_EXTRA_INDENTATION_LEVEL     10


void
SHoudiniFloatRampCurveEditor::Construct(const FArguments & InArgs)
{
	SCurveEditor::Construct(SCurveEditor::FArguments()
		.ViewMinInput(InArgs._ViewMinInput)
		.ViewMaxInput(InArgs._ViewMaxInput)
		.ViewMinOutput(InArgs._ViewMinOutput)
		.ViewMaxOutput(InArgs._ViewMaxOutput)
		.XAxisName(InArgs._XAxisName)
		.YAxisName(InArgs._YAxisName)
		.HideUI(InArgs._HideUI)
		.DrawCurve(InArgs._DrawCurve)
		.TimelineLength(InArgs._TimelineLength)
		.AllowZoomOutput(InArgs._AllowZoomOutput)
		.ShowInputGridNumbers(InArgs._ShowInputGridNumbers)
		.ShowOutputGridNumbers(InArgs._ShowOutputGridNumbers)
		.ShowZoomButtons(InArgs._ShowZoomButtons)
		.ZoomToFitHorizontal(InArgs._ZoomToFitHorizontal)
		.ZoomToFitVertical(InArgs._ZoomToFitVertical)
	);


	UCurveEditorSettings * CurveEditorSettings = GetSettings();
	if (CurveEditorSettings)
	{
		CurveEditorSettings->SetTangentVisibility(ECurveEditorTangentVisibility::NoTangents);
	}
}

void
SHoudiniColorRampCurveEditor::Construct(const FArguments & InArgs)
{
	SColorGradientEditor::Construct(SColorGradientEditor::FArguments()
		.ViewMinInput(InArgs._ViewMinInput)
		.ViewMaxInput(InArgs._ViewMaxInput)
	);
}


FReply 
SHoudiniFloatRampCurveEditor::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	FReply Reply = SCurveEditor::OnMouseButtonUp(MyGeometry, MouseEvent);
	
	if (!HoudiniFloatRampCurve.IsValid())
		return Reply;

	FRichCurve& FloatCurve = HoudiniFloatRampCurve.Get()->FloatCurve;

	TArray<TWeakObjectPtr<UHoudiniParameterRampFloat>>& FloatRampParameters = HoudiniFloatRampCurve.Get()->FloatRampParameters;

	if (FloatRampParameters.Num() < 1)
		return Reply;

	if (!FloatRampParameters[0].IsValid())
		return Reply;
	
	UHoudiniParameterRampFloat* MainParam = FloatRampParameters[0].Get();

	if (!MainParam)
		return Reply;

	// Do not allow modification when the parent HDA of the main param is being cooked.
	if (FHoudiniEngineUtils::IsHoudiniAssetComponentCooking(MainParam))
		return Reply;

	// Push the points of the main float ramp param to other parameters
	FHoudiniParameterDetails::ReplaceAllFloatRampParameterPointsWithMainParameter(FloatRampParameters);

	// Modification is based on the main parameter, use synced points if the main param is on auto update mode, use cached points otherwise.
	TArray<UHoudiniParameterRampFloatPoint*> & MainPoints = MainParam->IsAutoUpdate() ? MainParam->Points : MainParam->CachedPoints;

	int32 NumMainPoints = MainPoints.Num();

	// On mouse button up handler handles point modification only
	if (FloatCurve.GetNumKeys() != NumMainPoints)
		return Reply;

	bool bNeedToRefreshEditor= false;

	for (int32 Idx = 0; Idx < NumMainPoints; ++Idx)
	{
		UHoudiniParameterRampFloatPoint* MainPoint = MainPoints[Idx];

		if (!MainPoint)
			continue;

		float& CurvePosition = FloatCurve.Keys[Idx].Time;
		float& CurveValue = FloatCurve.Keys[Idx].Value;

		// This point is modified
		if (MainPoint->GetPosition() != CurvePosition || MainPoint->GetValue() != CurveValue) 
		{

			// The editor needs refresh only if the main parameter is on manual mode, and has been modified
			if (!MainParam->IsAutoUpdate())
				bNeedToRefreshEditor = true;

			// Iterate through the float ramp parameter of all selected HDAs.
			for (auto & NextRampFloat : FloatRampParameters) 
			{
				if (!NextRampFloat.IsValid())
					continue;

				UHoudiniParameterRampFloat* SelectedRampFloat = NextRampFloat.Get();

				if (!SelectedRampFloat)
					continue;

				// Do not modify the selected parameter if its parent HDA is being cooked
				if (FHoudiniEngineUtils::IsHoudiniAssetComponentCooking(SelectedRampFloat))
					continue;

				if (SelectedRampFloat->IsAutoUpdate()) 
				{
					// The selected float ramp parameter is on auto update mode, use its synced points.
					TArray<UHoudiniParameterRampFloatPoint*> &SelectedRampPoints = SelectedRampFloat->Points;

					if (SelectedRampPoints.IsValidIndex(Idx)) 
					{
						// Synced points in the selected ramp is more than or the same number as that in the main parameter,
						// modify the position and value of the synced point and mark them as changed.

						UHoudiniParameterRampFloatPoint*& ModifiedPoint = SelectedRampPoints[Idx];

						if (!ModifiedPoint)
							continue;

						if (ModifiedPoint->GetPosition() != CurvePosition && ModifiedPoint->PositionParentParm) 
						{
							ModifiedPoint->SetPosition(CurvePosition);
							ModifiedPoint->PositionParentParm->MarkChanged(true);
						}

						if (ModifiedPoint->GetValue() != CurveValue && ModifiedPoint->ValueParentParm) 
						{
							ModifiedPoint->SetValue(CurveValue);
							ModifiedPoint->ValueParentParm->MarkChanged(true);
						}
					}
					else 
					{
						// Synced points in the selected ramp is less than that in the main parameter
						// Since we have pushed the points of the main param to all of the selected ramps,
						// We need to modify the insert event.

						int32 IndexInEventsArray = Idx - SelectedRampPoints.Num();
						if (SelectedRampFloat->ModificationEvents.IsValidIndex(Idx))
						{
							UHoudiniParameterRampModificationEvent*& ModEvent = SelectedRampFloat->ModificationEvents[Idx];
							if (!ModEvent)
								continue;

							if (ModEvent->InsertPosition != CurvePosition)
								ModEvent->InsertPosition = CurvePosition;

							if (ModEvent->InsertFloat != CurveValue)
								ModEvent->InsertFloat = CurveValue;
						}

					}
				}
				else 
				{
					// The selected float ramp is on manual update mode, use the cached points.
					TArray<UHoudiniParameterRampFloatPoint*> &FloatRampCachedPoints = SelectedRampFloat->CachedPoints;

					// Since we have pushed the points in main param to all the selected float ramp, 
					// we need to modify the corresponding cached point in the selected float ramp.

					if (FloatRampCachedPoints.IsValidIndex(Idx))
					{
						UHoudiniParameterRampFloatPoint*& ModifiedCachedPoint = FloatRampCachedPoints[Idx];

						if (!ModifiedCachedPoint)
							continue;
						
						if (ModifiedCachedPoint->Position != CurvePosition)
						{
							ModifiedCachedPoint->Position = CurvePosition;
							SelectedRampFloat->bCaching = true;
						}

						if (ModifiedCachedPoint->Value != CurveValue)
						{
							ModifiedCachedPoint->Value = CurveValue;
							SelectedRampFloat->bCaching = true;
						}						
					}
				}
			}
		}
	}


	if (bNeedToRefreshEditor)
	{
		FHoudiniEngineUtils::UpdateEditorProperties(MainParam, true);
	}

	return Reply;
}

FReply 
SHoudiniFloatRampCurveEditor::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	FReply Reply = SCurveEditor::OnKeyDown(MyGeometry, InKeyEvent);

	if (InKeyEvent.GetKey().ToString() != FString("Enter"))
		return Reply;

	if (!HoudiniFloatRampCurve.IsValid() || !HoudiniFloatRampCurve.Get())
		return Reply;

	TArray<TWeakObjectPtr<UHoudiniParameterRampFloat>> FloatRampParameters = HoudiniFloatRampCurve.Get()->FloatRampParameters;

	if (FloatRampParameters.Num() < 1)
		return Reply;

	if (!FloatRampParameters[0].IsValid())
		return Reply;

	UHoudiniParameterRampFloat* MainParam = FloatRampParameters[0].Get();

	if (!MainParam)
		return Reply;

	// Do nothing if the main param is on auto update mode
	if (MainParam->IsAutoUpdate())
		return Reply;

	if (FHoudiniEngineUtils::IsHoudiniAssetComponentCooking(MainParam))
		return Reply;

	// Push the points in the main float ramp to the float ramp parameters in all selected HDAs.
	FHoudiniParameterDetails::ReplaceAllFloatRampParameterPointsWithMainParameter(FloatRampParameters);

	for (auto& NextFloatRamp : FloatRampParameters) 
	{
		if (!NextFloatRamp.IsValid())
			continue;

		UHoudiniParameterRampFloat* SelectedFloatRamp = NextFloatRamp.Get();

		if (!SelectedFloatRamp)
			continue;

		if (SelectedFloatRamp->IsAutoUpdate())
			continue;

		// Do not sync the selected parameter if its parent HDA is being cooked
		if (FHoudiniEngineUtils::IsHoudiniAssetComponentCooking(SelectedFloatRamp))
			continue;

		// Sync the cached points if the selected float ramp parameter is on manual update mode
		FHoudiniParameterDetails::SyncCachedFloatRampPoints(SelectedFloatRamp);
	}

	return Reply;
}

void
UHoudiniFloatRampCurve::OnCurveChanged(const TArray<FRichCurveEditInfo>& ChangedCurveEditInfos) 
{
	Super::OnCurveChanged(ChangedCurveEditInfos);
	
	if (FloatRampParameters.Num() < 1)
		return;

	if (!FloatRampParameters[0].IsValid())
		return;

	UHoudiniParameterRampFloat* MainParam = FloatRampParameters[0].Get();

	if (!MainParam)
		return;

	// Do not allow modification when the parent HDA of the main param is being cooked
	if (FHoudiniEngineUtils::IsHoudiniAssetComponentCooking(MainParam))
		return;

	// Push all the points of the Main parameter to other parameters
	FHoudiniParameterDetails::ReplaceAllFloatRampParameterPointsWithMainParameter(FloatRampParameters);

	// Modification is based on the Main Param, use synced points if the Main Param is on auto update mode, otherwise use its cached points.

	TArray<UHoudiniParameterRampFloatPoint*> & MainPoints = MainParam->IsAutoUpdate() ? MainParam->Points : MainParam->CachedPoints;

	int32 NumMainPoints = MainPoints.Num();

	bool bNeedUpdateEditor = false;

	// OnCurveChanged handler handles point delete and insertion only

	// A point is deleted.
	if (FloatCurve.GetNumKeys() < NumMainPoints) 
	{
		// Find the index of the deleted point
		for (int32 Idx = 0; Idx < NumMainPoints; ++Idx) 
		{
			UHoudiniParameterRampFloatPoint* MainPoint = MainPoints[Idx];

			if (!MainPoint)
				continue;

			float CurPointPosition = MainPoint->GetPosition();
			float CurCurvePosition = -1.0f;

			if (FloatCurve.Keys.IsValidIndex(Idx))
				CurCurvePosition = FloatCurve.Keys[Idx].Time;

			// Delete the point at Idx
			if (CurCurvePosition != CurPointPosition) 
			{		
				// Iterate through all the float ramp parameter in all the selected HDAs
				for (auto & NextFloatRamp : FloatRampParameters) 
				{
					if (!NextFloatRamp.IsValid())
						continue;

					UHoudiniParameterRampFloat* SelectedFloatRamp = NextFloatRamp.Get();

					if (!SelectedFloatRamp)
						continue;

					// Do not modify the selected parameter if its parent HDA is being cooked
					if (FHoudiniEngineUtils::IsHoudiniAssetComponentCooking(SelectedFloatRamp))
						continue;

					if (SelectedFloatRamp->IsAutoUpdate())
					{
						TArray<UHoudiniParameterRampFloatPoint*> & SelectedRampPoints = SelectedFloatRamp->Points;

						// The selected float ramp is on auto update mode:

						if (SelectedRampPoints.IsValidIndex(Idx)) 
						{
							// If the number of synced points of the selected float ramp is greater or equal to the number of points of that in the main param,
							// Create a Houdini engine manager event to delete the point at Idx of the selected float ramp;

							UHoudiniParameterRampFloatPoint* PointToDelete = SelectedRampPoints[Idx];

							if (!PointToDelete)
								continue;

							FHoudiniParameterDetails::CreateFloatRampParameterDeleteEvent(SelectedFloatRamp, PointToDelete->InstanceIndex);
							SelectedFloatRamp->MarkChanged(true);
						}
						else 
						{
							// If the number is smaller than that in the main param, since we have pushed all the points in the main param to the selected parameters,
							// delete the corresponding inserting event.

							int32 IdxInEventsArray = Idx - SelectedRampPoints.Num();
							if (SelectedFloatRamp->ModificationEvents.IsValidIndex(IdxInEventsArray)) 
								SelectedFloatRamp->ModificationEvents.RemoveAt(IdxInEventsArray);
						}
					}
					else 
					{
						// The selected float ramp is on manual update mode:
						// Since we have pushed all the points in main param to the cached points of the selected float ramp,
						// remove the corresponding points from the cached points array.

						if (SelectedFloatRamp->CachedPoints.IsValidIndex(Idx))
						{
							SelectedFloatRamp->CachedPoints.RemoveAt(Idx);
							SelectedFloatRamp->bCaching = true;
						}
					}
				}

				// Refresh the editor only when the main parameter is on manual update mode and has been modified.
				if (!MainParam->IsAutoUpdate())
					bNeedUpdateEditor = true;

				break;
			}
		}
	}

	// A point is inserted
	else if (FloatCurve.GetNumKeys() > NumMainPoints)
	{
		// Find the index of the inserted point
		for (int32 Idx = 0; Idx < FloatCurve.GetNumKeys(); ++Idx) 
		{

			float CurPointPosition = -1.0f;
			float CurCurvePosition = FloatCurve.Keys[Idx].Time;

			if (MainPoints.IsValidIndex(Idx)) 
			{
				UHoudiniParameterRampFloatPoint*& MainPoint = MainPoints[Idx];

				if (!MainPoint)
					continue;

				CurPointPosition = MainPoint->GetPosition();
			}

			// Insert instance at Idx
			if (CurPointPosition != CurCurvePosition) 
			{
				// Iterate through the float ramp parameter of all selected HDAs.
				for (auto & NextFloatRamp : FloatRampParameters) 
				{
					if (!NextFloatRamp.IsValid())
						continue;

					UHoudiniParameterRampFloat* SelectedFloatRamp = NextFloatRamp.Get();

					// Do not modify the selected parameter if its parent HDA is being cooked
					if (FHoudiniEngineUtils::IsHoudiniAssetComponentCooking(SelectedFloatRamp))
						continue;

					if (SelectedFloatRamp->IsAutoUpdate()) 
					{
						// If the selected float ramp is on auto update mode:
						// Since we have pushed all the points of main parameter to the selected,
						// create a Houdini engine manager event to insert a point. 

						FHoudiniParameterDetails::CreateFloatRampParameterInsertEvent(
							SelectedFloatRamp, CurCurvePosition, FloatCurve.Keys[Idx].Value, EHoudiniRampInterpolationType::LINEAR);

						SelectedFloatRamp->MarkChanged(true);
					}
					else 
					{
						// If the selected float ramp is on manual update mode:
						// push a new point to the cached points array
						UHoudiniParameterRampFloatPoint* NewCachedPoint = NewObject<UHoudiniParameterRampFloatPoint>(SelectedFloatRamp, UHoudiniParameterRampFloatPoint::StaticClass());
						if (!NewCachedPoint)
							continue;

						NewCachedPoint->Position = CurCurvePosition;
						NewCachedPoint->Value = FloatCurve.Keys[Idx].Value;
						NewCachedPoint->Interpolation = EHoudiniRampInterpolationType::LINEAR;

						if (Idx >= SelectedFloatRamp->CachedPoints.Num())
							SelectedFloatRamp->CachedPoints.Add(NewCachedPoint);
						else
							SelectedFloatRamp->CachedPoints.Insert(NewCachedPoint, Idx);

						SelectedFloatRamp->bCaching = true;
					}
				}

				// Refresh the editor only when the main parameter is on manual update mode and has been modified.
				if (!MainParam->IsAutoUpdate())
					bNeedUpdateEditor = true;

				break;
			}
		}
	}

	if (bNeedUpdateEditor)
	{
		FHoudiniEngineUtils::UpdateEditorProperties(MainParam, true);
	}

}


FReply
SHoudiniColorRampCurveEditor::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) 
{
	FReply Reply = SColorGradientEditor::OnMouseButtonDown(MyGeometry, MouseEvent);

	if (HoudiniColorRampCurve.IsValid()) 
	{
		UHoudiniColorRampCurve* Curve = HoudiniColorRampCurve.Get();
		if (Curve)
			Curve->bEditing = true;
	}
	
	return Reply;
}

FReply 
SHoudiniColorRampCurveEditor::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) 
{

	FReply Reply = SColorGradientEditor::OnMouseButtonUp(MyGeometry, MouseEvent);

	if (HoudiniColorRampCurve.IsValid()) 
	{
		UHoudiniColorRampCurve* Curve = HoudiniColorRampCurve.Get();

		if (Curve)
		{
			Curve->bEditing = false;
			Curve->OnColorRampCurveChanged(true);
		}
	}

	return Reply;

}

FReply
SHoudiniColorRampCurveEditor::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	FReply Reply = SColorGradientEditor::OnKeyDown(MyGeometry, InKeyEvent);

	if (InKeyEvent.GetKey().ToString() != FString("Enter"))
		return Reply;

	if (!HoudiniColorRampCurve.IsValid() || !HoudiniColorRampCurve.Get())
		return Reply;

	TArray<TWeakObjectPtr<UHoudiniParameterRampColor>> &ColorRampParameters = HoudiniColorRampCurve.Get()->ColorRampParameters;

	if (ColorRampParameters.Num() < 1)
		return Reply;

	UHoudiniParameterRampColor* MainParam = ColorRampParameters[0].Get();

	if (!MainParam)
		return Reply;

	// Do nothing if the main param is on auto update mode
	if (MainParam->IsAutoUpdate())
		return Reply;

	if (FHoudiniEngineUtils::IsHoudiniAssetComponentCooking(MainParam))
		return Reply;

	// Push the points in the main color ramp to the color ramp parameters in all selected HDAs.
	FHoudiniParameterDetails::ReplaceAllColorRampParameterPointsWithMainParameter(ColorRampParameters);

	for (auto& NextColorRamp : ColorRampParameters) 
	{
		if (!NextColorRamp.IsValid())
			continue;

		UHoudiniParameterRampColor* SelectedColorRamp = NextColorRamp.Get();

		if (!SelectedColorRamp)
			continue;

		if (SelectedColorRamp->IsAutoUpdate())
			continue;

		// Do not sync the selected parameter if its parent HDA is being cooked
		if (FHoudiniEngineUtils::IsHoudiniAssetComponentCooking(SelectedColorRamp))
			continue;

		// Sync the cached points if the selected color ramp is on manual update mode
		FHoudiniParameterDetails::SyncCachedColorRampPoints(SelectedColorRamp);
	}

	return Reply;
}

void 
UHoudiniColorRampCurve::OnCurveChanged(const TArray< FRichCurveEditInfo > & ChangedCurveEditInfos) 
{
	Super::OnCurveChanged(ChangedCurveEditInfos);

	OnColorRampCurveChanged();
}

void 
UHoudiniColorRampCurve::OnColorRampCurveChanged(bool bModificationOnly) 
{
	if (!FloatCurves)
		return;

	if (ColorRampParameters.Num() < 1)
		return;

	if (!ColorRampParameters[0].IsValid())
		return;

	UHoudiniParameterRampColor* MainParam = ColorRampParameters[0].Get();

	if (!MainParam)
		return;

	// Do not allow modification when the parent HDA of the main param is being cooked
	if (FHoudiniEngineUtils::IsHoudiniAssetComponentCooking(MainParam))
		return;

	// Push all the points of the main parameter to other parameters
	FHoudiniParameterDetails::ReplaceAllColorRampParameterPointsWithMainParameter(ColorRampParameters);

	// Modification is based on the Main Param, use synced points if the Main Param is on auto update mode,otherwise use its cached points.
	TArray<UHoudiniParameterRampColorPoint*> & MainPoints = MainParam->IsAutoUpdate() ? MainParam->Points : MainParam->CachedPoints;

	int32 NumMainPoints = MainPoints.Num();

	bool bNeedUpdateEditor = false;

	// OnCurveChanged handler of color ramp curve editor handles point delete, insert and color change

	// A point is deleted
	if (FloatCurves->GetNumKeys() < NumMainPoints)
	{
		if (bModificationOnly)
			return;

		// Find the index of the deleted point
		for (int32 Idx = 0; Idx < NumMainPoints; ++Idx)
		{
			UHoudiniParameterRampColorPoint* MainPoint = MainPoints[Idx];

			if (!MainPoint)
				continue;

			float CurPointPosition = MainPoint->GetPosition();
			float CurCurvePosition = -1.0f;

			if (FloatCurves[0].Keys.IsValidIndex(Idx))
				CurCurvePosition = FloatCurves[0].Keys[Idx].Time;

			// Delete the point at Idx
			if (CurCurvePosition != CurPointPosition)
			{
				// Iterate through all the color ramp parameter in all the selected HDAs
				for (auto & NextColorRamp : ColorRampParameters)
				{
					if (!NextColorRamp.IsValid())
						continue;

					UHoudiniParameterRampColor* SelectedColorRamp = NextColorRamp.Get();

					if (!SelectedColorRamp)
						continue;

					// Do not modify the selected parameter if its parent HDA is being cooked
					if (FHoudiniEngineUtils::IsHoudiniAssetComponentCooking(SelectedColorRamp))
						continue;

					if (SelectedColorRamp->IsAutoUpdate())
					{
						TArray<UHoudiniParameterRampColorPoint*> & SelectedRampPoints = SelectedColorRamp->Points;

						// The selected color ramp is on auto update mode:

						if (SelectedRampPoints.IsValidIndex(Idx))
						{
							// If the number of synced points of the selected color ramp is greater or equal to the number of points of that in the main param,
							// create a Houdini engine manager event to delete the point at Idx of the selected float ramp;

							UHoudiniParameterRampColorPoint* PointToDelete = SelectedRampPoints[Idx];

							if (!PointToDelete)
								continue;

							FHoudiniParameterDetails::CreateColorRampParameterDeleteEvent(SelectedColorRamp, PointToDelete->InstanceIndex);
							SelectedColorRamp->MarkChanged(true);
						}
						else
						{
							// If the number is smaller than that in the main param, since we have pushed all the points in the main param to the selected parameters,
							// delete the corresponding inserting event.

							int32 IdxInEventsArray = Idx - SelectedRampPoints.Num();
							if (SelectedColorRamp->ModificationEvents.IsValidIndex(IdxInEventsArray))
								SelectedColorRamp->ModificationEvents.RemoveAt(IdxInEventsArray);
						}
					}
					else
					{
						// The selected color ramp is on manual update mode:
						// Since we have pushed all the points in main param to the cached points of the selected float ramp,
						// remove the corresponding points from the cached points array
						if (SelectedColorRamp->CachedPoints.IsValidIndex(Idx))
						{
							SelectedColorRamp->CachedPoints.RemoveAt(Idx);
							SelectedColorRamp->bCaching = true;
						}
					}
				}

				// Refresh the editor only when the main parameter is on manual update mode and has been modified.
				if (!MainParam->IsAutoUpdate())
					bNeedUpdateEditor = true;

				break;
			}
		}
	}

	// A point is inserted
	else if (FloatCurves[0].GetNumKeys() > NumMainPoints)
	{

		if (bModificationOnly)
			return;

		// Find the index of the inserted point
		for (int32 Idx = 0; Idx < FloatCurves[0].GetNumKeys(); ++Idx)
		{

			float CurPointPosition = -1.0f;
			float CurCurvePosition = FloatCurves[0].Keys[Idx].Time;

			if (MainPoints.IsValidIndex(Idx))
			{
				UHoudiniParameterRampColorPoint*& MainPoint = MainPoints[Idx];

				if (!MainPoint)
					continue;

				CurPointPosition = MainPoint->GetPosition();
			}

			// Insert a point at Idx
			if (CurPointPosition != CurCurvePosition)
			{
				// Get the interpolation value of inserted color point

				FLinearColor ColorPrev = FLinearColor::Black;
				FLinearColor ColorNext = FLinearColor::White;
				float PositionPrev = 0.0f;
				float PositionNext = 1.0f;

				if (MainParam->IsAutoUpdate())
				{
					// Try to get its previous point's color
					if (MainParam->Points.IsValidIndex(Idx - 1))
					{
						ColorPrev = MainParam->Points[Idx - 1]->GetValue();
						PositionPrev = MainParam->Points[Idx - 1]->GetPosition();
					}

					// Try to get its next point's color
					if (MainParam->Points.IsValidIndex(Idx))
					{
						ColorNext = MainParam->Points[Idx]->GetValue();
						PositionNext = MainParam->Points[Idx]->GetPosition();
					}
				}
				else
				{
					// Try to get its previous point's color
					if (MainParam->CachedPoints.IsValidIndex(Idx - 1))
					{
						ColorPrev = MainParam->CachedPoints[Idx - 1]->GetValue();
						PositionPrev = MainParam->CachedPoints[Idx - 1]->GetPosition();
					}

					// Try to get its next point's color
					if (MainParam->CachedPoints.IsValidIndex(Idx))
					{
						ColorNext = MainParam->CachedPoints[Idx]->GetValue();
						PositionNext = MainParam->CachedPoints[Idx]->GetPosition();
					}
				}

				float TotalWeight = FMath::Abs(PositionNext - PositionPrev);
				float PrevWeight = FMath::Abs(CurCurvePosition - PositionPrev);
				float NextWeight = FMath::Abs(PositionNext - CurCurvePosition);

				FLinearColor InsertedColor = ColorPrev * (PrevWeight / TotalWeight) + ColorNext * (NextWeight / TotalWeight);

				// Iterate through the color ramp parameter of all selected HDAs.
				for (auto & NextColorRamp : ColorRampParameters)
				{
					if (!NextColorRamp.IsValid())
						continue;

					UHoudiniParameterRampColor* SelectedColorRamp = NextColorRamp.Get();

					if (!SelectedColorRamp)
						continue;

					// Do not modify the selected parameter if its parent HDA is being cooked
					if (FHoudiniEngineUtils::IsHoudiniAssetComponentCooking(SelectedColorRamp))
						continue;

					if (SelectedColorRamp->IsAutoUpdate())
					{
						// If the selected color ramp is on auto update mode:
						// Since we have pushed all the points of main parameter to the selected,
						// create a Houdini engine manager event to insert a point.

						FHoudiniParameterDetails::CreateColorRampParameterInsertEvent(
							SelectedColorRamp, CurCurvePosition, InsertedColor, EHoudiniRampInterpolationType::LINEAR);

						SelectedColorRamp->MarkChanged(true);
					}
					else
					{
						// If the selected color ramp is on manual update mode:
						// Push a new point to the cached points array
						UHoudiniParameterRampColorPoint* NewCachedPoint = NewObject<UHoudiniParameterRampColorPoint>(SelectedColorRamp, UHoudiniParameterRampColorPoint::StaticClass());
						if (!NewCachedPoint)
							continue;

						NewCachedPoint->Position = CurCurvePosition;
						NewCachedPoint->Value = InsertedColor;
						NewCachedPoint->Interpolation = EHoudiniRampInterpolationType::LINEAR;

						if (Idx >= SelectedColorRamp->CachedPoints.Num())
							SelectedColorRamp->CachedPoints.Add(NewCachedPoint);
						else
							SelectedColorRamp->CachedPoints.Insert(NewCachedPoint, Idx);

						SelectedColorRamp->bCaching = true;
					}
				}

				// Refresh the editor only when the main parameter is on manual update mode and has been modified.
				if (!MainParam->IsAutoUpdate())
					bNeedUpdateEditor = true;

				break;
			}
		}
	}

	// A point's color is changed
	else
	{
		if (bEditing)
			return;

		for (int32 Idx = 0; Idx < NumMainPoints; ++Idx)
		{
			UHoudiniParameterRampColorPoint* MainPoint = MainPoints[Idx];

			if (!MainPoint)
				continue;

			// Only handle color change
			{
				float CurvePosition = FloatCurves[0].Keys[Idx].Time;
				float PointPosition = MainPoint->GetPosition();

				FLinearColor CurveColor = FLinearColor::Black;
				FLinearColor PointColor = MainPoint->GetValue();

				CurveColor.R = FloatCurves[0].Keys[Idx].Value;
				CurveColor.G = FloatCurves[1].Keys[Idx].Value;
				CurveColor.B = FloatCurves[2].Keys[Idx].Value;

				// Color is changed at Idx
				if (CurveColor != PointColor || CurvePosition != PointPosition)
				{
					// Iterate through the all selected color ramp parameters
					for (auto & NextColorRamp : ColorRampParameters)
					{
						if (!NextColorRamp.IsValid())
							continue;

						UHoudiniParameterRampColor* SelectedColorRamp = NextColorRamp.Get();

						if (!SelectedColorRamp)
							continue;

						// Do not modify the selected parameter if its parent HDA is being cooked
						if (FHoudiniEngineUtils::IsHoudiniAssetComponentCooking(SelectedColorRamp))
							continue;

						if (SelectedColorRamp->IsAutoUpdate())
						{
							// The selected color ramp parameter is on auto update mode 

							if (SelectedColorRamp->Points.IsValidIndex(Idx))
							{
								// If the number of synced points in the selected color ramp is more or equal to that in the main parameter:
								// Modify the corresponding synced point of the selected color ramp, and marked it as changed.

								UHoudiniParameterRampColorPoint* Point = SelectedColorRamp->Points[Idx];

								if (!Point)
									continue;

								if (Point->GetValue() != CurveColor && Point->ValueParentParm)
								{
									Point->SetValue(CurveColor);
									Point->ValueParentParm->MarkChanged(true);
								}

								if (Point->GetPosition() != CurvePosition && Point->PositionParentParm)
								{
									Point->SetPosition(CurvePosition);
									Point->PositionParentParm->MarkChanged(true);
								}
							}
							else
							{
								// If the number of synced points in the selected color ramp is less than that in the main parameter:
								// Since we have push the points in the main parameter to all selected parameters, 
								// we need to modify the corresponding insert event.

								int32 IdxInEventsArray = Idx - SelectedColorRamp->Points.Num();

								if (SelectedColorRamp->ModificationEvents.IsValidIndex(IdxInEventsArray))
								{
									UHoudiniParameterRampModificationEvent* Event = SelectedColorRamp->ModificationEvents[IdxInEventsArray];

									if (!Event)
										continue;

									if (Event->InsertColor != CurveColor)
										Event->InsertColor = CurveColor;

									if (Event->InsertPosition != CurvePosition)
										Event->InsertPosition = CurvePosition;
								}
							}
						}
						else
						{
							// The selected color ramp is on manual update mode
							// Since we have push the points in the main parameter to all selected parameters,
							// modify the corresponding point in the cached points array of the selected color ramp.
							if (SelectedColorRamp->CachedPoints.IsValidIndex(Idx))
							{
								UHoudiniParameterRampColorPoint* CachedPoint = SelectedColorRamp->CachedPoints[Idx];

								if (!CachedPoint)
									continue;

								if (CachedPoint->Value != CurveColor)
								{
									CachedPoint->Value = CurveColor;
									bNeedUpdateEditor = true;
								}

								if (CachedPoint->Position != CurvePosition)
								{
									CachedPoint->Position = CurvePosition;
									SelectedColorRamp->bCaching = true;
									bNeedUpdateEditor = true;
								}
							}
						}
					}
				}
			}
		}
	}


	if (bNeedUpdateEditor)
	{
		FHoudiniEngineUtils::UpdateEditorProperties(MainParam, true);
	}
}

UHoudiniFloatCurveEditorParentClass::~UHoudiniFloatCurveEditorParentClass()
{
	if (CurveEditor.IsValid())
		CurveEditor->SetCurveOwner(nullptr);
}

UHoudiniColorCurveEditorParentClass::~UHoudiniColorCurveEditorParentClass() 
{
	if (CurveEditor.IsValid())
		CurveEditor->SetCurveOwner(nullptr);
}


template< class T >
bool FHoudiniParameterDetails::CastParameters(
	TArray<UHoudiniParameter*> InParams, TArray<T*>& OutCastedParams )
{
	for (auto CurrentParam : InParams)
	{
		T* CastedParam = Cast<T>(CurrentParam);
		if (CastedParam && !CastedParam->IsPendingKill())
			OutCastedParams.Add(CastedParam);
	}

	return (OutCastedParams.Num() == InParams.Num());
}


void
FHoudiniParameterDetails::CreateWidget(IDetailCategoryBuilder & HouParameterCategory, TArray<UHoudiniParameter*> &InParams)
{
	if (InParams.Num() <= 0)
		return;

	UHoudiniParameter* InParam = InParams[0];
	if (!InParam || InParam->IsPendingKill())
		return;

	// This parameter is a part of the last float ramp.
	if (CurrentRampFloat) 
	{
		CreateWidgetFloatRamp(HouParameterCategory, InParams);
		return;	
	}
	// This parameter is a part of the last float ramp.
	if (CurrentRampColor) 
	{
		CreateWidgetColorRamp(HouParameterCategory, InParams);
		return;
	}

	// Set the indentation level.
	int32 Indent = GetIndentationLevel(InParam);
	Indentation.Add(InParam->GetParmId(), Indent);

	switch (InParam->GetParameterType())
	{
		case EHoudiniParameterType::Float:
		{				
			CreateWidgetFloat(HouParameterCategory, InParams);
		}
		break;

		case EHoudiniParameterType::Int:
		{
			CreateWidgetInt(HouParameterCategory, InParams);
		}
		break;

		case EHoudiniParameterType::String:
		{
			CreateWidgetString(HouParameterCategory, InParams);
		}
		break;

		case EHoudiniParameterType::IntChoice:
		case EHoudiniParameterType::StringChoice:
		{
			CreateWidgetChoice(HouParameterCategory, InParams);
		}
		break;

		case EHoudiniParameterType::Separator:
		{
			TArray<UHoudiniParameterSeparator*> SepParams;
			if (CastParameters<UHoudiniParameterSeparator>(InParams, SepParams))
			{
				bool bEnabled = InParams.IsValidIndex(0) ? !SepParams[0]->IsDisabled() : true;
				CreateWidgetSeparator(HouParameterCategory, InParams, bEnabled);
			}
		}
		break;

		case EHoudiniParameterType::Color:
		{
			CreateWidgetColor(HouParameterCategory, InParams);	
		}
		break;

		case EHoudiniParameterType::Button:
		{
			CreateWidgetButton(HouParameterCategory, InParams);
		}
		break;

		case EHoudiniParameterType::ButtonStrip:
		{
			CreateWidgetButtonStrip(HouParameterCategory, InParams);
		}
		break;

		case EHoudiniParameterType::Label:
		{
			CreateWidgetLabel(HouParameterCategory, InParams);
		}
		break;

		case EHoudiniParameterType::Toggle:
		{
			CreateWidgetToggle(HouParameterCategory, InParams);
		}
		break;

		case EHoudiniParameterType::File:
		case EHoudiniParameterType::FileDir:
		case EHoudiniParameterType::FileGeo:
		case EHoudiniParameterType::FileImage:
		{
			CreateWidgetFile(HouParameterCategory, InParams);
		}
		break;

		case EHoudiniParameterType::FolderList: 
		{	
			CreateWidgetFolderList(HouParameterCategory, InParams);
		}
		break;

		case EHoudiniParameterType::Folder: 
		{
			CreateWidgetFolder(HouParameterCategory, InParams);
		}
		break;

		case EHoudiniParameterType::MultiParm: 
		{
			CreateWidgetMultiParm(HouParameterCategory, InParams);
		}
		break;

		case EHoudiniParameterType::FloatRamp: 
		{
			CreateWidgetFloatRamp(HouParameterCategory, InParams);
		}
		break;

		case EHoudiniParameterType::ColorRamp: 
		{
			CreateWidgetColorRamp(HouParameterCategory, InParams);
		}
		break;

		case EHoudiniParameterType::Input:
		{
			CreateWidgetOperatorPath(HouParameterCategory, InParams);
		}
		break;

		case EHoudiniParameterType::Invalid:
		{
			HandleUnsupportedParmType(HouParameterCategory, InParams);
		}
		break;

		default:
		{
			HandleUnsupportedParmType(HouParameterCategory, InParams);
		}
		break;
	}
}

void
FHoudiniParameterDetails::CreateNameWidget(FDetailWidgetRow* Row, TArray<UHoudiniParameter*> &InParams, bool WithLabel)
{
	if (InParams.Num() <= 0)
		return;

	UHoudiniParameter* MainParam = InParams[0];
	if (!MainParam|| MainParam->IsPendingKill())
		return;

	if (!Row)
		return;

	FString IndentationStr = GetIndentationString(MainParam);

	FString ParameterLabelStr = FString("");

	TSharedRef<SHorizontalBox> HorizontalBox = SNew(SHorizontalBox);
	if (MainParam->IsDirectChildOfMultiParm()) 
	{
		ParameterLabelStr += MainParam->GetParameterLabel();

		// Add Indentation space holder.
		HorizontalBox->AddSlot()
		.AutoWidth()
		.Padding(2.0f, 0.0f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(IndentationStr))
		];

		// If it is head of an multiparm instance
		if (MainParam->GetChildIndex() == 0)
		{

			int32 CurrentMultiParmInstanceIndex = 0;
			if (MultiParmInstanceIndices.Contains(MainParam->GetParentParmId()))
			{
				MultiParmInstanceIndices[MainParam->GetParentParmId()] += 1;
				CurrentMultiParmInstanceIndex = MultiParmInstanceIndices[MainParam->GetParentParmId()];
			}
			ParameterLabelStr += TEXT(" (") + FString("") + FString::FromInt(CurrentMultiParmInstanceIndex + 1) + TEXT(")");

			CreateWidgetMultiParmObjectButtons(HorizontalBox, InParams);
		}


		const FText & FinalParameterLabelText = WithLabel ? FText::FromString(ParameterLabelStr) : FText::GetEmpty();
		HorizontalBox->AddSlot()
		.AutoWidth()
		.HAlign(HAlign_Left)
		.Padding(2.0f, 0.0f)
		[
			SNew(STextBlock)
			.Text(FinalParameterLabelText)
			.ToolTipText(GetParameterTooltip(MainParam))
			.Font(FEditorStyle::GetFontStyle(MainParam->IsDefault() ? TEXT("PropertyWindow.NormalFont") : TEXT("PropertyWindow.BoldFont")))
		];
	}
	else 
	{
		ParameterLabelStr = ParameterLabelStr + IndentationStr + MainParam->GetParameterLabel();
		const FText & FinalParameterLabelText = WithLabel ? FText::FromString(ParameterLabelStr) : FText::GetEmpty();
		HorizontalBox->AddSlot().AutoWidth().VAlign(VAlign_Top)
		[
			SNew(STextBlock)
			.Text(FinalParameterLabelText)
			.ToolTipText(GetParameterTooltip(MainParam))
			.Font(FEditorStyle::GetFontStyle(MainParam->IsDefault() ? TEXT("PropertyWindow.NormalFont") : TEXT("PropertyWindow.BoldFont")))
		];
	}

	Row->NameWidget.Widget = HorizontalBox;
}

void
FHoudiniParameterDetails::CreateNameWidgetWithAutoUpdate(FDetailWidgetRow* Row, TArray<UHoudiniParameter*> &InParams, bool WithLabel)
{
	if (InParams.Num() <= 0)
		return;

	UHoudiniParameter* MainParam = InParams[0];
	if (!MainParam || MainParam->IsPendingKill())
		return;

	if (!Row)
		return;

	FString IndentationStr = GetIndentationString(MainParam);
	FString ParameterLabelStr = FString("");
	TSharedRef<SHorizontalBox> HorizontalBox = SNew(SHorizontalBox);
	TSharedRef<SVerticalBox> VerticalBox = SNew(SVerticalBox);
	if (MainParam->IsDirectChildOfMultiParm())
	{
		ParameterLabelStr += MainParam->GetParameterLabel();

		// Add Indentation space holder.
		HorizontalBox->AddSlot()
		.AutoWidth()
		.Padding(2.0f, 0.0f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(IndentationStr))
		];

		// If it is head of an multiparm instance
		if (MainParam->GetChildIndex() == 0)
		{
			int32 CurrentMultiParmInstanceIndex = 0;
			if (MultiParmInstanceIndices.Contains(MainParam->GetParentParmId()))
			{
				MultiParmInstanceIndices[MainParam->GetParentParmId()] += 1;
				CurrentMultiParmInstanceIndex = MultiParmInstanceIndices[MainParam->GetParentParmId()];
			}
			ParameterLabelStr += TEXT(" (") + FString("") + FString::FromInt(CurrentMultiParmInstanceIndex + 1) + TEXT(")");

			CreateWidgetMultiParmObjectButtons(HorizontalBox, InParams);
		}

		if (MainParam->GetParameterType() == EHoudiniParameterType::ColorRamp)
		{
			UHoudiniParameterRampColor* RampParameter = Cast<UHoudiniParameterRampColor>(MainParam);
			if (RampParameter)
			{
				if (RampParameter->bCaching)
					ParameterLabelStr += "*";
			}
		}

		const FText & FinalParameterLabelText = WithLabel ? FText::FromString(ParameterLabelStr) : FText::GetEmpty();
		HorizontalBox->AddSlot()
		.AutoWidth()
		.Padding(2.0f, 0.0f)
		[
			SAssignNew(VerticalBox, SVerticalBox)
		];

		VerticalBox->AddSlot()
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Center)
		[
			SNew(STextBlock)
			.Text(FinalParameterLabelText)
			.ToolTipText(GetParameterTooltip(MainParam))
			.Font(FEditorStyle::GetFontStyle(MainParam->IsDefault() ? TEXT("PropertyWindow.NormalFont") : TEXT("PropertyWindow.BoldFont")))
		];
	}
	else
	{
		// TODO: Refactor me...extend 'auto/manual update' to all parameters? (It only applies to color and float ramps for now.)
		ParameterLabelStr = ParameterLabelStr + IndentationStr + MainParam->GetParameterLabel();

		bool bParamNeedUpdate = false;
		if (MainParam->GetParameterType() == EHoudiniParameterType::ColorRamp)
		{
			UHoudiniParameterRampColor* RampParameter = Cast<UHoudiniParameterRampColor>(MainParam);
			if (RampParameter)
				bParamNeedUpdate = RampParameter->bCaching;
		}
		else if (MainParam->GetParameterType() == EHoudiniParameterType::FloatRamp)
		{
			UHoudiniParameterRampFloat* RampParameter = Cast<UHoudiniParameterRampFloat>(MainParam);
			if (RampParameter)
				bParamNeedUpdate = RampParameter->bCaching;
		}

		if (bParamNeedUpdate)
			ParameterLabelStr += "*";

		const FText & FinalParameterLabelText = WithLabel ? FText::FromString(ParameterLabelStr) : FText::GetEmpty();
		HorizontalBox->AddSlot()
		.AutoWidth()
		.VAlign(VAlign_Top)
		[
			SAssignNew(VerticalBox, SVerticalBox)
		];

		VerticalBox->AddSlot()
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Center)
		[
			SNew(STextBlock)
			.Text(FinalParameterLabelText)
			.ToolTipText(GetParameterTooltip(MainParam))
			.Font(FEditorStyle::GetFontStyle(MainParam->IsDefault() ? TEXT("PropertyWindow.NormalFont") : TEXT("PropertyWindow.BoldFont")))
		];
	}

	auto IsAutoUpdateChecked = [MainParam]()
	{
		if (!MainParam || MainParam->IsPendingKill())
			return ECheckBoxState::Unchecked;

		return MainParam->IsAutoUpdate() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	};

	auto OnAutoUpdateCheckBoxStateChanged = [MainParam, InParams](ECheckBoxState NewState)
	{
		if (NewState == ECheckBoxState::Checked)
		{
			for (auto & NextSelectedParam : InParams)
			{
				if (!NextSelectedParam)
					continue;

				if (NextSelectedParam->IsAutoUpdate())
					continue;

				// Do not allow mode change when the Houdini asset component is cooking
				if (FHoudiniEngineUtils::IsHoudiniAssetComponentCooking(NextSelectedParam))
					continue;

				switch (MainParam->GetParameterType())
				{
					case EHoudiniParameterType::ColorRamp:
					{
						UHoudiniParameterRampColor* ColorRampParameter = Cast<UHoudiniParameterRampColor>(NextSelectedParam);

						if (!ColorRampParameter)
							continue;

						// Do not sync the selected color ramp parameter if its parent HDA is being cooked
						if (FHoudiniEngineUtils::IsHoudiniAssetComponentCooking(ColorRampParameter))
							continue;

						// Sync the Cached curve points at update mode switch.
						FHoudiniParameterDetails::SyncCachedColorRampPoints(ColorRampParameter);
					}
					break;

					case EHoudiniParameterType::FloatRamp:
					{
						UHoudiniParameterRampFloat* FloatRampParameter = Cast<UHoudiniParameterRampFloat>(NextSelectedParam);

						if (!FloatRampParameter)
							continue;

						// Do not sync the selected float ramp parameter if its parent HDA is being cooked
						if (FHoudiniEngineUtils::IsHoudiniAssetComponentCooking(FloatRampParameter))
							continue;

						// Sync the Cached curve points at update mode switch.
						FHoudiniParameterDetails::SyncCachedFloatRampPoints(FloatRampParameter);
					}
					break;

					default:
						break;
				}

				NextSelectedParam->SetAutoUpdate(true);
			}
		}
		else
		{
			for (auto & NextSelectedParam : InParams)
			{
				if (!NextSelectedParam)
					continue;

				if (!NextSelectedParam->IsAutoUpdate())
					continue;

				// Do not allow mode change when the Houdini asset component is cooking
				if (FHoudiniEngineUtils::IsHoudiniAssetComponentCooking(NextSelectedParam))
					continue;

				NextSelectedParam->SetAutoUpdate(false);
			}
		}
	};

	// Auto update check box
	TSharedPtr<SCheckBox> CheckBox;
	VerticalBox->AddSlot()
	//.VAlign(VAlign_Center)
	//.HAlign(HAlign_Center)
	[
		SAssignNew(CheckBox, SCheckBox)
		.OnCheckStateChanged_Lambda([OnAutoUpdateCheckBoxStateChanged](ECheckBoxState NewState)
		{
			OnAutoUpdateCheckBoxStateChanged(NewState);
		})
		.IsChecked_Lambda([IsAutoUpdateChecked]()
		{
			return IsAutoUpdateChecked();
		})
		.Content()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("AutoUpdate", "Auto-update"))
			.ToolTipText(LOCTEXT("AutoUpdateTip", "When enabled, this parameter will automatically update its value while editing. Turning this off will allow you to more easily update it, and the update can be pushed by checking the toggle again."))
			.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
		]
	];

	if ((MainParam->GetParameterType() != EHoudiniParameterType::FloatRamp) && (MainParam->GetParameterType() != EHoudiniParameterType::ColorRamp))
		CheckBox->SetVisibility(EVisibility::Hidden);

	Row->NameWidget.Widget = HorizontalBox;
}


FDetailWidgetRow*
FHoudiniParameterDetails::CreateNestedRow(IDetailCategoryBuilder & HouParameterCategory, TArray<UHoudiniParameter*> InParams, bool bDecreaseChildCount)
{
	if (InParams.Num() <= 0)
		return nullptr;

	UHoudiniParameter* MainParam = InParams[0];

	if (!MainParam || MainParam->IsPendingKill())
		return nullptr;

	// Created row for the current parameter, if there is not a row created, do not display the parameter.
	FDetailWidgetRow* Row = nullptr;

	// Current parameter is in a multiparm instance (directly)
	if (MainParam->IsDirectChildOfMultiParm())
	{
		int ParentMultiParmId = MainParam->GetParentParmId();

		if (!AllMultiParms.Contains(ParentMultiParmId)) // error state
			return nullptr;

		UHoudiniParameterMultiParm* ParentMultiParm = AllMultiParms[ParentMultiParmId];

		// The parent multiparm is visible.
		if (ParentMultiParm && ParentMultiParm->IsShown() && MainParam->ShouldDisplay())
		{
			if (MainParam->GetParameterType() == EHoudiniParameterType::FolderList && MainParam->GetTupleSize() > 1)
				CreateWidgetTabMenu(HouParameterCategory, Row, InParams);
			else
				Row = &(HouParameterCategory.AddCustomRow(FText::GetEmpty()));			
		}
	}
	// This item is not a direct child of a multiparm.
	else
	{
		bool bIsFolder = MainParam->GetParameterType() == EHoudiniParameterType::Folder;
		int32 NestedMinStackDepth = bIsFolder ? 1 : 0;

		// Current parameter is inside a folder.
		if (FolderChildCounterStack.Num() > NestedMinStackDepth)
		{
			// If the current parameter is a folder, we take the top second queue on the stack, since the top one represents itself.
			// Otherwise take the top queue on the stack.

			TArray<bool> & CurrentLayerFolderQueue = bIsFolder ? 
				FolderStack[FolderStack.Num() - 2] : FolderStack.Last();

			TArray<int32> & CurrentLayerChildrenCounterQueue = bIsFolder ? 
				FolderChildCounterStack[FolderChildCounterStack.Num() - 2] : FolderChildCounterStack.Last();

			if (CurrentLayerChildrenCounterQueue.Num() <= 0 || CurrentLayerFolderQueue.Num() <= 0) // Error state
				return nullptr;

			bool ParentFolderVisible = CurrentLayerFolderQueue[0];

			// If its parent folder is visible, display current parameter,
			// Otherwise just prune the stacks.
			if (ParentFolderVisible)
			{
				int32 ParentFolderId = MainParam->GetParentParmId();

				// If the current parameter is a folder, its parent is a folderlist.
				// So we need to continue to get the parent of the folderlist.
				if (MainParam->GetParameterType() == EHoudiniParameterType::Folder) 
				{
					if (AllFoldersAndTabs.Contains(ParentFolderId))
						ParentFolderId = AllFoldersAndTabs[ParentFolderId]->GetParentParmId();
					else
						return nullptr;   // error state
				}

				UHoudiniParameterFolder* ParentFolder = nullptr;

				if (AllFoldersAndTabs.Contains(ParentFolderId))
					ParentFolder = Cast<UHoudiniParameterFolder>(AllFoldersAndTabs[ParentFolderId]);

				bool bShouldDisplayRow = MainParam->ShouldDisplay();

				if (ParentFolder)
					bShouldDisplayRow &= (ParentFolder->IsTab() && ParentFolder->IsChosen()) || (!ParentFolder->IsTab() && ParentFolder->IsExpanded());

				if (bShouldDisplayRow)
				{
					if (MainParam->GetParameterType() == EHoudiniParameterType::FolderList && MainParam->GetTupleSize() > 1)
						CreateWidgetTabMenu(HouParameterCategory, Row, InParams);
					else
						Row = &(HouParameterCategory.AddCustomRow(FText::GetEmpty()));					
				}
			}

			if (bDecreaseChildCount)
			{
				CurrentLayerChildrenCounterQueue[0] -= 1;
				PruneStack();
			}
		}
		// This parameter is in the root dir.
		else
		{
			if (MainParam->ShouldDisplay())
			{
				if (MainParam->GetParameterType() == EHoudiniParameterType::FolderList && MainParam->GetTupleSize() > 1)
					CreateWidgetTabMenu(HouParameterCategory, Row, InParams);
				else
					Row = &(HouParameterCategory.AddCustomRow(FText::GetEmpty()));
			}
		}
	}

	if (!MainParam->IsVisible())
		return nullptr;

	return Row;
}

void
FHoudiniParameterDetails::HandleUnsupportedParmType(IDetailCategoryBuilder & HouParameterCategory, TArray<UHoudiniParameter*> &InParams)
{
	if (InParams.Num() < 1)
		return;

	UHoudiniParameter* MainParam = InParams[0];
	if (!MainParam || MainParam->IsPendingKill())
		return;
	
	CreateNestedRow(HouParameterCategory, (TArray<UHoudiniParameter*>)InParams);
}

void
FHoudiniParameterDetails::CreateWidgetFloat(
	IDetailCategoryBuilder & HouParameterCategory,
	TArray<UHoudiniParameter*>& InParams )
{
	TArray<UHoudiniParameterFloat*> FloatParams;
	if (!CastParameters<UHoudiniParameterFloat>(InParams, FloatParams))
		return;

	if (FloatParams.Num() <= 0)
		return;

	UHoudiniParameterFloat* MainParam = FloatParams[0];
	if (!MainParam || MainParam->IsPendingKill())
		return;

	// Create a new detail row
	FDetailWidgetRow* Row = CreateNestedRow(HouParameterCategory, InParams);

	if (!Row)
		return;

	// Create the standard parameter name widget.
	CreateNameWidget(Row, InParams, true);

	// Helper function to find a unit from a string (name or abbreviation) 
	auto ParmUnit = FUnitConversion::UnitFromString(*(MainParam->GetUnit()));
	EUnit Unit = EUnit::Unspecified;
	if (FUnitConversion::Settings().ShouldDisplayUnits() && ParmUnit.IsSet())
		Unit = ParmUnit.GetValue();

	TSharedPtr<INumericTypeInterface<float>> paramTypeInterface;
	paramTypeInterface = MakeShareable(new TNumericUnitTypeInterface<float>(Unit));
	
	// Lambdas for slider begin
	auto SliderBegin = [&](TArray<UHoudiniParameterFloat*> FloatParams)
	{
		// Record a transaction for undo/redo
		FScopedTransaction Transaction(
			TEXT(HOUDINI_MODULE_RUNTIME),
			LOCTEXT("HoudiniParameterFloatChange", "Houdini Parameter Float: Changing a value"),
			FloatParams[0]->GetOuter());

		for (int Idx = 0; Idx < FloatParams.Num(); Idx++)
		{
			FloatParams[Idx]->Modify();
		}
	};

	// Lambdas for slider end
	auto SliderEnd = [&](TArray<UHoudiniParameterFloat*> FloatParams)
	{
		// Mark the value as changed to trigger an update
		for (int Idx = 0; Idx < FloatParams.Num(); Idx++)
		{
			FloatParams[Idx]->MarkChanged(true);
		}
	};

	// Lambdas for changing the parameter value
	auto ChangeFloatValueAt = [&](const float& Value, const int32& Index, const bool& DoChange, TArray<UHoudiniParameterFloat*> FloatParams)
	{
		// Record a transaction for undo/redo
		FScopedTransaction Transaction(
			TEXT(HOUDINI_MODULE_RUNTIME),
			LOCTEXT("HoudiniParameterFloatChange", "Houdini Parameter Float: Changing a value"),
			FloatParams[0]->GetOuter() );

		bool bChanged = false;
		for (int Idx = 0; Idx < FloatParams.Num(); Idx++)
		{
			if (!FloatParams[Idx])
				continue;

			FloatParams[Idx]->Modify();
			if (FloatParams[Idx]->SetValueAt(Value, Index))
			{
				// Only mark the param has changed if DoChange is true!!!
				if(DoChange)
					FloatParams[Idx]->MarkChanged(true);
				bChanged = true;
			}
		}

		if (!bChanged || !DoChange)
		{
			// Cancel the transaction if no parameter's value has actually been changed
			Transaction.Cancel();
		}		
	};

	auto RevertToDefault = [&](const int32& TupleIndex, TArray<UHoudiniParameterFloat*> FloatParams)
	{
		// Record a transaction for undo/redo
		FScopedTransaction Transaction(
			TEXT(HOUDINI_MODULE_RUNTIME),
			LOCTEXT("HoudiniParameterFloatChange", "Houdini Parameter Float: Revert to default value"),
			FloatParams[0]->GetOuter());

		if (TupleIndex < 0) 
		{
			for (int32 Idx = 0; Idx < FloatParams.Num(); Idx++) 
			{
				if (!FloatParams[Idx])
					continue;

				if (FloatParams[Idx]->IsDefault())
					continue;

				FloatParams[Idx]->RevertToDefault(-1);
			}
		}
		else
		{
			for (int32 Idx = 0; Idx < FloatParams.Num(); Idx++)
			{
				if (!FloatParams[Idx])
					continue;

				if (FloatParams[Idx]->IsDefaultValueAtIndex(TupleIndex))
					continue;

				FloatParams[Idx]->RevertToDefault(TupleIndex);
			}
		}
		return FReply::Handled();
	};
	
	TSharedRef< SVerticalBox > VerticalBox = SNew(SVerticalBox);
	if (MainParam->GetTupleSize() == 3)
	{
		// Should we swap Y and Z fields (only relevant for Vector3)
		// Ignore the swapping if that parameter has the noswap tag
		bool SwapVector3 = !MainParam->GetNoSwap();
		VerticalBox->AddSlot().Padding(2, 2, 5, 2)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(SVectorInputBox)
				.bColorAxisLabels(true)
				.X(TAttribute<TOptional<float>>::Create(TAttribute<TOptional<float>>::FGetter::CreateUObject(MainParam, &UHoudiniParameterFloat::GetValue, 0)))
				.Y(TAttribute<TOptional<float>>::Create(TAttribute<TOptional<float>>::FGetter::CreateUObject(MainParam, &UHoudiniParameterFloat::GetValue, SwapVector3 ? 2 : 1)))
				.Z(TAttribute<TOptional<float>>::Create(TAttribute<TOptional<float>>::FGetter::CreateUObject(MainParam, &UHoudiniParameterFloat::GetValue, SwapVector3 ? 1 : 2)))
				.OnXCommitted_Lambda( [=](float Val, ETextCommit::Type TextCommitType) { ChangeFloatValueAt( Val, 0, true, FloatParams); } )
				.OnYCommitted_Lambda( [=](float Val, ETextCommit::Type TextCommitType) { ChangeFloatValueAt( Val, SwapVector3 ? 2 : 1, true, FloatParams); } )
				.OnZCommitted_Lambda( [=](float Val, ETextCommit::Type TextCommitType) { ChangeFloatValueAt( Val, SwapVector3 ? 1 : 2, true, FloatParams); } )
				.TypeInterface(paramTypeInterface)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f, 0.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SButton)
				.ToolTipText(LOCTEXT("RevertToDefault", "Revert to default"))
				.ButtonStyle(FEditorStyle::Get(), "NoBorder")
				.ContentPadding(0)
				.Visibility_Lambda([FloatParams]()
				{
					for (auto & SelectedParam : FloatParams) 
					{
						if (!SelectedParam)
							continue;

						if (!SelectedParam->IsDefault())
							return EVisibility::Visible;
					}

					return EVisibility::Hidden;
				})
				.OnClicked_Lambda([FloatParams, RevertToDefault]() { return RevertToDefault(-1, FloatParams); })
				[
					SNew(SImage)
					.Image(FEditorStyle::GetBrush("PropertyWindow.DiffersFromDefault"))
				]
			]
		];
	}
	else
	{
		for (int32 Idx = 0; Idx < MainParam->GetTupleSize(); ++Idx)
		{
			TSharedPtr<SNumericEntryBox<float>> NumericEntryBox;
			VerticalBox->AddSlot()
			.Padding(2, 2, 5, 2)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.0f)
				[
					SAssignNew(NumericEntryBox, SNumericEntryBox< float >)
					.AllowSpin(true)

					.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))

					.MinValue(MainParam->GetMin())
					.MaxValue(MainParam->GetMax())

					.MinSliderValue(MainParam->GetUIMin())
					.MaxSliderValue(MainParam->GetUIMax())

					.Value(TAttribute<TOptional<float>>::Create(TAttribute<TOptional<float>>::FGetter::CreateUObject(MainParam, &UHoudiniParameterFloat::GetValue, Idx)))
					.OnValueChanged_Lambda([=](float Val) { ChangeFloatValueAt(Val, Idx, false, FloatParams); })
					.OnValueCommitted_Lambda([=](float Val, ETextCommit::Type TextCommitType) {	ChangeFloatValueAt(Val, Idx, true, FloatParams); })
					.OnBeginSliderMovement_Lambda([=]() { SliderBegin(FloatParams); })
					.OnEndSliderMovement_Lambda([=](const float NewValue) { SliderEnd(FloatParams); })
					.SliderExponent(1.0f)
					.TypeInterface(paramTypeInterface)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(2.0f, 0.0f)
				.VAlign(VAlign_Center)
				[
					SNew(SButton)
					.ToolTipText(LOCTEXT("RevertToDefault", "Revert to default"))
					.ButtonStyle(FEditorStyle::Get(), "NoBorder")
					.ContentPadding(0)
					.OnClicked_Lambda([Idx, FloatParams, RevertToDefault]() { return RevertToDefault(Idx, FloatParams); })
					.Visibility_Lambda([Idx, FloatParams]()
					{
						for (auto & SelectedParam :FloatParams)
						{
							if (!SelectedParam)
								continue;

							if (!SelectedParam->IsDefaultValueAtIndex(Idx))
								return EVisibility::Visible;
						}

						return EVisibility::Hidden;
					})
					[
						SNew(SImage)
						.Image(FEditorStyle::GetBrush("PropertyWindow.DiffersFromDefault"))
					]
				]
			];
		}
	}

	Row->ValueWidget.Widget = VerticalBox;

	Row->ValueWidget.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH);
	Row->ValueWidget.Widget->SetEnabled(!MainParam->IsDisabled());

}

void
FHoudiniParameterDetails::CreateWidgetInt(IDetailCategoryBuilder & HouParameterCategory, TArray<UHoudiniParameter*>& InParams)
{
	TArray<UHoudiniParameterInt*> IntParams;
	if (!CastParameters<UHoudiniParameterInt>(InParams, IntParams))

	if (IntParams.Num() <= 0)
		return;

	UHoudiniParameterInt* MainParam = IntParams[0];
	if (!MainParam || MainParam->IsPendingKill())
		return;

	// Create a new detail row
	FDetailWidgetRow* Row = CreateNestedRow(HouParameterCategory, InParams);

	if (!Row)
		return;

	// Create the standard parameter name widget.
	CreateNameWidget(Row, InParams, true);

	TSharedRef< SVerticalBox > VerticalBox = SNew(SVerticalBox);

	// Helper function to find a unit from a string (name or abbreviation) 
	auto ParmUnit = FUnitConversion::UnitFromString(*(MainParam->GetUnit()));
	EUnit Unit = EUnit::Unspecified;
	if (FUnitConversion::Settings().ShouldDisplayUnits() && ParmUnit.IsSet())
		Unit = ParmUnit.GetValue();

	TSharedPtr<INumericTypeInterface<int32>> paramTypeInterface;
	paramTypeInterface = MakeShareable(new TNumericUnitTypeInterface<int32>(Unit));

	// Lambda for slider begin
	auto SliderBegin = [&](TArray<UHoudiniParameterInt*> IntParams)
	{
		// Record a transaction for undo/redo
		FScopedTransaction Transaction(
			TEXT(HOUDINI_MODULE_RUNTIME),
			LOCTEXT("HoudiniParameterIntChange", "Houdini Parameter Int: Changing a value"),
			IntParams[0]->GetOuter());

		for (int Idx = 0; Idx < IntParams.Num(); Idx++)
		{
			IntParams[Idx]->Modify();
		}
	};
	
	// Lambda for slider end
	auto SliderEnd = [&](TArray<UHoudiniParameterInt*> IntParams)
	{
		// Mark the value as changed to trigger an update
		for (int Idx = 0; Idx < IntParams.Num(); Idx++)
		{
			IntParams[Idx]->MarkChanged(true);
		}
	};
	
	// Lambda for changing the parameter value
	auto ChangeIntValueAt = [&](const int32& Value, const int32& Index, const bool& DoChange, TArray<UHoudiniParameterInt*> IntParams)
	{
		FScopedTransaction Transaction(
			TEXT(HOUDINI_MODULE_RUNTIME),
			LOCTEXT("HoudiniParameterIntChange", "Houdini Parameter Int: Changing a value"),
			IntParams[0]->GetOuter());

		bool bChanged = false;
		for (int Idx = 0; Idx < IntParams.Num(); Idx++)
		{
			if (!IntParams[Idx])
				continue;

			IntParams[Idx]->Modify();
			if (IntParams[Idx]->SetValueAt(Value, Index)) 
			{
				// Only mark the param has changed if DoChange is true!!!
				if (DoChange)
					IntParams[Idx]->MarkChanged(true);
				bChanged = true;
			}
		}

		if (!bChanged || !DoChange)
		{
			// Cancel the transaction if there is no param has actually been changed
			Transaction.Cancel();
		}
	};

	auto RevertToDefault = [&](const int32& TupleIndex, TArray<UHoudiniParameterInt*> IntParams)
	{
		for (int32 Idx = 0; Idx < IntParams.Num(); Idx++) 
		{
			if (!IntParams[Idx])
				continue;

			if (IntParams[Idx]->IsDefaultValueAtIndex(TupleIndex))
				continue;

			IntParams[Idx]->RevertToDefault(TupleIndex);
		}
			
		return FReply::Handled();
	};

	for (int32 Idx = 0; Idx < MainParam->GetTupleSize(); ++Idx)
	{
		TSharedPtr< SNumericEntryBox< int32 > > NumericEntryBox;
		VerticalBox->AddSlot().Padding(2, 2, 5, 2)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SAssignNew(NumericEntryBox, SNumericEntryBox< int32 >)
				.AllowSpin(true)

				.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))

				.MinValue(MainParam->GetMin())
				.MaxValue(MainParam->GetMax())

				.MinSliderValue(MainParam->GetUIMin())
				.MaxSliderValue(MainParam->GetUIMax())

				.Value( TAttribute<TOptional<int32>>::Create(TAttribute<TOptional<int32>>::FGetter::CreateUObject(MainParam, &UHoudiniParameterInt::GetValue, Idx)))
				.OnValueChanged_Lambda( [=](int32 Val) { ChangeIntValueAt(Val, Idx, false, IntParams); } )
				.OnValueCommitted_Lambda([=](float Val, ETextCommit::Type TextCommitType) { ChangeIntValueAt(Val, Idx, true, IntParams); })
				.OnBeginSliderMovement_Lambda( [=]() { SliderBegin(IntParams); })
				.OnEndSliderMovement_Lambda([=](const float NewValue) { SliderEnd(IntParams); })
				.SliderExponent(1.0f)
				.TypeInterface(paramTypeInterface)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f, 0.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SButton)
				.ToolTipText(LOCTEXT("RevertToDefault", "Revert to default"))
				.ButtonStyle(FEditorStyle::Get(), "NoBorder")
				.ContentPadding(0)
				.Visibility_Lambda([Idx, IntParams]()
				{
					for (auto & NextSelectedParam : IntParams) 
					{
						if (!NextSelectedParam)
							continue;
	
						if (!NextSelectedParam->IsDefaultValueAtIndex(Idx))
							return EVisibility::Visible;
					}

					return EVisibility::Hidden;
				})
				.OnClicked_Lambda([Idx, IntParams, RevertToDefault]() { return RevertToDefault(Idx, IntParams); })
				[
					SNew(SImage)
					.Image(FEditorStyle::GetBrush("PropertyWindow.DiffersFromDefault"))
				]
			]
		];
		/*
		if (NumericEntryBox.IsValid())
			NumericEntryBox->SetEnabled(!MainParam->IsDisabled());
		*/
	}

	Row->ValueWidget.Widget = VerticalBox;
	Row->ValueWidget.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH);
	Row->ValueWidget.Widget->SetEnabled(!MainParam->IsDisabled());

}

void
FHoudiniParameterDetails::CreateWidgetString( IDetailCategoryBuilder & HouParameterCategory, TArray<UHoudiniParameter*>& InParams)
{
	TArray<UHoudiniParameterString*> StringParams;
	if (!CastParameters<UHoudiniParameterString>(InParams, StringParams))
		return;

	if (StringParams.Num() <= 0)
		return;
	
	UHoudiniParameterString* MainParam = StringParams[0];
	if (!MainParam || MainParam->IsPendingKill())
		return;

	// Create a new detail row
	FDetailWidgetRow* Row = CreateNestedRow(HouParameterCategory, InParams);

	if (!Row)
		return;

	bool bIsMultiLine = false;
	bool bIsUnrealRef = false;

	// Create the standard parameter name widget.
	CreateNameWidget(Row, InParams, true);
	TSharedRef< SVerticalBox > VerticalBox = SNew(SVerticalBox);

	TMap<FString, FString>& Tags = MainParam->GetTags();
	if (Tags.Contains(HOUDINI_PARAMETER_STRING_REF_TAG) && FCString::Atoi(*Tags[HOUDINI_PARAMETER_STRING_REF_TAG]) == 1) 
	{
		bIsUnrealRef = true;
	}

	if (Tags.Contains(HOUDINI_PARAMETER_STRING_MULTILINE_TAG)) 
	{
		bIsMultiLine = true;
	}

	for (int32 Idx = 0; Idx < MainParam->GetTupleSize(); ++Idx)
	{
		// Lambda for changing the parameter value
		auto ChangeStringValueAt = [&](const FString& Value, UObject* ChosenObj, const int32& Index, const bool& DoChange, TArray<UHoudiniParameterString*> StringParams)
		{
			FScopedTransaction Transaction(
				TEXT(HOUDINI_MODULE_RUNTIME),
				LOCTEXT("HoudiniParameterSrtingChange", "Houdini Parameter String: Changing a value"),
				StringParams[0]->GetOuter());

			bool bChanged = false;
			for (int Idx = 0; Idx < StringParams.Num(); Idx++)
			{
				if (!StringParams[Idx])
					continue;

				StringParams[Idx]->Modify();
				if (StringParams[Idx]->SetValueAt(Value, Index)) 
				{
					StringParams[Idx]->MarkChanged(true);
					bChanged = true;
				}

				StringParams[Idx]->SetAssetAt(ChosenObj, Index);
			}

			if (!bChanged || !DoChange)
			{
				// Cancel the transaction if there is no param actually has been changed
				Transaction.Cancel();
			}

			FHoudiniEngineUtils::UpdateEditorProperties(StringParams[0], false);
		};

		auto RevertToDefault = [&](const int32& TupleIndex, TArray<UHoudiniParameterString*> StringParams)
		{
			for (int32 Idx = 0; Idx < StringParams.Num(); Idx++) 
			{
				if (!StringParams[Idx])
					continue;

				if (StringParams[Idx]->IsDefaultValueAtIndex(TupleIndex))
					continue;

				StringParams[Idx]->RevertToDefault(TupleIndex);
			}
				
			return FReply::Handled();
		};

		if (bIsUnrealRef)
		{
			TSharedPtr< SEditableTextBox > EditableTextBox;
			TSharedPtr< SHorizontalBox > HorizontalBox;
			VerticalBox->AddSlot().Padding(2, 2, 5, 2)
			[
				SNew(SAssetDropTarget)
				.OnIsAssetAcceptableForDrop_Lambda([](const UObject* InObject)
					{return true; })
				.OnAssetDropped_Lambda([=](UObject* InObject)
				{
					// Get the asset reference string for this object
					FString ReferenceStr = UHoudiniParameterString::GetAssetReference(InObject);

					ChangeStringValueAt(ReferenceStr, nullptr, Idx, true, StringParams);
				})
				[
					SAssignNew(HorizontalBox, SHorizontalBox)
				]
			];

			// Thumbnail
			// Get thumbnail pool for this builder.
			TSharedPtr< FAssetThumbnailPool > AssetThumbnailPool = HouParameterCategory.GetParentLayout().GetThumbnailPool();

			UObject* EditObject = MainParam->GetAssetAt(Idx);
			TSharedPtr< FAssetThumbnail > StaticMeshThumbnail = MakeShareable(
				new FAssetThumbnail(EditObject, 64, 64, AssetThumbnailPool));

			TSharedPtr<SBorder> ThumbnailBorder;
			HorizontalBox->AddSlot().Padding(0.f, 0.f, 2.f, 0.f).AutoWidth()
			[
				SAssignNew(ThumbnailBorder, SBorder)
				.OnMouseDoubleClick_Lambda([EditObject, Idx](const FGeometry&, const FPointerEvent&)
				{
					if (EditObject && GEditor) 
						GEditor->EditObject(EditObject);
					
					return FReply::Handled();
				})
				.Padding(5.f)
				[
					SNew(SBox)
					.WidthOverride(64)
					.HeightOverride(64)
					[
						StaticMeshThumbnail->MakeThumbnailWidget()
					]
				]
			];

			ThumbnailBorder->SetBorderImage(TAttribute< const FSlateBrush * >::Create(
				TAttribute< const FSlateBrush * >::FGetter::CreateLambda([ThumbnailBorder]()
			{
				if (ThumbnailBorder.IsValid() && ThumbnailBorder->IsHovered())
					return FEditorStyle::GetBrush("PropertyEditor.AssetThumbnailLight");
				else
					return FEditorStyle::GetBrush("PropertyEditor.AssetThumbnailShadow");
			}
			)));

			FText MeshNameText = FText::GetEmpty();
			//if (InputObject)
			//	MeshNameText = FText::FromString(InputObject->GetName());

			TSharedPtr< SComboButton > StaticMeshComboButton;

			TSharedPtr< SHorizontalBox > ButtonBox;
			HorizontalBox->AddSlot()
			.Padding(0.0f, 4.0f, 4.0f, 4.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.HAlign(HAlign_Fill)
				[
					SAssignNew(ButtonBox, SHorizontalBox)
					+ SHorizontalBox::Slot()
					[
						SAssignNew(StaticMeshComboButton, SComboButton)
						.ButtonStyle(FEditorStyle::Get(), "PropertyEditor.AssetComboStyle")
						.ForegroundColor(FEditorStyle::GetColor("PropertyEditor.AssetName.ColorAndOpacity"))
						.ContentPadding(2.0f)
						.ButtonContent()
						[
							SNew(STextBlock)
							.TextStyle(FEditorStyle::Get(), "PropertyEditor.AssetClass")
							.Font(FEditorStyle::GetFontStyle(FName(TEXT("PropertyWindow.NormalFont"))))
							.Text(FText::FromString(MainParam->GetValueAt(Idx)))
						]
					]
				]
			];
			
			StaticMeshComboButton->SetOnGetMenuContent(FOnGetContent::CreateLambda([StaticMeshComboButton, ChangeStringValueAt, Idx, StringParams]()
			{
				TArray<const UClass *> AllowedClasses;
				AllowedClasses.Add(UObject::StaticClass());
				TArray< UFactory * > NewAssetFactories;
				return PropertyCustomizationHelpers::MakeAssetPickerWithMenu(
					FAssetData(nullptr),
					true,
					AllowedClasses,
					NewAssetFactories,
					FOnShouldFilterAsset(),
					FOnAssetSelected::CreateLambda([StaticMeshComboButton, ChangeStringValueAt, Idx, StringParams](const FAssetData & AssetData)
					{
						UObject * Object = AssetData.GetAsset();
						if (!Object || Object->IsPendingKill())
							return;

						// Get the asset reference string for this object
						FString ReferenceStr = UHoudiniParameterString::GetAssetReference(Object);

						StaticMeshComboButton->SetIsOpen(false);
						ChangeStringValueAt(ReferenceStr, Object, Idx, true, StringParams);
					}),
					FSimpleDelegate::CreateLambda([]() {}));
				})
			);
		}
		else if (bIsMultiLine) 
		{
			TSharedPtr< SMultiLineEditableTextBox > MultiLineEditableTextBox;
			VerticalBox->AddSlot().Padding(2, 2, 5, 2).AutoHeight()
			[
				SNew(SAssetDropTarget)
				.OnIsAssetAcceptableForDrop_Lambda([](const UObject* InObject)
					{return true;})
				.OnAssetDropped_Lambda([=](UObject* InObject) 
				{
					// Get the asset reference string for this object
					FString ReferenceStr = UHoudiniParameterString::GetAssetReference(InObject);

					FString NewString = ReferenceStr;
					if (StringParams[0]->GetValueAt(Idx).Len() > 0)
						NewString = StringParams[0]->GetValueAt(Idx) + "\n" + NewString;

					ChangeStringValueAt(NewString, nullptr, Idx, true, StringParams);
				})
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Top)
					[
						SAssignNew(MultiLineEditableTextBox, SMultiLineEditableTextBox)
						.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
						.Text(FText::FromString(MainParam->GetValueAt(Idx)))
						.OnTextCommitted_Lambda([=](const FText& Val, ETextCommit::Type TextCommitType) { ChangeStringValueAt(Val.ToString(), nullptr, Idx, true, StringParams); })
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(2.0f, 0.0f)
					.VAlign(VAlign_Center)
					[
						SNew(SButton)
						.ToolTipText(LOCTEXT("RevertToDefault", "Revert to default"))
						.ButtonStyle(FEditorStyle::Get(), "NoBorder")
						.ContentPadding(0)
						.Visibility_Lambda([Idx, StringParams]()
						{
							for (auto & NextSelectedParam : StringParams) 
							{
								if (!NextSelectedParam)
									continue;

								if (!NextSelectedParam->IsDefaultValueAtIndex(Idx))
									return EVisibility::Visible;
							}

							return EVisibility::Hidden;
						})
						.OnClicked_Lambda([Idx, StringParams, RevertToDefault]() { return RevertToDefault(Idx, StringParams); })
						[
							SNew(SImage)
							.Image(FEditorStyle::GetBrush("PropertyWindow.DiffersFromDefault"))
						]
					]
				]
			];
		}
		else
		{		
			TSharedPtr< SEditableTextBox > EditableTextBox;
			VerticalBox->AddSlot().Padding(2, 2, 5, 2)
			[
				SNew(SAssetDropTarget)
				.OnIsAssetAcceptableForDrop_Lambda([](const UObject* InObject) 
					{return true;})
				.OnAssetDropped_Lambda([=](UObject* InObject) 
				{
					// Get the asset reference string for this object
					FString ReferenceStr = UHoudiniParameterString::GetAssetReference(InObject);

					ChangeStringValueAt(ReferenceStr, nullptr, Idx, true, StringParams);
				})
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.0f)
					[
						SAssignNew(EditableTextBox, SEditableTextBox)
						.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
						.Text(FText::FromString(MainParam->GetValueAt(Idx)))
						.OnTextCommitted_Lambda([=](const FText& Val, ETextCommit::Type TextCommitType) 
							{ ChangeStringValueAt(Val.ToString(), nullptr, Idx, true, StringParams); })
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(2.0f, 0.0f)
					.VAlign(VAlign_Center)
					[
						SNew(SButton)
						.ToolTipText(LOCTEXT("RevertToDefault", "Revert to default"))
						.ButtonStyle(FEditorStyle::Get(), "NoBorder")
						.ContentPadding(0)
						.Visibility_Lambda([Idx, StringParams]()
						{
							for (auto & NextSelectedParam : StringParams)
							{
								if (!NextSelectedParam)
									continue;

								if (!NextSelectedParam->IsDefaultValueAtIndex(Idx))
									return EVisibility::Visible;
							}	

							return EVisibility::Hidden;
						})
						.OnClicked_Lambda([Idx, StringParams, RevertToDefault]()
							{ return RevertToDefault(Idx, StringParams); })
						[
							SNew(SImage)
								.Image(FEditorStyle::GetBrush("PropertyWindow.DiffersFromDefault"))
						]
					]
				]
			];  
		} 
		
	}

	Row->ValueWidget.Widget = VerticalBox;
	Row->ValueWidget.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH);
	Row->ValueWidget.Widget->SetEnabled(!MainParam->IsDisabled());

}

void
FHoudiniParameterDetails::CreateWidgetColor(IDetailCategoryBuilder & HouParameterCategory, TArray<UHoudiniParameter*>& InParams)
{
	TArray<UHoudiniParameterColor*> ColorParams;
	if (!CastParameters<UHoudiniParameterColor>(InParams, ColorParams))
		return;

	if (ColorParams.Num() <= 0)
		return;

	UHoudiniParameterColor* MainParam = ColorParams[0];
	if (!MainParam || MainParam->IsPendingKill())
		return;
	
	// Create a new detail row
	FDetailWidgetRow* Row = CreateNestedRow(HouParameterCategory, InParams);

	if (!Row)
		return;

	// Create the standard parameter name widget.
	CreateNameWidget(Row, InParams, true);

	bool bHasAlpha = (MainParam->GetTupleSize() == 4);

	// Add color picker UI.
	TSharedPtr<SColorBlock> ColorBlock;
	TSharedRef<SVerticalBox> VerticalBox = SNew(SVerticalBox);
	VerticalBox->AddSlot().Padding(2, 2, 5, 2)
	[
		SAssignNew(ColorBlock, SColorBlock)
		.Color(MainParam->GetColorValue())
		.ShowBackgroundForAlpha(bHasAlpha)
		.OnMouseButtonDown(FPointerEventHandler::CreateLambda(
		[MainParam, ColorParams, ColorBlock, bHasAlpha](const FGeometry & MyGeometry, const FPointerEvent & MouseEvent)
		{
			if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
				return FReply::Unhandled();

			FColorPickerArgs PickerArgs;
			PickerArgs.ParentWidget = ColorBlock;
			PickerArgs.bUseAlpha = bHasAlpha;
			PickerArgs.DisplayGamma = TAttribute< float >::Create(
				TAttribute< float >::FGetter::CreateUObject(GEngine, &UEngine::GetDisplayGamma));
			PickerArgs.OnColorCommitted = FOnLinearColorValueChanged::CreateLambda([&](FLinearColor InColor) {

				FScopedTransaction Transaction(
					TEXT(HOUDINI_MODULE_RUNTIME),
					LOCTEXT("HoudiniParameterColorChange", "Houdini Parameter Color: Changing value"),
					MainParam->GetOuter(), true);

				bool bChanged = false;
				for (auto & Param : ColorParams) 
				{
					if (!Param)
						continue;

					Param->Modify();
					if (Param->SetColorValue(InColor)) 
					{
						Param->MarkChanged(true);
						bChanged = true;
					}
				}

				// cancel the transaction if there is actually no value changed
				if (!bChanged)
				{
					Transaction.Cancel();
				}

			});
			PickerArgs.InitialColorOverride = MainParam->GetColorValue();
			PickerArgs.bOnlyRefreshOnOk = true;
			OpenColorPicker(PickerArgs);
			return FReply::Handled();
		}))
	];

	Row->ValueWidget.Widget = VerticalBox;
	Row->ValueWidget.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH);
	Row->ValueWidget.Widget->SetEnabled(!MainParam->IsDisabled());
}

void 
FHoudiniParameterDetails::CreateWidgetButton(IDetailCategoryBuilder & HouParameterCategory, TArray<UHoudiniParameter*>& InParams) 
{
	TArray<UHoudiniParameterButton*> ButtonParams;
	if (!CastParameters<UHoudiniParameterButton>(InParams, ButtonParams))
		return;

	if (ButtonParams.Num() <= 0)
		return;

	UHoudiniParameterButton* MainParam = ButtonParams[0];
	if (!MainParam || MainParam->IsPendingKill())
		return;

	// Create a new detail row
	FDetailWidgetRow* Row = CreateNestedRow(HouParameterCategory, InParams);

	if (!Row)
		return;

	// Create the standard parameter name widget.
	CreateNameWidget(Row, InParams, true);

	FText ParameterLabelText = FText::FromString(MainParam->GetParameterLabel());
	FText ParameterTooltip = GetParameterTooltip(MainParam);

	TSharedRef<SHorizontalBox> HorizontalBox = SNew(SHorizontalBox);
	TSharedPtr<SButton> Button;

	// Add button UI.
	HorizontalBox->AddSlot().Padding(1, 2, 4, 2)
	[
		SAssignNew(Button, SButton)
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Center)
		.Text(ParameterLabelText)
		.ToolTipText(ParameterTooltip)
		.OnClicked(FOnClicked::CreateLambda( [MainParam, ButtonParams]()
		{
			for (auto & Param : ButtonParams) 
			{
				if (!Param)
					continue;

				// There is no undo redo operation for button
				Param->MarkChanged(true);
			}

			return FReply::Handled();
		}))
	];

	Row->ValueWidget.Widget = HorizontalBox;
	Row->ValueWidget.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH);
	Row->ValueWidget.Widget->SetEnabled(!MainParam->IsDisabled());
}

void 
FHoudiniParameterDetails::CreateWidgetButtonStrip(IDetailCategoryBuilder & HouParameterCategory, TArray<UHoudiniParameter*>& InParams) 
{
	TArray<UHoudiniParameterButtonStrip*> ButtonStripParams;
	if (!CastParameters<UHoudiniParameterButtonStrip>(InParams, ButtonStripParams))
		return;

	if (ButtonStripParams.Num() <= 0)
		return;

	UHoudiniParameterButtonStrip* MainParam = ButtonStripParams[0];
	if (!MainParam || MainParam->IsPendingKill())
		return;

	// Create a new detail row
	FDetailWidgetRow* Row = CreateNestedRow(HouParameterCategory, InParams);

	// Create the standard parameter name widget.
	CreateNameWidget(Row, InParams, true);

	if (!Row)
		return;

	auto OnButtonStateChanged = [MainParam, ButtonStripParams](ECheckBoxState NewState, int32 Idx) 
	{
	
		/*
		FScopedTransaction Transaction(
			TEXT(HOUDINI_MODULE_RUNTIME),
			LOCTEXT("HoudiniParameterButtonStripChange", "Houdini Parameter Button Strip: Changing value"),
			MainParam->GetOuter(), true);
		*/
		int32 StateInt = NewState == ECheckBoxState::Checked ? 1 : 0;
		bool bChanged = false;

		for (auto & NextParam : ButtonStripParams)
		{
			if (!NextParam || NextParam->IsPendingKill())
				continue;

			if (!NextParam->Values.IsValidIndex(Idx))
				continue;

			//NextParam->Modify();
			if (NextParam->SetValueAt(Idx, StateInt)) 
			{
				NextParam->MarkChanged(true);
				bChanged = true;
			}
		}

		//if (!bChanged)
		//	Transaction.Cancel();
	
	};


	FText ParameterLabelText = FText::FromString(MainParam->GetParameterLabel());
	FText ParameterTooltip = GetParameterTooltip(MainParam);

	TSharedRef<SHorizontalBox> HorizontalBox = SNew(SHorizontalBox);
	FLinearColor BgColor(0.53f, 0.81f, 0.82f, 1.0f);   // Sky Blue Backgroud color

	for (int32 Idx = 0; Idx < MainParam->Count; ++Idx) 
	{
		if (!MainParam->Values.IsValidIndex(Idx) || !MainParam->Labels.IsValidIndex(Idx))
			continue;

		bool bPressed = MainParam->Values[Idx] > 0;
		FText LabelText = FText::FromString(MainParam->Labels[Idx]);

		TSharedPtr<SCheckBox> Button;

		HorizontalBox->AddSlot().Padding(0).FillWidth(1.0f)
		[
			SAssignNew(Button, SCheckBox)
			.Style(FEditorStyle::Get(), "Property.ToggleButton.Middle")
			.IsChecked(bPressed ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
			.OnCheckStateChanged_Lambda(OnButtonStateChanged, Idx)
			.Content()
			[
				SNew(STextBlock)
				.Text(LabelText)
				.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			]
		];

		Button->SetColorAndOpacity(BgColor);
	}

	Row->ValueWidget.Widget = HorizontalBox;
	Row->ValueWidget.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH);
	Row->ValueWidget.MaxDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH);
	Row->ValueWidget.Widget->SetEnabled(!MainParam->IsDisabled());
}

void
FHoudiniParameterDetails::CreateWidgetLabel(IDetailCategoryBuilder & HouParameterCategory, TArray<UHoudiniParameter*> &InParams) 
{
	TArray<UHoudiniParameterLabel*> LabelParams;
	if (!CastParameters<UHoudiniParameterLabel>(InParams, LabelParams))
		return;

	if (LabelParams.Num() <= 0)
		return;

	UHoudiniParameterLabel* MainParam = LabelParams[0];
	if (!MainParam || MainParam->IsPendingKill())
		return;

	// Create a new detail row
	FDetailWidgetRow* Row = CreateNestedRow(HouParameterCategory, InParams);

	if (!Row)
		return;

	// Create the standard parameter name widget.
	CreateNameWidget(Row, InParams, true);

	TSharedRef <SVerticalBox> VerticalBox = SNew(SVerticalBox);

	for (int32 Index = 0; Index < MainParam->GetTupleSize(); ++Index) 
	{
		FString NextLabelString = MainParam->GetStringAtIndex(Index);
		FText ParameterLabelText = FText::FromString(NextLabelString);
		
		TSharedPtr<STextBlock> TextBlock;

		// Add Label UI.
		VerticalBox->AddSlot().Padding(1, 2, 4, 2)
		[
			SAssignNew(TextBlock, STextBlock).Text(ParameterLabelText)
		];
	}

	Row->ValueWidget.Widget = VerticalBox;
	Row->ValueWidget.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH);
	Row->ValueWidget.Widget->SetEnabled(!MainParam->IsDisabled());
}

void 
FHoudiniParameterDetails::CreateWidgetToggle(IDetailCategoryBuilder & HouParameterCategory, TArray<UHoudiniParameter*> &InParams) 
{
	TArray<UHoudiniParameterToggle*> ToggleParams;
	if (!CastParameters<UHoudiniParameterToggle>(InParams, ToggleParams))
		return;

	if (ToggleParams.Num() <= 0)
		return;

	UHoudiniParameterToggle* MainParam = ToggleParams[0];
	if (!MainParam || MainParam->IsPendingKill())
		return;

	// Create a new detail row
	FDetailWidgetRow* Row = CreateNestedRow(HouParameterCategory, InParams);

	if (!Row)
		return;

	// Create the standard parameter name widget.
	CreateNameWidget(Row, InParams, true);

	FText ParameterLabelText = FText::FromString(MainParam->GetParameterLabel());

	TSharedRef<SVerticalBox> VerticalBox = SNew(SVerticalBox);
	auto IsToggleCheckedLambda = [MainParam](int32 Index)
	{
		if (Index >= MainParam->GetNumValues())
			return ECheckBoxState::Unchecked;

		if (MainParam->GetValueAt(Index))
			return ECheckBoxState::Checked;

		return ECheckBoxState::Unchecked;
	};

	auto OnToggleCheckStateChanged = [MainParam, ToggleParams](ECheckBoxState NewState, int32 Index) 
	{
		if (Index >= MainParam->GetNumValues())
			return;

		FScopedTransaction Transaction(
			TEXT(HOUDINI_MODULE_RUNTIME),
			LOCTEXT("HoudiniParameterToggleChange", "Houdini Parameter Toggle: Changing value"),
			MainParam->GetOuter(), true);

		bool bState = (NewState == ECheckBoxState::Checked);

		bool bChanged = false;
		for (auto & Param : ToggleParams) 
		{
			if (!Param)
				continue;

			Param->Modify();
			if (Param->SetValueAt(bState, Index)) 
			{
				bChanged = true;
				Param->MarkChanged(true);
			}
		}

		// Cancel the transaction if no parameter has actually been changed
		if (!bChanged)
		{
			Transaction.Cancel();
		}
	};

	for (int32 Index = 0; Index < MainParam->GetTupleSize(); ++Index) 
	{
		TSharedPtr< SCheckBox > CheckBox;
		VerticalBox->AddSlot()
		.Padding(2, 2, 5, 2)
			[
				SAssignNew(CheckBox, SCheckBox)
				.OnCheckStateChanged_Lambda([OnToggleCheckStateChanged, Index](ECheckBoxState NewState) {
					OnToggleCheckStateChanged(NewState, Index);

				})
				.IsChecked_Lambda([IsToggleCheckedLambda, Index]() {
					return IsToggleCheckedLambda(Index);
				})
				.Content()
				[
					SNew(STextBlock)
					.Text(ParameterLabelText)
					.ToolTipText(GetParameterTooltip(MainParam))
					.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				]
			];
	}

	Row->ValueWidget.Widget = VerticalBox;
	Row->ValueWidget.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH);
	Row->ValueWidget.Widget->SetEnabled(!MainParam->IsDisabled());

}

void FHoudiniParameterDetails::CreateWidgetFile(IDetailCategoryBuilder & HouParameterCategory, TArray<UHoudiniParameter*> &InParams) 
{
	TArray<UHoudiniParameterFile*> FileParams;
	if (!CastParameters<UHoudiniParameterFile>(InParams, FileParams))
		return;

	if (FileParams.Num() <= 0)
		return;

	UHoudiniParameterFile* MainParam = FileParams[0];
	if (!MainParam || MainParam->IsPendingKill())
		return;

	// Create a new detail row
	FDetailWidgetRow* Row = CreateNestedRow(HouParameterCategory, InParams);

	if (!Row)
		return;

	// Create the standard parameter name widget.
	CreateNameWidget(Row, InParams, true);

	TSharedRef<SVerticalBox> VerticalBox = SNew(SVerticalBox);

	FString FileTypeWidgetFilter = TEXT("All files (*.*)|*.*");
	if (!MainParam->GetFileFilters().IsEmpty())
		FileTypeWidgetFilter = FString::Printf(TEXT("%s files (%s)|%s"), *MainParam->GetFileFilters(), *MainParam->GetFileFilters(), *MainParam->GetFileFilters());

	FString BrowseWidgetDirectory = FEditorDirectories::Get().GetLastDirectory(ELastDirectory::GENERIC_OPEN);

	auto UpdateCheckRelativePath = [MainParam](const FString & PickedPath) 
	{

		UHoudiniAssetComponent* HoudiniAssetComponent = Cast<UHoudiniAssetComponent>(MainParam->GetOuter());
		if (MainParam->GetOuter() && !PickedPath.IsEmpty() && FPaths::IsRelative(PickedPath))
		{
			// Check if the path is relative to the UE4 project
			FString AbsolutePath = FPaths::ConvertRelativePathToFull(PickedPath);
			if (FPaths::FileExists(AbsolutePath))
			{
				return AbsolutePath;
			}
			
			// Check if the path is relative to the asset
			
			if (HoudiniAssetComponent && !HoudiniAssetComponent->IsPendingKill())
			{
				if (HoudiniAssetComponent->HoudiniAsset && !HoudiniAssetComponent->HoudiniAsset->IsPendingKill())
				{
					FString AssetFilePath = FPaths::GetPath(HoudiniAssetComponent->HoudiniAsset->AssetFileName);
					if (FPaths::FileExists(AssetFilePath))
					{
						FString UpdatedFileWidgetPath = FPaths::Combine(*AssetFilePath, *PickedPath);
						if (FPaths::FileExists(UpdatedFileWidgetPath))
						{
							return UpdatedFileWidgetPath;
						}
					}
				}
			}
		}

		return PickedPath;
	};

	for (int32 Idx = 0; Idx < MainParam->GetTupleSize(); ++Idx)
	{
		FString FileWidgetPath = MainParam->GetValueAt(Idx);
		FString FileWidgetBrowsePath = BrowseWidgetDirectory;

		if (!FileWidgetPath.IsEmpty())
		{
			FString FileWidgetDirPath = FPaths::GetPath(FileWidgetPath);
			if (!FileWidgetDirPath.IsEmpty())
				FileWidgetBrowsePath = FileWidgetDirPath;
		}
					
		bool IsDirectoryPicker = MainParam->GetParameterType() == EHoudiniParameterType::FileDir;
		bool bIsNewFile = !MainParam->IsReadOnly();

		FText BrowseTooltip = LOCTEXT("FileButtonToolTipText", "Choose a file from this computer");
		if (IsDirectoryPicker)
			BrowseTooltip = LOCTEXT("DirButtonToolTipText", "Choose a directory from this computer");

		VerticalBox->AddSlot().Padding(2, 2, 5, 2)
		[
			SNew(SNewFilePathPicker)
			.BrowseButtonImage(FEditorStyle::GetBrush("PropertyWindow.Button_Ellipsis"))
			.BrowseButtonStyle(FEditorStyle::Get(), "HoverHintOnly")
			.BrowseButtonToolTip(BrowseTooltip)
			.BrowseDirectory(FileWidgetBrowsePath)
			.BrowseTitle(LOCTEXT("PropertyEditorTitle", "File picker..."))
			.FilePath(FileWidgetPath)
			.FileTypeFilter(FileTypeWidgetFilter)
			.IsNewFile(bIsNewFile)
			.IsDirectoryPicker(IsDirectoryPicker)
			.ToolTipText_Lambda([MainParam]()
			{
				// return the current param value as a tooltip
				FString FileValue = MainParam ? MainParam->GetValueAt(0) : FString();
				return FText::FromString(FileValue);
			})
			.OnPathPicked(FOnPathPicked::CreateLambda([MainParam, FileParams, UpdateCheckRelativePath, Idx](const FString & PickedPath) 
			{
				if (MainParam->GetNumValues() <= Idx)
					return;
				
				FScopedTransaction Transaction(
					TEXT(HOUDINI_MODULE_RUNTIME),
					LOCTEXT("HoudiniParameterFileChange", "Houdini Parameter File: Changing a file path"),
					MainParam->GetOuter(), true);

				bool bChanged = false;

				for (auto & Param : FileParams) 
				{
					if (!Param)
						continue;

					Param->Modify();
					if (Param->SetValueAt(UpdateCheckRelativePath(PickedPath), Idx)) 
					{
						bChanged = true;
						Param->MarkChanged(true);
					}	
				}

				// Cancel the transaction if no value has actually been changed
				if (!bChanged) 
				{
					Transaction.Cancel();
				}
			}))
		];

	}

	Row->ValueWidget.Widget = VerticalBox;
	Row->ValueWidget.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH);
	Row->ValueWidget.Widget->SetEnabled(!MainParam->IsDisabled());
}


void
FHoudiniParameterDetails::CreateWidgetChoice(IDetailCategoryBuilder & HouParameterCategory, TArray<UHoudiniParameter*>& InParams)
{
	TArray<UHoudiniParameterChoice*> ChoiceParams;
	if (!CastParameters<UHoudiniParameterChoice>(InParams, ChoiceParams))
		return;

	if (ChoiceParams.Num() <= 0)
		return;

	UHoudiniParameterChoice* MainParam = ChoiceParams[0];
	if (!MainParam || MainParam->IsPendingKill())
		return;

	// Create a new detail row
	FDetailWidgetRow* Row = CreateNestedRow(HouParameterCategory, InParams);

	if (!Row)
		return;

	// Create the standard parameter name widget.
	CreateNameWidget(Row, InParams, true);

	// Lambda for changing the parameter value
	auto ChangeSelectionLambda = [ChoiceParams](TSharedPtr< FString > NewChoice, ESelectInfo::Type SelectType) 
	{
		if (!NewChoice.IsValid())
			return;

		FScopedTransaction Transaction(
			TEXT(HOUDINI_MODULE_RUNTIME),
			LOCTEXT("HoudiniParameterChoiceChange", "Houdini Parameter Choice: Changing selection"),
			ChoiceParams[0]->GetOuter());

		const int32 NewIntValue = ChoiceParams[0]->GetIntValueFromLabel(*NewChoice);

		bool bChanged = false;
		for (int Idx = 0; Idx < ChoiceParams.Num(); Idx++)
		{
			if (!ChoiceParams[Idx])
				continue;

			ChoiceParams[Idx]->Modify();
			if (ChoiceParams[Idx]->SetIntValue(NewIntValue)) 
			{
				bChanged = true;
				ChoiceParams[Idx]->MarkChanged(true);
				ChoiceParams[Idx]->UpdateStringValueFromInt();
			}
		}

		if (!bChanged)
		{
			// Cancel the transaction if no parameter was changed
			Transaction.Cancel();
		}
	};

	// 
	MainParam->UpdateChoiceLabelsPtr();
	TArray<TSharedPtr<FString>>* OptionSource = MainParam->GetChoiceLabelsPtr();
	TSharedPtr<FString> IntialSelec;
	if (OptionSource && OptionSource->IsValidIndex(MainParam->GetIntValue()))
	{
		IntialSelec = (*OptionSource)[MainParam->GetIntValue()];
	}

	TSharedRef< SHorizontalBox > HorizontalBox = SNew(SHorizontalBox);
	TSharedPtr< SComboBox< TSharedPtr< FString > > > ComboBox;
	HorizontalBox->AddSlot().Padding( 2, 2, 5, 2 )
	[
		SAssignNew( ComboBox, SComboBox< TSharedPtr< FString > > )
		.OptionsSource(OptionSource)
		.InitiallySelectedItem(IntialSelec)
		.OnGenerateWidget_Lambda(
			[]( TSharedPtr< FString > InItem ) 
			{
				return SNew(STextBlock).Text(FText::FromString(*InItem));
			})
		.OnSelectionChanged_Lambda(
			[ChangeSelectionLambda](TSharedPtr< FString > NewChoice, ESelectInfo::Type SelectType)
			{
				ChangeSelectionLambda(NewChoice, SelectType);
			})
		[
			SNew(STextBlock)
			.Text_Lambda([MainParam]() { return FText::FromString(MainParam->GetLabel()); })
			.Font( FEditorStyle::GetFontStyle( TEXT( "PropertyWindow.NormalFont" ) ) )
		]
	];

	if ( ComboBox.IsValid() )
		ComboBox->SetEnabled( !MainParam->IsDisabled() );

	Row->ValueWidget.Widget = HorizontalBox;
	Row->ValueWidget.MinDesiredWidth( HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH );
	Row->ValueWidget.Widget->SetEnabled(!MainParam->IsDisabled());

}

void
FHoudiniParameterDetails::CreateWidgetSeparator(IDetailCategoryBuilder & HouParameterCategory, TArray<UHoudiniParameter*>& InParams, const bool& InIsEnabled)
{
	if (InParams.Num() <= 0)
		return;

	UHoudiniParameter* MainParam = InParams[0];

	if (!MainParam)
		return;

	// Create a new detail row
	FDetailWidgetRow* Row = CreateNestedRow(HouParameterCategory, InParams);

	if (!Row)
		return;

	(*Row)
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.Padding(0, 0, 5, 0)
		[
			SNew(SSeparator)
			.Thickness(0.5f)
		]
	];

	Row->ValueWidget.Widget->SetEnabled(!MainParam->IsDisabled());

}

void 
FHoudiniParameterDetails::CreateWidgetOperatorPath(IDetailCategoryBuilder & HouParameterCategory, TArray<UHoudiniParameter*> &InParams) 
{
	TArray<UHoudiniParameterOperatorPath*> OperatorPathParams;
	if (!CastParameters<UHoudiniParameterOperatorPath>(InParams, OperatorPathParams))
		return;

	if (OperatorPathParams.Num() <= 0)
		return;

	UHoudiniParameterOperatorPath* MainParam = OperatorPathParams[0];
	if (!MainParam || MainParam->IsPendingKill())
		return;

	UHoudiniInput* MainInput = MainParam->HoudiniInput.Get();
	if (!MainInput)
		return;

	// Build an array of edited inputs for multi edition
	TArray<UHoudiniInput*> EditedInputs;
	EditedInputs.Add(MainInput);

	// Add the corresponding inputs found in the other HAC
	for (int LinkedIdx = 1; LinkedIdx < OperatorPathParams.Num(); LinkedIdx++)
	{
		UHoudiniInput* LinkedInput = OperatorPathParams[LinkedIdx]->HoudiniInput.Get();
		if (!LinkedInput || LinkedInput->IsPendingKill())
			continue;

		// Linked params should match the main param! If not try to find one that matches
		if (!LinkedInput->Matches(*MainInput))
			continue;

		EditedInputs.Add(LinkedInput);
	}

	// Create a new detail row
	FDetailWidgetRow* Row = CreateNestedRow(HouParameterCategory, InParams);
	if (!Row)
		return;

	// Create the standard parameter name widget.
	CreateNameWidget(Row, InParams, true);

	FHoudiniInputDetails::CreateWidget(HouParameterCategory, EditedInputs, Row);

	Row->ValueWidget.Widget->SetEnabled(!MainParam->IsDisabled());
}

void
FHoudiniParameterDetails::CreateWidgetFloatRamp(IDetailCategoryBuilder & HouParameterCategory, TArray<UHoudiniParameter*> &InParams) 
{
	if (InParams.Num() < 1)
		return;

	UHoudiniParameter* MainParam = InParams[0];
	if (!MainParam || MainParam->IsPendingKill())
		return;

	//  Parsing a float ramp: 1->(2->3->4)*->5  //
	switch (MainParam->GetParameterType()) 
	{
		//*****State 1: Float Ramp*****//
		case EHoudiniParameterType::FloatRamp:
		{
			UHoudiniParameterRampFloat* FloatRampParameter = Cast<UHoudiniParameterRampFloat>(MainParam);
			if (FloatRampParameter) 
			{
				CurrentRampFloat = FloatRampParameter;
				CurrentRampFloatPointsArray.Empty();

				CurrentRampParameterList = InParams;

				FDetailWidgetRow *Row = CreateWidgetRampCurveEditor(HouParameterCategory, InParams);
				CurrentRampRow = Row;
			}
			break;
		}

		case EHoudiniParameterType::Float:
		{
			UHoudiniParameterFloat* FloatParameter = Cast<UHoudiniParameterFloat>(MainParam);
			if (FloatParameter)
			{
				bool bCreateNewPoint = true;
				if (CurrentRampFloatPointsArray.Num() > 0) 
				{
					UHoudiniParameterRampFloatPoint* LastPtInArr = CurrentRampFloatPointsArray.Last();
					if (LastPtInArr && !LastPtInArr->ValueParentParm)
						bCreateNewPoint = false;
				}

				//*****State 2: Float Parameter (position)*****//
				if (bCreateNewPoint) 
				{
					// Create a new float ramp point, and add its pointer to the current float points buffer array.
					UHoudiniParameterRampFloatPoint* NewRampFloatPoint = NewObject< UHoudiniParameterRampFloatPoint >(MainParam);
					CurrentRampFloatPointsArray.Add(NewRampFloatPoint);

					if (FloatParameter->GetNumberOfValues() <= 0)
						return;
					// Set the float ramp point's position parent parm, and value
					NewRampFloatPoint->PositionParentParm = FloatParameter;
					NewRampFloatPoint->SetPosition(FloatParameter->GetValuesPtr()[0]);
				}
				//*****State 3: Float Parameter (value)*****//
				else 
				{
					if (FloatParameter->GetNumberOfValues() <= 0)
						return;
					// Get the last point in the buffer array
					if (CurrentRampFloatPointsArray.Num() > 0)
					{
						// Set the last inserted float ramp point's float parent parm, and value
						UHoudiniParameterRampFloatPoint* LastAddedFloatRampPoint = CurrentRampFloatPointsArray.Last();
						LastAddedFloatRampPoint->ValueParentParm = FloatParameter;
						LastAddedFloatRampPoint->SetValue(FloatParameter->GetValuesPtr()[0]);
					}
				}
			}

			break;
		}
		//*****State 4: Choice parameter*****//
		case EHoudiniParameterType::IntChoice:
		{
			UHoudiniParameterChoice* ChoiceParameter = Cast<UHoudiniParameterChoice>(MainParam);
			if (ChoiceParameter)
			{
				// Set the last inserted float ramp point's interpolation parent parm, and value
				UHoudiniParameterRampFloatPoint* LastAddedFloatRampPoint = CurrentRampFloatPointsArray.Last();

				LastAddedFloatRampPoint->InterpolationParentParm = ChoiceParameter;
				LastAddedFloatRampPoint->SetInterpolation(UHoudiniParameter::GetHoudiniInterpMethodFromInt(ChoiceParameter->GetIntValue()));

				// Set the index of this point in the multi parm.
				LastAddedFloatRampPoint->InstanceIndex = CurrentRampFloatPointsArray.Num() - 1;
			}


			//*****State 5: All ramp points have been parsed, finish!*****//
			if (CurrentRampFloatPointsArray.Num() >= (int32)CurrentRampFloat->MultiParmInstanceCount)
			{
				CurrentRampFloatPointsArray.Sort([](const UHoudiniParameterRampFloatPoint& P1, const UHoudiniParameterRampFloatPoint& P2) {
					return P1.Position < P2.Position;
				});

				CurrentRampFloat->Points = CurrentRampFloatPointsArray;

				// Not caching, points are synced, update cached points
				if (!CurrentRampFloat->bCaching)
				{
					CurrentRampFloat->CachedPoints.Empty();
					for (auto NextSyncedPoint : CurrentRampFloat->Points)
					{
						UHoudiniParameterRampFloatPoint * DuplicatedFloatPoint = DuplicateObject<UHoudiniParameterRampFloatPoint>(NextSyncedPoint, CurrentRampFloat);
						CurrentRampFloat->CachedPoints.Add(DuplicatedFloatPoint);
					}
				}

				CreateWidgetRampPoints(CurrentRampRow, CurrentRampFloat, CurrentRampParameterList);

				CurrentRampFloat->SetDefaultValues();

				CurrentRampFloat = nullptr;
				CurrentRampRow = nullptr;
			}

			break;
		}

		default:
			break;
	}

}

void
FHoudiniParameterDetails::CreateWidgetColorRamp(IDetailCategoryBuilder & HouParameterCategory, TArray<UHoudiniParameter*> &InParams)
{
	if (InParams.Num() < 1)
		return;

	UHoudiniParameter* MainParam = InParams[0];
	if (!MainParam || MainParam->IsPendingKill())
		return;

	//  Parsing a color ramp: 1->(2->3->4)*->5  //
	switch (MainParam->GetParameterType())
	{
		//*****State 1: Color Ramp*****//
		case EHoudiniParameterType::ColorRamp:
		{
			UHoudiniParameterRampColor* RampColor = Cast<UHoudiniParameterRampColor>(MainParam);
			if (RampColor) 
			{
				CurrentRampColor = RampColor;
				CurrentRampColorPointsArray.Empty();

				CurrentRampParameterList = InParams;

				FDetailWidgetRow *Row = CreateWidgetRampCurveEditor(HouParameterCategory, InParams);
				CurrentRampRow = Row;
			}

			break;
		}
		//*****State 2: Float parameter*****//
		case EHoudiniParameterType::Float:
		{
			UHoudiniParameterFloat* FloatParameter = Cast<UHoudiniParameterFloat>(MainParam);
			if (FloatParameter) 
			{
				// Create a new color ramp point, and add its pointer to the current color points buffer array.
				UHoudiniParameterRampColorPoint* NewRampColorPoint = NewObject< UHoudiniParameterRampColorPoint >(MainParam);
				CurrentRampColorPointsArray.Add(NewRampColorPoint);

				if (FloatParameter->GetNumberOfValues() <= 0)
					return;
				// Set the color ramp point's position parent parm, and value
				NewRampColorPoint->PositionParentParm = FloatParameter;
				NewRampColorPoint->SetPosition(FloatParameter->GetValuesPtr()[0]);
			}

			break;
		}
		//*****State 3: Color parameter*****//
		case EHoudiniParameterType::Color:
		{
			UHoudiniParameterColor* ColorParameter = Cast<UHoudiniParameterColor>(MainParam);
			if (ColorParameter && CurrentRampColorPointsArray.Num() > 0) 
			{
				// Set the last inserted color ramp point's color parent parm, and value
				UHoudiniParameterRampColorPoint* LastAddedColorRampPoint = CurrentRampColorPointsArray.Last();
				LastAddedColorRampPoint->ValueParentParm = ColorParameter;
				LastAddedColorRampPoint->SetValue(ColorParameter->GetColorValue());
			}

			break;
		}
		//*****State 4: Choice Parameter*****//
		case EHoudiniParameterType::IntChoice: 
		{
			UHoudiniParameterChoice* ChoiceParameter = Cast<UHoudiniParameterChoice>(MainParam);
			if (ChoiceParameter) 
			{
				// Set the last inserted color ramp point's interpolation parent parm, and value
				UHoudiniParameterRampColorPoint*& LastAddedColorRampPoint = CurrentRampColorPointsArray.Last();

				LastAddedColorRampPoint->InterpolationParentParm = ChoiceParameter;
				LastAddedColorRampPoint->SetInterpolation(UHoudiniParameter::GetHoudiniInterpMethodFromInt(ChoiceParameter->GetIntValue()));

				// Set the index of this point in the multi parm.
				LastAddedColorRampPoint->InstanceIndex = CurrentRampColorPointsArray.Num() - 1;
			}


			//*****State 5: All ramp points have been parsed, finish!*****//
			if (CurrentRampColorPointsArray.Num() >= (int32)CurrentRampColor->MultiParmInstanceCount) 
			{
				CurrentRampColorPointsArray.Sort([](const UHoudiniParameterRampColorPoint& P1, const UHoudiniParameterRampColorPoint& P2) 
				{
					return P1.Position < P2.Position;
				});

				CurrentRampColor->Points = CurrentRampColorPointsArray;

				// Not caching, points are synced, update cached points
				
				if (!CurrentRampColor->bCaching) 
				{
					CurrentRampColor->CachedPoints.Empty();
					for (auto NextSyncedPoint : CurrentRampColor->Points) 
					{
						UHoudiniParameterRampColorPoint * DuplicatedColorPoint = DuplicateObject<UHoudiniParameterRampColorPoint>(NextSyncedPoint, CurrentRampColor);
						CurrentRampColor->CachedPoints.Add(DuplicatedColorPoint);
					}
				}
				
			
				CreateWidgetRampPoints(CurrentRampRow, CurrentRampColor, CurrentRampParameterList);

				CurrentRampColor->SetDefaultValues();

				CurrentRampColor = nullptr;
				CurrentRampRow = nullptr;
			}

			break;
		}

		default:
			break;
	}

}


FDetailWidgetRow*
FHoudiniParameterDetails::CreateWidgetRampCurveEditor(IDetailCategoryBuilder & HouParameterCategory, TArray<UHoudiniParameter*> &InParams)
{
	if (InParams.Num() <= 0)
		return nullptr;

	UHoudiniParameter* MainParam = InParams[0];
	if (!MainParam)
		return nullptr;
	
	// Create a new detail row
	FDetailWidgetRow* Row = CreateNestedRow(HouParameterCategory, (TArray<UHoudiniParameter*>)InParams);
	if (!Row)
		return nullptr;

	Row->ValueWidget.Widget->SetEnabled(!MainParam->IsDisabled());

	// Create the standard parameter name widget with an added autoupdate checkbox.
	CreateNameWidgetWithAutoUpdate(Row, InParams, true);

	TSharedRef<SVerticalBox> VerticalBox = SNew(SVerticalBox);	
	if (MainParam->GetParameterType() == EHoudiniParameterType::ColorRamp)
	{
		UHoudiniParameterRampColor *RampColorParam = Cast<UHoudiniParameterRampColor>(MainParam);
		if (!RampColorParam)
			return nullptr;
		
		TSharedPtr<SHoudiniColorRampCurveEditor> ColorGradientEditor;
		VerticalBox->AddSlot()
		.Padding(2, 2, 5, 2)
		.AutoHeight()
		[
			SNew(SBorder)
			.VAlign(VAlign_Fill)
			[
				SAssignNew(ColorGradientEditor, SHoudiniColorRampCurveEditor)
				.ViewMinInput(0.0f)
				.ViewMaxInput(1.0f)
			]
		];

		if (!ColorGradientEditor.IsValid())
			return nullptr;

		UHoudiniColorCurveEditorParentClass* ColorCurveParent = 
			NewObject<UHoudiniColorCurveEditorParentClass>(MainParam, UHoudiniColorCurveEditorParentClass::StaticClass());
		ColorCurveParents.Add(ColorCurveParent);

		ColorCurveParent->CurveEditor = ColorGradientEditor;
		CurrentRampParameterColorCurve = NewObject<UHoudiniColorRampCurve>(
				ColorCurveParent, UHoudiniColorRampCurve::StaticClass(), NAME_None, RF_Transactional | RF_Public);

		if (!CurrentRampParameterColorCurve)
			return nullptr;

		TArray<UHoudiniParameterRampColor*> ColorRampParameters;
		CastParameters<UHoudiniParameterRampColor>(InParams, ColorRampParameters);
		for (auto NextColorRamp : ColorRampParameters)
		{
			CurrentRampParameterColorCurve->ColorRampParameters.Add(NextColorRamp);
		}
		ColorGradientEditor->HoudiniColorRampCurve = CurrentRampParameterColorCurve;

		// Clear default curve points
		for (int Idx = 0; Idx < 4; ++Idx)
		{
			FRichCurve& RichCurve = (CurrentRampParameterColorCurve->FloatCurves)[Idx];
			if (RichCurve.GetNumKeys() > 0)
				RichCurve.Keys.Empty();
		}
		ColorGradientEditor->SetCurveOwner(CurrentRampParameterColorCurve);
	}
	else if(MainParam->GetParameterType() == EHoudiniParameterType::FloatRamp)
	{
		UHoudiniParameterRampFloat *RampFloatParam = Cast<UHoudiniParameterRampFloat>(MainParam);
		if (!RampFloatParam)
			return nullptr;

		TSharedPtr<SHoudiniFloatRampCurveEditor> FloatCurveEditor;
		VerticalBox->AddSlot()
		.Padding(2, 2, 5, 2)
		.AutoHeight()
		[
			SNew(SBorder)
			.VAlign(VAlign_Fill)
			[
				SAssignNew(FloatCurveEditor, SHoudiniFloatRampCurveEditor)
				.ViewMinInput(0.0f)
				.ViewMaxInput(1.0f)
				.HideUI(true)
				.DrawCurve(true)
				.ViewMinInput(0.0f)
				.ViewMaxInput(1.0f)
				.ViewMinOutput(0.0f)
				.ViewMaxOutput(1.0f)
				.TimelineLength(1.0f)
				.AllowZoomOutput(false)
				.ShowInputGridNumbers(false)
				.ShowOutputGridNumbers(false)
				.ShowZoomButtons(false)
				.ZoomToFitHorizontal(false)
				.ZoomToFitVertical(false)
				.XAxisName(FString("X"))
				.YAxisName(FString("Y"))
				.ShowCurveSelector(false)

			]
		];

		if (!FloatCurveEditor.IsValid())
			return nullptr;

		UHoudiniFloatCurveEditorParentClass* FloatCurveParent =
			NewObject<UHoudiniFloatCurveEditorParentClass>(MainParam, UHoudiniFloatCurveEditorParentClass::StaticClass());
		FloatCurveParents.Add(FloatCurveParent);

		FloatCurveParent->CurveEditor = FloatCurveEditor;
		CurrentRampParameterFloatCurve = NewObject<UHoudiniFloatRampCurve>(
				FloatCurveParent, UHoudiniFloatRampCurve::StaticClass(), NAME_None, RF_Transactional | RF_Public);

		if (!CurrentRampParameterFloatCurve)
			return nullptr;

		TArray<UHoudiniParameterRampFloat*> FloatRampParameters;
		CastParameters<UHoudiniParameterRampFloat>(InParams, FloatRampParameters);		
		for (auto NextFloatRamp : FloatRampParameters)
		{
			CurrentRampParameterFloatCurve->FloatRampParameters.Add(NextFloatRamp);
		}
		FloatCurveEditor->HoudiniFloatRampCurve = CurrentRampParameterFloatCurve;

		FloatCurveEditor->SetCurveOwner(CurrentRampParameterFloatCurve, true);
	}

	Row->ValueWidget.Widget = VerticalBox;
	Row->ValueWidget.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH);
	Row->ValueWidget.MaxDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH);

	return Row;
}


void 
FHoudiniParameterDetails::CreateWidgetRampPoints(FDetailWidgetRow* Row, UHoudiniParameter* InParameter, TArray<UHoudiniParameter*>& InParams) 
{
	if (!Row || !InParameter)
		return;

	if (InParams.Num() < 1)
		return;
	
	UHoudiniParameter* MainParam = InParams[0];
	if (!MainParam || MainParam->IsPendingKill())
		return;

	UHoudiniParameterRampFloat * MainFloatRampParameter = nullptr; 
	UHoudiniParameterRampColor * MainColorRampParameter = nullptr;

	TArray<UHoudiniParameterRampFloat*> FloatRampParameterList;
	TArray<UHoudiniParameterRampColor*> ColorRampParameterList;
	if (MainParam->GetParameterType() == EHoudiniParameterType::FloatRamp)
	{
		MainFloatRampParameter = Cast<UHoudiniParameterRampFloat>(MainParam);

		if (!MainFloatRampParameter)
			return;

		if (!CastParameters<UHoudiniParameterRampFloat>(InParams, FloatRampParameterList))
			return;
	}
	else if (MainParam->GetParameterType() == EHoudiniParameterType::ColorRamp)
	{
		MainColorRampParameter = Cast<UHoudiniParameterRampColor>(MainParam);

		if (!MainColorRampParameter)
			return;

		if (!CastParameters<UHoudiniParameterRampColor>(InParams, ColorRampParameterList))
			return;
	}
	else 
	{
		return;
	}

	// Lambda for computing the float point to be inserted
	auto GetInsertFloatPointLambda = [MainFloatRampParameter](
		const int32& InsertAtIndex, 
		float& OutPosition,
		float& OutValue, 
		EHoudiniRampInterpolationType& OutInterpType) mutable
	{
		if (!MainFloatRampParameter)
			return;

		float PrevPosition = 0.0f;
		float NextPosition = 1.0f;
		
		TArray<UHoudiniParameterRampFloatPoint*> &CurrentPoints = MainFloatRampParameter->Points;
		TArray<UHoudiniParameterRampFloatPoint*> &CachedPoints = MainFloatRampParameter->CachedPoints;

		int32 NumPoints = 0;
		if (MainFloatRampParameter->IsAutoUpdate())
			NumPoints = CurrentPoints.Num();
		else
			NumPoints = CachedPoints.Num();

		if (InsertAtIndex >= NumPoints)
		{
			// Insert at the end
			if (NumPoints > 0)
			{
				UHoudiniParameterRampFloatPoint* PrevPoint = nullptr;
				if (MainFloatRampParameter->IsAutoUpdate())
					PrevPoint = CurrentPoints.Last();
				else
					PrevPoint = CachedPoints.Last();

				if (PrevPoint)
				{
					PrevPosition = PrevPoint->GetPosition();
					OutInterpType = PrevPoint->GetInterpolation();
				}
			}
		}
		else if (InsertAtIndex <= 0)
		{
			// Insert at the beginning
			if (NumPoints > 0)
			{
				UHoudiniParameterRampFloatPoint* NextPoint = nullptr;
				if (MainFloatRampParameter->IsAutoUpdate())
					NextPoint = CurrentPoints[0];
				else
					NextPoint = CachedPoints[0];

				if (NextPoint)
				{
					NextPosition = NextPoint->GetPosition();
					OutInterpType = NextPoint->GetInterpolation();
				}
			}
		}
		else
		{
			// Insert in the middle
			if (NumPoints > 1)
			{
				UHoudiniParameterRampFloatPoint* PrevPoint = nullptr;
				UHoudiniParameterRampFloatPoint* NextPoint = nullptr;

				if (MainFloatRampParameter->IsAutoUpdate()) 
				{
					PrevPoint = CurrentPoints[InsertAtIndex - 1];
					NextPoint = CurrentPoints[InsertAtIndex];
				}
				else 
				{
					PrevPoint = CachedPoints[InsertAtIndex - 1];
					NextPoint = CachedPoints[InsertAtIndex];
				}

				if (PrevPoint)
				{
					PrevPosition = PrevPoint->GetPosition();
					OutInterpType = PrevPoint->GetInterpolation();
				}

				if (NextPoint)
				{
					NextPosition = NextPoint->GetPosition();
				}

				if (PrevPoint && NextPoint)
				{
					OutValue = (PrevPoint->GetValue() + NextPoint->GetValue()) / 2.0;
				}
			}
		}

		OutPosition = (PrevPosition + NextPosition) / 2.0f;
	};


	// Lambda for computing the color point to be inserted
	auto GetInsertColorPointLambda = [MainColorRampParameter](
		const int32& InsertAtIndex,
		float& OutPosition,
		FLinearColor& OutColor,
		EHoudiniRampInterpolationType& OutInterpType) mutable
	{
		if (!MainColorRampParameter)
			return;

		float PrevPosition = 0.0f;
		float NextPosition = 1.0f;

		TArray<UHoudiniParameterRampColorPoint*> &CurrentPoints = MainColorRampParameter->Points;
		TArray<UHoudiniParameterRampColorPoint*> &CachedPoints = MainColorRampParameter->CachedPoints;

		int32 NumPoints = 0;
		if (MainColorRampParameter->IsAutoUpdate())
			NumPoints = CurrentPoints.Num();
		else
			NumPoints = CachedPoints.Num();

		if (InsertAtIndex >= NumPoints)
		{
			// Insert at the end
			if (NumPoints > 0)
			{
				UHoudiniParameterRampColorPoint* PrevPoint = nullptr;

				if (MainColorRampParameter->IsAutoUpdate())
					PrevPoint = CurrentPoints.Last();
				else
					PrevPoint = CachedPoints.Last();

				if (PrevPoint)
				{
					PrevPosition = PrevPoint->GetPosition();
					OutInterpType = PrevPoint->GetInterpolation();
				}
			}
		}
		else if (InsertAtIndex <= 0)
		{
			// Insert at the beginning
			if (NumPoints > 0)
			{
				UHoudiniParameterRampColorPoint* NextPoint = nullptr;

				if (MainColorRampParameter->IsAutoUpdate())
					NextPoint = CurrentPoints[0];
				else
					NextPoint = CachedPoints[0];

				if (NextPoint)
				{
					NextPosition = NextPoint->GetPosition();
					OutInterpType = NextPoint->GetInterpolation();
				}
			}
		}
		else
		{
			// Insert in the middle
			if (NumPoints > 1)
			{
				UHoudiniParameterRampColorPoint* PrevPoint = nullptr;
				UHoudiniParameterRampColorPoint* NextPoint = nullptr;

				if (MainColorRampParameter->IsAutoUpdate()) 
				{
					PrevPoint = CurrentPoints[InsertAtIndex - 1];
					NextPoint = CurrentPoints[InsertAtIndex];
				}
				else 
				{
					PrevPoint = CachedPoints[InsertAtIndex - 1];
					NextPoint = CachedPoints[InsertAtIndex];
				}

				if (PrevPoint) 
				{
					PrevPosition = PrevPoint->GetPosition();
					OutInterpType = PrevPoint->GetInterpolation();
				}

				if (NextPoint)
				{
					NextPosition = NextPoint->GetPosition();
				}

				if (PrevPoint && NextPoint) 
				{
					OutColor = (PrevPoint->GetValue() + NextPoint->GetValue()) / 2.0;
				}
			}
		}

		OutPosition = (PrevPosition + NextPosition) / 2.0f;
	};
	
	int32 RowIndex = 0;
	auto InsertRampPoint_Lambda = [GetInsertColorPointLambda, GetInsertFloatPointLambda](
		UHoudiniParameterRampFloat* MainRampFloat, 
		UHoudiniParameterRampColor* MainRampColor, 
		TArray<UHoudiniParameterRampFloat*> &RampFloatList,
		TArray<UHoudiniParameterRampColor*> &RampColorList,
		const int32& Index) mutable 
	{
		if (MainRampFloat)
		{
			float InsertPosition = 0.0f;
			float InsertValue = 1.0f;
			EHoudiniRampInterpolationType InsertInterp = EHoudiniRampInterpolationType::LINEAR;

			GetInsertFloatPointLambda(Index, InsertPosition, InsertValue, InsertInterp);

			FHoudiniParameterDetails::ReplaceAllFloatRampParameterPointsWithMainParameter(RampFloatList);

			for (auto & NextFloatRamp : RampFloatList)
			{
				if (NextFloatRamp->IsAutoUpdate())
				{
					CreateFloatRampParameterInsertEvent(
						NextFloatRamp, InsertPosition, InsertValue, InsertInterp);

					NextFloatRamp->MarkChanged(true);
				}
				else
				{
					UHoudiniParameterRampFloatPoint* NewCachedPoint = NewObject<UHoudiniParameterRampFloatPoint>
						(NextFloatRamp, UHoudiniParameterRampFloatPoint::StaticClass());
					NewCachedPoint->Position = InsertPosition;
					NewCachedPoint->Value = InsertValue;
					NewCachedPoint->Interpolation = InsertInterp;

					NextFloatRamp->CachedPoints.Add(NewCachedPoint);
					NextFloatRamp->bCaching = true;
				}
			}

			if (!MainRampFloat->IsAutoUpdate())
			{
				FHoudiniEngineUtils::UpdateEditorProperties(MainRampFloat, true);
			}

		}
		else if (MainRampColor)
		{
			float InsertPosition = 0.0f;
			FLinearColor InsertColor = FLinearColor::Black;
			EHoudiniRampInterpolationType InsertInterp = EHoudiniRampInterpolationType::LINEAR;

			GetInsertColorPointLambda(Index, InsertPosition, InsertColor, InsertInterp);

			FHoudiniParameterDetails::ReplaceAllColorRampParameterPointsWithMainParameter(RampColorList);
			for (auto& NextColorRamp : RampColorList)
			{
				if (NextColorRamp->IsAutoUpdate())
				{
					CreateColorRampParameterInsertEvent(
						NextColorRamp, InsertPosition, InsertColor, InsertInterp);

					NextColorRamp->MarkChanged(true);
				}
				else
				{
					UHoudiniParameterRampColorPoint* NewCachedPoint = NewObject<UHoudiniParameterRampColorPoint>
						(NextColorRamp, UHoudiniParameterRampColorPoint::StaticClass());
					NewCachedPoint->Position = InsertPosition;
					NewCachedPoint->Value = InsertColor;
					NewCachedPoint->Interpolation = InsertInterp;

					NextColorRamp->CachedPoints.Add(NewCachedPoint);
					NextColorRamp->bCaching = true;
				}
			}

			if (!MainRampColor->IsAutoUpdate())
			{
				FHoudiniEngineUtils::UpdateEditorProperties(MainRampColor, true);
			}
		}
	};

	auto DeleteRampPoint_Lambda = [](
		UHoudiniParameterRampFloat* MainRampFloat,
		UHoudiniParameterRampColor* MainRampColor, 
		TArray<UHoudiniParameterRampFloat*> &FloatRampList,
		TArray<UHoudiniParameterRampColor*> &ColorRampList,
		const int32& Index) mutable
	{
		if (MainRampFloat)
		{
			FHoudiniParameterDetails::ReplaceAllFloatRampParameterPointsWithMainParameter(FloatRampList);

			for (auto& NextFloatRamp : FloatRampList)
			{
				if (NextFloatRamp->IsAutoUpdate())
				{
					if (NextFloatRamp->Points.Num() == 0)
						return;

					UHoudiniParameterRampFloatPoint* PointToDelete = nullptr;

					if (Index == -1)
						PointToDelete = NextFloatRamp->Points.Last();
					else if (NextFloatRamp->Points.IsValidIndex(Index))
						PointToDelete = NextFloatRamp->Points[Index];
					
					if (!PointToDelete)
						return;

					const int32 & InstanceIndexToDelete = PointToDelete->InstanceIndex;

					CreateFloatRampParameterDeleteEvent(NextFloatRamp, InstanceIndexToDelete);
					NextFloatRamp->MarkChanged(true);
				}
				else
				{
					if (NextFloatRamp->CachedPoints.Num() == 0)
						return;

					if (Index == -1) 
						NextFloatRamp->CachedPoints.Pop();
					else if (NextFloatRamp->CachedPoints.IsValidIndex(Index))
						NextFloatRamp->CachedPoints.RemoveAt(Index);
					else
						return;

					NextFloatRamp->bCaching = true;
				}
			}

			if (!MainRampFloat->IsAutoUpdate())
			{
				FHoudiniEngineUtils::UpdateEditorProperties(MainRampFloat, true);
			}
		}
		else
		{
			FHoudiniParameterDetails::ReplaceAllColorRampParameterPointsWithMainParameter(ColorRampList);

			for (auto& NextColorRamp : ColorRampList)
			{
				if (NextColorRamp->IsAutoUpdate())
				{
					if (NextColorRamp->Points.Num() == 0)
						return;

					UHoudiniParameterRampColorPoint* PointToRemove = nullptr;

					if (Index == -1)
						PointToRemove = NextColorRamp->Points.Last();
					else if (NextColorRamp->Points.IsValidIndex(Index))
						PointToRemove = NextColorRamp->Points[Index];

					if (!PointToRemove)
						return;

					const int32 & InstanceIndexToDelete = PointToRemove->InstanceIndex;

					CreateColorRampParameterDeleteEvent(NextColorRamp, InstanceIndexToDelete);

					NextColorRamp->MarkChanged(true);
				}
				else
				{
					if (NextColorRamp->CachedPoints.Num() == 0)
						return;

					if (Index == -1)
						NextColorRamp->CachedPoints.Pop();
					else if (NextColorRamp->CachedPoints.IsValidIndex(Index))
						NextColorRamp->CachedPoints.RemoveAt(Index);
					else
						return;

					NextColorRamp->bCaching = true;
				}
			}

			if (!MainRampColor->IsAutoUpdate())
			{
				FHoudiniEngineUtils::UpdateEditorProperties(MainRampColor, true);
			}
		}
	};


	TSharedRef<SVerticalBox> VerticalBox = StaticCastSharedRef<SVerticalBox>(Row->ValueWidget.Widget);

	TSharedPtr<SUniformGridPanel> GridPanel;
	VerticalBox->AddSlot()
	.Padding(2, 2, 5, 2)
	.AutoHeight()
	[
		SAssignNew(GridPanel, SUniformGridPanel)	
	];

	//AllUniformGridPanels.Add(GridPanel.Get());

	GridPanel->SetSlotPadding(FMargin(2.f, 2.f, 5.f, 3.f));	
	GridPanel->AddSlot(0, RowIndex)
	[
		SNew(STextBlock)
		.Text(FText::FromString(TEXT("Position")))
		.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))		
	];

	FString ValueString = TEXT("Value");
	if (!MainFloatRampParameter)
		ValueString = TEXT("Color");

	GridPanel->AddSlot(1, RowIndex)
	[
		SNew(STextBlock)
			.Text(FText::FromString(ValueString))
			.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
	];

	GridPanel->AddSlot(2, RowIndex)
	[
		SNew(STextBlock)
			.Text(FText::FromString(TEXT("Interp.")))
			.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
	];

	
	GridPanel->AddSlot(3, RowIndex)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.Padding(3.f, 0.f)
		.MaxWidth(35.f)
		.AutoWidth()
		[
			PropertyCustomizationHelpers::MakeAddButton(
				FSimpleDelegate::CreateLambda([InsertRampPoint_Lambda, MainFloatRampParameter, MainColorRampParameter, FloatRampParameterList, ColorRampParameterList]() mutable
				{
					int32 InsertAtIndex = -1;
					if (MainFloatRampParameter) 
					{
						if (MainFloatRampParameter->IsAutoUpdate())
							InsertAtIndex = MainFloatRampParameter->Points.Num();
						else
							InsertAtIndex = MainFloatRampParameter->CachedPoints.Num();
					}
					else if (MainColorRampParameter) 
					{
						if (MainColorRampParameter->IsAutoUpdate())
							InsertAtIndex = MainColorRampParameter->Points.Num();
						else
							InsertAtIndex = MainColorRampParameter->CachedPoints.Num();
					}

					InsertRampPoint_Lambda(MainFloatRampParameter, MainColorRampParameter, FloatRampParameterList, ColorRampParameterList, InsertAtIndex);
				}),
				LOCTEXT("AddRampPoint", "Add a ramp point to the end"), true)
		]
		+ SHorizontalBox::Slot()
		.Padding(3.f, 0.f)
		.MaxWidth(35.f)
		.AutoWidth()
		[
			PropertyCustomizationHelpers::MakeRemoveButton(
				FSimpleDelegate::CreateLambda([DeleteRampPoint_Lambda, MainFloatRampParameter, MainColorRampParameter, FloatRampParameterList, ColorRampParameterList]() mutable
				{
					DeleteRampPoint_Lambda(
						MainFloatRampParameter, MainColorRampParameter, FloatRampParameterList, ColorRampParameterList, -1);
				}),
				LOCTEXT("DeleteRampPoint", "Delete the last ramp point"), true)
		]
		
	];	
	
	EUnit Unit = EUnit::Unspecified;
	TSharedPtr<INumericTypeInterface<float>> paramTypeInterface;
	paramTypeInterface = MakeShareable(new TNumericUnitTypeInterface<float>(Unit));

	int32 PointCount = 0;
	// Use Synced points on auto update mode
	// Use Cached points on manual update mode
	if (MainFloatRampParameter)
	{
		if (MainFloatRampParameter->IsAutoUpdate())
			PointCount = MainFloatRampParameter->Points.Num();
		else
			PointCount = MainFloatRampParameter->CachedPoints.Num();
	}

	if (MainColorRampParameter)
	{
		if (MainColorRampParameter->IsAutoUpdate())
			PointCount = MainColorRampParameter->Points.Num();
		else
			PointCount = MainColorRampParameter->CachedPoints.Num();
	}

	// Lambda function for changing a ramp point
	auto OnPointChangeCommit = [](
		UHoudiniParameterRampFloat* MainRampFloat, UHoudiniParameterRampColor* MainRampColor, 
		UHoudiniParameterRampFloatPoint* MainRampFloatPoint, UHoudiniParameterRampColorPoint* MainRampColorPoint,
		TArray<UHoudiniParameterRampFloat*> &RampFloatList, TArray<UHoudiniParameterRampColor*> &RampColorList, 
		const int32& Index, const FString& ChangedDataName, 
		const float& NewPosition, const float& NewFloat, 
		const FLinearColor& NewColor, 
		const EHoudiniRampInterpolationType& NewInterpType) mutable
	{
		if (MainRampFloat && MainRampFloatPoint)
		{
			if (ChangedDataName == FString("position") && MainRampFloatPoint->GetPosition() == NewPosition)
				return;
			if (ChangedDataName == FString("value") && MainRampFloatPoint->GetValue() == NewFloat)
				return;
			if (ChangedDataName == FString("interp") && MainRampFloatPoint->GetInterpolation() == NewInterpType)
				return;

			FHoudiniParameterDetails::ReplaceAllFloatRampParameterPointsWithMainParameter(RampFloatList);
			for (auto NextFloatRamp : RampFloatList)
			{
				if (!NextFloatRamp)
					continue;

				if (NextFloatRamp->IsAutoUpdate())
				{
					if (NextFloatRamp->Points.IsValidIndex(Index))
					{
						UHoudiniParameterRampFloatPoint* CurrentFloatRampPoint = NextFloatRamp->Points[Index];
						if (!CurrentFloatRampPoint)
							continue;

						if (ChangedDataName == FString("position"))
						{
							if (!CurrentFloatRampPoint->PositionParentParm)
								continue;

							CurrentFloatRampPoint->SetPosition(NewPosition);
							CurrentFloatRampPoint->PositionParentParm->MarkChanged(true);
						}
						else if (ChangedDataName == FString("value")) 
						{
							if (!CurrentFloatRampPoint->PositionParentParm)
								continue;

							CurrentFloatRampPoint->SetValue(NewFloat);
							CurrentFloatRampPoint->ValueParentParm->MarkChanged(true);
						}
						else if (ChangedDataName == FString("interp"))
						{
							if (!CurrentFloatRampPoint->InterpolationParentParm)
								continue;

							CurrentFloatRampPoint->SetInterpolation(NewInterpType);
							CurrentFloatRampPoint->InterpolationParentParm->MarkChanged(true);
						}
					}
					else
					{
						int32 IdxInEventsArray = Index - NextFloatRamp->Points.Num();
						if (NextFloatRamp->ModificationEvents.IsValidIndex(IdxInEventsArray))
						{
							UHoudiniParameterRampModificationEvent* Event = NextFloatRamp->ModificationEvents[IdxInEventsArray];
							if (!Event)
								continue;

							if (ChangedDataName == FString("position")) 
							{
								Event->InsertPosition = NewPosition;
							}
							else if (ChangedDataName == FString("value"))
							{
								Event->InsertFloat = NewFloat;
							}
							else if (ChangedDataName == FString("interp")) 
							{
								Event->InsertInterpolation = NewInterpType;
							}
						}
					}
				}
				else
				{
					if (NextFloatRamp->CachedPoints.IsValidIndex(Index))
					{
						UHoudiniParameterRampFloatPoint* CachedPoint = NextFloatRamp->CachedPoints[Index];

						if (!CachedPoint)
							continue;

						if (ChangedDataName == FString("position"))
						{
							CachedPoint->Position = NewPosition;
						}
						else if (ChangedDataName == FString("value"))
						{
							CachedPoint->Value = NewFloat;
						}
						else if (ChangedDataName == FString("interp"))
						{
							CachedPoint->Interpolation = NewInterpType;
						}
						
						NextFloatRamp->bCaching = true;
					}
				}

				if (!MainRampFloat->IsAutoUpdate())
					FHoudiniEngineUtils::UpdateEditorProperties(MainRampFloat, true);
			}
		}
		else if (MainRampColor && MainRampColorPoint)
		{
			if (ChangedDataName == FString("position") && MainRampColorPoint->GetPosition() == NewPosition)
				return;
			
			if (ChangedDataName == FString("value") && MainRampColorPoint->GetValue() == NewColor)
				return;
			
			if (ChangedDataName == FString("interp") && MainRampColorPoint->GetInterpolation() == NewInterpType)
				return;

			FHoudiniParameterDetails::ReplaceAllColorRampParameterPointsWithMainParameter(RampColorList);
			for (auto NextColorRamp : RampColorList)
			{
				if (!NextColorRamp)
					continue;

				if (NextColorRamp->IsAutoUpdate())
				{
					if (NextColorRamp->Points.IsValidIndex(Index))
					{
						UHoudiniParameterRampColorPoint* CurrentColorRampPoint = NextColorRamp->Points[Index];
						if (!CurrentColorRampPoint)
							continue;

						if (ChangedDataName == FString("position"))
						{
							if (!CurrentColorRampPoint->PositionParentParm)
								continue;

							CurrentColorRampPoint->SetPosition(NewPosition);
							CurrentColorRampPoint->PositionParentParm->MarkChanged(true);
						}
						else if (ChangedDataName == FString("value"))
						{
							if (!CurrentColorRampPoint->PositionParentParm)
								continue;

							CurrentColorRampPoint->SetValue(NewColor);
							CurrentColorRampPoint->ValueParentParm->MarkChanged(true);
						}
						else if (ChangedDataName == FString("interp"))
						{
							if (!CurrentColorRampPoint->InterpolationParentParm)
								continue;

							CurrentColorRampPoint->SetInterpolation(NewInterpType);
							CurrentColorRampPoint->InterpolationParentParm->MarkChanged(true);
						}
					}
					else
					{
						int32 IdxInEventsArray = Index - NextColorRamp->Points.Num();
						if (NextColorRamp->ModificationEvents.IsValidIndex(IdxInEventsArray))
						{
							UHoudiniParameterRampModificationEvent* Event = NextColorRamp->ModificationEvents[IdxInEventsArray];
							if (!Event)
								continue;

							if (ChangedDataName == FString("position"))
							{
								Event->InsertPosition = NewPosition;
							}
							else if (ChangedDataName == FString("value"))
							{
								Event->InsertColor = NewColor;
							}
							else if (ChangedDataName == FString("interp"))
							{
								Event->InsertInterpolation = NewInterpType;
							}

						}
					}
				}
				else
				{
					if (NextColorRamp->CachedPoints.IsValidIndex(Index))
					{
						UHoudiniParameterRampColorPoint* CachedPoint = NextColorRamp->CachedPoints[Index];

						if (!CachedPoint)
							continue;

						if (ChangedDataName == FString("position"))
						{
							CachedPoint->Position = NewPosition;
						}
						else if (ChangedDataName == FString("value"))
						{
							CachedPoint->Value = NewColor;
						}
						else if (ChangedDataName == FString("interp"))
						{
							CachedPoint->Interpolation = NewInterpType;
						}

						NextColorRamp->bCaching = true;
					}
				}

				if (!MainRampColor->IsAutoUpdate())
					FHoudiniEngineUtils::UpdateEditorProperties(MainRampColor, true);
			}
		}
	};

	for (int32 Index = 0; Index < PointCount; ++Index) 
	{
		UHoudiniParameterRampFloatPoint* NextFloatRampPoint = nullptr;
		UHoudiniParameterRampColorPoint* NextColorRampPoint = nullptr;		
		
		if (MainFloatRampParameter)
		{
			if (MainFloatRampParameter->IsAutoUpdate())
				NextFloatRampPoint = MainFloatRampParameter->Points[Index];
			else
				NextFloatRampPoint = MainFloatRampParameter->CachedPoints[Index];
		}
		if (MainColorRampParameter)
		{
			if (MainColorRampParameter->IsAutoUpdate())
				NextColorRampPoint = MainColorRampParameter->Points[Index];
			else
				NextColorRampPoint = MainColorRampParameter->CachedPoints[Index];
		}

		if (!NextFloatRampPoint && !NextColorRampPoint)
			continue;
		
		RowIndex += 1;
		
		float CurPos = 0.f;
		if (NextFloatRampPoint)
			CurPos = NextFloatRampPoint->Position;
		else
			CurPos = NextColorRampPoint->Position;
		

		GridPanel->AddSlot(0, RowIndex)
		[
			SNew(SNumericEntryBox<float>)
			.AllowSpin(true)
			.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			.Value(CurPos)
			.OnValueChanged_Lambda([](float Val) {})
			.OnValueCommitted_Lambda([OnPointChangeCommit, MainFloatRampParameter, MainColorRampParameter, NextFloatRampPoint, NextColorRampPoint, FloatRampParameterList, ColorRampParameterList, Index](float Val, ETextCommit::Type TextCommitType) mutable
			{
				OnPointChangeCommit(MainFloatRampParameter, MainColorRampParameter,
					NextFloatRampPoint, NextColorRampPoint,
					FloatRampParameterList, ColorRampParameterList,
					Index, FString("position"),
					Val, float(-1.0),
					FLinearColor(),
					EHoudiniRampInterpolationType::LINEAR);
			})
			.OnBeginSliderMovement_Lambda([]() {})
			.OnEndSliderMovement_Lambda([OnPointChangeCommit, MainFloatRampParameter, MainColorRampParameter, NextFloatRampPoint, NextColorRampPoint, FloatRampParameterList, ColorRampParameterList, Index](const float Val) mutable
			{
				OnPointChangeCommit(MainFloatRampParameter, MainColorRampParameter,
					NextFloatRampPoint, NextColorRampPoint,
					FloatRampParameterList, ColorRampParameterList,
					Index, FString("position"),
					Val, float(-1.0),
					FLinearColor(),
					EHoudiniRampInterpolationType::LINEAR);
			})
			.SliderExponent(1.0f)
			.TypeInterface(paramTypeInterface)
		];

		if (NextFloatRampPoint)
		{
			GridPanel->AddSlot(1, RowIndex)
			[
				SNew(SNumericEntryBox< float >)
				.AllowSpin(true)
				.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				.Value(NextFloatRampPoint->Value)
				.OnValueChanged_Lambda([](float Val){})
				.OnValueCommitted_Lambda([OnPointChangeCommit, MainFloatRampParameter, MainColorRampParameter,
					NextFloatRampPoint, NextColorRampPoint, FloatRampParameterList, ColorRampParameterList, Index](float Val, ETextCommit::Type TextCommitType) mutable
				{
					OnPointChangeCommit(MainFloatRampParameter, MainColorRampParameter,
						NextFloatRampPoint, NextColorRampPoint,
						FloatRampParameterList, ColorRampParameterList,
						Index, FString("value"),
						float(-1.0), Val,
						FLinearColor(),
						EHoudiniRampInterpolationType::LINEAR);
				})
				.OnBeginSliderMovement_Lambda([]() {})
				.OnEndSliderMovement_Lambda([OnPointChangeCommit, MainFloatRampParameter, MainColorRampParameter, 
					NextFloatRampPoint, NextColorRampPoint, FloatRampParameterList, ColorRampParameterList, Index](const float Val) mutable
				{
					OnPointChangeCommit(MainFloatRampParameter, MainColorRampParameter,
						NextFloatRampPoint, NextColorRampPoint,
						FloatRampParameterList, ColorRampParameterList,
						Index, FString("value"),
						float(-1.0), Val,
						FLinearColor(),
						EHoudiniRampInterpolationType::LINEAR);
				})
				.SliderExponent(1.0f)
				.TypeInterface(paramTypeInterface)
			];
		}
		else if (NextColorRampPoint)
		{	
			auto OnColorChangeLambda = [OnPointChangeCommit, MainFloatRampParameter, MainColorRampParameter, 
				NextFloatRampPoint, NextColorRampPoint, FloatRampParameterList, ColorRampParameterList, Index](FLinearColor InColor) mutable
			{
				OnPointChangeCommit(MainFloatRampParameter, MainColorRampParameter,
						NextFloatRampPoint, NextColorRampPoint,
						FloatRampParameterList, ColorRampParameterList,
						Index, FString("value"),
						float(-1.0), float(-1.0),
						InColor,
						EHoudiniRampInterpolationType::LINEAR);
			};

			// Add color picker UI.
			//TSharedPtr<SColorBlock> ColorBlock;
			GridPanel->AddSlot(1, RowIndex)
			[
				SNew(SColorBlock)
				.Color(NextColorRampPoint->Value)
				.OnMouseButtonDown( FPointerEventHandler::CreateLambda(
					[NextColorRampPoint, OnColorChangeLambda](const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
					{
						if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
							return FReply::Unhandled();

						FColorPickerArgs PickerArgs;
						PickerArgs.bUseAlpha = true;
						PickerArgs.DisplayGamma = TAttribute< float >::Create(
							TAttribute< float >::FGetter::CreateUObject(GEngine, &UEngine::GetDisplayGamma));
						PickerArgs.OnColorCommitted = FOnLinearColorValueChanged::CreateLambda(OnColorChangeLambda);
						FLinearColor InitColor = NextColorRampPoint->Value;
						PickerArgs.InitialColorOverride = InitColor;
						PickerArgs.bOnlyRefreshOnOk = true;
						OpenColorPicker(PickerArgs);
						return FReply::Handled();
					}))
			];
		}

		int32 CurChoice = 0;
		if (NextFloatRampPoint)
			CurChoice = (int)NextFloatRampPoint->Interpolation;
		else
			CurChoice = (int)NextColorRampPoint->Interpolation;

		TSharedPtr <SComboBox<TSharedPtr< FString >>> ComboBoxCurveMethod;
		GridPanel->AddSlot(2, RowIndex)
		[
			SAssignNew(ComboBoxCurveMethod, SComboBox<TSharedPtr<FString>>)
			.OptionsSource(FHoudiniEngineEditor::Get().GetHoudiniParameterRampInterpolationMethodLabels())
			.InitiallySelectedItem((*FHoudiniEngineEditor::Get().GetHoudiniParameterRampInterpolationMethodLabels())[CurChoice])
			.OnGenerateWidget_Lambda([](TSharedPtr<FString> ChoiceEntry)
			{
				FText ChoiceEntryText = FText::FromString(*ChoiceEntry);
				return SNew(STextBlock)
					.Text(ChoiceEntryText)
					.ToolTipText(ChoiceEntryText)
					.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")));
			})
			.OnSelectionChanged_Lambda(
				[OnPointChangeCommit, MainFloatRampParameter, MainColorRampParameter,
				NextFloatRampPoint, NextColorRampPoint, FloatRampParameterList, 
				ColorRampParameterList, Index](TSharedPtr<FString> NewChoice, ESelectInfo::Type SelectType) mutable
			{
				EHoudiniRampInterpolationType NewInterpType = UHoudiniParameter::GetHoudiniInterpMethodFromString(*NewChoice.Get());

				OnPointChangeCommit(MainFloatRampParameter, MainColorRampParameter,
					NextFloatRampPoint, NextColorRampPoint,
					FloatRampParameterList, ColorRampParameterList,
					Index, FString("interp"),
					float(-1.0), float(-1.0),
					FLinearColor(),
					NewInterpType);
			})
			[
				SNew(STextBlock)
				.Text_Lambda([NextFloatRampPoint, NextColorRampPoint]()
				{
					EHoudiniRampInterpolationType CurInterpType = EHoudiniRampInterpolationType::InValid;
					if (NextFloatRampPoint)
						CurInterpType = NextFloatRampPoint->GetInterpolation();
					else
						CurInterpType = NextColorRampPoint->GetInterpolation();

					return FText::FromString(UHoudiniParameter::GetStringFromHoudiniInterpMethod(CurInterpType));
				})
				.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			]
		];
		
		GridPanel->AddSlot(3, RowIndex)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(3.f, 0.f)
			.MaxWidth(35.f)
			.AutoWidth()
			[
				PropertyCustomizationHelpers::MakeAddButton( FSimpleDelegate::CreateLambda(
					[InsertRampPoint_Lambda, MainFloatRampParameter, 
					MainColorRampParameter, FloatRampParameterList, 
					ColorRampParameterList, Index]() mutable
					{
						InsertRampPoint_Lambda(MainFloatRampParameter, MainColorRampParameter, FloatRampParameterList, ColorRampParameterList, Index);
					}),
					LOCTEXT("AddRampPoint", "Add a ramp point before this point"), true)
			]
			+ SHorizontalBox::Slot()
			.Padding(3.f, 0.f)
			.MaxWidth(35.f)
			.AutoWidth()
			[
				PropertyCustomizationHelpers::MakeDeleteButton(FSimpleDelegate::CreateLambda(
					[DeleteRampPoint_Lambda, MainFloatRampParameter,
					MainColorRampParameter, FloatRampParameterList,
					ColorRampParameterList, Index]() mutable
					{
						DeleteRampPoint_Lambda(MainFloatRampParameter, MainColorRampParameter, FloatRampParameterList, ColorRampParameterList, Index);
					}),
					LOCTEXT("DeleteFloatRampPoint", "Delete this ramp point"), true)
			]
		];		
		
		if (MainFloatRampParameter && CurrentRampParameterFloatCurve)
		{
			ERichCurveInterpMode RichCurveInterpMode = UHoudiniParameter::EHoudiniRampInterpolationTypeToERichCurveInterpMode(NextFloatRampPoint->GetInterpolation());
			FRichCurve & RichCurve = CurrentRampParameterFloatCurve->FloatCurve;
			FKeyHandle const KeyHandle = RichCurve.AddKey(NextFloatRampPoint->GetPosition(), NextFloatRampPoint->GetValue());
			RichCurve.SetKeyInterpMode(KeyHandle, RichCurveInterpMode);
		}

		if (MainColorRampParameter && CurrentRampParameterColorCurve)
		{
			ERichCurveInterpMode RichCurveInterpMode = UHoudiniParameter::EHoudiniRampInterpolationTypeToERichCurveInterpMode(NextColorRampPoint->GetInterpolation());
			for (int32 CurveIdx = 0; CurveIdx < 4; ++CurveIdx)
			{
				FRichCurve & RichCurve = CurrentRampParameterColorCurve->FloatCurves[CurveIdx];

				FKeyHandle const KeyHandle = RichCurve.AddKey(NextColorRampPoint->GetPosition(), NextColorRampPoint->GetValue().Component(CurveIdx));
				RichCurve.SetKeyInterpMode(KeyHandle, RichCurveInterpMode);
			}
		}
	}	

	if (MainFloatRampParameter)
		GridPanel->SetEnabled(!MainFloatRampParameter->IsDisabled());

	if (MainColorRampParameter)
		GridPanel->SetEnabled(!MainColorRampParameter->IsDisabled());	
}

void 
FHoudiniParameterDetails::CreateWidgetFolderList(IDetailCategoryBuilder & HouParameterCategory, TArray<UHoudiniParameter*> &InParams)
{
	TArray<UHoudiniParameterFolderList*> FolderListParams;
	if (!CastParameters<UHoudiniParameterFolderList>(InParams, FolderListParams))
		return;

	if (FolderListParams.Num() <= 0)
		return;

	UHoudiniParameterFolderList* MainParam = FolderListParams[0];
	if (!MainParam || MainParam->IsPendingKill())
		return;

	AllFoldersAndTabs.Add(MainParam->GetParmId(), MainParam);
	MainParam->GetTabs().Empty();

	CurrentFolderListSize = MainParam->GetTupleSize();

	if (MainParam->IsDirectChildOfMultiParm())
		MultiParmInstanceIndices.Add(MainParam->GetParmId(), -1);

	if (CurrentFolderListSize == 0)
		return;

	if (MainParam->GetTupleSize() > 1) 
	{
		// Create an entry in the SelectedIndices when first time visiting the tab directory.
		if (MainParam->ShouldDisplay())
		{
			bCurrentTabMenu = true;
			CurrentTabMenuFolderList = MainParam;

			FDetailWidgetRow* TabRow = CreateNestedRow(HouParameterCategory, InParams, false);
		
			CurrentTabMenuFolderList->SetIsTabMenu(true);
		}
	}

	// When see a folder list, go deepth first search at this step.
	// Push an empty queue to the stack.
	FolderChildCounterStack.Add(TArray<int32>());
	FolderStack.Add(TArray<bool>());

}


void
FHoudiniParameterDetails::CreateWidgetFolder(IDetailCategoryBuilder & HouParameterCategory, TArray<UHoudiniParameter*> &InParams)
{
	TArray<UHoudiniParameterFolder*> FolderParams;
	if (!CastParameters<UHoudiniParameterFolder>(InParams, FolderParams))
		return;

	if (FolderParams.Num() <= 0)
		return;

	UHoudiniParameterFolder* MainParam = FolderParams[0];
	if (!MainParam || MainParam->IsPendingKill())
		return;
	
	if (FolderStack.Num() <= 0)  // error state
		return;

	// If a folder is invisible, its children won't be listed by HAPI. 
	// So just prune the stack in such case.
	if (!MainParam->IsVisible())
	{
		CurrentFolderListSize -= 1;

		if (CurrentFolderListSize == 0)
		{
			CurrentTabMenuBox = nullptr;
			CurrentTabMenuFolderList = nullptr;
			bCurrentTabMenu = false;
			
			if (FolderChildCounterStack.Num() > 1)
			{
				TArray<int32> &ParentFolderChildCounterQueue = FolderChildCounterStack[FolderChildCounterStack.Num() - 2];
				if (ParentFolderChildCounterQueue.Num() > 0)
					ParentFolderChildCounterQueue[0] -= 1;
			}

			PruneStack();
		}

		return;
	}

	AllFoldersAndTabs.Add(MainParam->GetParmId(), MainParam);

	// Consider Tab menu with only one tab as a simple folder.
	if (!bCurrentTabMenu) 
	{
		if (MainParam->IsTab())
			MainParam->SetFolderType(EHoudiniFolderParameterType::Collapsible);
	}

	// Case 1: The folder is a direct child of a multiparm.
	if (MainParam->IsDirectChildOfMultiParm())
	{
		if (FolderStack.Num() <= 0 || FolderChildCounterStack.Num() <= 0)   // error state
			return;

		if (!AllMultiParms.Contains(MainParam->GetParentParmId())) // error state
			return;

		UHoudiniParameterMultiParm* ParentMultiParm = AllMultiParms[MainParam->GetParentParmId()];

		if (!ParentMultiParm)
			return;

		bool bExpanded = ParentMultiParm->IsShown();

		// Case 1-1: The folder is NOT in a tab menu.
		if (!bCurrentTabMenu)
		{
			bExpanded &= MainParam->IsExpanded();

			// If the parent multiparm is shown.
			if (ParentMultiParm->IsShown())
			{
				FDetailWidgetRow* FolderHeaderRow = CreateNestedRow(HouParameterCategory, InParams, false);
				CreateFolderHeaderUI(FolderHeaderRow, InParams);
			}
		}
		// Case 1-2: The folder IS under a tab menu.
		else 
		{
			bExpanded &= MainParam->IsChosen();

			if(CurrentTabMenuBox)	// If the tab is visible
				CreateWidgetTab(MainParam);
		}

		if (MainParam->GetTupleSize() > 0 && (!bCurrentTabMenu || !CurrentTabMenuBox))
		{
			TArray<bool> & MyQueue = FolderStack.Last();
			TArray<int32> & MyChildCounterQueue = FolderChildCounterStack.Last();
			MyQueue.Add(bExpanded);
			MyChildCounterQueue.Add(MainParam->GetTupleSize());
		}
	}

	// Case 2: The folder is NOT a direct child of a multiparm.
	else 
	{
		// Case 2-1: The folder is in another folder.
		if (FolderStack.Num() > 1 && FolderChildCounterStack.Num() > 1 && CurrentFolderListSize > 0) 
		{
			TArray<bool>& MyFolderQueue = FolderStack.Last();
			TArray<int32> & MyFolderChildCounterQueue = FolderChildCounterStack.Last();

			TArray<bool> & ParentFolderQueue = FolderStack[FolderStack.Num() - 2];
			TArray<int32> & ParentFolderChildCounterQueue = FolderChildCounterStack[FolderChildCounterStack.Num() - 2];

			if (ParentFolderQueue.Num() <= 0 || ParentFolderChildCounterQueue.Num() <= 0)	// error state
				return;

			if (ParentFolderChildCounterQueue[0] <= 0)	// error state
				return;

			// Peek the folder queue of the last layer to get the folder's parent.
			bool ParentFolderVisible = ParentFolderQueue[0];

			bool bExpanded = ParentFolderVisible;

			// Case 2-1-1: The folder is NOT in a tab menu.
			if (!bCurrentTabMenu) 
			{
				bExpanded &= MainParam->IsExpanded();
			
				// The parent folder is visible.
				if (ParentFolderVisible)
				{
					// Add the folder header UI.
					FDetailWidgetRow* FolderHeaderRow = CreateNestedRow(HouParameterCategory, InParams, false);
					CreateFolderHeaderUI(FolderHeaderRow, InParams);
				}
			}
			// Case 2-1-2: The folder IS in a tab menu.
			else 
			{
				bExpanded &= MainParam->IsChosen();

				if (CurrentTabMenuBox) // The tab menu is visible
					CreateWidgetTab(MainParam);
			}

			if (MainParam->GetTupleSize() > 0 && (!CurrentTabMenuBox || !bCurrentTabMenu)) 
			{
				MyFolderQueue.Add(bExpanded);
				MyFolderChildCounterQueue.Add(MainParam->GetTupleSize());
			}
		}
		// Case 2-2: The folder is in the root.
		else 
		{
			bool bExpanded = true;

			// Case 2-2-1: The folder is NOT under a tab menu.
			if (!bCurrentTabMenu) 
			{
				if (FolderStack.Num() <= 0 || FolderChildCounterStack.Num() <= 0) // error state
					return;

				// Create Folder header under root.
				FDetailWidgetRow* FolderRow = CreateNestedRow(HouParameterCategory, InParams, false);
				CreateFolderHeaderUI(FolderRow, InParams);

				bExpanded &= MainParam->IsExpanded();
			}
			// Case 2-2-2: The folder IS under a tab menu.
			else 
			{
				bExpanded &= MainParam->IsChosen();

				if (CurrentTabMenuBox)
					CreateWidgetTab(MainParam);
			}

			if (MainParam->GetTupleSize() > 0 && (!bCurrentTabMenu || !CurrentTabMenuBox))
			{
				TArray<bool> & RootQueue = FolderStack.Last();
				TArray<int32> & RootChildCounterQueue = FolderChildCounterStack.Last();
				RootQueue.Add(bExpanded);
				RootChildCounterQueue.Add(MainParam->GetTupleSize());
			}

		}	
	}


	CurrentFolderListSize -= 1;

	if (CurrentFolderListSize == 0)
	{
		CurrentTabMenuBox = nullptr;
		CurrentTabMenuFolderList = nullptr;
		bCurrentTabMenu = false;
		if (FolderChildCounterStack.Num() > 1 && FolderStack.Num() > 1 && !MainParam->IsDirectChildOfMultiParm())
		{
			TArray<int32> &ParentFolderChildCounterQueue = FolderChildCounterStack[FolderChildCounterStack.Num() - 2];
			if (ParentFolderChildCounterQueue.Num() > 0)
				ParentFolderChildCounterQueue[0] -= 1;
		}

		PruneStack();
	}

}

void
FHoudiniParameterDetails::CreateFolderHeaderUI(FDetailWidgetRow* HeaderRow, TArray<UHoudiniParameter*> &InParams)
{
	TArray<UHoudiniParameterFolder*> FolderParams;
	if (!CastParameters<UHoudiniParameterFolder>(InParams, FolderParams))
		return;

	if (FolderParams.Num() <= 0)
		return;

	UHoudiniParameterFolder* MainParam = FolderParams[0];

	if (!MainParam || MainParam->IsPendingKill())
		return;

	TSharedPtr<SVerticalBox> VerticalBox;
	if (HeaderRow == nullptr)
		return;

	FString LabelStr = MainParam->GetParameterLabel();
	int Indent = Indentation.Contains(MainParam->GetParmId()) ? Indent = Indentation[MainParam->GetParmId()] : 0;

	TSharedPtr<SHorizontalBox> HorizontalBox;
	TSharedPtr<SButton> ExpanderArrow;
	TSharedPtr<SImage> ExpanderImage;

	FString IndentStr = GetIndentationString(MainParam);

	FText IndentText = FText::FromString(IndentStr);

	HeaderRow->NameWidget.Widget = SAssignNew(HorizontalBox, SHorizontalBox);

	// Add indentation space holder.
	HorizontalBox->AddSlot().AutoWidth()
	[
		SNew(STextBlock)
		.Text(IndentText)
		.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
	];

	if (MainParam->IsDirectChildOfMultiParm() && MainParam->GetChildIndex() == 1) 
	{
		int32 CurrentMultiParmInstanceIndex = 0;
		if (MultiParmInstanceIndices.Contains(MainParam->GetParentParmId()))
		{
			MultiParmInstanceIndices[MainParam->GetParentParmId()] += 1;
			CurrentMultiParmInstanceIndex = MultiParmInstanceIndices[MainParam->GetParentParmId()];
			LabelStr = LabelStr + TEXT(" (")  + FString::FromInt(CurrentMultiParmInstanceIndex) + TEXT(")");
		}

		CreateWidgetMultiParmObjectButtons(HorizontalBox, InParams);
	}

	HorizontalBox->AddSlot().Padding(1.0f).VAlign(VAlign_Center).AutoWidth()
	[
		SAssignNew(ExpanderArrow, SButton)
		.ButtonStyle(FEditorStyle::Get(), "NoBorder")
		.ClickMethod(EButtonClickMethod::MouseDown)
		.Visibility(EVisibility::Visible)
		.OnClicked_Lambda([=]()
		{
			MainParam->ExpandButtonClicked();
			
			FHoudiniEngineUtils::UpdateEditorProperties(MainParam, true);

			return FReply::Handled();
		})
		[
			SAssignNew(ExpanderImage, SImage)
			.ColorAndOpacity(FSlateColor::UseForeground())
		]
	];

	
	FText LabelText = FText::FromString(LabelStr);

	HorizontalBox->AddSlot().Padding(1.0f).VAlign(VAlign_Center).AutoWidth()
	[
		SNew(STextBlock)
		.Text(LabelText)
		.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
	];
	
	ExpanderImage->SetImage(
		TAttribute<const FSlateBrush*>::Create(
			TAttribute<const FSlateBrush*>::FGetter::CreateLambda([=]() {
		FName ResourceName;
		if(MainParam->IsExpanded())
		{
			ResourceName = ExpanderArrow->IsHovered() ? "TreeArrow_Expanded_Hovered" : "TreeArrow_Expanded";
		}
		else
		{
			ResourceName = ExpanderArrow->IsHovered() ? "TreeArrow_Collapsed_Hovered" : "TreeArrow_Collapsed";
		}

		return FEditorStyle::GetBrush(ResourceName);
	})));

	if(MainParam->GetFolderType() == EHoudiniFolderParameterType::Simple)
		ExpanderArrow->SetEnabled(false);

}

void FHoudiniParameterDetails::CreateWidgetTabMenu(IDetailCategoryBuilder & HouParameterCategory, FDetailWidgetRow* OutputRow, TArray<UHoudiniParameter*> &InParams)
{
	TArray<UHoudiniParameterFolderList*> FolderListParams;
	if (!CastParameters<UHoudiniParameterFolderList>(InParams, FolderListParams))
		return;

	if (FolderListParams.Num() <= 0)
		return;

	UHoudiniParameterFolderList* MainParam = FolderListParams[0];

	if (!MainParam || MainParam->IsPendingKill())
		return;

	TSharedPtr<SHorizontalBox> HorizontalBox;
	OutputRow = &(HouParameterCategory.AddCustomRow(FText::GetEmpty())
	[
		SAssignNew(HorizontalBox, SHorizontalBox)
	]);

	FString TabIndentationStr = GetIndentationString(MainParam);

	const FText & TabMenuHeaderIndentation = FText::FromString(TabIndentationStr);
	HorizontalBox->AddSlot().AutoWidth()
	[
		SNew(STextBlock)
		.Text(TabMenuHeaderIndentation)
	];

	if (MainParam->IsDirectChildOfMultiParm() && MainParam->GetChildIndex() == 0)
	{
		CreateWidgetMultiParmObjectButtons(HorizontalBox, InParams);
	}

	CurrentTabMenuBox = HorizontalBox;
	bCurrentTabMenu = true;
}

void FHoudiniParameterDetails::CreateWidgetTab(UHoudiniParameterFolder* InFolder)
{
	if (!InFolder || !CurrentTabMenuBox || !CurrentTabMenuFolderList)
		return;

	if (FolderStack.Num() <= 0 || FolderChildCounterStack.Num() <= 0)
		return;

	CurrentTabMenuFolderList->AddTabFolder(InFolder);

	FText FolderLabelText = FText::FromString(InFolder->GetParameterLabel() + FString("             "));

	TArray<bool> & MyFolderQueue = FolderStack.Last();
	TArray<int32> & MyFolderChildCounterQueue = FolderChildCounterStack.Last();

	int32 TabParmId = InFolder->GetParmId();
	bool bChosen = InFolder->IsTab() && InFolder->IsChosen();

	float MaxTabButtonHeight = 12.f;
	float MaxTabButtonWidth = 55.f;

	if (bChosen)
	{
		MaxTabButtonHeight = 18.f;
		MaxTabButtonWidth = 70.f;
	}

	// Lambda function to check if the current tab is checked.
	auto IsCheckedTab = [](bool bChecked)
	{
		return bChecked ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	};

	UHoudiniParameterFolderList* CurrentTabMenuFolderListLocal = CurrentTabMenuFolderList;

	// Lambda for selecting tabs.
	auto OnTabCheckStateChanged = [=](int32 TabId, ECheckBoxState NewState)
	{
		if (NewState == ECheckBoxState::Checked)
		{
			if (CurrentTabMenuFolderListLocal) 
			{
				if (!CurrentTabMenuFolderListLocal->bIsTabMenu || CurrentTabMenuFolderListLocal->TabFolders.Num() < 1)
					return;
				CurrentTabMenuFolderListLocal->bChooseMade;
				InFolder->SetChosen(true);

				for (UHoudiniParameterFolder* NextFolder : CurrentTabMenuFolderListLocal->TabFolders)
				{
					if (InFolder->GetParmId() != NextFolder->GetParmId())
						NextFolder->SetChosen(false);
				}
			}

			FHoudiniEngineUtils::UpdateEditorProperties(InFolder, true);
		}
	};

	// Create a check box UI for the tab.
	CurrentTabMenuBox->AddSlot().Padding(0, 1, 0, 1).MaxWidth(MaxTabButtonWidth).HAlign(HAlign_Left).VAlign(VAlign_Fill).AutoWidth()
		[
			SNew(SVerticalBox) +
			SVerticalBox::Slot().MaxHeight(MaxTabButtonHeight)[

				SNew(SCheckBox)
					.Style(FEditorStyle::Get(), "Property.ToggleButton.Middle")
					.IsChecked_Lambda([=]()
				{
					return IsCheckedTab(bChosen);
				})
					.OnCheckStateChanged_Lambda([OnTabCheckStateChanged, TabParmId](ECheckBoxState NewState)
				{
					return OnTabCheckStateChanged(TabParmId, NewState);
				})
					.Content()
					[
						SNew(STextBlock)
						.Text(FolderLabelText)
					.ToolTipText(GetParameterTooltip(InFolder))
					.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
					]
			]
		];


	if (InFolder->GetTupleSize() > 0)
	{
		MyFolderQueue.Add(bChosen);
		MyFolderChildCounterQueue.Add(InFolder->GetTupleSize());
	}
}

void
FHoudiniParameterDetails::CreateWidgetMultiParm(IDetailCategoryBuilder & HouParameterCategory, TArray<UHoudiniParameter*> &InParams) 
{
	TArray<UHoudiniParameterMultiParm*> MultiParmParams;
	if (!CastParameters<UHoudiniParameterMultiParm>(InParams, MultiParmParams))
		return;

	if (MultiParmParams.Num() <= 0)
		return;

	UHoudiniParameterMultiParm* MainParam = MultiParmParams[0];
	if (!MainParam || MainParam->IsPendingKill())
		return;

	AllMultiParms.Add(MainParam->GetParmId(), MainParam);

	// Create a new detail row
	FDetailWidgetRow * Row = CreateNestedRow(HouParameterCategory, InParams);

	if (!Row) 
	{
		MainParam->SetIsShown(false);
		return;
	}

	MainParam->SetIsShown(true);

	MultiParmInstanceIndices.Add(MainParam->GetParmId(), -1);

	CreateNameWidget(Row, InParams, true);

	auto OnInstanceValueChangedLambda = [MainParam](int32 InValue) 
	{
		if (InValue < 0)
			return;

		int32 ChangesCount = FMath::Abs(MainParam->MultiParmInstanceLastModifyArray.Num() - InValue);

		if (MainParam->MultiParmInstanceLastModifyArray.Num() > InValue)
		{
			for (int32 Idx = 0; Idx < ChangesCount; ++Idx)
				MainParam->RemoveElement(-1);

			MainParam->MarkChanged(true);
		}
		else if (MainParam->MultiParmInstanceLastModifyArray.Num() < InValue)
		{
			for (int32 Idx = 0; Idx < ChangesCount; ++Idx)
				MainParam->InsertElement();

			MainParam->MarkChanged(true);
		}
	};

	// Add multiparm UI.
	TSharedRef<SHorizontalBox> HorizontalBox = SNew(SHorizontalBox);
	TSharedPtr< SNumericEntryBox< int32 > > NumericEntryBox;
	int32 NumericalCount = MainParam->MultiParmInstanceCount;
	HorizontalBox->AddSlot().Padding(2, 2, 5, 2)
		[
			SAssignNew(NumericEntryBox, SNumericEntryBox< int32 >)
			.AllowSpin(true)

		.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
		.OnValueChanged(SNumericEntryBox<int32>::FOnValueChanged::CreateLambda([OnInstanceValueChangedLambda](int32 InValue) {
				OnInstanceValueChangedLambda(InValue);
		}))
		.Value(NumericalCount)
		];

	HorizontalBox->AddSlot().AutoWidth().Padding(2.0f, 0.0f)
		[
			PropertyCustomizationHelpers::MakeAddButton(FSimpleDelegate::CreateLambda([MainParam, MultiParmParams]()
	{
		FScopedTransaction Transaction(
			TEXT(HOUDINI_MODULE_RUNTIME),
			LOCTEXT("HoudiniParameterMultiParamAddInstance", "Houdini Parameter Multi Parameter: Adding an instance"),
			MainParam->GetOuter(), true);

		for (auto& Param : MultiParmParams)
		{
			if (!Param)
				continue;

			// Add a reverse step for redo/undo
			Param->MultiParmInstanceLastModifyArray.Add(EHoudiniMultiParmModificationType::Removed);

			Param->MarkChanged(true);
			Param->Modify();

			if (Param->MultiParmInstanceLastModifyArray.Num() > 0)
				Param->MultiParmInstanceLastModifyArray.RemoveAt(Param->MultiParmInstanceLastModifyArray.Num() - 1);

			Param->InsertElement();

		}
	}),
				LOCTEXT("AddMultiparmInstanceToolTipAddLastInstance", "Add an Instance"), true)
		];

	HorizontalBox->AddSlot().AutoWidth().Padding(2.0f, 0.0f)
		[
			// Remove the last multiparm instance
			PropertyCustomizationHelpers::MakeRemoveButton(FSimpleDelegate::CreateLambda([MainParam, MultiParmParams]()
	{

		FScopedTransaction Transaction(
			TEXT(HOUDINI_MODULE_RUNTIME),
			LOCTEXT("HoudiniParameterMultiParamDeleteInstance", "Houdini Parameter Multi Parameter: Deleting an instance"),
			MainParam->GetOuter(), true);

		for (auto & Param : MultiParmParams)
		{
			//if (Param->MultiParmInstanceNum <= 0)
			//	return;

			TArray<EHoudiniMultiParmModificationType>& LastModifiedArray = Param->MultiParmInstanceLastModifyArray;
			int32 RemovedIndex = LastModifiedArray.Num() - 1;
			while (LastModifiedArray.IsValidIndex(RemovedIndex) && LastModifiedArray[RemovedIndex] == EHoudiniMultiParmModificationType::Removed)
				RemovedIndex -= 1;

			// Add a reverse step for redo/undo
			EHoudiniMultiParmModificationType PreviousModType = EHoudiniMultiParmModificationType::None;
			if (LastModifiedArray.IsValidIndex(RemovedIndex))
			{
				PreviousModType = LastModifiedArray[RemovedIndex];
				LastModifiedArray[RemovedIndex] = EHoudiniMultiParmModificationType::Inserted;
			}

			Param->MarkChanged(true);

			Param->Modify();

			if (LastModifiedArray.IsValidIndex(RemovedIndex))
			{
				LastModifiedArray[RemovedIndex] = PreviousModType;
			}

			Param->RemoveElement(-1);
		}

	}),
				LOCTEXT("RemoveLastMultiParamLastToolTipRemoveLastInstance", "Remove the last instance"), true)

		];

	HorizontalBox->AddSlot().AutoWidth().Padding(2.0f, 0.0f)
		[
			PropertyCustomizationHelpers::MakeEmptyButton(FSimpleDelegate::CreateLambda([MainParam, MultiParmParams]()
	{
		
		FScopedTransaction Transaction(
			TEXT(HOUDINI_MODULE_RUNTIME),
			LOCTEXT("HoudiniParameterMultiParamDeleteAllInstances", "Houdini Parameter Multi Parameter: Deleting all instances"),
			MainParam->GetOuter(), true);

		for (auto & Param : MultiParmParams)
		{
			//if (Param->MultiParmInstanceNum == 0)
				//	return;

			TArray<EHoudiniMultiParmModificationType>& LastModifiedArray = Param->MultiParmInstanceLastModifyArray;
			TArray<int32> IndicesToReverse;

			for (int32 Index = 0; Index < LastModifiedArray.Num(); ++Index)
			{
				if (LastModifiedArray[Index] == EHoudiniMultiParmModificationType::None)
				{
					LastModifiedArray[Index] = EHoudiniMultiParmModificationType::Inserted;
					IndicesToReverse.Add(Index);
				}
			}

			Param->MarkChanged(true);

			Param->Modify();

			for (int32 & Index : IndicesToReverse)
			{
				if (LastModifiedArray.IsValidIndex(Index))
					LastModifiedArray[Index] = EHoudiniMultiParmModificationType::None;
			}


			Param->EmptyElements();
		}

	}),
				LOCTEXT("HoudiniParameterRemoveAllMultiparmInstancesToolTip", "Remove all instances"), true)
		];

	Row->ValueWidget.Widget = HorizontalBox;
	Row->ValueWidget.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH);
}

void
FHoudiniParameterDetails::CreateWidgetMultiParmObjectButtons(TSharedPtr<SHorizontalBox> HorizontalBox, TArray<UHoudiniParameter*> InParams)
{
	
	if (InParams.Num() <= 0)
		return;

	UHoudiniParameter* MainParam = InParams[0];

	if (!MainParam || MainParam->IsPendingKill())
		return;

	if (!HorizontalBox || !AllMultiParms.Contains(MainParam->GetParentParmId()) || !MultiParmInstanceIndices.Contains(MainParam->GetParentParmId()))
		return;

	UHoudiniParameterMultiParm* MainParentMultiParm = AllMultiParms[MainParam->GetParentParmId()];

	if (!MainParentMultiParm)
		return;

	if (!MainParentMultiParm->IsShown())
		return;

	// push all parent multiparm of the InParams to the array
	TArray<UHoudiniParameterMultiParm*> ParentMultiParams;
	for (auto & InParam : InParams) 
	{
		if (!InParam)
			continue;

		if (!MultiParmInstanceIndices.Contains(InParam->GetParentParmId()))
			continue;

		if (InParam->GetChildIndex() == 0)
		{
			UHoudiniParameterMultiParm* ParentMultiParm = AllMultiParms[InParam->GetParentParmId()];

			if (ParentMultiParm)
				ParentMultiParams.Add(ParentMultiParm);
		}
	}


	int32 InstanceIndex = MultiParmInstanceIndices[MainParam->GetParentParmId()];

	TSharedRef<SWidget> AddButton = PropertyCustomizationHelpers::MakeAddButton(FSimpleDelegate::CreateLambda([ParentMultiParams, InstanceIndex]()
	{
		for (auto & ParentParam : ParentMultiParams)
		{
			// Add button call back
			if (!ParentParam)
				continue;

			TArray<EHoudiniMultiParmModificationType>& LastModifiedArray = ParentParam->MultiParmInstanceLastModifyArray;

			if (!LastModifiedArray.IsValidIndex(InstanceIndex))
					continue;

			FScopedTransaction Transaction(
				TEXT(HOUDINI_MODULE_RUNTIME),
				LOCTEXT("HoudiniParameterMultiParmAddBeforeCurInstance", "Houdini Parameter Multi Parm: Adding an instance"),
				ParentParam->GetOuter(), true);


			int32 Index = InstanceIndex;

			// Add a reverse step for undo/redo
			if (Index >= LastModifiedArray.Num())
				LastModifiedArray.Add(EHoudiniMultiParmModificationType::Removed);
			else
				LastModifiedArray.Insert(EHoudiniMultiParmModificationType::Removed, Index);

			ParentParam->MarkChanged(true);
			ParentParam->Modify();

			if (Index >= LastModifiedArray.Num() - 1 && LastModifiedArray.Num())
				LastModifiedArray.RemoveAt(LastModifiedArray.Num() - 1);
			else
				LastModifiedArray.RemoveAt(Index);

			ParentParam->InsertElementAt(InstanceIndex);
			
		}
	}),
		LOCTEXT("HoudiniParameterMultiParamAddBeforeCurrentInstanceToolTip", "Insert an instance before this instance"));


	TSharedRef<SWidget> RemoveButton = PropertyCustomizationHelpers::MakeRemoveButton(FSimpleDelegate::CreateLambda([ParentMultiParams, InstanceIndex]()
	{
		for (auto & ParentParam : ParentMultiParams) 
		{
			FScopedTransaction Transaction(
				TEXT(HOUDINI_MODULE_RUNTIME),
				LOCTEXT("HoudiniParameterMultiParmDeleteCurInstance", "Houdini Parameter Multi Parm: Deleting an instance"),
				ParentParam->GetOuter(), true);


			TArray<EHoudiniMultiParmModificationType>& LastModifiedArray = ParentParam->MultiParmInstanceLastModifyArray;

			int32 Index = InstanceIndex;
			EHoudiniMultiParmModificationType PreviousModType = EHoudiniMultiParmModificationType::None;
			while (LastModifiedArray.IsValidIndex(Index) && LastModifiedArray[Index] == EHoudiniMultiParmModificationType::Removed)
			{
				Index -= 1;
			}

			if (LastModifiedArray.IsValidIndex(Index))
			{
				PreviousModType = LastModifiedArray[Index];
				LastModifiedArray[Index] = EHoudiniMultiParmModificationType::Inserted;
			}

			ParentParam->MarkChanged(true);

			ParentParam->Modify();

			if (LastModifiedArray.IsValidIndex(Index))
			{
				LastModifiedArray[Index] = PreviousModType;
			}

			ParentParam->RemoveElement(InstanceIndex);
		}

	}),
		LOCTEXT("HoudiniParameterMultiParamDeleteCurrentInstanceToolTip", "Remove an instance"), true);


	HorizontalBox->AddSlot().AutoWidth().Padding(0.0f, 0.0f)[AddButton];
	HorizontalBox->AddSlot().AutoWidth().Padding(0.0f, 0.0f)[RemoveButton];

	int32 StartIdx = MainParam->GetParameterType() == EHoudiniParameterType::Folder ? 1 : 0;
	if (MainParam->GetChildIndex() != StartIdx)
	{
		AddButton.Get().SetVisibility(EVisibility::Hidden);
		RemoveButton.Get().SetVisibility(EVisibility::Hidden);
	}
	
}

FString
FHoudiniParameterDetails::GetIndentationString(UHoudiniParameter* InParam) 
{
	FString IndentationString;

	if (!InParam || InParam->IsPendingKill() || !Indentation.Contains(InParam->GetParmId()))
		return IndentationString;

	int32 IndentationLevel = Indentation[InParam->GetParmId()];

	bool bReduceSpaceForMultiParmButtons = false;

	if (InParam->IsDirectChildOfMultiParm() && InParam->GetChildIndex() == 0)
		bReduceSpaceForMultiParmButtons = true;

	if (InParam->GetParameterType() == EHoudiniParameterType::Folder) 
	{
		if (AllFoldersAndTabs.Contains(InParam->GetParentParmId())) 
		{
			UHoudiniParameter* ParentFolderList = AllFoldersAndTabs[InParam->GetParentParmId()];
			if (ParentFolderList->GetChildIndex() == 0 && InParam->GetChildIndex() == 0)
				bReduceSpaceForMultiParmButtons = true;
		}

		if (AllMultiParms.Contains(InParam->GetParentParmId()) && InParam->GetChildIndex() == 1) 
		{
			bReduceSpaceForMultiParmButtons = true;
		
		}
	}

	if (bReduceSpaceForMultiParmButtons)
		IndentationLevel = IndentationLevel - MULTIPARM_INDENTATION_LEVEL;

	for (int32 n = 0; n < IndentationLevel; ++n) 
	{
		IndentationString += BASE_INDENTATION;
	}


	return IndentationString;

}

int32
FHoudiniParameterDetails::GetIndentationLevel(UHoudiniParameter* InParam) 
{
	if (!InParam || InParam->IsPendingKill())
		return 0;

	if (InParam->GetParentParmId() < 0)
		return 0;

	int32 ParentIndent = Indentation.Contains(InParam->GetParentParmId()) ? Indentation[InParam->GetParentParmId()] : 0;

	// Keep the same indentation as its parent folderlist if a parameter is a floder
	if (InParam->GetParameterType() == EHoudiniParameterType::Folder) 
	{
		if (AllMultiParms.Contains(InParam->GetParentParmId())) 
		{
			return ParentIndent + MULTIPARM_INDENTATION_LEVEL;
		}
		else
		{
			if (AllFoldersAndTabs.Contains(InParam->GetParentParmId())) 
			{
				UHoudiniParameterFolder* ParentFolder = Cast<UHoudiniParameterFolder>(AllFoldersAndTabs[InParam->GetParentParmId()]);
				if (ParentFolder && ParentFolder->IsTab())
					ParentIndent += TAB_FOLDER_CHILD_FOLDER_EXTRA_INDENTATION_LEVEL;
			
			}
			
			return ParentIndent;
		}
	}

	// If the parameter is under a multiparm
	if (InParam->IsDirectChildOfMultiParm()) 
	{
		return ParentIndent + MULTIPARM_INDENTATION_LEVEL;
	}

	// If the parameter is under a folder or a tab
	if (AllFoldersAndTabs.Contains(InParam->GetParentParmId())) 
	{
		UHoudiniParameterFolder* ParentFolder = Cast<UHoudiniParameterFolder>(AllFoldersAndTabs[InParam->GetParentParmId()]);
		if (ParentFolder && ParentFolder->IsTab())
			return ParentIndent + INDENTATION_LEVEL + TAB_FOLDER_CHILD_OBJECT_EXTRA_INDENTATION_LEVEL;

		return ParentIndent + INDENTATION_LEVEL;
	}


	return ParentIndent;
}

void
FHoudiniParameterDetails::PruneStack()
{
	for (int32 StackItr = FolderStack.Num() - 1; StackItr >= 0; --StackItr)
	{
		TArray<bool> &CurrentQueue = FolderStack[StackItr];
		TArray<int32> &CurrentCounterQueue = FolderChildCounterStack[StackItr];

		for (int32 QueueItr = CurrentQueue.Num() - 1; QueueItr >= 0; --QueueItr)
		{
			if (CurrentCounterQueue[QueueItr] == 0)
			{
				CurrentQueue.RemoveAt(QueueItr);
				CurrentCounterQueue.RemoveAt(QueueItr);

			}
		}
		if (CurrentQueue.Num() == 0)
		{
			FolderStack.RemoveAt(StackItr);
			FolderChildCounterStack.RemoveAt(StackItr);
		}
	}
}

FText
FHoudiniParameterDetails::GetParameterTooltip(UHoudiniParameter* InParam)
{
	if (!InParam || InParam->IsPendingKill())
		return FText();

	// Tooltip starts with Label (name)
	FString Tooltip = InParam->GetParameterLabel() + TEXT(" (") + InParam->GetParameterName() + TEXT(")");

	// If the parameter has some help, append it
	FString Help = InParam->GetParameterHelp();
	if (!Help.IsEmpty())
		Tooltip += TEXT("\n") + Help;

	// If the parameter has an expression, append it
	if (InParam->HasExpression())
	{
		FString Expr = InParam->GetExpression();
		if (!Expr.IsEmpty())
			Tooltip += TEXT("\nExpression: ") + Expr;
	}

	return FText::FromString(Tooltip);
}

void
FHoudiniParameterDetails::SyncCachedColorRampPoints(UHoudiniParameterRampColor* ColorRampParameter) 
{
	if (!ColorRampParameter)
		return;

	// Do not sync when the Houdini asset component is cooking
	if (FHoudiniEngineUtils::IsHoudiniAssetComponentCooking(ColorRampParameter))
		return;

	TArray<UHoudiniParameterRampColorPoint*> &CachedPoints = ColorRampParameter->CachedPoints;
	TArray<UHoudiniParameterRampColorPoint*> &CurrentPoints = ColorRampParameter->Points;

	bool bCurveNeedsUpdate = false;
	bool bRampParmNeedsUpdate = false;

	int32 Idx = 0;

	while (Idx < CachedPoints.Num() && Idx < CurrentPoints.Num())
	{
		UHoudiniParameterRampColorPoint* CachedPoint = CachedPoints[Idx];
		UHoudiniParameterRampColorPoint* CurrentPoint = CurrentPoints[Idx];

		if (!CachedPoint || !CurrentPoint)
			continue;

		if (CachedPoint->GetPosition() != CurrentPoint->GetPosition())
		{
			if (CurrentPoint->PositionParentParm)
			{
				CurrentPoint->SetPosition(CachedPoint->GetPosition());
				CurrentPoint->PositionParentParm->MarkChanged(true);
				bCurveNeedsUpdate = true;
			}
		}

		if (CachedPoint->GetValue() != CurrentPoint->GetValue())
		{
			if (CurrentPoint->ValueParentParm)
			{
				CurrentPoint->SetValue(CachedPoint->GetValue());
				CurrentPoint->ValueParentParm->MarkChanged(true);
				bCurveNeedsUpdate = true;
			}
		}

		if (CachedPoint->GetInterpolation() != CurrentPoint->GetInterpolation())
		{
			if (CurrentPoint->InterpolationParentParm)
			{
				CurrentPoint->SetInterpolation(CachedPoint->GetInterpolation());
				CurrentPoint->InterpolationParentParm->MarkChanged(true);
				bCurveNeedsUpdate = true;
			}
		}

		Idx += 1;
	}

	// Insert points
	for (int32 IdxCachedPointLeft = Idx; IdxCachedPointLeft < CachedPoints.Num(); ++IdxCachedPointLeft)
	{
		UHoudiniParameterRampColorPoint* CachedPoint = CachedPoints[IdxCachedPointLeft];

		CreateColorRampParameterInsertEvent(
			ColorRampParameter, CachedPoint->Position, CachedPoint->Value, CachedPoint->Interpolation);

		bCurveNeedsUpdate = true;
		bRampParmNeedsUpdate = true;

	}

	// Delete points
	for (int32 IdxCurrentPointLeft = Idx; IdxCurrentPointLeft < CurrentPoints.Num(); ++IdxCurrentPointLeft)
	{
		ColorRampParameter->RemoveElement(IdxCurrentPointLeft);

		UHoudiniParameterRampColorPoint* Point = CurrentPoints[IdxCurrentPointLeft];

		if (!Point)
			continue;

		CreateColorRampParameterDeleteEvent(ColorRampParameter, Point->InstanceIndex);

		bCurveNeedsUpdate = true;
		bRampParmNeedsUpdate = true;
	}


	ColorRampParameter->MarkChanged(bRampParmNeedsUpdate);
}

void 
FHoudiniParameterDetails::SyncCachedFloatRampPoints(UHoudiniParameterRampFloat* FloatRampParameter) 
{
	if (!FloatRampParameter)
		return;

	// Do not sync when the Houdini asset component is cooking
	if (FHoudiniEngineUtils::IsHoudiniAssetComponentCooking(FloatRampParameter))
		return;

	TArray<UHoudiniParameterRampFloatPoint*> &CachedPoints = FloatRampParameter->CachedPoints;
	TArray<UHoudiniParameterRampFloatPoint*> &CurrentPoints = FloatRampParameter->Points;

	int32 Idx = 0;

	while (Idx < CachedPoints.Num() && Idx < CurrentPoints.Num())
	{
		UHoudiniParameterRampFloatPoint* &CachedPoint = CachedPoints[Idx];
		UHoudiniParameterRampFloatPoint* &CurrentPoint = CurrentPoints[Idx];

		if (!CachedPoint || !CurrentPoint)
			continue;

		if (CachedPoint->GetPosition() != CurrentPoint->GetPosition()) 
		{
			if (CurrentPoint->PositionParentParm) 
			{
				CurrentPoint->SetPosition(CachedPoint->GetPosition());
				CurrentPoint->PositionParentParm->MarkChanged(true);
			}
		}

		if (CachedPoint->GetValue() != CurrentPoint->GetValue()) 
		{
			if (CurrentPoint->ValueParentParm) 
			{
				CurrentPoint->SetValue(CachedPoint->GetValue());
				CurrentPoint->ValueParentParm->MarkChanged(true);
			}
		}

		if (CachedPoint->GetInterpolation() != CurrentPoint->GetInterpolation()) 
		{
			if (CurrentPoint->InterpolationParentParm) 
			{
				CurrentPoint->SetInterpolation(CachedPoint->GetInterpolation());
				CurrentPoint->InterpolationParentParm->MarkChanged(true);
			}
		}

		Idx += 1;
	}

	// Insert points
	for (int32 IdxCachedPointLeft = Idx; IdxCachedPointLeft < CachedPoints.Num(); ++IdxCachedPointLeft) 
	{
		UHoudiniParameterRampFloatPoint *&CachedPoint = CachedPoints[IdxCachedPointLeft];
		if (!CachedPoint)
			continue;

		CreateFloatRampParameterInsertEvent(
			FloatRampParameter, CachedPoint->Position, CachedPoint->Value, CachedPoint->Interpolation);

		FloatRampParameter->MarkChanged(true);
	}

	// Remove points
	for (int32 IdxCurrentPointLeft = Idx; IdxCurrentPointLeft < CurrentPoints.Num(); ++IdxCurrentPointLeft) 
	{
		FloatRampParameter->RemoveElement(IdxCurrentPointLeft);

		UHoudiniParameterRampFloatPoint* Point = CurrentPoints[IdxCurrentPointLeft];

		if (!Point)
			continue;

		CreateFloatRampParameterDeleteEvent(FloatRampParameter, Point->InstanceIndex);

		FloatRampParameter->MarkChanged(true);
	}
}

void 
FHoudiniParameterDetails::CreateFloatRampParameterDeleteEvent(UHoudiniParameterRampFloat* InParam, const int32 &InDeleteIndex) 
{
	if (!InParam || InParam->IsPendingKill())
		return;

	UHoudiniParameterRampModificationEvent* DeleteEvent = NewObject<UHoudiniParameterRampModificationEvent>(
		InParam, UHoudiniParameterRampModificationEvent::StaticClass());

	if (!DeleteEvent)
		return;

	DeleteEvent->SetFloatRampEvent();
	DeleteEvent->SetDeleteEvent();
	DeleteEvent->DeleteInstanceIndex = InDeleteIndex;

	InParam->ModificationEvents.Add(DeleteEvent);
}

void
FHoudiniParameterDetails::CreateColorRampParameterDeleteEvent(UHoudiniParameterRampColor* InParam, const int32 &InDeleteIndex)
{
	if (!InParam || InParam->IsPendingKill())
		return;

	UHoudiniParameterRampModificationEvent* DeleteEvent = NewObject<UHoudiniParameterRampModificationEvent>(
		InParam, UHoudiniParameterRampModificationEvent::StaticClass());

	if (!DeleteEvent)
		return;

	DeleteEvent->SetColorRampEvent();
	DeleteEvent->SetDeleteEvent();
	DeleteEvent->DeleteInstanceIndex = InDeleteIndex;

	InParam->ModificationEvents.Add(DeleteEvent);
}

void 
FHoudiniParameterDetails::CreateFloatRampParameterInsertEvent(UHoudiniParameterRampFloat* InParam,
	const float& InPosition, const float& InValue, const EHoudiniRampInterpolationType &InInterp) 
{
	if (!InParam || InParam->IsPendingKill())
		return;

	UHoudiniParameterRampModificationEvent* InsertEvent = NewObject<UHoudiniParameterRampModificationEvent>(
		InParam, UHoudiniParameterRampModificationEvent::StaticClass());

	if (!InsertEvent)
		return;

	InsertEvent->SetFloatRampEvent();
	InsertEvent->SetInsertEvent();
	InsertEvent->InsertPosition = InPosition;
	InsertEvent->InsertFloat = InValue;
	InsertEvent->InsertInterpolation = InInterp;

	InParam->ModificationEvents.Add(InsertEvent);
}

void 
FHoudiniParameterDetails::CreateColorRampParameterInsertEvent(UHoudiniParameterRampColor* InParam,
	const float& InPosition, const FLinearColor& InColor, const EHoudiniRampInterpolationType &InInterp) 
{
	if (!InParam || InParam->IsPendingKill())
		return;

	UHoudiniParameterRampModificationEvent* InsertEvent = NewObject<UHoudiniParameterRampModificationEvent>(
		InParam, UHoudiniParameterRampModificationEvent::StaticClass());

	if (!InsertEvent)
		return;

	InsertEvent->SetColorRampEvent();
	InsertEvent->SetInsertEvent();
	InsertEvent->InsertPosition = InPosition;
	InsertEvent->InsertColor = InColor;
	InsertEvent->InsertInterpolation = InInterp;

	InParam->ModificationEvents.Add(InsertEvent);
}

void
FHoudiniParameterDetails:: ReplaceAllFloatRampParameterPointsWithMainParameter(TArray<UHoudiniParameterRampFloat*>& FloatRampParameters)
{
	if (FloatRampParameters.Num() < 1)
		return;

	UHoudiniParameterRampFloat* MainParam = FloatRampParameters[0];

	if (!MainParam)
		return;

	if (FHoudiniEngineUtils::IsHoudiniAssetComponentCooking(MainParam))
		return;

	for (int32 Idx = 1; Idx < FloatRampParameters.Num(); ++Idx) 
	{
		UHoudiniParameterRampFloat* NextFloatRampParameter = FloatRampParameters[Idx];

		if (!NextFloatRampParameter)
			continue;

		FHoudiniParameterDetails::ReplaceFloatRampParameterPointsWithMainParameter(MainParam, NextFloatRampParameter);
	}
}

void 
FHoudiniParameterDetails::ReplaceAllFloatRampParameterPointsWithMainParameter(TArray<TWeakObjectPtr<UHoudiniParameterRampFloat>>& FloatRampParameters) 
{
	if (FloatRampParameters.Num() < 1)
		return;

	if (!FloatRampParameters[0].IsValid())
		return;

	UHoudiniParameterRampFloat* MainParam = FloatRampParameters[0].Get();

	if (!MainParam)
		return;

	if (FHoudiniEngineUtils::IsHoudiniAssetComponentCooking(MainParam))
		return;


	for (int32 Idx = 1; Idx < FloatRampParameters.Num(); ++Idx)
	{
		if (!FloatRampParameters[Idx].IsValid())
			continue;

		UHoudiniParameterRampFloat* NextFloatRampParameter = FloatRampParameters[Idx].Get();

		if (!NextFloatRampParameter)
			continue;

		FHoudiniParameterDetails::ReplaceFloatRampParameterPointsWithMainParameter(MainParam, NextFloatRampParameter);
	}

}

void
FHoudiniParameterDetails:: ReplaceFloatRampParameterPointsWithMainParameter(UHoudiniParameterRampFloat* Param, UHoudiniParameterRampFloat* MainParam)
{
	if (!Param || !MainParam)
		return;

	if (FHoudiniEngineUtils::IsHoudiniAssetComponentCooking(Param))
		return;

	// Use Synced points if the MainParam is on auto update mode
	// Use Cached points if the Mainparam is on manual update mode

	TArray<UHoudiniParameterRampFloatPoint*> & MainPoints = MainParam->IsAutoUpdate() ? MainParam->Points : MainParam->CachedPoints;

	if (Param->IsAutoUpdate())
	{
		TArray<UHoudiniParameterRampFloatPoint*> & Points = Param->Points;

		int32 PointIdx = 0;
		while (MainPoints.IsValidIndex(PointIdx) && Points.IsValidIndex(PointIdx))
		{
			UHoudiniParameterRampFloatPoint*& MainPoint = MainPoints[PointIdx];
			UHoudiniParameterRampFloatPoint*& Point = Points[PointIdx];

			if (!MainPoint || !Point)
				continue;

			if (MainPoint->GetPosition() != Point->GetPosition())
			{
				if (Point->PositionParentParm)
				{
					Point->SetPosition(MainPoint->GetPosition());
					Point->PositionParentParm->MarkChanged(true);
				}
			}

			if (MainPoint->GetValue() != Point->GetValue())
			{
				if (Point->ValueParentParm)
				{
					Point->SetValue(MainPoint->GetValue());
					Point->ValueParentParm->MarkChanged(true);
				}
			}

			if (MainPoint->GetInterpolation() != Point->GetInterpolation())
			{
				if (Point->InterpolationParentParm)
				{
					Point->SetInterpolation(MainPoint->GetInterpolation());
					Point->InterpolationParentParm->MarkChanged(true);
				}
			}

			PointIdx += 1;
		}

		int32 PointInsertIdx = PointIdx;
		int32 PointDeleteIdx = PointIdx;

		// skip the pending modification events
		for (auto & Event : Param->ModificationEvents)
		{
			if (!Event)
				continue;

			if (Event->IsInsertEvent())
				PointInsertIdx += 1;

			if (Event->IsDeleteEvent())
				PointDeleteIdx += 1;
		}

		// There are more points in MainPoints array
		for (PointInsertIdx; PointInsertIdx < MainPoints.Num(); ++PointInsertIdx)
		{
			UHoudiniParameterRampFloatPoint*& NextMainPoint = MainPoints[PointInsertIdx];

			if (!NextMainPoint)
				continue;

			FHoudiniParameterDetails::CreateFloatRampParameterInsertEvent(Param,
				NextMainPoint->GetPosition(), NextMainPoint->GetValue(), NextMainPoint->GetInterpolation());

			Param->MarkChanged(true);
		}

		// There are more points in Points array
		for (PointDeleteIdx; PointDeleteIdx < Points.Num(); ++PointDeleteIdx)
		{
			UHoudiniParameterRampFloatPoint*& NextPoint = Points[PointDeleteIdx];

			if (!NextPoint)
				continue;

			FHoudiniParameterDetails::CreateFloatRampParameterDeleteEvent(Param, NextPoint->InstanceIndex);

			Param->MarkChanged(true);
		}

	}
	else
	{
		TArray<UHoudiniParameterRampFloatPoint*> &Points = Param->CachedPoints;

		int32 PointIdx = 0;
		while (MainPoints.IsValidIndex(PointIdx) && Points.IsValidIndex(PointIdx))
		{
			UHoudiniParameterRampFloatPoint*& MainPoint = MainPoints[PointIdx];
			UHoudiniParameterRampFloatPoint*& Point = Points[PointIdx];

			if (!MainPoint || !Point)
				continue;

			if (Point->Position != MainPoint->Position)
			{
				Point->Position = MainPoint->Position;
				Param->bCaching = true;
			}

			if (Point->Value != MainPoint->Value)
			{
				Point->Value = MainPoint->Value;
				Param->bCaching = true;
			}

			if (Point->Interpolation != MainPoint->Interpolation)
			{
				Point->Interpolation = MainPoint->Interpolation;
				Param->bCaching = true;
			}

			PointIdx += 1;
		}

		// There are more points in MainPoints array
		for (int32 MainPointsLeftIdx = PointIdx; MainPointsLeftIdx < MainPoints.Num(); ++MainPointsLeftIdx)
		{
			UHoudiniParameterRampFloatPoint* NextMainPoint = MainPoints[MainPointsLeftIdx];

			if (!NextMainPoint)
				continue;

			UHoudiniParameterRampFloatPoint* NewCachedPoint = NewObject<UHoudiniParameterRampFloatPoint>(Param, UHoudiniParameterRampFloatPoint::StaticClass());

			if (!NewCachedPoint)
				continue;

			NewCachedPoint->Position = NextMainPoint->GetPosition();
			NewCachedPoint->Value = NextMainPoint->GetValue();
			NewCachedPoint->Interpolation = NextMainPoint->GetInterpolation();

			Points.Add(NewCachedPoint);

			Param->bCaching = true;
		}

		// there are more points in Points array
		for (int32 PointsLeftIdx = PointIdx; PointIdx < Points.Num(); ++PointIdx)
		{
			Points.Pop();
			Param->bCaching = true;
		}
	}

}


void 
FHoudiniParameterDetails::ReplaceAllColorRampParameterPointsWithMainParameter(TArray<UHoudiniParameterRampColor*>& ColorRampParameters) 
{
	if (ColorRampParameters.Num() < 1)
		return;

	UHoudiniParameterRampColor* MainParam = ColorRampParameters[0];

	if (!MainParam)
		return;

	if (FHoudiniEngineUtils::IsHoudiniAssetComponentCooking(MainParam))
		return;

	for (int32 Idx = 1; Idx < ColorRampParameters.Num(); ++Idx) 
	{
		UHoudiniParameterRampColor* NextColorRampParam = ColorRampParameters[Idx];

		if (!NextColorRampParam)
			continue;

		FHoudiniParameterDetails::ReplaceColorRampParameterPointsWithMainParameter(MainParam, NextColorRampParam);
	}
}

void 
FHoudiniParameterDetails::ReplaceAllColorRampParameterPointsWithMainParameter(TArray<TWeakObjectPtr<UHoudiniParameterRampColor>>& ColorRampParameters) 
{
	if (ColorRampParameters.Num() < 1)
		return;

	if (!ColorRampParameters[0].IsValid())
		return;

	UHoudiniParameterRampColor* MainParam = ColorRampParameters[0].Get();

	if (!MainParam)
		return;

	if (FHoudiniEngineUtils::IsHoudiniAssetComponentCooking(MainParam))
		return;

	for (int32 Idx = 1; Idx < ColorRampParameters.Num(); ++Idx)
	{
		if (!ColorRampParameters[Idx].IsValid())
			continue;

		UHoudiniParameterRampColor* NextColorRampParameter = ColorRampParameters[Idx].Get();

		if (!NextColorRampParameter)
			continue;

		FHoudiniParameterDetails::ReplaceColorRampParameterPointsWithMainParameter(MainParam, NextColorRampParameter);

	}

}

void 
FHoudiniParameterDetails::ReplaceColorRampParameterPointsWithMainParameter(UHoudiniParameterRampColor* Param, UHoudiniParameterRampColor* MainParam) 
{
	if (!Param || !MainParam)
		return;

	if (FHoudiniEngineUtils::IsHoudiniAssetComponentCooking(Param))
		return;

	// Use Synced points if the MainParam is on auto update mode
	// Use Cached points if the Mainparam is on manual update mode

	TArray<UHoudiniParameterRampColorPoint*> & MainPoints = MainParam->IsAutoUpdate() ? MainParam->Points : MainParam->CachedPoints;

	if (Param->IsAutoUpdate())
	{
		TArray<UHoudiniParameterRampColorPoint*> & Points = Param->Points;

		int32 PointIdx = 0;
		while (MainPoints.IsValidIndex(PointIdx) && Points.IsValidIndex(PointIdx))
		{
			UHoudiniParameterRampColorPoint*& MainPoint = MainPoints[PointIdx];
			UHoudiniParameterRampColorPoint*& Point = Points[PointIdx];

			if (!MainPoint || !Point)
				continue;

			if (MainPoint->GetPosition() != Point->GetPosition())
			{
				if (Point->PositionParentParm)
				{
					Point->SetPosition(MainPoint->GetPosition());
					Point->PositionParentParm->MarkChanged(true);
				}
			}

			if (MainPoint->GetValue() != Point->GetValue())
			{
				if (Point->ValueParentParm)
				{
					Point->SetValue(MainPoint->GetValue());
					Point->ValueParentParm->MarkChanged(true);
				}
			}

			if (MainPoint->GetInterpolation() != Point->GetInterpolation())
			{
				if (Point->InterpolationParentParm)
				{
					Point->SetInterpolation(MainPoint->GetInterpolation());
					Point->InterpolationParentParm->MarkChanged(true);
				}
			}

			PointIdx += 1;

		}

		int32 PointInsertIdx = PointIdx;
		int32 PointDeleteIdx = PointIdx;

		// skip the pending modification events
		for (auto & Event : Param->ModificationEvents)
		{
			if (!Event)
				continue;

			if (Event->IsInsertEvent())
				PointInsertIdx += 1;

			if (Event->IsDeleteEvent())
				PointDeleteIdx += 1;
		}

		// There are more points in MainPoints array
		for (PointInsertIdx; PointInsertIdx < MainPoints.Num(); ++PointInsertIdx)
		{
			UHoudiniParameterRampColorPoint*& NextMainPoint = MainPoints[PointInsertIdx];

			if (!NextMainPoint)
				continue;

			FHoudiniParameterDetails::CreateColorRampParameterInsertEvent(Param,
				NextMainPoint->GetPosition(), NextMainPoint->GetValue(), NextMainPoint->GetInterpolation());

			Param->MarkChanged(true);
		}

		// There are more points in Points array
		for (PointDeleteIdx; PointDeleteIdx < Points.Num(); ++PointDeleteIdx)
		{
			UHoudiniParameterRampColorPoint*& NextPoint = Points[PointDeleteIdx];

			if (!NextPoint)
				continue;

			FHoudiniParameterDetails::CreateColorRampParameterDeleteEvent(Param, NextPoint->InstanceIndex);

			Param->MarkChanged(true);
		}
	}
	else
	{
		TArray<UHoudiniParameterRampColorPoint*> &Points = Param->CachedPoints;

		int32 PointIdx = 0;
		while (MainPoints.IsValidIndex(PointIdx) && Points.IsValidIndex(PointIdx))
		{
			UHoudiniParameterRampColorPoint*& MainPoint = MainPoints[PointIdx];
			UHoudiniParameterRampColorPoint*& Point = Points[PointIdx];

			if (!MainPoint || !Point)
				continue;

			if (Point->Position != MainPoint->Position)
			{
				Point->Position = MainPoint->Position;
				Param->bCaching = true;
			}

			if (Point->Value != MainPoint->Value)
			{
				Point->Value = MainPoint->Value;
				Param->bCaching = true;
			}

			if (Point->Interpolation != MainPoint->Interpolation)
			{
				Point->Interpolation = MainPoint->Interpolation;
				Param->bCaching = true;
			}

			PointIdx += 1;
		}

		// There are more points in Main Points array.
		for (int32 MainPointsLeftIdx = PointIdx; MainPointsLeftIdx < MainPoints.Num(); ++MainPointsLeftIdx)
		{
			UHoudiniParameterRampColorPoint* NextMainPoint = MainPoints[MainPointsLeftIdx];

			if (!NextMainPoint)
				continue;

			UHoudiniParameterRampColorPoint* NewCachedPoint = NewObject<UHoudiniParameterRampColorPoint>(Param, UHoudiniParameterRampColorPoint::StaticClass());

			if (!NewCachedPoint)
				continue;

			NewCachedPoint->Position = NextMainPoint->Position;
			NewCachedPoint->Value = NextMainPoint->Value;
			NewCachedPoint->Interpolation = NextMainPoint->Interpolation;

			Points.Add(NewCachedPoint);

			Param->bCaching = true;
		}

		// There are more points in Points array
		for (int32 PointsleftIdx = PointIdx; PointIdx < MainPoints.Num(); ++PointsleftIdx)
		{
			Points.Pop();

			Param->bCaching = true;
		}
	}


}

#undef LOCTEXT_NAMESPACE