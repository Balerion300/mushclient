param([string]$RepoRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path,[int]$TimeoutSeconds=60,[switch]$BuildOnly,[switch]$LegacyNotepad)
$ErrorActionPreference='Stop'
$taskVswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$taskVsRoot=& $taskVswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$taskCompilerRoot=(Get-ChildItem (Join-Path $taskVsRoot 'VC\Tools\MSVC') -Directory | Sort-Object {[version]$_.Name} -Descending | Select-Object -First 1).FullName
$taskTemporary=Join-Path $env:TEMP ('mushclient-grouped-mdi-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $taskTemporary | Out-Null
Set-Content -LiteralPath (Join-Path $taskTemporary 'framework.rc') -Value @('#include "afxres.h"','LANGUAGE LANG_ENGLISH, SUBLANG_ENGLISH_US','#include "afxribbon.rc"') -Encoding ascii
$taskResponse=@('/nologo /Zi /Od /MT /EHsc /std:c++14 /D_WIN32_WINNT=0x0601 /DNO_HTMLHELP /DNDEBUG /DWIN32 /D_WINDOWS /DZLIB_DLL /DLUA51 /D_CRT_SECURE_NO_WARNINGS /D_CRT_NON_CONFORMING_SWPRINTFS /DNO_WARN_MBCS_MFC_DEPRECATION',('/I"'+$RepoRoot+'" /I"'+$RepoRoot+'\luacom" /I"'+$RepoRoot+'\openssl\include"'),('/Fe"'+$taskTemporary+'\grouped-mdi-probe.exe" /Fo"'+$taskTemporary+'\\"'),('"'+$RepoRoot+'\tests\workspace\grouped_mdi_probe.cpp"'),('"'+$RepoRoot+'\WorkspaceStyle.cpp"'),('"'+$RepoRoot+'\WorkspaceAppearance.cpp"'),('"'+$taskTemporary+'\framework.res"'),'/link /DEBUG /SUBSYSTEM:WINDOWS Shlwapi.lib')
Set-Content -LiteralPath (Join-Path $taskTemporary 'build.rsp') -Value $taskResponse -Encoding ascii
Set-Content -LiteralPath (Join-Path $taskTemporary 'build.cmd') -Value @('@echo off',('call "'+$taskVsRoot+'\VC\Auxiliary\Build\vcvars32.bat" >nul'),('set LIB='+$taskCompilerRoot+'\atlmfc\lib\spectre\x86;%LIB%'),('cd /d "'+$taskTemporary+'"'),('rc /nologo /i "'+$taskCompilerRoot+'\atlmfc\include" /fo framework.res framework.rc'),'if errorlevel 1 exit /b %errorlevel%',('cl @"'+$taskTemporary+'\build.rsp"')) -Encoding ascii
& cmd.exe /d /c (Join-Path $taskTemporary 'build.cmd')
if($LASTEXITCODE -ne 0){throw ('Grouped MDI fixture compilation failed; '+$taskTemporary)}
Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $taskTemporary 'grouped-mdi-probe.exe'),(Join-Path $RepoRoot 'tests\workspace\grouped_mdi_probe.cpp'),(Join-Path $RepoRoot 'WorkspaceAppearance.cpp'),(Join-Path $RepoRoot 'WorkspaceStyle.cpp') | Format-List | Out-File -LiteralPath (Join-Path $taskTemporary 'identity.txt')
if($BuildOnly){Write-Output ('Evidence: '+$taskTemporary);return}
$taskArgument=if($LegacyNotepad){'--legacy-notepad'}else{'--native-notepad'}
$taskProcess=Start-Process -FilePath (Join-Path $taskTemporary 'grouped-mdi-probe.exe') -ArgumentList $taskArgument -WorkingDirectory $taskTemporary -WindowStyle Hidden -PassThru
try{if(!$taskProcess.WaitForExit($TimeoutSeconds*1000)){throw ('Grouped MDI fixture timed out; '+$taskTemporary)};Get-Content -LiteralPath (Join-Path $taskTemporary 'grouped-mdi-results.txt');Write-Output ('Evidence: '+$taskTemporary);if($taskProcess.ExitCode -ne 0){throw ('Grouped MDI fixture failed: '+$taskProcess.ExitCode)}}finally{if(!$taskProcess.HasExited){Stop-Process -Id $taskProcess.Id -Force}}
