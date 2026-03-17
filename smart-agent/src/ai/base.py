"""Base AI Backend Interface"""

from abc import ABC, abstractmethod
from typing import AsyncIterator


class AIBackend(ABC):
    """Abstract base class for AI backends"""

    def __init__(self, config: dict):
        self.config = config

    @abstractmethod
    async def chat(self, message: str, context: list = None) -> AsyncIterator[str]:
        """Send a message and stream the response

        Args:
            message: User input message
            context: Conversation history

        Yields:
            Response chunks
        """
        pass

    @abstractmethod
    async def is_available(self) -> bool:
        """Check if the backend is available and working"""
        pass

    async def generate(self, message: str, **kwargs) -> str:
        """Generate a complete response (blocking version)"""
        chunks = []
        async for chunk in self.chat(message, **kwargs):
            chunks.append(chunk)
        return ''.join(chunks)

    async def close(self):
        """Release backend resources. Override in subclasses as needed."""
        pass