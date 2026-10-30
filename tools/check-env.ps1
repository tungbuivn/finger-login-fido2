# check-env.ps1 - chi doc, khong thay doi he thong.
$ErrorActionPreference = 'Continue'

function Line($name, $value) {
    Write-Host ("{0,-28} {1}" -f ($name + ":"), $value)
}

Write-Host "=== Secure Boot ==="
try {
    $sb = Confirm-SecureBootUEFI -ErrorAction Stop
    Line "SecureBoot" $sb
} catch {
    Line "SecureBoot" "khong xac dinh / khong ho tro"
}

Write-Host "`n=== Test signing ==="
try {
    $b = bcdedit 2>$null | Select-String -Pattern 'testsigning'
    if ($b) { Line "testsigning" ($b.Line.Trim()) } else { Line "testsigning" "khong thay / chua bat" }
} catch {
    Line "testsigning" "can Administrator de doc bcdedit"
}

Write-Host "`n=== Windows Kits ==="
$kits = "C:\Program Files (x86)\Windows Kits\10\Include"
if (Test-Path $kits) {
    $vhf = Get-ChildItem $kits -Recurse -Filter vhf.h -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($vhf) { Line "vhf.h" $vhf.FullName } else { Line "vhf.h" "KHONG THAY - can WDK" }
    Get-ChildItem $kits -Directory | Select-Object -First 5 -ExpandProperty Name | ForEach-Object { Line "SDK/WDK" $_ }
} else {
    Line "Windows Kits" "KHONG THAY $kits"
}

Write-Host "`n=== Visual Studio / MSBuild ==="
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (Test-Path $vswhere) {
    & $vswhere -latest -products * -property installationPath | ForEach-Object { Line "VS" $_ }
} else {
    Line "VS" "khong thay vswhere.exe"
}

Write-Host "`n=== devcon ==="
$devcon = Get-Command devcon.exe -ErrorAction SilentlyContinue
if ($devcon) { Line "devcon" $devcon.Source } else { Line "devcon" "khong co trong PATH (WDK Tools)" }

Write-Host "`n=== Browser ==="
foreach ($p in @(
    "$env:ProgramFiles\Google\Chrome\Application\chrome.exe",
    "${env:ProgramFiles(x86)}\Microsoft\Edge\Application\msedge.exe"
)) {
    if (Test-Path $p) { Line "browser" $p }
}

Write-Host "`nXong. Khong thay doi cau hinh nao."
