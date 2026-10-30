# build-driver.ps1 - build Phase 1 VHF driver without Visual Studio WDK integration.
# Requires: WDK installed, VS Build Tools C++.
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$kits = 'C:\Program Files (x86)\Windows Kits\10'

$sdkVer = Get-ChildItem "$kits\Lib" -Directory |
    Where-Object { Test-Path (Join-Path $_.FullName 'km\x64') } |
    Sort-Object Name -Descending | Select-Object -First 1
if (-not $sdkVer) { throw 'Khong tim thay WDK km libs' }
$ver = $sdkVer.Name

$wdf = Get-ChildItem "$kits\Lib\wdf\kmdf\x64" -Directory |
    Sort-Object { [version]($_.Name) } -Descending | Select-Object -First 1
if (-not $wdf) { throw 'Khong tim thay WDF KMDF libs' }

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { throw 'Khong tim thay Visual Studio C++ tools' }
$vcvars = Join-Path $vs 'VC\Auxiliary\Build\vcvars64.bat'
if (-not (Test-Path $vcvars)) { throw "Khong tim thay $vcvars" }

$out = Join-Path $root 'build'
New-Item -ItemType Directory -Force -Path $out | Out-Null

$inc = @(
    "$kits\Include\$ver\km",
    "$kits\Include\$ver\shared",
    "$kits\Include\$ver\km\crt",
    "$kits\Include\wdf\kmdf\$($wdf.Name)"
) | ForEach-Object { "/I`"$_`"" }

$cflags = '/nologo /c /kernel /GS- /Gz /Zl /W3 /D_AMD64_ /DAMD64 /D_WIN64 /DNT /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN /DPOOL_NX_OPTIN=1'
$cl = "cl $cflags $($inc -join ' ') /Fo:`"$out\fido2.obj`" `"$root\fido2.cpp`""
$link = "link /NOLOGO /DRIVER /SUBSYSTEM:NATIVE /MACHINE:X64 /ENTRY:FxDriverEntry /OUT:`"$out\fido2.sys`" `"$out\fido2.obj`" " +
    "/LIBPATH:`"$kits\Lib\$ver\km\x64`" /LIBPATH:`"$kits\Lib\wdf\kmdf\x64\$($wdf.Name)`" " +
    'wdfdriverentry.lib wdfldr.lib vhfkm.lib ntoskrnl.lib hal.lib BufferOverflowK.lib ntstrsafe.lib'

cmd /c "call `"$vcvars`" >nul && $cl && $link"
if ($LASTEXITCODE -ne 0) { throw "Build driver failed ($LASTEXITCODE)" }
Write-Host "OK: $out\fido2.sys"
