
#include "WaterSimulator.h"
#include "Voxel_Craft/Chunks/GreedyChunk.h"
#include "Voxel_Craft/Utils/Enums.h"

FWaterSimulator::FWaterSimulator(const FIntVector& InChunkSize)
: ChunkSize(InChunkSize),                 // initializer list here
   bInfiniteSourcesEnabled(true),
   bEvaporationEnabled(false),
   EvaporationRate(5.0f)
{
    
}

void FWaterSimulator::SetChunkFetcher(const TFunction<AGreedyChunk*(const FIntVector&)>& InChunkFetcher)
{
    GetChunkAt = InChunkFetcher;
}

void FWaterSimulator::EnqueueWaterBlock(const FIntVector& GlobalPosition, uint8 WaterLevel)
{
    WaterQueue.Enqueue({ GlobalPosition, WaterLevel });

    UE_LOG(LogTemp, Warning, TEXT("Enqueued water block at %s with level %d"), *GlobalPosition.ToString(), WaterLevel);

    
    // If this is a source block, track it for infinite sources feature
    if (WaterLevel == 0 && bInfiniteSourcesEnabled)
    {
        WaterSources.Add(GlobalPosition);
        UE_LOG(LogTemp, Warning, TEXT("Added water source at %s"), *GlobalPosition.ToString());

    }
}

void FWaterSimulator::Tick(float DeltaTime)
{
    constexpr int32 MaxIterations = 1; // Prevent infinite loops
    int32 IterationCount = 0;

    // Process water flow
    while (!WaterQueue.IsEmpty() && IterationCount++ < MaxIterations)
    {
        TPair<FIntVector, uint8> Entry;
        WaterQueue.Dequeue(Entry);

        const FIntVector& Position = Entry.Key;
        const uint8 WaterLevel = Entry.Value;

        // Check if this block could become an infinite source
        if (bInfiniteSourcesEnabled && WaterLevel > 0)
        {
            CheckForInfiniteSource(Position);
        }

        TrySpread(Position, WaterLevel);
    }
    
    // Process evaporation if enabled
    if (bEvaporationEnabled)
    {
        ProcessEvaporation(DeltaTime);
    }
}

void FWaterSimulator::TrySpread(const FIntVector& Position, uint8 CurrentStrength)
{
    UE_LOG(LogTemp, Warning, TEXT("Trying to spread from %s with strength %d"), *Position.ToString(), CurrentStrength);

    // In Minecraft, water only spreads up to 7 blocks from source
    if (CurrentStrength >= 8) return; // Too weak to spread

    static const TArray<FIntVector> SpreadDirs = {
        FIntVector(0, 0, -1), // Down first (gravity)
        FIntVector(1, 0, 0),  // Then horizontally
        FIntVector(-1, 0, 0),
        FIntVector(0, 1, 0),
        FIntVector(0, -1, 0)
    };

    for (const FIntVector& Dir : SpreadDirs)
    {
        
        FIntVector NeighborPos = Position + Dir;

        // Get the chunk that contains the neighbor block
        AGreedyChunk* NeighborChunk = GetChunkAt(NeighborPos);
        if (!NeighborChunk)
        {
            UE_LOG(LogTemp, Warning, TEXT("❌ No chunk found at neighbor position %s"), *NeighborPos.ToString());
            continue;
        }

        // Convert NeighborPos to local block position inside its chunk
        FIntVector ChunkCoords(
            FMath::FloorToInt((float)NeighborPos.X / ChunkSize.X),
            FMath::FloorToInt((float)NeighborPos.Y / ChunkSize.Y),
            FMath::FloorToInt((float)NeighborPos.Z / ChunkSize.Z)
        );
        FIntVector ChunkOrigin = ChunkCoords * ChunkSize;
        FIntVector LocalBlockPos = NeighborPos - ChunkOrigin;

        // Verify the block is inside that chunk's bounds
        if (!NeighborChunk->IsInsideChunk(LocalBlockPos))
        {
            UE_LOG(LogTemp, Warning, TEXT("❌ Neighbor local position %s not inside chunk for world pos %s"), *LocalBlockPos.ToString(), *NeighborPos.ToString());
            continue;
        }

        // Read block type and water level
        EBlock NeighborBlock = NeighborChunk->GetBlockWorld(NeighborPos);
        uint8 ExistingStrength = NeighborChunk->GetMeta(NeighborPos);

        UE_LOG(LogTemp, Warning, TEXT("🔎 Checking neighbor %s: block=%d, strength=%d"), *NeighborPos.ToString(), (int32)NeighborBlock, ExistingStrength);

        // Determine new strength
        uint8 NewStrength = (Dir.Z < 0) ? CurrentStrength : CurrentStrength + 1;

        // Can spread into air or weaker water
        if (NeighborBlock == EBlock::Air ||
            (NeighborBlock == EBlock::Water && ExistingStrength > NewStrength))
        {
            NeighborChunk->SetBlockAt(NeighborPos, EBlock::Water);
            NeighborChunk->SetMeta(NeighborPos, NewStrength);
            NeighborChunk->UpdateMesh();

            UE_LOG(LogTemp, Warning, TEXT("✅ Placed water at %s with strength %d"), *NeighborPos.ToString(), NewStrength);

            WaterQueue.Enqueue({ NeighborPos, NewStrength });

            if (bEvaporationEnabled && NewStrength > 0)
            {
                FlowingWaterBlocks.Add(NeighborPos, 0.0f); // Track for evaporation
            }
        }
    }
}

void FWaterSimulator::CheckForInfiniteSource(const FIntVector& Position)
{
    // Already a source? Skip check
    if (IsWaterSource(Position))
        return;
        
    // Minecraft rule: If a water block has 2+ source blocks adjacent horizontally,
    // it becomes a source block too (creates infinite water pools)
    
    static const TArray<FIntVector> HorizontalDirs = {
        FIntVector(1, 0, 0),
        FIntVector(-1, 0, 0),
        FIntVector(0, 1, 0),
        FIntVector(0, -1, 0)
    };
    
    int32 AdjacentSources = 0;
    
    for (const FIntVector& Dir : HorizontalDirs)
    {
        FIntVector AdjacentPos = Position + Dir;
        
        if (IsWaterSource(AdjacentPos))
        {
            AdjacentSources++;
            
            // Early out once we find 2 sources
            if (AdjacentSources >= 2)
                break;
        }
    }
    
    // If we have 2+ adjacent sources, this block becomes a source
    if (AdjacentSources >= 2)
    {
        AGreedyChunk* Chunk = GetChunkAt(Position);
        if (Chunk && Chunk->GetBlockWorld(Position) == EBlock::Water)
        {
            // Convert to source block
            Chunk->SetMeta(Position, 0);
            
            // Add to source tracking
            WaterSources.Add(Position);
            
            // Re-spread from this new source
            WaterQueue.Enqueue({ Position, 0 });
        }
    }
}

bool FWaterSimulator::IsWaterSource(const FIntVector& Position) const
{
        UE_LOG(LogTemp, Warning, TEXT("IsWaterSource called for %s"), *Position.ToString());

    if (!GetChunkAt)
    {
        UE_LOG(LogTemp, Warning, TEXT("IsWaterSource: GetChunkAt is null"));
        return false;
    }
        
    AGreedyChunk* Chunk = GetChunkAt(Position);
    if (!Chunk)
    {
        UE_LOG(LogTemp, Warning, TEXT("IsWaterSource: No chunk found at %s"), *Position.ToString());
        return false;
    }
        
    EBlock BlockType = Chunk->GetBlockWorld(Position);
    uint8 MetaValue = Chunk->GetMeta(Position);

    bool bIsSource = (BlockType == EBlock::Water && MetaValue == 0);

    UE_LOG(LogTemp, Warning, TEXT("IsWaterSource check at %s -> BlockType: %d, Meta: %d, Result: %s"),
        *Position.ToString(),
        static_cast<int32>(BlockType),
        MetaValue,
        bIsSource ? TEXT("TRUE") : TEXT("FALSE"));

    return bIsSource;
}

void FWaterSimulator::ProcessEvaporation(float DeltaTime)
{
    TArray<FIntVector> BlocksToRemove;
    
    // Process each flowing water block
    for (auto& Pair : FlowingWaterBlocks)
    {
        const FIntVector& Position = Pair.Key;
        float& Timer = Pair.Value;
        
        // Update timer
        Timer += DeltaTime;
        
        // Time to evaporate?
        if (Timer >= EvaporationRate)
        {
            AGreedyChunk* Chunk = GetChunkAt(Position);
            if (Chunk && Chunk->GetBlockWorld(Position) == EBlock::Water)
            {
                uint8 CurrentLevel = Chunk->GetMeta(Position);
                
                // Increase water level (reduce flow)
                CurrentLevel++;
                
                if (CurrentLevel >= 8)
                {
                    // Water completely evaporated
                    Chunk->SetBlockAt(Position, EBlock::Air);
                    Chunk->SetMeta(Position, 0);
                    Chunk->UpdateMesh();
                    BlocksToRemove.Add(Position);
                }
                else
                {
                    // Update water level
                    Chunk->SetMeta(Position, CurrentLevel);
                    
                    // Reset timer
                    Timer = 0.0f;
                }
            }
            else
            {
                // Block is no longer water, remove from tracking
                BlocksToRemove.Add(Position);
            }
        }
    }
    
    // Clean up removed blocks
    for (const FIntVector& Pos : BlocksToRemove)
    {
        FlowingWaterBlocks.Remove(Pos);
    }
}

void FWaterSimulator::SetInfiniteSourcesEnabled(bool bEnable)
{
    bInfiniteSourcesEnabled = bEnable;
    
    // Clear source tracking data if disabled
    if (!bEnable)
    {
        WaterSources.Empty();
    }
}

void FWaterSimulator::SetEvaporationEnabled(bool bEnable, float Rate)
{
    bEvaporationEnabled = bEnable;
    EvaporationRate = FMath::Max(0.1f, Rate);
    
    // Clear evaporation tracking data if disabled
    if (!bEnable)
    {
        FlowingWaterBlocks.Empty();
    }
}