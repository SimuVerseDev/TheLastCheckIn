# 공용 카트·물건 상호작용 테스트

## 에셋 구성

- `Test/Character/BP_Cart_MainTestCharacter`: `BP_FirstPersonCharacter` 자식. 공용 `PlayerInteractionComponent`와 테스트 조준점을 사용합니다.
- `Test/GameMode/BP_Cart_MainTestGameMode`: 메인 게임모드 자식. 위 캐릭터와 메인 PlayerController를 사용합니다.
- `Test/Maps/Lvl_Cart_test`: 위 테스트 게임모드를 사용합니다.
- 기존 테스트 캐릭터는 비교용으로 남아 있습니다. 전용 E 이벤트와 카트 이동 Tick 호출은 제거했습니다.

## 현재 구현 위치

입력 경로는 `IMC_Default` → `IA_Interact` → `PlayerInteractionComponent` → 대상의 `Interact`입니다.

카트 본체는 `BP_CleaningCart`가 잡기/놓기와 운반 Tick을 관리합니다. 개별 물건은 부모 `BP_CartPickupBase`가 Interactable을 구현하며 `ItemToggle`, `ItemUpdateCarry`, `ItemRelease`, `TryReturnItem`에서 상태와 이동, 반납을 관리합니다. 대걸레는 기존 손 소켓 및 GetHandCarryTransform 보정을 사용합니다.

`IsInteractionActive`로 운반 중인 대상을 유지하므로 다른 곳을 바라보아도 E로 놓을 수 있습니다. 카트 본체는 Visibility Overlap, 물건은 Block을 사용하며 공용 컴포넌트의 Multi Sweep이 물건을 우선 선택하고 카트 겹침을 보조 대상으로 처리합니다. 추적은 첫 차단 충돌에서 멈춥니다.

기존 `Test/UI/WBP_CartPickupPrompt`의 모양을 유지했습니다. 물건의 `ShowItemPrompt`/`HideItemPrompt`가 공용 포커스 이벤트에 연결되어 있으며, 들고 있는 동안 숨기고 포커스가 끝나면 제거합니다. 새 물건도 BP_CartPickupBase를 상속하면 같은 경로를 사용합니다.

## 플레이 방법

1. `Lvl_Cart_test`에서 Play합니다.
2. 물건을 가까이서 바라보면 기존 `[E 집기]`가 표시됩니다. E로 집습니다.
3. 이동하거나 회전하며 따라오는지 확인합니다. 다른 곳을 바라보고 E를 누르면 놓습니다.
4. 원래 카트 슬롯을 바라보고 E를 누르면 반납합니다. 다른 물건의 반납 슬롯은 원래 자리를 조준하라는 안내를 표시합니다.
5. 카트 본체를 조준하면 E로 카트를 잡고, 다시 E로 놓습니다.

## 검증 결과 (2026-11-02)

- 에디터 정상 종료 후 C++ 빌드: UnrealBuildTool `Result: Succeeded` 확인.
- PickupBase, CleaningCart, Cube, Mop, Cloth, TrashBin 블루프린트 컴파일 및 저장 완료.
- 메인 캐릭터의 자식인 MainTestCharacter로 실제 키보드 E 입력 검증.
- 대걸레 집기 및 시선을 돌린 상태의 놓기, 기존 UI 표시/숨김 확인.
- 일반 걸레 집기, 운반자 이동에 따른 위치 갱신, 원래 슬롯 반납, 재집기, 시선 밖 놓기 확인.
- 반납 후 `IsStored=true`, 물리 시뮬레이션 꺼짐, 슬롯 부모 연결, 액터 위치와 슬롯 위치 일치, PhysicsHandle 해제를 확인.
- 감지 변경 후 카트 본체 잡기/시선 밖 놓기 회귀 검증 완료.

진단용 위치 변경은 PIE 인스턴스에만 적용했습니다. 걸레 반납 재검증에서는 대걸레 충돌의 간섭을 분리하기 위해 PIE의 대걸레만 다른 위치로 옮겼습니다. 배치된 물건의 충돌 크기에 따라 앞 물건이 먼저 선택될 수 있습니다. 모든 물건 종류의 개별 물리 동작과 멀티플레이는 이번에 검증하지 않았습니다.

메인 BP_FirstPersonCharacter 원본은 수정하지 않았습니다. 실제 메인 맵에서 사용하는 캐릭터에 공용 PlayerInteractionComponent와 IA_Interact 입력 매핑이 있으면 같은 경로로 동작합니다. 테스트용 자식에는 이미 연결되어 있습니다. 기존 LMB Housekeeping 집기 시스템은 변경하지 않았습니다.