param([string]$VsTools = 'C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\Auxiliary\Build\vcvars64.bat')
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$source = [IO.File]::ReadAllText((Join-Path $repo 'src/server/db/src/ClientManagerPrivateShop.cpp'))
$names = @('DeletePrivateShop', 'PrivateShopGameDespawn', 'PrivateShopStartPremiumEvent', 'UpdatePrivateShopPremiumEvent', 'PrivateShopFailedBuy', 'PrivateShopBuy', 'PrivateShopWithdraw')
$functions = foreach ($name in $names) {
    $pattern = '(?ms)^(?:void|bool) CClientManager::' + $name + '\([^\r\n]*\)\r?\n\{.*?^\}'
    $match = [regex]::Match($source, $pattern)
    if (!$match.Success) { throw "Missing production function: $name" }
    $match.Value
}
$build = Join-Path ([IO.Path]::GetTempPath()) ('private-shop-tests-' + [guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($build) | Out-Null
[IO.File]::WriteAllText((Join-Path $build 'private_shop_functions.inc'), ($functions -join "`n"))
$gameSource = [IO.File]::ReadAllText((Join-Path $repo 'src/server/game/src/private_shop_manager.cpp'))
$gameFunctions = foreach ($name in @('SetPendingBuildBundle', 'PreservePendingBuild', 'BuildPrivateShopResult')) {
    $match = [regex]::Match($gameSource, '(?ms)^void CPrivateShopManager::' + $name + '\([^\r\n]*\)\r?\n\{.*?^\}')
    if (!$match.Success) { throw "Missing production game function: $name" }
    $match.Value
}
[IO.File]::WriteAllText((Join-Path $build 'private_shop_game_functions.inc'), ($gameFunctions -join "`n"))
$test = Join-Path $PSScriptRoot 'private_shop_lifecycle.cpp'
$exe = Join-Path $build 'lifecycle.exe'
$obj = Join-Path $build 'lifecycle.obj'
$command = '""{0}" >nul && cl /nologo /EHsc /std:c++17 /W3 /I"{1}" "{2}" /Fo"{3}" /Fe"{4}" && "{4}""' -f $VsTools, $build, $test, $obj, $exe
$gameTest = Join-Path $PSScriptRoot 'private_shop_pending_build.cpp'
$gameExe = Join-Path $build 'pending.exe'
$gameObj = Join-Path $build 'pending.obj'
$command = $command.Substring(0, $command.Length - 1) + (' && cl /nologo /EHsc /std:c++17 /W3 /I"{0}" "{1}" /Fo"{2}" /Fe"{3}" && "{3}""' -f $build, $gameTest, $gameObj, $gameExe)
$process = New-Object System.Diagnostics.Process
$process.StartInfo.FileName = $env:ComSpec
$process.StartInfo.Arguments = '/d /s /c ' + $command
$process.StartInfo.UseShellExecute = $false
$process.StartInfo.CreateNoWindow = $true
$process.StartInfo.RedirectStandardOutput = $true
$process.StartInfo.RedirectStandardError = $true
$process.Start() | Out-Null
$stdout = $process.StandardOutput.ReadToEndAsync()
$stderr = $process.StandardError.ReadToEndAsync()
$process.WaitForExit()
Write-Output $stdout.Result
Write-Output $stderr.Result
if ($process.ExitCode -ne 0) { throw "Lifecycle regression tests failed: $($process.ExitCode)" }
