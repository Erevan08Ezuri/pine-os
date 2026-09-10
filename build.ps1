param([ValidateSet('desktop')][string]$Target = 'desktop')
$ErrorActionPreference = 'Stop'
$cmakeCommand = Get-Command cmake -ErrorAction SilentlyContinue
$cmake = if ($cmakeCommand) { $cmakeCommand.Source } else { $null }
if (-not $cmake) {
  $vswhere = 'C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe'
  if (Test-Path -LiteralPath $vswhere) {
    $install = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if ($install) {
      $bundled = Join-Path $install 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
      if (Test-Path -LiteralPath $bundled) { $cmake = $bundled }
    }
  }
}
if (-not $cmake) { throw 'CMake 3.24+ and a C++20 compiler are required. Install Visual Studio Build Tools with Desktop development with C++.' }
& $cmake -S $PSScriptRoot -B (Join-Path $PSScriptRoot 'build\desktop') -A x64 -DBUILD_TESTING=ON
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $cmake --build (Join-Path $PSScriptRoot 'build\desktop') --config Release --parallel
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
Write-Host '[PINE][BUILD] Desktop build complete.'
