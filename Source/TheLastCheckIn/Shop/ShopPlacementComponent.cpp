#include "Shop/ShopPlacementComponent.h"
#include "Core/UI/UILayers.h"
#include "Shop/ShopPurchaseSubsystem.h"
#include "Shop/ShopPlacedProductComponent.h"
#include "Core/Interaction/InteractionExtension.h"
#include "Core/Interaction/PlayerInteractionComponent.h"
#include "Combat/CrowbarCombatComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SViewport.h"
#include "Core/GameFlow/HotelPhaseSubsystem.h"
#include "InputCoreTypes.h"
#include "Desktop/DesktopWidget.h"

namespace
{
	bool HasGameFocus(const UShopPlacementComponent* Placement)
	{
		if (!IsValid(Placement) || !Placement->GetWorld()) return false;
		auto* Client = Placement->GetWorld()->GetGameViewport();
		const auto Viewport = Client ? Client->GetGameViewportWidget() : nullptr;
		return Viewport && Viewport->HasAnyUserFocus().IsSet();
	}
	// Intercept only placement controls, before Enhanced Input and the PIE Escape shortcut.
	// Mouse motion and movement keys still reach the normal player input mappings.
	class FShopPlacementInputProcessor final : public IInputProcessor, public TSharedFromThis<FShopPlacementInputProcessor>
	{
	public:
		explicit FShopPlacementInputProcessor(UShopPlacementComponent* InPlacement) : Placement(InPlacement) {}
		
		void RetryFocusAfterPurchase() { bRetryPurchaseFocus = true; }
		
		virtual void Tick(float, FSlateApplication&, TSharedRef<ICursor>) override
		{
			const bool bRetryFocus = bRetryPurchaseFocus;
			bRetryPurchaseFocus = false;

			if (!Placement.IsValid() || !Placement->HasPlacementInteraction())
				return;

			if (HasGameFocus(Placement.Get()))
				return;

			if (bRetryFocus)
			{
				if (auto* PC = Cast<APlayerController>(Placement->GetOwner()))
				{
					FInputModeGameOnly Mode;
					Mode.SetConsumeCaptureMouseDown(false);
					PC->SetInputMode(Mode);
					return;
				}
			}

			Placement->CancelPlacement();
		}
		virtual bool HandleKeyDownEvent(FSlateApplication&, const FKeyEvent& Event) override
		{
			return HandlePress(Event.GetKey(), Event.IsRepeat());
		}
		virtual bool HandleKeyUpEvent(FSlateApplication&, const FKeyEvent& Event) override
		{
			return IsActive() && Placement->HasPlacementInteraction() && IsPlacementKey(Event.GetKey());
		}
		virtual bool HandleMouseButtonDownEvent(FSlateApplication&, const FPointerEvent& Event) override
		{
			if (Event.GetEffectingButton() != EKeys::LeftMouseButton || !IsActive()) return false;
			const auto KeepAlive = AsShared();
			bEatLeftRelease = Placement->HandlePlacementPress();
			return bEatLeftRelease;
		}
		virtual bool HandleMouseButtonDoubleClickEvent(FSlateApplication&, const FPointerEvent& Event) override
		{
			return HandleMouseButtonDownEvent(FSlateApplication::Get(), Event);
		}
		virtual bool HandleMouseButtonUpEvent(FSlateApplication&, const FPointerEvent& Event) override
		{
			if (Event.GetEffectingButton() != EKeys::LeftMouseButton) return false;
			const auto KeepAlive = AsShared();
			const bool bConsumed = bEatLeftRelease;
			bEatLeftRelease = false;
			if (bConsumed && Placement.IsValid()) Placement->HandlePlacementRelease();
			return bConsumed;
		}
	private:
		TWeakObjectPtr<UShopPlacementComponent> Placement;
		bool bEatLeftRelease = false;
		bool bRetryPurchaseFocus = false; 
		static bool IsPlacementKey(FKey Key)
		{
			return Key == EKeys::Escape || Key == EKeys::LeftMouseButton || Key == EKeys::Q || Key == EKeys::E;
		}
		bool IsActive() const
		{
			// Do not intercept editor shortcuts or another UI after it takes focus.
			return HasGameFocus(Placement.Get());
		}
		bool HandlePress(FKey Key, bool bRepeat)
		{
			if (!IsPlacementKey(Key) || !IsActive() || !Placement->HasPlacementInteraction()) return false;
			// Gameplay callbacks may end play and unregister this processor during dispatch.
			const auto KeepAlive = AsShared();
			if (!bRepeat)
			{
				if (Key == EKeys::Escape) Placement->CancelPlacement();
				else if (Key == EKeys::LeftMouseButton) Placement->HandlePlacementPress();
				else Placement->RotatePreview(Key == EKeys::Q ? -15 : 15);
			}
			return true;
		}
	};
}

TSharedRef<SWidget> UShopPlacementWidget::RebuildWidget()
{
	SetIsFocusable(false);
	SetVisibility(ESlateVisibility::HitTestInvisible);
	if (!WidgetTree) WidgetTree = NewObject<UWidgetTree>(this);
	auto* Root = WidgetTree->ConstructWidget<UCanvasPanel>();
	WidgetTree->RootWidget = Root;
	auto* Panel = WidgetTree->ConstructWidget<UBorder>();
	Panel->SetBrushColor(FLinearColor(0, 0, 0, 0.85f));
	Panel->SetVisibility(ESlateVisibility::HitTestInvisible);
	auto* PanelSlot = Root->AddChildToCanvas(Panel);
	PanelSlot->SetAnchors(FAnchors(0.5f, 1.0f));
	PanelSlot->SetAlignment(FVector2D(0.5f, 1.0f));
	PanelSlot->SetPosition(FVector2D(0, -35));
	PanelSlot->SetSize(FVector2D(850, 100));
	Status = WidgetTree->ConstructWidget<UTextBlock>();
	Status->SetJustification(ETextJustify::Center);
	Status->SetAutoWrapText(true);
	Panel->AddChild(Status);
	return Super::RebuildWidget();
}

void UShopPlacementWidget::SetStatus(const FText& Text)
{
	if (Status) Status->SetText(Text);
}

void UShopPlacementWidget::NativeDestruct()
{
	if (Placement) Placement->CancelPlacement();
	Super::NativeDestruct();
}

UShopPlacementComponent::UShopPlacementComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UShopPlacementComponent::BeginPlay()
{
	Super::BeginPlay();
	Purchase = GetWorld()->GetSubsystem<UShopPurchaseSubsystem>();
	EnsurePlacementInput();
	if (auto* Phase = GetWorld()->GetSubsystem<UHotelPhaseSubsystem>())
		Phase->OnPhaseChanged.AddDynamic(this, &UShopPlacementComponent::HandlePhaseChanged);
}

void UShopPlacementComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	CancelPlacement();
	if (PlacementInput && FSlateApplication::IsInitialized()) FSlateApplication::Get().UnregisterInputPreProcessor(PlacementInput);
	PlacementInput.Reset();
	if (auto* Phase = GetWorld()->GetSubsystem<UHotelPhaseSubsystem>())
		Phase->OnPhaseChanged.RemoveDynamic(this, &UShopPlacementComponent::HandlePhaseChanged);
	Super::EndPlay(Reason);
}

void UShopPlacementComponent::HandlePhaseChanged(EHotelPhase OldPhase, EHotelPhase NewPhase)
{
	if (NewPhase == EHotelPhase::Settlement) CancelPlacement();
}

bool UShopPlacementComponent::PurchaseAndPlace(UDataTable* Table, FName ProductId, UUserWidget* ShopToClose, FText& Error)
{
	auto* PC = Cast<APlayerController>(GetOwner());
	auto* Phase = GetWorld()->GetSubsystem<UHotelPhaseSubsystem>();
	if (!PC || !PC->IsLocalPlayerController() || !PC->GetPawn() || HasPlacementInteraction() ||
		(Phase && Phase->GetCurrentPhase() == EHotelPhase::Settlement))
	{
		Error = NSLOCTEXT("Shop", "CannotPlace", "지금은 상품을 배치할 수 없습니다.");
		return false;
	}
	Purchase = GetWorld()->GetSubsystem<UShopPurchaseSubsystem>();
	if (!Purchase || !Purchase->BeginPurchase(Table, ProductId, Error)) return false;
	bOwnsPurchase = true;
	PlacementPawn = PC->GetPawn();
	const auto& Product = Purchase->GetPendingProduct();
	if (!CreatePreview(Product.PreviewMesh.Get(), FVector(Product.Scale)) || !OpenControls())
	{
		CancelPlacement();
		Error = NSLOCTEXT("Shop", "PreviewFailed", "배치 화면을 열 수 없어 구매 금액을 환불했습니다.");
		return false;
	}
	// Release the desktop's input locks, then keep normal movement and mouse-look during placement.
	if (auto* Desktop = Cast<UDesktopWidget>(ShopToClose)) Desktop->CloseDesktop();
	else if (ShopToClose) ShopToClose->RemoveFromParent();
	bPreviousCursor = PC->bShowMouseCursor;
	bChangedCursor = true;
	FInputModeGameOnly Mode;
	Mode.SetConsumeCaptureMouseDown(false);
	PC->SetInputMode(Mode);
	PC->bShowMouseCursor = false;
	EnsurePlacementInput();
	if (PlacementInput)
	{
		auto* Processor =
			static_cast<FShopPlacementInputProcessor*>(PlacementInput.Get());
		Processor->RetryFocusAfterPurchase();
	}
	Yaw = PC->GetControlRotation().Yaw;
	UpdatePreview();
	return true;
}

void UShopPlacementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction)
{
	Super::TickComponent(DeltaTime, TickType, TickFunction);
	if (bHoldingProduct)
	{
		UpdateRelocationHold(DeltaTime);
		return;
	}
	if (!IsPlacing()) return;
	auto* PC = Cast<APlayerController>(GetOwner());
	const bool bTransactionValid = RelocatingProduct ? IsValid(RelocatingProduct) && RelocatingProduct->IsRelocating()
		: bAwaitingFailureDismissal || (Purchase && Purchase->HasPendingPurchase());
	if (!IsValid(Preview) || !bTransactionValid || !PlacementPawn.IsValid() ||
		!PC || PC->GetPawn() != PlacementPawn.Get() || !Controls || !Controls->IsInViewport())
	{
		CancelPlacement();
		return;
	}
	if (!bAwaitingFailureDismissal) UpdatePreview();
}

void UShopPlacementComponent::EnsurePlacementInput()
{
	auto* PC = Cast<APlayerController>(GetOwner());
	if (!PlacementInput && PC && PC->IsLocalPlayerController() && FSlateApplication::IsInitialized())
	{
		PlacementInput = MakeShared<FShopPlacementInputProcessor>(this);
		FSlateApplication::Get().RegisterInputPreProcessor(PlacementInput, 0);
	}
}

bool UShopPlacementComponent::CreatePreview(UStaticMesh* Mesh, const FVector& Scale)
{
	if (!Mesh) return false;
	FActorSpawnParameters Params;
	Params.ObjectFlags |= RF_Transient;
	Params.OverrideLevel = GetWorld()->PersistentLevel;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Preview = GetWorld()->SpawnActor<AStaticMeshActor>(Params);
	if (!Preview) return false;
	auto* Component = Preview->GetStaticMeshComponent();
	Component->SetMobility(EComponentMobility::Movable);
	Component->SetStaticMesh(Mesh);
	Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Component->SetCanEverAffectNavigation(false);
	Preview->SetActorScale3D(Scale);
	Preview->SetActorEnableCollision(false);
	Preview->SetActorTickEnabled(false);
	return true;
}

bool UShopPlacementComponent::OpenControls()
{
	if (!Controls) Controls = CreateWidget<UShopPlacementWidget>(Cast<APlayerController>(GetOwner()));
	if (!Controls) return false;
	Controls->Placement = this;
	if (!Controls->IsInViewport()) Controls->AddToViewport(UILayer::ShopPlacement);
	return true;
}

UShopPlacedProductComponent* UShopPlacementComponent::FindRelocationTarget() const
{
	auto* PC = Cast<APlayerController>(GetOwner());
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!Pawn || PC->IsMoveInputIgnored() || PC->IsLookInputIgnored()) return nullptr;
	if (auto* Phase = GetWorld()->GetSubsystem<UHotelPhaseSubsystem>())
		if (Phase->GetCurrentPhase() == EHotelPhase::Settlement) return nullptr;
	if (auto* Combat = Pawn->FindComponentByClass<UCrowbarCombatComponent>()) if (Combat->IsEquipped()) return nullptr;
	if (auto* Interaction = Pawn->FindComponentByClass<UPlayerInteractionComponent>()) if (Interaction->HasActiveInteraction()) return nullptr;
	for (UActorComponent* Component : Pawn->GetComponents())
		if (const auto* Extension = Cast<IInteractionExtension>(Component)) if (Extension->BlocksFocus()) return nullptr;
	FVector Start;
	FRotator View;
	PC->GetPlayerViewPoint(Start, View);
	FHitResult Hit;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(ShopRelocationTarget), false, Pawn);
	if (!GetWorld()->LineTraceSingleByChannel(Hit, Start, Start + View.Vector() * 350, ECC_Visibility, Query)) return nullptr;
	AActor* Actor = Hit.GetActor();
	auto* Product = Actor ? Actor->FindComponentByClass<UShopPlacedProductComponent>() : nullptr;
	return Product && Product->CanRelocate() ? Product : nullptr;
}

bool UShopPlacementComponent::HandlePlacementPress()
{
	if (IsPlacing())
	{
		if (!RelocatingProduct) ConfirmPlacement();
		return true;
	}
	if (bHoldingProduct) return true;
	UShopPlacedProductComponent* Target = FindRelocationTarget();
	if (!Target) return false; // Ordinary world interactions and equipped tools retain their input.
	HoldCandidate = Target;
	bHoldingProduct = true;
	HoldElapsed = 0;
	PlacementPawn = CastChecked<APlayerController>(GetOwner())->GetPawn();
	if (!OpenControls()) { ClosePlacement(); return true; }
	Controls->SetStatus(NSLOCTEXT("Shop", "HoldToRelocate", "좌클릭을 꾹 누르면 재배치합니다.\n누른 채 이동 · 버튼을 놓으면 배치 · Esc: 원래 자리로 복원"));
	return true;
}

void UShopPlacementComponent::UpdateRelocationHold(float DeltaTime)
{
	auto* PC = Cast<APlayerController>(GetOwner());
	if (!PC || !PlacementPawn.IsValid() || PC->GetPawn() != PlacementPawn.Get() || !HoldCandidate.IsValid() ||
		FindRelocationTarget() != HoldCandidate.Get() || !Controls || !Controls->IsInViewport())
	{
		CancelPlacement();
		return;
	}
	HoldElapsed += FMath::Max(0.0f, DeltaTime);
	const float Duration = FMath::Max(0.1f, RelocationHoldSeconds);
	if (HoldElapsed >= Duration) { BeginRelocation(); return; }
	Controls->SetStatus(FText::Format(NSLOCTEXT("Shop", "RelocationProgress", "잡는 중… {0}%\n좌클릭을 계속 누르세요. Esc: 취소"),
		FText::AsNumber(FMath::FloorToInt(HoldElapsed / Duration * 100))));
}

bool UShopPlacementComponent::BeginRelocation()
{
	auto* Product = HoldCandidate.Get();
	if (!Product || !Product->BeginRelocation()) { ClosePlacement(); return false; }
	RelocatingProduct = Product;
	bHoldingProduct = false;
	HoldCandidate.Reset();
	AActor* Actor = Product->GetOwner();
	if (!CreatePreview(Product->GetPreviewMesh(), Actor->GetActorScale3D()) || !OpenControls())
	{
		CancelPlacement();
		return false;
	}
	Yaw = Actor->GetActorRotation().Yaw;
	UpdatePreview();
	return true;
}

bool UShopPlacementComponent::HandlePlacementRelease()
{
	if (bHoldingProduct) { ClosePlacement(); return true; }
	if (RelocatingProduct) { ConfirmPlacement(); return true; }
	return false;
}

void UShopPlacementComponent::UpdatePreview()
{
	bValidPlacement = false;
	auto* PC = Cast<APlayerController>(GetOwner());
	if (!PC || !IsValid(Preview) || !PlacementPawn.IsValid()) return;
	FVector Origin, Direction;
	int32 Width = 0, Height = 0;
	PC->GetViewportSize(Width, Height);
	// Use a fixed point relative to the camera, not the hidden cursor's old desktop position.
	if (Width <= 0 || Height <= 0 || !PC->DeprojectScreenPositionToWorld(Width * 0.60f, Height * 0.65f, Origin, Direction))
	{
		FRotator View;
		PC->GetPlayerViewPoint(Origin, View);
		const FRotationMatrix Axes(View);
		Direction = (View.Vector() + Axes.GetUnitAxis(EAxis::Y) * 0.3f - Axes.GetUnitAxis(EAxis::Z) * 0.5f).GetSafeNormal();
	}
	FCollisionQueryParams Query(SCENE_QUERY_STAT(ShopPlacement), false, PlacementPawn.Get());
	Query.AddIgnoredActor(Preview);
	if (IsValid(RelocatingProduct)) Query.AddIgnoredActor(RelocatingProduct->GetOwner());
	FHitResult Hit;
	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Origin, Origin + Direction * 800, ECC_Visibility, Query);
	const FRotator Rotation(0, Yaw, 0);
	const auto Bounds = Preview->GetStaticMeshComponent()->GetStaticMesh()->GetBounds();
	const FVector Scale = Preview->GetActorScale3D().GetAbs();
	const FVector Target = bHit ? Hit.ImpactPoint : Origin + Direction * 180;
	const FVector Pivot = Target + FVector(0, 0, 1 + (Bounds.BoxExtent.Z - Bounds.Origin.Z) * Scale.Z);
	Preview->SetActorLocationAndRotation(Pivot, Rotation);
	if (bHit && Hit.ImpactNormal.Z >= 0.7f && FVector::Dist2D(Target, PlacementPawn->GetActorLocation()) <= 350)
	{
		FCollisionQueryParams OverlapQuery(SCENE_QUERY_STAT(ShopOverlap), false, Preview);
		if (IsValid(RelocatingProduct)) OverlapQuery.AddIgnoredActor(RelocatingProduct->GetOwner());
		const FVector Extent = (Bounds.BoxExtent * Scale - FVector(2)).ComponentMax(FVector(1));
		const FVector Center = Preview->GetActorTransform().TransformPosition(Bounds.Origin);
		// Room/quest trigger volumes overlap pawns and must not count as solid furniture or walls.
		bValidPlacement = !GetWorld()->OverlapBlockingTestByChannel(Center, Rotation.Quaternion(), ECC_Pawn,
			FCollisionShape::MakeBox(Extent), OverlapQuery);
	}
	if (Controls && RelocatingProduct)
		Controls->SetStatus(bValidPlacement
			? NSLOCTEXT("Shop", "RelocateHelp", "좌클릭을 놓으면 배치   |   Q / E: 회전   |   Esc: 원래 자리로 복원\n누른 채 자유롭게 이동하고 시점을 돌릴 수 있습니다.")
			: NSLOCTEXT("Shop", "RelocateBlocked", "배치 불가: 가까운 빈 바닥을 선택하세요.\n여기서 버튼을 놓거나 Esc를 누르면 원래 자리로 돌아갑니다."));
	else if (Controls)
		Controls->SetStatus(bValidPlacement
			? NSLOCTEXT("Shop", "FreeLookPlaceHelp", "좌클릭: 배치 확정   |   Q / E: 회전   |   Esc: 취소 및 전액 환불\n평소처럼 이동하고 마우스로 시점을 돌려 배치할 위치를 선택하세요.")
			: NSLOCTEXT("Shop", "FreeLookInvalidLocation", "배치 불가: 이동하거나 시점을 돌려 가까운 빈 바닥을 선택하세요.\nQ / E: 회전   |   Esc: 취소 및 전액 환불"));
}

void UShopPlacementComponent::RotatePreview(float Degrees)
{
	Yaw = FMath::UnwindDegrees(Yaw + Degrees);
	UpdatePreview();
}

bool UShopPlacementComponent::ConfirmPlacement()
{
	if (RelocatingProduct)
	{
		UpdatePreview();
		const bool bMoved = bValidPlacement && IsValid(Preview) && IsValid(RelocatingProduct) &&
			RelocatingProduct->FinishRelocation(Preview->GetActorTransform());
		ClosePlacement(); // Also rolls back an invalid release.
		return bMoved;
	}
	if (!Purchase || !Purchase->HasPendingPurchase()) return false;
	UpdatePreview(); // Validate again at the moment of confirmation.
	if (!bValidPlacement || !Purchase || !IsValid(Preview)) return false;
	FText Error;
	AActor* Product = Purchase->ConfirmPurchase(Preview->GetActorTransform(), Error);
	if (!Product && Controls)
	{
		// Keep the refunded failure visible until Esc; there is no active reservation now.
		Controls->SetStatus(Error);
		bAwaitingFailureDismissal = true;
		if (Preview) Preview->SetActorHiddenInGame(true);
		return false;
	}
	ClosePlacement();
	return Product != nullptr;
}

void UShopPlacementComponent::CancelPlacement()
{
	if (Purchase && bOwnsPurchase) Purchase->CancelPurchase();
	ClosePlacement();
}

void UShopPlacementComponent::ClosePlacement()
{
	if (IsValid(RelocatingProduct)) RelocatingProduct->CancelRelocation();
	RelocatingProduct = nullptr;
	HoldCandidate.Reset();
	bHoldingProduct = false;
	HoldElapsed = 0;
	bAwaitingFailureDismissal = false;
	bOwnsPurchase = false;
	if (IsValid(Preview)) Preview->Destroy();
	Preview = nullptr;
	bValidPlacement = false;
	PlacementPawn.Reset();
	auto* PC = Cast<APlayerController>(GetOwner());
	if (Controls)
	{
		Controls->Placement = nullptr;
		Controls->RemoveFromParent();
		Controls = nullptr;
	}
	if (PC && bChangedCursor)
	{
		auto* Client = GetWorld()->GetGameViewport();
		const auto Viewport = Client ? Client->GetGameViewportWidget() : nullptr;
		if (Viewport && Viewport->HasAnyUserFocus().IsSet()) PC->bShowMouseCursor = bPreviousCursor;
	}
	bChangedCursor = false;
}
