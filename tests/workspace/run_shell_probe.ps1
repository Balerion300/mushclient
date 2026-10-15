param([string]$RepoRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path,[int]$TimeoutSeconds=60)
$ErrorActionPreference='Stop'
$taskVswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$taskVsRoot=& $taskVswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(-not $taskVsRoot){throw 'Visual C++ tools are required.'}
$taskCompilerRoot=(Get-ChildItem (Join-Path $taskVsRoot 'VC\Tools\MSVC') -Directory | Sort-Object {[version]$_.Name} -Descending | Select-Object -First 1).FullName
$taskMfcLib=Join-Path $taskCompilerRoot 'atlmfc\lib\spectre\x86'
if(-not(Test-Path -LiteralPath $taskMfcLib)){$taskMfcLib=Join-Path $taskCompilerRoot 'atlmfc\lib\x86'}
$taskTemporary=Join-Path ([IO.Path]::GetTempPath()) ('mushclient-shell-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $taskTemporary | Out-Null
$taskResponse=@('/nologo /O2 /MT /EHsc /std:c++14 /D_WIN32_WINNT=0x0601 /DNO_HTMLHELP /DNDEBUG /DWIN32 /D_WINDOWS /DZLIB_DLL /DLUA51 /D_CRT_SECURE_NO_WARNINGS /D_CRT_NON_CONFORMING_SWPRINTFS /DNO_WARN_MBCS_MFC_DEPRECATION',('/I"'+$RepoRoot+'" /I"'+$RepoRoot+'\luacom" /I"'+$RepoRoot+'\openssl\include"'),('/Fe"'+$taskTemporary+'\shell-probe.exe" /Fo"'+$taskTemporary+'\\"'),('"'+$RepoRoot+'\tests\workspace\shell_probe.cpp"'),('"'+$RepoRoot+'\workspace\WorkspaceShell.cpp"'),('"'+$RepoRoot+'\WorkspaceStyle.cpp"'),'/link /SUBSYSTEM:WINDOWS Shlwapi.lib Dwmapi.lib UxTheme.lib')
$taskRsp=Join-Path $taskTemporary 'build.rsp';Set-Content -LiteralPath $taskRsp -Encoding ASCII -Value $taskResponse
$taskCmd=Join-Path $taskTemporary 'build.cmd';Set-Content -LiteralPath $taskCmd -Encoding ASCII -Value @('@echo off',('call "'+$taskVsRoot+'\VC\Auxiliary\Build\vcvars32.bat" >nul'),('set LIB='+$taskMfcLib+';%LIB%'),('cd /d "'+$taskTemporary+'"'),('cl @"'+$taskRsp+'"'))
& cmd.exe /d /c $taskCmd
if($LASTEXITCODE -ne 0){throw ('Shell fixture compilation failed; '+$taskTemporary)}
Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $taskTemporary 'shell-probe.exe'),(Join-Path $RepoRoot 'workspace\WorkspaceShell.cpp'),(Join-Path $RepoRoot 'WorkspaceStyle.cpp') | Format-List | Out-File -LiteralPath (Join-Path $taskTemporary 'identity.txt')
$taskProcess=Start-Process -FilePath (Join-Path $taskTemporary 'shell-probe.exe') -WorkingDirectory $taskTemporary -WindowStyle Hidden -PassThru
try {if(-not $taskProcess.WaitForExit($TimeoutSeconds*1000)){throw ('Shell fixture timed out; '+$taskTemporary)};Get-Content -LiteralPath (Join-Path $taskTemporary 'shell-results.txt');Write-Output ('Evidence: '+$taskTemporary);if($taskProcess.ExitCode -ne 0){throw ('Shell fixture failed: '+$taskProcess.ExitCode)}}finally{if(-not $taskProcess.HasExited){Stop-Process -Id $taskProcess.Id -Force}}
