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
  New-Item -ItemType Directory -Force 'WinRel\workspace-probe-objects' | Out-Null
  & (Join-Path $taskSdkBase "bin\$taskSdkVersion\x64\rc.exe") /nologo /I "${taskCompilerRoot}\atlmfc\include" /I "${taskSdkBase}\Include\${taskSdkVersion}\um" /I "${taskSdkBase}\Include\${taskSdkVersion}\shared" /fo WinRel\workspace-probe-objects\workspace-modern-resources.res MUSHclient.rc
  if($LASTEXITCODE -ne 0){throw 'Native workspace resource compilation failed.'}
  $taskPcreObjects=Get-ChildItem 'WinRel' -Filter 'pcre_*.obj'
  if($taskPcreObjects.Count -lt 10){ throw 'Build the Release client first; this probe uses its existing PCRE objects.' }
  $taskResponse=@'
/nologo /Zi /MT /EHsc /D_WIN32_WINNT=0x0601 /DNO_HTMLHELP /DNDEBUG /DWIN32 /D_WINDOWS /DZLIB_DLL /DLUA51 /D_CRT_SECURE_NO_WARNINGS /D_CRT_NON_CONFORMING_SWPRINTFS /DNO_WARN_MBCS_MFC_DEPRECATION /I. /Iluacom /Iopenssl\include
/FeWinRel\workspace-image-pixels.exe /FoWinRel\workspace-probe-objects\
tests\workspace\image_pixel_probe.cpp WorkspacePane.cpp WorkspaceStyle.cpp InlineImage.cpp workspace\WorkspaceInputPolicy.cpp
/link /DEBUG /SUBSYSTEM:WINDOWS /LTCG WinRel\workspace-probe-objects\workspace-modern-resources.res Shlwapi.lib
'@
  $taskName='workspace-image-pixels'
  $taskResponse=$taskResponse.Replace('Shlwapi.lib','Shlwapi.lib Psapi.lib')
  $taskResponse += "`n"+ (($taskPcreObjects | ForEach-Object { '"'+$_.FullName+'"' }) -join ' ')
  Set-Content -LiteralPath 'WinRel\workspace-probe-objects\build.rsp' -Value $taskResponse
  & (Join-Path $taskCompilerRoot 'bin\Hostx64\x86\cl.exe') '@WinRel\workspace-probe-objects\build.rsp'
  if($LASTEXITCODE -ne 0){ throw 'Native probe compilation failed.' }
  $taskProcess=Start-Process -FilePath (Join-Path $RepoRoot ('WinRel\'+$taskName+'.exe')) -WorkingDirectory $RepoRoot -WindowStyle Hidden -PassThru
  if(-not $taskProcess.WaitForExit($TimeoutSeconds*1000)){ throw "Native probe did not finish; process ID $($taskProcess.Id)." }
  Get-Content -LiteralPath ('WinRel\'+$taskName+'.txt')
  if($taskProcess.ExitCode -ne 0){ throw "Native probe failed: exit $($taskProcess.ExitCode)." }
} finally {
  $env:INCLUDE=$taskOldInclude
  $env:LIB=$taskOldLib
  Set-Location -LiteralPath $taskLocation
}
