# Housekeeping 도메인 설계 (파트 1: 청소·상호작용)

> 상태: **설계 단계** — 아직 코드 없음. 이 문서가 합의되면 아래 폴더 구조대로 구현한다.

## 0. 원칙 (Bounded Context)

- 내 코드는 전부 `Source/TheLastCheckIn/Housekeeping/` 안에, 에셋은 `Content/Housekeeping/` 안에 둔다.
- 클래스 접두사는 **`HK`** (HouseKeeping). 예: `UHKInteractionComponent`, `AHKTrashActor`.
- 다른 파트는 내부 클래스를 직접 건드리지 않는다. 대신 **공개 계약(Public API)** 두 개만 쓴다.
  1. `IHKInteractable` — E키 상호작용 인터페이스 (Shared Kernel: 프런트 카운터, 손님 내보내기 등 다른 파트도 구현해서 씀)
  2. `UHKCleaningSubsystem` — 객실 청결도 조회/리셋 + 이벤트 (Application Service)
- 공용 파일(`Build.cs`, `TheLastCheckInCharacter`, `GameMode`, `DefaultEngine.ini`)은 **가급적 안 건드린다**. 꼭 필요하면 아래 [7. 외부 연동 지점](#7-외부-연동-지점-공용-파일-수정-필요)에 적고 팀에 알린다.
- include는 모듈 루트 기준 경로로 한다 → `#include "Housekeeping/Interaction/HKInteractable.h"`
  (Build.cs의 `PublicIncludePaths`에 `"TheLastCheckIn"`이 이미 있으므로 **Build.cs 수정 불필요**)

## 1. 폴더 구조

```
Source/TheLastCheckIn/Housekeeping/
├── DESIGN.md
├── Interaction/                 ① E키 상호작용 공통 규칙 (공개 계약)
│   ├── HKInteractable.h            UInterface: CanInteract / Interact / GetPrompt / GetHoldDuration
│   ├── HKInteractionComponent.h/.cpp   플레이어에 붙음: 시선 트레이스, 포커스, 누르기/홀드 처리
│   └── HKInteractionTypes.h        FHKInteractPrompt 등 공용 구조체
├── Carry/                       ② 들기·던지기 (물리)
│   └── HKCarryComponent.h/.cpp     PhysicsHandle 기반 들기/놓기/던지기
├── Cleanables/                  ②③ 청소 대상 오브젝트
│   ├── HKCleanableComponent.h/.cpp 청결도 기여 단위 (가중치, bIsClean)
│   ├── HKTrashActor.h/.cpp         쓰레기 (줍기 가능, 물리)
│   ├── HKTrashBin.h/.cpp           쓰레기통 (들어오면 제거 + 청결 처리)
│   ├── HKBedActor.h/.cpp           침구 정리 (홀드 E)
│   ├── HKLaundryItem.h/.cpp        시트/수건 (들 수 있음, Dirty/Clean)
│   └── HKWashingMachine.h/.cpp     세탁기 (Dirty 투입 → 타이머 → Clean)
├── Room/                        ④ 객실 청결도 계산
│   ├── HKRoomVolume.h/.cpp         객실 영역 박스, 안의 Cleanable 수집·계산
│   └── HKCleaningSubsystem.h/.cpp  UWorldSubsystem: RoomId → 청결도 레지스트리 (공개 계약)
└── UI/                          ① ④ UI
    ├── HKInteractPromptWidget.h/.cpp   "[E] 줍기" + 홀드 진행 링
    └── HKRoomCleanlinessWidget.h/.cpp  "102호 청결도 75%"

Content/Housekeeping/
├── Input/        IA_Interact, IA_Throw, IMC_Housekeeping
├── Blueprints/   BP_Trash_*, BP_TrashBin, BP_Bed, BP_Laundry_*, BP_WashingMachine, BP_RoomVolume
├── UI/           WBP_InteractPrompt, WBP_RoomCleanliness
└── Meshes/ Materials/ Sounds/
```

## 2. ① E키 상호작용 공통 규칙

### 2.1 `IHKInteractable` (UInterface, BlueprintNativeEvent → BP에서도 구현 가능)

| 함수 | 반환 | 설명 |
|---|---|---|
| `CanInteract(AActor* Instigator)` | `bool` | 지금 상호작용 가능한가 (이미 정리된 침대 → false) |
| `GetInteractPrompt(AActor* Instigator)` | `FText` | UI에 띄울 문구: "줍기", "정리하기", "체크인" |
| `GetHoldDuration()` | `float` | 0 = 탭, >0 = 그 초만큼 홀드해야 완료 |
| `Interact(AActor* Instigator)` | `void` | 실행 (홀드면 완료 시점에 호출) |
| `OnFocusBegin/End(AActor* Instigator)` | `void` | (선택) 외곽선 하이라이트 등 |

> 다른 파트 사용 예: 프런트 카운터(파트3) `CanInteract`=true, `Interact`=체크인 UI 열기. 손님 내보내기(파트2) `GetPrompt`="내보내기".

### 2.2 `UHKInteractionComponent` (플레이어 캐릭터에 부착)

- 0.05~0.1초 타이머로 카메라 기준 라인트레이스 (거리 `InteractRange` 기본 250cm, 채널 `ECC_Visibility`)
- 맞은 액터가 `IHKInteractable` 구현 + `CanInteract`면 **FocusedActor**로 지정
- 상태: `Idle → Focused → Holding(진행률) → Completed`
- 시선이 벗어나거나 키를 떼면 홀드 취소
- 델리게이트 (UI용):
  - `OnFocusChanged(AActor* NewFocus, FText Prompt)`
  - `OnHoldProgress(float Alpha01)`
- **입력은 컴포넌트가 스스로 바인딩**: BeginPlay에서 `IMC_Housekeeping`을 EnhancedInput 서브시스템에 추가 → 공용 캐릭터/IMC 수정 불필요
- 들고 있는 물건이 있으면 E는 CarryComponent의 "놓기"로 위임 (우선순위: 들고있음 > 포커스 대상)

## 3. ② 쓰레기 줍기/버리기 (물리)

### `UHKCarryComponent` (플레이어에 부착)
- `UPhysicsHandleComponent`로 카메라 앞 `HoldDistance`(≈120cm) 위치에 물체를 끌고 다님 → 벽에 부딪히고 흔들리는 물리감
- `Pickup(UPrimitiveComponent*)` / `Drop()` / `Throw()` (카메라 전방 임펄스 `ThrowImpulse`)
- 무게 제한: `MaxCarryMass` 초과면 못 듦
- 들 수 있는 액터 판별: `IHKInteractable::Interact` 안에서 해당 액터가 CarryComponent에 `Pickup` 요청 (CarryComponent는 대상 클래스를 몰라도 됨)

### `AHKTrashActor`
- StaticMesh (Simulate Physics) + `UHKCleanableComponent`(초기 `bIsClean=false`)
- Interact → 플레이어 CarryComponent로 Pickup

### `AHKTrashBin`
- 입구에 Box 트리거. `AHKTrashActor`가 Overlap → Cleanable `MarkClean()` 후 Destroy (던져 넣어도 인정)
- (확장) 쓰레기봉투가 꽉 차면 비우기

## 4. ③ 침구 정리 / 세탁

### MVP
- `AHKBedActor`: 상태 `Messy / Made`. Messy 메시 ↔ Made 메시 스왑. 홀드 E(2초) → Made, Cleanable `MarkClean()`

### 확장 (시간 되면)
```
더러운 침대 ──E──▶ 시트 벗기기 → AHKLaundryItem(Dirty) 스폰, 침대 = NoSheet
AHKLaundryItem(Dirty) ──들고 가서──▶ AHKWashingMachine 투입 → WashTime 타이머
세탁 완료 ──E──▶ AHKLaundryItem(Clean) 배출
AHKLaundryItem(Clean) ──침대에 E──▶ 침대 = Made
```
- 침대 상태: `Dirty → NoSheet → Made` (EHKBedState)
- 세탁기 상태: `Empty → Washing → Done` (EHKWasherState)

## 5. ④ 객실 청결도

### 5.1 `UHKCleanableComponent` — 청결도의 최소 단위 (컴포지션)
- `float Weight` (쓰레기 1, 침대 3, 얼룩 2 …)
- `bool bIsClean`, `MarkClean()`, `MarkDirty()`
- 상태가 바뀌면 `OnCleanStateChanged` 브로드캐스트 → 소속 RoomVolume이 재계산
- 아무 액터에나 붙이면 청소 대상이 됨 → 나중에 얼룩(Decal), 거울 등 추가 쉬움

### 5.2 `AHKRoomVolume`
- `FName RoomId` (예: `Room_101`) — **파트3 객실 목록과 같은 ID 써야 함**
- BeginPlay: 박스 안의 Cleanable 수집, 이후 런타임 Overlap으로 추가/제거 (쓰레기 들고 나가면 빠짐)
- `청결도(%) = Σ(clean 가중치) / Σ(전체 가중치) × 100` (대상 0개면 100%)
- 플레이어가 들어오면/나가면 청결도 UI 표시/숨김

### 5.3 `UHKCleaningSubsystem` (UWorldSubsystem) — **다른 파트용 공개 API**

| API | 사용 파트 | 설명 |
|---|---|---|
| `float GetRoomCleanliness(FName RoomId)` | 3 (객실 현황판) | 0~100 |
| `bool IsRoomClean(FName RoomId)` | 3 (배정 가능 여부) | `>= CleanThreshold` (기본 100% 또는 90%) |
| `void MakeRoomDirty(FName RoomId)` | 2/3 (체크아웃 시) | 쓰레기 스폰 + 침대 Messy로 리셋 |
| `OnRoomCleanlinessChanged(FName RoomId, float Percent)` | 3, 4 | 값 변경 이벤트 |
| `OnRoomBecameClean(FName RoomId)` | 3 | 오염 → 빈방 전환 트리거 |

- 객실 상태(빈방/투숙/오염) **자체는 파트3 소유**. 나는 "깨끗한가"만 알려주고, 상태 전환은 파트3이 이벤트 받아서 한다.

### 5.4 도메인 이벤트 흐름
```
[파트2/3] 손님 체크아웃 ──MakeRoomDirty(101)──▶ [HK] 쓰레기/침대 리셋 → Cleanliness 40%
[플레이어] 쓰레기 버림 → Cleanable.MarkClean → RoomVolume 재계산 → Subsystem
           ──OnRoomCleanlinessChanged(101, 80)──▶ [파트3] 현황판 갱신
           ──OnRoomBecameClean(101)──▶ [파트3] 101호 '오염' → '빈방'
[파트4] 낮 종료 → (선택) 청결도 정산 조회 GetRoomCleanliness
```

## 6. UI

- `WBP_InteractPrompt`: 크로스헤어 아래 `[E] {Prompt}` + 홀드 시 원형 진행바. `OnFocusChanged` / `OnHoldProgress` 바인딩
- `WBP_RoomCleanliness`: 방 안에서만 표시. `"{RoomName} 청결도 {N}%"` + 바. RoomVolume 진입 시 생성
- HUD 전체 레이아웃은 파트3과 겹칠 수 있음 → 위젯은 내 폴더에, **부착 위치(HUD에 Add)** 만 합의

## 7. 외부 연동 지점 (공용 파일 수정 필요)

| 항목 | 파일 | 내용 | 대안 |
|---|---|---|---|
| 컴포넌트 부착 | 플레이어 캐릭터 BP 또는 `TheLastCheckInCharacter` | `UHKInteractionComponent`, `UHKCarryComponent` 추가 | BP에서 Add Component만 (C++ 무수정) |
| 입력 | 공용 IMC | 없음 — 자체 `IMC_Housekeeping` 사용 | — |
| 트레이스 채널 | `DefaultEngine.ini` | 전용 `Interactable` 채널 | 일단 `ECC_Visibility` 사용 |
| 객실 ID | 파트3 데이터 | RoomId 규칙 합의 | — |

## 8. 팀과 합의할 것 (Open Questions)

1. **RoomId 형식**: `FName "Room_101"`? `int32 101`? (파트3이 정의하는 객실 데이터와 통일)
2. **청결 기준**: 100%여야 배정 가능? 아니면 임계값(90%)?
3. **체크아웃 시 더럽히기**는 누가 호출? (파트2 손님 퇴장 시점 vs 파트3 체크아웃 처리)
4. 객실 청결도가 **보상/평판**에 영향을 주는가? (파트3 경영 정산과 연결 여부)
5. E키 외에 **던지기 키** (좌클릭?) — 파트4 룸서비스 배달도 들고 다니기면 CarryComponent 공유 가능
6. 룸서비스 음식(파트4)을 들고 가는 방식이 쓰레기와 같다면 `UHKCarryComponent`를 공용으로 쓸지

## 9. 구현 순서 (제안)

1. `IHKInteractable` + `UHKInteractionComponent` + 프롬프트 UI → **가장 먼저 푸시** (다른 파트가 의존)
2. `UHKCarryComponent` + `AHKTrashActor` + `AHKTrashBin`
3. `UHKCleanableComponent` + `AHKRoomVolume` + `UHKCleaningSubsystem` + 청결도 UI
4. `AHKBedActor` (MVP)
5. 세탁 루프 (확장)
