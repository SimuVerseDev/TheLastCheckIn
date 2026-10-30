. "$PSScriptRoot/../CartInteractionRpc.ps1"
Invoke-UePython @'
base=unreal.load_asset('/Game/CleaningCart/Blueprints/BP_CartPickupBase')
cart=unreal.load_asset('/Game/CleaningCart/Blueprints/BP_CleaningCart')
print('implements_base',unreal.SystemLibrary.does_implement_interface(unreal.get_default_object(base.generated_class()),unreal.load_class(None,'/Script/TheLastCheckIn.Interactable')))
print('implements_cart',unreal.SystemLibrary.does_implement_interface(unreal.get_default_object(cart.generated_class()),unreal.load_class(None,'/Script/TheLastCheckIn.Interactable')))
print('blueprint_interface_struct',[n for n in dir(unreal) if 'Interface' in n and 'Blueprint' in n])
print('editor_funcs',[n for n in dir(unreal.BlueprintEditorLibrary) if not n.startswith('_')])
import editor_toolset.toolsets.blueprint as bpt
print('tool_module',bpt.__file__)
'@ | ConvertTo-Json -Depth 5
