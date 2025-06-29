
#include "GreedyChunk.h"

#include "Voxel_Craft/Utils/WaterSimulator.h"
#include "Containers/Map.h"
#include "Math/IntVector.h"

#include "Voxel_Craft/Utils/FastNoiseLite.h"

TMap<FIntVector, AGreedyChunk*> AGreedyChunk::LoadedChunks;

void AGreedyChunk::Setup()
{
	Blocks.SetNum(ChunkSize.X * ChunkSize.Y * ChunkSize.Z);
	BlockMeta.SetNum(ChunkSize.X * ChunkSize.Y * ChunkSize.Z);

	if (!Noise) Noise = new FastNoiseLite();
	if (!BiomeNoise) BiomeNoise = new FastNoiseLite();
	if (!RiverNoise) RiverNoise = new FastNoiseLite();
	if (!LakeNoise) LakeNoise = new FastNoiseLite();
	
	BiomeSettingsMap.Add(EBiomeType::Desert,   {0.01f, 0.2f, -5.0f});
	BiomeSettingsMap.Add(EBiomeType::Plains,   {0.02f, 0.4f, 0.0f});
	BiomeSettingsMap.Add(EBiomeType::Forest,   {0.02f, 0.5f, 2.0f});
	BiomeSettingsMap.Add(EBiomeType::Mountain, {0.005f, 1.2f, 10.0f});
	BiomeSettingsMap.Add(EBiomeType::Snowy,    {0.007f, 1.0f, 12.0f});
}

struct FBiomeRange
{
	float Min;
	float Max;
	EBiomeType Type;
};

const TArray<FBiomeRange> BiomeRanges = {
	{0.0f, 0.2f, EBiomeType::Desert},
	{0.2f, 0.4f, EBiomeType::Plains},
	{0.4f, 0.6f, EBiomeType::Forest},
	{0.6f, 0.8f, EBiomeType::Mountain},
	{0.8f, 1.0f, EBiomeType::Snowy},
};

FBiomeNoiseSettings LerpBiomeSettings(const FBiomeNoiseSettings& A, const FBiomeNoiseSettings& B, float Alpha)
{
	FBiomeNoiseSettings Result;
	Result.Frequency = FMath::Lerp(A.Frequency, B.Frequency, Alpha);
	Result.Amplitude = FMath::Lerp(A.Amplitude, B.Amplitude, Alpha);
	Result.Offset = FMath::Lerp(A.Offset, B.Offset, Alpha);
	return Result;
}



float AGreedyChunk::GetFractalNoise2D( FastNoiseLite* Noise, const float X, const float Y, const float Frequency, const int Octaves, const float Persistence)
{
	float Total = 0.0f;
	float MaxValue = 0.0f;
	float Amplitude = 1.0f;
	float Freq = Frequency;

	for (int i = 0; i < Octaves; ++i)
	{
		Total += Noise->GetNoise(X * Freq, Y * Freq) * Amplitude;
		MaxValue += Amplitude;
		Amplitude *= Persistence;
		Freq *= 2.0f;
	}

	return Total / MaxValue;
}


void AGreedyChunk::Generate2DHeightMap(const FVector Position)
{
	
	TArray<FIntVector> SurfacePositions;
	TArray<EBiomeType> Biomes;
	

	for (int x = 0; x < ChunkSize.X; x++)
	{
		for (int y = 0; y < ChunkSize.Y; y++)
		{
			const float Xpos = FMath::FloorToFloat(x + Position.X);  
			const float Ypos = FMath::FloorToFloat(y + Position.Y);
			
			const float BiomeValue = BiomeNoise->GetNoise(Xpos * 0.05f, Ypos * 0.05f);
            // Normalize biome noise to 0.0-1.0 range
            const float NormalizedBiomeValue = (BiomeValue + 1.0f) * 0.5f;

			float HeightSum = 0.0f;
			float WeightSum = 0.0f;

			EBiomeType DominantBiome = EBiomeType::Plains; // Default biome
			float MaxInfluence = 0.0f;

			// Multi-biome height blending
			for (const FBiomeRange& Range : BiomeRanges)
			{
				float Mid = (Range.Min + Range.Max) * 0.5f;
				float Distance = FMath::Abs(NormalizedBiomeValue - Mid);
				float Influence = FMath::Clamp(1.0f - Distance * 5.0f, 0.0f, 1.0f); // Weight falloff
				Influence = FMath::Pow(Influence, 2.5); // Smoother falloff (adjusted from 3)

				if (Influence <= 0.001f) continue;

				if (Influence > MaxInfluence)
				{
					MaxInfluence = Influence;
					DominantBiome = Range.Type;
				}

				const FBiomeNoiseSettings& Settings = BiomeSettingsMap[Range.Type];

				float NoiseValue = GetFractalNoise2D(Noise, Xpos, Ypos, Settings.Frequency, 4, 0.5f);
				float BiomeHeight = (NoiseValue + 1.0f) * 0.5f * Settings.Amplitude * ChunkSize.Z + Settings.Offset;

				HeightSum += BiomeHeight * Influence;
				WeightSum += Influence;
			}

			// Ensure we have at least some influence
			if (WeightSum < 0.0001f)
			{
				WeightSum = 1.0f;
				HeightSum = 30.0f; // Default height if no biome influence
				DominantBiome = EBiomeType::Plains;
			}

			float FinalHeight = HeightSum / WeightSum;
			
			// Add small-scale noise for terrain details
			FinalHeight += Noise->GetNoise(Xpos * 0.1f, Ypos * 0.1f) * 1.5f;

			// Ensure minimum terrain height for digging (at least 15 blocks deep)
			constexpr int MinimumHeight = 50;
			int Height = FMath::Clamp(FMath::RoundToInt(FinalHeight), MinimumHeight, ChunkSize.Z - 1);

			// Store for later tree placement
			Biomes.Add(DominantBiome);
			SurfacePositions.Add(FIntVector(x, y, Height));

			// Block layers
			for (int z = 0; z < ChunkSize.Z; z++)
			{
				if (z <= Height)
				{
					if (DominantBiome == EBiomeType::Desert)  // <-- Add this check
					{
					// Desert stratified layering
						if (z >= Height - 2) // Top 3 layers
							Blocks[GetBlockIndex(x, y, z)] = EBlock::Sand;
					else if (z >= Height - 5) // Next 3 layers
							Blocks[GetBlockIndex(x, y, z)] = EBlock::Sandstone;
					
					else
							Blocks[GetBlockIndex(x, y, z)] = EBlock::Stone;
						
					}
					else
					{
						// Underground layers
						if (z < Height - 3)
							Blocks[GetBlockIndex(x, y, z)] = EBlock::Stone;
						else if (z < Height)
							Blocks[GetBlockIndex(x, y, z)] = EBlock::Dirt;
						else if (z == Height)
							{
							// Surface block based on biome
							switch (DominantBiome)
								{
								case EBiomeType::Forest:   Blocks[GetBlockIndex(x, y, z)] = EBlock::Grass; break;
								case EBiomeType::Mountain: Blocks[GetBlockIndex(x, y, z)] = EBlock::Stone; break;
								case EBiomeType::Snowy:    Blocks[GetBlockIndex(x, y, z)] = EBlock::Snow; break;
								default:                   Blocks[GetBlockIndex(x, y, z)] = EBlock::Grass; break;
							}
							}
							
					}
				}
				else
				{
					// Air above terrain
					Blocks[GetBlockIndex(x, y, z)] = EBlock::Air;
				}
			}
		}
	}

		// Build a heightmap for fast surface Z lookup
	TArray<TArray<int>> HeightMap;
	HeightMap.SetNum(ChunkSize.X);
	for (int x = 0; x < ChunkSize.X; ++x) {
		HeightMap[x].SetNum(ChunkSize.Y);
		for (int y = 0; y < ChunkSize.Y; ++y) HeightMap[x][y] = -1;
	}
	for (const FIntVector& surf : SurfacePositions) {
		if (surf.X >= 0 && surf.X < ChunkSize.X && surf.Y >= 0 && surf.Y < ChunkSize.Y)
			HeightMap[surf.X][surf.Y] = surf.Z;
	}
	 // --- LAKE GENERATION (with LakeNoise, more natural, not every chunk) ---
	{
		for (const FIntVector& surf : SurfacePositions)
		{
		    float worldX = surf.X + Position.X;
		    float worldY = surf.Y + Position.Y;
		    float lakeNoise = LakeNoise->GetNoise(worldX, worldY);

		    if (lakeNoise < -0.35f && surf.Z < ChunkSize.Z * 0.8f) {
		        if (FMath::FRand() > 0.15f) continue;

		        int lakeRadius = FMath::RandRange(8, 14);
		        int centerWaterLevel = surf.Z; // The "ideal" water level for the lake center

		        for (int angle = 0; angle < 360; angle += 3) {
		            float rad = FMath::DegreesToRadians(angle);
		            float radiusOffset = LakeNoise->GetNoise(
		                worldX + FMath::Cos(rad) * lakeRadius,
		                worldY + FMath::Sin(rad) * lakeRadius
		            ) * 3.0f;

		            float finalRadius = lakeRadius + radiusOffset;

		            for (float r = 0; r < finalRadius; r += 1.0f) {
		                int lx = surf.X + FMath::RoundToInt(FMath::Cos(rad) * r);
		                int ly = surf.Y + FMath::RoundToInt(FMath::Sin(rad) * r);
		                if (lx < 0 || lx >= ChunkSize.X || ly < 0 || ly >= ChunkSize.Y) continue;

		                // Use heightmap to find local surface Z
		                int surfaceZ = HeightMap[lx][ly];
		                if (surfaceZ < 0) continue; // Out of bounds

		                // Calculate water level: usually min(centerWaterLevel, surfaceZ)
		                // Option 1: Use centerWaterLevel for a flat lake surface
		                // Option 2: Use min(centerWaterLevel, surfaceZ) to avoid floating water
		                int waterLevel = FMath::Min(centerWaterLevel, surfaceZ);

		                // Dig a depression (2 blocks deep), then fill with water
		                for (int lz = waterLevel - 2; lz <= waterLevel; ++lz) {
		                    if (lz >= 0 && lz < ChunkSize.Z) {
		                        int idx = GetBlockIndex(lx, ly, lz);
		                        EBlock b = Blocks[idx];
		                        if (b == EBlock::Air || b == EBlock::Dirt || b == EBlock::Grass || b == EBlock::Sand) {
		                            Blocks[idx] = EBlock::Water;
		                            BlockMeta[idx] = 0;
		                        }
		                    }
		                }
		            }
		        }
		    }
		}
	}

	// --- RIVER GENERATION (global, smooth, Minecraft-like) ---
	float riverWidth = 0.07f;

	for (int x = 0; x < ChunkSize.X; ++x) {
		for (int y = 0; y < ChunkSize.Y; ++y) {
			float riverNoiseScale = 0.3f;
			float wx = x + Position.X;
			float wy = y + Position.Y;

			float riverNoise = FMath::Abs(RiverNoise->GetNoise(wx * riverNoiseScale, wy * riverNoiseScale));
			if (riverNoise < riverWidth) {
				int z = HeightMap[x][y];
				if (z == -1) continue;

				int riverBed = FMath::Max(z - 2, 0);
				for (int dz = 0; dz <= 2; ++dz) {
					int zz = riverBed + dz;
					if (zz < ChunkSize.Z) {
						int idx = GetBlockIndex(x, y, zz);
						if (dz < 2) Blocks[idx] = EBlock::Dirt;
						else        Blocks[idx] = EBlock::Water;
						BlockMeta[idx] = 0;
					}
				}
				 HeightMap[x][y] = riverBed + 2;
			}
		}
	}
	
	// ---------- Tree & Cactus Placement ----------
	FRandomStream ChunkRand(FMath::Abs(Seed) ^ (static_cast<int32>(Position.X) * 73856093) ^ (static_cast<int32>(Position.Y) * 19349663));
	
	// Determine the number of vegetation attempts based on chunk and biome
	int MaxVegetation = ChunkSize.X * ChunkSize.Y / 30; // Base number for a chunk
	
	for (int i = 0; i < MaxVegetation; i++)
	{
		// Use noise-based selection for more natural distribution
		int idx = ChunkRand.RandRange(0, SurfacePositions.Num() - 1);
		
		const FIntVector& Surface = SurfacePositions[idx];
		const int x = Surface.X;
		const int y = Surface.Y;
		const int z = Surface.Z;
		const EBiomeType Biome = Biomes[idx];

		// Ensure we're not too close to chunk edges for trees
		if (x > 5 && x < ChunkSize.X - 6 && y > 5 && y < ChunkSize.Y - 6 && z + 6 < ChunkSize.Z)
		{
			// Different spawn rates based on biome
			float VegetationChance;
			
			switch (Biome)
			{
			case EBiomeType::Forest:
				VegetationChance = 0.7f; // High chance in forests
				break;
			case EBiomeType::Plains:
				VegetationChance = 0.2f; // Moderate chance in the plains
				break;
			case EBiomeType::Desert:
				VegetationChance = 0.3f; // Reasonable chance for cacti in the desert
				break;
			case EBiomeType::Mountain:
				VegetationChance = 0.08f; // Low chance in mountains
				break;
			case EBiomeType::Snowy:
				VegetationChance = 0.05f; // Very low chance in snow
				break;
			default:
				VegetationChance = 0.0f;
				break;
			}
			
			// Apply an additional noise factor for more natural distribution
			const float NoiseVal = FMath::Abs(Noise->GetNoise(
				(x + Position.X) * 0.1f, 
				(y + Position.Y) * 0.1f));
			
			// Use both flat chance and noise to determine if vegetation spawns
			if (ChunkRand.GetFraction() < VegetationChance * NoiseVal * 1.5f)
			{
				// Check blocks above are clear for vegetation
				bool CanPlaceVegetation = true;
				
				for (int checkZ = z + 1; checkZ <= z + 6; checkZ++)
				{
					if (checkZ < ChunkSize.Z && Blocks[GetBlockIndex(x, y, checkZ)] != EBlock::Air)
					{
						CanPlaceVegetation = false;
						break;
					}
				}
				
				// Distance check to avoid trees being too close together
				bool TooClose = false;
				if (Biome == EBiomeType::Forest || Biome == EBiomeType::Plains)
				{
					for (int j = 0; j < i; j++)
					{
						if (j >= SurfacePositions.Num()) continue;
						
						const FIntVector& OtherSurface = SurfacePositions[j];
						// If the distance is less than 5 blocks, consider it too close
						if (FMath::Abs(OtherSurface.X - x) + FMath::Abs(OtherSurface.Y - y) < 5)
						{
							TooClose = true;
							break;
						}
					}
				}
				
				if (CanPlaceVegetation && !TooClose)
				{
					int surfaceIdx = GetBlockIndex(x, y, z);
					EBlock SurfaceBlock = Blocks[surfaceIdx];

					// Only allow trees/cacti on solid non-water ground
					bool ValidForTree =
						(SurfaceBlock == EBlock::Grass || SurfaceBlock == EBlock::Dirt || SurfaceBlock == EBlock::Sand);
					bool ValidForCactus = (SurfaceBlock == EBlock::Sand);

					FRandomStream VegRand(ChunkRand.GetCurrentSeed() + i);

					if ((Biome == EBiomeType::Forest || Biome == EBiomeType::Plains) && ValidForTree)
						SpawnTreeAt(x, y, z, VegRand);
					else if (Biome == EBiomeType::Desert && ValidForCactus)
						SpawnCactusAt(x, y, z+1);
				}
			}
		}
	}
}


void AGreedyChunk::Generate3DHeightMap(const FVector Position)
{
	for (int x = 0; x < ChunkSize.X; ++x)
	{
		for (int y = 0; y < ChunkSize.Y; ++y)
		{
			for (int z = 0; z < ChunkSize.Z; ++z)
			{
				const auto NoiseValue = Noise->GetNoise(x + Position.X, y + Position.Y, z + Position.Z);

				if (NoiseValue >= 0)
				{
					Blocks[GetBlockIndex(x, y, z)] = EBlock::Air;
				}
				else
				{
					Blocks[GetBlockIndex(x, y, z)] = EBlock::Stone;
				}
			}
		}
	}
}

void AGreedyChunk::GenerateMesh()
{
	MeshPerMaterial.SetNum(2);
	VertexCountPerMat.SetNum(2);
	// Sweep over each axis (X, Y, Z)
	for (int Axis = 0; Axis < 3; ++Axis)
	{
		// 2 Perpendicular axis
		const int Axis1 = (Axis + 1) % 3;
		const int Axis2 = (Axis + 2) % 3;

		const int MainAxisLimit = ChunkSize[Axis];
		const int Axis1Limit = ChunkSize[Axis1];
		const int Axis2Limit = ChunkSize[Axis2];

		auto DeltaAxis1 = FIntVector::ZeroValue;
		auto DeltaAxis2 = FIntVector::ZeroValue;

		auto ChunkItr = FIntVector::ZeroValue;
		auto AxisMask = FIntVector::ZeroValue;

		AxisMask[Axis] = 1;

		TArray<FMask> Mask;
		Mask.SetNum(Axis1Limit * Axis2Limit);

		// Check each slice of the chunk
		for (ChunkItr[Axis] = -1; ChunkItr[Axis] < MainAxisLimit;)
		{
			int N = 0;

			// Compute Mask
			for (ChunkItr[Axis2] = 0; ChunkItr[Axis2] < Axis2Limit; ++ChunkItr[Axis2])
			{
				for (ChunkItr[Axis1] = 0; ChunkItr[Axis1] < Axis1Limit; ++ChunkItr[Axis1])
				{
					FIntVector ComparePos = ChunkItr + AxisMask;

					EBlock CurrentBlock = IsInsideChunk(ChunkItr)
						? GetBlock(ChunkItr)
						: GetBlockWithNeighbors(ChunkItr );

					EBlock CompareBlock = IsInsideChunk(ComparePos)
						? GetBlock(ComparePos)
						: GetBlockWithNeighbors(ComparePos);
					
					const bool CurrentBlockIsSolid = (CurrentBlock != EBlock::Air && CurrentBlock != EBlock::Water);
					const bool CompareBlockIsSolid = (CompareBlock != EBlock::Air && CompareBlock != EBlock::Water);
					const bool CurrentBlockIsWater = (CurrentBlock == EBlock::Water);
					const bool CompareBlockIsWater = (CompareBlock == EBlock::Water);

					if ((CurrentBlock == EBlock::Log && CompareBlock == EBlock::Leaves))
					{
						Mask[N++] = FMask{CurrentBlock, 1}; // Show log face
					}
					else if ((CompareBlock == EBlock::Log && CurrentBlock == EBlock::Leaves))
					{
						Mask[N++] = FMask{CompareBlock, -1};
					}
					else if (CurrentBlockIsSolid && (!CompareBlockIsSolid)) // land vs. air or land vs. water
					{
						Mask[N++] = FMask{CurrentBlock, 1};
					}
					else if (CurrentBlockIsWater && CompareBlock == EBlock::Air) // water vs. air
					{
						Mask[N++] = FMask{CurrentBlock, 1};
					}
					else if (CompareBlockIsSolid && (!CurrentBlockIsSolid)) // land vs. air or land vs. water (opposite direction)
					{
						Mask[N++] = FMask{CompareBlock, -1};
					}
					else if (CompareBlockIsWater && CurrentBlock == EBlock::Air) // water vs. air (opposite a direction)
					{
						Mask[N++] = FMask{CompareBlock, -1};
					}
					else
					{
						Mask[N++] = FMask{EBlock::Null, 0}; // skip faces between solid/solid, water/water, or air/air
					}
				}
			}

			++ChunkItr[Axis];
			N = 0;

			// Generate Mesh From Mask
			for (int j = 0; j < Axis2Limit; ++j)
			{
				for (int i = 0; i < Axis1Limit;)
				{
					if (Mask[N].Normal != 0)
					{
						const auto CurrentMask = Mask[N];
						ChunkItr[Axis1] = i;
						ChunkItr[Axis2] = j;

						int Width;

						for (Width = 1; i + Width < Axis1Limit && CompareMask(Mask[N + Width], CurrentMask); ++Width)
						{
						}

						int Height;
						bool Done = false;

						for (Height = 1; j + Height < Axis2Limit; ++Height)
						{
							for (int k = 0; k < Width; ++k)
							{
								if (CompareMask(Mask[N + k + Height * Axis1Limit], CurrentMask)) continue;

								Done = true;
								break;
							}

							if (Done) break;
						}

						DeltaAxis1[Axis1] = Width;
						DeltaAxis2[Axis2] = Height;

						CreateQuad(
							CurrentMask, AxisMask, Width, Height,
							ChunkItr,
							ChunkItr + DeltaAxis1,
							ChunkItr + DeltaAxis2,
							ChunkItr + DeltaAxis1 + DeltaAxis2
						);

						DeltaAxis1 = FIntVector::ZeroValue;
						DeltaAxis2 = FIntVector::ZeroValue;

						for (int l = 0; l < Height; ++l)
						{
							for (int k = 0; k < Width; ++k)
							{
								Mask[N + k + l * Axis1Limit] = FMask{EBlock::Null, 0};
							}
						}

						i += Width;
						N += Width;
					}
					else
					{
						i++;
						N++;
					}
				}
			}
		}
	}
}


void AGreedyChunk::CreateQuad(
	const FMask Mask,
	const FIntVector AxisMask,
	const int Width,
	const int Height,
	const FIntVector V1,
	const FIntVector V2,
	const FIntVector V3,
	const FIntVector V4
)
{
	const auto Normal = FVector(AxisMask * Mask.Normal);
	
	// Get the material index for this block type
	const int32 MaterialIndex = GetMaterialIndex(Mask.Block, Normal);
	
	// Make sure we have enough space in our per-material arrays
	if (MaterialIndex >= MeshPerMaterial.Num())
	{
		UE_LOG(LogTemp, Warning, TEXT("Material index %d is out of bounds! Max is %d"), MaterialIndex, MeshPerMaterial.Num());
		return;
	}
	
	// Add the vertices to the appropriate material's mesh data
	const int32 CurrentVertexCount = VertexCountPerMat[MaterialIndex];
	
	MeshPerMaterial[MaterialIndex].Vertices.Append({
		FVector(V1) * 100,
		FVector(V2) * 100,
		FVector(V3) * 100,
		FVector(V4) * 100
	});

	MeshPerMaterial[MaterialIndex].Triangles.Append({
		CurrentVertexCount,
		CurrentVertexCount + 2 + Mask.Normal,
		CurrentVertexCount + 2 - Mask.Normal,
		CurrentVertexCount + 3,
		CurrentVertexCount + 1 - Mask.Normal,
		CurrentVertexCount + 1 + Mask.Normal
	});

	MeshPerMaterial[MaterialIndex].Normals.Append({
		Normal,
		Normal,
		Normal,
		Normal
	});

	const auto Color = FColor(0, 0, 0, GetTextureIndex(Mask.Block, Normal, V1));
	MeshPerMaterial[MaterialIndex].Colors.Append({
		Color,
		Color,
		Color,
		Color
	});

	if (Normal.X == 1 || Normal.X == -1)
	{
		MeshPerMaterial[MaterialIndex].UV0.Append({
			FVector2D(Width, Height),
			FVector2D(0, Height),
			FVector2D(Width, 0),
			FVector2D(0, 0),
		});
	}
	else
	{
		MeshPerMaterial[MaterialIndex].UV0.Append({
			FVector2D(Height, Width),
			FVector2D(Height, 0),
			FVector2D(0, Width),
			FVector2D(0, 0),
		});
	}

	VertexCountPerMat [MaterialIndex] += 4;
}

void AGreedyChunk::ModifyVoxelData(const FIntVector Position, const EBlock Block)
{
	const int Index = GetBlockIndex(Position.X, Position.Y, Position.Z);
	Blocks[Index] = Block;
	if (WaterSimulator)
	{
		UE_LOG(LogTemp, Warning, TEXT("WaterSimulator is valid (not null)"));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("WaterSimulator is NULL!"));
	}

	if (Block == EBlock::Water && WaterSimulator)
	{
		SetMeta(Position, 1);

		uint8 WaterLevel = GetMeta(Position); // Get current water meta value
		WaterSimulator->EnqueueWaterBlock(Position, WaterLevel);
		UE_LOG(LogTemp, Warning, TEXT("Enqueued water block at with level %d"), WaterLevel);

	}
	
	
	// Remove from tracked cactus tops if it's being erased
	if (Block == EBlock::Air)
	{
		OriginalTopCactusBlocks.Remove(Position);
	}
	
}


int AGreedyChunk::GetBlockIndex( int X,  int Y,  int Z) const
{
	return Z * ChunkSize.Y * ChunkSize.X + Y * ChunkSize.X + X;
}

void AGreedyChunk::InitializeChunkOrigin(const FIntVector& Coords)
{
	ChunkOrigin = FIntVector(Coords.X * ChunkSize.X*100, Coords.Y * ChunkSize.Y*100, Coords.Z * ChunkSize.Z*100);
	

	FIntVector ChunkCoords(FMath::FloorToInt(static_cast<float>(ChunkOrigin.X) / (ChunkSize.X * 100)),
							FMath::FloorToInt(static_cast<float>(ChunkOrigin.Y) / (ChunkSize.Y * 100)),
							FMath::FloorToInt(static_cast<float>(ChunkOrigin.Z) / (ChunkSize.Z * 100)));
	UE_LOG(LogTemp, Warning, TEXT("INITIALIZE ORIGIN: ChunkCoords (%d,%d,%d), ChunkOrigin (%d,%d,%d)"),
		   ChunkCoords.X, ChunkCoords.Y, ChunkCoords.Z,
		   ChunkOrigin.X, ChunkOrigin.Y, ChunkOrigin.Z);
	DrawDebugBox(GetWorld(), FVector(ChunkOrigin), FVector(ChunkSize.X * 100, ChunkSize.Y * 100, ChunkSize.Z * 100), FColor::Red, true, 10.0f);
}

EBlock AGreedyChunk::GetBlock(const FIntVector LocalPos) const
{
	
	if (LocalPos.X >= 0 && LocalPos.X < ChunkSize.X &&
	   LocalPos.Y >= 0 && LocalPos.Y < ChunkSize.Y &&
	   LocalPos.Z >= 0 && LocalPos.Z < ChunkSize.Z)
	{
		return Blocks[GetBlockIndex(LocalPos.X, LocalPos.Y, LocalPos.Z)];
	}

	// Out of bounds: figure out neighbor chunk coordinates
	FIntVector ChunkCoords(
			FMath::FloorToInt(static_cast<float>(ChunkOrigin.X) / ChunkSize.X),
			FMath::FloorToInt(static_cast<float>(ChunkOrigin.Y) / ChunkSize.Y),
			FMath::FloorToInt(static_cast<float>(ChunkOrigin.Z) / ChunkSize.Z)
	);
	FIntVector NeighborCoords = ChunkCoords;

	FIntVector NeighborLocalPos = LocalPos;
	if (NeighborLocalPos.X < 0) { NeighborCoords.X -= 1; NeighborLocalPos.X += ChunkSize.X; }
	if (NeighborLocalPos.X >= ChunkSize.X) { NeighborCoords.X += 1; NeighborLocalPos.X -= ChunkSize.X; }
	if (NeighborLocalPos.Y < 0) { NeighborCoords.Y -= 1; NeighborLocalPos.Y += ChunkSize.Y; }
	if (NeighborLocalPos.Y >= ChunkSize.Y) { NeighborCoords.Y += 1; NeighborLocalPos.Y -= ChunkSize.Y; }
	if (NeighborLocalPos.Z < 0) { NeighborCoords.Z -= 1; NeighborLocalPos.Z += ChunkSize.Z; }
	if (NeighborLocalPos.Z >= ChunkSize.Z) { NeighborCoords.Z += 1; NeighborLocalPos.Z -= ChunkSize.Z; }

	if (auto* NeighborChunk = LoadedChunks.Find(NeighborCoords))
	{
		UE_LOG(LogTemp, Warning, TEXT("Found neighbor chunk at %d,%d,%d"), NeighborCoords.X, NeighborCoords.Y, NeighborCoords.Z);

		return (*NeighborChunk)->Blocks[GetBlockIndex(NeighborLocalPos.X, NeighborLocalPos.Y, NeighborLocalPos.Z)];
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Missing neighbor chunk at %d,%d,%d, treating as Air"), NeighborCoords.X, NeighborCoords.Y, NeighborCoords.Z);
		return EBlock::Air;
	}
	// If no neighbor chunk, treat as air (so the face is rendered)
}

void AGreedyChunk::SetWaterSimulator(FWaterSimulator* InSimulator)
{
		WaterSimulator = InSimulator;

}

bool AGreedyChunk::IsTopmostCactusBlock(const FIntVector& BlockPos) const
{
	FIntVector Above = BlockPos + FIntVector(0, 0, 1);
	return GetBlock(Above) != EBlock::Cactus;
}

bool AGreedyChunk::CompareMask(const FMask M1, const FMask M2)
{
	return M1.Block == M2.Block && M1.Normal == M2.Normal;
}

inline bool AreVectorsClose(const FVector& A, const FVector& B, float Tolerance = 0.0001f)
{
	return FVector::DistSquared(A, B) < FMath::Square(Tolerance);
}

int32 AGreedyChunk::GetMaterialIndex(const EBlock Block, const FVector& Normal)
{
	if (Block == EBlock::Null || Block == EBlock::Air)
		return 0;
	switch (Block) {
	case EBlock::Grass:
	case EBlock::Dirt:
	case EBlock::Stone:
	case EBlock::Log:
	case EBlock::Sand:
	case EBlock::Cactus:
	case EBlock::Snow:
	case EBlock::Sandstone:
		return 0; // These blocks use material 0
        
	case EBlock::Leaves:
	case EBlock::Water:
		return 1; // Leaves use material 1
        
	default:
		UE_LOG(LogTemp, Warning, TEXT("Unknown block type encountered in GetMaterialIndex!"));
		return 0; // Default material 
	}
}

int AGreedyChunk::GetTextureIndex(const EBlock Block, const FVector& Normal, const FIntVector& BlockPos) const
{
	switch (Block)
	{
	case EBlock::Grass:
		{
			if (AreVectorsClose(Normal, FVector::UpVector)) return 0;
			if (AreVectorsClose(Normal, FVector::DownVector)) return 2;
			return 1;
		}
	case EBlock::Dirt:
		return 2; 
	case EBlock::Stone:
		return 3; 
	case EBlock::Log:
		{
			if (AreVectorsClose(Normal, FVector::UpVector)) return 4; 
			if (AreVectorsClose(Normal, FVector::DownVector)) return 4;
			return 5;
		}
	case EBlock::Leaves:
		{
			return 6;
		}
	case EBlock::Sand:
		{
			return 7; // Sand texture for all faces
		}
	case EBlock::Snow:
		{
			return 8; // Snow texture for all faces
		}
	case EBlock::Cactus:
		{
			if (AreVectorsClose(Normal, FVector::UpVector)) // Top face
			{
				// Use the original top cactus texture if it was tagged during generation
				if (OriginalTopCactusBlocks.Contains(BlockPos))
					return 9;
				else
					return 10;
			}
			else if (AreVectorsClose(Normal, FVector::DownVector))
			{
				return 10;
			}
			else
			{
				return 11;
			}
		}
	case  EBlock::Sandstone:
		{
			return 12;
		}
	case EBlock::Water:
		{
			return 13;
		}

	default:
		 UE_LOG(LogTemp, Warning, TEXT("Unknown block type encountered in GetTextureIndex! Block: %s"), *UEnum::GetValueAsString(Block));
			return 255; 
	}
}

void AGreedyChunk::SpawnTreeAt(int x, int y, int z, const FRandomStream& TreeRand)
{
	// Test blocks in cardinal directions
	
	const int TrunkHeight = 4 + TreeRand.RandRange(0, 2); // 4-6 blocks tall
	constexpr int LeafHeight = 3; // Classic Minecraft tree leaf height
	const int LeafStartHeight = TrunkHeight - LeafHeight + 1; // Start leaves before top of trunk
    
	// Generate trunk
	for (int i = 0; i < TrunkHeight; ++i)
	{
		const int tz = z + i;
		if (tz >= ChunkSize.Z) break;

		int BlockIndex = GetBlockIndex(x, y, tz);
		if (Blocks[BlockIndex] == EBlock::Air) 
			Blocks[BlockIndex] = EBlock::Log;
	}


	// Generate leaves using LeafHeight
	for (int layer = 0; layer < LeafHeight; layer++)
	{
		const int lz = z + LeafStartHeight + layer;
		if (lz >= ChunkSize.Z) continue;

	
    	
		// Determine radius based on layer position
		int radius;
		if (layer == 0 || layer == LeafHeight - 1) {
			radius = 1; // Top and bottom layers are smaller (3x3)
		} else {
			radius = 2; // Middle layers are larger (5x5)
		}

		
		// Generate square of leaves for this layer
		for (int dx = -radius; dx <= radius; dx++) {
			for (int dy = -radius; dy <= radius; dy++)
			{
				int lx = x + dx;
				int ly = y + dy;
                
				// Check bounds
				if (lx < 0 || ly < 0 || lz < 0 || lx >= ChunkSize.X || ly >= ChunkSize.Y || lz >= ChunkSize.Z)
					continue;
                    
				// Skip trunk position (except at the very top if trunk doesn't reach)
				if (dx == 0 && dy == 0 && lz < z + TrunkHeight)
					continue;
                    
				// Skip corners for rounder appearance on middle layers
				if (radius == 2 && (FMath::Abs(dx) == 2 && FMath::Abs(dy) == 2))
					continue;
                    
				int BlockIndex = GetBlockIndex(lx, ly, lz);
				if (Blocks[BlockIndex] == EBlock::Air)
				{
					Blocks[BlockIndex] = EBlock::Leaves;
				}
			}
		}
    
		// Add optional top leaf
		int topZ = z + LeafStartHeight + LeafHeight;
		if (topZ < ChunkSize.Z && TreeRand.FRand() < 0.5f) {
			int BlockIndex = GetBlockIndex(x, y, topZ);
			if (Blocks[BlockIndex] == EBlock::Air)
				Blocks[BlockIndex] = EBlock::Leaves;
		}
	}
}

void AGreedyChunk::SpawnCactusAt(int x, int y, int z)
{
	// Spawn 2-block cactus
	FIntVector base(x, y, z);
	FIntVector top(x, y, z + 1);

	Blocks[GetBlockIndex(base.X, base.Y, base.Z)] = EBlock::Cactus;
	Blocks[GetBlockIndex(top.X, top.Y, top.Z)] = EBlock::Cactus;

	// Record the original topmost block
	OriginalTopCactusBlocks.Add(top);
}

void AGreedyChunk::UpdateMesh()
{
	Super::UpdateMesh();
}

bool AGreedyChunk::IsInsideChunk(const FIntVector& LocalPos) const
{
	return LocalPos.X >= 0 && LocalPos.X < ChunkSize.X &&
		   LocalPos.Y >= 0 && LocalPos.Y < ChunkSize.Y &&
		   LocalPos.Z >= 0 && LocalPos.Z < ChunkSize.Z;
}
EBlock AGreedyChunk::GetBlockWorld(const FIntVector& WorldPosition) const
{
	FIntVector Local = WorldPosition - ChunkOrigin;
    UE_LOG(LogTemp, Warning, TEXT("GetBlockWorld called with Local: X=%d Y=%d Z=%d"), Local.X, Local.Y, Local.Z);
	return GetBlock(Local);

}

uint8 AGreedyChunk::GetMeta(const FIntVector& Position) const
{
	FIntVector LocalPos = Position - ChunkOrigin;
	if (IsInsideChunk(LocalPos))
	{
		int32 Index = LocalPos.Z * ChunkSize.X * ChunkSize.Y + LocalPos.Y * ChunkSize.X + LocalPos.X;
		return BlockMeta[Index];
	}
	else
	{
		AGreedyChunk* NeighborChunk = AGreedyChunk::GetChunkAt(Position, ChunkSize);
		if (!NeighborChunk)
			return 0;  // safe default: no block / no water

		return NeighborChunk->GetMeta(Position);
	}
}

AGreedyChunk* AGreedyChunk::GetChunkAt(const FIntVector& WorldBlockPosition, const FIntVector& ChunkSize)
{
	UE_LOG(LogTemp, Warning, TEXT("GetChunkAt called with WorldBlockPosition: (%d,%d,%d), ChunkSize: (%d,%d,%d)"),
		WorldBlockPosition.X, WorldBlockPosition.Y, WorldBlockPosition.Z,
		ChunkSize.X, ChunkSize.Y, ChunkSize.Z);
	int32 ChunkX = FMath::FloorToInt(static_cast<float>(WorldBlockPosition.X) / ChunkSize.X);
	int32 ChunkY = FMath::FloorToInt(static_cast<float>(WorldBlockPosition.Y) / ChunkSize.Y);
	int32 ChunkZ = FMath::FloorToInt(static_cast<float>(WorldBlockPosition.Z) / ChunkSize.Z);
	FIntVector ChunkCoords(ChunkX, ChunkY, ChunkZ);
	UE_LOG(LogTemp, Warning, TEXT("Calculated ChunkCoords: (%d,%d,%d)"), ChunkX, ChunkY, ChunkZ);
	if (AGreedyChunk::LoadedChunks.Contains(ChunkCoords))
	{
		return AGreedyChunk::LoadedChunks[ChunkCoords];
	}
	return nullptr;
}
void AGreedyChunk::SetBlockAt(const FIntVector& Position, EBlock BlockType)
{
	
	FIntVector LocalPos = Position - ChunkOrigin;
	if (!IsInsideChunk(LocalPos))
	{
		// Neighbor logic
		if (AGreedyChunk* NeighborChunk = AGreedyChunk::GetChunkAt(Position, ChunkSize))
			NeighborChunk->SetBlockAt(Position, BlockType);
		return;
	}
	int32 Index = LocalPos.Z * ChunkSize.X * ChunkSize.Y + LocalPos.Y * ChunkSize.X + LocalPos.X;
	Blocks[Index] = BlockType;
}

void AGreedyChunk::SetMeta(const FIntVector& Position, uint8 MetaValue)
{
	FIntVector LocalPos = Position - ChunkOrigin;
	
	if (!IsInsideChunk(LocalPos))
	  {
		  if (AGreedyChunk* NeighborChunk = AGreedyChunk::GetChunkAt(Position, ChunkSize))
            NeighborChunk->SetMeta(Position, MetaValue);
        return;
    }
	int32 Index = LocalPos.Z * ChunkSize.X * ChunkSize.Y + LocalPos.Y * ChunkSize.X + LocalPos.X;
	BlockMeta[Index] = MetaValue;
}
EBlock AGreedyChunk::GetBlockWithNeighbors(const FIntVector& Pos) const
{
	
	if (IsInsideChunk(Pos))
	{
		return Blocks[GetBlockIndex(Pos.X, Pos.Y, Pos.Z)];
	}

	check(ChunkSize.X != 0 && ChunkSize.Y != 0 && ChunkSize.Z != 0);

	
	// Convert global Pos to the chunk that contains it
	FIntVector WorldBlockPos = (ChunkOrigin / 100) + Pos;  // Convert ChunkOrigin from units to blocks, then add local Pos

	// Divide by (ChunkSize * 100) because you scaled by 100 units per block
	int32 NeighborX = FMath::FloorToInt(static_cast<float>(WorldBlockPos.X) / ChunkSize.X);
	int32 NeighborY = FMath::FloorToInt(static_cast<float>(WorldBlockPos.Y) / ChunkSize.Y);
	int32 NeighborZ = FMath::FloorToInt(static_cast<float>(WorldBlockPos.Z) / ChunkSize.Z);
	FIntVector NeighborChunkCoords(NeighborX, NeighborY, NeighborZ);
	
	AGreedyChunk** NeighborChunkPtr = LoadedChunks.Find(NeighborChunkCoords);
	if (!NeighborChunkPtr || !*NeighborChunkPtr) return EBlock::Air;

	AGreedyChunk* NeighborChunk = *NeighborChunkPtr;

	// Get local position relative to that neighbor chunk
	FIntVector LocalPos = WorldBlockPos - (NeighborChunk->ChunkOrigin / 100);

	if (!NeighborChunk->IsInsideChunk(LocalPos)) return EBlock::Air;

	return NeighborChunk->Blocks[NeighborChunk->GetBlockIndex(LocalPos.X, LocalPos.Y, LocalPos.Z)];
}