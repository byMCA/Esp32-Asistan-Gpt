from flask import Flask, request, jsonify, send_from_directory, send_file
import numpy as np
import soundfile as sf
import os
import logging
from datetime import datetime
import openai
from werkzeug.utils import secure_filename
import speech_recognition as sr
from gtts import gTTS
from pydub import AudioSegment
import time
from threading import Timer
import tempfile
import sys
import io

# === Config ===
CONFIG = {
    "TEMP_DIR": "temp_chunks",
    "SAMPLE_RATE": 16000,
    "VOLUME_BOOST": 4.0,
    "MAX_SAMPLE_VALUE": 32767.0,
    "LANGUAGE": "tr-TR",
    "OPENAI_MODEL": "gpt-4o-mini",
    "OPENAI_API_KEY": "Senin Api ",
    "TTS_DIR": "tts_cache",
    "AUDIO_FORMAT": "wav",
    "BITRATE": "16k",
    "HOST": "0.0.0.0",
    "PORT": 5000,
    "DELETE_AFTER_SERVE": True,
    "MAX_FILE_AGE": 3600,
    "WHISPER_MODEL": "whisper-1"
}

# Set UTF-8 encoding for stdout
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')

openai.api_key = CONFIG["OPENAI_API_KEY"]

# === Setup ===
app = Flask(__name__)
os.makedirs(CONFIG["TEMP_DIR"], exist_ok=True)
os.makedirs(CONFIG["TTS_DIR"], exist_ok=True)

# === Logging Configuration ===
class UnicodeStreamHandler(logging.StreamHandler):
    def emit(self, record):
        try:
            msg = self.format(record)
            stream = self.stream
            stream.write(msg + self.terminator)
            self.flush()
        except UnicodeEncodeError:
            msg = self.format(record).encode('ascii', 'replace').decode('ascii')
            stream.write(msg + self.terminator)
            self.flush()
        except Exception:
            self.handleError(record)

logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s',
    handlers=[
        logging.FileHandler("server.log", encoding='utf-8'),
        UnicodeStreamHandler()
    ]
)
logger = logging.getLogger(__name__)

# === Utility Functions ===

def clean_temp_folder():
    """Remove all files from TEMP_DIR safely."""
    for f in os.listdir(CONFIG["TEMP_DIR"]):
        try:
            os.remove(os.path.join(CONFIG["TEMP_DIR"], f))
        except Exception as e:
            logger.warning(f"Failed to remove temp file {f}: {e}")

def clean_tts_folder():
    """Remove all files from TTS_DIR safely."""
    for f in os.listdir(CONFIG["TTS_DIR"]):
        try:
            os.remove(os.path.join(CONFIG["TTS_DIR"], f))
        except Exception as e:
            logger.warning(f"Failed to remove TTS file {f}: {e}")

def boost_volume(audio: np.ndarray) -> np.ndarray:
    """Boost audio volume and clip samples safely."""
    boosted = audio * CONFIG["VOLUME_BOOST"]
    return np.clip(boosted, -CONFIG["MAX_SAMPLE_VALUE"], CONFIG["MAX_SAMPLE_VALUE"]).astype(np.int16)

def transcribe_with_whisper(audio_path: str) -> str:
    """Convert WAV file to text using OpenAI Whisper."""
    try:
        with open(audio_path, "rb") as audio_file:
            result = openai.Audio.transcribe(
                model=CONFIG["WHISPER_MODEL"],
                file=audio_file,
                language="tr"
            )
        return result["text"]
    except Exception as e:
        logger.error(f"Whisper transcription error: {e}")
        return ""

def speed_change(sound, speed=1.0):
    """Change the playback speed of an AudioSegment."""
    # Changing frame_rate speeds up/down the audio
    altered = sound._spawn(sound.raw_data, overrides={
        "frame_rate": int(sound.frame_rate * speed)
    })
    return altered.set_frame_rate(sound.frame_rate)

def text_to_speech(text: str, filename: str) -> str | None:
    """Convert text to speech, save as WAV, return saved filepath or None."""
    try:
        # First save as MP3 (gTTS only outputs MP3)
        temp_mp3_path = os.path.join(CONFIG["TTS_DIR"], f"temp_{filename}.mp3")
        tts = gTTS(text=text, lang='tr')
        tts.save(temp_mp3_path)
        
        # Convert MP3 to WAV with proper format for ESP32
        final_path = os.path.join(CONFIG["TTS_DIR"], f"{filename}.wav")
        audio = AudioSegment.from_mp3(temp_mp3_path)
        audio = audio.normalize(headroom=1.0) + 2
        pitched_audio = speed_change(audio, speed=1.25)


        pitched_audio.export(final_path,
    format="wav",
    bitrate=CONFIG["BITRATE"],
    parameters=[
        "-ar", "16000",  # Örnekleme hızı: 16kHz
        "-ac", "1",      # Mono ses
        "-acodec", "pcm_s16le"  # 16-bit PCM (ESP32 için uygun)
    ])
        
        # Remove temp MP3 file
        os.remove(temp_mp3_path)
        return final_path
    except Exception as e:
        logger.error(f"TTS error: {e}")
        return None

import time
import os
from threading import Timer
import logging

logger = logging.getLogger(__name__)

# Global değişken olarak konuşma geçmişi (tek kullanıcı örneği)
chat_history = [
    {
        "role": "system",
        "content": """
SENIN PROMPTUN!!!!!!!!!!!!!!!!!!!!!!
"""
    }
]

def ask_chatgpt(prompt: str) -> str:
    """OpenAI ChatGPT ile konuşma geçmişini tutarak sohbet et."""
    global chat_history
    # Kullanıcı mesajını geçmişe ekle
    chat_history.append({"role": "user", "content": prompt})

    try:
        response = openai.ChatCompletion.create(
            model=CONFIG["OPENAI_MODEL"], 
            messages=chat_history,
            temperature=0.85,
            top_p=0.95,
            max_tokens=250,
            frequency_penalty=0.3,
            presence_penalty=0.4
        )
        assistant_reply = response.choices[0].message.content.strip()

        # Asistan cevabını da geçmişe ekle
        chat_history.append({"role": "assistant", "content": assistant_reply})

        return assistant_reply
    except Exception as e:
        logger.error(f"ChatGPT error: {e}")
        return f"[ChatGPT error: {e}]"


def cleanup_old_files():
    """TTS dizinindeki eski dosyaları sil."""
    now = time.time()
    for f in os.listdir(CONFIG["TTS_DIR"]):
        filepath = os.path.join(CONFIG["TTS_DIR"], f)
        if os.path.isfile(filepath):
            file_age = now - os.path.getmtime(filepath)
            if file_age > CONFIG["MAX_FILE_AGE"]:
                try:
                    os.remove(filepath)
                    logger.info(f"Deleted old file: {f}")
                except Exception as e:
                    logger.warning(f"Failed to delete old file {f}: {e}")

    # Fonksiyonu periyodik olarak tekrar çağır
    Timer(CONFIG["MAX_FILE_AGE"], cleanup_old_files).start()

# Temizlemeyi başlat
cleanup_old_files()

# === Flask Routes ===

@app.route("/chat", methods=["POST"])
def receive_chunk():
    """Receive a raw audio chunk as binary data and save it securely."""
    if not request.data:
        logger.warning("No data received in /chat")
        return jsonify({"error": "No data received"}), 400

    timestamp = datetime.utcnow().strftime('%Y%m%d%H%M%S%f')
    filename = secure_filename(f"chunk_{timestamp}.raw")
    filepath = os.path.join(CONFIG["TEMP_DIR"], filename)

    try:
        with open(filepath, "wb") as f:
            f.write(request.data)
        logger.info(f"Received audio chunk: {filename}")
        return jsonify({"message": "Chunk received", "filename": filename}), 200
    except Exception as e:
        logger.error(f"Error saving chunk: {e}")
        return jsonify({"error": f"Failed to save audio chunk: {e}"}), 500

@app.route("/chat/end", methods=["POST"])
def process_audio():
    """Combine received audio chunks, convert to text, query ChatGPT, generate TTS and return info."""
    try:
        chunk_files = sorted(f for f in os.listdir(CONFIG["TEMP_DIR"]) if f.endswith(".raw"))
        if not chunk_files:
            logger.warning("No audio chunks found in temp folder.")
            return jsonify({"error": "No audio data found"}), 400

        # Combine all chunks into one numpy array
        combined_audio = np.concatenate([
            np.frombuffer(open(os.path.join(CONFIG["TEMP_DIR"], f), "rb").read(), dtype=np.int16)
            for f in chunk_files
        ])

        # Boost volume and save as WAV
        boosted_audio = boost_volume(combined_audio)
        temp_wav_path = os.path.join(CONFIG["TEMP_DIR"], "combined.wav")
        sf.write(temp_wav_path, boosted_audio, CONFIG["SAMPLE_RATE"], subtype="PCM_16")

        # Convert speech to text using Whisper
        recognized_text = transcribe_with_whisper(temp_wav_path)
        os.remove(temp_wav_path)
        clean_temp_folder()

        if not recognized_text:
            logger.info("Speech recognition returned empty result.")
            return jsonify({
                "text": "",
                "response": "[Speech not understood]",
                "audio_url": ""
            }), 200

        logger.info(f"Recognized text: {recognized_text}")

        # Get ChatGPT response
        chat_response = ask_chatgpt(recognized_text)
        logger.info(f"ChatGPT response: {chat_response}")

        # Generate TTS file (WAV format)
        tts_filename = f"response_{datetime.utcnow().strftime('%Y%m%d%H%M%S')}"
        tts_filepath = text_to_speech(chat_response, tts_filename)

        audio_url = f"/response/{tts_filename}.wav" if tts_filepath else ""

        return jsonify({
            "text": recognized_text,
            "response": chat_response,
            "audio_url": audio_url
        }), 200

    except Exception as e:
        logger.error(f"Error processing audio: {e}")
        clean_temp_folder()
        return jsonify({"error": f"Internal server error: {e}"}), 500

@app.route("/response", methods=["GET"])
@app.route("/response/", methods=["GET"])
@app.route("/response/<filename>", methods=["GET"])
def serve_audio(filename=None):
    """Serve WAV audio files from TTS_DIR safely."""
    if filename is None:
        # Get the most recent WAV file
        wav_files = [f for f in os.listdir(CONFIG["TTS_DIR"]) if f.endswith('.wav')]
        if not wav_files:
            return jsonify({"error": "No audio files available"}), 404
            
        # Sort by modification time (newest first)
        wav_files.sort(key=lambda x: os.path.getmtime(os.path.join(CONFIG["TTS_DIR"], x)), reverse=True)
        filename = wav_files[0]
    
    safe_name = secure_filename(filename)
    filepath = os.path.join(CONFIG["TTS_DIR"], safe_name)

    if not os.path.isfile(filepath):
        logger.error(f"Requested audio file not found: {safe_name}")
        return jsonify({"error": "File not found"}), 404

    logger.info(f"Serving audio file: {safe_name}")
    
    try:
        response = send_file(
            filepath,
            mimetype="audio/wav",
            as_attachment=False
        )
        
        if CONFIG["DELETE_AFTER_SERVE"]:
            @response.call_on_close
            def delete_file():
                try:
                    os.remove(filepath)
                    logger.info(f"Deleted served audio file: {safe_name}")
                except Exception as e:
                    logger.warning(f"Failed to delete audio file {safe_name}: {e}")
        
        return response
    except Exception as e:
        logger.error(f"Error serving audio file: {e}")
        return jsonify({"error": "Internal server error"}), 500

@app.route("/status", methods=["GET"])
def status():
    """Return server status including temp and TTS folder file counts."""
    temp_count = len(os.listdir(CONFIG["TEMP_DIR"]))
    tts_count = len(os.listdir(CONFIG["TTS_DIR"]))
    return jsonify({
        "status": "running",
        "temp_files_count": temp_count,
        "tts_files_count": tts_count
    })

# === Main Entry Point ===
if __name__ == "__main__":
    clean_temp_folder()
    clean_tts_folder()
    Timer(CONFIG["MAX_FILE_AGE"], cleanup_old_files).start()
    logger.info("Starting audio processing server...")
    app.run(host=CONFIG["HOST"], port=CONFIG["PORT"], threaded=True) 