// This file is a mix of code showing how I handle UMG widgets in C++
// -----------------------------------------------------------

// Code in the Controller class initializes UMG widgets and adds them to the viewport
void AMGPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// INITIALIZE HUD
	if (IsLocalPlayerController())
	{
		// Make sure we have Game input (might have had a menu open in Lobby or something)
		FInputModeGameOnly InputData;
		SetInputMode(InputData);

		// Health Bars
		if (HealthWidgetClass)
		{
			HealthWidgetWBP = CreateWidget<UMGHealthWidget>(this, HealthWidgetClass);
			HealthWidgetWBP->AddToViewport();
		}

		// Scoreboard
		if (ScoreboardMainClass)
		{
			ScoreboardMainWBP = CreateWidget<UScoreboard_MAIN>(this, ScoreboardMainClass);
			ScoreboardMainWBP->SetVisibility(ESlateVisibility::HitTestInvisible);
			ScoreboardMainWBP->AddToViewport();
		}
    
    // ETC...

		// Loop check for player ready (make sure refs are valid for updating scoreboard and such)
		GetWorldTimerManager().SetTimer(PlayerReadyHandle, this, &AMGPlayerController::PlayerReady, 0.2f, true);
	}
}


// ----------------------------------------- //


// Widgets are all defined in C++. The header files contain variables and function definitions so I can call them on the
// back-end, but the actual logic for the widgets in handled in Blueprints (see screenshots in this folder for examples)
UCLASS()
class INNOCUOUSTITLE_API UMGHealthWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:

	// VARIABLES
	UPROPERTY(BlueprintReadOnly)
	uint8 TeamID;

	UPROPERTY(BlueprintReadOnly)
	float AmmoBarSegments;

	// FUNCTIONS
	// General
	UFUNCTION(BlueprintImplementableEvent, Category = "HUD")
	void Init();

	UFUNCTION(BlueprintImplementableEvent, Category = "HUD")
	void ShowHUD(bool Show);

	UFUNCTION(BlueprintImplementableEvent, Category = "HUD")
	void NotifyPlayer(const FName& PlayerMessage);

	UFUNCTION(BlueprintImplementableEvent, Category = "HUD")
	void ScorePopup(const FName& ScoreReason, int32 Score, int32 HealthAmt, int32 ManaAmt);

	// Weapon
	UFUNCTION(BlueprintImplementableEvent, Category = "HUD")
	void InitWeaponSegments();

	UFUNCTION(BlueprintImplementableEvent, Category = "HUD")
	void SetWeaponIcons(UTexture2D* WeaponIcon, bool Weapon1);

	UFUNCTION(BlueprintImplementableEvent, Category = "HUD")
	void UpdateAmmoClip(uint8 NewAmmoInClip);

  // ETC...
 };


// --------------------------------------------- //


// Here we can see the "ScorePopup" function in the HealthWidgetWBP class gets called in a CLIENT RPC on the PlayerController.
// This lets me call it from C++ in, for example, the GameState class and have it run on clients as needed

UFUNCTION(Client, Reliable) void CLIENT_ScorePopup(const FName& ScoreReason, int32 Score, int32 HealthAmt, int32 ManaAmt);
void AMGPlayerController::CLIENT_ScorePopup_Implementation(const FName& ScoreReason, int32 Score, int32 HealthAmt, int32 ManaAmt)
{
	if (HealthWidgetWBP) { HealthWidgetWBP->ScorePopup(ScoreReason, Score, HealthAmt, ManaAmt); }
}
