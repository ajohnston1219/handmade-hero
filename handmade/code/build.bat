@echo off

:: TODO - can we just build both with one exe?

IF NOT EXIST ..\..\build mkdir ..\..\build
pushd ..\..\build
cl -MT -nologo -Gm- -GR- -EHa- -Oi -WX -W4 -wd4201 -wd4100 -wd4189 -DHANDMADE_INTERNAL=1 -DHANDMADE_SLOW=1 -FC -Z7 -Fmwin32_handmade.map ..\handmade\code\win32_handmade.cpp /link -opt:ref -subsystem:windows,5.1 user32.lib Gdi32.lib
::  WX:   Treat warnings as errors
::  W4:   Warning level
::  wd:   Ignore warning
::  D:    Declare variable
::  FC:   Display full path of source code files
::  Zi:   Complete debugging info
::  Z7:   Older version of debugging info
::  Oi:   Use intrinsics when possible
::  GR-:  Turn off runtime type info
::  EHa-: Turn off exception handling
::  -Gm-: Turn off minimal rebuild
::  -Fm:  Build a map file
::  -opt:ref: Be aggressive about not importing too much
::  -subsystem:windows,5.1 for backwards compatability (XP)
popd