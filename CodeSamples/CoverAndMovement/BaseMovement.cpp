// Sample of code used to handle movement input on WASD/Left Stick
//----------------------------------------------------------------

// FORWARD
void AMGCharacter::MoveForward(float Value)
{
	// NOT IN COVER
	if (!bCoverSliding && bCanMove)
	{
		if (bUsingGamepad)
		{
			// Deadzone setup (radial)
			FVector2D StickInput = FVector2D(InputComponent->GetAxisValue("MoveRight"), Value);
			if (StickInput.Size() < DeadzoneDebugVal) { Value = 0; }
			else
			{
				FRotator MovementRot = UKismetMathLibrary::MakeRotator(0.0f, 0.0f, GetControlRotation().Yaw);
				FVector MovementVector = UKismetMathLibrary::ClampVectorSize(UKismetMathLibrary::GetForwardVector(MovementRot), WalkSpeed, WalkSpeed);
				AddMovementInput(MovementVector, Value);
			}
		}
		else
		{
			if (Value != 0)
			{
				FRotator MovementRot = UKismetMathLibrary::MakeRotator(0.0f, 0.0f, GetControlRotation().Yaw);
				AddMovementInput(UKismetMathLibrary::GetForwardVector(MovementRot), Value);
			}
		}
	}

	// TAKING COVER
	else if (bCanMoveInCover && bCanMove)
	{
		// Deadzone check for gamepad
		if (bUsingGamepad)
		{
			FVector2D StickInput = FVector2D(InputComponent->GetAxisValue("MoveRight"), Value);
			if (StickInput.Size() < DeadzoneDebugVal) { Value = 0; }
		}

		// NOT AIMING - Calculate forward value based on camera direction
		if (!bIsAim)
		{
			FwdCoverValue = (UKismetMathLibrary::DegCos((UKismetMathLibrary::NormalizedDeltaRotator(GetControlRotation(), GetActorRotation())).Yaw)
				* Value) + ((UKismetMathLibrary::DegSin(0 - (UKismetMathLibrary::NormalizedDeltaRotator(GetControlRotation(), GetActorRotation())).Yaw)
					* InputComponent->GetAxisValue("MoveRight")));
			
			// Escape cover condition
			if (FwdCoverValue < EscapeCoverLimit) { EscapeCover(); }
			else if (bSwatEscape && FwdCoverValue > EscapeCoverSWATLimit) { EscapeCover(); }
		}
		
		// AIMING - Similar to when we're not aiming, but we need a normalized rotator with respect to the cover normal, not player rotation
		else
		{
			FwdCoverValue = ((UKismetMathLibrary::DegCos((UKismetMathLibrary::NormalizedDeltaRotator(GetControlRotation(), 
				CurrentLocationComponent->GetComponentRotation())).Yaw) * Value) + ((UKismetMathLibrary::DegSin(0 - (UKismetMathLibrary::NormalizedDeltaRotator
				(GetControlRotation(), CurrentLocationComponent->GetComponentRotation())).Yaw) * InputComponent->GetAxisValue("MoveRight")))) * -1.0f;

			// Escape cover condition
			if (FwdCoverValue < EscapeCoverLimit) { EscapeCover(); }
		}
	}

	// Calculate Direction for Aim Offset
	if ((bAimHeld || bIsDowned) && CurrentMoveValueForward != Value)
	{
		CurrentMoveValueForward = Value;
		if (IsLocallyControlled())
		{
			if (!HasAuthority()) { SRV_SetMoveForward(Value); }
			else { MoveForwardVal = Value; }
		}
	}
}

// RIGHT
void AMGCharacter::MoveRight(float Value)
{
	// NOT IN COVER
	if (!bCoverSliding && bCanMove)
	{
		if (bUsingGamepad)
		{
			// Deadzone setup (radial)
			FVector2D StickInput = FVector2D(Value, InputComponent->GetAxisValue("MoveForward"));
			if (StickInput.Size() < DeadzoneDebugVal) { Value = 0; }
			else
			{
				FRotator MovementRot = UKismetMathLibrary::MakeRotator(0.0f, 0.0f, GetControlRotation().Yaw);
				FVector MovementVector = UKismetMathLibrary::ClampVectorSize(UKismetMathLibrary::GetRightVector(MovementRot), WalkSpeed, WalkSpeed);
				AddMovementInput(MovementVector, Value);
			}
		}
		else
		{
			if (Value != 0)
			{
				FRotator MovementRot = UKismetMathLibrary::MakeRotator(0.0f, 0.0f, GetControlRotation().Yaw);
				AddMovementInput(UKismetMathLibrary::GetRightVector(MovementRot), Value);
			}
		}
	}

	// TAKING COVER
	else if (bCoverSlideComplete && bCanMoveInCover && !bSwatting)
	{
		// Deadzone check for gamepad
		if (bUsingGamepad)
		{
			FVector2D StickInput = FVector2D(Value, InputComponent->GetAxisValue("MoveForward"));
			if (StickInput.Size() < DeadzoneDebugVal) { Value = 0; }
		}

		// NOT AIMING
		if (!bIsAim)
		{
			RightCoverValue = (UKismetMathLibrary::DegSin((UKismetMathLibrary::NormalizedDeltaRotator(GetControlRotation(), GetActorRotation())).Yaw) * InputComponent->GetAxisValue("MoveForward")) +
				((UKismetMathLibrary::DegCos(0 - (UKismetMathLibrary::NormalizedDeltaRotator(GetControlRotation(), GetActorRotation())).Yaw) * Value));
		}
		
		// AIMING
		else
		{
			RightCoverValue = ((UKismetMathLibrary::DegSin((UKismetMathLibrary::NormalizedDeltaRotator(GetControlRotation(), 
				CurrentLocationComponent->GetComponentRotation())).Yaw) * InputComponent->GetAxisValue("MoveForward")) + ((UKismetMathLibrary::DegCos(0 - 
				(UKismetMathLibrary::NormalizedDeltaRotator(GetControlRotation(), CurrentLocationComponent->GetComponentRotation())).Yaw) * Value))) * -1.0f;
		}

		// NOT MOVING OR INPUT IS TOWARDS WALL - Stop movement
		if ((RightCoverValue < RightCoverLimit && RightCoverValue > -RightCoverLimit) || FwdCoverValue > FwdCoverLimit || !bCanMove)
		{
			// Note: Can't use "StopCoverMovement" function because this needs to run even if RightCoverValue = 0
			RightCoverValue = 0.0f;
			if (Controller) { Controller->StopMovement(); }
			if (!HasAuthority()) { SRV_SetRightCoverValue(0.0f); }
			
			// Check for actions at edge
			if ((bEdgeLeft || bEdgeRight) && (abs(InputComponent->GetAxisValue("MoveForward")) > 0.3 || abs(Value) > 0.3))
			{
				CheckCoverActionsAtEdge();

				// Show vault cover icon in the case the edge box is blocking
				if (!bCanPeekAim && bCanVaultCover && !bIsAim && FwdCoverValue > FwdCoverLimit)
				{
					//GEngine->AddOnScreenDebugMessage(-1, 0.01f, FColor::Orange, FString::Printf(TEXT("Vault Cover")));
				}
			}

			// @TODO Show vault cover icon when we're not in an edge box
			else if (bCanVaultCover && !bIsAim && !(bEdgeLeft && !bTakingCoverRight) && !(bEdgeRight && bTakingCoverRight) && FwdCoverValue > 0.70)
			{
				//GEngine->AddOnScreenDebugMessage(-1, 0.01f, FColor::Orange, FString::Printf(TEXT("Vault Cover")));
			}
		}

		// VALID INPUT - COVER RIGHT
		else if (RightCoverValue > RightCoverLimit)
		{
			if (!bEdgeRight) // Don't move past edge box
			{
				CheckPeekAiming(); // Ensure peek aiming is off

				// Set values needed for Anim replication
				if (HasAuthority() && !bTakingCoverRight && !bIsAim)
				{ 
					bTakingCoverRight = true; 
					AttachWeaponToOtherHand();
				}
				else if (!HasAuthority())
				{
					if (RightCoverValue != 1.0f) { SRV_SetRightCoverValue(1.0f); }
					if (!bTakingCoverRight && !bIsAim) 
					{ 
						bTakingCoverRight = true;
						SRV_SetCoverRightTrue(); 
						AttachWeaponToOtherHand();
					}
				}

				// Check for player blocking cover movement
				if (!LineTraceForCoverPlayer())
				{
					RightCoverValue = 1.0f;
					LineTraceForCoverInfo();
				}
				else { StopCoverMovement(); }
			}

			// In Right Edge Box
			else { CheckCoverActionsAtEdge(); }
		}

		// VALID INPUT - COVER LEFT
		else if (RightCoverValue < -RightCoverLimit)
		{
			if (!bEdgeLeft) // Don't move past edge box
			{
				CheckPeekAiming(); // Ensure peek aiming is off

				// Set values needed for Anim replication
				if (HasAuthority() && bTakingCoverRight && !bIsAim)
				{ 
					bTakingCoverRight = false; 
					AttachWeaponToOtherHand();
				}
				else if (!HasAuthority())
				{
					if (RightCoverValue != -1.0f) { SRV_SetRightCoverValue(-1.0f); }
					if (bTakingCoverRight && !bIsAim) 
					{ 
						bTakingCoverRight = false;
						SRV_SetCoverRightFalse(); 
						AttachWeaponToOtherHand();
					}
				}

				// Check for player blocking cover movement
				if (!LineTraceForCoverPlayer())
				{
					RightCoverValue = -1.0f;
					LineTraceForCoverInfo();
				}
				else { StopCoverMovement(); }
			}
			
			// In Left Edge Box
			else { CheckCoverActionsAtEdge(); }
		}
		
		// Add Movement Input
		FVector CoverDirection = UKismetMathLibrary::GetRightVector(UKismetMathLibrary::MakeRotFromXZ(CurrentLocationComponent->GetForwardVector()
			* FVector(-1.0f, -1.0f, 0.0f), GetCapsuleComponent()->GetUpVector()));
		AddMovementInput(CoverDirection, RightCoverValue);
	}
		
	// Calculate Direction for Aim Offset
	if ((bAimHeld || bIsDowned) && CurrentMoveValueRight != Value)
	{
		CurrentMoveValueRight = Value;
		if (IsLocallyControlled())
		{
			if (!HasAuthority()) { SRV_SetMoveRight(Value); }
			else { MoveRightVal = Value; }
		}
	}
}
