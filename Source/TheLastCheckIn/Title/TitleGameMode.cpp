#include "Title/TitleGameMode.h"
#include "Title/TitlePlayerController.h"

ATitleGameMode::ATitleGameMode()
{
	PlayerControllerClass = ATitlePlayerController::StaticClass();
	DefaultPawnClass = nullptr;
}