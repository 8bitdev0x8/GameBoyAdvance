$ErrorActionPreference = 'Stop'
$taskRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$taskCompiler = Join-Path $env:WINDIR 'Microsoft.NET/Framework64/v4.0.30319/csc.exe'
$taskSource = Join-Path $PSScriptRoot 'RomUploader.cs'
$taskOutput = Join-Path $taskRoot 'GBA ROM Uploader.exe'
$taskVersions = Get-Content -LiteralPath (Join-Path $PSScriptRoot '../app-versions.json') -Raw | ConvertFrom-Json
if ($taskVersions.frontend -notmatch '^\d+\.\d+\.\d+$') { throw 'Invalid frontend version' }
$taskGenerated = Join-Path $taskRoot 'build/rom-uploader'
New-Item -ItemType Directory -Path $taskGenerated -Force | Out-Null
$taskAssembly = Join-Path $taskGenerated 'AssemblyInfo.cs'
@"
using System.Reflection;
[assembly: AssemblyTitle("GBA ROM Uploader")]
[assembly: AssemblyProduct("GBA ROM Uploader")]
[assembly: AssemblyVersion("$($taskVersions.frontend).0")]
[assembly: AssemblyFileVersion("$($taskVersions.frontend).0")]
[assembly: AssemblyInformationalVersion("$($taskVersions.frontend)")]
"@ | Set-Content -LiteralPath $taskAssembly -Encoding UTF8
& $taskCompiler /nologo /target:winexe /optimize+ "/out:$taskOutput" /reference:System.Windows.Forms.dll /reference:System.Drawing.dll /reference:System.Web.Extensions.dll $taskSource $taskAssembly
if ($LASTEXITCODE -ne 0) { throw 'Desktop application compilation failed' }
Write-Host "Application ready: $taskOutput"
