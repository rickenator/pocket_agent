"""Ollama Backend Implementation"""

import httpx
import json
from typing import AsyncIterator
from .base import AIBackend


class OllamaBackend(AIBackend):
    """Ollama AI backend for local LLM inference"""

    def __init__(self, config: dict):
        super().__init__(config)
        self.host = config.get('host', 'http://localhost:11434')
        self.model = config.get('model', 'llama2')
        self.temperature = config.get('temperature', 0.7)
        self.max_tokens = config.get('max_tokens', 500)
        self.client = httpx.AsyncClient(base_url=self.host, timeout=30.0)

    async def is_available(self) -> bool:
        """Check if Ollama is running"""
        try:
            response = await self.client.get("/api/tags")
            return response.status_code == 200
        except httpx.RequestError:
            return False

    async def chat(self, message: str, context: list = None) -> AsyncIterator[str]:
        """Chat with Ollama streaming response"""
        messages = [{"role": "user", "content": message}]

        # Add context if provided
        if context:
            for msg in context:
                messages.append(msg)

        payload = {
            "model": self.model,
            "messages": messages,
            "stream": True,
            "options": {
                "temperature": self.temperature,
                "num_predict": self.max_tokens
            }
        }

        try:
            async with self.client.stream("POST", "/api/chat", json=payload) as response:
                if response.status_code != 200:
                    raise Exception(f"Ollama API error: {response.status_code}")

                async for line in response.aiter_lines():
                    if line.strip():
                        data = json.loads(line)
                        if 'message' in data and 'content' in data['message']:
                            yield data['message']['content']

        except httpx.RequestError as e:
            yield f"[Error] Failed to connect to Ollama: {e}"

    async def close(self):
        """Close the HTTP client"""
        await self.client.aclose()

    def __del__(self):
        """Cleanup on deletion"""
        if hasattr(self, 'client') and self.client:
            try:
                self.client.aclose()
            except:
                pass

    def __del__(self):
        """Cleanup on deletion"""
        if hasattr(self, 'client') and self.client:
            self.client.aclose()


class LocalLLMProxy:
    """Proxy for local LLM inference (alternative interface)"""

    def __init__(self, backend: OllamaBackend):
        self.backend = backend

    async def generate(self, prompt: str, max_tokens: int = 500) -> str:
        """Generate text from prompt"""
        chunks = []
        async for chunk in self.backend.chat(prompt):
            chunks.append(chunk)
        return ''.join(chunks)

    async def list_models(self) -> list:
        """List available models"""
        response = self.backend.client.get("/api/tags")
        if response.status_code == 200:
            return response.json().get('models', [])
        return []