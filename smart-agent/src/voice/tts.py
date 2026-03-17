"""Text-to-Speech Module"""

import os
import tempfile
import subprocess
from typing import Optional
from .mic import MicrophoneManager


class TextToSpeech:
    """Handles text-to-speech conversion"""

    def __init__(self, config: dict, mic_manager: MicrophoneManager):
        self.config = config
        self.mic = mic_manager
        self.output_device = config.get('device', 'default')
        self.volume = config.get('volume', 1.0)
        self.sample_rate = config.get('sample_rate', 24000)

    def speak(self, text: str, blocking: bool = True) -> Optional[str]:
        """Convert text to speech

        Args:
            text: Text to speak
            blocking: Wait for speech to complete

        Returns:
            Audio file path if blocking=False
        """
        if not text:
            return None

        # Generate speech file using system TTS
        try:
            with tempfile.NamedTemporaryFile(suffix=".wav", delete=False) as f:
                wav_path = f.name

            # Use system TTS (macOS/Linux/Windows)
            if os.name == 'nt':  # Windows
                import pyttsx3
                engine = pyttsx3.init()
                engine.setProperty('rate', 150)
                engine.save_to_file(text, wav_path)
                engine.runAndWait()
            elif os.uname().sysname == 'Darwin':  # macOS
                subprocess.run(["say", "-r", "150", text, "-o", wav_path], check=True)
            else:  # Linux
                subprocess.run(["espeak", "-v", "en", "-s", "150", "-q", text, "-w", wav_path], check=True)

            # Play audio
            if blocking:
                self.play_audio(wav_path)
                os.remove(wav_path)
            else:
                return wav_path

            return None

        except Exception as e:
            print(f"Error in TTS: {e}")
            return None

    def speak_stream(self, text: str, callback=None):
        """Stream speech with chunk callbacks"""
        if not text:
            return

        try:
            with tempfile.NamedTemporaryFile(suffix=".wav", delete=False) as f:
                wav_path = f.name

            # Generate speech
            if os.name == 'nt':
                import pyttsx3
                engine = pyttsx3.init()
                engine.setProperty('rate', 150)
                engine.save_to_file(text, wav_path)
                engine.runAndWait()
            elif os.uname().sysname == 'Darwin':
                subprocess.run(["say", "-r", "150", text, "-o", wav_path], check=True)
            else:
                subprocess.run(["espeak", "-v", "en", "-s", "150", "-q", text, "-w", wav_path], check=True)

            # Stream audio
            self.stream_audio(wav_path, callback)

            if os.path.exists(wav_path):
                os.remove(wav_path)

        except Exception as e:
            print(f"Error streaming speech: {e}")

    def play_audio(self, audio_path: str):
        """Play audio file"""
        try:
            if os.name == 'nt':
                import winsound
                winsound.PlaySound(audio_path, winsound.SND_FILENAME)
            elif os.uname().sysname == 'Darwin':
                subprocess.run(["afplay", audio_path], check=True)
            else:
                subprocess.run(["aplay", "-q", audio_path], check=True)
        except Exception as e:
            print(f"Error playing audio: {e}")

    def stream_audio(self, audio_path: str, callback=None):
        """Stream audio with callbacks for each chunk"""
        try:
            if os.name == 'nt':
                import winsound
                winsound.PlaySound(audio_path, winsound.SND_FILENAME | winsound.SND_ASYNC)
            elif os.uname().sysname == 'Darwin':
                subprocess.Popen(["afplay", audio_path])
            else:
                subprocess.Popen(["aplay", "-q", audio_path])

            if callback:
                callback()

        except Exception as e:
            print(f"Error streaming audio: {e}")

    def speak_async(self, text: str):
        """Speak asynchronously without blocking"""
        import threading
        thread = threading.Thread(target=self.speak, args=(text, False))
        thread.start()