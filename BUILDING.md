# Download a GitHub build

Open **Actions > Windows gallery package > Run workflow**. Download the successful
run's **DLSS5-Gallery-windows-x64** artifact and extract the ZIP inside it.

The package contains the Windows x64 executable, shaders and all 41 Phoenix GPU
kernels for **sm_120 (RTX 50-series desktop architecture)**. Compilation and package
integrity are checked on GitHub; GPU rendering and performance are not tested there.

## First run

You need Windows, a compatible NVIDIA driver/GPU, Python 3.11+ for setup, and your
own supported `nvngx_dlssnr.dll` as model data. This package does not download or
distribute that DLL. Its required SHA-256 is
`e16bcf15e16e13f527491cdf7845b2fe6521a738d8f7c9c721866a8496e1fc8e`.

Run `SETUP.cmd "C:\Models\nvngx_dlssnr.dll"` from the extracted folder. Setup
verifies the package, checks the model, downloads and verifies the separate scene
asset pack (about 972 MiB, 3.5 GiB installed), and creates your local configuration.
Then open `RUN.cmd`. No Visual Studio or CUDA compiler is needed to use this download.
The scene pack includes its own attribution and licenses.

For an existing verified asset cache, use
`python runtime_setup.py --model "C:\Models\nvngx_dlssnr.dll" --offline`.
Use `--asset-pack PATH` for a previously downloaded asset ZIP. Use
`python runtime_setup.py --verify-package` to check binaries without models or a GPU.

## Build from source

The existing README describes full local builds. GitHub pins the Phoenix submodule,
public NVIDIA NGX headers and CUDA 12.8.1 compiler archives. Archive SHA-256 values
come from NVIDIA's [CUDA redistribution manifest](https://developer.download.nvidia.com/compute/cuda/redist/redistrib_12.8.1.json).
`python build.py gallery_desktop --compile-only --ngx-sdk PATH` compiles public
inputs without creating a local model configuration or downloading scene assets.
The executable links the C++ runtime statically. Model data and generated local
configuration are excluded from the downloadable package.
