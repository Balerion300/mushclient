param([ValidateSet('Start','Stop','Status')][string]$Action='Status')
$ErrorActionPreference='Stop'
$taskDirectory=Join-Path ([IO.Path]::GetTempPath()) 'MUSHclient-Profiling'
$taskMarker=Join-Path $taskDirectory 'owned-cpu-recording.json'
if($Action -eq 'Status'){& wpr.exe -status;if($LASTEXITCODE -ne 0){throw 'WPR status failed.'};return}
$taskPrincipal=New-Object Security.Principal.WindowsPrincipal([Security.Principal.WindowsIdentity]::GetCurrent())
if(-not $taskPrincipal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)){
  throw 'Run this script in PowerShell opened with Run as administrator. WPR CPU recording requires an elevated token.'
}
if($Action -eq 'Start'){
  if(Test-Path -LiteralPath $taskMarker){throw 'This helper already owns a recording marker. Stop that recording before starting another.'}
  $taskStatus=(& wpr.exe -status 2>&1 | Out-String)
  if($taskStatus -notmatch 'WPR is not recording'){throw "A recording may already be active; this helper will not alter it. $taskStatus"}
  New-Item -ItemType Directory -Path $taskDirectory -Force | Out-Null
  $taskOutput=Join-Path $taskDirectory ('MUSHclient-CPU-'+(Get-Date -Format 'yyyyMMdd-HHmmss')+'.etl')
  & wpr.exe -start CPU -filemode
  if($LASTEXITCODE -ne 0){throw 'WPR could not start. If this terminal is elevated, check the Profile system performance user right.'}
  @{output=$taskOutput;started=(Get-Date).ToString('o')} | ConvertTo-Json | Set-Content -LiteralPath $taskMarker -Encoding UTF8
  Write-Output 'Recording CPU. Reproduce the slowdown for 30-60 seconds, then run this script with -Action Stop.'
} else {
  if(-not(Test-Path -LiteralPath $taskMarker)){throw 'This helper has no owned recording; it will not stop somebody else''s trace.'}
  $taskRecording=Get-Content -LiteralPath $taskMarker -Raw | ConvertFrom-Json
  & wpr.exe -stop $taskRecording.output
  if($LASTEXITCODE -ne 0){throw 'WPR could not stop; the ownership marker has been retained.'}
  Remove-Item -LiteralPath $taskMarker
  Write-Output $taskRecording.output
}
