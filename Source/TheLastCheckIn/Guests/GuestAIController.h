#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "GuestAIController.generated.h"

UCLASS()
class THELASTCHECKIN_API AGuestAIController : public AAIController
{
	GENERATED_BODY()
	
public:
	UFUNCTION(BlueprintCallable, Category = "Guests|AI")
	bool SetGuestMoveTarget(AActor* NewTarget);
	
	// 풀에서 꺼낸 손님의 AI 시작
	bool StartGuestAI(AActor* StartTarget);

	// 풀에 반환할 손님의 AI 정지
	void StopGuestAI();

protected:
	virtual void OnPossess(APawn* InPawn) override;
};