"""Deterministic scenarios for the bounded voice manager.

This catalog asserts lifecycle invariants and counter values directly.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import Iterable, TypeAlias


@dataclass(frozen=True)
class RawMidiEvent:
    sample: int
    data: bytes
    label: str = ""

    def validate(self, duration_samples: int) -> None:
        if not 0 <= self.sample < duration_samples:
            raise ValueError(f"MIDI event outside scenario: {self.sample}")
        if not 1 <= len(self.data) <= 3:
            raise ValueError("MIDI channel messages must contain 1-3 bytes")
        status = self.data[0]
        if not 0x80 <= status < 0xF0:
            raise ValueError(f"Not a MIDI channel message: {self.data!r}")
        expected = 2 if status & 0xF0 in (0xC0, 0xD0) else 3
        if len(self.data) != expected or any(value >= 0x80 for value in self.data[1:]):
            raise ValueError(f"Malformed MIDI channel message: {self.data!r}")

    @property
    def is_note_on(self) -> bool:
        return self.data[0] & 0xF0 == 0x90 and self.data[2] != 0


@dataclass(frozen=True)
class VoiceLimitEvent:
    sample: int
    limit: int

    def validate(self, duration_samples: int, block_size: int) -> None:
        if not 0 <= self.sample < duration_samples or self.sample % block_size != 0:
            raise ValueError("Voice-limit changes must occur at a host-block boundary")
        if not 1 <= self.limit <= 16:
            raise ValueError(f"Invalid voice limit: {self.limit}")


VoiceAction: TypeAlias = RawMidiEvent | VoiceLimitEvent


@dataclass(frozen=True)
class VoiceScenario:
    id: str
    description: str
    duration_samples: int
    voice_limit: int = 16
    sample_rate: int = 48_000
    block_size: int = 128
    actions: tuple[VoiceAction, ...] = field(default_factory=tuple)
    tags: tuple[str, ...] = ("core",)

    @property
    def expected_note_starts(self) -> int:
        return sum(isinstance(action, RawMidiEvent) and action.is_note_on for action in self.actions)

    def validate(self) -> None:
        if not self.id or any(c not in "abcdefghijklmnopqrstuvwxyz0123456789._-" for c in self.id):
            raise ValueError(f"Unsafe scenario id: {self.id!r}")
        if self.sample_rate <= 0 or self.block_size <= 0 or self.duration_samples <= 0:
            raise ValueError(f"Invalid dimensions for {self.id}")
        if not 1 <= self.voice_limit <= 16:
            raise ValueError(f"Invalid initial voice limit for {self.id}")
        previous_sample = -1
        for action in self.actions:
            if action.sample < previous_sample:
                raise ValueError(f"Actions are not time ordered in {self.id}")
            if isinstance(action, RawMidiEvent):
                action.validate(self.duration_samples)
            else:
                action.validate(self.duration_samples, self.block_size)
            previous_sample = action.sample


def _status(kind: int, channel: int) -> int:
    if not 1 <= channel <= 16:
        raise ValueError(f"MIDI channel must be 1-16, got {channel}")
    return kind | (channel - 1)


def note_on(sample: int, channel: int, note: int, velocity: int = 100) -> RawMidiEvent:
    return RawMidiEvent(sample, bytes((_status(0x90, channel), note, velocity)), "note_on")


def note_off(sample: int, channel: int, note: int, velocity: int = 0) -> RawMidiEvent:
    return RawMidiEvent(sample, bytes((_status(0x80, channel), note, velocity)), "note_off")


def poly_pressure(sample: int, channel: int, note: int, value: int) -> RawMidiEvent:
    return RawMidiEvent(sample, bytes((_status(0xA0, channel), note, value)), "poly_pressure")


def control_change(sample: int, channel: int, controller: int, value: int) -> RawMidiEvent:
    return RawMidiEvent(sample, bytes((_status(0xB0, channel), controller, value)), f"cc{controller}")


def channel_pressure(sample: int, channel: int, value: int) -> RawMidiEvent:
    return RawMidiEvent(sample, bytes((_status(0xD0, channel), value)), "channel_pressure")


def pitch_bend(sample: int, channel: int, value: int) -> RawMidiEvent:
    if not 0 <= value <= 0x3FFF:
        raise ValueError(f"Pitch bend must be 0-16383, got {value}")
    return RawMidiEvent(sample, bytes((_status(0xE0, channel), value & 0x7F, value >> 7)), "pitch_bend")


def _same_sample_replacement() -> VoiceScenario:
    actions: list[VoiceAction] = []
    for index in range(8):
        actions.append(note_on(0, index + 2, 48 + index, 90))
    for index in range(8):
        actions.append(note_on(512, index + 2, 72 + index, 100))
    for index in range(8):
        actions.append(note_off(2_048, index + 2, 72 + index))
    return VoiceScenario(
        "voice.same-sample-replacement.48k.128",
        "Replace a rendered eight-note MPE chord at one sample with only two fade slots.",
        4_096,
        voice_limit=8,
        actions=tuple(actions),
        tags=("smoke", "core"),
    )


def _same_note_retrigger() -> VoiceScenario:
    actions: list[VoiceAction] = []
    for index in range(100):
        sample = index * 32
        actions.extend(
            (
                note_on(sample, 2, 60, 80 + index % 40),
                poly_pressure(sample + 1, 2, 60, index % 128),
                control_change(sample + 2, 2, 74, (index * 17) % 128),
                pitch_bend(sample + 3, 2, (index * 997) & 0x3FFF),
            )
        )
    actions.append(note_off(3_300, 2, 60))
    return VoiceScenario(
        "voice.same-note-generation.48k.128",
        "Retrigger one member-channel note 100 times with per-note expression.",
        4_096,
        voice_limit=4,
        actions=tuple(actions),
        tags=("core",),
    )


def _pedal_and_panic() -> VoiceScenario:
    actions: list[VoiceAction] = [control_change(0, 1, 64, 127)]
    for index in range(6):
        actions.append(note_on(0, index + 2, 54 + index, 96))
        actions.append(note_off(256, index + 2, 54 + index))
    actions.extend(
        (
            control_change(512, 1, 64, 0),
            control_change(768, 1, 123, 0),
            note_on(1_024, 2, 72, 100),
            channel_pressure(1_025, 2, 81),
            control_change(1_280, 1, 120, 0),
        )
    )
    actions.sort(key=lambda action: action.sample)
    return VoiceScenario(
        "voice.pedal-panic.48k.128",
        "Exercise MPE-zone sustain, natural all-notes-off, and hard all-sound-off.",
        2_048,
        voice_limit=8,
        actions=tuple(actions),
        tags=("smoke", "core"),
    )


def _limit_automation() -> VoiceScenario:
    actions: list[VoiceAction] = []
    for index in range(12):
        actions.append(note_on(0, index + 2, 42 + index, 100))
    actions.append(VoiceLimitEvent(512, 1))
    actions.append(note_on(512, 15, 76, 100))
    actions.append(VoiceLimitEvent(1_024, 16))
    for index in range(8):
        actions.append(note_on(1_024, index + 2, 84 + index, 92))
    actions.sort(key=lambda action: action.sample)
    return VoiceScenario(
        "voice.limit-16-1-16.48k.128",
        "Lower and restore logical polyphony without resizing the physical pool.",
        2_048,
        actions=tuple(actions),
        tags=("core",),
    )


def _face_roll() -> VoiceScenario:
    actions: list[VoiceAction] = []
    for index in range(128):
        sample = index * 8
        channel = 2 + index % 15
        actions.append(note_on(sample, channel, 24 + (index * 29) % 96, 70 + index % 58))
    return VoiceScenario(
        "voice.face-roll-128.48k.128",
        "A dense MPE-member face-roll must start every note while remaining bounded.",
        2_048,
        voice_limit=8,
        actions=tuple(actions),
        tags=("smoke", "core"),
    )


SCENARIOS = (
    _same_sample_replacement(),
    _same_note_retrigger(),
    _pedal_and_panic(),
    _limit_automation(),
    _face_roll(),
)

for _scenario in SCENARIOS:
    _scenario.validate()


def select_scenarios(suite: str, scenario_ids: Iterable[str] = ()) -> tuple[VoiceScenario, ...]:
    requested = tuple(scenario_ids)
    known = {scenario.id for scenario in SCENARIOS}
    unknown = sorted(set(requested) - known)
    if unknown:
        raise ValueError(f"Unknown lifecycle scenario(s): {', '.join(unknown)}")
    if requested:
        selected = set(requested)
        return tuple(scenario for scenario in SCENARIOS if scenario.id in selected)
    if suite == "all":
        return SCENARIOS
    if suite not in {"smoke", "core"}:
        raise ValueError(f"Unknown lifecycle suite: {suite}")
    return tuple(scenario for scenario in SCENARIOS if suite in scenario.tags)


def make_random_stress(event_count: int, seed: int = 0x5EED) -> VoiceScenario:
    """Build a deterministic mixed-message stress stream without using system RNG."""
    if event_count < 1:
        raise ValueError("Stress event count must be positive")
    state = seed & 0xFFFFFFFF or 1

    def next_value() -> int:
        nonlocal state
        state ^= state << 13 & 0xFFFFFFFF
        state ^= state >> 17
        state ^= state << 5 & 0xFFFFFFFF
        state &= 0xFFFFFFFF
        return state

    actions: list[VoiceAction] = []
    active_notes = [60] * 15
    for sample in range(event_count):
        value = next_value()
        channel = 2 + value % 15
        member = channel - 2
        kind = sample % 5
        if kind == 0:
            note = 24 + (value >> 8) % 96
            active_notes[member] = note
            actions.append(note_on(sample, channel, note, 1 + (value >> 16) % 127))
        elif kind == 1:
            actions.append(pitch_bend(sample, channel, value & 0x3FFF))
        elif kind == 2:
            actions.append(control_change(sample, channel, 74, (value >> 9) & 0x7F))
        elif kind == 3:
            actions.append(channel_pressure(sample, channel, (value >> 17) & 0x7F))
        else:
            actions.append(note_off(sample, channel, active_notes[member]))
    duration = ((event_count + 127) // 128 + 1) * 128
    scenario = VoiceScenario(
        f"voice.random-{event_count}.48k.128",
        f"Deterministic randomized stream containing {event_count} MIDI events.",
        duration,
        voice_limit=8,
        actions=tuple(actions),
        tags=("stress",),
    )
    scenario.validate()
    return scenario
