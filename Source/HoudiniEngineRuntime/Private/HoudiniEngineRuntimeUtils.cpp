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

#include "HoudiniEngineRuntimeUtils.h"

#include "EngineUtils.h"

FString
FHoudiniEngineRuntimeUtils::GetLibHAPIName()
{
	static const FString LibHAPIName =

#if PLATFORM_WINDOWS
		HAPI_LIB_OBJECT_WINDOWS;
#elif PLATFORM_MAC
		HAPI_LIB_OBJECT_MAC;
#elif PLATFORM_LINUX
		HAPI_LIB_OBJECT_LINUX;
#else
		TEXT("");
#endif

	return LibHAPIName;
}


void 
FHoudiniEngineRuntimeUtils::GetBoundingBoxesFromActors(const TArray<AActor*> InActors, TArray<FBox>& OutBBoxes)
{
	OutBBoxes.Empty();

	for (auto CurrentActor : InActors)
	{
		if (!CurrentActor || CurrentActor->IsPendingKill())
			continue;

		OutBBoxes.Add(CurrentActor->GetComponentsBoundingBox(true, true));
	}
}


bool 
FHoudiniEngineRuntimeUtils::FindActorsOfClassInBounds(UWorld* World, TSubclassOf<AActor> ActorType, const TArray<FBox>& BBoxes, const TArray<AActor*>* ExcludeActors, TArray<AActor*>& OutActors)
{
	if (!IsValid(World))
		return false;
	
	OutActors.Empty();
	for (TActorIterator<AActor> ActorItr(World); ActorItr; ++ActorItr)
	{
		AActor* CurrentActor = *ActorItr;
		if (!IsValid(CurrentActor))
			continue;
		
		if (!CurrentActor->GetClass()->IsChildOf(ActorType.Get()))
			continue;

		if (ExcludeActors && ExcludeActors->Contains(CurrentActor))
			continue;

		// Special case
		// Ignore the SkySpheres?
		FString ClassName = CurrentActor->GetClass() ? CurrentActor->GetClass()->GetName() : FString();
		if (ClassName.Contains("BP_Sky_Sphere"))
			continue;

		FBox ActorBounds = CurrentActor->GetComponentsBoundingBox(true);
		for (auto InBounds : BBoxes)
		{
			// Check if both actor's bounds intersects
			if (!ActorBounds.Intersect(InBounds))
				continue;

			OutActors.Add(CurrentActor);
			break;
		}
	}

	return true;
}
