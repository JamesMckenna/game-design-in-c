@echo off


set CommonCompilerFlags=-MT -nologo -Gm- -GR- -EHa -Od -Oi -WX -W4 -wd4201 -wd4100 -wd4189 -DHANDMADE_INTERNAL=1 -DHANDMADE_SLOW=1 -DHANDMADE_WIN32=1 -FC -Zi 
set CommonLinkerFlags= -incremental:no -opt:ref user32.lib gdi32.lib winmm.lib


IF NOT EXIST .\build mkdir .\build
pushd .\build
REM 32bit buils
REM cl %CommonCompilerFlags% ..\code\win32_handmade.cpp /link -subsystem:windows,5.1 %CommonLinkerFlags% 

REM 64bit build
del *.pdb > NUL 2> NUL
cl %CommonCompilerFlags% ..\code\handmade.cpp -Fmhandmade.map -LD /link -incremental:no /PDB:handmade_%date:~-4,4%%date:~-10,2%%date:~-7,2%_%time:~3,2%%time:~3,2%%time:~6,2%.pdb /EXPORT:GameGetSoundSamples /EXPORT:GameGetUpdateRenderer
cl %CommonCompilerFlags% ..\code\win32_handmade.cpp -Fmwin32_handmade.map /link %CommonLinkerFlags% 
popd