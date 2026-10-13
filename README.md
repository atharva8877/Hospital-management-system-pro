# Hospital Management Patient Alert System

An Arduino Mega patient alert interface with a TFT display and physical buttons.
The Python bridge listens to the Wokwi serial connection and sends Telegram
alerts; Twilio phone calls can be enabled optionally.

## Hardware and simulation

The PlatformIO project targets an Arduino Mega 2560. Open the project in
PlatformIO to build the firmware. The Wokwi circuit is described in
`diagram.json`; the simulator serial bridge defaults to
`socket://127.0.0.1:4000`.

## Python bridge setup

Install the bridge dependencies:

```powershell
python -m pip install -r requirements.txt
```

Set the required Telegram configuration in PowerShell before running `app.py`:

```powershell
$env:TELEGRAM_BOT_TOKEN = "your-bot-token"
$env:TELEGRAM_CAREGIVER_CHAT_ID = "caregiver-chat-id"
$env:TELEGRAM_DOCTOR_CHAT_ID = "doctor-chat-id"
python app.py
```

Outbound Twilio calls are optional. Set `TWILIO_ACCOUNT_SID`,
`TWILIO_AUTH_TOKEN`, `TWILIO_FROM_NUMBER`, `CAREGIVER_PHONE_NUMBER`, and
`DOCTOR_PHONE_NUMBER` in the environment to enable them. Set `WOKWI_PORT` to
override the default serial bridge address.

Do not commit credentials, chat identifiers, or phone numbers. Credentials
previously placed in source should be revoked and replaced before use.
