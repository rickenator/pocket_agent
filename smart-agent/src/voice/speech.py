"""Speech Recognition Module"""

import logging
import SpeechRecognition as sr
import io
import httpx
from typing import Optional, Callable, List
from .mic import MicrophoneManager

logger = logging.getLogger(__name__)

# Default wake-word list (case-insensitive)
DEFAULT_WAKE_WORDS = ["hey buddy", "hey agent", "ok agent"]


class WakeWordDetector:
    """Detects a wake-word in a transcript before forwarding to STT.

    Operates as a lightweight keyword spotter: it checks whether any of the
    configured phrases appear in the recognised text.  No on-device ML model
    is required — recognition is performed by the same STT back-end used for
    normal queries.

    Usage::

        detector = WakeWordDetector(wake_words=["hey buddy", "ok agent"])
        text = detector.strip_wake_word("Hey Buddy, what's the weather?")
        # → "what's the weather?"
        detector.is_wake_word("hey buddy")  # → True
    """

    def __init__(self, wake_words: List[str] = None):
        self.wake_words: List[str] = [w.lower() for w in (wake_words or DEFAULT_WAKE_WORDS)]

    def detected(self, text: str) -> bool:
        """Return True if *text* starts with any wake-word."""
        if not text:
            return False
        lower = text.strip().lower()
        return any(lower.startswith(w) for w in self.wake_words)

    def is_wake_word(self, text: str) -> bool:
        """Return True if *text* is exactly a wake-word (no payload)."""
        if not text:
            return False
        lower = text.strip().lower()
        return any(lower == w for w in self.wake_words)

    def strip_wake_word(self, text: str) -> str:
        """Remove the leading wake-word from *text* and return the remainder."""
        lower = text.strip().lower()
        for w in self.wake_words:
            if lower.startswith(w):
                remainder = text.strip()[len(w):].lstrip(" ,")
                return remainder
        return text


class WhisperHTTPClient:
    """Thin synchronous wrapper around a faster-whisper-server HTTP endpoint.

    Sends a WAV audio buffer to ``POST <url>/inference`` and returns the
    transcript string.  Raises :class:`RuntimeError` on failure so the caller
    can fall back to an alternative provider.

    Parameters
    ----------
    url:
        Base URL of the Whisper server, e.g. ``http://localhost:9000``.
    language:
        BCP-47 language tag (default: ``"en"``).
    timeout:
        Request timeout in seconds (default: ``10``).
    """

    def __init__(self, url: str = "http://localhost:9000",
                 language: str = "en", timeout: int = 10):
        self.url = url.rstrip("/")
        self.language = language
        self.timeout = timeout

    def transcribe(self, wav_bytes: bytes) -> str:
        """POST *wav_bytes* to the Whisper server and return the transcript.

        Raises ``RuntimeError`` when the server is unreachable or returns an
        error so callers can implement fallback logic.
        """
        try:
            files = {"file": ("audio.wav", io.BytesIO(wav_bytes), "audio/wav")}
            data = {"language": self.language}
            resp = httpx.post(
                f"{self.url}/inference",
                files=files,
                data=data,
                timeout=self.timeout,
            )
            resp.raise_for_status()
            payload = resp.json()
            text = payload.get("text", "").strip()
            if not text:
                raise RuntimeError("Whisper returned an empty transcript")
            logger.info("Whisper transcript: %s", text)
            return text
        except (httpx.RequestError, httpx.HTTPStatusError) as exc:
            raise RuntimeError(f"Whisper server error: {exc}") from exc


class SpeechRecognizer:
    """Handles speech-to-text conversion.

    Supports two providers selected via config['provider']:
      - 'whisper'  — local faster-whisper-server (HTTP)
      - 'google'   — Google Cloud Speech Recognition (fallback when Whisper
                     is unreachable)

    The ``api`` key is also accepted for backwards compatibility.

    When ``wake_words`` are configured the recogniser will silently drop
    utterances that do not begin with a wake-word.
    """

    def __init__(self, config: dict, mic_manager: MicrophoneManager):
        self.config = config
        self.mic = mic_manager
        self.recognizer = sr.Recognizer()
        self.energy_threshold = config.get('energy_threshold', 1000)
        self.silence_timeout = config.get('silence_timeout', 3)

        # Resolve provider (new key 'provider' supersedes old 'api')
        provider = config.get('provider', config.get('api', 'google')).lower()
        self._use_whisper = 'whisper' in provider

        if self._use_whisper:
            whisper_url = config.get('whisper_url', 'http://localhost:9000')
            language = config.get('language', 'en').split('-')[0]  # e.g. "en-US" → "en"
            self._whisper = WhisperHTTPClient(
                url=whisper_url,
                language=language,
                timeout=config.get('timeout_ms', 10000) // 1000,
            )
        else:
            self._whisper = None

        # Optional wake-word detection
        wake_words = config.get('wake_words')
        self._wake_detector = WakeWordDetector(wake_words) if wake_words else None
        self.language = config.get('language', 'en-US')

    def _transcribe_audio(self, audio: sr.AudioData) -> Optional[str]:
        """Transcribe *audio* using Whisper (with Google fallback) or Google."""
        if self._use_whisper and self._whisper:
            try:
                wav_bytes = audio.get_wav_data()
                return self._whisper.transcribe(wav_bytes)
            except RuntimeError as exc:
                logger.warning("Whisper unavailable (%s) — falling back to Google STT", exc)
                # Fall through to Google STT
        # Google STT (primary or fallback)
        try:
            return self.recognizer.recognize_google(audio, language=self.language)
        except sr.UnknownValueError:
            logger.debug("Google STT could not understand audio")
            return None
        except sr.RequestError as exc:
            logger.error("Google STT API error: %s", exc)
            return None

    def listen(self, timeout: int = None) -> Optional[str]:
        """Listen for speech and return text.

        When wake-word detection is enabled the method returns ``None`` for
        utterances that do not begin with a configured wake-word.  The wake-word
        prefix is stripped from the returned text.

        Args:
            timeout: Maximum seconds to listen (None for infinite)

        Returns:
            Transcribed text (with wake-word stripped) or None if no speech
            detected or wake-word not present.
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
            text = self._transcribe_audio(audio)
            if not text:
                return None

            logger.info("Recognized: %s", text)

            # Wake-word filtering
            if self._wake_detector:
                if not self._wake_detector.detected(text):
                    logger.debug("Wake-word not detected — ignoring utterance")
                    return None
                text = self._wake_detector.strip_wake_word(text)
                if not text:
                    logger.debug("Wake-word only (no payload) — ignoring")
                    return None

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
                        text = self.recognizer.recognize_google(audio, language=self.language)
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
