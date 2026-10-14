param([string]$VcVars = 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars32.bat')
$ErrorActionPreference = 'Stop'
$sourceRoot = Join-Path $PSScriptRoot '..\third_party\scintilla\win32'
if (!(Test-Path -LiteralPath $VcVars)) { throw "Missing VS2022 x86 environment: $VcVars" }
Push-Location $sourceRoot
try {
  & cmd.exe /d /c "call `"$VcVars`" >nul && nmake /f scintilla.mak QUIET=1"
  if ($LASTEXITCODE -ne 0) { throw 'Scintilla source build failed' }
} finally { Pop-Location }
Get-FileHash (Join-Path $sourceRoot '..\bin\Scintilla.dll') -Algorithm SHA256
