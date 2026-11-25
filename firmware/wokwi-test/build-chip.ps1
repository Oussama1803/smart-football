$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$clang = 'C:\msys64\ucrt64\bin\clang.exe'
if (-not (Test-Path -LiteralPath $clang)) {
  throw "clang not found at $clang; install/use the repository's documented WASM toolchain."
}
$out = Join-Path $root 'chips\shot-sequencer.chip.wasm'
$clangArgs = @(
  '--target=wasm32-unknown-unknown', '-ffreestanding', '-nostdlib',
  '-Wl,--import-memory', '-Wl,--export-table', '-Wl,--no-entry', '-Werror', '-O2',
  '-I', (Join-Path $root '..\..\chips'),
  '-o', $out, (Join-Path $root 'chips\shot-sequencer.c')
)
& $clang @clangArgs
if ($LASTEXITCODE -ne 0) { throw "WASM build failed with exit code $LASTEXITCODE" }
Write-Output "Built test-only chip: $out"
