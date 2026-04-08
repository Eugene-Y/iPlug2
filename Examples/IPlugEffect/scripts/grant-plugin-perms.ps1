# Run as Administrator (auto-elevates if needed). Optional param -Full to grant Full control.
param([switch]$Full)

function Ensure-Elevated {
  $isElevated = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
  if (-not $isElevated) {
    Write-Host "Relaunching elevated..."
    $pwshCmdInfo = Get-Command pwsh -ErrorAction SilentlyContinue
    $psCmdInfo   = Get-Command powershell -ErrorAction SilentlyContinue
    $pwshExe = if ($pwshCmdInfo) { $pwshCmdInfo.Definition } else { $null }
    $psExe   = if ($psCmdInfo)   { $psCmdInfo.Definition }   else { $null }
    $exe     = if ($pwshExe) { $pwshExe } elseif ($psExe) { $psExe } else { $null }

    if (-not $exe) {
      Write-Error "No 'pwsh' or 'powershell' executable found to relaunch elevated. Run this script in an elevated shell."
      exit 1
    }

    $scriptPath = if ($PSCommandPath) { $PSCommandPath } else { $MyInvocation.MyCommand.Path }
    $argList = "-NoProfile -ExecutionPolicy Bypass -File `"$scriptPath`""
    if ($MyInvocation.UnboundArguments.Count -gt 0) { $argList += " " + ($MyInvocation.UnboundArguments -join ' ') }
    Start-Process -FilePath $exe -ArgumentList $argList -Verb RunAs
    exit
  }
}

Ensure-Elevated

$user = if ($env:USERDOMAIN -and $env:USERDOMAIN -ne "") { "$env:USERDOMAIN\$env:USERNAME" } else { $env:USERNAME }
$acl  = if ($Full) { "F" } else { "M" }

$folders = @(
  "$env:ProgramFiles\VstPlugins",
  "$env:ProgramFiles\Common Files\VST3",
  "$env:ProgramFiles\Common Files\Avid\Audio\Plug-Ins",
  "$env:ProgramFiles\Common Files\CLAP"
)

foreach ($f in $folders) {
  Write-Host ("[POSTBUILD] Processing {0}" -f $f)
  if (-not (Test-Path $f)) {
    Write-Host ("[POSTBUILD] Creating {0}..." -f $f)
    try {
      New-Item -ItemType Directory -Path $f -Force | Out-Null
      Write-Host ("[POSTBUILD][OK] Created {0}" -f $f)
    } catch {
      Write-Warning ("[POSTBUILD] Failed to create {0}: {1}" -f $f, $_.Exception.Message)
    }
  }

  $perm = "{0}:(OI)(CI){1}" -f $user, $acl
  Write-Host ("[POSTBUILD] Granting {0} {1} on {2} (icacls arg: {3})" -f $user, $acl, $f, $perm)

  $icaclsOutput = & icacls.exe $f /grant $perm /T 2>&1
  if ($LASTEXITCODE -ne 0) {
    Write-Warning ("[POSTBUILD] icacls failed on {0} (exit {1})" -f $f, $LASTEXITCODE)
    Write-Host $icaclsOutput
  } else {
    Write-Host ("[POSTBUILD][OK] Permissions set on {0}" -f $f)
  }
  Write-Host ""
}

Write-Host "Done."