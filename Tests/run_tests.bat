@echo off
rem Host 单元测试 (Windows, 需要 gcc 在 PATH 中)
setlocal
set APP=..\Pressure_Regulating_Box\App&set MID=..\Pressure_Regulating_Box\Middle&set BSP=..\Pressure_Regulating_Box\BSP&set DRV=..\Pressure_Regulating_Box\Drivers
gcc -std=c99 -Wall -I stubs -I %APP% -I %MID% -I %BSP% -I %DRV% test_main.c test_json.c test_sensor_buffer.c test_threshold.c test_valve.c stubs\stub_runtime.c %APP%\valve.c %MID%\json_builder.c %MID%\sensor_buffer.c %MID%\threshold.c -o tests.exe
if errorlevel 1 exit /b 1
tests.exe
exit /b %errorlevel%
