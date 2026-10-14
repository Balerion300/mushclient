param([string]$RepoRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path, [int]$TimeoutSeconds=60)
$ErrorActionPreference='Stop'
$taskVswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$taskVsRoot=& $taskVswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(-not $taskVsRoot){ throw 'Visual C++ tools are required.' }
$taskCompilerRoot=(Get-ChildItem (Join-Path $taskVsRoot 'VC\Tools\MSVC') -Directory | Sort-Object {[version]$_.Name} -Descending | Select-Object -First 1).FullName
$taskSdkBase=Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10'
$taskSdkVersion=(Get-ChildItem (Join-Path $taskSdkBase 'Include') -Directory | Where-Object { Test-Path (Join-Path $_.FullName 'ucrt\new.h') } | Sort-Object {[version]$_.Name} -Descending | Select-Object -First 1).Name
if(-not $taskSdkVersion){ throw 'Windows SDK C++ headers are required.' }
$taskOldInclude=$env:INCLUDE
$taskOldLib=$env:LIB
$taskLocation=Get-Location
try {
  Set-Location -LiteralPath $RepoRoot
  $env:INCLUDE="${taskCompilerRoot}\include;${taskCompilerRoot}\atlmfc\include;${taskSdkBase}\Include\${taskSdkVersion}\ucrt;${taskSdkBase}\Include\${taskSdkVersion}\shared;${taskSdkBase}\Include\${taskSdkVersion}\um"
  $taskMfcLib=Join-Path $taskCompilerRoot 'atlmfc\lib\spectre\x86'
  if(-not(Test-Path $taskMfcLib)){ $taskMfcLib=Join-Path $taskCompilerRoot 'atlmfc\lib\x86' }
  $env:LIB="${taskCompilerRoot}\lib\x86;${taskMfcLib};${taskSdkBase}\Lib\${taskSdkVersion}\ucrt\x86;${taskSdkBase}\Lib\${taskSdkVersion}\um\x86"
  New-Item -ItemType Directory -Force 'WinRel\scripted-logging-probe-objects' | Out-Null
  $taskPcreObjects=Get-ChildItem 'WinRel' -Filter 'pcre_*.obj'
  if($taskPcreObjects.Count -lt 10){ throw 'Existing native PCRE objects are required.' }
  Copy-Item -LiteralPath '.distribution-source\extracted\lua5.1.dll' -Destination 'WinRel\scripted-logging-probe-objects\lua5.1.dll' -Force
  $taskResponse=@"
/nologo /MT /DWIN32 /D_CRT_SECURE_NO_WARNINGS /I.
/FeWinRel\scripted-logging-probe-objects\scripted-logging-lua-probe.exe /FoWinRel\scripted-logging-probe-objects\
tests\scripted_logging\lua_probe.c scripting\lrexlib.c
/link /SUBSYSTEM:CONSOLE /LTCG lua5.1.lib
"@
  $taskResponse += "`n"+ (($taskPcreObjects | ForEach-Object { '"'+$_.FullName+'"' }) -join ' ')
  Set-Content -LiteralPath 'WinRel\scripted-logging-probe-objects\build.rsp' -Value $taskResponse
  & (Join-Path $taskCompilerRoot 'bin\Hostx64\x86\cl.exe') '@WinRel\scripted-logging-probe-objects\build.rsp'
  if($LASTEXITCODE -ne 0){ throw 'Standalone Lua probe compilation failed.' }
  & 'WinRel\scripted-logging-probe-objects\scripted-logging-lua-probe.exe'
  if($LASTEXITCODE -ne 0){ throw 'Standalone Lua helper tests failed.' }
} finally {
  $env:INCLUDE=$taskOldInclude
  $env:LIB=$taskOldLib
  Set-Location -LiteralPath $taskLocation
}
