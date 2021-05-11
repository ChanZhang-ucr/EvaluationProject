// Copyright Epic Games, Inc. All Rights Reserved.

#include "EvaluationGameCharacter.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/GameplayStatics.h"

//////////////////////////////////////////////////////////////////////////
// AEvaluationGameCharacter

AEvaluationGameCharacter::AEvaluationGameCharacter()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	// set our turn rates for input
	BaseTurnRate = 45.f;
	BaseLookUpRate = 45.f;

	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true; // Character moves in the direction of input...	
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 540.0f, 0.0f); // ...at this rotation rate
	GetCharacterMovement()->JumpZVelocity = 600.f;
	GetCharacterMovement()->AirControl = 0.2f;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 300.0f; // The camera follows at this distance behind the character	
	CameraBoom->bUsePawnControlRotation = true; // Rotate the arm based on the controller

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName); // Attach the camera to the end of the boom and let the boom adjust to match the controller orientation
	FollowCamera->bUsePawnControlRotation = false; // Camera does not rotate relative to arm

	// Climbing
	bIsCliming = false;
	CollisionCounter = 0;
	ClimbSpeedUpValue = 100.f;
	CSphereRadius = 16.f;
	ClimbEndingForceMultiplier = 4.5f;

	CCS_Init(CollisionDown, CSphereRadius, FVector(60.f, 0.f, -80.f), "CSphereDown", "Trace0", true);
	CCS_Init(CollisionUp, CSphereRadius, FVector(60.f, 0.f, 80.f), "CSphereUp", "Trace1", true);
	CCS_Init(CollisionLeft, CSphereRadius, FVector(60.f, -50.f, 0.f), "CSphereLeft", "Trace2", true);
	CCS_Init(CollisionRight, CSphereRadius, FVector(60.f, 50.f, 0.f), "CSphereRight", "Trace3", true);

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named MyCharacter (to avoid direct content references in C++)
}

/** Begin Play() */
void AEvaluationGameCharacter::BeginPlay()
{
	Super::BeginPlay();
}

//////////////////////////////////////////////////////////////////////////
// Input

void AEvaluationGameCharacter::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	// Set up gameplay key bindings
	check(PlayerInputComponent);
	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ACharacter::Jump);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &ACharacter::StopJumping);

	PlayerInputComponent->BindAxis("MoveForward", this, &AEvaluationGameCharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &AEvaluationGameCharacter::MoveRight);

	// We have 2 versions of the rotation bindings to handle different kinds of devices differently
	// "turn" handles devices that provide an absolute delta, such as a mouse.
	// "turnrate" is for devices that we choose to treat as a rate of change, such as an analog joystick
	PlayerInputComponent->BindAxis("Turn", this, &APawn::AddControllerYawInput);
	PlayerInputComponent->BindAxis("TurnRate", this, &AEvaluationGameCharacter::TurnAtRate);
	PlayerInputComponent->BindAxis("LookUp", this, &APawn::AddControllerPitchInput);
	PlayerInputComponent->BindAxis("LookUpRate", this, &AEvaluationGameCharacter::LookUpAtRate);

	// handle touch devices
	PlayerInputComponent->BindTouch(IE_Pressed, this, &AEvaluationGameCharacter::TouchStarted);
	PlayerInputComponent->BindTouch(IE_Released, this, &AEvaluationGameCharacter::TouchStopped);

	// VR headset functionality
	PlayerInputComponent->BindAction("ResetVR", IE_Pressed, this, &AEvaluationGameCharacter::OnResetVR);
}


void AEvaluationGameCharacter::OnResetVR()
{
	// If EvaluationGame is added to a project via 'Add Feature' in the Unreal Editor the dependency on HeadMountedDisplay in EvaluationGame.Build.cs is not automatically propagated
	// and a linker error will result.
	// You will need to either:
	//		Add "HeadMountedDisplay" to [YourProject].Build.cs PublicDependencyModuleNames in order to build successfully (appropriate if supporting VR).
	// or:
	//		Comment or delete the call to ResetOrientationAndPosition below (appropriate if not supporting VR)
	UHeadMountedDisplayFunctionLibrary::ResetOrientationAndPosition();
}

void AEvaluationGameCharacter::TouchStarted(ETouchIndex::Type FingerIndex, FVector Location)
{
		Jump();
}

void AEvaluationGameCharacter::TouchStopped(ETouchIndex::Type FingerIndex, FVector Location)
{
		StopJumping();
}

void AEvaluationGameCharacter::TurnAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerYawInput(Rate * BaseTurnRate * GetWorld()->GetDeltaSeconds());
}

void AEvaluationGameCharacter::LookUpAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerPitchInput(Rate * BaseLookUpRate * GetWorld()->GetDeltaSeconds());
}

void AEvaluationGameCharacter::MoveForward(float Value)
{
	if ((Controller != nullptr) && (Value != 0.f))
	{
		if (bIsCliming && Value > 0.f)
		{
			LaunchCharacter(FVector(0.f, 0.f, Value * ClimbSpeedUpValue), true, true);
		}
		else
		{
			// find out which way is forward
			const FRotator Rotation = Controller->GetControlRotation();
			const FRotator YawRotation(0, Rotation.Yaw, 0);

			// get forward vector
			const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
			AddMovementInput(Direction, Value);
		}
	}
}

void AEvaluationGameCharacter::MoveRight(float Value)
{
	if ( (Controller != nullptr) && (Value != 0.f) )
	{
		// find out which way is right
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);
	
		// get right vector 
		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
		// add movement in that direction
		AddMovementInput(Direction, Value);
	}
}

// climbing
void AEvaluationGameCharacter::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (CollisionCounter < 4) {
		CollisionCounter++;
	}
	if (CollisionCounter >= 3) {
		bIsCliming = true;
	}
}

void AEvaluationGameCharacter::OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, TEXT("end overlap event"));
	if (CollisionCounter > 0) {
		CollisionCounter--;
	}
	if (CollisionCounter <= 3) {
		LaunchCharacter(FVector(0.f, 0.f, ClimbEndingForceMultiplier * ClimbSpeedUpValue), true, true);
		bIsCliming = false;
	}
}

void AEvaluationGameCharacter::CCS_Init(USphereComponent* Collision, float radius, FVector Location, FName ObjName, FName CollisionProfileName, bool HideInGame)
{
	Collision = CreateDefaultSubobject<USphereComponent>(ObjName);
	Collision->SetupAttachment(RootComponent);
	Collision->SetSphereRadius(radius);
	Collision->SetRelativeLocation(Location);
	Collision->SetCollisionProfileName(CollisionProfileName);
	Collision->SetHiddenInGame(HideInGame);

	Collision->OnComponentBeginOverlap.AddDynamic(this, &AEvaluationGameCharacter::OnOverlapBegin);
	Collision->OnComponentEndOverlap.AddDynamic(this, &AEvaluationGameCharacter::OnOverlapEnd);
}

void AEvaluationGameCharacter::Tick(float perSecondTick)
{

}
