[CmdletBinding()]
param(
    [switch]$Strict
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Get-PathCommand
{
    param([Parameter(Mandatory = $true)][string]$Name)

    $command = Get-Command $Name -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($null -eq $command)
    {
        return $null
    }

    if ($command.Source)
    {
        return $command.Source
    }

    return $command.Path
}

function Get-VsWherePath
{
    $vsWhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vsWhere)
    {
        return $vsWhere
    }

    return $null
}

function Get-VisualStudioInstallations
{
    $vsWhere = Get-VsWherePath
    if (-not $vsWhere)
    {
        return @()
    }

    $json = & $vsWhere -products * -format json
    if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($json))
    {
        return @()
    }

    return @($json | ConvertFrom-Json | Sort-Object installationVersion -Descending)
}

function Find-VisualStudioLlvmTool
{
    param([Parameter(Mandatory = $true)][string]$Name)

    foreach ($installation in Get-VisualStudioInstallations)
    {
        $candidates = @(
            (Join-Path $installation.installationPath "VC\Tools\Llvm\x64\bin\$Name.exe"),
            (Join-Path $installation.installationPath "VC\Tools\Llvm\bin\$Name.exe")
        )

        foreach ($candidate in $candidates)
        {
            if (Test-Path $candidate)
            {
                return $candidate
            }
        }
    }

    return $null
}

function Find-Tool
{
    param(
        [Parameter(Mandatory = $true)]
        [string]$Name,
        [switch]$AllowVisualStudioLlvm
    )

    $path = Get-PathCommand $Name
    if ($path)
    {
        return [pscustomobject]@{
            Name = $Name
            Path = $path
            Source = "PATH"
        }
    }

    if ($AllowVisualStudioLlvm)
    {
        $vsPath = Find-VisualStudioLlvmTool $Name
        if ($vsPath)
        {
            return [pscustomobject]@{
                Name = $Name
                Path = $vsPath
                Source = "Visual Studio LLVM"
            }
        }
    }

    return [pscustomobject]@{
        Name = $Name
        Path = $null
        Source = "missing"
    }
}

function Test-ToolVersion
{
    param(
        [Parameter(Mandatory = $true)]
        [pscustomobject]$Tool,
        [string[]]$Arguments = @("--version")
    )

    if (-not $Tool.Path)
    {
        return [pscustomobject]@{
            Works = $false
            Summary = "not found"
        }
    }

    try
    {
        $output = & $Tool.Path @Arguments 2>&1
        $exitCode = $LASTEXITCODE
        if ($exitCode -ne 0)
        {
            return [pscustomobject]@{
                Works = $false
                Summary = "exited with $exitCode"
            }
        }

        $firstLine = @($output | Where-Object { -not [string]::IsNullOrWhiteSpace($_) } |
            Select-Object -First 1)
        return [pscustomobject]@{
            Works = $true
            Summary = if ($firstLine.Count -gt 0) { [string]$firstLine[0] } else { "ok" }
        }
    }
    catch
    {
        return [pscustomobject]@{
            Works = $false
            Summary = $_.Exception.Message
        }
    }
}

function Write-ToolResult
{
    param(
        [Parameter(Mandatory = $true)]
        [string]$Label,
        [Parameter(Mandatory = $true)]
        [pscustomobject]$Tool,
        [Parameter(Mandatory = $true)]
        [pscustomobject]$Version,
        [Parameter(Mandatory = $true)]
        [string]$Level
    )

    $status = if ($Version.Works) { "OK" } else { "MISSING" }
    if ($Tool.Path -and -not $Version.Works)
    {
        $status = "BROKEN"
    }

    Write-Host ("[{0}] {1} ({2})" -f $status, $Label, $Level)
    Write-Host ("      path: {0}" -f ($(if ($Tool.Path) { $Tool.Path } else { "<not found>" })))
    Write-Host ("      source: {0}" -f $Tool.Source)
    Write-Host ("      check: {0}" -f $Version.Summary)
}

$requiredFailures = 0
$recommendedFailures = 0

$checks = @(
    @{ Label = "Git"; Name = "git"; Required = $true; VsLlvm = $false },
    @{ Label = "CMake"; Name = "cmake"; Required = $true; VsLlvm = $false },
    @{ Label = "CTest"; Name = "ctest"; Required = $true; VsLlvm = $false },
    @{ Label = "MSBuild"; Name = "msbuild"; Required = $true; VsLlvm = $false },
    @{ Label = "Ninja"; Name = "ninja"; Required = $false; VsLlvm = $false },
    @{ Label = "clang-format"; Name = "clang-format"; Required = $false; VsLlvm = $true },
    @{ Label = "clang-tidy"; Name = "clang-tidy"; Required = $false; VsLlvm = $true },
    @{ Label = "clang++"; Name = "clang++"; Required = $false; VsLlvm = $true },
    @{ Label = "clang-cl"; Name = "clang-cl"; Required = $false; VsLlvm = $true },
    @{ Label = "MSVC cl"; Name = "cl"; Required = $false; VsLlvm = $false },
    @{ Label = "MSVC link"; Name = "link"; Required = $false; VsLlvm = $false },
    @{ Label = "Bash"; Name = "bash"; Required = $false; VsLlvm = $false }
)

foreach ($check in $checks)
{
    $tool = Find-Tool -Name $check.Name -AllowVisualStudioLlvm:([bool]$check.VsLlvm)
    $version = Test-ToolVersion -Tool $tool
    $level = if ($check.Required) { "required" } else { "recommended" }

    Write-ToolResult -Label $check.Label -Tool $tool -Version $version -Level $level

    if (-not $version.Works)
    {
        if ($check.Required)
        {
            $requiredFailures++
        }
        else
        {
            $recommendedFailures++
        }
    }
}

Write-Host ""
Write-Host "Notes:"
Write-Host "- The default Windows MSVC presets require Visual Studio/MSBuild."
Write-Host "- The windows-ninja-analysis preset should be run from a Visual Studio Developer shell."
Write-Host "- clang-format and clang-tidy should be on PATH for scripts/format.ps1."
Write-Host "- Tools found through Visual Studio LLVM are usable by full path but may not be on PATH."

if ($requiredFailures -gt 0)
{
    throw "$requiredFailures required tool check(s) failed."
}

if ($Strict -and $recommendedFailures -gt 0)
{
    throw "$recommendedFailures recommended tool check(s) failed."
}

if ($recommendedFailures -gt 0)
{
    Write-Host ""
    Write-Host "$recommendedFailures recommended tool check(s) failed."
}
