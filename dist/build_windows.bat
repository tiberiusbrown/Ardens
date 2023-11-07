cd /D "%~dp0"

rmdir /s /q build
mkdir build
cmake -B build .. -DARDENS_LLVM=0 -DARDENS_DEBUGGER=0 -DARDENS_PLAYER=0 -DARDENS_DIST=1
cmake --build build --config Release -j %NUMBER_OF_PROCESSORS%
echo f | xcopy /f /y build\Release\*.exe .\

pause
