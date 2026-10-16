param([string]$RepoRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path,[ValidateSet('Release','Debug')][string]$Configuration='Release')
$ErrorActionPreference='Stop'
$taskVswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$taskVsRoot=& $taskVswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(-not $taskVsRoot){throw 'Visual C++ tools are required.'}
$taskTemporary=Join-Path ([IO.Path]::GetTempPath()) ('mushclient-spell-probe-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $taskTemporary | Out-Null
$taskResponse=@('/nologo /O2 /MT /EHsc /std:c++17 /D_WIN32_WINNT=0x0602 /DUNICODE /D_UNICODE',('/I"'+$RepoRoot+'"'),('/Fe"'+$taskTemporary+'\spell-probe.exe" /Fo"'+$taskTemporary+'\\"'),('"'+$RepoRoot+'\tests\workspace\spell_probe.cpp"'),'/link /SUBSYSTEM:CONSOLE Ole32.lib Uuid.lib User32.lib Gdi32.lib Comctl32.lib')
if($Configuration -eq 'Debug'){$taskResponse[0]=$taskResponse[0].Replace('/O2 /MT','/Od /MTd /D_DEBUG')}
$taskRsp=Join-Path $taskTemporary 'build.rsp';Set-Content -LiteralPath $taskRsp -Encoding ASCII -Value $taskResponse
$taskCmd=Join-Path $taskTemporary 'build.cmd';Set-Content -LiteralPath $taskCmd -Encoding ASCII -Value @('@echo off',('call "'+$taskVsRoot+'\VC\Auxiliary\Build\vcvars32.bat" >nul'),('cd /d "'+$taskTemporary+'"'),('cl @"'+$taskRsp+'"'))
& cmd.exe /d /c $taskCmd
if($LASTEXITCODE -ne 0){throw ('Spell fixture compilation failed; '+$taskTemporary)}
Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $taskTemporary 'spell-probe.exe'),(Join-Path $RepoRoot 'workspace\WorkspaceSpell.h') | Format-List | Out-File -LiteralPath (Join-Path $taskTemporary 'identity.txt') -Encoding utf8
Write-Output ('Built only: '+$taskTemporary)
