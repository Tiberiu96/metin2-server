param(
    [string]$VsTools = 'C:/Program Files/Microsoft Visual Studio/18/Insiders/VC/Auxiliary/Build/vcvars32.bat',
    [string]$PythonDll = 'C:/Users/skema/Desktop/ClientIgnition/python27.dll'
)
$ErrorActionPreference = 'Stop'
$build = Join-Path $env:TEMP ('private-shop-timer-' + [guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($build) | Out-Null
$source = Join-Path $PSScriptRoot 'private_shop_python_runner.cpp'
$test = Join-Path $PSScriptRoot 'private_shop_timer.py'
$exe = Join-Path $build 'timer.exe'
$obj = Join-Path $build 'timer.obj'
$command = '""{0}" >nul && cl /nologo /EHsc "{1}" /Fo"{2}" /Fe"{3}" && "{3}" "{4}" "{5}""' -f $VsTools, $source, $obj, $exe, $PythonDll, $test
$process = New-Object Diagnostics.Process
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
if ($process.ExitCode -ne 0) { throw "Timer tests failed: $($process.ExitCode)" }
