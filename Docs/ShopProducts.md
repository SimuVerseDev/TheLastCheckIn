# 상품 등록과 구매·배치

GamePlay 서브레벨이나 레벨 블루프린트를 수정하지 않고 동작합니다. 새 C++ 컴포넌트가 플레이어 컨트롤러에 추가되므로 빌드 후 Unreal Editor를 재시작하세요.

## 상품 데이터

콘텐츠 브라우저에서 `/Game/Desktop/Data/DT_ShopProducts`를 열어 행을 추가합니다. 행 구조는 `ShopProductRow`입니다. 다른 테이블을 사용하려면 **Project Settings → Game → Hotel Shop → Product Table**에서 선택하세요.

| 필드 | 내용 |
|---|---|
| Row Name | 상품 고유 ID. 예: Chair_Wood |
| Display Name | 화면에 표시할 상품 이름 |
| Category | Furniture: 구매 → 가구 / Tool: 구매 → 도구 |
| Price | 구매 가격(G), 1 이상 |
| Icon | 카드 이미지. 비워 두면 기존 자리표시 이미지 |
| Preview Mesh | 필수 Static Mesh. 배치 중 표시할 외형 |
| Actor Class | 확정 후 생성할 BP/액터 클래스. 비워 두면 Preview Mesh를 사용하는 StaticMeshActor 생성 |
| Scale | 메시와 생성 액터에 적용할 양수 배율 |
| Available | 체크하면 판매 목록에 표시 |
| Sort Order | 작은 숫자부터 정렬 |

Preview Mesh는 실제 BP와 원점·크기·방향을 맞춰 주세요. 기본 배치 검사는 이 메시의 경계 상자를 기준으로 합니다. 복잡한 BP의 메시 구성과 다르면 프록시 메시가 필요합니다. Actor Class의 BP는 임의 위치에서 런타임 생성 가능한 형태여야 합니다.

`ShopProducts.example.csv`에는 등록 형식을 보여주는 샘플 두 행이 있습니다. 샘플 가격은 테스트용이며 Available=false라 판매되지 않습니다. 실제 상품과 가격은 등록해야 합니다.

## 플레이 흐름

1. 컴퓨터의 **구매 → 가구/도구**에서 상품 카드를 클릭합니다.
2. 잔액과 데이터를 검사하고 가격을 한 번 차감합니다. 데스크탑 UI를 닫고 화면 오른쪽 아래 방향에 배치 미리보기를 표시합니다.
3. 평소처럼 이동하고 마우스로 자유롭게 시점을 돌립니다. 미리보기는 화면 오른쪽 아래 방향에서 시점을 따라갑니다. 플레이어에서 수평 거리 350cm 이내이며 비어 있는 비교적 평평한 바닥에 배치할 수 있습니다. Q/E로 15도씩 물건을 회전합니다.
4. **좌클릭**하면 실제 물건을 생성하고 구매를 확정합니다. **Esc**는 미리보기를 제거하고 전액 환불합니다.

물건 생성 실패, 정산 전환, 플레이어 변경, 컨트롤러 종료도 대기 중 구매를 취소합니다. 환불은 당일 매출을 늘리지 않습니다. 확정 이후 Esc로 구매를 취소할 수는 없습니다. 미리보기에는 실제 상품 BP의 BeginPlay·상호작용·물리가 실행되지 않습니다.

## 구매한 물건 재배치

구매로 생성된 Static Mesh 및 BP에는 재배치 컴포넌트가 자동으로 붙습니다. 레벨이나 각 상품 BP에 따로 추가할 필요가 없습니다.

1. 빈손으로 가까운 구매 물건을 화면 중앙에서 바라보고 **좌클릭을 0.5초간 누릅니다**. 잡히기 전에 시선을 돌리거나 버튼을 놓으면 이동하지 않습니다.
2. **좌클릭을 계속 누른 채** 이동·시점을 조작하고, Q/E로 회전합니다.
3. 유효한 위치에서 **좌클릭을 놓으면 확정**합니다. 유효하지 않은 위치에서 놓거나 Esc를 누르면 원래 위치·회전으로 복원합니다.

재배치에는 차감이나 환불이 없습니다. 기존 액터를 유지하므로 조명 설정·BP 변수도 유지됩니다. 물리 시뮬레이션은 미리보기 중 잠시 멈추고 완료/취소 후 복구합니다. 다른 UI로 전환하거나 정산이 시작되는 경우도 취소합니다.

이 기능은 구매한 물건에 적용됩니다. 원래 레벨에 있던 물건의 상호작용은 그대로입니다. 빠루를 장착했거나 다른 물건을 들고 있으면 먼저 내려놓아야 재배치할 수 있습니다. 구매한 물건에 대한 짧은 좌클릭은 재배치를 시작하지 않습니다.

현재 범위는 한 번에 상품 한 개를 구매·배치하는 싱글플레이 흐름입니다. 자동 배치, 구매 물건의 세이브/로드, 재판매, 멀티플레이 서버 검증은 포함하지 않습니다. 배치 완료 이벤트 `ShopPurchaseSubsystem.OnPurchaseConfirmed(ProductId, Product)`를 나중에 인벤토리·세이브 로직에 연결할 수 있습니다.

## 코드 연결

- `ShopPurchaseSubsystem`: BeginPurchase → ConfirmPurchase 또는 CancelPurchase. 가격과 에셋을 구매 시점에 보관합니다.
- `ShopPlacementComponent`: 플레이어 컨트롤러에 자동 생성. `PurchaseAndPlace(Table, ProductId, ShopToClose, Error)`가 데스크탑 닫기와 배치 입력 전환까지 처리합니다.
- `ShopPlacedProductComponent`: 구매 시 액터에 자동 추가. 상품 ID·미리보기 메시를 보관하고 원본 액터의 재배치/취소 상태를 복원합니다.
- `DesktopSurfaceWidget`: 설정된 테이블에서 판매 가능한 상품을 읽고 카드 클릭을 연결합니다.
- `HotelEconomySubsystem.RefundPurchase`: 잔액만 복원하는 내부 C++ 함수입니다.

`ConfirmPurchase`를 직접 호출하는 별도 배치 시스템은 먼저 배치 위치를 검증해야 합니다. 기존 UI는 `ShopPlacementComponent.ConfirmPlacement`를 통해 위치를 검증합니다.
