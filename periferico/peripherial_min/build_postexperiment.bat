@echo off
call "%~dp0build_experiment.bat" postexperiment
exit /b %ERRORLEVEL%
