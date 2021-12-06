// A smattering of code from my GameInstance class that shows some interaction with the online subsystem and error handling.
// This is very WIP since I've only really started focusing on this part of development recently, but it does all work as expected
// ----------------------------------------------------------------------

// (Editor/Game Starts)
UMGGameInstance::UMGGameInstance(const FObjectInitializer& ObjectInitializer){}

// INIT (Game Starts)
void UMGGameInstance::Init()
{
	Super::Init();

	// Reference Online Subsystem - OSys can be used to access various interfaces containing useful functions (see docs on IOnlineSubsystem)
	OSys = IOnlineSubsystem::Get();
	if (OSys)
	{
		UE_LOG(LogTemp, Warning, TEXT("GAMEINSTANCE: Subsystem Found: %s"), *OSys->GetSubsystemName().ToString());

		// Get Session Interface and bind delegates
		SessionInterface = OSys->GetSessionInterface();
		if (SessionInterface.IsValid())
		{
			UE_LOG(LogTemp, Warning, TEXT("GAMEINSTANCE: Session Interface found"));

			SessionInterface->OnCreateSessionCompleteDelegates.AddUObject(this, &UMGGameInstance::OnCreateSessionComplete);
			SessionInterface->OnDestroySessionCompleteDelegates.AddUObject(this, &UMGGameInstance::OnDestroySessionComplete);
			SessionInterface->OnFindSessionsCompleteDelegates.AddUObject(this, &UMGGameInstance::OnFindSessionsComplete);
			SessionInterface->OnJoinSessionCompleteDelegates.AddUObject(this, &UMGGameInstance::OnJoinSessionComplete);
		}

		/* RETRIEVING FRIENDS LIST, PLATFORM AGNOSTIC /*
		
		FString FriendString = EFriendsLists::ToString(EFriendsLists::OnlinePlayers);
		FriendInterface = OSys->GetFriendsInterface();
		if (FriendInterface.IsValid()) { FriendInterface->ReadFriendsList(0, FriendString, FOnReadFriendsListComplete::CreateUObject(this, &UMGGameInstance::FinishedReadingFriendsList)); }
		*/
	}

	/// BINDINGS ///

	// FCoreUObjectDelegates is a class containing bindings which are fired during various engine events
	// Here, we use PreLoadMap and PostLoadMap and bind those to our LoadingScreen functions
	FCoreUObjectDelegates::PreLoadMap.AddUObject(this, &UMGGameInstance::BeginLoadingScreen);
	FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UMGGameInstance::EndLoadingScreen);
	

	// Handle network failures
	GetEngine()->OnNetworkFailure().AddUObject(this, &UMGGameInstance::HandleNetworkFailure);
}


// EXAMPLE - Callback Delegate after Friend's List has been read, platform agnostic
void UMGGameInstance::FinishedReadingFriendsList(int32 LocalUserNum, bool bWasSuccessful, const FString& ListName, const FString& ErrorStr)
{
	if (bWasSuccessful)
	{
		FString FriendListName;
		TArray<TSharedRef<FOnlineFriend>> FriendArray;
		FriendInterface->GetFriendsList(0, ListName, FriendArray);
		for (TSharedPtr<FOnlineFriend> MahFriend : FriendArray)
		{
			UE_LOG(LogTemp, Warning, TEXT("%s"), *MahFriend->GetDisplayName(OSys->GetSubsystemName().ToString()) );
		}
	}
}



//////////////
// SESSION  //
//////////////

// HOST
void UMGGameInstance::HostGame()
{
	// Create Session
	if (SessionInterface.IsValid())
	{
		bIsHosting = true;

		// Check for existing session and destroy it if it already exists
		FNamedOnlineSession* ExistingSession = SessionInterface->GetNamedSession(SESSION_NAME);
		if (ExistingSession)
		{
			SessionInterface->DestroySession(SESSION_NAME); // Calls "OnDestroySessionComplete" when done
		}

		// Setup new session
		else 
		{ 
			bIsHosting = false;
			SetupSession(); 
		}
	}
}


// JOIN
void UMGGameInstance::JoinGame()
{
	if (SessionInterface.IsValid())
	{
		SessionInterface->JoinSession(0, SESSION_NAME, SessionSearch->SearchResults[0]);
	}
}


// FIND
void UMGGameInstance::FindGame()
{
	// Search for joinable sessions
	if (SessionInterface.IsValid() && SessionSearch.IsValid())
	{
		SessionInterface->FindSessions(0, SessionSearch.ToSharedRef());
	}
}


// SEARCH FOR MATCH
void UMGGameInstance::SearchMatch(const FString& MatchInfo)
{
	// FOnlineSessionSearch is a TSharedPtr, needs to be converted to TSharedRef
	SessionSearch = MakeShareable(new FOnlineSessionSearch());
	if (SessionSearch.IsValid())
	{
		// Parse game type and sub type from player selection
		TArray<FString> ParsedMatchInfo;
		MatchInfo.ParseIntoArray(ParsedMatchInfo, TEXT("-"), true);

		if (ParsedMatchInfo.Num() == 2)
		{
			GameType = *ParsedMatchInfo[0];
			SubType = *ParsedMatchInfo[1];

			if (SubType == "4v4") { NumPlayersInGame = 8; }
			else if (SubType == "2v2") { NumPlayersInGame = 4; }
			else { NumPlayersInGame = 0; }
		}
		else { GEngine->AddOnScreenDebugMessage(-1, 4.0f, FColor::Red, FString::Printf(TEXT("ERROR - GameInstance - Expected 2 elements in ParsedMatchInfo"))); }

		// Setup Query Settings - This limits our session query results based on key-value pairs
		if (OSys->GetSubsystemName() == "STEAM")
		{
			SessionSearch->bIsLanQuery = false;
			SessionSearch->QuerySettings.Set(SEARCH_PRESENCE, true, EOnlineComparisonOp::Equals);    // Use Steam presence to get ALL open lobbies for this AppID
			SessionSearch->QuerySettings.Set(SEARCH_KEYWORDS, SubType, EOnlineComparisonOp::Equals); // Filter results by string
			SessionSearch->MaxSearchResults = 20000;
		}
		else if (OSys->GetSubsystemName() == "NULL")
		{
			SessionSearch->bIsLanQuery = true;
			SessionSearch->QuerySettings.Set(SEARCH_KEYWORDS, SubType, EOnlineComparisonOp::Equals);
		}

		// Update Menu
		UWorld* World = GetWorld();
		if (World)
		{
			AMGMenuController* PC = Cast<AMGMenuController>(World->GetFirstPlayerController());
			if (PC)
			{
				// Set status text based on game type
				PC->UpdateMenuStatusText(GameType, "Searching for " + SubType + " match!");
				PC->StartedSearch(GameType);
			}
		}

		// Search for joinable sessions
		UE_LOG(LogTemp, Warning, TEXT("Searching for joinable sessions..."));

		FTimerHandle FindGameHandle;
		GetWorld()->GetTimerManager().SetTimer(FindGameHandle, this, &UMGGameInstance::FindGame, StartSearchDelayTime, false);
	}
}


// CANCEL SEARCH (Called on both a manual cancel and on callback errors)
void UMGGameInstance::CancelSearch()
{
	UE_LOG(LogTemp, Warning, TEXT("CANCELLED SEARCH"))
	bCanceledSearch = false;

	UWorld* World = GetWorld();
	if (World)
	{
		AMGMenuController* PC = Cast<AMGMenuController>(World->GetFirstPlayerController());
		if (PC)
		{
			PC->CanceledSearch(GameType);
		}
	}
}


// SETUP SESSION
void UMGGameInstance::SetupSession()
{
	if (SessionInterface.IsValid())
	{
		// Setup Session Settings
		FOnlineSessionSettings SessionSettings;
		SessionSettings.NumPublicConnections = NumPlayersInGame;
		SessionSettings.bShouldAdvertise = true;
		SessionSettings.bAllowJoinInProgress = true;
		SessionSettings.Set(SEARCH_KEYWORDS, SubType, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);

		// Check for Steam and configure settings appropriately
		if (OSys->GetSubsystemName() == "NULL") 
		{ 
			UE_LOG(LogTemp, Warning, TEXT("Setup LAN match (NULL Subsystem)"));
			SessionSettings.bIsLANMatch = true; 
		}
		else if (OSys->GetSubsystemName() == "STEAM")
		{ 
			SessionSettings.bIsLANMatch = false; 
			SessionSettings.bUsesPresence = true;
		}
		
		// Create session
		SessionInterface->CreateSession(0, SESSION_NAME, SessionSettings); // Calls "OnCreateSessionComplete" when done
	}
}


////////////////
// CALLBACKS  //
////////////////

// CREATE SESSION CALLBACK
void UMGGameInstance::OnCreateSessionComplete(FName SessionName, bool Success)
{
	UWorld* World = GetWorld();

	// If session successfully created...
	if (Success && World)
	{
		APlayerController* PC = World->GetFirstPlayerController();
		if (PC)
		{
			// Remove all widgets
			UWidgetLayoutLibrary::RemoveAllWidgets(World);

			// Setup input mode
			FInputModeGameOnly InputData;

			PC->SetInputMode(InputData);
			PC->bShowMouseCursor = false;
		}

		// Server Travel
		UE_LOG(LogTemp, Warning, TEXT("GAMEINSTANCE: Successfully created %s session!"), *SessionName.ToString());
		World->ServerTravel("/Game/Maps/LobbyMap" + GameType + "?listen");
	}
}


// DESTROY SESSION CALLBACK
void UMGGameInstance::OnDestroySessionComplete(FName SessionName, bool Success)
{
	if (Success) 
	{ 
		UE_LOG(LogTemp, Error, TEXT("GAMEINSTANCE: Destroyed %s session"), *SessionName.ToString()); 
		if (bIsHosting)
		{
			bIsHosting = false;
			SetupSession();
		}
	}
	else { UE_LOG(LogTemp, Error, TEXT("GAMEINSTANCE: Failed to destroy %s session"), *SessionName.ToString()); }
}


// FIND SESSION CALLBACK
void UMGGameInstance::OnFindSessionsComplete(bool Success)
{
	if (Success && SessionSearch.IsValid())
	{
		if (!bCanceledSearch)
		{
			FOnlineSessionSearchResult ValidResult; // Valid session result variable to make sure we connect to ours over Steam

			// Iterate over search results and display
			for (const FOnlineSessionSearchResult& SearchResult : SessionSearch->SearchResults)
			{
				// Check that result is valid AND it contains our custom session setting
				// NOTE: This is all to make sure we get a valid session, b/c with Steam lots of lobbies get found and it often tries to connect 
				// to the wrong one.
				if (SearchResult.IsValid() && SearchResult.Session.SessionSettings.Get(SEARCH_KEYWORDS, SubType))
				{
					UE_LOG(LogTemp, Warning, TEXT("GAMEINSTANCE: Session search result: %s"), *SubType);
					ValidResult = SearchResult;
				}
			}

			// Get Menu PlayerController so we can send notifications to menu
			UWorld* World = GetWorld();
			if (World)
			{
				AMGMenuController* PC = Cast<AMGMenuController>(World->GetFirstPlayerController());
				if (PC)
				{
					// Join first valid session if one is available
					if (SessionInterface.IsValid() && ValidResult.IsValid())
					{
						PC->EnteringLobby(GameType);
						PC->UpdateMenuStatusText(GameType, "Joining Lobby...");
						UE_LOG(LogTemp, Warning, TEXT("GAMEINSTANCE: Attempting to join session..."));

						SessionSearch->SearchResults[0] = ValidResult;

						FTimerHandle JoinGameHandle;
						GetWorld()->GetTimerManager().SetTimer(JoinGameHandle, this, &UMGGameInstance::JoinGame, JoinSessionDelayTime, false);
					}

					// If no session is available, create a new joinable lobby
					else
					{
						PC->EnteringLobby(GameType);
						PC->UpdateMenuStatusText(GameType, "Creating Lobby...");
						UE_LOG(LogTemp, Error, TEXT("GAMEINSTANCE: No sessions found. Hosting..."));

						// Breathing room before hosting game...
						FTimerHandle HostGameHandle;
						GetWorld()->GetTimerManager().SetTimer(HostGameHandle, this, &UMGGameInstance::HostGame, CreateSessionDelayTime, false);
					}
				}
			}
		}

		// Player canceled search
		else { UE_LOG(LogTemp, Error, TEXT("GAMEINSTANCE: User cancelled the search...")); CancelSearch(); }
	}

	// Success = False 
	else { UE_LOG(LogTemp, Error, TEXT("GAMEINSTANCE: SessionSearch not valid! Cancelling...")); CancelSearch(); }
}


// JOIN SESSION CALLBACK
void UMGGameInstance::OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	if (SessionInterface.IsValid())
	{
		// Resolve connect address
		FString TravelAddress;
		if (SessionInterface->GetResolvedConnectString(SessionName, TravelAddress))
		{
			// Join the session via ClientTravel
			APlayerController* ClientPC = GetFirstLocalPlayerController();
			if (ClientPC)
			{
				// Remove all widgets
				UWorld* World = GetWorld();
				if (World) { UWidgetLayoutLibrary::RemoveAllWidgets(World); }

				// Setup input mode
				FInputModeGameOnly InputData;
				ClientPC->SetInputMode(InputData);
				ClientPC->bShowMouseCursor = false;

				// Client travel to hosted map
				UE_LOG(LogTemp, Warning, TEXT("GAMEINSTANCE: Joining session @ %s..."), *TravelAddress);
				ClientPC->ClientTravel(TravelAddress, ETravelType::TRAVEL_Absolute);
			}
		}

		// Unable to resolve travel address
		else 
		{ 
			UE_LOG(LogTemp, Error, TEXT("GAMEINSTANCE: Failed to resolve connect string!")); 
			CancelSearch();
		}
	}
}


// START SESSION (Prevents other players from joining)
void UMGGameInstance::StartSession()
{
	if (SessionInterface.IsValid())
	{
		SessionInterface->StartSession(SESSION_NAME);
	}
}

// END SESSION (Players can potentially join again)
void UMGGameInstance::EndSession()
{
	if (SessionInterface.IsValid())
	{
		SessionInterface->EndSession(SESSION_NAME);
	}
}

// DESTROY SESSOIN (Completely tears down session)
void UMGGameInstance::DestroyExistingSession()
{
	if (SessionInterface)
	{
		SessionInterface->DestroySession(SESSION_NAME);
	}
}


////////////
// OTHER  //
////////////

// RETURN TO MAIN MENU
void UMGGameInstance::ReturnToMainMenu()
{
	// Return to Main Menu via Client Travel
	APlayerController* ClientPC = GetFirstLocalPlayerController();
	if (ClientPC)
	{
		// Remove all widgets
		UWorld* World = GetWorld();
		if (World) { UWidgetLayoutLibrary::RemoveAllWidgets(World); }

		// Destroy the current session (only happens on the Client that quits)
		DestroyExistingSession();

		// Setup input mode
		FInputModeGameOnly InputData;
		ClientPC->SetInputMode(InputData);
		ClientPC->bShowMouseCursor = false;

		// Client travel to main menu
		UE_LOG(LogTemp, Warning, TEXT("Returning to Main Menu... "));
		ClientPC->ClientTravel("/Game/Maps/PrototypeMap/PrototypeMap_MainMenu", ETravelType::TRAVEL_Absolute);
	}
}


// LOADING SCREEN IMPLEMENTATION (Uses MoviePlayer Module)
void UMGGameInstance::BeginLoadingScreen(const FString& MapName)
{
	if (!IsRunningDedicatedServer())
	{
		FLoadingScreenAttributes LoadingScreen;
		LoadingScreen.WidgetLoadingScreen = FLoadingScreenAttributes::NewTestLoadingScreenWidget();
		LoadingScreen.bAutoCompleteWhenLoadingCompletes = true;
		// LoadingScreen.MinimumLoadingScreenDisplayTime = 2.0f;
		// LoadingScreen.bAllowEngineTick = true; // Causes crashes on ConnectionLost Network Error

		GetMoviePlayer()->SetupLoadingScreen(LoadingScreen);
		GetMoviePlayer()->PlayMovie();
	}
}
void UMGGameInstance::EndLoadingScreen(UWorld* InLoadedWorld) 
{

}


// HANDLE NETWORK FAILURE (Called when connection issues occur, use FailureType to handle specific errors)
void UMGGameInstance::HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString)
{
	// Create pointer to ENetworkFailure UEnum type
	const UEnum* EnumPtr = FindObject<UEnum>(ANY_PACKAGE, TEXT("ENetworkFailure"), true);
	if (EnumPtr)
	{
		// Retrieve Enum value from ENetworkFailure
		FString FailureString = EnumPtr->GetDisplayNameTextByIndex(FailureType).ToString();
		UE_LOG(LogTemp, Error, TEXT("NETWORK ERROR! VALUE: %s"), *FailureString);

		// Destroy any existing sessions left over on disconnected clients
		DestroyExistingSession();

		// Connection Timeout
		if (FailureString == "ConnectionTimeout")
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Purple, FString::Printf(TEXT("Connection timed out. Did the server shut down before we could join?")));
		}

		// Connection Lost (Disconnected from host)
		else if (FailureString == "ConnectionLost")
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Purple, FString::Printf(TEXT("Connection lost. Host disconnected or server shut down.")));
		}

		else { GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Purple, FString::Printf(TEXT("NETWORK FAILURE. Failure String: %s"), *FailureString)); }
	}	
}
