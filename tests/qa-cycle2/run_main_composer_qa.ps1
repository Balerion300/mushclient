$ErrorActionPreference='Stop'
$taskRepo=(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$taskOriginal=Join-Path $taskRepo 'tests\workspace\run_composer_probe.ps1'
$taskScript=(Get-Content -LiteralPath $taskOriginal -Raw)
$taskScript=$taskScript.Replace('tests\workspace\composer_probe.cpp','tests\qa-cycle2\main_composer_qa.cpp')
$taskScript=$taskScript.Replace('composer-results.txt','composer_qa.txt')
$taskScript=$taskScript.Replace('mushclient-composer-','mushclient-main-composer-qa-')
$taskDerived=Join-Path ([IO.Path]::GetTempPath()) ('main-composer-qa-runner-'+[guid]::NewGuid().ToString('N')+'.ps1')
Set-Content -LiteralPath $taskDerived -Value $taskScript
& $taskDerived -RepoRoot $taskRepo -Configuration Debug
