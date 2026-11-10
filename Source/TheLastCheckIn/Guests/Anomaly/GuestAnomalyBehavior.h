#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GuestAnomalyBehavior.generated.h"

class AGuestCharacter;
class UGuestAnomalyComponent;
class UGuestAnomalyDefinition;

UCLASS(Abstract, Blueprintable, BlueprintType)
class THELASTCHECKIN_API UGuestAnomalyBehavior : public UActorComponent
{
    GENERATED_BODY()

public:
    UGuestAnomalyBehavior();

    void InitializeBehavior(
        UGuestAnomalyComponent* InHost,
        AGuestCharacter* InGuest,
        UGuestAnomalyDefinition* InDefinition,
        AActor* InContext);

    // 자식 행동이 필요한 대상이나 설정을 검사
    virtual bool CanStart() const;

    void StartBehavior();
    void StopBehavior();

    void ReceiveSignal(
        FName Signal,
        AActor* InstigatorActor);

    UFUNCTION(BlueprintPure, Category = "Anomaly")
    bool IsRunning() const
    {
        return bRunning;
    }

protected:
    UPROPERTY(Transient, BlueprintReadOnly, Category = "Anomaly")
    TObjectPtr<AGuestCharacter> Guest;

    UPROPERTY(Transient, BlueprintReadOnly, Category = "Anomaly")
    TObjectPtr<UGuestAnomalyDefinition> Definition;
    
    UPROPERTY(Transient, BlueprintReadOnly, Category = "Anomaly")
    TObjectPtr<AActor> ContextActor;

    UFUNCTION(BlueprintNativeEvent, Category = "Anomaly")
    void OnStarted();

    virtual void OnStarted_Implementation();

    UFUNCTION(BlueprintNativeEvent, Category = "Anomaly")
    void OnStopped();

    virtual void OnStopped_Implementation();

    UFUNCTION(BlueprintNativeEvent, Category = "Anomaly")
    void OnSignalReceived(FName Signal, AActor* InstigatorActor);

    virtual void OnSignalReceived_Implementation(FName Signal, AActor* InstigatorActor);

    UFUNCTION(BlueprintCallable, Category = "Anomaly")
    void RequestResolved();

    UFUNCTION(BlueprintCallable, Category = "Anomaly")
    void RequestCancelled();

private:
    UPROPERTY(Transient)
    TObjectPtr<UGuestAnomalyComponent> Host;

    bool bRunning = false;
    int32 ReceivedSignals = 0;
};