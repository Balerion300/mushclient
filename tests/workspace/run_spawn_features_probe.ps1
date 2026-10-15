param([string]$RepoRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path)
$ErrorActionPreference='Stop'
$taskOutput=Join-Path $env:TEMP ('mushclient-spawn-features-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $taskOutput | Out-Null
$taskMain=[IO.File]::ReadAllText((Join-Path $RepoRoot 'mainfrm.cpp'))
$taskIntegration=[IO.File]::ReadAllText((Join-Path $RepoRoot 'WorkspaceIntegration.cpp'))
function Extract-Function([string]$source,[string]$signature) {
  $taskStart=$source.IndexOf($signature)
  if($taskStart -lt 0){throw "Missing production function: $signature"}
  $taskBrace=$source.IndexOf('{',$taskStart);$taskDepth=1;$taskEnd=$taskBrace+1
  while($taskDepth -gt 0 -and $taskEnd -lt $source.Length){if($source[$taskEnd] -eq '{'){$taskDepth++};if($source[$taskEnd] -eq '}'){$taskDepth--};$taskEnd++}
  if($taskDepth){throw 'Unbalanced production function'}
  return $source.Substring($taskStart,$taskEnd-$taskStart)
}
$taskFunctions=@()
foreach($taskSignature in @('bool WorkspaceSpawnSchema(', 'bool CMainFrame::AddWorkspaceRoute(', 'bool CMainFrame::RouteWorkspaceRecord(', 'void CMainFrame::RestoreWorkspaceRoutes(', 'workspace_spawn::Preferences CMainFrame::WorkspaceTranscriptPreferences(', 'bool CMainFrame::WorkspaceShortcutAvailable(', 'bool CMainFrame::SetWorkspaceTranscriptPreferences(', 'std::vector<SpawnNavigation> WorkspaceNavigation(', 'bool CMainFrame::FocusWorkspaceTranscript(', 'bool CMainFrame::FocusRelativeWorkspaceTranscript(', 'bool CMainFrame::HandleWorkspaceShortcut(', 'void CMainFrame::ClearWorkspaceTranscript(')) {
  $taskFunctions+=Extract-Function $taskMain $taskSignature
}
$taskFunctions+=Extract-Function $taskIntegration 'bool WorkspacePublishRecord('
Set-Content -LiteralPath (Join-Path $taskOutput 'spawn-production.inc') -Value ($taskFunctions -join "`r`n") -Encoding UTF8
$taskSources=@('tests\workspace\spawn_features_probe.cpp','WorkspacePane.cpp','WorkspaceStyle.cpp','InlineImage.cpp','workspace\WorkspaceInputPolicy.cpp') | ForEach-Object {'"'+(Join-Path $RepoRoot $_)+'"'}
$taskPcre=Get-ChildItem (Join-Path $RepoRoot 'WinRel') -Filter 'pcre_*.obj'
if($taskPcre.Count -lt 10){throw 'Existing Release PCRE objects are required; this fixture never builds the solution.'}
$taskResponse=@"
/nologo /MT /EHsc /D_WIN32_WINNT=0x0601 /DNO_HTMLHELP /DNDEBUG /DWIN32 /D_WINDOWS /DZLIB_DLL /DLUA51 /D_CRT_SECURE_NO_WARNINGS /D_CRT_NON_CONFORMING_SWPRINTFS /DNO_WARN_MBCS_MFC_DEPRECATION
/I"$RepoRoot" /I"$RepoRoot\luacom" /I"$RepoRoot\openssl\include" /I"$taskOutput"
/Fe"$taskOutput\spawn-features-probe.exe" /Fo"$taskOutput\\"
$($taskSources -join ' ')
/link /SUBSYSTEM:WINDOWS /LTCG Shlwapi.lib "$RepoRoot\WinRel\sqlite3_wrapper.obj"
$((@($taskPcre | ForEach-Object {'"'+$_.FullName+'"'})) -join ' ')
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
  if($LASTEXITCODE -ne 0){throw 'Focused spawn fixture compilation failed.'}
  $taskProcess=Start-Process -FilePath (Join-Path $taskOutput 'spawn-features-probe.exe') -WorkingDirectory $RepoRoot -WindowStyle Hidden -PassThru
  if(!$taskProcess.WaitForExit(60000)){Stop-Process -Id $taskProcess.Id;throw 'Owned spawn fixture timed out.'}
  Get-Content -LiteralPath (Join-Path $taskOutput 'spawn-results.txt')
  Write-Output "Evidence: $taskOutput"
  if($taskProcess.ExitCode -ne 0){throw "Focused spawn fixture failed: $($taskProcess.ExitCode)"}
} finally {Pop-Location}
