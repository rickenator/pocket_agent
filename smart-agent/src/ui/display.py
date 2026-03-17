"""Display Manager for Round AMOLED Display"""

from typing import Optional, Callable
from PIL import Image, ImageDraw, ImageFont
import math


class DisplayManager:
    """Manages the round AMOLED display on ESP32"""

    def __init__(self, config: dict):
        self.config = config
        self.width = config.get('width', 466)
        self.height = config.get('height', 466)
        self.rotation = config.get('rotation', 0)
        self.brightness = config.get('brightness', 255)
        self.is_initialized = False

        # Initialize display (will be ESP-specific)
        self.initialize_display()

    def initialize_display(self):
        """Initialize the display hardware"""
        try:
            # Try to import ESP-specific display driver
            from esp32_display import ESP32AMOLED

            self.display = ESP32AMOLED(
                width=self.width,
                height=self.height,
                rotation=self.rotation,
                brightness=self.brightness
            )
            self.is_initialized = True
            print("Display initialized successfully")
        except ImportError:
            print("ESP display driver not found - running in PC simulation mode")
            self.display = None

    def clear(self):
        """Clear the display"""
        if self.display:
            self.display.fill((0, 0, 0))
            self.display.display()
        else:
            # PC simulation
            print("[Display] Cleared")

    def show_text(self, text: str, x: int = 0, y: int = 0,
                  size: int = 20, color: tuple = (255, 255, 255),
                  align: str = "left"):
        """Show text on display"""
        if not self.display:
            print(f"[Display] Text: '{text}' (simulated)")
            return

        try:
            # Create image with transparency
            img = Image.new('RGBA', (self.width, self.height), (0, 0, 0, 0))
            draw = ImageDraw.Draw(img)

            # Draw text
            font = ImageFont.truetype("Arial.ttf", size)
            bbox = draw.textbbox((0, 0), text, font=font)
            text_width = bbox[2] - bbox[0]
            text_height = bbox[3] - bbox[1]

            # Calculate position
            if align == "center":
                pos_x = (self.width - text_width) // 2
                pos_y = y
            elif align == "right":
                pos_x = self.width - text_width - 10
                pos_y = y
            else:  # left
                pos_x = x
                pos_y = y

            # Draw text with shadow
            draw.text((pos_x + 2, pos_y + 2), text,
                     font=font, fill=(0, 0, 0, 128))
            draw.text((pos_x, pos_y), text,
                     font=font, fill=color)

            # Convert to RGB and send to display
            rgb_img = img.convert('RGB')
            self.display.display(rgb_img)

        except Exception as e:
            print(f"[Display] Error: {e}")

    def show_message(self, text: str, duration: int = 3000):
        """Show a temporary message"""
        self.clear()
        self.show_text(text, size=24)
        if duration > 0 and self.display:
            import time
            time.sleep(duration / 1000)

    def show_status(self, status: str, value: str = ""):
        """Show status indicator"""
        if not self.display:
            print(f"[Display] Status: {status}: {value}")
            return

        try:
            self.clear()
            font = ImageFont.truetype("Arial.ttf", 18)

            # Draw status bar
            draw = Image.new('RGBA', (self.width, self.height), (0, 0, 0, 0))
            d = ImageDraw.Draw(draw)

            # Status text
            status_text = f"Status: {status}"
            if value:
                status_text += f" - {value}"

            d.text((20, 20), status_text, font=font, fill=(255, 255, 255))

            # Show on display
            rgb_img = draw.convert('RGB')
            self.display.display(rgb_img)

        except Exception as e:
            print(f"[Display] Error showing status: {e}")

    def show_icon(self, icon: str, x: int, y: int, size: int = 50):
        """Show a simple icon"""
        if not self.display:
            print(f"[Display] Icon: {icon} at ({x}, {y})")
            return

        try:
            img = Image.new('RGBA', (self.width, self.height), (0, 0, 0, 0))
            draw = ImageDraw.Draw(img)

            # Simple shapes for icons
            if icon == "mic":
                draw.ellipse([x, y, x + size, y + size], fill=(255, 0, 0))
            elif icon == "speaker":
                draw.ellipse([x, y, x + size, y + size], fill=(0, 255, 0))
            elif icon == "wifi":
                draw.arc([x, y, x + size, y + size], 0, 180, fill=(0, 0, 255), width=3)
                draw.arc([x + 10, y + 10, x + size - 10, y + size - 10], 0, 180, fill=(0, 0, 255), width=3)
            elif icon == "chat":
                draw.ellipse([x, y, x + size, y + size], fill=(255, 255, 0))

            rgb_img = img.convert('RGB')
            self.display.display(rgb_img)

        except Exception as e:
            print(f"[Display] Error showing icon: {e}")

    def update_progress(self, progress: float, max_value: float = 1.0):
        """Update progress indicator"""
        if not self.display:
            print(f"[Display] Progress: {progress}/{max_value}")
            return

        try:
            self.clear()
            draw = Image.new('RGBA', (self.width, self.height), (0, 0, 0, 0))
            d = ImageDraw.Draw(draw)

            # Progress bar
            bar_width = 200
            bar_height = 20
            bar_x = (self.width - bar_width) // 2
            bar_y = (self.height - bar_height) // 2

            # Background
            d.rectangle([bar_x, bar_y, bar_x + bar_width, bar_y + bar_height],
                       fill=(50, 50, 50))

            # Progress
            progress_width = int(bar_width * (progress / max_value))
            d.rectangle([bar_x, bar_y, bar_x + progress_width, bar_y + bar_height],
                       fill=(0, 255, 0))

            # Text
            d.text((20, 20), f"Progress: {int(progress * 100)}%", font=ImageFont.truetype("Arial.ttf", 16), fill=(255, 255, 255))

            rgb_img = draw.convert('RGB')
            self.display.display(rgb_img)

        except Exception as e:
            print(f"[Display] Error updating progress: {e}")

    def close(self):
        """Cleanup display"""
        if self.display:
            self.display.cleanup()

    def sleep(self, milliseconds: int):
        """Sleep without blocking"""
        if self.display:
            import time
            time.sleep(milliseconds / 1000)
        else:
            import time
            time.sleep(milliseconds / 1000)

    def power_down(self):
        """Power down display to save power"""
        if self.display:
            self.display.power_down()
        else:
            print("[Display] Power down simulation")