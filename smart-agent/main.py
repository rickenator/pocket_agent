#!/usr/bin/env python3
"""
Smart Agent - Main Application
Voice-enabled AI assistant with round AMOLED display
"""

import asyncio
import logging
import sys
import yaml
from pathlib import Path

logger = logging.getLogger(__name__)

# Add src to path
sys.path.insert(0, str(Path(__file__).parent / 'src'))

from ai.ollama import OllamaBackend
from ai.gemini import GeminiBackend
from voice.mic import MicrophoneManager
from voice.speech import SpeechRecognizer
from voice.tts import TextToSpeech
from ui.display import DisplayManager
from ui.widgets import MainWidget


class SmartAgent:
    """Main smart agent application"""

    def __init__(self, config_path: str = "config/settings.yaml"):
        self.config = self.load_config(config_path)
        self.conversation_history: list = []
        self.setup_ai_backend()
        self.setup_display()
        self.setup_voice()

    def load_config(self, config_path: str):
        """Load configuration file"""
        path = Path(config_path)
        if not path.exists():
            path = Path(__file__).parent / 'config' / 'settings.example.yaml'

        with open(path, 'r') as f:
            config = yaml.safe_load(f)

        return config

    def setup_ai_backend(self):
        """Initialize AI backend based on configuration"""
        backend_type = self.config.get('ai_backend', 'ollama')

        if backend_type == 'ollama':
            self.ai = OllamaBackend(self.config.get('ollama', {}))
        elif backend_type == 'gemini':
            self.ai = GeminiBackend(self.config.get('gemini', {}))
        else:
            raise ValueError(f"Unknown AI backend: {backend_type}")

    def setup_display(self):
        """Initialize display"""
        self.display = DisplayManager(self.config.get('display', {}))
        self.ui = MainWidget(self.display)

    def setup_voice(self):
        """Initialize voice components"""
        mic_config = self.config.get('voice', {})
        mic = MicrophoneManager(mic_config)
        mic.setup()

        self.speech = SpeechRecognizer(mic_config, mic)
        self.tts = TextToSpeech(mic_config, mic)

    async def run(self):
        """Main application loop"""
        logger.info("Smart Agent starting...")

        # Show welcome screen
        self.ui.show_welcome()

        # Check AI backend
        ai_available = await self.ai.is_available()
        self.ui.status.show_ai_status(
            self.config.get('ai_backend', 'ollama'),
            ai_available
        )

        if not ai_available:
            logger.warning("AI backend not available!")
            return

        # Main interaction loop
        while True:
            # Show idle state
            self.ui.handle_interaction("idle")
            self.display.show_message("Press mic to speak", duration=3000)

            # Listen for speech
            self.ui.handle_interaction("listening")
            self.display.show_message("Listening...", duration=2000)

            text = await asyncio.to_thread(self.speech.listen)
            if not text:
                continue

            # Show user message
            self.ui.chat.add_message("user", text)

            # Generate AI response
            self.ui.handle_interaction("thinking")
            self.display.show_message("AI is thinking...", duration=2000)

            try:
                response_chunks = []
                async for chunk in self.ai.chat(text, context=self.conversation_history):
                    response_chunks.append(chunk)
                    # Stream to display
                    self.display.show_text(chunk[-20:], size=16, color=(200, 200, 200))

                response = ''.join(response_chunks)

                # Update conversation history
                self.conversation_history.append({"role": "user", "content": text})
                self.conversation_history.append({"role": "assistant", "content": response})

                # Show AI response
                self.ui.chat.add_message("assistant", response)

                # Speak response
                self.tts.speak(response)
            except Exception as e:
                logger.error("AI chat error: %s", e)
                self.ui.handle_interaction("idle")

    async def run_continuous(self):
        """Run with continuous listening"""
        logger.info("Starting continuous listening mode...")

        while True:
            text = await asyncio.to_thread(self.speech.listen)
            if not text:
                continue

            self.ui.chat.add_message("user", text)
            self.ui.handle_interaction("thinking")

            try:
                response_chunks = []
                async for chunk in self.ai.chat(text, context=self.conversation_history):
                    response_chunks.append(chunk)
                    self.display.show_text(chunk[-15:], size=16, color=(0, 255, 0))
                    self.tts.speak(chunk)

                response = ''.join(response_chunks)
                # Update conversation history
                self.conversation_history.append({"role": "user", "content": text})
                self.conversation_history.append({"role": "assistant", "content": response})

                self.ui.chat.add_message("assistant", response)
            except Exception as e:
                logger.error("AI chat error: %s", e)
                self.ui.handle_interaction("idle")

    def close(self):
        """Cleanup resources"""
        self.display.close()
        if hasattr(self, 'speech'):
            self.speech.close()
        if hasattr(self, 'tts'):
            self.tts.close()
        if hasattr(self, 'ai'):
            self.ai.close()


async def main():
    """Main entry point"""
    agent = SmartAgent()
    log_level = agent.config.get('app', {}).get('log_level', 'INFO')
    logging.basicConfig(
        level=getattr(logging, log_level.upper(), logging.INFO),
        format='%(asctime)s %(levelname)s %(name)s: %(message)s',
    )

    try:
        # Check if continuous mode requested
        if len(sys.argv) > 1 and sys.argv[1] == '--continuous':
            await agent.run_continuous()
        else:
            await agent.run()
    except KeyboardInterrupt:
        logger.info("Shutting down...")
    finally:
        agent.close()


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        logger.info("Bye!")