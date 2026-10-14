param([string]$RepoRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path, [string]$Source='cold_route_probe.cpp', [int]$TimeoutSeconds=60)
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
  $taskProduction=[IO.File]::ReadAllText((Join-Path $RepoRoot 'mainfrm.cpp'))
  $taskMethods=@()
  foreach($taskSignature in @('bool CMainFrame::RouteWorkspaceRecord','void CMainFrame::RestoreWorkspaceRoutes')) {
    $taskStart=$taskProduction.IndexOf($taskSignature)
    if($taskStart -lt 0){throw "Missing production method $taskSignature"}
    $taskBrace=$taskProduction.IndexOf('{',$taskStart)
    $taskDepth=1;$taskEnd=$taskBrace+1
    while($taskDepth -gt 0 -and $taskEnd -lt $taskProduction.Length) {
      if($taskProduction[$taskEnd] -eq '{'){$taskDepth++}
      if($taskProduction[$taskEnd] -eq '}'){$taskDepth--}
      $taskEnd++
    }
    if($taskDepth -ne 0){throw 'Unbalanced production method'}
    $taskMethods += $taskProduction.Substring($taskStart,$taskEnd-$taskStart)
  }
  Set-Content -LiteralPath 'WinRel\workspace-probe-objects\cold-route-methods.inc' -Value ($taskMethods -join "`n") -Encoding UTF8
  $taskPcreObjects=Get-ChildItem 'WinRel' -Filter 'pcre_*.obj'
  if($taskPcreObjects.Count -lt 10){ throw 'Build the Release client first; this probe uses its existing PCRE objects.' }
  $taskResponse=@'
/nologo /Zi /MT /EHsc /D_WIN32_WINNT=0x0601 /DNO_HTMLHELP /DNDEBUG /DWIN32 /D_WINDOWS /DZLIB_DLL /DLUA51 /D_CRT_SECURE_NO_WARNINGS /D_CRT_NON_CONFORMING_SWPRINTFS /DNO_WARN_MBCS_MFC_DEPRECATION /I. /IWinRel\workspace-probe-objects /Iluacom /Iopenssl\include
/FeWinRel\workspace-cold-route-probe.exe /FoWinRel\workspace-probe-objects\
tests\workspace\cold_route_probe.cpp WorkspacePane.cpp InlineImage.cpp workspace\WorkspaceInputPolicy.cpp
/link /DEBUG /SUBSYSTEM:WINDOWS /LTCG Shlwapi.lib WinRel\sqlite3_wrapper.obj
'@
  $taskName=if($Source -eq 'native_soak.cpp'){'workspace-native-soak'}else{'workspace-cold-route-probe'}
  $taskResponse=$taskResponse.Replace('tests\workspace\native_probe.cpp','tests\workspace\'+$Source).Replace('workspace-cold-route-probe',$taskName).Replace('Shlwapi.lib','Shlwapi.lib Psapi.lib')
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
