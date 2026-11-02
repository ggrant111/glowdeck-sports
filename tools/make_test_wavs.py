#!/usr/bin/env python3
"""Generate original, redistributable GlowDeck GameDay test audio files.

Uses only Python stdlib. Makes canonical PCM mono 16kHz 16-bit WAV files
compatible with ESP32 v0.4 microSD and Fire TV v0.4 import.
"""
import argparse
import math
import struct
import wave
from pathlib import Path

RATE = 16000
PRESETS = {
    "HORN1": ([196, 247, 294], 2.4),
    "HORN2": ([247, 294, 370], 2.1),
    "SIREN": ([330, 440, 550], 1.7),
}


def create(path: Path, name: str, volume: float = 0.28) -> None:
    frequencies, seconds = PRESETS[name]
    total = int(seconds * RATE)
    with wave.open(str(path), "wb") as audio:
        audio.setnchannels(1)
        audio.setsampwidth(2)
        audio.setframerate(RATE)
        chunk = bytearray()
        for i in range(total):
            t = i / RATE
            ramp = min(1.0, t / 0.12, (seconds - t) / 0.25)
            if name == "SIREN":
                frequencies = [330 + 145 * math.sin(9 * t), 440, 550]
            signal = sum(
                math.sin(2 * math.pi * f * t) / (2.0 + index * 2.0)
                for index, f in enumerate(frequencies)
            )
            sample = int(max(-32767, min(32767, signal * ramp * volume * 32767)))
            chunk.extend(struct.pack("<h", sample))
            if len(chunk) >= 4096:
                audio.writeframesraw(chunk)
                chunk.clear()
        if chunk:
            audio.writeframesraw(chunk)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("output", type=Path, help="Folder for HORN1.wav, HORN2.wav, SIREN.wav")
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    for preset in PRESETS:
        path = args.output / f"{preset}.wav"
        create(path, preset)
        print(f"Created {path} ({path.stat().st_size} bytes)")


if __name__ == "__main__":
    main()
