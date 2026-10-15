param([string]$RepoRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path)
$ErrorActionPreference='Stop'
$taskOutput=Join-Path $env:TEMP ('mushclient-style-components-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $taskOutput | Out-Null
$taskProduction=[IO.File]::ReadAllText((Join-Path $RepoRoot 'WorkspaceStyle.cpp')).Replace('#include "stdafx.h"','')
Set-Content -LiteralPath (Join-Path $taskOutput 'style-production.inc') -Value $taskProduction -Encoding UTF8
$taskSources=@('tests\workspace\style_components_probe.cpp','tests\workspace\style_platform_adapter.cpp') | ForEach-Object {'"'+(Join-Path $RepoRoot $_)+'"'}
$taskResponse=@"
/nologo /MT /EHsc /std:c++14 /D_WIN32_WINNT=0x0601 /DNO_HTMLHELP /DNDEBUG /DWIN32 /D_WINDOWS /DZLIB_DLL /DLUA51 /D_CRT_SECURE_NO_WARNINGS /D_CRT_NON_CONFORMING_SWPRINTFS /DNO_WARN_MBCS_MFC_DEPRECATION
/I"$RepoRoot" /I"$RepoRoot\luacom" /I"$RepoRoot\openssl\include" /I"$taskOutput"
/Fe"$taskOutput\style-components-probe.exe" /Fo"$taskOutput\\"
$($taskSources -join ' ')
/link /SUBSYSTEM:WINDOWS Oleacc.lib
"@
Set-Content -LiteralPath (Join-Path $taskOutput 'build.rsp') -Value $taskResponse -Encoding ascii
Set-Content -LiteralPath (Join-Path $taskOutput 'build.cmd') -Encoding ascii -Value @"
@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars32.bat" >nul
set "LIB=%VCToolsInstallDir%atlmfc\lib\spectre\x86;%LIB%"
cl @"$taskOutput\build.rsp"
"@
Push-Location -LiteralPath $taskOutput
try {
  & .\build.cmd
  if($LASTEXITCODE -ne 0){throw 'Shared native style fixture compilation failed.'}
  $taskProcess=Start-Process -FilePath (Join-Path $taskOutput 'style-components-probe.exe') -WorkingDirectory $taskOutput -WindowStyle Hidden -PassThru
  if(!$taskProcess.WaitForExit(60000)){Stop-Process -Id $taskProcess.Id;throw 'Owned style fixture timed out.'}
  Get-Content -LiteralPath (Join-Path $taskOutput 'style-results.txt')
  Write-Output "Evidence: $taskOutput"
  if($taskProcess.ExitCode -ne 0){throw "Shared native style fixture failed: $($taskProcess.ExitCode)"}
} finally {Pop-Location}
