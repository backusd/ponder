[CmdletBinding()]
param(
    [string]$Preset = "windows-msvc-debug",
    [switch]$Check,
    [switch]$Tidy
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$SourceRoots = @("apps", "libs", "plugins", "tests", "tools")
$SourceExtensions = @(".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx")
$CompileExtensions = @(".cc", ".cpp", ".cxx")

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

function Require-Command
{
    param([Parameter(Mandatory = $true)][string]$Name)

    if (-not (Get-Command $Name -ErrorAction SilentlyContinue))
    {
        throw "Required command '$Name' was not found on PATH."
    }
}

Push-Location $RepoRoot
try
{
    Require-Command "clang-format"

    $Files = foreach ($Root in $SourceRoots)
    {
        if (Test-Path $Root)
        {
            Get-ChildItem -Path $Root -Recurse -File |
                Where-Object { $SourceExtensions -contains $_.Extension }
        }
    }

    $FilePaths = @($Files | Sort-Object FullName | ForEach-Object { $_.FullName })
    if ($FilePaths.Count -eq 0)
    {
        Write-Host "No source files found."
        exit 0
    }

    foreach ($FilePath in $FilePaths)
    {
        if ($Check)
        {
            Invoke-PonderCommand -Executable "clang-format" `
                -Arguments @("--dry-run", "--Werror", $FilePath)
        }
        else
        {
            Invoke-PonderCommand -Executable "clang-format" -Arguments @("-i", $FilePath)
        }
    }

    if ($Tidy)
    {
        Require-Command "clang-tidy"

        $BuildDir = Join-Path $RepoRoot ("build/{0}" -f $Preset)
        $CompileDatabase = Join-Path $BuildDir "compile_commands.json"
        if (-not (Test-Path $CompileDatabase))
        {
            throw "clang-tidy requires '$CompileDatabase'. Configure a preset that exports it."
        }

        $CompileFiles = @(
            $Files |
                Where-Object { $CompileExtensions -contains $_.Extension } |
                Sort-Object FullName |
                ForEach-Object { $_.FullName }
        )

        foreach ($FilePath in $CompileFiles)
        {
            Invoke-PonderCommand -Executable "clang-tidy" `
                -Arguments @($FilePath, "-p", $BuildDir)
        }
    }
}
finally
{
    Pop-Location
}
