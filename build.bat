@echo off
setlocal
set "SRC_DIR=%~dp0"
if not exist build\ mkdir build
pushd build
cl /D_CRT_SECURE_NO_WARNINGS /DPLATFORM_WIN32 /Zi /W3 /nologo %SRC_DIR%\example.c user32.lib gdi32.lib
popd build
