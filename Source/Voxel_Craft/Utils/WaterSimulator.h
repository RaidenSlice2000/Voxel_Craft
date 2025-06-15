
#pragma once

#include "CoreMinimal.h"
#include "Math/IntVector.h"
#include "Containers/Queue.h"

class AGreedyChunk;
enum class EBlock : uint8;

/**
 * FWaterSimulator
 * Simulates Minecraft-style flowing water in voxel-based worlds.
 */
class FWaterSimulator
{
public:

	FWaterSimulator(const FIntVector& InChunkSize);
	/**
	 * Add a water block to the simulator queue
	 * @param GlobalPosition Global position of the block
	 * @param WaterLevel Level of water (0=source, 1-7=flowing)
	 */
	void EnqueueWaterBlock(const FIntVector& GlobalPosition, uint8 WaterLevel = 0);

	/**
	 * Process the water simulator for this frame
	 * @param DeltaTime Time elapsed since last tick
	 */
	void Tick(float DeltaTime);

	/**
	 * Set the function used to retrieve chunks by position
	 * @param InChunkFetcher Function to get chunk at position
	 */
	void SetChunkFetcher(const TFunction<AGreedyChunk*(const FIntVector&)>& InChunkFetcher);

	/**
	 * Enable/disable infinite water sources (2x2 pools create new sources)
	 * @param bEnable Whether to enable infinite sources
	 */
	void SetInfiniteSourcesEnabled(bool bEnable);

	/**
	 * Enable/disable water evaporation over time
	 * @param bEnable Whether to enable evaporation
	 * @param Rate Time in seconds before water level increases by 1
	 */
	void SetEvaporationEnabled(bool bEnable, float Rate = 5.0f);

private:
	FIntVector ChunkSize;  // This needs to exist if you want to initialize it

	// Try to spread water from a position with the current strength
	void TrySpread(const FIntVector& Position, uint8 CurrentStrength);
	
	// Check if a position should become an infinite water source
	void CheckForInfiniteSource(const FIntVector& Position);
	bool IsWaterSource(const FIntVector& Position) const;
	
	// Process evaporation of flowing water
	void ProcessEvaporation(float DeltaTime);

	// Queue of water blocks to process
	TQueue<TPair<FIntVector, uint8>> WaterQueue;

	// Function to fetch chunk at position
	TFunction<AGreedyChunk*(const FIntVector&)> GetChunkAt;

	// Whether infinite water sources are enabled (2x2 creates source)
	bool bInfiniteSourcesEnabled;
	
	// Whether evaporation is enabled
	bool bEvaporationEnabled;
	
	// Evaporation rate in seconds per level
	float EvaporationRate;
	
	// Track water sources for infinite water features
	TSet<FIntVector> WaterSources;
	
	// Track flowing water and evaporation timers
	TMap<FIntVector, float> FlowingWaterBlocks;


};