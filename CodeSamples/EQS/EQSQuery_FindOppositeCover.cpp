// Sample of a query test performed by AI enemies that is used to find cover out-of-sight of the player characters

#include "EnvQueryTest_FindOppositeCover.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_ActorBase.h"
#include "EnvironmentQuery/Contexts/EnvQueryContext_Querier.h"

#include "Math/UnrealMathUtility.h"
#include "Kismet/KismetMathLibrary.h"
#include "Components/ArrowComponent.h"
#include "CoverCollisionBox.h"

// CONSTRUCTOR
UEnvQueryTest_FindOppositeCover::UEnvQueryTest_FindOppositeCover(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
    SetWorkOnFloatValues(false);
    OppositeFrom = UEnvQueryContext_Querier::StaticClass();     // Define the context parameter class
    ValidItemType = UEnvQueryItemType_ActorBase::StaticClass(); // Define the query item class (i.e. the querier objects themselves)

    AngleToleranceLow = -150.0f; // How much leeway to give when finding opposite cover on angle of view. Expanding the range = more tolerant.
    AngleToleranceHigh = -25.0f; 
}


// QUERY TEST CODE
void UEnvQueryTest_FindOppositeCover::RunTest(FEnvQueryInstance& QueryInstance) const
{
    // Check query owner valid
    UObject* QueryOwner = QueryInstance.Owner.Get();
    if (QueryOwner == nullptr)
    {
        return;
    }

    // Get locations of all context items
    TArray<FVector> ContextLocations;
    if (!QueryInstance.PrepareContext(OppositeFrom, ContextLocations))
    {
        return;
    }

    BoolValue.BindData(QueryOwner, QueryInstance.QueryID);
    bool bWantsValid = BoolValue.GetValue();

    // Iterate through context actors specified in the EQS
    for (FEnvQueryInstance::ItemIterator It(this, QueryInstance); It; ++It)
    {
        bool bSatisfiesTest = false;
        
        // Make sure we have a valid CoverCollisionBox that is low cover and can be aimed over
        ACoverCollisionBox* FoundBox = Cast<ACoverCollisionBox>(GetItemActor(QueryInstance, It.GetIndex()));
        if (FoundBox && FoundBox->bLowCover && FoundBox->bCanAimOver && !FoundBox->bBlockAIUse)
        {
            FVector BoxFwdVec = FoundBox->LocationComponent->GetRightVector();
            for (int32 ContextIndex = 0; ContextIndex < ContextLocations.Num(); ContextIndex++)
            {
                // Determine whether the LocComponent on the box is facing towards or away from the context Actor
                FVector BoxToEnemyVec = ContextLocations[ContextIndex] - FoundBox->LocationComponent->GetComponentLocation();
                if (BoxToEnemyVec.Normalize())
                {
                    float CrossResult = UKismetMathLibrary::Cross_VectorVector(BoxToEnemyVec, BoxFwdVec).Z;
                    float AngleBetween = UKismetMathLibrary::DegAcos(UKismetMathLibrary::Dot_VectorVector(BoxFwdVec, BoxToEnemyVec));
                    if (CrossResult < 0) { AngleBetween *= -1;  }
                    if (AngleBetween > AngleToleranceLow && AngleBetween < AngleToleranceHigh) { bSatisfiesTest = true; }
                }
                
                It.SetScore(TestPurpose, FilterType, bSatisfiesTest, bWantsValid);
            }
        }
    }
}


// Title and description that appear on the node in EQS
FText UEnvQueryTest_FindOppositeCover::GetDescriptionTitle() const
{
    return FText::FromString(FString::Printf(TEXT("FindOppositeCover: from %s"), *UEnvQueryTypes::DescribeContext(OppositeFrom).ToString()));
}

FText UEnvQueryTest_FindOppositeCover::GetDescriptionDetails() const
{
    return FText::FromString("Returns true if cover is low, aimable, and opposite context actor");
}
