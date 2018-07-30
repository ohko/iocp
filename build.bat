@echo off

mkdir out

call "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\Common7\Tools\VsDevCmd.bat"
cd D:\vs\iocp\
d:

msbuild iocp.sln /nologo /clp:Summary;Verbosity=minimal /p:Configuration=Release;Platform=x86

copy Release\server.exe out
copy Release\client.exe out
copy Release\iocp.lib out

cd server
rd /S /Q Debug
rd /S /Q Release
rd /S /Q x64
cd ..

cd client
rd /S /Q Debug
rd /S /Q Release
rd /S /Q x64
cd ..

cd iocp
rd /S /Q Debug
rd /S /Q Release
rd /S /Q x64
cd ..

rd /S /Q Debug
rd /S /Q Release
rd /S /Q x64
rd /S /Q .vs

pause
exit
