@echo off
setlocal
set "SRC_DIR=%~dp0"
if not exist build\ mkdir build
pushd build
cl /DPLATFORM_WIN32 /Zi /W3 /nologo %SRC_DIR%\toui.c user32.lib
popd build
