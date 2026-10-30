. "$PSScriptRoot/CartInteractionRpc.ps1"
$null=New-Item -ItemType Directory -Force "$PSScriptRoot/InteractionMigration"
foreach($name in @('BP_CartPickupBase','BP_CleaningCart')) {
 $bp=UeRef "/Game/CleaningCart/Blueprints/$name.$name"
 $graphs=Invoke-UeTool $UeBlueprint 'list_graphs' @{blueprint=$bp}
 foreach($graph in $graphs) {
  $dsl=Invoke-UeTool $UeBlueprint 'read_graph_dsl' @{graph=$graph}
  $fn=($graph.refPath -split ':')[-1]
  $dsl | Set-Content "$PSScriptRoot/InteractionMigration/${name}__${fn}.dsl" -Encoding UTF8
  Write-Output "$name $fn"
 }
}
$bp=UeRef '/Game/CleaningCart/Test/Character/BP_Cart_TestCharacter.BP_Cart_TestCharacter'
foreach($fn in @('EventGraph','CartUpdateCarry','GrabCartObject','TryReturnToCart','FindCartInteraction','CartRelease')) {
 $dsl=Invoke-UeTool $UeBlueprint 'read_graph_dsl' @{graph=(UeRef ($bp.refPath+':'+$fn))}
 $dsl | Set-Content "$PSScriptRoot/InteractionMigration/Character__$fn.dsl" -Encoding UTF8
}
foreach($set in @($UeObject,$UeScene,$UeAssets,'editor_toolset.toolsets.actor.ActorTools','editor_toolset.toolsets.programmatic.ProgrammaticToolset')) {
 Get-UeToolset $set | ConvertTo-Json -Depth 50 | Set-Content "$PSScriptRoot/InteractionMigration/$(($set -split '\.')[-1]).json" -Encoding UTF8
}
Invoke-UeTool $UeEditor 'IsPIERunning'
