// Fill out your copyright notice in the Description page of Project Settings.


#include "NaiveChunk.h"

#include "Voxel_Craft/Utils/FastNoiseLite.h"

void ANaiveChunk::Setup()
{
	// Initialize Blocks
	Blocks.SetNum(ChunkSize.X * ChunkSize.Y * ChunkSize.Z);
}

void ANaiveChunk::Generate2DHeightMap(const FVector Position)
{
	for (int x = 0; x < ChunkSize.X; x++)
	{
		for (int y = 0; y < ChunkSize.Y; y++)
		{
			const float Xpos = x + Position.X;
			const float ypos = y + Position.Y;
			
			const int Height = FMath::Clamp(FMath::RoundToInt((Noise->GetNoise(Xpos, ypos) + 1) * ChunkSize.Z / 2), 0, ChunkSize.Z);

			for (int z = 0; z < Height; z++)
			{
				Blocks[GetBlockIndex(x,y,z)] = EBlock::Stone;
			}

			for (int z = Height; z < ChunkSize.Z; z++)
			{
				Blocks[GetBlockIndex(x,y,z)] = EBlock::Air;
			}
			
		}
	}
}

void ANaiveChunk::Generate3DHeightMap(const FVector Position)
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

void ANaiveChunk::GenerateMesh()
{
	for (int x = 0; x < ChunkSize.X; x++)
	{
		for (int y = 0; y < ChunkSize.Y; y++)
		{
			for (int z = 0; z < ChunkSize.Z; z++)
			{
				if (Blocks[GetBlockIndex(x,y,z)] != EBlock::Air)
				{
					const auto Position = FVector(x,y,z);
					
					for (auto Direction : {EChunkDirection::Forward, EChunkDirection::Right, EChunkDirection::Back, EChunkDirection::Left, EChunkDirection::Up, EChunkDirection::Down})
					{
						if (Check(GetPositionInDirection(Direction, Position)))
						{
							CreateFace(Direction, Position * 100);
						}
					}
				}
			}
		}
	}
}

bool ANaiveChunk::Check(const FVector& Position) const
{
	if (Position.X >= ChunkSize.X || Position.Y >= ChunkSize.Y || Position.X < 0 || Position.Y < 0 || Position.Z < 0) return true;
	if (Position.Z >= ChunkSize.Z) return true;
	return Blocks[GetBlockIndex(Position.X, Position.Y, Position.Z)] == EBlock::Air;
}

void ANaiveChunk::CreateFace(const EChunkDirection Direction, const FVector& Position)
{
	const auto Color = FColor::MakeRandomColor();
	const auto Normal = GetNormal(Direction);
	
	MeshData.Vertices.Append(GetFaceVertices(Direction, Position));
	MeshData.Triangles.Append({ VertexCount + 3, VertexCount + 2, VertexCount, VertexCount + 2, VertexCount + 1, VertexCount });
	MeshData.Normals.Append({Normal, Normal, Normal, Normal});
	MeshData.Colors.Append({Color, Color, Color, Color});
	MeshData.UV0.Append({ FVector2D(1,1), FVector2D(1,0), FVector2D(0,0), FVector2D(0,1) });

	VertexCount += 4;
}

TArray<FVector> ANaiveChunk::GetFaceVertices(EChunkDirection Direction, const FVector& Position) const
{
	TArray<FVector> Vertices;

	for (int i = 0; i < 4; i++)
	{
		Vertices.Add(BlockVertexData[BlockTriangleData[i + static_cast<int>(Direction) * 4]] + Position);
	}
	
	return Vertices;
}

FVector ANaiveChunk::GetPositionInDirection(const EChunkDirection Direction, const FVector& Position)
{
	switch (Direction)
	{
	case EChunkDirection::Forward: return Position + FVector::ForwardVector;
	case EChunkDirection::Back: return Position + FVector::BackwardVector;
	case EChunkDirection::Left: return Position + FVector::LeftVector;
	case EChunkDirection::Right: return Position + FVector::RightVector;
	case EChunkDirection::Up: return Position + FVector::UpVector;
	case EChunkDirection::Down: return Position + FVector::DownVector;
	default: throw std::invalid_argument("Invalid direction");
	}
}

FVector ANaiveChunk::GetNormal(const EChunkDirection Direction)
{
	switch (Direction)
	{
	case EChunkDirection::Forward: return FVector::ForwardVector;
	case EChunkDirection::Right: return FVector::RightVector;
	case EChunkDirection::Back: return FVector::BackwardVector;
	case EChunkDirection::Left: return FVector::LeftVector;
	case EChunkDirection::Up: return FVector::UpVector;
	case EChunkDirection::Down: return FVector::DownVector;
	default: throw std::invalid_argument("Invalid direction");
	}
}

void ANaiveChunk::ModifyVoxelData(const FIntVector Position, const EBlock Block)
{
	const int Index = GetBlockIndex(Position.X, Position.Y, Position.Z);
	
	Blocks[Index] = Block;
}

int ANaiveChunk::GetBlockIndex(const int X, const int Y, const int Z) const
{
	return Z * ChunkSize.X * ChunkSize.Y + Y * ChunkSize.X + X;
}