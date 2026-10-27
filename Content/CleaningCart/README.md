# CleaningCart 프로토타입

## 실행

`/Game/CleaningCart/Test/Maps/Lvl_Cart_test`를 열고 Play를 누릅니다.
시작 지점 앞의 작은 카트가 상호작용용 카트입니다. 기존 큰 메시도 참고용으로 남아 있습니다.

- 카트 본체를 가까이에서 조준하고 **E**: 카트를 잡고 이동합니다.
- 잡은 상태에서 **E**: 카트를 놓습니다.
- 카트를 놓은 뒤 위의 큐브를 조준하면 중앙 커서 아래에 **[E 집기]**가 표시됩니다. **E**를 누르면 큐브를 집습니다.
- 집기 안내는 손이 비어 있고, 300cm 이내의 집을 수 있는 BP_CartCube를 조준할 때만 표시됩니다. 시선을 돌리거나 물품을 들면 숨겨집니다.
- 큐브를 든 상태에서 **원래 빈 자리를 조준하고 E**: 그 물품이 원래 포인트에 돌아갑니다. 카트를 이동해도 새 위치의 포인트를 따라갑니다.
- 큐브를 든 상태에서 다른 빈 자리를 조준하고 **E**: 원래 자리를 조준하라는 안내가 나오며 계속 들고 있습니다.
- 큐브를 든 상태에서 빈 자리를 조준하지 않고 **E**: 바닥에 놓습니다. 바닥에서도 다시 집을 수 있습니다.
- 상호작용 거리는 카메라 기준 300cm입니다. 카트 본체의 큰 충돌 상자 안에 있는 큐브와 반환 영역도 조준할 수 있습니다. Visibility를 차단하는 벽 등은 통과하지 않습니다.
- 카트를 잡고 있는 동안 물품을 꺼낼 수 없습니다. 한 번에 하나만 잡습니다.

큐브 3개는 플레이 시작 시 생성됩니다. 물품을 집으면 원래 자리가 비고, 그 자리의 반환 감지 박스가 활성화됩니다. 반환하면 박스를 다시 비활성화합니다. 비어 있는 자리에서 맨손으로 E를 눌러도 카트를 잡지 않습니다.

## 편집할 에셋

| 경로 (CleaningCart 기준) | 역할 |
| --- | --- |
| `Blueprints/BP_CleaningCart` | 실제 카트 Actor 블루프린트 |
| `Blueprints/BP_CartCube` | 임시 물품 Actor 블루프린트 |
| `Blueprints/BP_CartPickupBase` | 공통 집기 상태, 충돌체, 외형 및 카트 보관 처리 |
| `Test/Character/BP_Cart_TestCharacter` | E키, 조준 판정, Physics Handle |
| `Test/UI/WBP_CartPickupPrompt` | 중앙 커서 아래 집기 안내 문구 |
| `Test/GameMode/BP_Cart_TestGameMode` | 카트 테스트 캐릭터 지정 |
| `Test/Blueprints/BP_CleaningCart` | 사용자가 만든 기존 스태틱 메시. 이름이 같지만 블루프린트가 아닙니다. |

카트의 Components에서 `ItemPoint_1`, `ItemPoint_2`, `ItemPoint_3`의 위치를 조절하면 물품 생성 위치가 바뀝니다.
각 포인트 아래의 `ReturnZone_1~3`는 빈 자리 조준용 Box Collision입니다. 상대 위치는 (0, 0, 0)으로 두고 Box Extent로 조준 범위를 조절합니다. 시작 설정은 No Collision이며, 실행 중 물품을 집을 때 Query Only로 전환합니다. 사용자께서 배치한 세 포인트의 위치는 유지했습니다.

카트 본체 `PickupBody` 전체를 NoCollision으로 바꾸지 마세요. 본체는 물리 충돌을 유지하고 Visibility만 Overlap으로 처리합니다. 캐릭터는 Multi Line Trace로 본체 안쪽의 물품/반환 박스를 찾습니다. 물품을 들고 있는 동안에는 자유 축의 Physics Constraint로 해당 물품과 원래 카트 사이의 충돌만 제외합니다.

외형을 바꿀 때는 카트/큐브 블루프린트의 Class Defaults에서 `VisualAsset`, `VisualScale`, `VisualOffset`을 설정합니다.
충돌 상자 크기는 `BodyHalfExtent`로 조절합니다. 카트 이동 시 높이 오프셋은 테스트 캐릭터의 `CartUpdateCarry`에서 조절합니다.
현재 프로토타입은 단일 플레이어용입니다.

## 반환 로직을 수정할 때

- `BP_CleaningCart.SpawnCartItems`: 세 물품을 만들고 각각의 ItemPoint와 ReturnZone을 연결합니다.
- `BP_CartPickupBase.InitializeCartSlot`: HomeCart, HomeSlot, HomeReturnZone을 연결합니다.
- `BP_CartPickupBase.EnableReturnPoint`: 물품을 집었을 때 빈 자리 감지를 켭니다.
- `BP_CartPickupBase.StoreOnCart`: 원래 포인트에 붙이고 물리 시뮬레이션/반환 박스를 끕니다.
- `BP_Cart_TestCharacter.FindCartInteraction`: 본체 안쪽 물품과 빈 자리까지 찾습니다.
- `BP_Cart_TestCharacter.TryReturnToCart`: E를 눌렀을 때 들고 있는 물품의 원래 자리인지 확인하고 반환합니다.

## 집기 안내 UI 수정

`Test/UI/WBP_CartPickupPrompt`의 TextBlock에서 문구, 글꼴 크기, 색상을 바꿀 수 있습니다. Canvas Slot의 앵커는 중앙 (0.5, 0.5), 위치는 (0, 40)입니다.

캐릭터의 `UpdateCartPickupPrompt`는 기존 `FindCartInteraction`으로 조준 대상을 확인하고, BP_CartCube이며 CanPickUp이 true인 경우 위젯을 HitTestInvisible로 표시합니다. 나머지 경우에는 Collapsed로 숨깁니다. Event Tick의 CartUpdateCarry 뒤에서 호출하며, 위젯은 BeginPlay에서 한 번만 생성합니다.

`Tools/Backups/BeforePickupPrompt_*`에는 사용자가 연결한 상태의 캐릭터와 위젯 백업이 있습니다.

## 검증 기록

- 집기 안내 UI: 큐브 조준 시 표시, 시선 이탈/집은 상태/카트 본체 조준/거리 밖에서 숨김, 바닥에 내려놓은 큐브 조준 시 재표시를 PIE에서 확인. 관련 위젯과 캐릭터 컴파일 통과.

- 최초 구현 관련 블루프린트 5개 컴파일 통과. 반환 기능 수정 후 관련 블루프린트 4개도 경고를 오류로 취급한 컴파일 통과.
- 세 ItemPoint에서 집기/원래 자리 반환 확인. 1번은 카트 본체 충돌 상자 내부에서 확인.
- 다른 빈 자리에서는 물품을 계속 들고 있고, 맨손으로 빈 자리를 조준해도 카트를 잡지 않음 확인.
- 바닥에 놓은 물품 재집기 → 카트를 이동한 뒤 원래 슬롯에 반환 확인. 반환 후 상대 위치 (0,0,0), 물리 시뮬레이션 꺼짐 확인.
- 반복 상호작용 후 물품은 3개로 유지됨 확인.
- PIE에서 카트 집기/놓기, 다시 잡기, 캐릭터 이동 시 카트 추적 확인.
- 카트에 큐브 3개 생성 및 카트를 잡은 동안 3개 모두 집기 불가 확인.
- 카트를 놓은 뒤 3개 모두 집기 가능 상태 확인.
- 큐브 각각 집기/놓기, 보관 위치에서 분리, 해제 후 물리/중력 활성화 확인.
- 바닥에 놓은 큐브 다시 집기, 빈 공간에서 상호작용 시 상태 유지 확인.
- E키의 Pressed 핀이 `CartInteract`에 연결됨을 검사했고, 자동 플레이 검사는 그 함수를 호출해 수행했습니다. 실제 키보드 입력 주입 테스트는 수행하지 않았습니다.
- 임시 검사 블루프린트와 액터를 제거하고 PIE를 종료한 상태로 저장했습니다.

`Tools/BlueprintGraphs`에는 최종 블루프린트 로직을 읽을 수 있는 DSL 기록이 있습니다. 내보내기 도구가 키 입력 이벤트의 실행 연결을 DSL에서 생략하므로, E키 연결은 `Character_InputWiring.json`에 별도로 기록했습니다. DSL은 참고용이며 전체 에셋 복원본은 아닙니다.
`Tools/Backups/BeforeInteraction`에는 기존 캐릭터, 게임모드, 테스트 맵의 수정 전 파일이 `.bak` 확장자로 보관되어 있습니다.
`Tools/Backups/BeforeSlotReturn_*`에는 이번 반환 기능 수정 전 에셋과 그래프 기록이 있습니다.
복원이 필요할 때는 에디터를 닫고 해당 원본 경로로 개별 복원합니다.

게임 콘텐츠 및 소스의 수정 범위는 `Content/CleaningCart` 내부입니다. 공용 C++ 코드와 다른 콘텐츠 폴더는 수정하지 않았습니다.
