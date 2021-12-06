// Function that determines whether or not we can slide to cover, using a line trace and some vector math...
// -----------------------------------------------------------------------

bool AMGCharacter::TakeCoverCondition()
{
	if (IsLocallyControlled())
	{
		if ((abs(InputComponent->GetAxisValue("MoveForward")) > 0.1f || abs(InputComponent->GetAxisValue("MoveRight")) > 0.1f) && !bIsRoll
			&& !bIsRolling && !bIsAim && !bAimHeld && !bIsDowned)
		{
			// Calculate player input direction
			FRotator CamRollRot = UKismetMathLibrary::MakeRotator(CameraBoom->GetTargetRotation().Roll, 0.0f, CameraBoom->GetTargetRotation().Yaw);
			FVector InputDirectionOrig = (UKismetMathLibrary::GetForwardVector(CamRollRot) * InputComponent->GetAxisValue("MoveForward")) +
				(UKismetMathLibrary::GetRightVector(CamRollRot) * InputComponent->GetAxisValue("MoveRight"));
			InputDirection = UKismetMathLibrary::ClampVectorSize(InputDirectionOrig, CoverTraceDistance, CoverTraceDistance);

			// Line trace for cover
			FVector TraceStart = GetCapsuleComponent()->GetComponentLocation();
			FVector TraceEnd = TraceStart + (InputDirection);

			FCollisionQueryParams CoverQueryParams;
			CoverQueryParams.AddIgnoredActor(this);

			FHitResult CoverHitResult;
			if (GetWorld()->LineTraceSingleByChannel(CoverHitResult, TraceStart, TraceEnd, ECC_COVERTRACE, CoverQueryParams))
			{
				//DrawDebugLine(GetWorld(), TraceStart, CoverHitResult.ImpactPoint, FColor::Purple, false, 3.0f);
				ACoverCollisionBox* HitCover = Cast<ACoverCollisionBox>(CoverHitResult.Actor);
				if (HitCover)
				{
					// Set Location Component to traced box
					CurrentLocationComponent = HitCover->LocationComponent;

					// If another player is in the way of our slide, we can't continue
					if (LineTraceForPlayerBlockingCover(CurrentLocationComponent, 20.0f)) { return false; }
					
					// Check conditions for sliding to edge box
					else if (HitCover->EdgeReference)
					{
						float SlideAngle = UKismetMathLibrary::NormalizedDeltaRotator(InputDirection.Rotation(), FRotator(0.0f,
							CurrentLocationComponent->GetComponentRotation().Yaw + 180.0f, 0.0f)).Yaw;
						float DegreesThreshold = 15.0f;

						// If we're sliding in towards the edge...
						if ((HitCover->EdgeReference->bRightEdge && SlideAngle > DegreesThreshold) || (!HitCover->EdgeReference->bRightEdge && SlideAngle < -DegreesThreshold))
						{
							// Set HitCover to the reference instead
							HitCover = HitCover->EdgeReference;
							CurrentLocationComponent = HitCover->LocationComponent;
						}

						// If alternate EdgeReference is set, that's definitely where we want to go
						else if (HitCover->EdgeReferenceAlt && SlideAngle < -DegreesThreshold)
						{
							HitCover = HitCover->EdgeReferenceAlt;
							CurrentLocationComponent = HitCover->LocationComponent;
						}
					}

					// Get info from box
					float AngleCheck = UKismetMathLibrary::Dot_VectorVector(CurrentLocationComponent->GetForwardVector(), InputDirection);
					if (AngleCheck <= 0)
					{
            			// Get a bunch of data from the boxes, etc...
						return true;
					}
				}
			}
		}
	}
	return false;
}
