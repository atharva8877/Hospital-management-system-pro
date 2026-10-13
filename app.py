import os
import time
import io
import urllib.parse
from xml.sax.saxutils import escape
from gtts import gTTS
import pygame
import requests
import serial

# --- Configuration ---
WOKWI_PORT = os.getenv("WOKWI_PORT", "socket://127.0.0.1:4000")
BOT_TOKEN = os.getenv("TELEGRAM_BOT_TOKEN", "")

CAREGIVER_CHAT_ID = os.getenv("TELEGRAM_CAREGIVER_CHAT_ID", "")
DOCTOR_CHAT_ID = os.getenv("TELEGRAM_DOCTOR_CHAT_ID", "")

# Twilio configuration for outbound phone calls.
TWILIO_ACCOUNT_SID = os.getenv("TWILIO_ACCOUNT_SID", "")
TWILIO_AUTH_TOKEN = os.getenv("TWILIO_AUTH_TOKEN", "")
TWILIO_FROM_NUMBER = os.getenv("TWILIO_FROM_NUMBER", "")

CAREGIVER_PHONE_NUMBER = os.getenv("CAREGIVER_PHONE_NUMBER", "")
DOCTOR_PHONE_NUMBER = os.getenv("DOCTOR_PHONE_NUMBER", "")

PHONE_NUMBERS_BY_CHAT = {
    CAREGIVER_CHAT_ID: CAREGIVER_PHONE_NUMBER,
    DOCTOR_CHAT_ID: DOCTOR_PHONE_NUMBER,
}

pygame.mixer.init()

# --- System State Variables ---
waiting_for_reply = False
last_update_id = None

def validate_configuration():
    required_settings = {
        "TELEGRAM_BOT_TOKEN": BOT_TOKEN,
        "TELEGRAM_CAREGIVER_CHAT_ID": CAREGIVER_CHAT_ID,
        "TELEGRAM_DOCTOR_CHAT_ID": DOCTOR_CHAT_ID,
    }
    missing_settings = [name for name, value in required_settings.items() if not value]
    if missing_settings:
        raise RuntimeError(
            "Set the required environment variables: " + ", ".join(missing_settings)
        )

def init_telegram_offset():
    """Clears old Telegram messages."""
    global last_update_id
    url = f"https://api.telegram.org/bot{BOT_TOKEN}/getUpdates"
    try:
        res = requests.get(url, timeout=5).json()
        if res.get("ok") and len(res["result"]) > 0:
            last_update_id = res["result"][-1]["update_id"] + 1
    except Exception:
        pass

def check_telegram_replies():
    """Polls Telegram for text replies from either caregiver or doctor."""
    global last_update_id, waiting_for_reply
    
    url = f"https://api.telegram.org/bot{BOT_TOKEN}/getUpdates"
    params = {"timeout": 1}
    if last_update_id:
        params["offset"] = last_update_id

    try:
        response = requests.get(url, params=params, timeout=2).json()
        if response.get("ok"):
            for result in response["result"]:
                last_update_id = result["update_id"] + 1
                
                if "message" in result and "text" in result["message"]:
                    reply_text = result["message"]["text"]
                    sender = result["message"]["from"]["first_name"]
                    print(f"\n[{sender} Replied]: {reply_text}")
                    
                    # Convert text to audio and play it 3 times on PC
                    play_reply_audio(f"{sender} says: {reply_text}")
                    
                    # Unlock system
                    waiting_for_reply = False
                    print("[System] Alert resolved. Ready for new patient requests.\n")
    except requests.exceptions.RequestException:
        pass 

def play_reply_audio(text: str):
    """Plays the received text reply 3 times on the local system."""
    print("[Bedside Speaker] Announcing reply...")
    try:
        tts = gTTS(text=text, lang="en")
        temp_file = "reply.mp3"
        tts.save(temp_file)

        for i in range(1, 4):
            print(f" -> Playback {i} of 3")
            pygame.mixer.music.load(temp_file)
            pygame.mixer.music.play()
            while pygame.mixer.music.get_busy():
                time.sleep(0.1)
            pygame.mixer.music.unload()
            time.sleep(1) 
            
        if os.path.exists(temp_file):
            os.remove(temp_file)
    except Exception as e:
        print(f"[Audio Error]: {e}")

def play_local_bedside_speaker(tts_text: str):
    """Plays the initial outgoing alert confirmation on PC."""
    try:
        tts = gTTS(text=tts_text, lang="en")
        temp_file = "bedside_alert.mp3"
        tts.save(temp_file)
        pygame.mixer.music.load(temp_file)
        pygame.mixer.music.play()
        while pygame.mixer.music.get_busy():
            time.sleep(0.1)
        pygame.mixer.music.unload()
        if os.path.exists(temp_file):
            os.remove(temp_file)
    except Exception:
        pass

def send_text_alert(chat_id: str, text: str):
    url = f"https://api.telegram.org/bot{BOT_TOKEN}/sendMessage"
    requests.post(url, json={"chat_id": chat_id, "text": text}, timeout=5)

def send_audio_alert(chat_id: str, text: str):
    try:
        tts = gTTS(text=text, lang="en")
        filepath = "telegram_alert.mp3"
        tts.save(filepath)
        url = f"https://api.telegram.org/bot{BOT_TOKEN}/sendAudio"
        with open(filepath, "rb") as audio_file:
            requests.post(url, data={"chat_id": chat_id, "caption": "🔊 Voice Alert"}, files={"audio": audio_file}, timeout=10)
        if os.path.exists(filepath):
            os.remove(filepath)
    except Exception:
        pass

def send_voice_call(phone_number: str, text: str):
    """Place a call and speak the same alert text using a trial-safe URL webhook."""
    if not all((TWILIO_ACCOUNT_SID, TWILIO_AUTH_TOKEN, TWILIO_FROM_NUMBER, phone_number)) or TWILIO_ACCOUNT_SID == "YOUR_TWILIO_SID_HERE":
        print("[Call] Skipped: Twilio credentials or recipient phone number is missing.")
        return

    url = f"https://api.twilio.com/2010-04-01/Accounts/{TWILIO_ACCOUNT_SID}/Calls.json"
    
    # Generate a standard TwiML payload (no premium voices) and encode it into a Twimlet URL
    twiml_payload = f"<Response><Say>{text}</Say></Response>"
    safe_url = f"http://twimlets.com/echo?Twiml={urllib.parse.quote(twiml_payload)}"

    try:
        response = requests.post(
            url,
            auth=(TWILIO_ACCOUNT_SID, TWILIO_AUTH_TOKEN),
            data={"To": phone_number, "From": TWILIO_FROM_NUMBER, "Url": safe_url},
            timeout=10,
        )
        response.raise_for_status()
        print(f"[Call] Started for {phone_number}.")
    except requests.RequestException as error:
        print(f"[Call Error] Could not call {phone_number}: {error}")

def handle_triage(event_raw: str):
    global waiting_for_reply
    print(f"\n[Processing Event] {event_raw}")

    # Determine message content and target audience based on button pressed
    target_chats = [CAREGIVER_CHAT_ID] 

    if "EMERGENCY" in event_raw:
        text_msg = "🚨 [CRITICAL ALERT] Patient Bed 1 triggered EMERGENCY!"
        voice_msg = "Emergency alert! Patient in Bed 1 requires immediate assistance."
        target_chats.append(DOCTOR_CHAT_ID) # Add doctor to routing
    elif "WATER" in event_raw:
        text_msg = "💧 [LOW PRIORITY] Patient Bed 1 requested Water."
        voice_msg = "Patient in Bed 1 has requested a glass of water."
    elif "FOOD" in event_raw:
        text_msg = "🍲 [NORMAL PRIORITY] Patient Bed 1 requested Food."
        voice_msg = "Patient in Bed 1 is requesting food."
    elif "WASHROOM" in event_raw:
        text_msg = "🚻 [ASSISTANCE] Patient Bed 1 requested Washroom help."
        voice_msg = "Patient in Bed 1 needs assistance to the washroom."
    else:
        text_msg = f"ℹ️ Notification: {event_raw}"
        voice_msg = f"New notification: {event_raw}"

    waiting_for_reply = True
    print("[System] Alert locked. Dispatching 5 alerts to designated personnel...")

    play_local_bedside_speaker(voice_msg)

    # Spam the alert 5 times to the designated targets
    for i in range(5):
        print(f"Dispatching alert wave {i+1}/5...")
        for chat_id in target_chats:
            send_text_alert(chat_id, text_msg)
            send_audio_alert(chat_id, voice_msg)
        time.sleep(2) # Wait 2 seconds between blasts to avoid Telegram rate limits

    print("[System] Five alert waves complete. Calling designated personnel...")
    for chat_id in target_chats:
        phone_number = PHONE_NUMBERS_BY_CHAT.get(chat_id, "")
        send_voice_call(phone_number, voice_msg)

    print("[System] All alerts sent. Waiting for reply...")

if __name__ == "__main__":
    validate_configuration()
    init_telegram_offset()
    print("Connecting to Wokwi Simulator bridge on port 4000...")
    try:
        ser = serial.serial_for_url(WOKWI_PORT, baudrate=115200, timeout=0.1)
        print("Connected! Listening for patient bed signals...\n")

        while True:
            check_telegram_replies()

            if ser.in_waiting > 0:
                line = ser.readline().decode("utf-8", errors="ignore").strip()
                if line and "ALERT:" in line:
                    if not waiting_for_reply:
                        handle_triage(line)
                        
            time.sleep(0.5)

    except serial.SerialException:
        print("Could not connect to Wokwi. Ensure the simulator is running first!")