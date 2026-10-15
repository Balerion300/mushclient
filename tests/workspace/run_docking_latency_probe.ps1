param([string]$RepoRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path,[string]$Label='current',[string]$SourceDirectory='',[int]$TimeoutSeconds=240)
$ErrorActionPreference='Stop'
if($Label -notmatch '^[a-zA-Z0-9_-]+$'){throw 'Label must contain only letters, digits, underscore, or hyphen.'}
$taskVswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$taskVsRoot=& $taskVswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(-not $taskVsRoot){throw 'Visual C++ tools are required.'}
$taskCompilerRoot=(Get-ChildItem (Join-Path $taskVsRoot 'VC\Tools\MSVC') -Directory|Sort-Object {[version]$_.Name} -Descending|Select-Object -First 1).FullName
$taskSdkBase=Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10'
$taskSdkVersion=(Get-ChildItem (Join-Path $taskSdkBase 'Include') -Directory|Where-Object {Test-Path (Join-Path $_.FullName 'ucrt\new.h')}|Sort-Object {[version]$_.Name} -Descending|Select-Object -First 1).Name
$taskOldInclude=$env:INCLUDE;$taskOldLib=$env:LIB;$taskOldOutput=$env:MUSH_DOCK_LATENCY_OUTPUT;$taskLocation=Get-Location
try {
 Set-Location -LiteralPath $RepoRoot
 $env:INCLUDE="${taskCompilerRoot}\include;${taskCompilerRoot}\atlmfc\include;${taskSdkBase}\Include\${taskSdkVersion}\ucrt;${taskSdkBase}\Include\${taskSdkVersion}\shared;${taskSdkBase}\Include\${taskSdkVersion}\um"
 $taskMfcLib=Join-Path $taskCompilerRoot 'atlmfc\lib\spectre\x86';if(-not(Test-Path $taskMfcLib)){$taskMfcLib=Join-Path $taskCompilerRoot 'atlmfc\lib\x86'}
 $env:LIB="${taskCompilerRoot}\lib\x86;${taskMfcLib};${taskSdkBase}\Lib\${taskSdkVersion}\ucrt\x86;${taskSdkBase}\Lib\${taskSdkVersion}\um\x86"
 $taskDirectory="WinRel\docking-latency-$Label-objects";New-Item -ItemType Directory -Force $taskDirectory|Out-Null
 $taskPcreObjects=Get-ChildItem WinRel -Filter 'pcre_*.obj';if($taskPcreObjects.Count -lt 10){throw 'Build the Release client first; this isolated probe reuses its PCRE objects.'}
 $taskPane='WorkspacePane.cpp';$taskInclude='/I.'
 if($SourceDirectory){$taskPane=Join-Path $SourceDirectory 'WorkspacePane.cpp';$taskInclude='/I"'+$SourceDirectory+'" /I.'}
 $taskResponse=@"
/nologo /MT /EHsc /D_WIN32_WINNT=0x0601 /DNO_HTMLHELP /DNDEBUG /DWIN32 /D_WINDOWS /DZLIB_DLL /DLUA51 /D_CRT_SECURE_NO_WARNINGS /D_CRT_NON_CONFORMING_SWPRINTFS /DNO_WARN_MBCS_MFC_DEPRECATION $taskInclude /Iluacom /Iopenssl\include
/FeWinRel\docking-latency-$Label.exe /Fo$taskDirectory\
tests\workspace\docking_latency_probe.cpp "$taskPane" WorkspaceStyle.cpp InlineImage.cpp workspace\WorkspaceInputPolicy.cpp
/link /SUBSYSTEM:WINDOWS /LTCG Shlwapi.lib
"@
 $taskResponse+="`n"+(($taskPcreObjects|ForEach-Object {'"'+$_.FullName+'"'}) -join ' ')
 $taskResponsePath=Join-Path $taskDirectory build.rsp;Set-Content -LiteralPath $taskResponsePath -Value $taskResponse
 & (Join-Path $taskCompilerRoot 'bin\Hostx64\x86\cl.exe') "@$taskResponsePath"
 if($LASTEXITCODE -ne 0){throw 'Docking latency probe compilation failed.'}
 $env:MUSH_DOCK_LATENCY_OUTPUT="WinRel/docking-latency-$Label"
 $taskProcess=Start-Process -FilePath (Join-Path $RepoRoot "WinRel\docking-latency-$Label.exe") -WorkingDirectory $RepoRoot -WindowStyle Hidden -PassThru
 if(-not $taskProcess.WaitForExit($TimeoutSeconds*1000)){Stop-Process -Id $taskProcess.Id;throw 'Docking latency probe timed out.'}
 Get-Content -LiteralPath "WinRel\docking-latency-$Label.txt"
 if($taskProcess.ExitCode -ne 0){throw "Docking latency probe failed: exit $($taskProcess.ExitCode)."}
} finally {$env:INCLUDE=$taskOldInclude;$env:LIB=$taskOldLib;$env:MUSH_DOCK_LATENCY_OUTPUT=$taskOldOutput;Set-Location -LiteralPath $taskLocation}
