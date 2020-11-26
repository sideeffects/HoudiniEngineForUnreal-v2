/*
* Copyright (c) <2017> Side Effects Software Inc.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*
*/

#include "HoudiniInstancedActorComponent.h"

#include "HoudiniMeshSplitInstancerComponent.h"
#include "HoudiniRuntimeSettings.h"
#include "HoudiniEngineRuntimePrivatePCH.h"

#include "HoudiniPluginSerializationVersion.h"
#include "HoudiniCompatibilityHelpers.h"

#include "UObject/DevObjectVersion.h"
#include "Serialization/CustomVersion.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"

#include "Internationalization/Internationalization.h"

#define LOCTEXT_NAMESPACE HOUDINI_LOCTEXT_NAMESPACE 

UHoudiniInstancedActorComponent::UHoudiniInstancedActorComponent( const FObjectInitializer& ObjectInitializer )
: Super( ObjectInitializer )
, InstancedObject( nullptr )
{
	//
	// 	Set default component properties.
	//
	Mobility = EComponentMobility::Static;
	bCanEverAffectNavigation = true;
	bNeverNeedsRenderUpdate = false;
	Bounds = FBox(ForceInitToZero);
}


void
UHoudiniInstancedActorComponent::Serialize(FArchive& Ar)
{
	int64 InitialOffset = Ar.Tell();

	bool bLegacyComponent = false;
	if (Ar.IsLoading())
	{
		int32 Ver = Ar.CustomVer(FHoudiniCustomSerializationVersion::GUID);
		if (Ver < VER_HOUDINI_PLUGIN_SERIALIZATION_VERSION_V2_BASE && Ver >= VER_HOUDINI_PLUGIN_SERIALIZATION_VERSION_BASE)
		{
			bLegacyComponent = true;
		}
	}

	if (bLegacyComponent)
	{
		// Legacy serialization
		// Either try to convert or skip depending on the setting value
		const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault<UHoudiniRuntimeSettings>();
		bool bEnableBackwardCompatibility = HoudiniRuntimeSettings->bEnableBackwardCompatibility;
		if (bEnableBackwardCompatibility)
		{
			HOUDINI_LOG_WARNING(TEXT("Loading deprecated version of UHoudiniInstancedActorComponent : converting v1 object to v2."));

			Super::Serialize(Ar);

			UHoudiniInstancedActorComponent_V1* CompatibilityIAC = NewObject<UHoudiniInstancedActorComponent_V1>();
			CompatibilityIAC->Serialize(Ar);
			CompatibilityIAC->UpdateFromLegacyData(this);
		}
		else
		{
			HOUDINI_LOG_WARNING(TEXT("Loading deprecated version of UHoudiniInstancedActorComponent : serialization will be skipped."));

			Super::Serialize(Ar);

			// Skip v1 Serialized data
			if (FLinker* Linker = Ar.GetLinker())
			{
				int32 const ExportIndex = this->GetLinkerIndex();
				FObjectExport& Export = Linker->ExportMap[ExportIndex];
				Ar.Seek(InitialOffset + Export.SerialSize);
				return;
			}
		}
	}
	else
	{
		// Normal v2 serialization
		Super::Serialize(Ar);
	}
}


void UHoudiniInstancedActorComponent::OnComponentDestroyed( bool bDestroyingHierarchy )
{
    ClearAllInstances();
    Super::OnComponentDestroyed( bDestroyingHierarchy );
}


void 
UHoudiniInstancedActorComponent::AddReferencedObjects(UObject * InThis, FReferenceCollector & Collector )
{
    UHoudiniInstancedActorComponent * ThisHIAC = Cast< UHoudiniInstancedActorComponent >(InThis);
    if ( ThisHIAC && !ThisHIAC->IsPendingKill() )
    {
        if ( ThisHIAC->InstancedObject && !ThisHIAC->InstancedObject->IsPendingKill() )
            Collector.AddReferencedObject( ThisHIAC->InstancedObject, ThisHIAC );

        Collector.AddReferencedObjects(ThisHIAC->InstancedActors, ThisHIAC );
    }
}


int32
UHoudiniInstancedActorComponent::AddInstance(const FTransform& InstanceTransform, AActor * NewActor)
{
	if (!NewActor || NewActor->IsPendingKill())
		return -1;

	NewActor->AttachToComponent(this, FAttachmentTransformRules::KeepRelativeTransform);
	NewActor->SetActorRelativeTransform(InstanceTransform);
	return InstancedActors.Add(NewActor);
}


bool
UHoudiniInstancedActorComponent::SetInstanceAt(const int32& Idx, const FTransform& InstanceTransform, AActor * NewActor)
{
	if (!NewActor || NewActor->IsPendingKill())
		return false;

	if (!InstancedActors.IsValidIndex(Idx))
		return false;

	NewActor->AttachToComponent(this, FAttachmentTransformRules::KeepRelativeTransform);
	NewActor->SetActorRelativeTransform(InstanceTransform);
	NewActor->RegisterAllComponents();
	InstancedActors[Idx] = NewActor;

	return true;
}


bool
UHoudiniInstancedActorComponent::SetInstanceTransformAt(const int32& Idx, const FTransform& InstanceTransform)
{
	if (!InstancedActors.IsValidIndex(Idx))
		return false;

	InstancedActors[Idx]->AttachToComponent(this, FAttachmentTransformRules::KeepRelativeTransform);
	InstancedActors[Idx]->SetActorRelativeTransform(InstanceTransform);

	return true;
}


void 
UHoudiniInstancedActorComponent::ClearAllInstances()
{
    for ( AActor* Instance : InstancedActors )
    {
        if ( Instance && !Instance->IsPendingKill() )
            Instance->Destroy();
    }
    InstancedActors.Empty();
}


void
UHoudiniInstancedActorComponent::SetNumberOfInstances(const int32& NewInstanceNum)
{
	int32 OldInstanceNum = InstancedActors.Num();

	// If we want less instances than we already have, destroy the extra properly
	if (NewInstanceNum < OldInstanceNum)
	{
		for (int32 Idx = NewInstanceNum - 1; Idx < InstancedActors.Num(); Idx++)
		{
			AActor* Instance = InstancedActors.IsValidIndex(Idx) ? InstancedActors[Idx] : nullptr;
			if (Instance && !Instance->IsPendingKill())
				Instance->Destroy();
		}
	}
	
	// Grow the array with nulls if needed
	InstancedActors.SetNumZeroed(NewInstanceNum);
}


void 
UHoudiniInstancedActorComponent::OnComponentCreated()
{
    Super::OnComponentCreated();

    // If our instances are parented to another actor we should duplicate them
    bool bNeedDuplicate = false;
    for (auto CurrentInstance : InstancedActors)
    {
        if ( !CurrentInstance || CurrentInstance->IsPendingKill() )
            continue;

        if ( CurrentInstance->GetAttachParentActor() != GetOwner() )
            bNeedDuplicate = true;
    }

    if ( !bNeedDuplicate )
        return;

	// TODO: CHECK ME!
    // We need to duplicate our instances
    TArray<AActor*> SourceInstances = InstancedActors;
    InstancedActors.Empty();
    for (AActor* CurrentInstance : SourceInstances)
    {
        if ( !CurrentInstance || CurrentInstance->IsPendingKill() )
            continue;

        FTransform InstanceTransform;
        if ( CurrentInstance->GetRootComponent() )
            InstanceTransform = CurrentInstance->GetRootComponent()->GetRelativeTransform();

       // AddInstance( InstanceTransform );
    }
}

#undef LOCTEXT_NAMESPACE