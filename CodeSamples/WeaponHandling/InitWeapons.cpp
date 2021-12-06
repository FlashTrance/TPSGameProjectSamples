// Sample code from both the Character and PlayerController class showing some initialization of weapons on spawn
// ---------------------------------------------------------------------------------

// PLAYERCONTROLLER - POSSESSED PAWN
void AMGPlayerController::OnPossess(APawn* aPawn)
{
	Super::OnPossess(aPawn);

	// Initialize Health on Possession
	if (HasAuthority())
	{
		// Setup Refs
		AMGCharacter* CharRef = Cast<AMGCharacter>(aPawn);
		if (CharRef)
		{
			// Set Server PC Reference
			CharRef->PC = this;
	
			// Health + Mana
			CharRef->Health = 100.0f;
			CharRef->MaxHealth = CharRef->Health;
			CharRef->Mana = 250.0f;
			CharRef->MaxMana = CharRef->Mana;
		}

		// Init Weapons and Abilities
		GetWorld()->GetTimerManager().SetTimer(InitAbilitiesHandle, this, &AMGPlayerController::InitClientAbilities, 0.1f, true);
		GetWorld()->GetTimerManager().SetTimer(InitWeaponsClientHandle, this, &AMGPlayerController::InitClientWeapons, 0.1f, true);
		
		// Listen Server Initialization
		if (IsLocalPlayerController())
		{
			// ShootAnim Multicast and HUD
			GetWorld()->GetTimerManager().SetTimer(InitializeHUDHandle, this, &AMGPlayerController::InitializeHUD, 0.1f, true);
			GetWorld()->GetTimerManager().SetTimer(InitShootAnimHandle, this, &AMGPlayerController::InitializeShootAnim, 1.0f, true);
		}

		// Debug Info
		if (PS) { UE_LOG(LogTemp, Warning, TEXT("TeamID: %d"), (PS->TeamID));}
	}
}



// PLAYER CONTROLLER - SPAWN WEAPONS (Runs on Server)
void AMGPlayerController::InitClientWeapons()
{
	AMGCharacter* CharRef = Cast<AMGCharacter>(GetCharacter());
	if (CharRef && CharRef->GetMesh() && PS && Weapon1Class && Weapon2Class)
	{
		GetWorldTimerManager().ClearTimer(InitWeaponsClientHandle);

		CharRef->TeamID = PS->TeamID;
		CharRef->SRV_SpawnWeapons();
	}
	UE_LOG(LogTemp, Error, TEXT("Infinite Timer Loop: InitClientWeapons - PlayerController"));
}


// ---------------------------------------------------- //


// CHARACTER CLASS - Spawn Weapons
void AMGCharacter::SRV_SpawnWeapons_Implementation()
{
	if (HasAuthority() && PC)
	{
		// Set up Spawn Params
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnParams.Owner = this;

		// PlayerState Ref
		AMGPlayerState* PS = Cast<AMGPlayerState>(PC->PlayerState);

		// Weapon2 Spawn and Attach to Holster
		Weapon2 = GetWorld()->SpawnActor<AMGWeapon>(PC->Weapon2Class, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
		if (Weapon2)
		{
			Weapon2->SetOwner(this);
			CurrentWeapon = Weapon2;

			// Attach weapon to appropriate sockets
			FName AttachSocket;
			if (Weapon2->bIsPrimary) { AttachSocket = HolsterSocketNameBack; }
			else { AttachSocket = HolsterSocketNameHip; }
			CurrentWeapon->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, AttachSocket);
		}

		// Weapon1 Spawn and Attach to Player Hand
		Weapon1 = GetWorld()->SpawnActor<AMGWeapon>(PC->Weapon1Class, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
		if (Weapon1)
		{
			Weapon1->SetOwner(this);
			CurrentWeapon = Weapon1;

			CurrentWeapon->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, WeaponSocketName);

			if (Weapon1->bIsPrimary) { bPrimaryEquipped = true; }
		}

		// Set bullet types from PS
		if (PS)
		{
			Weapon2->bShredderRounds = PS->LoadoutStruct.weapon2.shredder_rounds;
			Weapon1->bShredderRounds = PS->LoadoutStruct.weapon1.shredder_rounds;
			Weapon2->bLoveRounds = PS->LoadoutStruct.weapon2.love_rounds;
			Weapon1->bLoveRounds = PS->LoadoutStruct.weapon1.love_rounds;
		}
	}
}
bool AMGCharacter::SRV_SpawnWeapons_Validate() { return true; }
