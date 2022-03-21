// Sample of Behavior Tree Decorator that is used to check whether a player character is in line of sight

#include "BTDecorator_MG_InLineOfSight.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "DrawDebugHelpers.h"

#include "AIController.h"
#include "Camera/CameraComponent.h"
#include "MGCharacter.h"

// CONSTRUCTOR
UBTDecorator_MG_InLineOfSight::UBTDecorator_MG_InLineOfSight()
{
	NodeName = "Cond_InLineOfSight";
	bCreateNodeInstance = true;
}


// Condition Check
bool UBTDecorator_MG_InLineOfSight::CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const
{
	AActor* AIOwner = Cast<AActor>(OwnerComp.GetAIOwner()->GetPawn());
	AMGCharacter* CoopPlayerTarget = Cast<AMGCharacter>(OwnerComp.GetBlackboardComponent()->GetValueAsObject(GetSelectedBlackboardKey()));
	if (CoopPlayerTarget && AIOwner)
	{
		// Setup line of sight trace (subtracting on Z to make sure it blocks low cover)
		FVector TraceStart = CoopPlayerTarget->GetActorLocation() + FVector(0, 0, 80.0f);
		FVector TraceEnd = AIOwner->GetActorLocation() - FVector(0, 0, 25.0f);

		FCollisionQueryParams LoSTraceParams;
		LoSTraceParams.AddIgnoredActor(CoopPlayerTarget);
		LoSTraceParams.bTraceComplex = true;
		FHitResult SightHit;

		if (GetWorld()->LineTraceSingleByChannel(SightHit, TraceStart, TraceEnd, ECC_AITRACE, LoSTraceParams))
		{
			// DrawDebugLine(GetWorld(), TraceStart, TraceEnd, FColor::Green, false, 3.0f, -1, 0.5f);
			if (SightHit.GetActor() == AIOwner) { return true; }
		}
		// else { DrawDebugLine(GetWorld(), TraceStart, TraceEnd, FColor::Red, false, 3.0f, -1, 0.5f); }
	}
	return false;
}


// Node Description
FString UBTDecorator_MG_InLineOfSight::GetStaticDescription() const
{
	return FString::Printf(TEXT("%s\nTrace: from %s"), *Super::GetStaticDescription(), *BlackboardKey.SelectedKeyName.ToString());
}
