#pragma once

#include "CoreMinimal.h"

#include "Voxel_craft/Utils/WaterSimulator.h"
#include "ChunkBase.h"
#include "Voxel_craft/Utils/Enums.h"

#include "GreedyChunk.generated.h"

class FastNoiseLite;
class UProceduralMeshComponent;

USTRUCT()
struct FBiomeNoiseSettings
{
	GENERATED_BODY()

	UPROPERTY()
	float Frequency = 0.01f;

	UPROPERTY()
	float Amplitude = 1.0f;

	UPROPERTY()
	float Offset = 0.0f;
};

UCLASS()
class AGreedyChunk final : public AChunkBase
{
	GENERATED_BODY()

	struct FMask
	{
		EBlock Block;
		int Normal;
	};
public:
	void InitializeChunkOrigin(const FIntVector& Coords);

	EBlock GetBlock(FIntVector Index) const;
	void SetWaterSimulator(FWaterSimulator* InSimulator);
	bool IsInsideChunk(const FIntVector& Position) const;
	EBlock GetBlockWorld(const FIntVector& WorldPosition) const;
	uint8 GetMeta(const FIntVector& Position) const;
	static AGreedyChunk* GetChunkAt(const FIntVector& WorldBlockPosition, const FIntVector& ChunkPosition);
	void SetBlockAt(const FIntVector& Position, EBlock BlockType);
	void SetMeta(const FIntVector& Position, uint8 MetaValue);
	virtual void UpdateMesh() override;

	static TMap<FIntVector, AGreedyChunk*> LoadedChunks;
	FIntVector GridCoord;
	
protected:
	virtual void Setup() override;
	static float GetFractalNoise2D(FastNoiseLite* Noise, float X, float Y, float Frequency, int Octaves, float Persistence);
	virtual void Generate2DHeightMap(FVector Position) override;
	virtual void Generate3DHeightMap(FVector Position) override;
	virtual void GenerateMesh() override;
	virtual void ModifyVoxelData(FIntVector Position, EBlock Block) override;
	virtual void LoadChunkMap() override;
	EBlock GetBlockWithNeighbors(const FIntVector& Pos) const;
	
private:

	FWaterSimulator* WaterSimulator = nullptr;
	
	TArray<EBlock> Blocks;
	
	TArray<uint8> BlockMeta;
	
	FastNoiseLite* BiomeNoise;
	
	FIntVector ChunkOrigin;
	
	TMap<EBiomeType, FBiomeNoiseSettings> BiomeSettingsMap;
	
	UPROPERTY()
	TSet<FIntVector> OriginalTopCactusBlocks;
	
	void CreateQuad(FMask Mask, FIntVector AxisMask, int Width, int Height, FIntVector V1, FIntVector V2, FIntVector V3, FIntVector V4);
	int GetBlockIndex(int X, int Y, int Z) const;
	bool IsTopmostCactusBlock(const FIntVector& BlockPos) const;
	static bool CompareMask(FMask M1, FMask M2);
	static int32 GetMaterialIndex(EBlock Block, const FVector& Normal);
	int GetTextureIndex(EBlock Block, const FVector& Normal, const FIntVector& BlockPos) const;
	void SpawnTreeAt(int x, int y, int z, const FRandomStream& TreeRand);
	void SpawnCactusAt(int x, int y, int z);
};