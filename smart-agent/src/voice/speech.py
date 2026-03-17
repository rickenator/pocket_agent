"""Speech Recognition Module"""

import logging
import SpeechRecognition as sr
import os
from typing import Optional, Callable, List
from .mic import MicrophoneManager

logger = logging.getLogger(__name__)


class SpeechRecognizer:
    """Handles speech-to-text conversion.

    Supports two providers selected via config['api']:
      - 'google'  (default) — Google Cloud Speech Recognition
      - 'openai'            — OpenAI Whisper (requires openai package)
    """

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
                    with open("temp_audio.wav", "rb") as af:
                        client = openai.OpenAI()
                        response = client.audio.transcriptions.create(
                            model="whisper-1",
                            file=af,
                            language=self.config.get('language', 'en')
                        )
                    text = response.text
                    os.remove("temp_audio.wav")
                except ImportError:
                    # Fallback to Google
                    text = self.recognizer.recognize_google(audio, language=self.config.get('language', 'en-US'))

            logger.info("Recognized: %s", text)
            return text

        except sr.WaitTimeoutError:
            logger.debug("No speech detected")
            return None
        except sr.UnknownValueError:
            logger.debug("Could not understand audio")
            return None
        except sr.RequestError as e:
            logger.error("STT API error: %s", e)
            return None
        except Exception as e:
            logger.error("Error during speech recognition: %s", e)
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
                    except (sr.UnknownValueError, sr.RequestError, Exception):
                        break

            audio.stop()
            return None

        except Exception as e:
            logger.error("Error in continuous listening: %s", e)
            return None

    def save_wav(self, audio_data: bytes, filename: str = "output.wav"):
        """Save audio data to WAV file"""
        with open(filename, "wb") as f:
            f.write(audio_data)

    def load_wav(self, filename: str = "input.wav") -> bytes:
        """Load WAV file"""
        with open(filename, "rb") as f:
            return f.read()


class MockSpeechRecognizer:
    """Mock speech recognizer for unit tests.

    Replays a pre-configured sequence of transcripts without requiring a
    microphone, audio hardware, or network connection.  Useful for testing
    the STT → LLM → TTS pipeline in CI or on machines without audio devices.

    Usage::

        recognizer = MockSpeechRecognizer(
            responses=["What time is it?", "Tell me a joke", None]
        )
        text = recognizer.listen()   # → "What time is it?"
        text = recognizer.listen()   # → "Tell me a joke"
        text = recognizer.listen()   # → None  (simulates silence / no speech)

    The recognizer loops back to the first response once the list is exhausted
    if ``loop=True`` (the default), or returns ``None`` indefinitely.
    """

    def __init__(self, responses: List[Optional[str]] = None, loop: bool = True):
        self.responses = responses or ["Hello, this is a mock response."]
        self.loop = loop
        self._index = 0

    def listen(self, timeout: int = None) -> Optional[str]:  # noqa: ARG002
        """Return the next pre-configured transcript."""
        if self._index >= len(self.responses):
            if self.loop:
                self._index = 0
            else:
                return None
        text = self.responses[self._index]
        self._index += 1
        if text is not None:
            logger.info("[MockSTT] Returning: %s", text)
        else:
            logger.debug("[MockSTT] Returning None (simulated silence)")
        return text

    def listen_continuous(self, callback: Callable[[str], None]) -> None:
        """Replay all non-None responses, then stop."""
        while True:
            text = self.listen()
            if text is None:
                break
            callback(text)

    def listen_with_stop(self, stop_event) -> Optional[str]:
        """Return the next transcript, or None if stop_event is set."""
        if stop_event.is_set():
            return None
        return self.listen()

    def reset(self):
        """Reset the replay index to the beginning."""
        self._index = 0
