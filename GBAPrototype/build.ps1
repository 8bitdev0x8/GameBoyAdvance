param([switch]$Clean, [string]$BiosPath, [switch]$Profile, [switch]$DebugUsb,
    [switch]$UsePreparedBios, [string]$BuildDirectory, [string]$OutputDirectory,
    [string]$CliPath, [string]$ConfigPath, [string]$PythonPath = 'python',
    [switch]$FrameBenchmark, [int]$BenchmarkWarmupFrames=5000,
    [int]$BenchmarkFrames=180, [ValidateRange(0,3)][int]$BenchmarkPolicy=3,
    [switch]$HstxDmaRing, [string]$LibraryDirectory)
$ErrorActionPreference = 'Stop'
$taskRoot = $PSScriptRoot
$taskSketch = Join-Path $taskRoot 'GBAFeather'
if (-not (Test-Path -LiteralPath (Join-Path $taskSketch 'src/core/cpu.cpp'))) { throw 'The emulator core is intentionally excluded from this public repository. See THIRD_PARTY_NOTICES.md.' }
$taskCli = 'C:/Program Files/Arduino IDE/resources/app/lib/backend/resources/arduino-cli.exe'
$taskConfig = Join-Path $env:USERPROFILE '.arduinoIDE/arduino-cli.yaml'
$taskBuild = Join-Path $taskRoot '../build/gba'
if ($BuildDirectory) { $taskBuild = [IO.Path]::GetFullPath($BuildDirectory) }
if ($CliPath) { $taskCli = $CliPath }
if ($ConfigPath) { $taskConfig = $ConfigPath }
$taskInclude = (Join-Path $taskSketch 'src').Replace('\','/')
# Local RP2350/8 MiB layout: reserve the top 16 KiB of main SRAM for core 0.
# Generate it at prelink time because Arduino removes generated files at startup.
$taskLinker = (Join-Path $taskRoot 'linker/feather-8mb-stack16k.ld').Replace('\','/')
$taskLinkerHook = 'recipe.hooks.linking.prelink.1.pattern="{runtime.tools.pqt-python3.path}/python3" -I "{runtime.platform.path}/tools/simplesub.py" --input "' + $taskLinker + '" --out "{build.path}/memmap_default.ld" --sub __GBA_STACK_LAYOUT__ __GBA_STACK_LAYOUT__'
# ROM asset was generated with an absolute .incbin path; regenerate if moved.
$taskCompileOptions = @()
if ($Clean) { $taskCompileOptions += '--clean' }
$taskBiosFlags = ''
if ($LibraryDirectory) {
    $taskCompileOptions += @('--library',[IO.Path]::GetFullPath($LibraryDirectory))
}
if ($HstxDmaRing) {
    $taskRingHeader = $LibraryDirectory -and (Test-Path -LiteralPath (Join-Path $LibraryDirectory 'src/drivers/dvhstx/HstxDmaRing.h'))
    $taskCompactHeader = $LibraryDirectory -and (Test-Path -LiteralPath (Join-Path $LibraryDirectory 'src/drivers/dvhstx/HstxCompactIrq.h'))
    if (!$taskRingHeader -and !$taskCompactHeader) {
        throw 'HSTX DMA-ring trial requires the isolated experimental library directory'
    }
    $taskBiosFlags += ' -DDVHSTX_DMA_RING=1'
}
if ($FrameBenchmark) {
    if (!$DebugUsb) {throw 'Frame benchmark requires -DebugUsb'}
    if ($BenchmarkWarmupFrames -lt 0 -or $BenchmarkWarmupFrames -gt 100000 -or $BenchmarkFrames -lt 1 -or $BenchmarkFrames -gt 180) {
        throw 'Frame benchmark requires 0-100000 warmup frames and 1-180 measured frames'
    }
    $taskBiosFlags += " -DGBA_FRAME_BENCHMARK -DGBA_BENCH_WARMUP_FRAMES=$BenchmarkWarmupFrames -DGBA_BENCH_FRAMES=$BenchmarkFrames -DGBA_BENCH_POLICY=$BenchmarkPolicy"
}
$taskUsbStack = 'nousb'
if ($DebugUsb) {
    $taskUsbStack = 'picosdk'
    $taskBiosFlags += ' -DGBA_DEBUG_USB'
}
if ($BiosPath) {
    & $PythonPath (Join-Path $taskRoot 'tools/prepare_bios.py') $BiosPath
    if ($LASTEXITCODE -ne 0) { throw 'Private BIOS preparation failed' }
    $taskBiosFlags += ' -DGBA_USE_LOCAL_BIOS'
}
elseif ($UsePreparedBios) {
    if (!(Test-Path -LiteralPath (Join-Path $taskSketch 'src/generated/LocalBios.h'))) {
        throw 'Prepared BIOS header is missing'
    }
    $taskBiosFlags += ' -DGBA_USE_LOCAL_BIOS'
}
$taskProfileTool = Join-Path $taskRoot 'tools/prepare_pc_profile.py'
if ($Profile) {
    $taskBiosFlags += ' -DGBA_ENABLE_PC_PROFILE'
    & $PythonPath $taskProfileTool --empty
    if ($LASTEXITCODE -ne 0) { throw 'PC profile table preparation failed' }
}
function Invoke-GbaCompile {
    & $taskCli --config-file $taskConfig compile @taskCompileOptions --fqbn "rp2040:rp2040:adafruit_feather_rp2350_hstx:psram=8mb,usbstack=$taskUsbStack" --build-path $taskBuild --build-property $taskLinkerHook --build-property 'build.flags.optimize=-O3' --build-property 'compiler.c.elf.extra_flags=-Wl,--wrap=abort' --build-property "compiler.cpp.extra_flags=-std=gnu++20 -fstack-usage -DGBA_PLATFORM_RP2350 -DGBA_ENABLE_HLE_BIOS$taskBiosFlags -I$taskInclude" $taskSketch
    if ($LASTEXITCODE -ne 0) { throw 'GBA prototype compilation failed' }
}
Invoke-GbaCompile
if ($Profile) {
    $taskNm = Join-Path $env:LOCALAPPDATA 'Arduino15/packages/rp2040/tools/pqt-gcc/4.1.0-1aec55e/bin/arm-none-eabi-nm.exe'
    $taskElf = Join-Path $taskBuild 'GBAFeather.ino.elf'
    & $PythonPath $taskProfileTool --elf $taskElf --nm $taskNm
    if ($LASTEXITCODE -ne 0) { throw 'PC symbol map generation failed' }
    $taskCompileOptions = @()
    if ($LibraryDirectory) { $taskCompileOptions += @('--library',[IO.Path]::GetFullPath($LibraryDirectory)) }
    Invoke-GbaCompile
    & $PythonPath $taskProfileTool --elf $taskElf --nm $taskNm --check
    if ($LASTEXITCODE -ne 0) { throw 'Final ELF does not match PC map; firmware not delivered' }
}
$taskOutput = Join-Path $taskRoot 'output'
if ($OutputDirectory) { $taskOutput = [IO.Path]::GetFullPath($OutputDirectory) }
New-Item -ItemType Directory -Path $taskOutput -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $taskBuild 'GBAFeather.ino.uf2') -Destination (Join-Path $taskOutput 'GBAFeather.uf2')
Write-Host ('UF2 ready: ' + (Join-Path $taskOutput 'GBAFeather.uf2'))
