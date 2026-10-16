$ErrorActionPreference='Stop'
$taskRepo=(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$taskOutput=Join-Path $env:TEMP ('mushclient-options-ux-'+[guid]::NewGuid().ToString('N'));New-Item -ItemType Directory -Path $taskOutput | Out-Null
$taskSource=Get-Content -LiteralPath (Join-Path $taskRepo 'WorkspaceRouteManager.cpp') -Raw
$taskStart=$taskSource.IndexOf('struct SpawnOptions {');$taskEnd=$taskSource.IndexOf('void ShowSpawnOptions(',$taskStart)
Set-Content -LiteralPath (Join-Path $taskOutput 'options-ux-production.inc') -Value $taskSource.Substring($taskStart,$taskEnd-$taskStart) -Encoding utf8
$taskRsp=@"
/nologo /MT /EHsc /std:c++14 /D_WIN32_WINNT=0x0602 /DNO_HTMLHELP /DNDEBUG /DWIN32 /D_WINDOWS /DZLIB_DLL /DLUA51 /D_CRT_SECURE_NO_WARNINGS /D_CRT_NON_CONFORMING_SWPRINTFS /DNO_WARN_MBCS_MFC_DEPRECATION
/I"$taskRepo" /I"$taskRepo\luacom" /I"$taskRepo\openssl\include" /I"$taskOutput"
/Fe"$taskOutput\options-ux.exe" /Fo"$taskOutput\\"
"$taskRepo\tests\workspace\options_ux_probe.cpp"
/link /SUBSYSTEM:WINDOWS Shlwapi.lib
"@
Set-Content -LiteralPath (Join-Path $taskOutput 'build.rsp') -Value $taskRsp -Encoding ascii
Set-Content -LiteralPath (Join-Path $taskOutput 'build.cmd') -Encoding ascii -Value @('@echo off','call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars32.bat" >nul','set "LIB=%VCToolsInstallDir%atlmfc\lib\spectre\x86;%LIB%"',('cd /d "'+$taskOutput+'"'),('cl @"'+$taskOutput+'\build.rsp"'))
& cmd.exe /d /c (Join-Path $taskOutput 'build.cmd');if($LASTEXITCODE -ne 0){throw 'Options UX fixture compilation failed.'}
$taskProcess=Start-Process -FilePath (Join-Path $taskOutput 'options-ux.exe') -WorkingDirectory $taskOutput -WindowStyle Hidden -PassThru
if(!$taskProcess.WaitForExit(20000)){Stop-Process -Id $taskProcess.Id;throw 'Owned hidden options fixture timed out.'}
Get-Content -LiteralPath (Join-Path $taskOutput 'options-ux-results.txt');Write-Output ('Evidence: '+$taskOutput)
if($taskProcess.ExitCode -ne 0){throw ('Hidden options probe failed: '+$taskProcess.ExitCode)}
