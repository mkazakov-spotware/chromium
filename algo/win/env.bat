@echo off

rem SET __CT_DOTNET_PATH_x64=C:\Program Files\dotnet
rem SET __CT_DOTNET_PATH_x86=C:\Program Files (x86)\dotnet
SET __CT_DOTNET_PATH=C:\Program Files (x86)\dotnet
SET __CT_HOSTFXR_PATH=C:\Program Files (x86)\dotnet\host\fxr\6.0.20\hostfxr.dll
SET __CT_PRODUCT_PATH=C:\Users\alex\lab\chromium2\src\out

SET __CT_DOTNET_gcServer=1
SET __CT_DOTNET_gcConcurrent=1
SET __CT_DOTNET_GCCpuGroup=1
SET __CT_DOTNET_Thread_UseAllCpuGroups=1

SET DOTNET_EnableDiagnostics=0
SET COMPlus_EnableDiagnostics=0
SET CORECLR_ENABLE_PROFILING=0
