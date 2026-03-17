"""Gemini Backend Implementation"""

import json
import httpx
from typing import AsyncIterator
from .base import AIBackend


class GeminiBackend(AIBackend):
    """Google Gemini AI backend"""

    def __init__(self, config: dict):
        super().__init__(config)
        self.api_key = config.get('api_key', '')
        self.model = config.get('model', 'gemini-1.5-flash')
        self.temperature = config.get('temperature', 0.7)
        self.max_tokens = config.get('max_tokens', 500)
        self.client = httpx.AsyncClient()

    async def is_available(self) -> bool:
        """Check if API key is configured"""
        return bool(self.api_key)

    async def chat(self, message: str, context: list = None) -> AsyncIterator[str]:
        """Chat with Gemini streaming response"""
        url = (
            f"https://generativelanguage.googleapis.com/v1beta/models/"
            f"{self.model}:streamGenerateContent?key={self.api_key}"
        )

        headers = {
            "Content-Type": "application/json"
        }

        contents = []

        # Add context (history) before the new message
        if context:
            for msg in context:
                if msg.get('role') == 'user':
                    contents.append({"role": "user", "parts": [{"text": msg.get('content', '')}]})
                elif msg.get('role') == 'assistant':
                    contents.append({"role": "model", "parts": [{"text": msg.get('content', '')}]})

        contents.append({"role": "user", "parts": [{"text": message}]})

        payload = {
            "contents": contents,
            "generationConfig": {
                "temperature": self.temperature,
                "maxOutputTokens": self.max_tokens,
            }
        }

        try:
            async with self.client.stream("POST", url, headers=headers, json=payload) as response:
                if response.status_code != 200:
                    error_text = await response.aread()
                    yield f"[Error] Gemini API error: {response.status_code} - {error_text}"
                    return

                async for line in response.aiter_lines():
                    if line.strip():
                        try:
                            data = json.loads(line)
                            if 'candidates' in data and len(data['candidates']) > 0:
                                content = data['candidates'][0].get('content', {})
                                parts = content.get('parts', [])
                                for part in parts:
                                    if 'text' in part:
                                        yield part['text']
                        except Exception as e:
                            continue

        except httpx.RequestError as e:
            yield f"[Error] Failed to connect to Gemini: {e}"

    async def close(self):
        """Close the HTTP client"""
        await self.client.aclose()

    def __del__(self):
        """Cleanup on deletion"""
        if hasattr(self, 'client') and self.client:
            try:
                import asyncio
                loop = asyncio.get_event_loop()
                if loop.is_running():
                    loop.create_task(self.client.aclose())
                else:
                    loop.run_until_complete(self.client.aclose())
            except Exception:
                pass