param([switch]$Clean)
$ErrorActionPreference = 'Stop'
$taskRoot = $PSScriptRoot
$taskSketch = Join-Path $taskRoot 'GBAFeather'
if (-not (Test-Path -LiteralPath (Join-Path $taskSketch 'src/core/cpu.cpp'))) {
    throw 'The emulator core is intentionally excluded from this public repository: upstream grants no redistribution licence. See docs/building.md and THIRD_PARTY_NOTICES.md. The standalone hardware sketches can be built independently.'
}
$taskCli = 'C:/Program Files/Arduino IDE/resources/app/lib/backend/resources/arduino-cli.exe'
$taskConfig = Join-Path $env:USERPROFILE '.arduinoIDE/arduino-cli.yaml'
$taskBuild = Join-Path $taskRoot '../build/gba'
$taskInclude = (Join-Path $taskSketch 'src').Replace('\','/')
# Local RP2350/8 MiB layout: reserve the top 16 KiB of main SRAM for core 0.
# Generate it at prelink time because Arduino removes generated files at startup.
$taskLinker = (Join-Path $taskRoot 'linker/feather-8mb-stack16k.ld').Replace('\','/')
$taskLinkerHook = 'recipe.hooks.linking.prelink.1.pattern="{runtime.tools.pqt-python3.path}/python3" -I "{runtime.platform.path}/tools/simplesub.py" --input "' + $taskLinker + '" --out "{build.path}/memmap_default.ld" --sub __GBA_STACK_LAYOUT__ __GBA_STACK_LAYOUT__'
# ROM asset was generated with an absolute .incbin path; regenerate if moved.
$taskCompileOptions = @()
if ($Clean) { $taskCompileOptions += '--clean' }
& $taskCli --config-file $taskConfig compile @taskCompileOptions --fqbn 'rp2040:rp2040:adafruit_feather_rp2350_hstx:psram=8mb,usbstack=nousb' --build-path $taskBuild --build-property $taskLinkerHook --build-property 'build.flags.optimize=-O3' --build-property "compiler.cpp.extra_flags=-std=gnu++20 -fstack-usage -DGBA_PLATFORM_RP2350 -DGBA_ENABLE_HLE_BIOS -I$taskInclude" $taskSketch
if ($LASTEXITCODE -ne 0) { throw 'GBA prototype compilation failed' }
$taskOutput = Join-Path $taskRoot 'output'
New-Item -ItemType Directory -Path $taskOutput -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $taskBuild 'GBAFeather.ino.uf2') -Destination (Join-Path $taskOutput 'GBAFeather.uf2')
Write-Host ('UF2 ready: ' + (Join-Path $taskOutput 'GBAFeather.uf2'))
