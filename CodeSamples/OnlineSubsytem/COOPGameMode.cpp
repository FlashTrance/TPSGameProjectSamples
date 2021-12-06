// A large smattering of code from a "Horde-like" PvE framework I'm working on.
// ------------------------------------------------------------------

#include "MGCoopGameMode.h"
#include "MGGameInstance.h"
#include "MGCoopGameState.h"
#include "MGPlayerStateCoop.h"
#include "MGPlayerStart.h"
#include "MGCoopController.h"

#include "AI_Base.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "Engine/World.h"

AMGCoopGameMode::AMGCoopGameMode()
{
	// Set default values
	GameStateClass = AMGCoopGameState::StaticClass();
	PlayerStateClass = AMGPlayerStateCoop::StaticClass();
	bUseSeamlessTravel = true;
	bDelayedStart = 1;

	WavePreparationTime = 30;
	TimeBeforeTimerStarts = 2;
	TimeBeforeWaveStarts = 4;
	TimeAfterWaveEnds = 6;

	MaxNumPlayers = 4;
	BaseWavePoints = 30;
	MaxAIPointValue = 2;
	MaxAIToSpawnAtOnce = 5;
	MaxNumAIOnField = 7;
}


// POST LOGIN (DEBUG ONLY!!!)
void AMGCoopGameMode::PostLogin(APlayerController* NewPlayer)
{
	if (NewPlayer)
	{
		Super::PostLogin(NewPlayer);

		// Track # of players
		NumberOfPlayers++;

		AMGCoopController* PC = Cast<AMGCoopController>(NewPlayer);
		if (PC)
		{
			// Add player to array
			PCArray.Add(PC);

			// Send HTTP Request for Server Data
			PC->GetLoadoutData();
			PC->GetPlayerStatsData();
		}
		else { return; }

		// Initialize PlayerState vars
		AMGPlayerStateCoop* PS = Cast<AMGPlayerStateCoop>(NewPlayer->PlayerState);
		if (PS)
		{
			PS->Kills = 0;
			PS->Deaths = 0;
			PS->Assists = 0;
			PS->PlayerScore = 0;
			PS->TeamID = 1;
			PS->MP_UUID = NumberOfPlayers - 1; // This is used primarily to keep track of indexes on the Scoreboard
		}
		else { return; }

		// Wait until conditions met to start game
		GetWorldTimerManager().SetTimer(StartMatchHandle, this, &AMGCoopGameMode::StartMatchAfterTimer, 1.0f, true);
	}
}

// PLAYER LOGIN (This runs after a player has seamlessly traveled)
void AMGCoopGameMode::HandleSeamlessTravelPlayer(AController*& TravellingPC)
{
	Super::HandleSeamlessTravelPlayer(TravellingPC);

	if (TravellingPC)
	{
		// Increment number of players
		NumberOfPlayers++;

		// Init Loadout Data from PlayerState
		AMGCoopController* PC = Cast<AMGCoopController>(TravellingPC);
		if (PC)
		{
			PCArray.Add(PC);
			PC->InitLoadoutFromPlayerState();
		}
	}
}


// POST SEAMLESS TRAVEL (Seamless travel has finished)
void AMGCoopGameMode::PostSeamlessTravel()
{
	Super::PostSeamlessTravel();

	GetWorldTimerManager().SetTimer(StartMatchHandle, this, &AMGCoopGameMode::StartMatchAfterTimer, 3.0f, true);
}


// PLAYER LOGOUT
void AMGCoopGameMode::Logout(AController* Exiting)
{
	// Decrement player count
	NumberOfPlayers--;

	// Cleanup Player
	AMGCoopController* ExitingPC = Cast<AMGCoopController>(Exiting);
	if (ExitingPC)
	{
		PCArray.Remove(ExitingPC);
		ExitingPC->CLIENT_CleanupWidgets();
	}

	// Log out
	UE_LOG(LogTemp, Error, TEXT("Player %s Logged Out"), *Exiting->GetName());
	Super::Logout(Exiting);
}



///////////
// WAVES //
///////////

void AMGCoopGameMode::StartNextWave()
{
	// Run EQS query to get spawn locations and pass this into SpawnNewMob()
	GetSpawnLocation(); 
}
void AMGCoopGameMode::SetupSpawnNewMob(FTransform SpawnTransform)
{
	// Spawn AI Characters on a Timer to prevent hitching
	FTimerDelegate SpawnDelegate = FTimerDelegate::CreateLambda([=]() {this->SpawnNewMob(SpawnTransform); });
	GetWorld()->GetTimerManager().SetTimer(SpawnAIHandle, SpawnDelegate, 2.0f, true, 0.0f);
}
void AMGCoopGameMode::SpawnNewMob(FTransform SpawnTransform)
{
	for (int i = 0; i < MaxAIToSpawnAtOnce; i++)
	{
		if (TotalNumAIOnField >= MaxNumAIOnField) { break; }

		// Spawn AI using queue value as index
		if (AISpawnArray.Num() > 0)
		{
			uint8 SpawnIndex = AISpawnArray[0];
			AISpawnArray.RemoveAt(0);
			AAI_Base* SpawnedAI = GetWorld()->SpawnActor<AAI_Base>(*PossibleAIToSpawn[SpawnIndex], SpawnTransform.GetLocation(), SpawnTransform.GetRotation().Rotator(), AISPawnParams);
			TotalNumAIOnField++;
		}

		// Check to see if spawn array is empty, and stop the spawn timer if so
		else
		{
			GetWorldTimerManager().ClearTimer(SpawnAIHandle);
			break;
		}
	}
}


void AMGCoopGameMode::EndWave()
{
	if (GS)
	{
		for (AMGCoopController* PC : PCArray)
		{
			if (PC) { PC->CLIENT_EndWave(GS->WaveNum); }
		}

		FTimerHandle WaveCompleteTimerHandle;
		GetWorldTimerManager().SetTimer(WaveCompleteTimerHandle, this, &AMGCoopGameMode::PrepareForNextWave, TimeAfterWaveEnds, false);
	}
}


void AMGCoopGameMode::PrepareForNextWave()
{
	GS = Cast<AMGCoopGameState>(GameState);
	if (GS)
	{
		// Setup current Wave variables
		GS->WaveNum++;
		TotalWavePoints = BaseWavePoints + GS->WaveNum;
		PossibleAIToSpawn.Empty();
		PossibleAIToSpawnVals.Empty();

		// Create an array of possible AI to spawn
		for (auto& Elem : AISelectionMap)
		{
			if (Elem.Value <= MaxAIPointValue) { PossibleAIToSpawn.Add(Elem.Key); PossibleAIToSpawnVals.Add(Elem.Value); }
			else if (Elem.Value > MaxAIPointValue) { break; }
		}

		// Create array of spawns (there's a little slack on the last element, since it could make TotalWavePoints negative)
		uint8 IndexToAdd = 0;
		uint8 EnemySpawns = 0;
		if (PossibleAIToSpawnVals.Num() > 0)
		{
			while (TotalWavePoints > 0)
			{
				IndexToAdd = FMath::RandRange(0, PossibleAIToSpawnVals.Num() - 1);
				AISpawnArray.Add(IndexToAdd);

				EnemySpawns++;
				TotalWavePoints -= PossibleAIToSpawnVals[IndexToAdd];
			}
		}
		else { TotalWavePoints = 0; }

		// Update Client HUDs
		GS->EnemySpawns = EnemySpawns;
		for (AMGCoopController* PC : PCArray)
		{
			if (PC) { PC->CLIENT_PrepareForNextWave(GS->WaveNum, EnemySpawns); }
		}

		// Set timer before continuing to allow client HUD animations to finish
		FTimerHandle StartWaveTimerHandle;
		GetWorldTimerManager().SetTimer(StartWaveTimerHandle, this, &AMGCoopGameMode::StartWaveTimer, TimeBeforeTimerStarts, false);
	}
}


void AMGCoopGameMode::StartWaveTimer()
{
	GetWorldTimerManager().SetTimer(WaveTimerHandle, this, &AMGCoopGameMode::WaveTimerCallback, 1.0f, true);
}
void AMGCoopGameMode::WaveTimerCallback()
{
	// Decrement wave timer 
	if (GS->TimeToNextWave > 0)
	{
		for (AMGCoopController* PC : PCArray)
		{
			if (PC) { PC->CLIENT_DecrementWaveTimer(GS->TimeToNextWave); }
		}
		GS->TimeToNextWave--;
	}

	// Start the next wave (StartNextWave is called from CoopController)
	else
	{
		// Show the final "0" on timer
		for (AMGCoopController* PC : PCArray)
		{
			if (PC) { PC->CLIENT_DecrementWaveTimer(GS->TimeToNextWave); }
		}

		// Reset variables
		GetWorldTimerManager().ClearTimer(WaveTimerHandle);
		GS->TimeToNextWave = WavePreparationTime;

		// Update client HUDs and call StartNextWave
		for (AMGCoopController* PC : PCArray)
		{
			if (PC) { PC->CLIENT_WaveStart(); }
		}

		// Set timer before starting wave to allow client HUD animations to finish
		FTimerHandle StartNextWaveHandle;
		GetWorldTimerManager().SetTimer(StartNextWaveHandle, this, &AMGCoopGameMode::StartNextWave, TimeBeforeWaveStarts, false);
	}
}


void AMGCoopGameMode::EnemyKilled()
{
	if (GS) 
	{ 
		TotalNumAIOnField--;

		// Update Client HUDs
		for (AMGCoopController* PC : PCArray)
		{
			if (PC) { PC->CLIENT_EnemyKilled(); }
		}

		// Check if wave is complete
		if (GS->EnemySpawns <= 0) { EndWave(); }
	}
}



////////////////
// MATCH FLOW //
////////////////

// START MATCH
void AMGCoopGameMode::StartMatchAfterTimer()
{
	if (StartMatchCondition())
	{
		GetWorldTimerManager().ClearTimer(StartMatchHandle);

		GS = Cast<AMGCoopGameState>(GameState);
		if (GS)
		{
			for (AMGCoopController* PC : PCArray)
			{
				if (PC)
				{
					GI = Cast<UMGGameInstance>(GetGameInstance());
					if (GI)
					{
						// Initialize HUD Vars
						PC->CLIENT_Init(WavePreparationTime);
						PC->CLIENT_UpdateMainScoreboard("Prototype Map", "CO-OP"); // @DEBUG

						// Setup GameState PlayerPayoutMap
						GS->PlayerPayoutMap.Add(PC, 0);

						// Init scoreboard info for THIS PC using data from PlayerState for ALL PCs
						for (AMGCoopController* PC_Next : PCArray) 
						{
							if (PC_Next)
							{
								// Retrieve info for scoreboard
								AMGPlayerStateCoop* PS = Cast<AMGPlayerStateCoop>(PC_Next->PlayerState);
								if (PS)
								{
									// Parse out rank information
									TArray<FString> ParsedRank;
									uint8 RankColor = 0, RankNumber = 0;

									PS->PlayerRanking.ParseIntoArray(ParsedRank, TEXT("-"), true);
									if (ParsedRank.Num() == 2)
									{
										RankColor = FCString::Atoi(*ParsedRank[0]);
										RankNumber = FCString::Atoi(*ParsedRank[1]);
									}

									// Initialize scoreboard entry for this player
									PC->CLIENT_InitScoreboardEntries(PS->TeamID, RankColor, RankNumber, PS->GetPlayerName());
								}
							}
						}
					}
				}
			}
		}
		
		StartMatch(); // Calls HandleMatchIsWaitingToStart() in GameState
		PrepareForNextWave();
	}
}


// START CONDITION
bool AMGCoopGameMode::StartMatchCondition()
{
	// Check all players connected
	if (NumberOfPlayers >= MaxNumPlayers && PCArray.Num() >= MaxNumPlayers)
	{
		// Check all players ready
		for (AMGCoopController* PC : PCArray)
		{
			if (!PC || !PC->bPlayerReady)
			{
				return false;
			}
		}
		return true;
	}
	return false;
}


// RETURN TO LOBBY (Match End)
void AMGCoopGameMode::ReturnToLobby()
{
	UWorld* World = GetWorld();
	if (World)
	{
		CleanupWidgets();

		// End session
		if (!GI) { GI = Cast<UMGGameInstance>(GetGameInstance()); }
		if (GI) { GI->EndSession(); }

		// Travel to match map
		World->ServerTravel("/Game/Maps/LobbyMapCoop"); // @DEBUG
		ShowLoadingScreen("/Game/Maps/LobbyMapCoop");   // @DEBUG
	}
}



/////////////
// CLEANUP //
/////////////

// Show Loading Screen
void AMGCoopGameMode::ShowLoadingScreen(const FString& MapName)
{
	for (AMGCoopController* PC : PCArray)
	{
		if (PC)
		{
			PC->CLIENT_ShowLoadingScreen(MapName);
		}
	}
}

// Cleanup Widgets
void AMGCoopGameMode::CleanupWidgets()
{
	for (AMGCoopController* PC : PCArray)
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

// ASSIGN PLAYER START
AActor* AMGCoopGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	// Variables
	AMGPlayerStart* FoundPlayerStart = nullptr;
	UClass* PawnClass = GetDefaultPawnClassForController(Player);
	APawn* PawnToFit = PawnClass ? PawnClass->GetDefaultObject<APawn>() : nullptr;
	TArray<AMGPlayerStart*> UnOccupiedStartPoints;
	TArray<AMGPlayerStart*> OccupiedStartPoints;
	UWorld* World = GetWorld();

	// Find Player Start
	for (TActorIterator<AMGPlayerStart> It(World); It; ++It)
	{
		AMGPlayerStart* PlayerStart = *It;

		if (PlayerStart)
		{
			FVector ActorLocation = PlayerStart->GetActorLocation();
			const FRotator ActorRotation = PlayerStart->GetActorRotation();

			if (!World->EncroachingBlockingGeometry(PawnToFit, ActorLocation, ActorRotation))
			{
				UnOccupiedStartPoints.Add(PlayerStart);
			}
			else if (World->FindTeleportSpot(PawnToFit, ActorLocation, ActorRotation))
			{
				OccupiedStartPoints.Add(PlayerStart);
			}
		}
	}

	// Assign Player Start
	if (UnOccupiedStartPoints.Num() > 0)
	{
		FoundPlayerStart = UnOccupiedStartPoints[FMath::RandRange(0, UnOccupiedStartPoints.Num() - 1)];
	}
	else if (OccupiedStartPoints.Num() > 0)
	{
		FoundPlayerStart = OccupiedStartPoints[FMath::RandRange(0, OccupiedStartPoints.Num() - 1)];
	}

	if (FoundPlayerStart) { return FoundPlayerStart; }
	return NULL;
}
