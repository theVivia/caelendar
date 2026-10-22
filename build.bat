@echo off
rem Build XingLi (MinGW-w64 g++, static single exe, release -O2)
setlocal
cd /d "%~dp0"
if not exist bin mkdir bin
set CXX=g++
set CXXFLAGS=-std=c++17 -O2 -municode -mwindows -DUNICODE -D_UNICODE -DNOMINMAX -D_WIN32_WINNT=0x0A00 -DWINVER=0x0A00 -I src
set LIBS=-lgdiplus -lcomctl32 -lshlwapi -lshell32 -luser32 -lgdi32 -lole32 -luuid -lmsimg32 -ldwmapi -lwinmm -lcomdlg32
set LDFLAGS=-municode -mwindows -static -static-libgcc -static-libstdc++ -s
if exist res\xingli.rc (
  pushd res
  windres xingli.rc -O coff -o ..\bin\xingli_res.o
  popd
)
%CXX% %CXXFLAGS% src\*.cpp bin\xingli_res.o -o bin\XingLi.exe %LIBS% %LDFLAGS%
echo BUILD_EXIT=%ERRORLEVEL%