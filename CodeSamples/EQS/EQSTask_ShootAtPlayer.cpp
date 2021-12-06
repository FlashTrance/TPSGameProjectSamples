// A Behavior Tree Task used to tell the AI to shoot at the player character. 

#include "BTTask_MG_ShootAtPlayer.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AI_Shooter_Basic.h"
#include "AIController.h"
#include "MGWeapon_AI.h"

#include "Engine/World.h"
#include "TimerManager.h"

// CONSTRUCTOR
UBTTask_MG_ShootAtPlayer::UBTTask_MG_ShootAtPlayer()
{
	NodeName = "MGShootAtPlayer";
	bCreateNodeInstance = true;

	NumShots = 5;
}


// EXECUTE TASK AI
EBTNodeResult::Type UBTTask_MG_ShootAtPlayer::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	FinishedShots = 0;

	// Check reference to AI Pawn and player target set in Blackboard
	AIRef = Cast<AAI_Shooter_Basic>(OwnerComp.GetAIOwner()->GetPawn());
	ShootTarget = Cast<AActor>(OwnerComp.GetBlackboardComponent()->GetValueAsObject(GetSelectedBlackboardKey()));
	AMGWeapon_AI* CurrentWeaponAI = Cast<AMGWeapon_AI>(AIRef->CurrentWeapon);
	if (AIRef && ShootTarget && CurrentWeaponAI)
	{
		CurrentWeaponAI->CurrentTarget = ShootTarget;
		AIRef->Multicast_SetShootAnim();
		AIRef->AimPressedNoChecks();

		TotalShots = NumShots + FMath::RandRange(-1, 2); // Randomize # of shots a bit

		// Check if we need to reload
		if (NumShots > CurrentWeaponAI->AmmoInClip) { AIRef->ReloadAndInteract(); }

		GetWorld()->GetTimerManager().SetTimer(AfterAimHandle, this, &UBTTask_MG_ShootAtPlayer::ShootLoop, 1.0f, false);
		return EBTNodeResult::InProgress;
	}
	return EBTNodeResult::Failed;
}

// SHOOT
void UBTTask_MG_ShootAtPlayer::ShootLoop()
{
	GetWorld()->GetTimerManager().ClearTimer(AfterAimHandle);
	GetWorld()->GetTimerManager().SetTimer(ShootHandle, this, &UBTTask_MG_ShootAtPlayer::StartShooting, 0.3f, true, 0.0f);
}

void UBTTask_MG_ShootAtPlayer::StartShooting()
{
	if (FinishedShots < TotalShots && AIRef)
	{ 
		AIRef->Shoot_Pressed();
		FinishedShots++; 
	}
	else
	{
		GetWorld()->GetTimerManager().ClearTimer(ShootHandle);
		if (AIRef) { AIRef->Aim_Released(); }

		UBehaviorTreeComponent* OwnerComp = Cast<UBehaviorTreeComponent>(GetOuter());
		if (OwnerComp) 
		{ 
			FinishLatentTask(*OwnerComp, EBTNodeResult::Succeeded); 
		}
	}
}


// Node Description
FString UBTTask_MG_ShootAtPlayer::GetStaticDescription() const
{
	return FString::Printf(TEXT("Shoot at player"));
}
