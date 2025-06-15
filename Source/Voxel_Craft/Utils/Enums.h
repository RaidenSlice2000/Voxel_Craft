#pragma once


UENUM(BlueprintType)
enum class EChunkDirection : uint8
{
	Forward, Right, Back, Left, Up, Down
};
UENUM(BlueprintType)
enum class EBlock  : uint8
{
	Null, Air, Stone, Dirt, Grass, Log, Leaves , Sand, Snow, Cactus, Sandstone , Water 

};
UENUM(BlueprintType)
enum class EGenerationType : uint8
{
	GT_3D UMETA(DisplayName = "3D"),
	GT_2D UMETA(DisplayName = "2D"),
};

UENUM(BlueprintType)
enum class EBiomeType : uint8
{
	None		UMETA(DisplayName = "None"),
	Desert		UMETA(DisplayName = "Desert"),
	Plains		UMETA(DisplayName = "Plains"),
	Forest		UMETA(DisplayName = "Forest"),
	Mountain	UMETA(DisplayName = "Mountain"),
	Snowy		UMETA(DisplayName = "Snowy")
};
