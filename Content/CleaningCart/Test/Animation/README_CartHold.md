# 청소 카트 기본 잡기 자세

- AS_CartHold: Manny용 1초 고정 포즈, 30fps. BP_Cart_TestCharacter에서 카트 본체를 잡는 동안 양팔에 적용됩니다.
- LS_CartHold_Work: 양팔 IK를 수정할 수 있는 작업용 Level Sequence입니다. 0과 30프레임에 같은 자세가 있습니다.
- Lvl_Cart_test의 CartHold_Preview_Manny: 편집 전용 미리보기입니다. 충돌 없음, 게임에서 숨김, Editor Only 설정입니다.

## 확인 및 수정

1. CleaningCart/Test/Maps/Lvl_Cart_test를 엽니다.
2. CleaningCart/Test/Animation/LS_CartHold_Work를 엽니다.
3. 아웃라이너에서 CartHold_Preview_Manny를 선택하고 F를 눌러 가까이 봅니다.
4. Sequencer의 CR_Mannequin_Body 트랙에서 hand_l_ik_ctrl / hand_r_ik_ctrl을 선택해 양손 위치를 조절합니다. 팔꿈치는 arm_l_pv_ik_ctrl / arm_r_pv_ik_ctrl로 조절합니다.
5. 고정 자세를 유지하려면 0프레임과 30프레임에 같은 값을 키로 저장합니다.
6. 수정 후 Sequencer의 마네퀸 바인딩을 우클릭하여 Bake Animation Sequence로 AS_CartHold에 다시 베이크합니다. 기존 애니메이션을 덮어쓸지 확인하고, 저장 위치는 이 Animation 폴더로 지정합니다.

## 현재 기준

실제 배치된 카트의 VisualMesh 상단은 월드 Z 약 138.44cm입니다. 손잡이가 없는 임시 박스이므로 위쪽 앞 모서리를 기준으로 처음에는 손목 Z 138.5cm, 좌우 간격 48cm로 맞췄고, 이후 사용자 요청으로 양손을 15cm 내려 현재 손목 Z는 123.5cm입니다. 마네퀸 위치는 (140, 0, 1.6), 회전 Yaw -90도입니다. 손목은 팔의 기본 각도를 따라 정렬했고 손가락 쥐기는 별도로 제작하지 않았습니다.

공용 Manny 메시·스켈레톤·Control Rig는 참조만 합니다. 캐릭터 AnimBP 연결은 완료했습니다. 실제 플레이에서 손과 카트가 맞닿는 위치는 카트 유지 거리와 함께 조정할 수 있습니다.

레벨 수정 전 디스크 파일 백업: CleaningCart/Tools/Animation/Lvl_Cart_test_before_pose.umap.bak

## 카트 잡기 연결

- 사용 AnimBP: CleaningCart/Test/Character/ABP_CartCharacter. BP_Cart_TestCharacter의 몸 메시 Anim Class에 지정했습니다.
- Event Graph: HeldCartObject가 유효하고 BP_CleaningCart인 경우에만 IsHoldingCart가 True입니다. 내려놓거나 물품을 잡으면 False입니다.
- CartHoldAlpha: FInterpTo_Constant, 속도 5로 0~1을 약 0.2초에 전환합니다.
- AnimGraph: 기존 Control Rig 출력을 Layered blend per bone의 Base Pose로, AS_CartHold를 Blend Poses 0으로 사용합니다. 가중치는 CartHoldAlpha이며 clavicle_l / clavicle_r부터 양팔에 적용합니다.
- First Person Mesh는 기존 ABP_FP_Copy로 몸 포즈를 복사합니다.
- 검증: 두 블루프린트 경고 포함 컴파일 통과, 저장 완료 확인. PIE에서 카트 집기/놓기 및 반복 전환, 대걸레·손걸레·쓰레기통 보유 시 카트 자세 미적용을 확인했습니다. 물리 E키 입력 주입 대신 E키에 연결된 CartInteract 함수를 호출했습니다. 실제 손과 카트의 접촉 위치는 후속 조정이 필요합니다.

## 팔 높이 조정

양손 IK와 팔꿈치 방향 컨트롤을 15cm 낮추고, 손목의 팔에 대한 각도를 유지하도록 보정했습니다. LS_CartHold_Work의 0/30프레임에 적용 후 기존 AS_CartHold에 재베이크했습니다. 애니메이션의 0/15/30프레임에서 양손 뼈 높이가 기존보다 각각 15cm 낮아졌음을 데이터로 확인했습니다. 이 높이 수정 후 PIE는 실행하지 않았습니다.
