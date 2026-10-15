param([string]$RepoRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path)
$ErrorActionPreference='Stop'
$taskVswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$taskVsRoot=& $taskVswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(-not $taskVsRoot){throw 'Visual C++ tools are required.'}
$taskCompilerRoot=(Get-ChildItem (Join-Path $taskVsRoot 'VC\Tools\MSVC') -Directory | Sort-Object {[version]$_.Name} -Descending | Select-Object -First 1).FullName
$taskSdkBase=Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10'
$taskSdkVersion=(Get-ChildItem (Join-Path $taskSdkBase 'Include') -Directory | Where-Object {Test-Path (Join-Path $_.FullName 'ucrt\new.h')} | Sort-Object {[version]$_.Name} -Descending | Select-Object -First 1).Name
$taskOldInclude=$env:INCLUDE
$taskOldLib=$env:LIB
$taskLocation=Get-Location
try {
  Set-Location -LiteralPath $RepoRoot
  $env:INCLUDE="${taskCompilerRoot}\include;${taskCompilerRoot}\atlmfc\include;${taskSdkBase}\Include\${taskSdkVersion}\ucrt;${taskSdkBase}\Include\${taskSdkVersion}\shared;${taskSdkBase}\Include\${taskSdkVersion}\um"
  $taskMfcLib=Join-Path $taskCompilerRoot 'atlmfc\lib\spectre\x86'
  if(-not(Test-Path $taskMfcLib)){$taskMfcLib=Join-Path $taskCompilerRoot 'atlmfc\lib\x86'}
  $env:LIB="${taskCompilerRoot}\lib\x86;${taskMfcLib};${taskSdkBase}\Lib\${taskSdkVersion}\ucrt\x86;${taskSdkBase}\Lib\${taskSdkVersion}\um\x86"
  $taskPcreObjects=Get-ChildItem 'WinRel' -Filter 'pcre_*.obj'
  if($taskPcreObjects.Count -lt 10){throw 'Release PCRE objects are required.'}
  $taskObjectDir='tests\qa-cycle1\objects'
  New-Item -ItemType Directory -Force $taskObjectDir | Out-Null
  $taskResponse=@'
/nologo /MT /EHsc /D_WIN32_WINNT=0x0601 /DNO_HTMLHELP /DNDEBUG /DWIN32 /D_WINDOWS /DZLIB_DLL /DLUA51 /D_CRT_SECURE_NO_WARNINGS /D_CRT_NON_CONFORMING_SWPRINTFS /DNO_WARN_MBCS_MFC_DEPRECATION /I. /Iluacom /Iopenssl\include
/Fetests\qa-cycle1\focus_reactivation_probe.exe /Fotests\qa-cycle1\objects\
tests\qa-cycle1\focus_reactivation_probe.cpp WorkspacePane.cpp WorkspaceStyle.cpp InlineImage.cpp workspace\WorkspaceInputPolicy.cpp
/link /SUBSYSTEM:WINDOWS /LTCG Shlwapi.lib Psapi.lib
'@
  $taskResponse += "`n"+(($taskPcreObjects | ForEach-Object {'"'+$_.FullName+'"'}) -join ' ')
  Set-Content -LiteralPath 'tests\qa-cycle1\objects\build.rsp' -Value $taskResponse
  & (Join-Path $taskCompilerRoot 'bin\Hostx64\x86\cl.exe') '@tests\qa-cycle1\objects\build.rsp'
  if($LASTEXITCODE -ne 0){throw 'QA probe compilation failed.'}
  $taskProcess=Start-Process -FilePath (Join-Path $RepoRoot 'tests\qa-cycle1\focus_reactivation_probe.exe') -WorkingDirectory $RepoRoot -WindowStyle Hidden -PassThru
  if(-not $taskProcess.WaitForExit(30000)){throw "QA probe timed out; process ID $($taskProcess.Id)."}
  $taskReceipt=Get-Content -LiteralPath 'tests\qa-cycle1\focus_reactivation.txt'
  $taskReceipt
  if($taskProcess.ExitCode -ne 0){throw "QA probe failed: exit $($taskProcess.ExitCode)."}
  if($taskReceipt[-1] -notmatch '^PASS '){throw 'QA focus assertion failed.'}
} finally {
  $env:INCLUDE=$taskOldInclude
  $env:LIB=$taskOldLib
  Set-Location -LiteralPath $taskLocation
}
