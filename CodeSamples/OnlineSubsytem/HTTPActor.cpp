// Utilizing Unreal's HTTP module to setup a framework for loading and storing information on a remote server (I use CouchDB, but this would
// work with any server expecting JSON input)
// --------------------------------------------------------------

#include "MGHttpService.h"
#include "Runtime/Online/HTTP/Public/Http.h"
#include "Json.h"
#include "JsonUtilities.h"
#include "MGJsonStructs.h"
#include "MGMenuController.h"
#include "MGPlayerController.h"

#include "Engine/GameEngine.h"

// Constructor
AMGHttpService::AMGHttpService()
{
	// Tick()
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;
}


// BEGIN PLAY()
void AMGHttpService::BeginPlay()
{
	Super::BeginPlay();
	
	// Set reference to HttpModule
	Http = &FHttpModule::Get();
}


// SETUP REQUEST
FHttpRequestRef AMGHttpService::CreateHttpRequest(FString URLEnding)
{
	// Create Request
	FHttpRequestRef Request = Http->CreateRequest();
	Request->SetURL(ServerBaseURL + URLEnding);
	SetRequestHeaders(Request);

	return Request;
}

// GET
FHttpRequestRef AMGHttpService::GetRequest(FString URLEnding)
{
	FHttpRequestRef Request = CreateHttpRequest(URLEnding);
	Request->SetVerb("GET");

	return Request;
}

// PUT
FHttpRequestRef AMGHttpService::PutRequest(FString URLEnding, FString ContentJsonString)
{
	FHttpRequestRef Request = CreateHttpRequest(URLEnding);
	Request->SetVerb("PUT");
	Request->SetContentAsString(ContentJsonString);

	return Request;
}

// POST
FHttpRequestRef AMGHttpService::PostRequest(FString URLEnding, FString ContentJsonString)
{
	FHttpRequestRef Request = CreateHttpRequest(URLEnding);
	Request->SetVerb("POST");
	Request->SetContentAsString(ContentJsonString);

	return Request;
}

// SEND - Sends the request returned by GET or POST for processing
void AMGHttpService::Send(FHttpRequestRef& Request)
{
	Request->ProcessRequest();
}


// SET HEADERS
void AMGHttpService::SetRequestHeaders(FHttpRequestRef& Request)
{
	Request->SetHeader(TEXT("User-Agent"), TEXT("X-UnrealEngine-Agent"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetHeader(TEXT("Accepts"), TEXT("application/json"));
}


// CHECK RESPONSE
bool AMGHttpService::IsValidResponse(FHttpResponsePtr Response, bool bWasSuccessful)
{
	if (bWasSuccessful && Response.IsValid())
	{
		if (EHttpResponseCodes::IsOk(Response->GetResponseCode()))
		{
			return true;
		}
		else 
		{
			UE_LOG(LogTemp, Warning, TEXT("Response ERROR Code: %d"), Response->GetResponseCode());
			return false;
		}
	}
	return false;
}


// STRUCT -> JSON
template <typename StructType>
void AMGHttpService::GetJsonStringFromStruct(StructType FilledStruct, FString& StringOut)
{
	FJsonObjectConverter::UStructToJsonObjectString(StructType::StaticStruct(), &FilledStruct, StringOut, 0, 0);
}



// GET LOADOUT INFO
void AMGHttpService::GetLoadoutInfoFromServer(FString PlayerID)
{
	// Make sure HTTP object is valid
	if (!Http) { Http = &FHttpModule::Get(); }

	// Create and send request
	FHttpRequestRef Request = GetRequest("loadout_options/" + PlayerID);
	Request->OnProcessRequestComplete().BindUObject(this, &AMGHttpService::GetLoadoutInfoFromServerResponse);
	Send(Request);
}
void AMGHttpService::GetLoadoutInfoFromServerResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	FLoadoutOptions LoadoutStruct;
	if (IsValidResponse(Response, bWasSuccessful))
	{
		// Try to convert JSON response to USTRUCT
		FString JsonString = Response->GetContentAsString();
		if (FJsonObjectConverter::JsonObjectStringToUStruct<FLoadoutOptions>(JsonString, &LoadoutStruct, 0, 0)) {}
		else
		{
			Request->CancelRequest();
		}
	}
	else { GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, FString::Printf(TEXT("BAD RESPONSE FROM REST API. Default classes will be used.")), true); }

	// Callback on Player Controller
	if (GetOwner())
	{
		AMGMenuController* PC = Cast<AMGMenuController>(GetOwner());
		AMGPlayerController* PCDebug = Cast<AMGPlayerController>(GetOwner()); // For development use only
		if (PC)
		{
			PC->GetLoadoutDataCallback(LoadoutStruct);
		}
		else if (PCDebug)
		{
			PCDebug->GetLoadoutDataCallback(LoadoutStruct);
		}
	}
}


// PUT LOADOUT INFO
void AMGHttpService::PutLoadoutInfoToServer(FString PlayerID, FLoadoutOptions LoadoutStruct)
{
	FString PutRequestJSON;
	GetJsonStringFromStruct<FLoadoutOptions>(LoadoutStruct, PutRequestJSON);

	FHttpRequestRef Request = PutRequest("loadout_options/" + PlayerID, PutRequestJSON);
	Request->OnProcessRequestComplete().BindUObject(this, &AMGHttpService::PutLoadoutInfoToServerResponse);
	Send(Request);
}
void AMGHttpService::PutLoadoutInfoToServerResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	if (IsValidResponse(Response, bWasSuccessful))
	{
		// Convert JSON response to USTRUCT
		FLoadoutOptions LoadoutStruct;
		FString PutResponse = Response->GetContentAsString();
		FJsonObjectConverter::JsonObjectStringToUStruct<FLoadoutOptions>(PutResponse, &LoadoutStruct, 0, 0);

		UE_LOG(LogTemp, Warning, TEXT("PUT response: %s"), *PutResponse);

		// Callback on Player Controller
		if (GetOwner())
		{
			AMGMenuController* PC = Cast<AMGMenuController>(GetOwner());
			if (PC)
			{
				PC->UpdateLoadoutDataCallback(LoadoutStruct);
			}
		}
	}
}


// POST LOADOUT INFO
void AMGHttpService::PostLoadoutInfoToServer(FString PlayerID, FLoadoutOptions LoadoutStruct)
{
	FString PostRequestJSON;
	GetJsonStringFromStruct<FLoadoutOptions>(LoadoutStruct, PostRequestJSON);

	FHttpRequestRef Request = PostRequest("loadout_options/" + PlayerID, PostRequestJSON);
	Request->OnProcessRequestComplete().BindUObject(this, &AMGHttpService::PostLoadoutInfoToServerResponse);
	Send(Request);
}
void AMGHttpService::PostLoadoutInfoToServerResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	if (IsValidResponse(Response, bWasSuccessful))
	{
		// Convert JSON response to USTRUCT
		FLoadoutOptions LoadoutStruct;
		FString PostResponse = Response->GetContentAsString();
		FJsonObjectConverter::JsonObjectStringToUStruct<FLoadoutOptions>(PostResponse, &LoadoutStruct, 0, 0);

		UE_LOG(LogTemp, Warning, TEXT("POST response: %s"), *PostResponse);
	}
}


// GET PLAYERSTATS INFO
void AMGHttpService::GetPlayerStatsInfoFromServer(FString PlayerID)
{
	// Make sure HTTP object is valid
	if (!Http) { Http = &FHttpModule::Get(); }

	// Create and send request
	FHttpRequestRef Request = GetRequest("player_stats/" + PlayerID);
	Request->OnProcessRequestComplete().BindUObject(this, &AMGHttpService::GetPlayerStatsInfoFromServerResponse);
	Send(Request);
}
void AMGHttpService::GetPlayerStatsInfoFromServerResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	FPlayerStats StatsStruct;
	if (IsValidResponse(Response, bWasSuccessful))
	{
		// Try to convert JSON response to USTRUCT
		FString JsonString = Response->GetContentAsString();
		if (FJsonObjectConverter::JsonObjectStringToUStruct<FPlayerStats>(JsonString, &StatsStruct, 0, 0)) {}
		else
		{
			Request->CancelRequest();
		}
	}
	else { GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, FString::Printf(TEXT("BAD RESPONSE FROM REST API. Default classes will be used.")), true); }

	// Callback on Player Controller
	if (GetOwner())
	{
		AMGMenuController* PC = Cast<AMGMenuController>(GetOwner());
		AMGPlayerController* PCDebug = Cast<AMGPlayerController>(GetOwner()); // For development use only
		if (PC)
		{
			PC->GetPlayerStatsDataCallback(StatsStruct);
		}
		else if (PCDebug)
		{
			PCDebug->GetPlayerStatsDataCallback(StatsStruct);
		}
	}
}
