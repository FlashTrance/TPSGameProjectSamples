// Shooting is not so straight-forward in this game. In addition to considering semi-auto vs auto fire, there is also a "Magic Firing Mode"
// feature, which when activated causes the player to use Mana instead of cullets as ammo, and also allows them to charge semi-automatic
// weapons before firing them.
// The code below contains the function in the Character class that executes when "Shoot" is pressed, as well as several functions in the Weapon
// class.

// CHARACTER CLASS - SHOOT
void AMGCharacter::Shoot_Pressed()
{
	bShootHeld = true;

	if (CurrentWeapon && ShootCondition())
	{
		// Set Vars
		if (HasAuthority()) { bIsShoot = true; }
		else { SRV_SetIsShoot(true); }

		FTimerHandle ShootHandle;
		GetWorld()->GetTimerManager().ClearTimer(ShootHandle);
		GetWorld()->GetTimerManager().SetTimer(ShootHandle, this, &AMGCharacter::DoneShooting, CurrentWeapon->TimeBetweenShots, false);

		// Select Fire Mode
		if (CurrentWeapon->bAutoFire)
		{
			if (!bMagicFiringMode) { CurrentWeapon->ShootAuto(); }
			else { CurrentWeapon->ShootMagicAuto(); }
		}
		else
		{
			if (!bMagicFiringMode) { CurrentWeapon->ShootSemiAuto(); }
			else 
			{ 
				CurrentWeapon->ShootMagicSemiAuto(); 

				// Check for charging semi-auto in Magic Firing Mode
				if (Mana >= CurrentWeapon->ChargedManaCost)
				{
					GetWorldTimerManager().ClearTimer(ChargingWeaponFXHandle);
					GetWorldTimerManager().ClearTimer(ChargeWeaponHandle);
					GetWorldTimerManager().SetTimer(ChargeWeaponHandle, this, &AMGCharacter::ChargedWeapon, CurrentWeapon->ChargeTime, false);

					// Crosshair Anim
					if (IsLocallyControlled() && PC->CHWidgetWBP) { PC->CHWidgetWBP->ChargingMagicShot(1 / CurrentWeapon->ChargeTime); }

					// Play Charging FX if shoot held down
					GetWorldTimerManager().SetTimer(ChargingWeaponFXHandle, this, &AMGCharacter::ChargingWeaponFX, 0.2f, false);
				}
			}
		}
	}
}


// ------------------------------------------------- //


// WEAPON CLASS - SHOOT
void AMGWeapon::Shoot()
{
	// Check ammo (needed for when player is holding shoot button during auto fire)
	if (AmmoInClip > 0)
	{
		// Run on server for replication
		if (!HasAuthority()) { SRVShoot(); }

		PlayerRef = Cast<APawn>(GetOwner());
		if (PlayerRef)
		{
			// Server - Decrement ammo in clip & Replicate Shoot FX
			if (HasAuthority())
			{
				AmmoInClip--;
				ShootFXCounter++;

				// Listen Server Clip Update
				if (PlayerRef->IsLocallyControlled())
				{
					AMGPlayerController* PC = Cast<AMGPlayerController>(PlayerRef->Controller);
					if (PC && PC->HealthWidgetWBP) { PC->HealthWidgetWBP->UpdateAmmoClip(AmmoInClip); }
				}
			}
			
			// Shoot FX on Local Client + Server
			ShootFX();

			if (PlayerRef->IsLocallyControlled()) 
			{ 
				// HUD FX
				AMGPlayerController* PC = Cast<AMGPlayerController>(PlayerRef->Controller);
				if (PC)
				{
					if (CamShakeClass) { PC->ClientStartCameraShake(CamShakeClass); }
					if (PC->CHWidgetWBP) { PC->CHWidgetWBP->CrosshairRecoilAnim(); }
				}

				// Spawn bullet projectile
				for (int i = 0; i < BulletCount; i++) { ProjectileTrajectoryCalc();	}
			}

			// Get World Time to use in Shoot Fire Mode functions
			LastShotTime = GetWorld()->TimeSeconds;
		}
	}
}


// SHOOT FIRE MODES
void AMGWeapon::ShootAuto()
{
	FirstDelay = FMath::Max(LastShotTime + TimeBetweenShots - GetWorld()->TimeSeconds, 0.0f); // Calculate whether we can shoot
	GetWorld()->GetTimerManager().SetTimer(ShootTimerHandle, this, &AMGWeapon::Shoot, TimeBetweenShots, true, FirstDelay); // Loops
}

void AMGWeapon::ShootSemiAuto()
{
	FirstDelay = FMath::Max(LastShotTime + TimeBetweenShots - GetWorld()->TimeSeconds, 0.0f);
	GetWorld()->GetTimerManager().SetTimer(ShootTimerHandle, this, &AMGWeapon::Shoot, TimeBetweenShots, false, FirstDelay); // Doesn't loop
}