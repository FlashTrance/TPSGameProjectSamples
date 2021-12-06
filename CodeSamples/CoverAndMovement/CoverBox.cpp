// Interacting with cover is facilitated largely by getting information from "CoverBox" actors placed in the world, which tell the Character where
// the normal plane is, whether they can aim/vault/etc., among other things. Below is an sample of the parent CoverBox class.
//--------------------------------------------------------------------

// COVER BOX CLASS (CPP)
#include "CoverCollisionBox.h"
#include "CoverEdgeBox.h"
#include "MGCharacter.h"
#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"

#include "Engine/GameEngine.h"
#include "Kismet\KismetMathLibrary.h"

// Constructor
ACoverCollisionBox::ACoverCollisionBox()
{
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;

	// COLLISION
	CoverCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("BoxCollision"));
	CoverCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
	CoverCollision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	CoverCollision->SetCollisionResponseToChannel(ECC_COVERTRACE, ECR_Block);
	CoverCollision->SetBoxExtent(FVector(30.0f, 50.0f, 50.0f));
	RootComponent = CoverCollision;

	// LOCATION
	LocationComponent = CreateDefaultSubobject<UArrowComponent>(TEXT("LocationComponent"));
	LocationComponent->SetupAttachment(CoverCollision);
}

// BEGIN PLAY()
void ACoverCollisionBox::BeginPlay()
{
	Super::BeginPlay();

	// Bind overlap event
	CoverCollision->OnComponentBeginOverlap.AddDynamic(this, &ACoverCollisionBox::OnActorBeginOverlap);
}


// OVERLAP EVENTS
void ACoverCollisionBox::OnActorBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// Make sure we're not an EdgeBox!
	ACoverEdgeBox* ThisActor = Cast<ACoverEdgeBox>(this);
	if (!ThisActor)
	{
		// Get reference to player
		AMGCharacter* PlayerChar = Cast<AMGCharacter>(OtherActor);
		if (PlayerChar && PlayerChar->IsLocallyControlled() && PlayerChar->bCoverSlideComplete)
		{
			// Check for adjacent wall
			if ((UKismetMathLibrary::Dot_VectorVector(PlayerChar->CurrentLocationComponent->GetForwardVector(), LocationComponent->GetForwardVector()) < 0.99
				&& !PlayerChar->bMovingToWall && !PlayerChar->bVaulting && !PlayerChar->bPeekAiming))
			{
				PlayerChar->CurrentLocationComponent = LocationComponent;
				if (!PlayerChar->bIsAim)
				{
					PlayerChar->MoveToAdjacentWall();
				}
			}
		}
	}
}


// ------------------------------------------------------ //


// COVER BOX CLASS (HEADER)
class UBoxComponent;
class UArrowComponent;
class ACoverEdgeBox;

UCLASS()
class INNOCUOUSTITLE_API ACoverCollisionBox : public AActor
{
	GENERATED_BODY()
	
// COMPONENTS
public:	

	// Constructor
	ACoverCollisionBox();

	// Components
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UBoxComponent* CoverCollision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UArrowComponent* LocationComponent;

protected:

	virtual void BeginPlay() override;

// FUNCTIONS AND VARIABLES
public:	
	
	// OVERRIDES
	UFUNCTION()
	void OnActorBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	// VARIABLES
	UPROPERTY(EditAnywhere, Category = "PlayerActions")
	bool bLowCover;

	UPROPERTY(EditAnywhere, Category = "PlayerActions")
	bool bCanAimOver;

	UPROPERTY(EditAnywhere, Category = "PlayerActions")
	bool bCanVaultOver;

	// Set true to prevent AI from taking cover on this box
	UPROPERTY(EditAnywhere, Category = "PlayerActions")
	bool bBlockAIUse; 

	// Set this to the cover box we should vault to
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerActions")
	ACoverCollisionBox* VaultReference; 

	// Set this on cover boxes that are next to an edge
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerActions")
	ACoverEdgeBox* EdgeReference; 

	// Set this on cover boxes with an edge box on both sides (this should be the LEFT edge)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerActions")
	ACoverEdgeBox* EdgeReferenceAlt; 
};
