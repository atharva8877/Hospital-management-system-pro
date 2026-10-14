# Hospital Management Patient Alert System

An ESP32 patient alert interface with a TFT display and physical buttons.
The Python bridge listens to the Wokwi serial connection and sends Telegram
alerts; Twilio phone calls can be enabled optionally.

## Hardware and simulation

The PlatformIO project targets an ESP32 DevKit V1 (`esp32dev`). Open the project in
PlatformIO to build the firmware. The Wokwi circuit is described in
`diagram.json`; the simulator serial bridge defaults to
`rfc2217://127.0.0.1:4000`.

The ESP32 firmware handles the display and buttons and sends `ALERT:` messages
over serial. `app.py` remains a PC-side service for Telegram, Twilio, and audio
playback; this change does not move those services onto the ESP32.

## Python bridge setup

Install the bridge dependencies:

```powershell
python -m pip install -r requirements.txt
```

Set the required Telegram configuration in PowerShell before running `app.py`
(see `Process.text` for the complete hardware and software build procedure):

```powershell
$env:TELEGRAM_BOT_TOKEN = "your-bot-token"
$env:TELEGRAM_CAREGIVER_CHAT_ID = "caregiver-chat-id"
$env:TELEGRAM_DOCTOR_CHAT_ID = "doctor-chat-id"
python app.py
```

For a physical ESP32 connected by USB, set `WOKWI_PORT` to its serial port,
for example:

```powershell
$env:WOKWI_PORT = "COM3"
python app.py
```

Outbound Twilio calls are optional. Set `TWILIO_ACCOUNT_SID`,
`TWILIO_AUTH_TOKEN`, `TWILIO_FROM_NUMBER`, `CAREGIVER_PHONE_NUMBER`, and
`DOCTOR_PHONE_NUMBER` in the environment to enable them. Set `WOKWI_PORT` to
override the default serial endpoint.

Do not commit credentials, chat identifiers, or phone numbers. Credentials
previously placed in source should be revoked and replaced before use.
