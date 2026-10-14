param([Parameter(Mandatory=$true)][int]$ProcessId,
      [Parameter(Mandatory=$true)][string]$ExpectedExecutable,
      [string]$Output='WinRel\cpu-sample.txt')
$ErrorActionPreference='Stop'
$taskRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$taskVswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$taskVs=& $taskVswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(-not $taskVs){throw 'VS2022 C++ tools are required.'}
$taskTemporary=Join-Path ([IO.Path]::GetTempPath()) ('mush-sampler-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $taskTemporary | Out-Null
$taskBuild=Join-Path $taskTemporary 'build.cmd'
@('@echo off',('call "'+$taskVs+'\VC\Auxiliary\Build\vcvars32.bat" >nul'),
  ('cd /d "'+$taskRoot+'"'),
  'cl /nologo /EHsc /MT /O2 tests\workspace\sample_cpu.cpp /FoWinRel\sample-cpu.obj /FeWinRel\sample-cpu.exe /link Dbghelp.lib Psapi.lib User32.lib') | Set-Content -LiteralPath $taskBuild -Encoding ASCII
& cmd.exe /d /c $taskBuild
if($LASTEXITCODE -ne 0){throw 'Sampler build failed.'}
Push-Location -LiteralPath $taskRoot
try {
  & '.\WinRel\sample-cpu.exe' $ProcessId $ExpectedExecutable 'WinRel' $Output
  if($LASTEXITCODE -ne 0){throw "Sampler failed or refused target: $LASTEXITCODE"}
  Write-Output (Join-Path $taskRoot $Output)
} finally {Pop-Location}
