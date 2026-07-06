[CmdletBinding()]
param(
    [string]$Preset = "windows-msvc-debug",
    [switch]$SkipBuild,
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$TestArguments = @()
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path

function Invoke-PonderCommand
{
    param(
        [Parameter(Mandatory = $true)]
        [string]$Executable,
        [string[]]$Arguments = @()
    )

    Write-Host ("+ {0} {1}" -f $Executable, ($Arguments -join " "))
    & $Executable @Arguments
    if ($LASTEXITCODE -ne 0)
    {
        exit $LASTEXITCODE
    }
}

Push-Location $RepoRoot
try
{
    Invoke-PonderCommand -Executable "cmake" -Arguments @("--preset", $Preset)

    if (-not $SkipBuild)
    {
        Invoke-PonderCommand -Executable "cmake" -Arguments @("--build", "--preset", $Preset)
    }

    Invoke-PonderCommand -Executable "ctest" -Arguments (@("--preset", $Preset) + $TestArguments)
}
finally
{
    Pop-Location
}
