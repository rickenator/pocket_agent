"""Gemini Backend Implementation"""

import os
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
        url = f"https://generativelanguage.googleapis.com/v1beta/models/{self.model}:streamGenerateContent"

        headers = {
            "Authorization": f"Bearer {self.api_key}",
            "Content-Type": "application/json"
        }

        contents = [{"role": "user", "parts": [{"text": message}]}]

        # Add context if provided
        if context:
            for msg in context:
                if msg.get('role') == 'user':
                    contents.append({"role": "user", "parts": [{"text": msg.get('content', '')}]})
                elif msg.get('role') == 'assistant':
                    contents.append({"role": "model", "parts": [{"text": msg.get('content', '')}]})

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
                            data = httpx.JSONResponse.content_to_json(line)
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
            self.client.aclose()