#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/Interaction/Interactable.h"
#include "Memo/MemoType.h"
#include "MemoPaperActor.generated.h"

class UStaticMeshComponent;
class UMemoSubsystem;

UCLASS()
class THELASTCHECKIN_API AMemoPaperActor
	: public AActor
	, public IInteractable
{
	GENERATED_BODY()

public:
	AMemoPaperActor();

	virtual bool CanInteract_Implementation(
		AActor* Interactor) const override;

	virtual FText GetInteractPrompt_Implementation(
		AActor* Interactor) const override;

	virtual void Interact_Implementation(
		AActor* Interactor) override;

	// BP에서 읽기 화면을 정상적으로 표시한 뒤 호출합니다.
	UFUNCTION(BlueprintCallable, Category="Memo")
	void ConfirmRead();

protected:
	virtual void BeginPlay() override;

	virtual void EndPlay(
		const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Memo")
	TObjectPtr<UStaticMeshComponent> PaperMesh;

	// 데이터 테이블과 행 이름을 함께 지정합니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Memo")
	FDataTableRowHandle Memo;

	// BP에서 WBP_MemoReader 등을 생성하고 내용을 전달합니다.
	UFUNCTION(BlueprintImplementableEvent, Category="Memo")
	void ShowMemo(const FText& Title, const FText& Body);

private:
	UFUNCTION()
	void HandleMemoStateChanged(
		FName ChangedMemoId,
		EMemoState NewState);

	void RefreshMemoVisibility();

	UPROPERTY(Transient)
	TObjectPtr<UMemoSubsystem> MemoSystem;
};