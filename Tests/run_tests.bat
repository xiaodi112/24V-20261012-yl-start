@echo off
rem Host unit tests (Windows, requires gcc in PATH)
setlocal
set APP=..\Pressure_Regulating_Box\App&set MID=..\Pressure_Regulating_Box\Middle&set BSP=..\Pressure_Regulating_Box\BSP&set DRV=..\Pressure_Regulating_Box\Drivers
gcc -std=c99 -Wall -DUNIT_TEST_HOOK -I stubs -I %APP% -I %MID% -I %BSP% -I %DRV% test_main.c test_json.c test_sensor_buffer.c test_threshold.c test_valve.c stubs\stub_runtime.c %APP%\valve.c %MID%\json_builder.c %MID%\sensor_buffer.c %MID%\threshold.c -o tests.exe
if errorlevel 1 exit /b 1
tests.exe
exit /b %errorlevel%
