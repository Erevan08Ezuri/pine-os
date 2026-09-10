param([ValidateSet('desktop')][string]$Target = 'desktop')
$ErrorActionPreference = 'Stop'
$candidates = @(
  (Join-Path $PSScriptRoot 'build\desktop\bin\Release\pine.exe'),
  (Join-Path $PSScriptRoot 'build\desktop\bin\pine.exe')
)
$pine = $candidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
if (-not $pine) { throw 'Pine OS is not built. Run .\build.ps1 desktop first.' }
Set-Location $PSScriptRoot
& $pine
