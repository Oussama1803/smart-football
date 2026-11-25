$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$sourceRoot = Join-Path $root '..\..\chips'
$clang = 'C:\msys64\ucrt64\bin\clang.exe'
if (-not (Test-Path -LiteralPath $clang)) {
  throw "clang not found at $clang; install/use the repository's documented WASM toolchain."
}

function Build-WasmChip([string]$source, [string]$output) {
  $compilerArgs = @(
    '--target=wasm32-unknown-unknown', '-ffreestanding', '-nostdlib',
    '-Wl,--import-memory', '-Wl,--export-table', '-Wl,--no-entry', '-Werror', '-O2',
    '-I', $sourceRoot, '-o', $output, $source
  )
  & $clang @compilerArgs
  if ($LASTEXITCODE -ne 0) { throw "WASM build failed for $source with exit code $LASTEXITCODE" }
  Write-Output "Built $output"
}

$outDir = Join-Path $root 'chips'
$fsrSource = Join-Path $sourceRoot 'fsr-sensor\main.c'
$sequencerSource = Join-Path $root 'chips\shot-sequencer.c'
Build-WasmChip $fsrSource (Join-Path $outDir 'fsr-sensor.chip.wasm')
Build-WasmChip $sequencerSource (Join-Path $outDir 'shot-sequencer.chip.wasm')
Copy-Item -LiteralPath (Join-Path $sourceRoot 'fsr-sensor\fsr-sensor.chip.json') `
  -Destination (Join-Path $outDir 'fsr-sensor.chip.json') -Force
