// Below is the majority of the coode that checks a wide variety of situations when shooting and spawning projectiles. 
// Traces are performed from center screen AND from the muzzle for both player and non-player actors, and then results
// are compared to decide how we should respond.  

// For example, the crosshair might be aiming past a wall, but the muzzle of the weapon is pointed at the wall. 
// Or, the crosshair might be aiming past a PLAYER, but the muzzle is pointed at the player. In the first case, I'd 
// want to hit the wall, but in the second case, I want the bullet to go where the crosshair is aiming.

// Projectile Trajectory
void AMGWeapon::ProjectileTrajectoryCalc()
{
	// Vars
	FTransform Trajectory;
	AMGCharacter* PlayerChar = Cast<AMGCharacter>(GetOwner());
	if (PlayerChar)
	{
		// Setup trace with bullet spread
		float ConeDegrees;
		if (!PlayerChar->bMagicFiringMode) { ConeDegrees = FMath::DegreesToRadians(BulletSpread); }
		else { ConeDegrees = FMath::DegreesToRadians(MagicBulletSpread); }
		FVector ShotDirection = FMath::VRandCone(PlayerChar->FollowCamera->GetForwardVector(), ConeDegrees, ConeDegrees);
		FVector TraceStart = PlayerChar->FollowCamera->GetComponentLocation();
		FVector TraceEnd = TraceStart + (ShotDirection * 10000);

		// Setup query parameters for shoot trace
		FCollisionQueryParams ShootTraceParams;
		ShootTraceParams.AddIgnoredActor(PlayerChar);
		ShootTraceParams.AddIgnoredActor(this);
		ShootTraceParams.bTraceComplex = true;

		// Set transform values based on result of line trace
		FHitResult CenterScreenHit;
		FHitResult MuzzleHit;
		FVector WeaponSocketLoc = WeaponMesh->GetSocketLocation(MuzzleSocketName);

		// NOT IN COVER
		if (!PlayerChar->bCoverSliding)
		{
			// CENTER SCREEN TRACE HIT CHARACTER
			if (GetWorld()->LineTraceSingleByChannel(CenterScreenHit, TraceStart, TraceEnd, ECC_CHARACTERTRACE, ShootTraceParams))
			{
				// Check for strange angles (e.g. - player gun muzzle is beyond the opponent, but crosshair is still on opponent due to perspective)
				FVector StrangeVector = CenterScreenHit.ImpactPoint - WeaponSocketLoc;
				FVector MuzzleVector = WeaponMesh->GetSocketRotation(MuzzleSocketName).Vector();
				StrangeVector.Normalize();
				float StrangeAngle = UKismetMathLibrary::Dot_VectorVector(StrangeVector, WeaponMesh->GetSocketRotation(MuzzleSocketName).Vector());

				// NO STRANGE ANGLE - Shoot from center screen 
				if (StrangeAngle > -StrangeAngleLimit)
				{
					Trajectory.SetLocation(WeaponSocketLoc);
					Trajectory.SetRotation(UKismetMathLibrary::FindLookAtRotation(WeaponSocketLoc, CenterScreenHit.ImpactPoint).Quaternion());
					//DrawDebugLine(GetWorld(), WeaponSocketLoc, CenterScreenHit.ImpactPoint, FColor::Green, false, 2.0f, 0, 0.0f);
				}

				// STRANGE ANGLE - Shoot from muzzle
				else
				{
					Trajectory.SetLocation(WeaponSocketLoc);
					Trajectory.SetRotation(UKismetMathLibrary::FindLookAtRotation(WeaponSocketLoc, WeaponSocketLoc + (MuzzleVector * 10000)).Quaternion());
					//DrawDebugLine(GetWorld(), WeaponSocketLoc, WeaponSocketLoc + (MuzzleVector * 10000), FColor::Orange, false, 2.0f, 0, 0.0f);
				}

				SRV_SpawnProjectile(Trajectory);
			}
			else
			{
				// CENTER SCREEN TRACE HIT NON-PLAYER OBJECT
				if (GetWorld()->LineTraceSingleByChannel(CenterScreenHit, TraceStart, TraceEnd, ECC_WEAPONTRACE, ShootTraceParams))
				{
					// MUZZLE TRACE HIT PLAYER
					if (GetWorld()->LineTraceSingleByChannel(MuzzleHit, WeaponSocketLoc, TraceEnd, ECC_CHARACTERTRACE, ShootTraceParams))
					{
						// Center Screen Trace hit an object, but Muzzle Trace hit player, we should have missed! 
						// Use center-screen and cosmetic projectiles instead.
						Trajectory.SetLocation(WeaponSocketLoc);
						Trajectory.SetRotation(UKismetMathLibrary::FindLookAtRotation(WeaponSocketLoc, TraceEnd).Quaternion());
						SRV_SpawnCosmeticProjectile(Trajectory);

						Trajectory.SetLocation(TraceStart);
						Trajectory.SetRotation(UKismetMathLibrary::FindLookAtRotation(TraceStart, TraceEnd).Quaternion());
						SRV_SpawnCenterScreenProjectile(Trajectory);
						//DrawDebugLine(GetWorld(), WeaponSocketLoc, TraceEnd, FColor::Red, false, 2.0f, 0, 0.0f);
					}
					else
					{
						// MUZZLE TRACE HIT NON-PLAYER OBJECT
						if (GetWorld()->LineTraceSingleByChannel(MuzzleHit, WeaponSocketLoc, TraceEnd, ECC_WEAPONTRACE, ShootTraceParams))
						{
							// Center Screen Trace and Muzzle Trace both hit a non-player object, use regular projectile.
							Trajectory.SetLocation(WeaponSocketLoc);
							Trajectory.SetRotation(UKismetMathLibrary::FindLookAtRotation(WeaponSocketLoc, CenterScreenHit.ImpactPoint).Quaternion());
							//DrawDebugLine(GetWorld(), WeaponSocketLoc, CenterScreenHit.ImpactPoint, FColor::Green, false, 2.0f, 0, 0.0f);

							SRV_SpawnProjectile(Trajectory);
						}

						else
						{
							// Muzzle Trace failed, so we need to shoot from center screen
							Trajectory.SetLocation(WeaponSocketLoc);
							Trajectory.SetRotation(UKismetMathLibrary::FindLookAtRotation(WeaponSocketLoc, TraceEnd).Quaternion());
							SRV_SpawnCosmeticProjectile(Trajectory);

							Trajectory.SetLocation(TraceStart);
							Trajectory.SetRotation(UKismetMathLibrary::FindLookAtRotation(TraceStart, TraceEnd).Quaternion());
							SRV_SpawnCenterScreenProjectile(Trajectory);
							//DrawDebugLine(GetWorld(), WeaponSocketLoc, TraceEnd, FColor::Red, false, 2.0f, 0, 0.0f);
						}
					}
				}

				// CENTER SCREEN TRACE HIT NOTHING
				else
				{
					// MUZZLE TRACE HIT PLAYER
					if (GetWorld()->LineTraceSingleByChannel(MuzzleHit, WeaponSocketLoc, TraceEnd, ECC_CHARACTERTRACE, ShootTraceParams))
					{
						// Center Screen Trace hit nothing, but Muzzle Trace hit player, we should have missed! 
						// Use center-screen and cosmetic projectiles instead.
						Trajectory.SetLocation(WeaponSocketLoc);
						Trajectory.SetRotation(UKismetMathLibrary::FindLookAtRotation(WeaponSocketLoc, TraceEnd).Quaternion());
						SRV_SpawnCosmeticProjectile(Trajectory);

						Trajectory.SetLocation(TraceStart);
						Trajectory.SetRotation(UKismetMathLibrary::FindLookAtRotation(TraceStart, TraceEnd).Quaternion());
						SRV_SpawnCenterScreenProjectile(Trajectory);
						//DrawDebugLine(GetWorld(), WeaponSocketLoc, TraceEnd, FColor::Red, false, 2.0f, 0, 0.0f);
					}
					else
					{
						// MUZZLE TRACE HIT NON-PLAYER OBJECT
						if (GetWorld()->LineTraceSingleByChannel(MuzzleHit, WeaponSocketLoc, TraceEnd, ECC_WEAPONTRACE, ShootTraceParams))
						{
							// Center Screen Trace hit nothing, Muzzle Trace both hit an object, use regular projectile
							Trajectory.SetLocation(WeaponSocketLoc);
							Trajectory.SetRotation(UKismetMathLibrary::FindLookAtRotation(WeaponSocketLoc, TraceEnd).Quaternion());
							//DrawDebugLine(GetWorld(), WeaponSocketLoc, TraceEnd, FColor::Purple, false, 2.0f, 0, 0.0f);

							SRV_SpawnProjectile(Trajectory);
						}
						else
						{
							// Neither trace hit! Player is shooting the sky or something. Use regular projectile.
							Trajectory.SetLocation(WeaponSocketLoc);
							Trajectory.SetRotation(UKismetMathLibrary::FindLookAtRotation(WeaponSocketLoc, TraceEnd).Quaternion());
							//DrawDebugLine(GetWorld(), WeaponSocketLoc, TraceEnd, FColor::Blue, false, 1.0f, 0, 0.0f);

							SRV_SpawnProjectile(Trajectory);
						}
					}
				}
			}
		}

		// IN COVER
		else 
		{
			// Shooting from cover code not displayed here for brevity 
			// Basically it  makes sure we always use center-screen shooting while in cover
		}
	}
}


// Spawn Projectile On Server - REGULAR
void AMGWeapon::SRV_SpawnProjectile_Implementation(const FTransform ProjectileTrajectory)
{
		// Spawn Parameters
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		PlayerRef = Cast<APawn>(GetOwner());
		if (PlayerRef) { SpawnParams.Instigator = PlayerRef; }
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		// Spawn projectile
		GetWorld()->SpawnActor(ProjectileClass, &ProjectileTrajectory, SpawnParams);
}
bool AMGWeapon::SRV_SpawnProjectile_Validate(FTransform ProjectileTrajectory) { return true; }
