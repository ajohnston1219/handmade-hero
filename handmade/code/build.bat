@echo off

set CommonCompilerFlags=-MT -nologo -Gm- -GR- -EHa- -Od -Oi -WX -W4 -wd4201 -wd4100 -wd4189 -DHANDMADE_INTERNAL=1 -DHANDMADE_SLOW=1 -FC -Z7 -Fmwin32_handmade.map

set CommonLinkerFlags=-opt:ref user32.lib Gdi32.lib winmm.lib
:: TODO - can we just build both with one exe?

IF NOT EXIST ..\..\build mkdir ..\..\build
pushd ..\..\build

:: 32-bit build
:: cl %CommonCompilerFlags% ..\handmade\code\win32_handmade.cpp /link -subsystem:windows,5.1 %CommonLinkerFLags%
:: 64-bit build
cl %CommonCompilerFlags% ..\handmade\code\win32_handmade.cpp /link %CommonLinkerFLags%

::  WX:   Treat warnings as errors
::  W4:   Warning level
::  wd:   Ignore warning
::  D:    Declare variable
::  FC:   Display full path of source code files
::  Zi:   Complete debugging info
::  Z7:   Older version of debugging info
::  Oi:   Use intrinsics when possible
:: -Od:   Turn off optimizations for debugging
::  GR-:  Turn off runtime type info
::  EHa-: Turn off exception handling
::  -Gm-: Turn off minimal rebuild
::  -Fm:  Build a map file
::  -opt:ref: Be aggressive about not importing too much
::  -subsystem:windows,5.1 for backwards compatability (XP)
popd