@echo off

IF NOT EXIST ..\..\build mkdir ..\..\build
pushd ..\..\build
cl -WX -W4 -wd4201 -wd4100 -wd4189 -DHANDMADE_INTERNAL=1 -DHANDMADE_SLOW=1 -FC -Zi ..\handmade\code\win32_handmade.cpp user32.lib Gdi32.lib
::  WX: Treat warnings as errors
::  W4: Warning level
::  wd: Ignore warning
::  D:  Declare variable
::  FC: Display full path of source code files
::  Zi: Complete debugging info
popd