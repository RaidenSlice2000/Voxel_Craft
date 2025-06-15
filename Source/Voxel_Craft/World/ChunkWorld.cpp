
#include "ChunkWorld.h"

#include "TimerManager.h"
#include "Voxel_craft/Chunks/ChunkBase.h"
#include "Voxel_craft/Utils/WaterSimulator.h"
#include "Voxel_craft/Chunks/GreedyChunk.h"
#include "Kismet/GameplayStatics.h"

// Sets default values
AChunkWorld::AChunkWorld()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
}

// Called when the game starts or when spawned
void AChunkWorld::BeginPlay()
{
	Super::BeginPlay();
	
	PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	
	if (PlayerPawn)
	{
		GetWorldTimerManager().SetTimer(UpdateTimerHandle, this, &AChunkWorld::UpdateChunks, 0.5f, true);
	}
	WaterSimulator = new FWaterSimulator(ChunkSize); // ✅ create simulator

	WaterSimulator->SetChunkFetcher([this](const FIntVector& Position) -> AGreedyChunk*
		  {
				int32 ChunkX = FMath::FloorToInt(static_cast<float>(Position.X) / ChunkSize.X);
				int32 ChunkY = FMath::FloorToInt(static_cast<float>(Position.Y) / ChunkSize.Y);
				int32 ChunkZ = FMath::FloorToInt(static_cast<float>(Position.Z) / ChunkSize.Z);

			FIntVector ChunkCoords(ChunkX, ChunkY, ChunkZ);
		
			bool bFound = SpawnedChunks.Contains(ChunkCoords);

			UE_LOG(LogTemp, Warning, TEXT("📦 ChunkFetcher | WorldPos: %s → ChunkCoords: %s | Found: %s"),
				 *Position.ToString(),
				 *ChunkCoords.ToString(),
				 bFound ? TEXT("✅") : TEXT("❌"));

			return bFound ? Cast<AGreedyChunk>(SpawnedChunks[ChunkCoords]) : nullptr;
		  });
	
	switch (GenerationType)
	{
	case EGenerationType::GT_3D:
		Generate3DWorld();
		break;
	case EGenerationType::GT_2D:
		Generate2DWorld();
		break;
	default:
		throw std::invalid_argument("Invalid Generation Type");
	}
	
	UE_LOG(LogTemp, Warning, TEXT("%d Chunks Created"), ChunkCount);
}

void AChunkWorld::Generate3DWorld()
{
		if (!ChunkType)
		{
			UE_LOG(LogTemp, Error, TEXT("ChunkType is not set!"));
			return;  // Early exit if ChunkType is not set
		}
	for (int x = -DrawDistance; x <= DrawDistance; x++)
	{
		for (int y = -DrawDistance; y <= DrawDistance; ++y)
		{
			for (int z = -DrawDistance; z <= DrawDistance; ++z)
			{
				
				auto transform = FTransform(
					FRotator::ZeroRotator,
					FVector(x * ChunkSize.X * 100, y * ChunkSize.Y * 100, z * ChunkSize.Z * 100),
					FVector::OneVector
				);

				const auto chunk = GetWorld()->SpawnActorDeferred<AChunkBase>(
					ChunkType,
					transform,
					this
				);

				chunk->GenerationType = EGenerationType::GT_3D;
				chunk->Frequency = Frequency;
				chunk->Materials = Materials;
				chunk->ChunkSize = ChunkSize;

				chunk->SetSeed(Seed);

				UGameplayStatics::FinishSpawningActor(chunk, transform);

				ChunkCount++;
			}
		}
	}
}

void AChunkWorld::Generate2DWorld()
{
	
	for (int x = -DrawDistance; x <= DrawDistance; x++)
	{
		for (int y = -DrawDistance; y <= DrawDistance; ++y)
		{
			FIntVector Coord(x, y, 0);
			if (SpawnedChunks.Contains(Coord))
			{
				continue;
			}
			{
				auto transform = FTransform(
				FRotator::ZeroRotator,
				FVector(x * ChunkSize.X * 100, y * ChunkSize.Y * 100, 0),
				FVector::OneVector
			);

				const auto chunk = GetWorld()->SpawnActorDeferred<AChunkBase>(
					ChunkType,
					transform,
					this
				);

				chunk->GenerationType = EGenerationType::GT_2D;
				chunk->Frequency = Frequency;
				chunk->Materials = Materials;
				chunk->ChunkSize = ChunkSize;

				chunk->SetSeed(Seed);

				UGameplayStatics::FinishSpawningActor(chunk, transform);

				if (WaterSimulator && Cast<AGreedyChunk>(chunk))
				{
					Cast<AGreedyChunk>(chunk)->SetWaterSimulator(WaterSimulator);
				}
				
				SpawnedChunks.Add(Coord, chunk);
				ChunkCount++;
			}
		}
	}
}
FIntVector AChunkWorld::WorldToChunkCoord(const FVector& Location) const
{
	return FIntVector(
		FMath::FloorToInt(Location.X / (ChunkSize.X * 100)),
		FMath::FloorToInt(Location.Y / (ChunkSize.Y * 100)),
		GenerationType == EGenerationType::GT_3D
			? FMath::FloorToInt(Location.Z / (ChunkSize.Z * 100))
			: 0
	);
}
void AChunkWorld::SpawnChunkAt(const FIntVector& Coord)
{
	if (!ChunkType) return;

	if (SpawnedChunks.Contains(Coord))
	{
		return; // Do nothing if the chunk is already spawned
	}

	
	FVector SpawnLocation = FVector(Coord.X, Coord.Y, Coord.Z) * FVector(ChunkSize.X, ChunkSize.Y, ChunkSize.Z) * 100.0f;

	FTransform Transform(FRotator::ZeroRotator, SpawnLocation, FVector::OneVector);
	AChunkBase* Chunk = GetWorld()->SpawnActorDeferred<AChunkBase>(ChunkType, Transform, this);

	Chunk->GenerationType = GenerationType;
	Chunk->Frequency = Frequency;
	Chunk->Materials = Materials;
	Chunk->ChunkSize = ChunkSize;
	Chunk->SetSeed(Seed);

	UGameplayStatics::FinishSpawningActor(Chunk, Transform);
	
	if (AGreedyChunk* Greedy = Cast<AGreedyChunk>(Chunk))
	{
		Greedy->SetWaterSimulator(WaterSimulator);
	}
	
	SpawnedChunks.Add(Coord, Chunk);
}
void AChunkWorld::RemoveChunkAt(const FIntVector& Coord)
{
	if (AChunkBase* Chunk = SpawnedChunks.FindRef(Coord))
	{
		Chunk->Destroy();
		SpawnedChunks.Remove(Coord);
	}
}
void AChunkWorld::UpdateChunks()
{
	if (!PlayerPawn) return;

	FIntVector CenterCoord = WorldToChunkCoord(PlayerPawn->GetActorLocation());

	TSet<FIntVector> DesiredCoords;

	for (int x = -DrawDistance; x <= DrawDistance; ++x)
	{
		for (int y = -DrawDistance; y <= DrawDistance; ++y)
		{
			for (int z = GenerationType == EGenerationType::GT_3D ? -DrawDistance : 0;
				 z <= (GenerationType == EGenerationType::GT_3D ? DrawDistance : 0); ++z)
			{
				FIntVector Coord = CenterCoord + FIntVector(x, y, z);
				DesiredCoords.Add(Coord);

				if (!SpawnedChunks.Contains(Coord))
				{
					SpawnChunkAt(Coord);
				}
			}
		}
	}

	// Remove out-of-range chunks
	TArray<FIntVector> ToRemove;
	for (const auto& Pair : SpawnedChunks)
	{
		if (!DesiredCoords.Contains(Pair.Key))
		{
			ToRemove.Add(Pair.Key);
		}
	}

	for (const FIntVector& Coord : ToRemove)
	{
		RemoveChunkAt(Coord);
	}
}
void AChunkWorld::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (WaterSimulator)
	{
		WaterSimulator->Tick(DeltaTime);
	}
}
void AChunkWorld::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (WaterSimulator)
	{
		delete WaterSimulator;
		WaterSimulator = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}