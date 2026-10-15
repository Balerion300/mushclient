param([string]$RepoRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path,[int]$TimeoutSeconds=60)
$ErrorActionPreference='Stop'
$taskVswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$taskVsRoot=& $taskVswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(-not $taskVsRoot){throw 'Visual C++ tools are required.'}
$taskCompilerRoot=(Get-ChildItem (Join-Path $taskVsRoot 'VC\Tools\MSVC') -Directory | Sort-Object {[version]$_.Name} -Descending | Select-Object -First 1).FullName
$taskMfcLib=Join-Path $taskCompilerRoot 'atlmfc\lib\spectre\x86'
if(-not(Test-Path -LiteralPath $taskMfcLib)){$taskMfcLib=Join-Path $taskCompilerRoot 'atlmfc\lib\x86'}
$taskTemporary=Join-Path ([IO.Path]::GetTempPath()) ('mushclient-dock-layout-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $taskTemporary | Out-Null
$taskPcreObjects=Get-ChildItem -LiteralPath (Join-Path $RepoRoot 'WinRel') -Filter 'pcre_*.obj'
if($taskPcreObjects.Count -lt 10){throw 'Existing Release PCRE objects are required.'}
$taskResponse=@(
  '/nologo /O2 /MT /EHsc /D_WIN32_WINNT=0x0601 /DNO_HTMLHELP /DNDEBUG /DWIN32 /D_WINDOWS /DZLIB_DLL /DLUA51 /D_CRT_SECURE_NO_WARNINGS /D_CRT_NON_CONFORMING_SWPRINTFS /DNO_WARN_MBCS_MFC_DEPRECATION',
  ('/I"'+$RepoRoot+'" /I"'+$RepoRoot+'\luacom" /I"'+$RepoRoot+'\openssl\include"'),
  ('/Fe"'+$taskTemporary+'\dock-layout-probe.exe" /Fo"'+$taskTemporary+'\\"'),
  ('"'+$RepoRoot+'\tests\workspace\dock_layout_probe.cpp"'),
  ('"'+$RepoRoot+'\WorkspacePane.cpp"'),
  ('"'+$RepoRoot+'\InlineImage.cpp"'),
  ('"'+$RepoRoot+'\workspace\WorkspaceInputPolicy.cpp"'),
  '/link /SUBSYSTEM:WINDOWS /LTCG Shlwapi.lib Psapi.lib'
)
$taskResponse += $taskPcreObjects | ForEach-Object {'"'+$_.FullName+'"'}
$taskRsp=Join-Path $taskTemporary 'build.rsp';Set-Content -LiteralPath $taskRsp -Encoding ASCII -Value $taskResponse
$taskCmd=Join-Path $taskTemporary 'build.cmd'
Set-Content -LiteralPath $taskCmd -Encoding ASCII -Value @('@echo off',('call "'+$taskVsRoot+'\VC\Auxiliary\Build\vcvars32.bat" >nul'),('set LIB='+$taskMfcLib+';%LIB%'),('cd /d "'+$taskTemporary+'"'),('cl @"'+$taskRsp+'"'))
& cmd.exe /d /c $taskCmd
if($LASTEXITCODE -ne 0){throw ('Dock layout fixture compilation failed; '+$taskTemporary)}
$taskProcess=Start-Process -FilePath (Join-Path $taskTemporary 'dock-layout-probe.exe') -WorkingDirectory $taskTemporary -WindowStyle Hidden -PassThru
try {
  if(-not $taskProcess.WaitForExit($TimeoutSeconds*1000)){throw ('Dock layout fixture timed out; '+$taskTemporary)}
  Get-Content -LiteralPath (Join-Path $taskTemporary 'dock-layout-results.txt')
  Write-Output ('Evidence: '+$taskTemporary)
  if($taskProcess.ExitCode -ne 0){throw ('Dock layout fixture failed: '+$taskProcess.ExitCode)}
} finally {
  if(-not $taskProcess.HasExited){Stop-Process -Id $taskProcess.Id -Force}
}
