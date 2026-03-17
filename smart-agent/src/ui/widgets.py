"""UI Widgets for the Smart Agent"""

from typing import List, Callable
from .display import DisplayManager


class ChatWidget:
    """Widget for displaying chat messages"""

    def __init__(self, display: DisplayManager, max_messages: int = 10):
        self.display = display
        self.max_messages = max_messages
        self.messages = []
        self.y_offset = 30

    def add_message(self, role: str, text: str):
        """Add a message to the chat history"""
        self.messages.append({"role": role, "text": text})

        # Keep only recent messages
        if len(self.messages) > self.max_messages:
            self.messages = self.messages[-self.max_messages:]

        self.render()

    def clear(self):
        """Clear the chat display"""
        self.messages = []
        self.display.clear()

    def render(self):
        """Render the chat messages"""
        self.display.clear()

        for msg in self.messages:
            color = (0, 255, 0) if msg["role"] == "user" else (255, 255, 255)
            role = "You" if msg["role"] == "user" else "AI"

            # Show role
            self.display.show_text(f"{role}: ", size=14, color=color, align="left")

            # Show message (truncated if needed)
            text = msg["text"][:60]  # Limit to 60 chars
            if len(msg["text"]) > 60:
                text += "..."

            self.display.show_text(text, size=16, color=(200, 200, 200), align="left")

            self.display.sleep(20)  # Small delay between messages

    def stream_message(self, role: str, callback: Callable[[str], None]):
        """Stream a message as it's being generated"""
        self.display.clear()
        self.display.show_text(f"{role}:", size=18, color=(255, 255, 255))

        buffer = ""
        for char in callback:
            buffer += char
            self.display.show_text(buffer[-20:], size=16, color=(200, 200, 200))
            self.display.sleep(10)

        self.display.sleep(100)
        self.add_message(role, buffer)


class StatusWidget:
    """Widget for displaying system status"""

    def __init__(self, display: DisplayManager):
        self.display = display
        self.status_items = {}

    def update_status(self, key: str, value: str):
        """Update a status item"""
        self.status_items[key] = value
        self.render()

    def clear(self):
        """Clear status display"""
        self.status_items = {}
        self.display.clear()

    def render(self):
        """Render all status items"""
        self.display.clear()

        y = 30
        for key, value in self.status_items.items():
            text = f"{key}: {value}"
            self.display.show_text(text, size=14, color=(0, 255, 0), align="left")
            y += 25

    def show_wifi_status(self, connected: bool, ssid: str = ""):
        """Show WiFi connection status"""
        if connected:
            self.display.show_message(f"WiFi Connected: {ssid}", duration=2000)
        else:
            self.display.show_message("WiFi Disconnected", duration=2000)

    def show_ai_status(self, backend: str, available: bool):
        """Show AI backend status"""
        status = f"AI: {backend} - {'OK' if available else 'NOT READY'}"
        self.display.show_message(status, duration=2000)

    def show_battery_status(self, level: int):
        """Show battery level"""
        icon = "🔋" if level > 50 else "⚠️"
        self.display.show_message(f"{icon} {level}%", duration=2000)


class InputWidget:
    """Widget for showing user input waiting state"""

    def __init__(self, display: DisplayManager):
        self.display = display

    def show_listening(self):
        """Show listening indicator"""
        self.display.show_icon("mic", self.display.width // 2, self.display.height // 2 - 30)
        self.display.show_text("Listening...", size=20, color=(255, 255, 0), align="center")

    def show_typing(self):
        """Show typing indicator"""
        self.display.show_text("AI is typing...", size=20, color=(0, 255, 0), align="center")

    def clear(self):
        """Clear the input widget"""
        self.display.clear()


class MainWidget:
    """Main UI widget combining all elements"""

    def __init__(self, display: DisplayManager):
        self.display = display
        self.chat = ChatWidget(display)
        self.status = StatusWidget(display)
        self.input = InputWidget(display)

    def show_welcome(self):
        """Show welcome screen"""
        self.display.clear()
        self.display.show_text("Smart Agent", size=40, color=(0, 255, 255), align="center")
        self.display.show_text("Press and hold mic", size=18, color=(255, 255, 255), align="center")
        self.display.sleep(2000)

    def handle_interaction(self, state: str):
        """Handle different interaction states"""
        if state == "listening":
            self.input.show_listening()
        elif state == "thinking":
            self.input.show_typing()
        elif state == "idle":
            self.input.clear()