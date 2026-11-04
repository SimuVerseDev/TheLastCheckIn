#include "Housekeeping/Objects/Stain/HKWipeComponent.h"
#include "Housekeeping/Objects/Stain/HKStainActor.h"
#include "Housekeeping/Objects/Stain/HKStainDirtComponent.h"
#include "Housekeeping/Objects/Stain/HKWipeToolComponent.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

UHKWipeComponent::UHKWipeComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void UHKWipeComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	EndWipe();
	Super::EndPlay(EndPlayReason);
}

void UHKWipeComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	AHKStainActor* StainActor = Stain.Get();
	const UHKWipeToolComponent* ToolComponent = Tool.Get();
	UPrimitiveComponent* Body = GetToolBody();
	const APlayerController* PC = GetPlayerController();
	if (!StainActor || !ToolComponent || !Body || !PC
		|| ToolComponent->GetUser() != GetOwner() || StainActor->GetStain()->GetAmount() <= 0.0f)
	{
		EndWipe();
		return;
	}

	/*
	시선 기준으로 얼룩 표면 위의 가로·세로 축 - 마우스 위로 밀면 화면 위(바닥이면 앞)로 이동
	*/
	const FVector Normal = StainActor->GetSurfaceNormal();
	FVector Right = FVector::VectorPlaneProject(PC->GetControlRotation().RotateVector(FVector::RightVector), Normal).GetSafeNormal();
	if (Right.IsNearlyZero())
	{
		Right = FVector::VectorPlaneProject(FVector::RightVector, Normal).GetSafeNormal();
	}
	const FVector Up = FVector::CrossProduct(Right, Normal);

	float MouseX = 0.0f;
	float MouseY = 0.0f;
	PC->GetInputMouseDelta(MouseX, MouseY);
	FVector2D NewOffset = WipeOffset + FVector2D(MouseX, MouseY) * MouseSensitivity;
	const float Radius = StainActor->GetWipeRadius();
	if (NewOffset.Size() > Radius)
	{
		NewOffset = NewOffset.GetSafeNormal() * Radius;
	}
	const float Moved = FVector2D::Distance(NewOffset, WipeOffset);
	WipeOffset = NewOffset;

	const FVector Location = StainActor->GetWipeCenter() + Right * WipeOffset.X + Up * WipeOffset.Y + Normal * ToolComponent->GetSurfaceOffset();
	const FRotator Rotation = FRotationMatrix::MakeFromZX(Normal, Up).Rotator();
	Body->SetWorldLocationAndRotation(Location, Rotation, false, nullptr, ETeleportType::TeleportPhysics);

	StainActor->GetStain()->AddWipe(Moved * ToolComponent->GetWipeMultiplier());
}

bool UHKWipeComponent::BeginWipe(AHKStainActor* InStain, UHKWipeToolComponent* InTool)
{
	APlayerController* PC = GetPlayerController();
	if (IsWiping() || !InStain || !InTool || !PC)
	{
		return false;
	}
	Stain = InStain;
	Tool = InTool;
	UPrimitiveComponent* Body = GetToolBody();
	if (!Body)
	{
		Stain.Reset();
		Tool.Reset();
		return false;
	}
	WipeOffset = FVector2D::ZeroVector;
	Body->SetSimulatePhysics(false);
	PC->SetIgnoreLookInput(true);
	SetComponentTickEnabled(true);
	return true;
}

void UHKWipeComponent::EndWipe()
{
	if (!IsWiping())
	{
		return;
	}
	if (UPrimitiveComponent* Body = GetToolBody())
	{
		Body->SetSimulatePhysics(true);
	}
	if (APlayerController* PC = GetPlayerController())
	{
		PC->SetIgnoreLookInput(false);
	}
	Stain.Reset();
	Tool.Reset();
	SetComponentTickEnabled(false);
}

UPrimitiveComponent* UHKWipeComponent::GetToolBody() const
{
	const UHKWipeToolComponent* ToolComponent = Tool.Get();
	const AActor* ToolActor = ToolComponent ? ToolComponent->GetOwner() : nullptr;
	return ToolActor ? Cast<UPrimitiveComponent>(ToolActor->GetRootComponent()) : nullptr;
}

APlayerController* UHKWipeComponent::GetPlayerController() const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	return Pawn ? Pawn->GetController<APlayerController>() : nullptr;
}
