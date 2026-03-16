"""Speech Recognition Module"""

import SpeechRecognition as sr
import os
from typing import Optional, Callable
from .mic import MicrophoneManager


class SpeechRecognizer:
    """Handles speech-to-text conversion"""

    def __init__(self, config: dict, mic_manager: MicrophoneManager):
        self.config = config
        self.mic = mic_manager
        self.recognizer = sr.Recognizer()
        self.energy_threshold = config.get('energy_threshold', 1000)
        self.silence_timeout = config.get('silence_timeout', 3)

    def listen(self, timeout: int = None) -> Optional[str]:
        """Listen for speech and return text

        Args:
            timeout: Maximum seconds to listen (None for infinite)

        Returns:
            Transcribed text or None if no speech detected
        """
        # Calibrate if needed
        if not self.energy_threshold or self.energy_threshold < 500:
            self.mic.calibrate()

        try:
            # Convert PyAudio stream to SoundFile stream
            sr_audio = sr.AudioData(
                self.mic.read(),
                sample_rate=self.mic.sample_rate,
                width=2
            )

            # Set up recognizer
            self.recognizer.energy_threshold = self.energy_threshold
            self.recognizer.pause_threshold = self.silence_timeout
            self.recognizer.phrase_threshold = 0.5

            # Listen with timeout
            audio = self.recognizer.listen(sr_audio, timeout=timeout)

            # Transcribe
            if 'google' in self.config.get('api', 'openai').lower():
                text = self.recognizer.recognize_google(audio, language=self.config.get('language', 'en-US'))
            else:
                # Try OpenAI Whisper if available
                try:
                    import openai
                    with open("temp_audio.wav", "wb") as f:
                        f.write(audio.get_wav_data())
                    response = openai.Audio.transcribe(
                        model="whisper-1",
                        file=open("temp_audio.wav", "rb"),
                        language=self.config.get('language', 'en')
                    )
                    text = response.text
                    os.remove("temp_audio.wav")
                except ImportError:
                    # Fallback to Google
                    text = self.recognizer.recognize_google(audio, language=self.config.get('language', 'en-US'))

            print(f"Recognized: {text}")
            return text

        except sr.WaitTimeoutError:
            print("No speech detected")
            return None
        except sr.UnknownValueError:
            print("Could not understand audio")
            return None
        except sr.RequestError as e:
            print(f"API error: {e}")
            return None
        except Exception as e:
            print(f"Error during speech recognition: {e}")
            return None

    def listen_continuous(self, callback: Callable[[str], None]) -> None:
        """Continuously listen for speech

        Args:
            callback: Function to call with recognized text
        """
        while True:
            text = self.listen()
            if text:
                callback(text)
            else:
                break

    def listen_with_stop(self, stop_event) -> Optional[str]:
        """Listen with stop signal

        Args:
            stop_event: threading.Event to signal stop

        Returns:
            Transcribed text or None
        """
        try:
            audio = self.recognizer.listen_in_background(self.mic.stream, phrase_time_limit=self.silence_timeout)

            while not stop_event.is_set():
                if self.recognizer.non_speaking_duration > 0.5:
                    # Finished speaking
                    try:
                        audio.get_results()
                        text = self.recognizer.recognize_google(audio, language=self.config.get('language', 'en-US'))
                        return text
                    except:
                        break

            audio.stop()
            return None

        except Exception as e:
            print(f"Error in continuous listening: {e}")
            return None

    def save_wav(self, audio_data: bytes, filename: str = "output.wav"):
        """Save audio data to WAV file"""
        with open(filename, "wb") as f:
            f.write(audio_data)

    def load_wav(self, filename: str = "input.wav") -> bytes:
        """Load WAV file"""
        with open(filename, "rb") as f:
            return f.read()