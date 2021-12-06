// Sample of main function for handling sliding to cover after we've determined where to slide
// ---------------------------------------------------------------------------

// ACTIONS - COVER
// SLIDE
void AMGCharacter::SlideToCover()
{
	// Initial setup
	bCanceledSlide = false;
	GetWorld()->GetTimerManager().ClearTimer(TickHandle);
	GetWorld()->GetTimerManager().ClearTimer(MoveInCoverHandle);
	GetWorld()->GetTimerManager().ClearTimer(ErrorCorrectionHandle);

	// Disable error correction for move
	SetNoErrorCorrection();

	// Setup move variables
	FVector CoverArrowLoc = CurrentLocationComponent->GetComponentLocation();
	FVector CoverArrowFwdVec = CurrentLocationComponent->GetForwardVector();
	float DistanceToCover = (GetActorLocation() - CoverArrowLoc).Size();

	FVector_NetQuantize10 CoverMovementLoc = FVector_NetQuantize10(CoverArrowLoc.X, CoverArrowLoc.Y, CoverArrowLoc.Z) + (CoverArrowFwdVec * CoverWallDistance);
	FRotator CoverMoveRot = FRotator(0.0f, UKismetMathLibrary::MakeRotFromXZ(CoverArrowFwdVec, CoverArrowFwdVec).Yaw + 180.0f, 0.0f);

	// DebugTraceForGround(); // Debug Trace (for setting up LocComponents on inclines)

	// Angle based on InputDirection (defined in TakeCoverCondition)
	float SlideAngle = UKismetMathLibrary::NormalizedDeltaRotator(InputDirection.Rotation(), FRotator(0.0f, 
		CurrentLocationComponent->GetComponentRotation().Yaw + 180.0f, 0.0f)).Yaw;

	// Angle >= 0, take cover right
	if (SlideAngle >= 0)
	{
		if (!bTakingCoverRight)
		{
			if (HasAuthority()) { bTakingCoverRight = true; }
			else if (IsLocallyControlled()) { SRV_SetCoverRightTrue(); }
			AttachWeaponToOtherHand();
		}
	}
	else
	{
		if (bTakingCoverRight)
		{
			if (HasAuthority()) { bTakingCoverRight = false; }
			else if (IsLocallyControlled()) { SRV_SetCoverRightFalse(); }
			AttachWeaponToOtherHand();
		}
	}
		
	// Adjust time to move based on distance from cover
	float TimeToMoveToCover = (DistanceToCover / CoverTraceDistance) / 2.0f;
	
	// Adjust camera
	CameraAdjust(CoverLowOffset, DefaultFOV, 1.5f);

	// Setup Latent Action Info
	FLatentActionInfo LAI;
	LAI.CallbackTarget = this;
	LAI.ExecutionFunction = FName("SlideToCoverLAICallback");
	LAI.Linkage = 0;
	StopUUID = GetNextUUID();
	LAI.UUID = StopUUID;

	// Start timer for move in cover (prevents unintentional escaping from cover) 
	GetWorld()->GetTimerManager().SetTimer(MoveInCoverHandle, this, &AMGCharacter::SetCanMoveInCover, 0.1f, false);

	// Play Slide FX
	if (IsLocallyControlled())
	{
		if (!HasAuthority()) { SRV_PlayCoverSlideFX(); }
		else { Multicast_PlayCoverSlideFX(); }
	}

	// Set variables
	bCoverSliding = true;
	if (!HasAuthority() && IsLocallyControlled()) { SRV_SetCoverSlidingTrue(); }

	// Slide to cover
	SetActorTickEnabled(true);
	UKismetSystemLibrary::MoveComponentTo(GetCapsuleComponent(), CoverMovementLoc, CoverMoveRot, false, false, TimeToMoveToCover, true, 
		EMoveComponentAction::Move, LAI);
	if (!HasAuthority()) { SRV_SlideToCover(CoverMovementLoc, CoverMoveRot, TimeToMoveToCover, LAI); }
}
