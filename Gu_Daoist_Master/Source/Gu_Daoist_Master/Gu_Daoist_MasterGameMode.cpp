// Copyright Epic Games, Inc. All Rights Reserved.

#include "Gu_Daoist_MasterGameMode.h"
#include "GuPlayerState.h"

AGu_Daoist_MasterGameMode::AGu_Daoist_MasterGameMode()
{
    // Domain/refinement state survives pawn replacement on PlayerState.
    PlayerStateClass = AGuPlayerState::StaticClass();
}
