// The TakePointDamage function inside the main Character class
// ---------------------------------------------------------

void AMGCharacter::TakePointDamage(AActor* DamagedActor, float Damage, class AController* InstigatedBy, FVector HitLocation, class UPrimitiveComponent* FHitComponent, FName BoneName, FVector ShotFromDirection, const class UDamageType* DamageType, AActor* DamageCauser)
{
	if (HasAuthority())
	{
		// Check not dead
		if (Damage > 0 && Health > 0)
		{
			// Controller Pointer check
			AMGPlayerController* PC_Attacker = Cast<AMGPlayerController>(InstigatedBy);
			if (PC && PC_Attacker)
			{
				// PlayerState Pointer check
				AMGPlayerState* PS = Cast<AMGPlayerState>(PC->PlayerState);
				AMGPlayerState* PS_Attacker = Cast<AMGPlayerState>(PC_Attacker->PlayerState);
				if (PS && PS_Attacker)
				{
					// Check friendly fire
					if (TeamID != PS_Attacker->TeamID)
					{
						// Player about to be dead, add remaining health to payout map
						if (!bIsDowned && (Health - Damage) <= 0 && PC->PayoutMap.Find(PC_Attacker)) { PC->PayoutMap[PC_Attacker] += Health; }
						
						// Reduce health (clamp to not go below 0)
						Health = FMath::Clamp(Health - Damage, 0.0f, MaxHealth); 
						
						// Listen Server Updates
						if (IsLocallyControlled()) 
						{ 
							if (PC->HealthWidgetWBP) { PC->HealthWidgetWBP->UpdateHUDHealth(); }
							SRV_UpdateFocusWidget(1); 
						}
						
						// Local Client functions
						if (BoneName != "head") { PC_Attacker->CLIENT_ShowHitMarker(true, Health); }
						else { PC_Attacker->CLIENT_ShowHitMarker(false, Health); }

						AMGGameState* GS = Cast<AMGGameState>(UGameplayStatics::GetGameState(GetWorld()));
						if (GS)
						{ 
							// Not dead or down, add total damage to payout map 
              // (If we die, players who damage us earn score/health based on how much they damaged us, this is how we track that)
							if (!bIsDowned && Health > 0 && PC->PayoutMap.Find(PC_Attacker)) { PC->PayoutMap[PC_Attacker] += Damage; }

							else if (Health <= 0.0f)
							{
								// Not downed
								uint8 KillStateNum = KilledStateCondition(BoneName, DamageCauser);
								if (KillStateNum > 0)
								{
									// Set K/D Scores
									PS->Deaths++;
									PS_Attacker->Kills++;
									int32 Bonus = 0;
									if (!bIsDowned)
									{
										if (KillStateNum == 3)		{ PS_Attacker->PlayerScore += 50; Bonus += 50; } // Headshot
										else if (KillStateNum == 2) { PS_Attacker->PlayerScore += 25; Bonus += 25; } // Limb Break
									}
									
									// Update player scoreboards and check win condition
									GS->AddTeamScore(PC, PS_Attacker, Bonus);

									// Calculate impulse
									FVector_NetQuantize ImpulseDirection = FVector_NetQuantize(0, 0, 0);
									if (PC_Attacker->GetCharacter())
									{
										ImpulseDirection = HitLocation - PC_Attacker->GetCharacter()->GetActorLocation();
										ImpulseDirection.Normalize();
										ImpulseDirection = ImpulseDirection * DamageType->DamageImpulse;
									}

									AfterDeath(BoneName, ImpulseDirection, KillStateNum);
								}

								// Downed
								else
								{
									GS->AddSingleScore(PC_Attacker, 25, 1, 1, "Down");
									GS->UpdatePlayerStatusIcon(PS, 0, true);

									AfterDowned();
								}
							}
						}
					}
				}
			}
		}
	}
}
