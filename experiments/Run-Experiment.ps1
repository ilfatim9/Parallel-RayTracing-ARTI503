[CmdletBinding()]
param(
    [string]$Compiler = 'C:\msys64\ucrt64\bin\g++.exe',
    [ValidateRange(1, 100)][int]$Runs = 5,
    [ValidateSet('Sequential','Race','Critical','Atomic','Reduction','Lock')]
    [string[]]$Variants = @('Sequential','Race','Critical','Atomic','Reduction','Lock'),
    [string]$OutputDirectory
)

$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$compilerPath = (Get-Command $Compiler -CommandType Application).Source
if (!$OutputDirectory) {
    $OutputDirectory = Join-Path $root ('runs/' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
}
$output = (New-Item -ItemType Directory -Path $OutputDirectory).FullName
$reference = 'F17522BFC38E2ED16A2352D3EC4C747AE2FE89B9E52D1385C8678E5D915858EB'
$oldPath = $env:PATH
$oldThreads = $env:OMP_NUM_THREADS
$oldDynamic = $env:OMP_DYNAMIC
$invariant = [Globalization.CultureInfo]::InvariantCulture
$rows = [Collections.Generic.List[object]]::new()
$summaries = [Collections.Generic.List[object]]::new()

try {
    $env:PATH = "$(Split-Path $compilerPath -Parent);$oldPath"
    $env:OMP_DYNAMIC = 'FALSE'
    foreach ($variant in $Variants) {
        $source = Join-Path $root "snapshots/$variant/main.cc"
        $exe = Join-Path $output "$variant.exe"
        $flags = @('-std=c++17', '-O2')
        $threadCounts = @(1)
        if ($variant -ne 'Sequential') {
            $flags += '-fopenmp'
            $threadCounts = @(2, 4, 8)
        }
        & $compilerPath @flags $source -o $exe
        if ($LASTEXITCODE -ne 0) { throw "Build failed for $variant." }

        foreach ($threads in $threadCounts) {
            $env:OMP_NUM_THREADS = [string]$threads
            $seconds = @()
            foreach ($run in 1..$Runs) {
                $stem = "${variant}_${threads}_run${run}"
                $ppm = Join-Path $output "$stem.ppm"
                $log = Join-Path $output "$stem.log"
                $process = Start-Process -FilePath $exe -WorkingDirectory $output -WindowStyle Hidden -Wait -PassThru -RedirectStandardOutput $ppm -RedirectStandardError $log
                if ($process.ExitCode -ne 0) { throw "$stem failed. See $log." }
                $diagnostics = Get-Content -LiteralPath $log -Raw
                if ($diagnostics -notmatch '(?m)^Render time: ([0-9.eE+-]+) seconds\r?$') { throw "Missing render time in $log." }
                $elapsed = [double]::Parse($Matches[1], $invariant)
                if ($diagnostics -notmatch '(?m)^Completed pixels: (\d+)\r?$') { throw "Missing pixel count in $log." }
                $completed = [int]$Matches[1]
                # Derive the archival hash with the Windows PowerShell 5.1 encoding.
                $archivalText = ([IO.File]::ReadAllText($ppm) -replace '\r?\n', "`r`n")
                $unicode = [Text.Encoding]::Unicode
                $bytes = [byte[]]($unicode.GetPreamble() + $unicode.GetBytes($archivalText))
                $hasher = [Security.Cryptography.SHA256]::Create()
                try { $archiveHash = [BitConverter]::ToString($hasher.ComputeHash($bytes)).Replace('-', '') }
                finally { $hasher.Dispose() }

                $rows.Add([pscustomobject]@{
                    variant = $variant
                    requested_threads = $threads
                    run = $run
                    render_seconds = $elapsed.ToString('G', $invariant)
                    completed_pixels = $completed
                    raw_sha256 = (Get-FileHash -LiteralPath $ppm).Hash
                    archival_sha256 = $archiveHash
                    matches_reference = $archiveHash -eq $reference
                })
                $seconds += $elapsed
                Write-Host "$stem finished in $elapsed seconds with $completed pixels."
            }
            $summaries.Add([pscustomobject]@{
                variant = $variant
                requested_threads = $threads
                runs = $Runs
                mean_seconds = ($seconds | Measure-Object -Average).Average.ToString('F6', $invariant)
            })
        }
    }
} finally {
    $env:PATH = $oldPath
    $env:OMP_NUM_THREADS = $oldThreads
    $env:OMP_DYNAMIC = $oldDynamic
    if ($rows.Count) { $rows | Export-Csv (Join-Path $output 'runs.csv') -NoTypeInformation -Encoding UTF8 }
    if ($summaries.Count) { $summaries | Export-Csv (Join-Path $output 'means.csv') -NoTypeInformation -Encoding UTF8 }
}

$failures = @($rows | Where-Object {
    $_.variant -ne 'Race' -and ($_.completed_pixels -ne 90000 -or !$_.matches_reference)
})
if ($failures.Count) { throw "$($failures.Count) corrected runs failed verification. See $output\runs.csv." }
