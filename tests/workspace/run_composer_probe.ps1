param([string]$RepoRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path,[int]$TimeoutSeconds=60,[ValidateSet("Release","Debug")][string]$Configuration="Release")
$ErrorActionPreference='Stop'
$taskVswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$taskVsRoot=& $taskVswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(-not $taskVsRoot){throw 'Visual C++ tools are required.'}
$taskCompilerRoot=(Get-ChildItem (Join-Path $taskVsRoot 'VC\Tools\MSVC') -Directory | Sort-Object {[version]$_.Name} -Descending | Select-Object -First 1).FullName
$taskMfcLib=Join-Path $taskCompilerRoot 'atlmfc\lib\spectre\x86'
if(-not(Test-Path -LiteralPath $taskMfcLib)){$taskMfcLib=Join-Path $taskCompilerRoot 'atlmfc\lib\x86'}
$taskTemporary=Join-Path ([IO.Path]::GetTempPath()) ('mushclient-composer-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $taskTemporary | Out-Null
Set-Content -LiteralPath (Join-Path $taskTemporary 'framework.rc') -Value @('#include "afxres.h"','LANGUAGE LANG_ENGLISH, SUBLANG_ENGLISH_US','#include "afxribbon.rc"') -Encoding ascii
$taskResponse=@('/nologo /O2 /MT /EHsc /std:c++14 /D_WIN32_WINNT=0x0601 /DNO_HTMLHELP /DNDEBUG /DWIN32 /D_WINDOWS /DZLIB_DLL /DLUA51 /D_CRT_SECURE_NO_WARNINGS /D_CRT_NON_CONFORMING_SWPRINTFS /DNO_WARN_MBCS_MFC_DEPRECATION',('/I"'+$RepoRoot+'" /I"'+$RepoRoot+'\luacom" /I"'+$RepoRoot+'\openssl\include"'),('/Fe"'+$taskTemporary+'\composer-probe.exe" /Fo"'+$taskTemporary+'\\"'),('"'+$RepoRoot+'\tests\workspace\composer_probe.cpp"'),('"'+$RepoRoot+'\WorkspaceStyle.cpp"'),('"'+$taskTemporary+'\framework.res"'),'/link /SUBSYSTEM:WINDOWS Shlwapi.lib Dwmapi.lib UxTheme.lib')
if($Configuration -eq 'Debug'){$taskResponse[0]=$taskResponse[0].Replace('/O2 /MT','/Od /MTd').Replace('/DNDEBUG','/D_DEBUG')}
$taskRsp=Join-Path $taskTemporary 'build.rsp';Set-Content -LiteralPath $taskRsp -Encoding ASCII -Value $taskResponse
$taskCmd=Join-Path $taskTemporary 'build.cmd';Set-Content -LiteralPath $taskCmd -Encoding ASCII -Value @('@echo off',('call "'+$taskVsRoot+'\VC\Auxiliary\Build\vcvars32.bat" >nul'),('set LIB='+$taskMfcLib+';%LIB%'),('cd /d "'+$taskTemporary+'"'),('rc /nologo /i "'+$taskCompilerRoot+'\atlmfc\include" /fo framework.res framework.rc'),'if errorlevel 1 exit /b %errorlevel%',('cl @"'+$taskRsp+'"'))
& cmd.exe /d /c $taskCmd
if($LASTEXITCODE -ne 0){throw ('Composer fixture compilation failed; '+$taskTemporary)}
Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $taskTemporary 'composer-probe.exe'),(Join-Path $RepoRoot 'workspace\WorkspaceComposerSplitter.h'),(Join-Path $RepoRoot 'WorkspaceStyle.cpp') | Format-List | Out-File -LiteralPath (Join-Path $taskTemporary 'identity.txt')
$taskProcess=Start-Process -FilePath (Join-Path $taskTemporary 'composer-probe.exe') -WorkingDirectory $taskTemporary -WindowStyle Hidden -PassThru
try {if(-not $taskProcess.WaitForExit($TimeoutSeconds*1000)){throw ('Composer fixture timed out; '+$taskTemporary)};Get-Content -LiteralPath (Join-Path $taskTemporary 'composer-results.txt');Write-Output ('Evidence: '+$taskTemporary);if($taskProcess.ExitCode -ne 0){throw ('Composer fixture failed: '+$taskProcess.ExitCode)}}finally{if(-not $taskProcess.HasExited){Stop-Process -Id $taskProcess.Id -Force}}
