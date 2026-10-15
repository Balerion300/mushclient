param([string]$RepoRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path,[int]$TimeoutSeconds=60)
$ErrorActionPreference='Stop'
$taskVswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$taskVsRoot=& $taskVswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$taskCompilerRoot=(Get-ChildItem (Join-Path $taskVsRoot 'VC\Tools\MSVC') -Directory | Sort-Object {[version]$_.Name} -Descending | Select-Object -First 1).FullName
$taskTemporary=Join-Path $env:TEMP ('mushclient-appearance-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $taskTemporary | Out-Null
Set-Content -LiteralPath (Join-Path $taskTemporary 'framework.rc') -Value @('#include "afxres.h"','LANGUAGE LANG_ENGLISH, SUBLANG_ENGLISH_US','#include "afxribbon.rc"') -Encoding ascii
$taskProduction=[IO.File]::ReadAllText((Join-Path $RepoRoot 'WorkspaceStyle.cpp')).Replace('#include "stdafx.h"','')
Set-Content -LiteralPath (Join-Path $taskTemporary 'style-production.inc') -Value $taskProduction -Encoding UTF8
$taskResponse=@('/nologo /O2 /MT /EHsc /std:c++14 /D_WIN32_WINNT=0x0601 /DNO_HTMLHELP /DNDEBUG /DWIN32 /D_WINDOWS /DZLIB_DLL /DLUA51 /D_CRT_SECURE_NO_WARNINGS /D_CRT_NON_CONFORMING_SWPRINTFS /DNO_WARN_MBCS_MFC_DEPRECATION',('/I"'+$RepoRoot+'" /I"'+$RepoRoot+'\luacom" /I"'+$RepoRoot+'\openssl\include" /I"'+$taskTemporary+'"'),('/Fe"'+$taskTemporary+'\appearance-probe.exe" /Fo"'+$taskTemporary+'\\"'),('"'+$RepoRoot+'\tests\workspace\appearance_probe.cpp"'),('"'+$RepoRoot+'\tests\workspace\style_platform_adapter.cpp"'),('"'+$RepoRoot+'\WorkspaceAppearance.cpp"'),('"'+$taskTemporary+'\framework.res"'),'/link /SUBSYSTEM:WINDOWS Shlwapi.lib Oleacc.lib')
Set-Content -LiteralPath (Join-Path $taskTemporary 'build.rsp') -Value $taskResponse -Encoding ascii
Set-Content -LiteralPath (Join-Path $taskTemporary 'build.cmd') -Value @('@echo off',('call "'+$taskVsRoot+'\VC\Auxiliary\Build\vcvars32.bat" >nul'),('set LIB='+$taskCompilerRoot+'\atlmfc\lib\spectre\x86;%LIB%'),('cd /d "'+$taskTemporary+'"'),('rc /nologo /i "'+$taskCompilerRoot+'\atlmfc\include" /fo framework.res framework.rc'),'if errorlevel 1 exit /b %errorlevel%',('cl @"'+$taskTemporary+'\build.rsp"')) -Encoding ascii
& cmd.exe /d /c (Join-Path $taskTemporary 'build.cmd')
if($LASTEXITCODE -ne 0){throw ('Appearance fixture compilation failed; '+$taskTemporary)}
Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $taskTemporary 'appearance-probe.exe'),(Join-Path $RepoRoot 'WorkspaceAppearance.cpp'),(Join-Path $RepoRoot 'WorkspaceStyle.cpp') | Format-List | Out-File -LiteralPath (Join-Path $taskTemporary 'identity.txt')
$taskProcess=Start-Process -FilePath (Join-Path $taskTemporary 'appearance-probe.exe') -WorkingDirectory $taskTemporary -WindowStyle Hidden -PassThru
try{if(!$taskProcess.WaitForExit($TimeoutSeconds*1000)){throw ('Appearance fixture timed out; '+$taskTemporary)};Get-Content -LiteralPath (Join-Path $taskTemporary 'appearance-results.txt');Write-Output ('Evidence: '+$taskTemporary);if($taskProcess.ExitCode -ne 0){throw ('Appearance fixture failed: '+$taskProcess.ExitCode)}}finally{if(!$taskProcess.HasExited){Stop-Process -Id $taskProcess.Id -Force}}
