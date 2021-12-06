// Sample of a Behavior Tree Service used to keep the AI pawn's arms/weapon rotated towards the player character, usually while attacking them

#include "BTService_MG_RotateTowards.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Kismet/KismetMathLibrary.h"

#include "AI_Base.h"
#include "AIController.h"
#include "Camera\CameraComponent.h"
#include "GameFramework\CharacterMovementComponent.h"
#include "GameFramework\SpringArmComponent.h"
#include "MGCharacter.h"

#include "Engine/GameEngine.h"

// CONSTRUCTOR
UBTService_MG_RotateTowards::UBTService_MG_RotateTowards()
{
	NodeName = "Serv_RotateTowardsCharacter";
	bCreateNodeInstance = true;
}


// Receive Tick
void UBTService_MG_RotateTowards::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	AMGCharacter* CoopPlayerTarget = Cast<AMGCharacter>(OwnerComp.GetBlackboardComponent()->GetValueAsObject(GetSelectedBlackboardKey()));
	if (CoopPlayerTarget && !CoopPlayerTarget->bIsDead)
	{
		AAI_Base* AIRef = Cast<AAI_Base>(OwnerComp.GetAIOwner()->GetPawn());
		if (AIRef && AIRef->CurrentWeapon)
		{
			// Rotate AI towards player on yaw
			FRotator LookAtRot = UKismetMathLibrary::FindLookAtRotation(AIRef->CameraBoom->GetComponentLocation(), CoopPlayerTarget->GetActorLocation());
			AIRef->SetActorRotation(FRotator(AIRef->GetActorRotation().Pitch, LookAtRot.Yaw, LookAtRot.Roll));
			
			// Set PlayerPitch value so we only rotate the arms/camera on pitch, not the whole mesh
			AIRef->PlayerPitch = LookAtRot.Pitch;
			AIRef->CameraBoom->SetRelativeRotation(FRotator(LookAtRot.Pitch, AIRef->CameraBoom->GetRelativeRotation().Yaw, AIRef->CameraBoom->GetRelativeRotation().Roll));

			// Get movement input for use in AnimInstance
			UCharacterMovementComponent* AIMovementComp = Cast<UCharacterMovementComponent>(AIRef->GetMovementComponent());
			if (AIMovementComp && AIMovementComp->GetCurrentAcceleration().Size() != 0)
			{
				AIRef->PlayerYaw = LookAtRot.Yaw;
				if (AIMovementComp->GetCurrentAcceleration().Normalize())
				{
					float MaxAccel = AIMovementComp->GetMaxAcceleration();
					float x = AIMovementComp->GetCurrentAcceleration().Size() / MaxAccel;
					FVector InputVec = AIMovementComp->GetCurrentAcceleration() * x;

					AIRef->MoveForwardVal = FVector::DotProduct(InputVec, AIRef->GetActorForwardVector());
					AIRef->MoveRightVal = FVector::DotProduct(InputVec, AIRef->GetActorRightVector());
				}
			}
		}
	}
}


// Node Description
FString UBTService_MG_RotateTowards::GetStaticDescription() const
{
	return FString::Printf(TEXT("%s\nBlackboard Key: %s"), *Super::GetStaticDescription(), *BlackboardKey.SelectedKeyName.ToString());
}
