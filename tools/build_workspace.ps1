param([ValidateSet('Release','Debug')][string]$Configuration='Release')
$ErrorActionPreference='Stop'
$taskRoot=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$taskVswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$taskVsRoot=& $taskVswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(-not $taskVsRoot){throw 'Install VS2022 C++ Build Tools with x86 MFC and Windows SDK.'}
$taskMsbuild=Join-Path $taskVsRoot 'MSBuild\Current\Bin\MSBuild.exe'
$taskVcvars=Join-Path $taskVsRoot 'VC\Auxiliary\Build\vcvars32.bat'
$taskTools=Get-ChildItem (Join-Path $taskVsRoot 'VC\Tools\MSVC') -Directory | Sort-Object {[version]$_.Name} -Descending | Where-Object {Test-Path (Join-Path $_.FullName 'atlmfc\include\afxwin.h')} | Select-Object -First 1
if(-not $taskTools){throw 'The installed C++ toolset requires the MFC component.'}
$taskMfc=Join-Path $taskTools.FullName 'atlmfc\lib\x86'
if(-not(Test-Path (Join-Path $taskMfc 'nafxcw.lib'))){$taskMfc=Join-Path $taskTools.FullName 'atlmfc\lib\spectre\x86'}
$taskKey=Join-Path $taskMfc $(if($Configuration -eq 'Debug'){'nafxcwd.lib'}else{'nafxcw.lib'})
if(-not(Test-Path -LiteralPath $taskKey)){throw "Missing MFC library: $taskKey"}
$taskTemporary=Join-Path ([IO.Path]::GetTempPath()) ('mushclient-build-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $taskTemporary | Out-Null
$taskProps=Join-Path $taskTemporary 'local.props'
$taskMfcXml=[Security.SecurityElement]::Escape($taskMfc)
$taskKeyXml=[Security.SecurityElement]::Escape($taskKey)
$taskXml='<Project><PropertyGroup><LibraryPath>'+ $taskMfcXml + ';$(LibraryPath)</LibraryPath><MFC_KeyFile>'+ $taskKeyXml + '</MFC_KeyFile></PropertyGroup></Project>'
Set-Content -LiteralPath $taskProps -Value $taskXml -Encoding UTF8
Push-Location -LiteralPath $taskRoot
try {
 & $taskMsbuild MUSHclient_2022.sln /m "/p:Configuration=$Configuration" /p:Platform=Win32 "/p:VCToolsVersion=$($taskTools.Name)" "/p:ForceImportAfterCppTargets=$taskProps" /verbosity:minimal /nologo
 if($LASTEXITCODE -ne 0){throw 'MUSHclient build failed.'}
 & (Join-Path $taskRoot 'workspace\build-scintilla.ps1') -VcVars $taskVcvars
 Copy-Item -LiteralPath (Join-Path $taskRoot 'third_party\scintilla\bin\Scintilla.dll') -Destination (Join-Path $taskRoot $(if($Configuration -eq 'Release'){'WinRel'}else{'WinDebug'}))
 # Standalone native MCP executable has only static runtime dependencies.
 $taskCmd=Join-Path $taskTemporary 'companion.cmd'
 $taskSourceArgs=@('agent_companion\MUSHclientAgent.cpp','AgentWorkspaceCore.cpp','AgentBridgeProtocol.cpp') | ForEach-Object {'"'+(Join-Path $taskRoot $_)+'"'}
 $taskOut=Join-Path $taskRoot 'WinRel\MUSHclientAgent.exe'
 $taskLines=@('@echo off', ('call "'+$taskVcvars+'" >nul'),('cd /d "'+$taskTemporary+'"'),('cl /nologo /EHsc /std:c++14 /MT /D_WIN32_WINNT=0x0601 '+($taskSourceArgs -join ' ')+' /Fe:"'+$taskOut+'"'))
 Set-Content -LiteralPath $taskCmd -Value $taskLines -Encoding ASCII
 & cmd.exe /d /c $taskCmd
 if($LASTEXITCODE -ne 0){throw 'Native MCP companion build failed.'}
 Write-Output "Build complete: $Configuration Win32. Source-only notices and PennMUSH references are packaged separately."
} finally {Pop-Location}
