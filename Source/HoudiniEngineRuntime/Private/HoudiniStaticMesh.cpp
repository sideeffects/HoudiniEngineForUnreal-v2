/*
* Copyright (c) <2021> Side Effects Software Inc.
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

#include "HoudiniStaticMesh.h"

UHoudiniStaticMesh::UHoudiniStaticMesh(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
	bHasNormals = false;
	bHasTangents = false;
	bHasColors = false;
	NumUVLayers = false;
	bHasPerFaceMaterials = false;
}

void UHoudiniStaticMesh::Initialize(uint32 InNumVertices, uint32 InNumTriangles, uint32 InNumUVLayers, uint32 InInitialNumStaticMaterials, bool bInHasNormals, bool bInHasTangents, bool bInHasColors, bool bInHasPerFaceMaterials)
{
	// Initialize the vertex positions and triangle indices arrays
	VertexPositions.Init(FVector::ZeroVector, InNumVertices);
	TriangleIndices.Init(FIntVector(-1, -1, -1), InNumTriangles);
	if (InInitialNumStaticMaterials > 0)
		StaticMaterials.Init(FStaticMaterial(), InInitialNumStaticMaterials);
	else
		StaticMaterials.Empty();

	SetNumUVLayers(InNumUVLayers);
	SetHasNormals(bInHasNormals);
	SetHasTangents(bInHasTangents);
	SetHasColors(bInHasColors);
	SetHasPerFaceMaterials(bInHasPerFaceMaterials);
}

void UHoudiniStaticMesh::SetHasPerFaceMaterials(bool bInHasPerFaceMaterials)
{
	bHasPerFaceMaterials = bInHasPerFaceMaterials;
	if (bHasPerFaceMaterials)
		MaterialIDsPerTriangle.Init(-1, GetNumTriangles());
	else
		MaterialIDsPerTriangle.Empty();
}

void UHoudiniStaticMesh::SetHasNormals(bool bInHasNormals)
{
	bHasNormals = bInHasNormals;
	if (bHasNormals)
		VertexInstanceNormals.Init(FVector(0, 0, 1), GetNumVertexInstances());
	else
		VertexInstanceNormals.Empty();
}

void UHoudiniStaticMesh::SetHasTangents(bool bInHasTangents)
{
	bHasTangents = bInHasTangents;
	if (bHasTangents)
	{
		VertexInstanceUTangents.Init(FVector(1, 0, 0), GetNumVertexInstances());
		VertexInstanceVTangents.Init(FVector(0, 1, 0), GetNumVertexInstances());
	}
	else
	{
		VertexInstanceUTangents.Empty();
		VertexInstanceVTangents.Empty();
	}
}

void UHoudiniStaticMesh::SetHasColors(bool bInHasColors)
{
	bHasColors = bInHasColors;
	if (bHasColors)
		VertexInstanceColors.Init(FColor(127, 127, 127), GetNumVertexInstances());
	else
		VertexInstanceColors.Empty();
}

void UHoudiniStaticMesh::SetNumUVLayers(uint32 InNumUVLayers)
{
	NumUVLayers = InNumUVLayers;
	if (NumUVLayers > 0)
		VertexInstanceUVs.Init(FVector2D::ZeroVector, GetNumVertexInstances() * NumUVLayers);
	else
		VertexInstanceUVs.Empty();
}

void UHoudiniStaticMesh::SetNumStaticMaterials(uint32 InNumMaterials)
{
	if (InNumMaterials > 0)
		StaticMaterials.SetNum(InNumMaterials);
	else
		StaticMaterials.Empty();
}

void UHoudiniStaticMesh::SetVertexPosition(uint32 InVertexIndex, const FVector& InPosition)
{
	check(VertexPositions.IsValidIndex(InVertexIndex));

	VertexPositions[InVertexIndex] = InPosition;
}

void UHoudiniStaticMesh::SetTriangleVertexIndices(uint32 InTriangleIndex, const FIntVector& InTriangleVertexIndices)
{
	check(TriangleIndices.IsValidIndex(InTriangleIndex));
	check(VertexPositions.IsValidIndex(InTriangleVertexIndices[0]));
	check(VertexPositions.IsValidIndex(InTriangleVertexIndices[1]));
	check(VertexPositions.IsValidIndex(InTriangleVertexIndices[2]));

	TriangleIndices[InTriangleIndex] = InTriangleVertexIndices;
}

void UHoudiniStaticMesh::SetTriangleVertexNormal(uint32 InTriangleIndex, uint8 InTriangleVertexIndex, const FVector& InNormal)
{
	if (!bHasNormals)
	{
		return;
	}

	check(TriangleIndices.IsValidIndex(InTriangleIndex));
	const uint32 VertexInstanceIndex = InTriangleIndex * 3 + InTriangleVertexIndex;
	check(VertexInstanceNormals.IsValidIndex(VertexInstanceIndex));

	VertexInstanceNormals[VertexInstanceIndex] = InNormal;
}

void UHoudiniStaticMesh::SetTriangleVertexUTangent(uint32 InTriangleIndex, uint8 InTriangleVertexIndex, const FVector& InUTangent)
{
	if (!bHasTangents)
	{
		return;
	}

	check(TriangleIndices.IsValidIndex(InTriangleIndex));
	const uint32 VertexInstanceIndex = InTriangleIndex * 3 + InTriangleVertexIndex;
	check(VertexInstanceUTangents.IsValidIndex(VertexInstanceIndex));

	VertexInstanceUTangents[VertexInstanceIndex] = InUTangent;
}

void UHoudiniStaticMesh::SetTriangleVertexVTangent(uint32 InTriangleIndex, uint8 InTriangleVertexIndex, const FVector& InVTangent)
{
	if (!bHasTangents)
	{
		return;
	}

	check(TriangleIndices.IsValidIndex(InTriangleIndex));
	const uint32 VertexInstanceIndex = InTriangleIndex * 3 + InTriangleVertexIndex;
	check(VertexInstanceVTangents.IsValidIndex(VertexInstanceIndex));

	VertexInstanceVTangents[VertexInstanceIndex] = InVTangent;
}

void UHoudiniStaticMesh::SetTriangleVertexColor(uint32 InTriangleIndex, uint8 InTriangleVertexIndex, const FColor& InColor)
{
	if (!bHasColors)
	{
		return;
	}

	check(TriangleIndices.IsValidIndex(InTriangleIndex));
	const uint32 VertexInstanceIndex = InTriangleIndex * 3 + InTriangleVertexIndex;
	check(VertexInstanceColors.IsValidIndex(VertexInstanceIndex));

	VertexInstanceColors[VertexInstanceIndex] = InColor;
}

void UHoudiniStaticMesh::SetTriangleVertexUV(uint32 InTriangleIndex, uint8 InTriangleVertexIndex, uint8 InUVLayer, const FVector2D& InUV)
{
	if (NumUVLayers <= 0)
	{
		return;
	}

	check(TriangleIndices.IsValidIndex(InTriangleIndex));
	const uint32 VertexInstanceUVIndex = InUVLayer * GetNumVertexInstances() + InTriangleIndex * 3 + InTriangleVertexIndex;
	check(VertexInstanceUVs.IsValidIndex(VertexInstanceUVIndex));

	VertexInstanceUVs[VertexInstanceUVIndex] = InUV;
}

void UHoudiniStaticMesh::SetTriangleMaterialID(uint32 InTriangleIndex, int32 InMaterialID)
{
	if (!bHasPerFaceMaterials)
	{
		return;
	}

	check(TriangleIndices.IsValidIndex(InTriangleIndex));
	check(MaterialIDsPerTriangle.IsValidIndex(InTriangleIndex));

	MaterialIDsPerTriangle[InTriangleIndex] = InMaterialID;
}

void UHoudiniStaticMesh::SetStaticMaterial(uint32 InMaterialIndex, const FStaticMaterial& InStaticMaterial)
{
	check(StaticMaterials.IsValidIndex(InMaterialIndex));
	StaticMaterials[InMaterialIndex] = InStaticMaterial;
}

void UHoudiniStaticMesh::Optimize()
{
	VertexPositions.Shrink();
	TriangleIndices.Shrink();
	VertexInstanceColors.Shrink();
	VertexInstanceNormals.Shrink();
	VertexInstanceUTangents.Shrink();
	VertexInstanceVTangents.Shrink();
	VertexInstanceUVs.Shrink();
	MaterialIDsPerTriangle.Shrink();
	StaticMaterials.Shrink();
}

FBox UHoudiniStaticMesh::CalcBounds() const
{
	const uint32 NumVertices = VertexPositions.Num();

	if (NumVertices == 0)
		return FBox();

	const FVector InitPosition = VertexPositions[0];
	double MinX = InitPosition.X, MaxX = InitPosition.X, MinY = InitPosition.Y, MaxY = InitPosition.Y, MinZ = InitPosition.Z, MaxZ = InitPosition.Z;
	for (uint32 VertIdx = 0; VertIdx < NumVertices; ++VertIdx)
	{
		const FVector Position = VertexPositions[VertIdx];
		if (Position.X < MinX) MinX = Position.X; else if (Position.X > MaxX) MaxX = Position.X;
		if (Position.Y < MinY) MinY = Position.Y; else if (Position.Y > MaxY) MaxY = Position.Y;
		if (Position.Z < MinZ) MinZ = Position.Z; else if (Position.Z > MaxZ) MaxZ = Position.Z;
	}

	return FBox(FVector(MinX, MinY, MinZ), FVector(MaxX, MaxY, MaxZ));
}

UMaterialInterface* UHoudiniStaticMesh::GetMaterial(int32 InMaterialIndex)
{
	check(StaticMaterials.IsValidIndex(InMaterialIndex));

	return StaticMaterials[InMaterialIndex].MaterialInterface;
}

int32 UHoudiniStaticMesh::GetMaterialIndex(FName InMaterialSlotName) const
{
	if (InMaterialSlotName == NAME_None)
		return -1;

	const uint32 NumMaterials = StaticMaterials.Num();
	for (uint32 MaterialIndex = 0; MaterialIndex < NumMaterials; ++MaterialIndex)
	{
		if (StaticMaterials[MaterialIndex].MaterialSlotName == InMaterialSlotName)
			return (int32)MaterialIndex;
	}

	return -1;
}

void UHoudiniStaticMesh::Serialize(FArchive &InArchive)
{
	Super::Serialize(InArchive);

	VertexPositions.Shrink();
	VertexPositions.BulkSerialize(InArchive);

	TriangleIndices.Shrink();
	TriangleIndices.BulkSerialize(InArchive);

	VertexInstanceColors.Shrink();
	VertexInstanceColors.BulkSerialize(InArchive);

	VertexInstanceNormals.Shrink();
	VertexInstanceNormals.BulkSerialize(InArchive);

	VertexInstanceUTangents.Shrink();
	VertexInstanceUTangents.BulkSerialize(InArchive);

	VertexInstanceVTangents.Shrink();
	VertexInstanceVTangents.BulkSerialize(InArchive);

	VertexInstanceUVs.Shrink();
	VertexInstanceUVs.BulkSerialize(InArchive);

	MaterialIDsPerTriangle.Shrink();
	MaterialIDsPerTriangle.BulkSerialize(InArchive);
}
