$ErrorActionPreference = 'Stop'
$script:UeRpcId = 0
$script:UeRpcHeaders = @{ Accept = 'application/json, text/event-stream' }
function Invoke-UeRpc($Method, $Params) {
    $script:UeRpcId++
    $body = @{jsonrpc='2.0';id=$script:UeRpcId;method=$Method;params=$Params} | ConvertTo-Json -Depth 100 -Compress
    $response = Invoke-WebRequest -UseBasicParsing -Uri 'http://127.0.0.1:8000/mcp' -Method Post -ContentType 'application/json; charset=utf-8' -Headers $script:UeRpcHeaders -Body ([Text.Encoding]::UTF8.GetBytes($body)) -TimeoutSec 40
    if ($response.Headers['Mcp-Session-Id']) { $script:UeRpcHeaders['Mcp-Session-Id'] = $response.Headers['Mcp-Session-Id'] }
    $result = $response.Content | ConvertFrom-Json
    if ($result.error) { throw ($result.error | ConvertTo-Json -Depth 20) }
    return $result.result
}
function Invoke-UeTool($Toolset, $Name, $Arguments = @{}) {
    $result = Invoke-UeRpc 'tools/call' @{name='call_tool';arguments=@{toolset_name=$Toolset;tool_name=$Name;arguments=$Arguments}}
    if ($result.isError) { throw ($result.content | ConvertTo-Json -Depth 50) }
    if ($result.structuredContent) { $value = $result.structuredContent } else { $value = $result.content[0].text | ConvertFrom-Json }
    if ($value.PSObject.Properties.Name -contains 'returnValue') { return $value.returnValue }
    return $value
}
function Get-UeToolset($Name) {
    $result = Invoke-UeRpc 'tools/call' @{name='describe_toolset';arguments=@{toolset_name=$Name}}
    return ($result.content[0].text | ConvertFrom-Json).tools
}
function UeRef($Path) { return @{refPath=$Path} }
function Invoke-UePython($Code) {
    $id = [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()
    $path = "$PSScriptRoot/Animation/mop_runtime_request.json"
    $json = @{id=$id;code=$Code} | ConvertTo-Json -Depth 5 -Compress
    [IO.File]::WriteAllText($path,$json,(New-Object Text.UTF8Encoding $false))
    for($i=0;$i -lt 40;$i++) {
        Start-Sleep -Milliseconds 100
        if(Test-Path "$path.result") {
            try { $value=Get-Content "$path.result" -Raw | ConvertFrom-Json; if($value.id -eq $id) { return $value } } catch {}
        }
    }
    throw 'Editor Python diagnostic callback did not respond.'
}
$null = Invoke-UeRpc 'initialize' @{protocolVersion='2024-11-05';capabilities=@{};clientInfo=@{name='CartInteraction';version='1'}}
$UeBlueprint = 'editor_toolset.toolsets.blueprint.BlueprintTools'
$UeObject = 'editor_toolset.toolsets.object.ObjectTools'
$UeScene = 'editor_toolset.toolsets.scene.SceneTools'
$UeAssets = 'editor_toolset.toolsets.asset.AssetTools'
$UeEditor = 'EditorToolset.EditorAppToolset'
