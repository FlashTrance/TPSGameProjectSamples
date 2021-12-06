// A large smattering of code from the base Lobby Game Mode, which includes map voting and displays various information to the player,
// including showing player profile pictures, number of players in the lobby, time left until the match starts, etc.
// ----------------------------------------------------------

#include "MGLobbyGameMode.h"
#include "MGGameInstance.h"
#include "MGGameState.h"
#include "MGPlayerState.h"
#include "MGPlayerController.h"
#include "MGLobbyController.h"
#include "MGLobbyPawn.h"

#include "Kismet\GameplayStatics.h"
#include "Kismet\KismetStringLibrary.h"
#include "TimerManager.h"
#include "EngineUtils.h"
#include "Engine/World.h"

// Constructor
AMGLobbyGameMode::AMGLobbyGameMode()
{
	PlayerStateClass = AMGPlayerState::StaticClass();
	GameStateClass = AMGGameState::StaticClass();
	bUseSeamlessTravel = true;
	
	NumberOfPlayers = 0;
	TimeToStartMatch = MaxTimeToStartMatch;
	CloseVotingTime = 5;

	GenerateRandomMapIndices();
}


// BEGINPLAY()
void AMGLobbyGameMode::BeginPlay()
{
	Super::BeginPlay();

	UMGGameInstance* GI = Cast<UMGGameInstance>(GetGameInstance());
	if (GI && GI->NumPlayersInGame > 0) { LobbyCapacity = GI->NumPlayersInGame; }
}



// PLAYER LOGIN (Seamless Travel, Post-match)
void AMGLobbyGameMode::HandleSeamlessTravelPlayer(AController*& TravellingPC)
{
	Super::HandleSeamlessTravelPlayer(TravellingPC);

	if (TravellingPC)
	{
		// Increment number of players
		NumberOfPlayers++;

		if (NumberOfPlayers <= LobbyCapacity)
		{
			// Initialize Traveling Player
			AMGLobbyController* PC = Cast<AMGLobbyController>(TravellingPC);
			if (PC)
			{
				PCArray.Add(PC);
				PC->SetMapSelection(MapIndex1, MapIndex2, MapIndex3);
				PC->InitLoadoutFromPlayerState();
			}
			else { Logout(TravellingPC); }

			// Reset PlayerState vars
			AMGPlayerState* PS = Cast<AMGPlayerState>(PC->PlayerState);
			if (PS)
			{
				PS->Kills = 0;
				PS->Deaths = 0;
				PS->Assists = 0;
				PS->PlayerScore = 0;
				PS->Kisses = 0;

				// Track team numbers
				if (PS->TeamID == 1) { TeamLightNum++; }
				else { TeamDarkNum++; }
			}
			else { Logout(TravellingPC); }
		}

		// Check if lobby is full, start timer if so
		if (NumberOfPlayers >= LobbyCapacity)
		{
			GetWorldTimerManager().SetTimer(TimeRemainingHandle, this, &AMGLobbyGameMode::CheckTimeRemaining, 1.0f, true);
		}
	}
}


// POST SEAMLESS TRAVEL
void AMGLobbyGameMode::PostSeamlessTravel()
{
	Super::PostSeamlessTravel();

	StartLobbyAfterSeamlessTravel();
}


// PLAYER LOGIN
void AMGLobbyGameMode::PostLogin(APlayerController* NewPlayer)
{
	if (NumberOfPlayers <= LobbyCapacity)
	{
		Super::PostLogin(NewPlayer);
		UE_LOG(LogTemp, Warning, TEXT("Player %s joined the Lobby"), *NewPlayer->GetName());
		
		// Initialize Player
		NumberOfPlayers++;
		AMGLobbyController* PC = Cast<AMGLobbyController>(NewPlayer);
		if (PC) 
		{
			PCArray.Add(PC);

			// Post Login, need to send HTTP request to populate PlayerState
			PC->GetLoadoutData();
			PC->GetPlayerStatsData();

			// Setup map selection info
			PC->SetMapSelection(MapIndex1, MapIndex2, MapIndex3);
			if (Map1Votes > 0 || Map2Votes > 0 || Map3Votes > 0) { PC->CLIENT_UpdateMapVotes(Map1Votes, Map2Votes, Map3Votes); }
		}
		else { Logout(NewPlayer); }

		// Initialize PlayerState vars
		AMGPlayerState* PS = Cast<AMGPlayerState>(NewPlayer->PlayerState);
		if (PS)
		{
			PS->Kills = 0;
			PS->Deaths = 0;
			PS->Assists = 0;
			PS->PlayerScore = 0;
			PS->Kisses = 0;
			
			// Assign player to team
			if (TeamLightNum > TeamDarkNum)
			{
				PS->MP_UUID = TeamDarkNum;
				PS->TeamID = 0;
				TeamDarkNum++;
			}
			else
			{
				PS->MP_UUID = TeamLightNum;
				PS->TeamID = 1;
				TeamLightNum++;
			}
		}
		else { Logout(NewPlayer); }

		// Check if we've filled the lobby, start timer if so
		if (NumberOfPlayers >= LobbyCapacity)
		{
			ToggleMatchWaiting(false);
			GetWorldTimerManager().SetTimer(TimeRemainingHandle, this, &AMGLobbyGameMode::CheckTimeRemaining, 1.0f, true);
		}
	}
	else { Logout(NewPlayer); }
}


// PLAYER LOGOUT
void AMGLobbyGameMode::Logout(AController* Exiting)
{
	// Decrement number of players
	--NumberOfPlayers;

	// Cleanup Player
	AMGLobbyController* ExitingPC = Cast<AMGLobbyController>(Exiting);
	if (ExitingPC)
	{
		PCArray.Remove(ExitingPC);
		ExitingPC->CLIENT_CleanupWidgets();

		// Decrement appropriate team number
		AMGPlayerState* PS = Cast<AMGPlayerState>(ExitingPC->PlayerState);
		if (PS)
		{
			if (PS->TeamID == 1) { TeamLightNum--; }
			else				 { TeamDarkNum--; }
		}

		// Decrement appropriate map votes
		if (ExitingPC->MapVoteID > 0)
		{
			if (ExitingPC->MapVoteID == 1)		{ DecreaseMap1Votes(); }
			else if (ExitingPC->MapVoteID == 2) { DecreaseMap2Votes(); }
			else if (ExitingPC->MapVoteID == 3) { DecreaseMap3Votes(); }
		}
	}

	// If we were at capacity before, restart timer and open map voting back up
	if (NumberOfPlayers < LobbyCapacity) 
	{ 
		GetWorldTimerManager().ClearTimer(TimeRemainingHandle); 

		TimeToStartMatch = MaxTimeToStartMatch;
		UpdatePlayersInLobby();
		ToggleMatchWaiting(true);
		OpenMapVoting();
	}

	// Log out
	UE_LOG(LogTemp, Error, TEXT("Player %s Logged Out"), *Exiting->GetName());
	Super::Logout(Exiting);
}



////////////
// TIMERS //
////////////

// START LOBBY 
void AMGLobbyGameMode::StartLobbyAfterSeamlessTravel()
{
	UpdatePlayersInLobby();
	StartMatch();
}


// SERVER TRAVEL
void AMGLobbyGameMode::CheckTimeRemaining()
{
	// If time is still remaining...
	if (TimeToStartMatch > 0)
	{
		--TimeToStartMatch;

		// Update Player HUDs with new time remaining
		for (AMGLobbyController* PC : PCArray)
		{
			if (PC)
			{
				PC->CLIENT_UpdateMatchStarting(TimeToStartMatch);

				// Toggle the "Match Status Starting Box" in widget (only want to do this once, after the timer text has been set)
				if (TimeToStartMatch >= MaxTimeToStartMatch-1) { ToggleMatchWaiting(false); }

				// Close voting when timer gets low
				if (TimeToStartMatch == CloseVotingTime)
				{
					bVotingClosed = true;
					PC->CLIENT_CloseVoting();
				}
			}
		}
	}

	// Start match when timer reaches 0
	else
	{
		GetWorldTimerManager().ClearTimer(TimeRemainingHandle);

		CleanupWidgets();
		TravelToMatchMap(SetMatchMap());
	}
}

void AMGLobbyGameMode::TravelToMatchMap(const FString& MapName)
{
	UWorld* World = GetWorld();
	if (World)
	{
		// Start session
		FString MapNameNoSpace = UKismetStringLibrary::Replace(MapName, " ", "", ESearchCase::IgnoreCase);
		UMGGameInstance* GI = Cast<UMGGameInstance>(GetGameInstance());
		if (GI) 
		{ 
			GI->CurrentMapName = MapName;
			GI->CurrentGameMode = "Dev Mode"; // @DEBUG
			GI->StartSession(); 
		}

		// Travel to match map
		World->ServerTravel("/Game/Maps/" + MapNameNoSpace + "/" + MapNameNoSpace + "-TDM");
		ShowLoadingScreen(MapNameNoSpace);
	}
}



/////////////////
// HUD UPDATES //
/////////////////

// MATCH STATUS
void AMGLobbyGameMode::UpdatePlayersInLobby()
{
	for (AMGLobbyController* PC : PCArray)
	{
		if (PC) { PC->CLIENT_UpdateMatchWaiting(NumberOfPlayers, LobbyCapacity); }
	}
}

void AMGLobbyGameMode::UpdatePlayerAvatars(UTexture2D* PlayerAvatar, uint8 PlayerTeamID, uint8 PlayerMPID, AMGLobbyController* SelfRef)
{
	for (AMGLobbyController* PC : PCArray)
	{
		if (PC) 
		{ 
			// Update all currently connected players with calling client's data
			PC->CLIENT_UpdatePlayerAvatars(PlayerAvatar, PlayerTeamID, PlayerMPID);

			// Run on calling client for all players who were already in the Lobby
			if (PC != SelfRef) 
			{ 
				AMGPlayerState* PS = Cast<AMGPlayerState>(PC->PlayerState);
				if (PS) { SelfRef->CLIENT_UpdatePlayerAvatars(PC->GetSteamAvatar(), PS->TeamID, PS->MP_UUID); }
			}
		}
	}
}

void AMGLobbyGameMode::ToggleMatchWaiting(bool bIsWaiting)
{
	for (AMGLobbyController* PC : PCArray)
	{
		if (PC)
		{
			PC->CLIENT_UpdateMatchStatus(bIsWaiting);
		}
	}
}


// MAP VOTES
// @TODO - Consolidate into a single function
void AMGLobbyGameMode::IncreaseMap1Votes()
{
	if (!bVotingClosed)
	{
		Map1Votes++;
		for (AMGLobbyController* PC : PCArray)
		{
			if (PC)
			{
				PC->CLIENT_IncreaseMap1Votes();
			}
		}
	}
}

void AMGLobbyGameMode::DecreaseMap1Votes()
{
	if (!bVotingClosed)
	{
		Map1Votes--;
		for (AMGLobbyController* PC : PCArray)
		{
			if (PC)
			{
				PC->CLIENT_DecreaseMap1Votes();
			}
		}
	}
}

void AMGLobbyGameMode::IncreaseMap2Votes()
{
	if (!bVotingClosed)
	{
		Map2Votes++;
		for (AMGLobbyController* PC : PCArray)
		{
			if (PC)
			{
				PC->CLIENT_IncreaseMap2Votes();
			}
		}
	}
}

void AMGLobbyGameMode::DecreaseMap2Votes()
{
	if (!bVotingClosed)
	{
		Map2Votes--;
		for (AMGLobbyController* PC : PCArray)
		{
			if (PC)
			{
				PC->CLIENT_DecreaseMap2Votes();
			}
		}
	}
}

void AMGLobbyGameMode::IncreaseMap3Votes()
{
	if (!bVotingClosed)
	{
		Map3Votes++;
		for (AMGLobbyController* PC : PCArray)
		{
			if (PC)
			{
				PC->CLIENT_IncreaseMap3Votes();
			}
		}
	}
}

void AMGLobbyGameMode::DecreaseMap3Votes()
{
	if (!bVotingClosed)
	{
		Map3Votes--;
		for (AMGLobbyController* PC : PCArray)
		{
			if (PC)
			{
				PC->CLIENT_DecreaseMap3Votes();
			}
		}
	}
}

void AMGLobbyGameMode::OpenMapVoting()
{
	bVotingClosed = false;
	for (AMGLobbyController* PC : PCArray)
	{
		if (PC)
		{
			PC->CLIENT_OpenVoting();
		}
	}
}


// SET MATCH MAP (Sets MapName based on votes)
FString AMGLobbyGameMode::SetMatchMap()
{
	// Set map names from a valid PC
	for (AMGLobbyController* PC : PCArray)
	{
		if (PC)
		{
			Map1Name = PC->MapName1;
			Map2Name = PC->MapName2;
			Map3Name = PC->MapName3;
		}
		break;
	}

	// Determine which map to play on based on votes
	if (Map1Votes > Map2Votes && Map1Votes > Map3Votes)
	{
		return Map1Name;
	}
	else if (Map2Votes > Map1Votes && Map2Votes > Map3Votes)
	{
		return Map2Name;
	}
	else if (Map3Votes > Map1Votes && Map3Votes > Map2Votes)
	{
		return Map3Name;
	}

	// If we get here, at least two maps are tied for first
	else if (Map1Votes == Map2Votes)
	{
		TArray<FString> MapNameArray = { Map1Name, Map2Name, Map3Name };
		if (Map1Votes == Map3Votes)
		{
			return MapNameArray[FMath::RandRange(0, 2)];
		}
		else
		{
			return MapNameArray[FMath::RandRange(0, 1)];
		}
	}
	else if (Map1Votes == Map3Votes)
	{
		TArray<FString> MapNameArray = { Map1Name, Map3Name};
		return MapNameArray[FMath::RandRange(0, 1)];
	}
	else if (Map2Votes == Map3Votes)
	{
		TArray<FString> MapNameArray = { Map2Name, Map3Name };
		return MapNameArray[FMath::RandRange(0, 1)];
	}

	UE_LOG(LogTemp, Error, TEXT("Something went wrong with SetMatchMap function in LobbyGameMode..."));
	return Map1Name;
}



/////////////
// CLEANUP //
/////////////

// Join in Progress
void AMGLobbyGameMode::JoinInProgressCheck()
{
	for (AMGLobbyController* PC : PCArray)
	{
		if (PC && PC->GetCharacter())
		{
			AMGLobbyPawn* LobbyPawn = Cast<AMGLobbyPawn>(PC->GetCharacter());
			if (LobbyPawn)
			{
				// Runs an alternate FiringMode toggle on Pawn that sets material to Magic or Default
				LobbyPawn->ToggleMagicFiringModeJIP();
				LobbyPawn->SRV_InitFocusWidget();
			}	
		}
	}
}


// Show Loading Screen
void AMGLobbyGameMode::ShowLoadingScreen(const FString& MapName)
{
	for (AMGLobbyController* PC : PCArray)
	{
		if (PC)
		{
			PC->CLIENT_ShowLoadingScreen(MapName);
		}
	}
}


// Cleanup Widgets
void AMGLobbyGameMode::CleanupWidgets()
{
	for (AMGLobbyController* PC : PCArray)
	{
		if (PC)
		{
			PC->CLIENT_CleanupWidgets();
		}
	}
}



////////////////
// MANAGEMENT //
////////////////

// Generate Map Indices
void AMGLobbyGameMode::GenerateRandomMapIndices()
{
	for (int i = 0; i < TotalMapsInList; i++)
	{
		MapIndexArray.Add(i);
	}
	uint8 IndexToRemove;

	IndexToRemove = FMath::RandRange(0, MapIndexArray.Num() - 1);
	MapIndex1 = MapIndexArray[IndexToRemove];
	MapIndexArray.RemoveAt(IndexToRemove);

	IndexToRemove = FMath::RandRange(0, MapIndexArray.Num() - 1);
	MapIndex2 = MapIndexArray[IndexToRemove];
	MapIndexArray.RemoveAt(IndexToRemove);

	MapIndex3 = MapIndexArray[FMath::RandRange(0, MapIndexArray.Num() - 1)];
}


// SERVER ENDS LOBBY
void AMGLobbyGameMode::HandleLeavingMap()
{
	UMGGameInstance* GI = Cast<UMGGameInstance>(GetGameInstance());
	if (GI)
	{
		GI->DestroyExistingSession();
	}
}
