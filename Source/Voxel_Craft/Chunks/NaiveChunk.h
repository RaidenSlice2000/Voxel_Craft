// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "ChunkBase.h"
#include "Voxel_Craft/Utils/Enums.h"

#include "NaiveChunk.generated.h"

class FastNoiseLite;
class UProceduralMeshComponent;

UCLASS()
class ANaiveChunk final : public AChunkBase
{
	GENERATED_BODY()

protected:
	virtual void Setup() override;
	virtual void Generate2DHeightMap(FVector Position) override;
	virtual void Generate3DHeightMap(FVector Position) override;
	virtual void GenerateMesh() override;
	virtual void ModifyVoxelData(FIntVector Position, EBlock Block) override;

private:
	TArray<EBlock> Blocks;

	const FVector BlockVertexData[8] = {
		FVector(100,100,100),
		FVector(100,0,100),
		FVector(100,0,0),
		FVector(100,100,0),
		FVector(0,0,100),
		FVector(0,100,100),
		FVector(0,100,0),
		FVector(0,0,0)
	};

	const int BlockTriangleData[24] = {
		0,1,2,3, // Forward
		5,0,3,6, // Right
		4,5,6,7, // Back
		1,4,7,2, // Left
		5,4,1,0, // Up
		3,2,7,6  // Down
	};
	
	bool Check(const FVector& Position) const;
	void CreateFace(EChunkDirection Direction, const FVector& Position);
	TArray<FVector> GetFaceVertices(EChunkDirection Direction, const FVector& Position) const;
	static FVector GetPositionInDirection(EChunkDirection Direction, const FVector& Position);
	static FVector GetNormal(EChunkDirection Direction);
	int GetBlockIndex(int X, int Y, int Z) const;
};