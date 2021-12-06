// A modest collection of replicated functions and other replication-related code in the game (includes code from both header and cpp files)
// ----------------------------------------------------------------------

// UPDATING HEALTH IN WIDGETS (OnRep)
UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, ReplicatedUsing="OnRep_HealthChanged", Category = "Health") float Health;
UFUNCTION() void OnRep_HealthChanged();

void AMGCharacter::OnRep_HealthChanged()
{
	if (PC && PC->HealthWidgetWBP) 
	{ 
		PC->HealthWidgetWBP->UpdateHUDHealth(); 
		SRV_UpdateFocusWidget(1);
	}
}


// ---------------------------------------------------- //


// UPDATING AMMO INFO (OnRep)
UPROPERTY(ReplicatedUsing = "OnRep_AmmoClipChanged") uint8 AmmoInClip;
UFUNCTION() void OnRep_AmmoClipChanged();

void AMGWeapon::OnRep_AmmoClipChanged()
{
	PlayerRef = Cast<APawn>(GetOwner());
	if (PlayerRef)
	{
		AMGPlayerController* PC = Cast<AMGPlayerController>(PlayerRef->Controller);
		if (PC && PC->HealthWidgetWBP)
		{
			PC->HealthWidgetWBP->UpdateAmmoClip(AmmoInClip);

			// Low ammo "Reload" text
			if (!bLowAmmo && AmmoInClip <= LowAmmoThreshold && AmmoInPocket > 0 && PC->CHWidgetWBP) 
			{ 
				PC->CHWidgetWBP->LowAmmo();
				bLowAmmo = true;
			}
		}
	}
}


// ---------------------------------------------------- //


// REPLICATING WEAPON-RELATED ANIM BLEND SPACES TO OTHER CLIENTS (Server->Multicast)
UFUNCTION(Server, Reliable, WithValidation) void SRV_SetShootAnim();
UFUNCTION(NetMulticast, Reliable)void Multicast_SetShootAnim();

void AMGCharacter::SRV_SetShootAnim_Implementation()
{
	Multicast_SetShootAnim();
}
bool AMGCharacter::SRV_SetShootAnim_Validate() { return true; }

void AMGCharacter::Multicast_SetShootAnim_Implementation()
{
	if (CurrentWeapon && AnimInstance)
	{
			// Multicast Weapon anim blend spaces
			if (CurrentWeapon == Weapon1)
			{
				AnimInstance->ShootBlendSpace = Weapon1->ShootBlendSpace;
				AnimInstance->ShootBlendSpaceLeft = Weapon1->ShootBlendSpaceLeft;
			}
			else
			{
				AnimInstance->ShootBlendSpace = Weapon2->ShootBlendSpace;
				AnimInstance->ShootBlendSpaceLeft = Weapon2->ShootBlendSpaceLeft;
			}
	}
}


// ---------------------------------------------------- //


// GENERAL ANIMATION REPLICATION
// I set replicated property values on the server in the Character class, and then update the AnimInstance class using those properties to replicate animation
void UMGAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{

	Super::NativeUpdateAnimation(DeltaSeconds);

	PlayerRef = Cast<AMGCharacter>(Player);
	if (PlayerRef) {

		// Update Values
		Speed = Player->GetVelocity().Size();
		bIsMoving = Speed > 10.0f;
		bIsSprinting = Speed > 200.0f;
		bIsDowned = PlayerRef->bIsDowned;
		Pitch = PlayerRef->PlayerPitch;
		Yaw = PlayerRef->PlayerYaw;
		MoveForwardVal = PlayerRef->MoveForwardVal;
		MoveRightVal = PlayerRef->MoveRightVal;

		bIsAim = PlayerRef->bIsAim;
		bIsEquipping = PlayerRef->bIsEquipping;
		bIsReload = PlayerRef->bIsReload;
		bPrimaryWeapon = PlayerRef->bPrimaryEquipped;
		bIsRoll = PlayerRef->bIsRoll;
		bIsInteracting = PlayerRef->bIsInteracting;
		InteractionIndex = PlayerRef->InteractionIndex;

    // Etc......
	}
}


// ---------------------------------------------------- //


// A PORTION OF GetLifetimeReplicatedProps IN CHARACTER CLASS
void AMGCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// Weapons
	DOREPLIFETIME(AMGCharacter, CurrentWeapon);
	DOREPLIFETIME(AMGCharacter, Weapon1);
	DOREPLIFETIME(AMGCharacter, Weapon2); 
	DOREPLIFETIME(AMGCharacter, bPrimaryEquipped);
	DOREPLIFETIME(AMGCharacter, bIsAim);	
	DOREPLIFETIME_CONDITION(AMGCharacter, bCanAim, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AMGCharacter, bAimTransitionComplete, COND_OwnerOnly);
	DOREPLIFETIME(AMGCharacter, bTransitioningFiringMode);
	DOREPLIFETIME(AMGCharacter, bTransitioningFiringModeJIP);
	DOREPLIFETIME_CONDITION(AMGCharacter, bCanToggleFiringMode, COND_OwnerOnly);
	DOREPLIFETIME(AMGCharacter, bMagicFiringMode);
	DOREPLIFETIME(AMGCharacter, bWeaponCharged);
	DOREPLIFETIME(AMGCharacter, bIsShoot);
	DOREPLIFETIME(AMGCharacter, bIsReload);
	DOREPLIFETIME(AMGCharacter, bIsEquipping);
	DOREPLIFETIME_CONDITION(AMGCharacter, bCanEquipReloadInteract, COND_OwnerOnly);
	DOREPLIFETIME(AMGCharacter, bDoMeleeDamage);
	DOREPLIFETIME(AMGCharacter, bLHIK);
	DOREPLIFETIME(AMGCharacter, bAimLHIK);

	// Cover
	DOREPLIFETIME(AMGCharacter, bCoverSliding);
	DOREPLIFETIME(AMGCharacter, bCanceledSlide);
	DOREPLIFETIME(AMGCharacter, bCoverSlideComplete);
	DOREPLIFETIME(AMGCharacter, bUncrouched);
	DOREPLIFETIME(AMGCharacter, bTakingCoverRight);
	DOREPLIFETIME(AMGCharacter, bInLowCover);
	DOREPLIFETIME_CONDITION(AMGCharacter, bCanAimOver, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AMGCharacter, bCanVaultCover, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AMGCharacter, bCanPerformCoverActions, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AMGCharacter, bCanPeekAim, COND_OwnerOnly);
	DOREPLIFETIME(AMGCharacter, bVaulting);
	DOREPLIFETIME(AMGCharacter, RightCoverValue);
	DOREPLIFETIME_CONDITION(AMGCharacter, bEdgeRight, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AMGCharacter, bEdgeLeft, COND_OwnerOnly);
	DOREPLIFETIME(AMGCharacter, bPeekAiming);
	DOREPLIFETIME(AMGCharacter, bMovingOutOfCover);
	DOREPLIFETIME(AMGCharacter, bSwatting);
	DOREPLIFETIME_CONDITION(AMGCharacter, bSwatEscape, COND_OwnerOnly);

	// Health and Movement
	DOREPLIFETIME(AMGCharacter, TeamID);
	DOREPLIFETIME(AMGCharacter, PlayerPitch);
	DOREPLIFETIME_CONDITION(AMGCharacter, PlayerYaw, COND_SkipOwner);
	DOREPLIFETIME(AMGCharacter, MoveForwardVal);
	DOREPLIFETIME(AMGCharacter, MoveRightVal);
	DOREPLIFETIME(AMGCharacter, Health);
	DOREPLIFETIME_CONDITION(AMGCharacter, MaxHealth, COND_OwnerOnly);

  // Etc....
}


// ---------------------------------------------------- //


// In this game, players are given score, health, and mana based on how much damage they did to an opponent before the opponent was eliminated. 
// This is tracked for each player who damages the opponent in a Map stored on the PlayerState. In this example, code is run on the GameState
// when a player scores, the score is retrieved from the PlayerState, and then popups indicating that they scored are shown on each
// client's screen using CLIENT RPC calls to the PlayerController.

// In the PlayerController...
TMap<AMGPlayerController*, int32> PayoutMap; // Map of players who have damaged this Controller and will be "paid out" health/mana if we die
UFUNCTION(Client, Reliable) void CLIENT_ScorePopup(const FName& ScoreReason, int32 Score, int32 HealthAmt, int32 ManaAmt);


// In the GameState...
void AMGGameState::AddTeamScore(AMGPlayerController* PC_Victim, AMGPlayerState* AttackerPS, int32 BonusScore)
{
	if (HasAuthority())
	{
		// Attacker Updates (Payouts + Scoreboard)
		if (PC_Victim)
		{
			for (const TPair<AMGPlayerController*, int32>& Pair : PC_Victim->PayoutMap) // Looping through killed player's Payout Map
			{
				if (Pair.Key && Pair.Value > 0) // Only run code if there is a payout available
				{
					AMGCharacter* CharRef = Cast<AMGCharacter>(Pair.Key->GetCharacter());
					if (CharRef)
					{
						// Reward attackers with health / mana
						int32 HealthGain = FMath::Clamp(CharRef->Health + Pair.Value, 0.0f, CharRef->MaxHealth) - CharRef->Health;
						int32 ManaGain = FMath::Clamp(CharRef->Mana + (Pair.Value / 2), 0.0f, CharRef->MaxMana) - CharRef->Mana;
						CharRef->Health += HealthGain;
						CharRef->Mana += ManaGain;

						AMGPlayerState* PayoutPS = Cast<AMGPlayerState>(Pair.Key->PlayerState);
						if (PayoutPS)
						{
							// Client score popups
							PayoutPS->PlayerScore += Pair.Value;
							if (PayoutPS != AttackerPS) { Pair.Key->CLIENT_ScorePopup("Assist", Pair.Value, HealthGain, ManaGain); }
							else { Pair.Key->CLIENT_ScorePopup("Kill", Pair.Value + BonusScore, HealthGain, ManaGain); }

							// Update all scoreboards with attackers' data
							for (FConstPlayerControllerIterator Itr = GetWorld()->GetPlayerControllerIterator(); Itr; Itr++)
							{
								AMGPlayerController* PC = Cast<AMGPlayerController>(Itr->Get());
								if (PayoutPS != AttackerPS) { PC->CLIENT_UpdateScoreboardKD(false, true, PayoutPS->TeamID, PayoutPS->MP_UUID, PayoutPS->PlayerScore); } // Attacker, not killer
								else { PC->CLIENT_UpdateScoreboardKD(true, true, PayoutPS->TeamID, PayoutPS->MP_UUID, PayoutPS->PlayerScore); }	// Killer
							}
						}
					}

					// Reset payout value to 0
					PC_Victim->PayoutMap[Pair.Key] = 0;
				}
			}

			// Victim Updates
			AMGPlayerState* VictimPS = Cast<AMGPlayerState>(PC_Victim->PlayerState);
			if (VictimPS)
			{
				for (FConstPlayerControllerIterator Itr = GetWorld()->GetPlayerControllerIterator(); Itr; Itr++)
				{
					// Update all scoreboards with victim's data
					AMGPlayerController* PC = Cast<AMGPlayerController>(Itr->Get());
					PC->CLIENT_UpdateScoreboardKD(false, false, VictimPS->TeamID, VictimPS->MP_UUID, VictimPS->PlayerScore);

					// Remove pending payouts for dead player 
					if (PC != PC_Victim && PC->PayoutMap.Find(PC_Victim)) { PC->PayoutMap[PC_Victim] = 0; }
				}

				// Decrease enemy team spawns
				if (VictimPS->TeamID == 1) { TeamDarkSpawns--; }
				else { TeamLightSpawns--; }
			}
		}

		// Verify win condition on GM
		if (TeamLightSpawns <= 0 || TeamDarkSpawns <= 0)
		{
			AMGTDMGameMode* GM = Cast<AMGTDMGameMode>(AuthorityGameMode);
			if (GM) { GM->VerifyWinCondition(); }
		}
	}
}
