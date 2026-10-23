// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MaterialInstanceTool : ModuleRules
{
	public MaterialInstanceTool(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// 프로젝트 런타임 모듈에 의존하지 않는다. 이 모듈이 쓰는 것은 엔진 헤더뿐이고,
		// 그래서 어느 프로젝트의 Plugins 폴더에 넣어도 그대로 빌드된다
		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",

			// MaterialInstanceToolWidget 이 public 헤더에서 UEditorUtilityWidget 을 상속한다.
			// 그 헤더를 쓰는 쪽이 UUserWidget 까지 같이 봐야 하므로 공개 의존성이다
			"UMG",
			"Blutility"
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"UnrealEd",
			"AssetTools",
			"MaterialEditor",
			"ToolMenus",
			"ContentBrowser",
			"Slate",
			"SlateCore",

			// DetailsView 는 ScriptableEditorWidgets 가 제공한다. BP 위젯 팔레트로만 쓸 때는
			// 의존성이 필요 없지만, NativeConstruct 에서 SetObject 를 부르므로 .cpp 가 헤더를 본다
			"ScriptableEditorWidgets"
		});

		// Public / Private 은 UBT 가 표준 레이아웃으로 알아서 잡는다. 따로 적지 않는다
	}
}
