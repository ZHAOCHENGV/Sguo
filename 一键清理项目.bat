@echo off
echo ========================================
echo 清理 UE5 项目
echo ========================================
echo.

REM 关闭可能运行的进程
taskkill /F /IM UnrealEditor.exe 2>nul
taskkill /F /IM UnrealBuildTool.exe 2>nul
timeout /t 2 /nobreak >nul

echo 删除 Binaries 文件夹...
if exist "Binaries" rmdir /s /q "Binaries"

echo 删除 Intermediate 文件夹...
if exist "Intermediate" rmdir /s /q "Intermediate"

echo 删除 Saved 文件夹...
if exist "Saved" rmdir /s /q "Saved"

echo 删除 DerivedDataCache 文件夹...
if exist "DerivedDataCache" rmdir /s /q "DerivedDataCache"

echo 删除 .vs 文件夹...
if exist ".vs" rmdir /s /q ".vs"

echo 删除解决方案文件...
del /q *.sln 2>nul
del /q *.suo 2>nul
del /q *.user 2>nul

echo 删除插件的临时文件...
for /d %%i in (Plugins\*) do (
    if exist "%%i\Binaries" rmdir /s /q "%%i\Binaries"
    if exist "%%i\Intermediate" rmdir /s /q "%%i\Intermediate"
)

echo.
echo ========================================
echo 清理完成！
echo ========================================
echo.
echo 现在请：
echo 1. 右键点击 .uproject 文件
echo 2. 选择 "Generate Visual Studio project files"
echo 3. 打开 .sln 文件重新编译
echo.
pause