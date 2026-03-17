"""Text-to-Speech Module"""

import logging
import os
import subprocess
import tempfile
from typing import Optional

import httpx

logger = logging.getLogger(__name__)


class PiperHTTPClient:
    """Thin synchronous wrapper around a Piper TTS HTTP server.

    Sends a synthesis request to ``POST <url>/api/tts`` and returns raw WAV
    bytes.  Raises :class:`RuntimeError` on failure so the caller can fall
    back to a system TTS provider.

    Parameters
    ----------
    url:
        Base URL of the Piper server, e.g. ``http://localhost:5000``.
    voice:
        Piper voice identifier, e.g. ``"en_US-lessac-medium"``.
    timeout:
        Request timeout in seconds (default: ``15``).
    """

    def __init__(self, url: str = "http://localhost:5000",
                 voice: str = "en_US-lessac-medium",
                 timeout: int = 15):
        self.url = url.rstrip("/")
        self.voice = voice
        self.timeout = timeout

    def synthesize(self, text: str) -> bytes:
        """Return WAV bytes for *text*.

        Raises ``RuntimeError`` when the server is unreachable or returns an
        error so callers can implement fallback logic.
        """
        try:
            resp = httpx.post(
                f"{self.url}/api/tts",
                json={"text": text, "voice": self.voice},
                timeout=self.timeout,
            )
            resp.raise_for_status()
            if not resp.content:
                raise RuntimeError("Piper TTS returned empty audio")
            logger.debug("Piper TTS: synthesized %d bytes for %d chars", len(resp.content), len(text))
            return resp.content
        except (httpx.RequestError, httpx.HTTPStatusError) as exc:
            raise RuntimeError(f"Piper TTS server error: {exc}") from exc


class TextToSpeech:
    """Handles text-to-speech conversion.

    Provider selection via config['provider'] (or legacy config['api']):
      - 'piper'  — local Piper TTS HTTP server (default)
      - 'espeak' — local eSpeak-ng system command (fallback when Piper
                   is unreachable or not configured)
      - 'system' — OS-native TTS (macOS ``say``, Linux ``espeak``,
                   Windows ``pyttsx3``)

    When Piper is selected and the server is unreachable the class
    automatically falls back to the system TTS so the device remains
    functional without network access.
    """

    def __init__(self, config: dict, mic_manager=None):
        self.config = config
        self.mic = mic_manager
        self.volume = config.get('volume', 1.0)
        self.sample_rate = config.get('sample_rate', 22050)

        provider = config.get('provider', config.get('api', 'system')).lower()
        self._use_piper = 'piper' in provider

        if self._use_piper:
            piper_url = config.get('piper_url', 'http://localhost:5000')
            voice = config.get('voice', 'en_US-lessac-medium')
            timeout_s = config.get('timeout_ms', 15000) // 1000
            self._piper = PiperHTTPClient(url=piper_url, voice=voice, timeout=timeout_s)
        else:
            self._piper = None

    # ------------------------------------------------------------------
    # Public API
    # ------------------------------------------------------------------

    def speak(self, text: str, blocking: bool = True) -> Optional[str]:
        """Convert text to speech and play it.

        Attempts Piper TTS when configured; falls back to system TTS on
        failure (server unreachable, empty response, etc.).

        Args:
            text: Text to speak.
            blocking: Wait for playback to finish when True.

        Returns:
            Path to the temporary WAV file when ``blocking=False``, else None.
        """
        if not text:
            return None

        if self._piper:
            try:
                wav_bytes = self._piper.synthesize(text)
                return self._play_wav_bytes(wav_bytes, blocking)
            except RuntimeError as exc:
                logger.warning("Piper TTS failed (%s) — falling back to system TTS", exc)

        # System TTS fallback
        return self._speak_system(text, blocking)

    def speak_stream(self, text: str, callback=None):
        """Synthesise speech and stream it with optional chunk callbacks."""
        if not text:
            return

        if self._piper:
            try:
                wav_bytes = self._piper.synthesize(text)
                self._stream_wav_bytes(wav_bytes, callback)
                return
            except RuntimeError as exc:
                logger.warning("Piper TTS failed (%s) — falling back to system TTS", exc)

        self._speak_system_stream(text, callback)

    def speak_async(self, text: str):
        """Speak asynchronously without blocking."""
        import threading
        thread = threading.Thread(target=self.speak, args=(text, False))
        thread.daemon = True
        thread.start()

    # ------------------------------------------------------------------
    # Internal helpers
    # ------------------------------------------------------------------

    def _play_wav_bytes(self, wav_bytes: bytes, blocking: bool) -> Optional[str]:
        """Write *wav_bytes* to a temp file and play it."""
        try:
            with tempfile.NamedTemporaryFile(suffix=".wav", delete=False) as f:
                f.write(wav_bytes)
                wav_path = f.name

            if blocking:
                self.play_audio(wav_path)
                os.remove(wav_path)
                return None
            return wav_path
        except Exception as exc:
            logger.error("Error playing WAV bytes: %s", exc)
            return None

    def _stream_wav_bytes(self, wav_bytes: bytes, callback=None):
        """Write *wav_bytes* to a temp file and stream it."""
        try:
            with tempfile.NamedTemporaryFile(suffix=".wav", delete=False) as f:
                f.write(wav_bytes)
                wav_path = f.name
            self.stream_audio(wav_path, callback)
            if os.path.exists(wav_path):
                os.remove(wav_path)
        except Exception as exc:
            logger.error("Error streaming WAV bytes: %s", exc)

    def _speak_system(self, text: str, blocking: bool) -> Optional[str]:
        """Use the OS-native TTS engine."""
        try:
            with tempfile.NamedTemporaryFile(suffix=".wav", delete=False) as f:
                wav_path = f.name

            self._generate_system_tts(text, wav_path)

            if blocking:
                self.play_audio(wav_path)
                os.remove(wav_path)
                return None
            return wav_path
        except Exception as exc:
            logger.error("System TTS error: %s", exc)
            return None

    def _speak_system_stream(self, text: str, callback=None):
        """Use the OS-native TTS engine in streaming mode."""
        try:
            with tempfile.NamedTemporaryFile(suffix=".wav", delete=False) as f:
                wav_path = f.name
            self._generate_system_tts(text, wav_path)
            self.stream_audio(wav_path, callback)
            if os.path.exists(wav_path):
                os.remove(wav_path)
        except Exception as exc:
            logger.error("System TTS stream error: %s", exc)

    def _generate_system_tts(self, text: str, wav_path: str):
        """Generate a WAV file from *text* using the system TTS engine."""
        if os.name == 'nt':  # Windows
            import pyttsx3
            engine = pyttsx3.init()
            engine.setProperty('rate', 150)
            engine.save_to_file(text, wav_path)
            engine.runAndWait()
        elif os.uname().sysname == 'Darwin':  # macOS
            subprocess.run(["say", "-r", "150", text, "-o", wav_path], check=True)
        else:  # Linux
            subprocess.run(
                ["espeak", "-v", "en", "-s", "150", "-q", text, "-w", wav_path],
                check=True,
            )

    def play_audio(self, audio_path: str):
        """Play an audio file through the system audio device."""
        try:
            if os.name == 'nt':
                import winsound
                winsound.PlaySound(audio_path, winsound.SND_FILENAME)
            elif os.uname().sysname == 'Darwin':
                subprocess.run(["afplay", audio_path], check=True)
            else:
                subprocess.run(["aplay", "-q", audio_path], check=True)
        except Exception as exc:
            logger.error("Error playing audio: %s", exc)

    def stream_audio(self, audio_path: str, callback=None):
        """Play an audio file asynchronously and invoke *callback* when done."""
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
        except Exception as exc:
            logger.error("Error streaming audio: %s", exc)
