// Copyright Epic Games, Inc. All Rights Reserved.

#include "Voxel_CraftPickUpComponent.h"

UVoxel_CraftPickUpComponent::UVoxel_CraftPickUpComponent()
{
	// Setup the Sphere Collision
	SphereRadius = 32.f;
}

void UVoxel_CraftPickUpComponent::BeginPlay()
{
	Super::BeginPlay();

	// Register our Overlap Event
	OnComponentBeginOverlap.AddDynamic(this, &UVoxel_CraftPickUpComponent::OnSphereBeginOverlap);
}

void UVoxel_CraftPickUpComponent::OnSphereBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// Checking if it is a First Person Character overlapping
	AVoxel_CraftCharacter* Character = Cast<AVoxel_CraftCharacter>(OtherActor);
	if(Character != nullptr)
	{
		// Notify that the actor is being picked up
		OnPickUp.Broadcast(Character);

		// Unregister from the Overlap Event so it is no longer triggered
		OnComponentBeginOverlap.RemoveAll(this);
	}
}
