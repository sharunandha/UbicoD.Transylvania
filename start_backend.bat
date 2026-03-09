@echo off
cd /d "%~dp0backend"
if not exist package.json (
  echo backend\package.json not found.
  pause
  exit /b 1
)
if not exist node_modules (
  echo Installing backend dependencies...
  npm install
)
start "Backend" cmd /k "npm start"
echo Backend started at http://localhost:8080
