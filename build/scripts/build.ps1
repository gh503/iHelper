param(
    [string]$Arch = "x64",
    [string]$BuildType = "Release"
)

# 验证参数
$ValidArchs = @("x64", "x86")
$ValidBuildTypes = @("Debug", "Release", "MinSizeRel", "RelWithDebInfo")

if (-not $ValidArchs.Contains($Arch)) {
    Write-Host "Invalid architecture: $Arch. Valid: $($ValidArchs -join ', ')"
    exit 1
}

if (-not $ValidBuildTypes.Contains($BuildType)) {
    Write-Host "Invalid build type: $BuildType. Valid: $($ValidBuildTypes -join ', ')"
    exit 1
}

# 设置路径
$CurrentPath = Split-Path $PSScriptRoot -Parent
$ProjectRoot = Split-Path $CurrentPath -Parent
Write-Host "$ProjectRoot"
$Preset = "windows-$Arch-$($BuildType.ToLower())"
$BuildDir = "$ProjectRoot\build\build\$Preset"
$TargetsDir = "$ProjectRoot\targets\$Preset"

Write-Host "[iHelper] Building for Windows $Arch $BuildType with Clang..."
Write-Host "Using preset: $Preset"

# 配置项目
Set-Location "$ProjectRoot\src"
Write-Host "Configuring project..."
cmake --preset $Preset

# 创建构建目录
New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null
Set-Location $BuildDir
# 构建项目
Write-Host "Building project..."
cmake --build . --config $BuildType --parallel 8

# 安装项目
Write-Host "Installing project..."
cmake --install . --config $BuildType

Write-Host "[iHelper] Build completed! Output in $TargetsDir"