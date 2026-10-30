@echo off
setlocal EnableExtensions EnableDelayedExpansion
cd /d "%~dp0"

rem MIMIC - tek tikla Windows derlemesi.
rem Visual Studio C++ araclari veya CMake yoksa kendisi kurmayi teklif eder (winget ile).

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"

call :findvs
if not defined VSPATH (
  echo [MIMIC] Visual Studio C++ derleme araclari bulunamadi.
  echo [MIMIC] Ucretsiz "Build Tools for Visual Studio 2022" kurulacak ^(yaklasik 3-5 GB, 10-20 dk^).
  echo [MIMIC] Bir "Evet/Hayir" izin penceresi cikarsa "Evet"e bas.
  choice /c EH /m "Simdi kurulsun mu"
  if errorlevel 2 exit /b 1
  where winget >nul 2>nul
  if errorlevel 1 (
    echo [MIMIC] winget yok. Microsoft Store'dan "App Installer" uygulamasini guncelle, sonra tekrar dene.
    pause
    exit /b 1
  )
  winget install --id Microsoft.VisualStudio.2022.BuildTools -e --accept-package-agreements --accept-source-agreements --override "--wait --passive --add Microsoft.VisualStudio.Workload.VCTools --add Microsoft.VisualStudio.Component.VC.CMake.Project --includeRecommended"
  call :findvs
  if not defined VSPATH (
    echo [MIMIC] Kurulum tamamlanamadi. Bilgisayari yeniden baslatip build.bat'i tekrar ac.
    pause
    exit /b 1
  )
)
echo [MIMIC] Visual Studio: !VSPATH!

call :findcmake
if not defined CMAKE (
  echo [MIMIC] CMake bulunamadi, kuruluyor...
  winget install --id Kitware.CMake -e --accept-package-agreements --accept-source-agreements
  call :findcmake
)
if not defined CMAKE (
  echo [MIMIC] CMake kurulamadi. https://cmake.org/download adresinden "Windows x64 Installer"i kurup tekrar dene.
  pause
  exit /b 1
)
echo [MIMIC] CMake: !CMAKE!

echo.
echo [MIMIC] Proje hazirlaniyor ^(ilk seferde JUCE ve WebView2 SDK indirilir^)...
"!CMAKE!" -S . -B build -A x64
if errorlevel 1 goto fail

echo [MIMIC] Derleniyor ^(birkac dakika surer^)...
"!CMAKE!" --build build --config Release --parallel
if errorlevel 1 goto fail

set "OUT=build\MIMIC_artefacts\Release\VST3\MIMIC.vst3"
set "DEST=%CommonProgramFiles%\VST3"

echo.
echo [MIMIC] Derleme tamam: %OUT%
echo [MIMIC] %DEST% klasorune kopyalaniyor...
xcopy /E /I /Y /Q "%OUT%" "%DEST%\MIMIC.vst3" >nul 2>nul
if errorlevel 1 (
  echo [MIMIC] Kopyalamak icin yonetici izni lazim. build.bat'a sag tikla ^> "Yonetici olarak calistir"
  echo         ya da "%OUT%" klasorunu elle "%DEST%" icine tasi.
) else (
  echo [MIMIC] Yuklendi. FL Studio'da: Options ^> Manage plugins ^> Find plugins.
)
pause
exit /b 0

:fail
echo.
echo [MIMIC] Hata olustu. Bu pencerenin ekran goruntusunu Claude'a gonder.
pause
exit /b 1

rem ------------------------------------------------------------------
:findvs
set "VSPATH="
if exist "%VSWHERE%" (
  for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSPATH=%%i"
)
exit /b 0

:findcmake
set "CMAKE="
for /f "delims=" %%i in ('where cmake 2^>nul') do if not defined CMAKE set "CMAKE=%%i"
if not defined CMAKE if exist "%ProgramFiles%\CMake\bin\cmake.exe" set "CMAKE=%ProgramFiles%\CMake\bin\cmake.exe"
if not defined CMAKE if defined VSPATH if exist "!VSPATH!\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" set "CMAKE=!VSPATH!\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
exit /b 0
