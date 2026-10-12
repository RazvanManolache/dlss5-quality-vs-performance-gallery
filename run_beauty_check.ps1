param([string]$Build,[string]$Check,[string]$Output)
$ErrorActionPreference='Stop'
Set-Location -LiteralPath $PSScriptRoot
$conflicts=Get-Process -ErrorAction SilentlyContinue | Where-Object { $_.ProcessName -match '^(scene_lab|layer_profiler|frame_replay|kernel_profiler|re9|re8|Cyberpunk2077)$' }
if($conflicts){$conflicts | Select-Object Id,ProcessName;throw 'A game or GPU check is running.'}
$guardRoots=@($PSScriptRoot)
if(Test-Path -LiteralPath (Join-Path $PSScriptRoot 'local.json')){
    $configuration=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'local.json') -Raw | ConvertFrom-Json
    $guardRoots+=@($configuration.gpu_guard_roots | Where-Object { $_ -and (Test-Path -LiteralPath $_) })
}
$markers=rg --files --hidden --no-ignore -g '*gpu.ready' @guardRoots | ForEach-Object {Get-Item -LiteralPath $_} | Where-Object LastWriteTime -GT (Get-Date).AddMinutes(-30)
if($markers){$markers | Select-Object FullName,LastWriteTime;throw 'A recent GPU ready marker exists.'}
$gpu=nvidia-smi --query-gpu=utilization.gpu,memory.used --format=csv,noheader,nounits
Write-Output "GPU state before check: $gpu"
if([int]($gpu.Split(',')[0].Trim()) -gt 12){throw 'GPU is busy.'}
if(Test-Path -LiteralPath $Output){throw 'Fresh evidence directory required.'}
$marker=Join-Path $PSScriptRoot (Join-Path $Build 'beauty.gpu.ready')
Set-Content -LiteralPath $marker -Value "$PID $Check $Output $(Get-Date -Format o)"
try{& (Join-Path $PSScriptRoot (Join-Path $Build 'scene_lab.exe')) $Check $Output *> ($Output+'_console.log');$beautyExit=$LASTEXITCODE}finally{Remove-Item -LiteralPath $marker}
exit $beautyExit
