@echo off
SETLOCAL ENABLEDELAYEDEXPANSION

REM ---- Configura il firmware e la modalità OTA ----
SET "FILE=.pio/build/NAVIGATIONUNIT/firmware.bin"

REM ---- Carica il firmware usando ElegantOTA ----
ECHO Inizio upload del firmware %FILE% su %ESP_IP%...

curl -# -F "file=@%FILE%" "http://192.168.4.1/ota/upload" --fail

IF %ERRORLEVEL% NEQ 0 (
    ECHO Errore: Upload fallito
    PAUSE
    EXIT /B 1
)

ECHO Firmware caricato con successo!
PAUSE
