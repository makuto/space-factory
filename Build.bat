cd Dependencies\cakelisp
call Build.bat
@if %ERRORLEVEL% == 0 (
  echo Successfully built Cakelisp
  goto user
) else (
  echo Error while building cakelisp
  goto fail
)

:user
cd ../../
"Dependencies\cakelisp\bin\cakelisp.exe" --execute src/Config_Windows.cake src/SpaceFactory.cake
@if %ERRORLEVEL% == 0 (
  echo Success!
  goto success
) else (
  echo Error while building user program
  goto fail
)

:fail
goto end

:success
goto end

:end
echo Done
