param([string]$RepoRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path)
$ErrorActionPreference='Stop'
$taskOutput=Join-Path $env:TEMP ('valyria-cycle-input-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $taskOutput | Out-Null
$taskMain=[IO.File]::ReadAllText((Join-Path $RepoRoot 'mainfrm.cpp'))
function Extract-Function([string]$signature){$taskStart=$taskMain.IndexOf($signature);if($taskStart -lt 0){throw "Missing $signature"};$taskBrace=$taskMain.IndexOf('{',$taskStart);$taskDepth=1;$taskEnd=$taskBrace+1;while($taskDepth -gt 0 -and $taskEnd -lt $taskMain.Length){if($taskMain[$taskEnd] -eq '{'){$taskDepth++};if($taskMain[$taskEnd] -eq '}'){$taskDepth--};$taskEnd++};if($taskDepth){throw 'Unbalanced production source'};return $taskMain.Substring($taskStart,$taskEnd-$taskStart)}
$taskFunctions=@('bool CMainFrame::CycleWorkspaceInput(') | ForEach-Object {Extract-Function $_}
Set-Content -LiteralPath (Join-Path $taskOutput 'cycle-input-production.inc') -Value ($taskFunctions -join "`r`n") -Encoding UTF8
$taskResponse=@"
/nologo /MT /EHsc /std:c++14 /D_WIN32_WINNT=0x0602 /DNO_HTMLHELP /DNDEBUG /DWIN32 /D_WINDOWS /DZLIB_DLL /DLUA51 /D_CRT_SECURE_NO_WARNINGS /D_CRT_NON_CONFORMING_SWPRINTFS /DNO_WARN_MBCS_MFC_DEPRECATION
/I"$RepoRoot" /I"$RepoRoot\luacom" /I"$RepoRoot\openssl\include" /I"$taskOutput"
/Fe"$taskOutput\cycle-input-probe.exe" /Fo"$taskOutput\\"
"$RepoRoot\tests\workspace\cycle_input_probe.cpp"
/link /SUBSYSTEM:WINDOWS
"@
Set-Content -LiteralPath (Join-Path $taskOutput 'build.rsp') -Value $taskResponse -Encoding ascii
Set-Content -LiteralPath (Join-Path $taskOutput 'build.cmd') -Encoding ascii -Value @"
@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars32.bat" >nul
set "LIB=%VCToolsInstallDir%atlmfc\lib\spectre\x86;%LIB%"
cl @"$taskOutput\build.rsp"
"@
Push-Location -LiteralPath $taskOutput
try {& .\build.cmd;if($LASTEXITCODE -ne 0){throw 'Keyboard cycle fixture compilation failed'};Write-Output "Built only: $taskOutput"} finally {Pop-Location}
