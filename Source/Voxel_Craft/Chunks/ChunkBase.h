// Fill out your copyright notice in the Description page of Project Settings.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "Voxel_Craft/Utils/ChunkMeshData.h"
#include "Voxel_Craft/Utils/Enums.h"

#include "ChunkBase.generated.h"

class FastNoiseLite;
class UProceduralMeshComponent;

UCLASS(Abstract)
class VOXEL_CRAFT_API AChunkBase : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AChunkBase();

	virtual void UpdateMesh();

	UPROPERTY(EditDefaultsOnly, Category="Chunk")
	FIntVector ChunkSize = FIntVector(16, 16, 256);;
	
	//TObjectPtr<UMaterialInterface> Material;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chunk|Materials")
	TArray<UMaterialInterface*> Materials;
	
	float Frequency;
	EGenerationType GenerationType;

	UFUNCTION(BlueprintCallable, Category="Chunk")
	void ModifyVoxel(FIntVector Position, EBlock Block);
	
	void SetSeed(int32 InSeed) { Seed = InSeed; }
	

	
protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	
	virtual void Setup() PURE_VIRTUAL(AChunkBase::Setup, );
	virtual void Generate2DHeightMap(const FVector Position) PURE_VIRTUAL(AChunkBase::Generate2DHeightMap, );
	virtual void Generate3DHeightMap(const FVector Position) PURE_VIRTUAL(AChunkBase::Generate3DHeightMap, );
	virtual void GenerateMesh() PURE_VIRTUAL(AChunkBase::GenerateMesh, );
	
	virtual void ModifyVoxelData(const FIntVector Position, const EBlock Block) PURE_VIRTUAL(AChunkBase::ModifyVoxelData);

	int32 Seed;
	TObjectPtr<UProceduralMeshComponent> Mesh;
	FastNoiseLite* Noise;
	FastNoiseLite* BiomeNoise;
	FChunkMeshData MeshData;
	int VertexCount = 0;

	UPROPERTY()
	TArray<FChunkMeshData> MeshPerMaterial;

	UPROPERTY()
	TArray<int32> VertexCountPerMat;

private:
	void ApplyMesh() const;
	void ClearMesh();
	virtual void GenerateHeightMap();
};