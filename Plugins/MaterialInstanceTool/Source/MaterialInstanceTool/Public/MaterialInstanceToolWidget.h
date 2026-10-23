// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EditorUtilityWidget.h"
#include "MaterialInstanceToolWidget.generated.h"

class UButton;
class UDetailsView;
class UMaterialInterface;
class UMaterialInstanceToolSettings;
class UStaticMesh;
class UTexture;

/**
 *  MaterialInstanceTool 을 칸과 버튼으로 쓰는 에디터 유틸리티 위젯.
 *
 *  칸은 아래 "에디터 설정" 의 UPROPERTY 들이다. BP 위젯(EUW_MaterialInstanceTool)은
 *  레이아웃만 갖고 그래프 노드는 하나도 없다. 디테일 패널에 무엇을 보여줄지와
 *  버튼을 누르면 무엇을 할지는 NativeConstruct 가 C++ 에서 묶는다.
 *
 *  UMG 에 에셋 피커 위젯이 없어서 DetailsView 에 자기 자신을 보여주는 방식을 쓴다.
 *
 *  이 클래스는 일을 하지 않는다. 칸을 평평한 배열로 모아 MaterialInstanceTool 에
 *  넘기는 것까지다. 우클릭 메뉴와 이 위젯은 같은 함수를 부르는 서로 다른 입구다.
 */
UCLASS()
class MATERIALINSTANCETOOL_API UMaterialInstanceToolWidget : public UEditorUtilityWidget
{
	GENERATED_BODY()

	// ── 위젯 ──
	//
	// BP 위젯에 있는 같은 이름의 위젯에 자동으로 묶인다. 이름이 어긋나면 BP 컴파일이
	// 실패하므로, 조용히 null 이 되는 대신 에디터가 바로 알려준다

	/** 아래 네 칸을 디테일 패널 모양으로 그린다. categoriesToShow 는 "Tool" 로 맞춰져 있다 */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UDetailsView> ToolDetails;

	/** 눌리면 CreateMaterialInstances 가 불린다 */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> CreateButton;

public:

	// ── 에디터 설정 ──
	//
	// Category 를 "Tool" 로 묶어두는 이유는, DetailsView 의 categoriesToShow 로
	// 이 네 개만 거르기 위해서다. 안 묶으면 UserWidget 이 상속한 속성이 전부 같이 나온다

	/** 만들어질 인스턴스의 부모. 비워둘 수 없다 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tool")
	TObjectPtr<UMaterialInterface> Master = nullptr;

	/** 접미사 규칙. 비워두면 코드에 박힌 기본 규칙으로 간다 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tool")
	TObjectPtr<UMaterialInstanceToolSettings> Rules = nullptr;

	/** 비워두면 콘텐츠 브라우저에서 선택한 텍스처를 쓴다 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tool")
	TArray<TObjectPtr<UTexture>> Textures;

	/** 비워두면 인스턴스를 만드는 데까지만 하고 끝낸다 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tool")
	TArray<TObjectPtr<UStaticMesh>> Meshes;

public:

	//~ Begin UUserWidget
	virtual void NativeConstruct() override;
	//~ End UUserWidget

	// ── 공개 API ──

	/** 버튼이 부른다. 그래프에서 직접 부를 수도 있게 BlueprintCallable 로 둔다 */
	UFUNCTION(BlueprintCallable, Category = "Tool")
	void CreateMaterialInstances();

private:

	// ── 내부 헬퍼 ──

	/** 칸들을 진입점이 받는 모양의 한 배열로 모은다 */
	TArray<UObject*> BuildSelection() const;
};
