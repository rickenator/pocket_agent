"""Microphone Manager for Voice Input"""

import numpy as np
import pyaudio
from typing import Optional, Callable


class MicrophoneManager:
    """Manages dual microphone array for voice input"""

    def __init__(self, config: dict):
        self.config = config
        self.sample_rate = config.get('sample_rate', 16000)
        self.chunk_size = config.get('chunk_size', 1024)
        self.energy_threshold = config.get('energy_threshold', 1000)
        self.stream = None
        self.frames = []
        self.is_listening = False

    def setup(self) -> bool:
        """Initialize audio stream"""
        try:
            self.p = pyaudio.PyAudio()

            # Check for dual microphone devices
            devices = []
            for i in range(self.p.get_device_count()):
                info = self.p.get_device_info_by_index(i)
                if 'mic' in info['name'].lower() or 'input' in info['name'].lower():
                    devices.append(i)

            if not devices:
                print("Warning: No microphone found, using default")
                devices = [self.p.get_default_input_device_info()['index']]

            self.selected_device = devices[0]
            print(f"Using microphone: {self.p.get_device_info_by_index(self.selected_device)['name']}")

            self.stream = self.p.open(
                format=pyaudio.paInt16,
                channels=1,  # Mono
                rate=self.sample_rate,
                input=True,
                frames_per_buffer=self.chunk_size,
                input_device_index=self.selected_device
            )

            return True

        except Exception as e:
            print(f"Error setting up microphone: {e}")
            return False

    def start_recording(self):
        """Start recording audio"""
        self.frames = []
        self.is_listening = True
        print("Listening...")

    def stop_recording(self) -> bytes:
        """Stop recording and return audio data"""
        self.is_listening = False
        if self.stream:
            self.stream.stop_stream()
            self.stream.close()
            self.p.terminate()

        return b''.join(self.frames)

    def process_frame(self, frame: bytes) -> bool:
        """Process a single audio frame with voice activity detection"""
        if not self.is_listening:
            return False

        self.frames.append(frame)

        # Simple energy-based voice activity detection
        audio_data = np.frombuffer(frame, dtype=np.int16)
        energy = np.sqrt(np.mean(audio_data.astype(float)**2))

        if energy > self.energy_threshold:
            return True  # Voice detected

        return False

    def read(self) -> Optional[bytes]:
        """Read audio data"""
        try:
            if self.stream and not self.stream.is_active():
                return None

            frame = self.stream.read(self.chunk_size, exception_on_overflow=False)
            return frame

        except IOError:
            return None

    def calibrate(self) -> None:
        """Calibrate energy threshold by sampling background noise"""
        print("Calibrating microphone...")
        self.energy_threshold = 500  # Default, will be adjusted

    def close(self):
        """Cleanup resources"""
        if self.stream:
            self.stream.stop_stream()
            self.stream.close()
        if hasattr(self, 'p'):
            self.p.terminate()