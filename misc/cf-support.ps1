<#
.SYNOPSIS
  Create an archive of information to submit for CFEngine support tickets.

.DESCRIPTION
  Windows counterpart to the POSIX 'cf-support' script. Gathers system details,
  CFEngine configuration, service state, Windows Event Log entries logged by the
  agent, and the contents of the diagnostics directory (which includes
  report_dumps when enable_report_dumps is in place), then writes a zip archive
  in the current directory.

  Output layout deliberately matches the POSIX script: a single system-info.txt
  with '** <command>' section headers, plus individual collected files, all
  under a top-level cfengine_support_case_<case>-<host>-<timestamp> directory.

.PARAMETER Yes
  Non-interactive. Assume no ticket number and include masterfiles.

.EXAMPLE
  cf-support.ps1
.EXAMPLE
  cf-support.ps1 -Yes
#>
[CmdletBinding()]
param(
    [Alias('y')]
    [switch]$Yes,
    [string]$WorkDir
)

$ErrorActionPreference = 'Continue'

# --- Resolve WORKDIR/BINDIR ------------------------------------------------
# Installed into <WORKDIR>\bin, so derive from the script's own location first;
# that is correct for a non-default install path too.
if (-not $WorkDir) {
    if ($env:CFENGINE_TEST_OVERRIDE_WORKDIR) {
        $WorkDir = $env:CFENGINE_TEST_OVERRIDE_WORKDIR
    } elseif ($PSScriptRoot -and (Split-Path $PSScriptRoot -Leaf) -eq 'bin') {
        $WorkDir = Split-Path $PSScriptRoot -Parent
    } else {
        $WorkDir = Join-Path $env:ProgramFiles 'Cfengine'
    }
}
$BinDir = Join-Path $WorkDir 'bin'

if (-not (Test-Path $WorkDir)) {
    Write-Error "CFEngine working directory not found: $WorkDir`nPass -WorkDir <path> if CFEngine is installed elsewhere."
    exit 1
}

# --- Must be Administrator -------------------------------------------------
$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = New-Object Security.Principal.WindowsPrincipal($identity)
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    Write-Error "cf-support.ps1 must be run as Administrator"
    exit 1
}

# --- Collection naming -----------------------------------------------------
$caseNumber = 'NA'
if (-not $Yes) {
    $answer = Read-Host 'If you have it, please enter your support case number'
    if ($answer) { $caseNumber = $answer }
}
# Keep the name filesystem-safe; a pasted case reference may contain anything.
$caseNumber = ($caseNumber -replace '[^\w.-]', '_')

$timestamp = Get-Date -Format 'yyyy-MM-dd-HHmm'
$collection = "cfengine_support_case_$caseNumber-$env:COMPUTERNAME-$timestamp"
$tmpRoot = Join-Path ([System.IO.Path]::GetTempPath()) ([System.IO.Path]::GetRandomFileName())
$tmpdir = Join-Path $tmpRoot $collection
New-Item -ItemType Directory -Path $tmpdir -Force | Out-Null
$infoFile = Join-Path $tmpdir 'system-info.txt'

# --- Helpers (mirror log_cmd / file_add / gzip_add) -------------------------
function Add-Info {
    <#  Record a labelled block in system-info.txt, like log_cmd does.  #>
    param([string]$Label, [scriptblock]$Script)
    "** $Label" | Out-File -FilePath $infoFile -Append -Encoding utf8
    try {
        $out = & $Script 2>&1 | Out-String
        $out | Out-File -FilePath $infoFile -Append -Encoding utf8
        Write-Host "Captured output of $Label"
    } catch {
        "ERROR: $($_.Exception.Message)" | Out-File -FilePath $infoFile -Append -Encoding utf8
        Write-Host "Failed to capture $Label"
    }
    '' | Out-File -FilePath $infoFile -Append -Encoding utf8
}

function Add-Exe {
    <#  Run a CFEngine binary and record its output.  #>
    param([string]$Exe, [string[]]$Arguments = @())
    $path = Join-Path $BinDir $Exe
    if (-not (Test-Path $path)) {
        "** $Exe $Arguments" | Out-File -FilePath $infoFile -Append -Encoding utf8
        "$path not found" | Out-File -FilePath $infoFile -Append -Encoding utf8
        '' | Out-File -FilePath $infoFile -Append -Encoding utf8
        Write-Host "Command not found: $path"
        return
    }
    Add-Info "$Exe $($Arguments -join ' ')" { & $path @Arguments }
}

function Add-ExeToFile {
    param([string]$Exe, [string[]]$Arguments, [string]$OutName)
    $path = Join-Path $BinDir $Exe
    if (-not (Test-Path $path)) {
        Write-Host "Command not found: $path"
        return
    }
    & $path @Arguments 2>&1 | Out-File -FilePath (Join-Path $tmpdir $OutName) -Encoding utf8
    Write-Host "Captured output of $Exe to $OutName"
}

function Add-File {
    param([string]$Path)
    if (Test-Path $Path -PathType Leaf) {
        Copy-Item -Path $Path -Destination $tmpdir -Force
        Write-Host "Added file $Path"
    } else {
        "$Path file not found" | Out-File -FilePath $infoFile -Append -Encoding utf8
    }
}

function Add-Directory {
    param([string]$Path, [string]$Name)
    if (Test-Path $Path -PathType Container) {
        $dest = Join-Path $tmpdir $Name
        Copy-Item -Path $Path -Destination $dest -Recurse -Force
        Write-Host "Added directory $Path"
    } else {
        "$Path directory not found" | Out-File -FilePath $infoFile -Append -Encoding utf8
    }
}

# --- System information ----------------------------------------------------
Add-Info 'hostname' { $env:COMPUTERNAME }
Add-Info 'os version' {
    Get-CimInstance Win32_OperatingSystem |
        Select-Object Caption, Version, BuildNumber, OSArchitecture, ServicePackMajorVersion,
                      InstallDate, LastBootUpTime, LocalDateTime |
        Format-List
}
Add-Info 'computer system' {
    Get-CimInstance Win32_ComputerSystem |
        Select-Object Manufacturer, Model, Domain, PartOfDomain, TotalPhysicalMemory,
                      NumberOfProcessors, NumberOfLogicalProcessors |
        Format-List
}
Add-Info 'powershell version' { $PSVersionTable | Format-List }
Add-Info 'cpu' {
    Get-CimInstance Win32_Processor |
        Select-Object Name, NumberOfCores, NumberOfLogicalProcessors, MaxClockSpeed |
        Format-List
}
Add-Info 'memory' {
    $os = Get-CimInstance Win32_OperatingSystem
    [PSCustomObject]@{
        TotalVisibleMemoryMB = [math]::Round($os.TotalVisibleMemorySize / 1KB)
        FreePhysicalMemoryMB = [math]::Round($os.FreePhysicalMemory / 1KB)
        TotalVirtualMemoryMB = [math]::Round($os.TotalVirtualMemorySize / 1KB)
        FreeVirtualMemoryMB  = [math]::Round($os.FreeVirtualMemory / 1KB)
    } | Format-List
}

Add-Info 'disks' {
    Get-CimInstance Win32_LogicalDisk |
        Select-Object DeviceID, DriveType, FileSystem, VolumeName,
                      @{n='SizeGB';e={[math]::Round($_.Size/1GB,2)}},
                      @{n='FreeGB';e={[math]::Round($_.FreeSpace/1GB,2)}} |
        Format-Table -AutoSize
}

Add-Info 'processes' {
    # StartTime raises Access Denied on protected processes even as Administrator.
    Get-Process | Sort-Object -Property WS -Descending | Select-Object -First 60 |
        Select-Object Id, ProcessName, CPU, WS,
                      @{n='StartTime';e={ try { $_.StartTime } catch { 'n/a' } }} |
        Format-Table -AutoSize
}
Add-Info 'top processes by cpu' {
    Get-Process | Sort-Object -Property CPU -Descending |
        Select-Object -First 10 Id, ProcessName, CPU, WS |
        Format-Table -AutoSize
}

Add-Info 'network configuration' { ipconfig /all }
Add-Info 'listening sockets' { netstat -ano }

# --- CFEngine specific -----------------------------------------------------
Add-Exe 'cf-promises.exe' @('-V')
Add-Exe 'cf-key.exe' @('-p', (Join-Path $WorkDir 'ppkeys\localhost.pub'))
Add-Exe 'cf-key.exe' @('-s', '-n')
Add-Exe 'cf-check.exe' @('diagnose')

Add-Info 'policy version' {
    $promises = Join-Path $WorkDir 'inputs\promises.cf'
    if (Test-Path $promises) { Select-String -Path $promises -Pattern 'version\s*=' }
    else { "$promises not found" }
}

Add-ExeToFile 'cf-promises.exe' @('--show-classes', '--show-vars') 'classes-and-vars.txt'
Add-ExeToFile 'cf-agent.exe' @('--no-lock', '--file', 'update.cf',
    '--show-evaluated-classes', '--show-evaluated-vars') 'update-evaluated-classes-and-vars.txt'
Add-ExeToFile 'cf-agent.exe' @('--no-lock', '--file', 'promises.cf',
    '--show-evaluated-classes', '--show-evaluated-vars') 'promises-evaluated-classes-and-vars.txt'

# The MSI installs the executor as a service; other components are started by it.
Add-Info 'cfengine services' {
    Get-Service | Where-Object { $_.Name -like '*Cfengine*' -or $_.DisplayName -like '*CFEngine*' } |
        Select-Object Name, DisplayName, Status, StartType | Format-List
}
Add-Info 'cfengine service configuration' {
    Get-CimInstance Win32_Service |
        Where-Object { $_.Name -like '*Cfengine*' } |
        Select-Object Name, DisplayName, State, StartMode, StartName, PathName, ProcessId |
        Format-List
}

# The Windows equivalent of the POSIX script's syslog grep; the agent logs to
# the event log as the 'Cfengine Nova' source.
Add-Info 'cfengine event log (System, Cfengine Nova)' {
    try {
        Get-WinEvent -FilterHashtable @{LogName='System'; ProviderName='Cfengine Nova'} `
                     -MaxEvents 2000 -ErrorAction Stop |
            Select-Object TimeCreated, Id, LevelDisplayName, Message | Format-List
    } catch {
        "No events found for provider 'Cfengine Nova' in the System log: $($_.Exception.Message)"
    }
}
Add-Info 'cfengine event log (Application)' {
    try {
        Get-WinEvent -FilterHashtable @{LogName='Application'; ProviderName='Cfengine Nova'} `
                     -MaxEvents 2000 -ErrorAction Stop |
            Select-Object TimeCreated, Id, LevelDisplayName, Message | Format-List
    } catch {
        "No events found for provider 'Cfengine Nova' in the Application log"
    }
}

# The capped captures above can be flooded out by Information events, and some
# warnings exist nowhere else: an oversized report entry is dropped with only a
# client-side WARNING, so the hub never sees it.
Add-Info 'cfengine event log (warnings and errors only)' {
    $found = $false
    foreach ($logName in @('System', 'Application')) {
        try {
            $events = Get-WinEvent -FilterHashtable @{
                LogName = $logName; ProviderName = 'Cfengine Nova'; Level = 1,2,3
            } -MaxEvents 5000 -ErrorAction Stop
            if ($events) {
                $found = $true
                "--- $logName ---"
                $events | Select-Object TimeCreated, Id, LevelDisplayName, Message | Format-List
            }
        } catch { }
    }
    if (-not $found) { "No warning or error events for provider 'Cfengine Nova'" }
}

# The one report-collection failure mode with no hub-side evidence.
Add-Info 'report entries dropped for exceeding network limit' {
    $hits = @()
    foreach ($logName in @('System', 'Application')) {
        try {
            $hits += Get-WinEvent -FilterHashtable @{
                LogName = $logName; ProviderName = 'Cfengine Nova'; Level = 1,2,3
            } -MaxEvents 5000 -ErrorAction Stop |
                Where-Object { $_.Message -match 'exceeds network limit' }
        } catch { }
    }
    if ($hits.Count -eq 0) {
        'none found'
    } else {
        "$($hits.Count) dropped report entr(ies) -- these never reached the hub"
        $hits | Select-Object TimeCreated, Message | Format-List
    }
}

# Rows over CF_MAXTRANSSIZE (4024) are dropped from the reply with only a local
# WARNING. state\diff holds the same serialization, so count them first.
# Records are delimited by quote parity, not newlines.
Add-Info 'report rows exceeding CF_MAXTRANSSIZE in state\diff' {
    $limit = 4024
    $diffDir = Join-Path $WorkDir 'state\diff'
    if (-not (Test-Path $diffDir)) { "state\diff not found"; return }
    $files = @(Get-ChildItem $diffDir -Filter *.diff -ErrorAction SilentlyContinue)
    if ($files.Count -eq 0) { "no .diff files present"; return }
    foreach ($f in $files) {
        $rec = ''; $inq = 0; $n = 0; $max = 0; $over = 0
        foreach ($line in [System.IO.File]::ReadLines($f.FullName)) {
            $q = 0
            foreach ($ch in $line.ToCharArray()) { if ($ch -eq '"') { $q++ } }
            $rec = if ($rec -eq '') { $line } else { $rec + "`r`n" + $line }
            $inq = ($inq + $q) % 2
            if ($inq -eq 0) {
                # strlen() counts bytes, not characters.
                $len = [System.Text.Encoding]::UTF8.GetByteCount($rec)
                $n++
                if ($len -gt $max) { $max = $len }
                if ($len -gt $limit) { $over++ }
                $rec = ''
            }
        }
        "- $($f.Name): records=$n max_bytes=$max over_limit=$over"
    }
}

# --- Files -----------------------------------------------------------------
Add-File (Join-Path $WorkDir 'policy_server.dat')
Add-File (Join-Path $WorkDir 'outputs\previous')

foreach ($installLog in (Get-ChildItem -Path $WorkDir -Filter 'CFEngine-Install*' -ErrorAction SilentlyContinue)) {
    Add-File $installLog.FullName
}

# report_dumps live here; what a report collection problem is diagnosed from.
$diagnostics = Join-Path $WorkDir 'diagnostics'
if (Test-Path $diagnostics) {
    Write-Host 'Collecting diagnostics directory (includes report_dumps)...'
    Add-Directory $diagnostics 'diagnostics'
    $dumpDir = Join-Path $diagnostics 'report_dumps'
    $dumpCount = 0
    if (Test-Path $dumpDir) {
        $dumpCount = (Get-ChildItem -Path $dumpDir -File -ErrorAction SilentlyContinue | Measure-Object).Count
    }
    "** Report dumps" | Out-File -FilePath $infoFile -Append -Encoding utf8
    "- $dumpCount file(s) in diagnostics\report_dumps" | Out-File -FilePath $infoFile -Append -Encoding utf8
    "- enable_report_dumps present: $(Test-Path (Join-Path $WorkDir 'enable_report_dumps'))" |
        Out-File -FilePath $infoFile -Append -Encoding utf8
    '' | Out-File -FilePath $infoFile -Append -Encoding utf8
} else {
    Write-Host 'diagnostics directory not found, skipping'
}

# --- Masterfiles (optional, can be large) ----------------------------------
$includeMasterfiles = $true
if (-not $Yes) {
    $response = Read-Host 'Include masterfiles in support submission [Y/n]'
    if ($response -and $response -notmatch '^[Yy]') { $includeMasterfiles = $false }
}
if ($includeMasterfiles) {
    Add-Directory (Join-Path $WorkDir 'masterfiles') 'masterfiles'
} else {
    Write-Host 'Not including masterfiles in support submission'
}

# --- Archive ---------------------------------------------------------------
# Compress-Archive needs PS 5.0, tar.exe ships with Server 2019+. If neither is
# available leave the directory rather than failing after the collection work.
$archive = Join-Path (Get-Location) "$collection.zip"
$archived = $false
if (Get-Command Compress-Archive -ErrorAction SilentlyContinue) {
    try {
        Compress-Archive -Path $tmpdir -DestinationPath $archive -Force -ErrorAction Stop
        $archived = $true
    } catch {
        Write-Host "Compress-Archive failed: $($_.Exception.Message)"
    }
}
if (-not $archived -and (Get-Command tar.exe -ErrorAction SilentlyContinue)) {
    $archive = Join-Path (Get-Location) "$collection.tar.gz"
    & tar.exe -czf $archive -C $tmpRoot $collection
    if ($LASTEXITCODE -eq 0) { $archived = $true }
}

if ($archived) {
    Remove-Item -Path $tmpRoot -Recurse -Force -ErrorAction SilentlyContinue
    Write-Host "Please send $archive to CFEngine support staff."
} else {
    Write-Host "Could not create an archive; the collection is at $tmpdir"
    Write-Host 'Please compress that directory manually and send it to CFEngine support staff.'
}
