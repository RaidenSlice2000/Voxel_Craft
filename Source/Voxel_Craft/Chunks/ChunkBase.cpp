#include "ChunkBase.h"

#include "Voxel_craft/Utils/FastNoiseLite.h"
#include "ProceduralMeshComponent.h"


// Sets default values
AChunkBase::AChunkBase()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	Mesh = CreateDefaultSubobject<UProceduralMeshComponent>("Mesh");
	Noise = new FastNoiseLite();
	BiomeNoise = new FastNoiseLite();
	

	// Mesh Settings
	Mesh->SetCastShadow(false);

	// Set Mesh as root
	SetRootComponent(Mesh);
}


// Called when the game starts or when spawned
void AChunkBase::BeginPlay()
{
	Super::BeginPlay();

	Noise->SetSeed(Seed);

	Noise->SetFrequency(Frequency);
	Noise->SetNoiseType(FastNoiseLite::NoiseType_Perlin);
	Noise->SetFractalType(FastNoiseLite::FractalType_FBm);

	BiomeNoise->SetSeed(Seed + 1);
	
	BiomeNoise->SetFrequency(Frequency * 0.2f); // lower frequency for large biome patches
	BiomeNoise->SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2); // different type = more biome variety
	BiomeNoise->SetFractalType(FastNoiseLite::FractalType_None);

	Setup();
	
	GenerateHeightMap();

	ClearMesh();
	
	GenerateMesh();
	
	ApplyMesh();
}

void AChunkBase::GenerateHeightMap()
{
	switch (GenerationType)
	{
	case EGenerationType::GT_3D:
		Generate3DHeightMap(GetActorLocation() / 100);
		break;
	case EGenerationType::GT_2D:
		Generate2DHeightMap(GetActorLocation() / 100);
		break;
	default:
		throw std::invalid_argument("Invalid Generation Type");
	}
}

void AChunkBase::ApplyMesh() const
{
	if (!Mesh) 
	{
		UE_LOG(LogTemp, Warning, TEXT("Mesh is not valid!"));
		return;  // Early return if Mesh is invalid
	}
	// Clear any previous mesh sections
	Mesh->ClearAllMeshSections();

	// Loop through mesh data for each material slot
	for (int32 i = 0; i < MeshPerMaterial.Num(); ++i)
	{
		if (MeshPerMaterial[i].Vertices.Num() == 0) continue;

		Mesh->CreateMeshSection(
			i,
			MeshPerMaterial[i].Vertices,
			MeshPerMaterial[i].Triangles,
			MeshPerMaterial[i].Normals,
			MeshPerMaterial[i].UV0,
			MeshPerMaterial[i].Colors,
			TArray<FProcMeshTangent>(),
			true
		);

		// Check if there is a valid material to assign
		if (Materials.IsValidIndex(i))
		{
			Mesh->SetMaterial(i, Materials[i]);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Material index %d is out of bounds!"), i);
		}
	}
}
void AChunkBase::ClearMesh()
{
	VertexCount = 0;
	MeshData.Clear();

	// Clear out any previous per-material mesh data
	MeshPerMaterial.Empty();
	VertexCountPerMat.Empty();

	// Resize to match number of materials
	MeshPerMaterial.SetNum(Materials.Num());
	VertexCountPerMat.SetNum(Materials.Num());
}

void AChunkBase::ModifyVoxel(const FIntVector Position, const EBlock Block)
{
	if (Position.X >= ChunkSize.X || Position.Y >= ChunkSize.Y || Position.Z >= ChunkSize.Z || Position.X < 0 || Position.Y < 0 || Position.Z < 0) return;

	ModifyVoxelData(Position,Block);

	ClearMesh();

	GenerateMesh();

	ApplyMesh();
	
}
void AChunkBase::UpdateMesh()
{
	// Default update: regenerate mesh data and apply it
    ClearMesh();
	
	GenerateMesh(); // Call your existing virtual GenerateMesh function that builds the mesh data
    
	ApplyMesh();    // Call your private ApplyMesh function that applies MeshData to the procedural mesh component
}