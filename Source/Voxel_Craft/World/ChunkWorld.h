// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "Voxel_Craft/Utils/Enums.h"
#include "Voxel_Craft/Utils/WaterSimulator.h"
#include "ChunkWorld.generated.h"

class AChunkBase;

UCLASS()
class AChunkWorld final : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	
	UPROPERTY(EditInstanceOnly, Category = "World")
	TSubclassOf<AChunkBase> ChunkType;

	UPROPERTY(EditInstanceOnly, category = "World")
	int DrawDistance = 15;
	
	UPROPERTY(EditInstanceOnly, Category = "Chunk")
	TArray<UMaterialInterface*> Materials;
	
	UPROPERTY(EditInstanceOnly, category = "Chunk")
	FIntVector ChunkSize = FIntVector(16, 16, 256);
	
	UPROPERTY(EditInstanceOnly, Category="Height Map")
	EGenerationType GenerationType;

	UPROPERTY(EditInstanceOnly, Category="Height Map")
	float Frequency = 0.03f;

	UPROPERTY(EditInstanceOnly, Category = "World")
	int32 Seed = 1337; 

	AChunkWorld();
	
protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	APawn* PlayerPawn = nullptr;

	FWaterSimulator* WaterSimulator = nullptr;

	// Map of all currently spawned chunks
	TMap<FIntVector, AChunkBase*> SpawnedChunks;

	// Timer to periodically update chunks
	FTimerHandle UpdateTimerHandle;

	// Converts world position to chunk grid coordinate
	FIntVector WorldToChunkCoord(const FVector& Location) const;

	// Spawns a chunk at a specific chunk coordinate
	void SpawnChunkAt(const FIntVector& Coord);

	// Destroys and removes a chunk at a coordinate
	void RemoveChunkAt(const FIntVector& Coord);

	// Updates visible chunks around player
	void UpdateChunks();
	virtual void Tick(float DeltaTime) override;

	int ChunkCount;
	
	void Generate3DWorld();
	void Generate2DWorld();
};