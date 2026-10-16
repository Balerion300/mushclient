$ErrorActionPreference='Stop'
$taskRepo=(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$taskBase=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'build_spell_probe.ps1') -Raw
$taskBase=$taskBase.Replace('tests\workspace\spell_probe.cpp','tests\workspace\dialog_ux_probe.cpp').Replace('mushclient-spell-probe-','mushclient-dialog-ux-').Replace('spell-probe.exe','dialog-ux-probe.exe')
$taskRunner=Join-Path $env:TEMP ('mushclient-dialog-build-'+[guid]::NewGuid().ToString('N')+'.ps1');Set-Content -LiteralPath $taskRunner -Value $taskBase -Encoding utf8
$taskResult=& $taskRunner -RepoRoot $taskRepo -Configuration Release
$taskResult | ForEach-Object {Write-Output $_}
$taskEvidence=($taskResult | Where-Object {$_ -like 'Built only: *'} | Select-Object -Last 1).Substring(12)
$taskProcess=Start-Process -FilePath (Join-Path $taskEvidence 'dialog-ux-probe.exe') -WorkingDirectory $taskEvidence -WindowStyle Hidden -PassThru
if(!$taskProcess.WaitForExit(20000)){Stop-Process -Id $taskProcess.Id;throw 'Owned hidden dialog fixture timed out.'}
Get-Content -LiteralPath (Join-Path $taskEvidence 'dialog-ux-results.txt');Write-Output ('Evidence: '+$taskEvidence)
if($taskProcess.ExitCode -ne 0){throw ('Hidden dialog probe failed: '+$taskProcess.ExitCode)}
