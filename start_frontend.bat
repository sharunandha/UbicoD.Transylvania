@echo off
cd /d "%~dp0"
where npx >nul 2>nul
if %errorlevel% neq 0 (
  echo npx not found. Install Node.js first.
  pause
  exit /b 1
)
start "Frontend" cmd /k "npx --yes http-server -p 5500"
echo Frontend started at http://localhost:5500
