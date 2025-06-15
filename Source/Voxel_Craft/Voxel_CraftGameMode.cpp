// Copyright Epic Games, Inc. All Rights Reserved.

#include "Voxel_CraftGameMode.h"
#include "Voxel_CraftCharacter.h"
#include "UObject/ConstructorHelpers.h"

AVoxel_CraftGameMode::AVoxel_CraftGameMode()
	: Super()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnClassFinder(TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter"));
	DefaultPawnClass = PlayerPawnClassFinder.Class;

}
