@echo off
setlocal enabledelayedexpansion

cd /d "%~dp0"

git config --replace-all user.email "shawn.1963@yahoo.com"
git config --replace-all user.name "shawnmkennedy1"

for %%I in ("%cd%") do set "PROJECT_NAME=%%~nxI"
set "INI_FILE=%PROJECT_NAME%.ini"
set "AUTHOR=Shawn M. Kennedy"

echo --------------------------------------------------
echo UNIVERSAL BACKUP: %PROJECT_NAME%
echo --------------------------------------------------

set /p CHANGE_MSG="What did you change today? "

echo [Project_Summary] > "%INI_FILE%"
echo Name=%PROJECT_NAME% >> "%INI_FILE%"
echo Author=%AUTHOR% >> "%INI_FILE%"
echo Last_Backup=%date% %time% >> "%INI_FILE%"
echo Last_Change=%CHANGE_MSG% >> "%INI_FILE%"
echo. >> "%INI_FILE%"
echo [Project_Files] >> "%INI_FILE%"

set "file_count=0"
for /r %%f in (*) do (
    set "FILE_NAME=%%~nxf"
    echo %%f | find /i ".git" >nul || (
        if not "!FILE_NAME!"=="%INI_FILE%" (
            if not "!FILE_NAME!"=="%~nx0" (
                set /a file_count+=1
                echo File_!file_count!=!FILE_NAME! >> "%INI_FILE%"
            )
        )
    )
)
echo Total_Files=!file_count! >> "%INI_FILE%"

echo.
if not exist ".git" (
    git init
    git remote add origin https://github.com/shawnmkennedy1/%PROJECT_NAME%.git
) else (
    git remote set-url origin https://github.com/shawnmkennedy1/%PROJECT_NAME%.git
)

git branch -M main
git add . 
git commit -m "Backup: %PROJECT_NAME% - %CHANGE_MSG% (%date%)"
git push -u origin main --force

echo --------------------------------------------------
echo DONE! Created %INI_FILE% and synced to GitHub.
pause