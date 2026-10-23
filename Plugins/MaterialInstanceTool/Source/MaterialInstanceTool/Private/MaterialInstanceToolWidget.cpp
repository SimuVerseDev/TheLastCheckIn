// Fill out your copyright notice in the Description page of Project Settings.

#include "MaterialInstanceToolWidget.h"

#include "MaterialInstanceTool.h"
#include "MaterialInstanceToolSettings.h"
#include "EditorUtilityLibrary.h"
#include "Components/Button.h"
#include "Components/DetailsView.h"
#include "Engine/Texture.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"

DEFINE_LOG_CATEGORY_STATIC(LogMIToolWidget, Log, All);

void UMaterialInstanceToolWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// BP 위젯에는 그래프 노드가 하나도 없다. 디테일 패널이 보여줄 대상과 버튼 핸들러를
	// 여기서 묶는다. 칸이 이 위젯의 UPROPERTY 이므로 SetObject 의 대상은 this 다
	if (ToolDetails)
	{
		ToolDetails->SetObject(this);
	}

	if (CreateButton)
	{
		// NativeConstruct 는 위젯 탭을 다시 열 때마다 불린다. 먼저 떼지 않으면
		// 핸들러가 겹쳐 붙어서 버튼 한 번에 두 번 돌아간다
		CreateButton->OnClicked.RemoveDynamic(this, &UMaterialInstanceToolWidget::CreateMaterialInstances);
		CreateButton->OnClicked.AddDynamic(this, &UMaterialInstanceToolWidget::CreateMaterialInstances);
	}
}

TArray<UObject*> UMaterialInstanceToolWidget::BuildSelection() const
{
	TArray<UObject*> Selection;

	// 칸에 담긴 것을 먼저 넣는다. FindSettings 와 FindMaster 가 배열을 앞에서부터 훑으므로,
	// 콘텐츠 브라우저에 다른 설정 에셋이나 머티리얼이 선택돼 있어도 칸이 이긴다
	if (Master)
	{
		Selection.Add(Master);
	}

	if (Rules)
	{
		Selection.Add(Rules);
	}

	if (Textures.Num() > 0)
	{
		for (const TObjectPtr<UTexture>& Texture : Textures)
		{
			// 배열 칸은 비어 있는 행을 남길 수 있다. 그대로 넣으면 GroupTextures 에서 걸러지지만,
			// 여기서 떨구면 "접미사 규칙에 없는 텍스처" 경고가 빈 이름으로 찍히는 일이 없다
			if (Texture)
			{
				Selection.Add(Texture);
			}
		}
	}
	else
	{
		// 칸이 비었으면 콘텐츠 브라우저 선택을 쓴다. 한 머티리얼에 텍스처가 네다섯 장이고,
		// 배열 칸에서 + 를 눌러 하나씩 고르는 것보다 브라우저에서 긁는 쪽이 빠르다.
		// GetSelectedAssetsOfClass 는 IsInstanceOf 로 거르므로 UTexture2D 도 함께 잡힌다
		Selection.Append(UEditorUtilityLibrary::GetSelectedAssetsOfClass(UTexture::StaticClass()));
	}

	for (const TObjectPtr<UStaticMesh>& Mesh : Meshes)
	{
		if (Mesh)
		{
			Selection.Add(Mesh);
		}
	}

	return Selection;
}

void UMaterialInstanceToolWidget::CreateMaterialInstances()
{
	const TArray<UObject*> Selection = BuildSelection();

	// 텍스처가 하나도 없는 경우만 여기서 막는다. 부모가 없는 것은 진입점이 분명한 말로 찍어주지만,
	// 텍스처가 없으면 "묶음 0 개" 만 찍히고 조용히 끝나서 왜 아무것도 안 생겼는지 알 수 없다
	const bool bHasTexture = Selection.ContainsByPredicate(
		[](const UObject* Object)
		{
			return Object && Object->IsA<UTexture>();
		});

	if (!bHasTexture)
	{
		UE_LOG(LogMIToolWidget, Error,
			TEXT("텍스처가 없다. Textures 칸을 채우거나 콘텐츠 브라우저에서 텍스처를 선택할 것"));
		return;
	}

	MaterialInstanceTool::CreateMaterialInstances(Selection);
}
