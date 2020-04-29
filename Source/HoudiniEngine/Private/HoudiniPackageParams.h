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

#include "UObject/ObjectMacros.h"

class UStaticMesh;

enum class EPackageMode : int8
{
	CookToLevel,
	CookToTemp,
	Bake
};

enum class EPackageReplaceMode : int8
{
	CreateNewAssets,
	ReplaceExistingAssets
};

class HOUDINIENGINE_API FHoudiniPackageParams
{
public:
	//
	FHoudiniPackageParams();
	//
	~FHoudiniPackageParams();

	// Helper functions returning the default behavior expected when cooking mesh
	static EPackageMode GetDefaultStaticMeshesCookMode() { return EPackageMode::CookToTemp; };
	// Helper functions returning the default behavior expected when cooking materials or textures
	static EPackageMode GetDefaultMaterialAndTextureCookMode() { return EPackageMode::CookToTemp; };
	// Helper functions returning the default behavior for replacing existing package
	static EPackageReplaceMode GetDefaultReplaceMode() { return EPackageReplaceMode::ReplaceExistingAssets; };

	// Returns the name for the package depending on the mode
	FString GetPackageName() const;
	// Returns the package's path depending on the mode
	FString GetPackagePath() const;
	// Returns the object flags corresponding to the current package mode
	EObjectFlags GetObjectFlags() const;

	// Helper function to create a Package for a given object
	UPackage* CreatePackageForObject(FString& OutPackageName) const;

	// Helper function to create an object and its package
	template<typename T> T* CreateObjectAndPackage();

public:

	// The current cook/baking mode
	EPackageMode PackageMode;
	// How to handle existing assets? replace or rename?
	EPackageReplaceMode ReplaceMode;

	// When cooking in bake mode - folder to create assets in
	FString BakeFolder;
	// When cooking in temp mode - folder to create assets in
	FString TempCookFolder;
	
	// Package to save to
	UObject* OuterPackage;

	// Name of the package we want to create
	// If null, we'll generate one from ASSET_OBJ_GEO_SPLIT
	FString ObjectName;

	// Name of the HDA
	FString HoudiniAssetName;

	//
	int32	ObjectId;
	//
	int32	GeoId;
	//
	int32	PartId;
	//
	FString SplitStr;

	// GUID used for the owner
	FGuid ComponentGUID;
};
