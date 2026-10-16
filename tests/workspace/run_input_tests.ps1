$ErrorActionPreference='Stop'
$taskRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$taskVswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$taskVsRoot=& $taskVswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(-not $taskVsRoot){throw 'Visual C++ tools are required.'}
$taskTemporary=Join-Path ([IO.Path]::GetTempPath()) ('mushclient-input-tests-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $taskTemporary | Out-Null
$taskVcvars=Join-Path $taskVsRoot 'VC\Auxiliary\Build\vcvars32.bat'
foreach($taskCase in @('WorkspaceInputPolicy','WorkspaceInputEncoding','WorkspaceSharedHistory')){
  $taskCmd=Join-Path $taskTemporary ($taskCase+'.cmd')
  $taskExe=Join-Path $taskTemporary ($taskCase+'.exe')
  $taskSource=Join-Path $taskRoot ('workspace\'+$taskCase+'.cpp')
  if($taskCase -eq 'WorkspaceSharedHistory'){$taskSource=Join-Path $taskRoot 'workspace\WorkspaceSharedHistoryTests.cpp'}
  $taskTests=Join-Path $taskRoot ('workspace\'+$taskCase+'Tests.cpp')
  if($taskCase -eq 'WorkspaceSharedHistory'){$taskTests=$taskSource;$taskSource=Join-Path $taskRoot 'workspace\WorkspaceInputPolicy.cpp'}
  Set-Content -LiteralPath $taskCmd -Encoding ASCII -Value @('@echo off',('call "'+$taskVcvars+'" >nul'),('cd /d "'+$taskTemporary+'"'),('cl /nologo /EHsc /std:c++14 /MT "'+$taskSource+'" "'+$taskTests+'" /Fe:"'+$taskExe+'"'), 'if errorlevel 1 exit /b 1', ('"'+$taskExe+'"'))
  & cmd.exe /d /c $taskCmd
  if($LASTEXITCODE -ne 0){throw ($taskCase+' failed.')}
}
