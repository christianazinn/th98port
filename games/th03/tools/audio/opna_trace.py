#!/usr/bin/env python3
"""Normalize PC-98 OPNA I/O and compare exact or controlled effect traces."""

from __future__ import annotations

import argparse
import csv
import decimal
import os
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path


RAW_HEADER = ("ordinal", "pic_ms", "port", "value")
NORMALIZED_HEADER = ("sequence", "source_ordinal", "pic_ms", "address", "value")
MARKER_HEADER = ("ordinal", "pic_ms", "code")
EVENT_HEADER = ("event", "sequence", "chip_clock")
OPNA_CLOCK_HZ = 7_987_200
EFFECT_BURST_GAP_MS = decimal.Decimal("2")


class TraceError(RuntimeError):
    pass


@dataclass(frozen=True)
class Register:
    sequence: int
    address: int
    value: int


@dataclass(frozen=True)
class TimedRegister:
    sequence: int
    timestamp: decimal.Decimal
    address: int
    value: int


@dataclass(frozen=True)
class NativeRegister:
    sequence: int
    chip_clock: int
    address: int
    value: int


def parse_hex(value: str, maximum: int, field: str) -> int:
    if not value or any(digit not in "0123456789abcdefABCDEF" for digit in value):
        raise TraceError(f"invalid {field}: {value!r}")
    parsed = int(value, 16)
    if parsed > maximum:
        raise TraceError(f"out-of-range {field}: {value!r}")
    return parsed


def load_raw(path: Path) -> list[tuple[int, str, int, int]]:
    try:
        with path.open("r", encoding="ascii", newline="") as source:
            reader = csv.DictReader(source, delimiter="\t")
            if tuple(reader.fieldnames or ()) != RAW_HEADER:
                raise TraceError("raw trace header is not T3 PC-98 OPNA I/O")
            result: list[tuple[int, str, int, int]] = []
            previous_ordinal = -1
            previous_time = decimal.Decimal("-1")
            for row in reader:
                if None in row or any(row[field] is None for field in RAW_HEADER):
                    raise TraceError("raw trace row has the wrong field count")
                try:
                    ordinal = int(row["ordinal"], 10)
                    timestamp = decimal.Decimal(row["pic_ms"])
                except (ValueError, decimal.InvalidOperation) as error:
                    raise TraceError("raw trace has an invalid ordinal/time") from error
                if ordinal != previous_ordinal + 1:
                    raise TraceError("raw trace ordinals are not contiguous from zero")
                if not timestamp.is_finite() or timestamp < previous_time:
                    raise TraceError("raw trace time regressed or is not finite")
                port = parse_hex(row["port"], 0xFFFF, "port")
                value = parse_hex(row["value"], 0xFF, "value")
                if port not in (0x188, 0x18A, 0x18C, 0x18E):
                    raise TraceError(f"unexpected OPNA port: {port:04X}")
                result.append((ordinal, format(timestamp, "f"), port, value))
                previous_ordinal = ordinal
                previous_time = timestamp
    except OSError as error:
        raise TraceError(f"cannot read raw trace: {error}") from error
    if not result:
        raise TraceError("raw trace is empty")
    return result


def normalize(rows: list[tuple[int, str, int, int]]) -> list[tuple[int, int, str, int, int]]:
    address_latches: dict[int, int] = {}
    result: list[tuple[int, int, str, int, int]] = []
    for ordinal, timestamp, port, value in rows:
        if port == 0x188:
            address_latches[0] = value
        elif port == 0x18C:
            address_latches[1] = value
        else:
            bank = 0 if port == 0x18A else 1
            if bank not in address_latches:
                raise TraceError(
                    f"data write at ordinal {ordinal} has no bank-{bank} address"
                )
            result.append((
                len(result) + 1,
                ordinal,
                timestamp,
                address_latches[bank] | (bank << 8),
                value,
            ))
    if not result:
        raise TraceError("raw trace contains no register writes")
    return result


def write_normalized(
    output: Path, rows: list[tuple[int, int, str, int, int]]
) -> None:
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary_name: str | None = None
    try:
        with tempfile.NamedTemporaryFile(
            "w", encoding="ascii", newline="", delete=False,
            dir=output.parent, prefix=f".{output.name}.", suffix=".tmp",
        ) as destination:
            temporary_name = destination.name
            writer = csv.writer(destination, delimiter="\t", lineterminator="\n")
            writer.writerow(NORMALIZED_HEADER)
            for sequence, ordinal, timestamp, address, value in rows:
                writer.writerow((
                    sequence, ordinal, timestamp, f"{address:03X}", f"{value:02X}"
                ))
        os.replace(temporary_name, output)
        temporary_name = None
    except OSError as error:
        raise TraceError(f"cannot publish normalized trace: {error}") from error
    finally:
        if temporary_name is not None:
            try:
                Path(temporary_name).unlink()
            except OSError:
                pass


def load_registers(path: Path) -> list[Register]:
    try:
        with path.open("r", encoding="ascii", newline="") as source:
            reader = csv.DictReader(source, delimiter="\t")
            fields = tuple(reader.fieldnames or ())
            if not all(field in fields for field in ("sequence", "address", "value")):
                raise TraceError("register trace lacks sequence/address/value")
            result: list[Register] = []
            for row in reader:
                try:
                    sequence = int(row["sequence"], 10)
                except (TypeError, ValueError) as error:
                    raise TraceError("invalid register sequence") from error
                if sequence != len(result) + 1:
                    raise TraceError("register sequences are not contiguous from one")
                result.append(Register(
                    sequence,
                    parse_hex(row["address"], 0x1FF, "address"),
                    parse_hex(row["value"], 0xFF, "value"),
                ))
    except OSError as error:
        raise TraceError(f"cannot read register trace: {error}") from error
    if not result:
        raise TraceError("register trace is empty")
    return result


def load_markers(path: Path) -> dict[int, decimal.Decimal]:
    try:
        with path.open("r", encoding="ascii", newline="") as source:
            reader = csv.DictReader(source, delimiter="\t")
            if tuple(reader.fieldnames or ()) != MARKER_HEADER:
                raise TraceError("marker trace header is not T3 control markers")
            result: dict[int, decimal.Decimal] = {}
            previous_ordinal = -1
            previous_time = decimal.Decimal("-1")
            for row in reader:
                try:
                    ordinal = int(row["ordinal"], 10)
                    timestamp = decimal.Decimal(row["pic_ms"])
                    code = parse_hex(row["code"], 0xFF, "marker code")
                except (TypeError, ValueError, decimal.InvalidOperation) as error:
                    raise TraceError("marker trace has an invalid row") from error
                if ordinal != previous_ordinal + 1:
                    raise TraceError("marker ordinals are not contiguous from zero")
                if not timestamp.is_finite() or timestamp < previous_time:
                    raise TraceError("marker time regressed or is not finite")
                if code in result:
                    raise TraceError(f"duplicate marker code: {code:02X}")
                result[code] = timestamp
                previous_ordinal = ordinal
                previous_time = timestamp
    except OSError as error:
        raise TraceError(f"cannot read marker trace: {error}") from error
    if not result:
        raise TraceError("marker trace is empty")
    return result


def load_timed_registers(path: Path) -> list[TimedRegister]:
    try:
        with path.open("r", encoding="ascii", newline="") as source:
            reader = csv.DictReader(source, delimiter="\t")
            fields = tuple(reader.fieldnames or ())
            required = ("sequence", "pic_ms", "address", "value")
            if not all(field in fields for field in required):
                raise TraceError("timed register trace lacks required fields")
            result: list[TimedRegister] = []
            previous_time = decimal.Decimal("-1")
            for row in reader:
                try:
                    sequence = int(row["sequence"], 10)
                    timestamp = decimal.Decimal(row["pic_ms"])
                except (TypeError, ValueError, decimal.InvalidOperation) as error:
                    raise TraceError("timed register trace has an invalid row") from error
                if sequence != len(result) + 1:
                    raise TraceError("register sequences are not contiguous from one")
                if not timestamp.is_finite() or timestamp < previous_time:
                    raise TraceError("register time regressed or is not finite")
                result.append(TimedRegister(
                    sequence, timestamp,
                    parse_hex(row["address"], 0x1FF, "address"),
                    parse_hex(row["value"], 0xFF, "value"),
                ))
                previous_time = timestamp
    except OSError as error:
        raise TraceError(f"cannot read timed register trace: {error}") from error
    if not result:
        raise TraceError("timed register trace is empty")
    return result


def load_native_registers(path: Path) -> list[NativeRegister]:
    try:
        with path.open("r", encoding="ascii", newline="") as source:
            reader = csv.DictReader(source, delimiter="\t")
            required = ("sequence", "chip_clock", "address", "value")
            if not all(field in tuple(reader.fieldnames or ()) for field in required):
                raise TraceError("native register trace lacks required fields")
            result: list[NativeRegister] = []
            previous_clock = -1
            for row in reader:
                try:
                    sequence = int(row["sequence"], 10)
                    chip_clock = int(row["chip_clock"], 10)
                except (TypeError, ValueError) as error:
                    raise TraceError("native register trace has an invalid row") from error
                if sequence != len(result) + 1:
                    raise TraceError("native register sequences are not contiguous from one")
                if chip_clock < previous_clock:
                    raise TraceError("native register chip clock regressed")
                result.append(NativeRegister(
                    sequence, chip_clock,
                    parse_hex(row["address"], 0x1FF, "address"),
                    parse_hex(row["value"], 0xFF, "value"),
                ))
                previous_clock = chip_clock
    except OSError as error:
        raise TraceError(f"cannot read native register trace: {error}") from error
    if not result:
        raise TraceError("native register trace is empty")
    return result


def load_native_events(path: Path) -> dict[str, tuple[int, int]]:
    try:
        with path.open("r", encoding="ascii", newline="") as source:
            reader = csv.DictReader(source, delimiter="\t")
            if tuple(reader.fieldnames or ()) != EVENT_HEADER:
                raise TraceError("native event trace header is not T3ATR001")
            result: dict[str, tuple[int, int]] = {}
            for row in reader:
                event = row["event"]
                if event in result:
                    raise TraceError(f"duplicate native event: {event}")
                try:
                    sequence = int(row["sequence"], 10)
                    chip_clock = int(row["chip_clock"], 10)
                except (TypeError, ValueError) as error:
                    raise TraceError("native event trace has an invalid row") from error
                if sequence < 0 or chip_clock < 0:
                    raise TraceError("native event trace has a negative value")
                result[event] = (sequence, chip_clock)
    except OSError as error:
        raise TraceError(f"cannot read native event trace: {error}") from error
    for required in ("effect-call", "effect-started", "effect-ended"):
        if required not in result:
            raise TraceError(f"native event trace lacks {required}")
    return result


def is_fm6_register(address: int, value: int) -> bool:
    if address == 0x028:
        return (value & 0x07) == 0x06
    if address in (0x1A2, 0x1A6, 0x1B2, 0x1B6):
        return True
    for base in range(0x30, 0xA0, 0x10):
        if address in tuple(0x100 | (base + 2 + (slot * 4)) for slot in range(4)):
            return True
    return False


def is_fm6_keyon(address: int, value: int) -> bool:
    return address == 0x028 and (value & 0x07) == 0x06 and (value & 0xF0) != 0


def canonical_group(rows: list[tuple[int, int]]) -> tuple[tuple[int, int], ...]:
    final: dict[int, int] = {}
    for address, value in rows:
        final[address] = value
    return tuple(sorted(final.items()))


def select_effect_group_range(
    groups: list[list[tuple[int, int]]], label: str
) -> tuple[int, int]:
    start = next((
        index for index, group in enumerate(groups)
        if any(is_fm6_keyon(address, value) for address, value in group)
    ), None)
    if start is None:
        raise TraceError(f"{label} effect has no FM6 key-on")
    end = next((
        index for index in range(start + 1, len(groups))
        if (0x1B6, 0xC0) in groups[index]
    ), None)
    if end is None:
        raise TraceError(f"{label} effect has no later FM6 music restoration")
    return start, end


def pc_effect_window(
    rows: list[TimedRegister], start: decimal.Decimal, end: decimal.Decimal
) -> tuple[list[list[tuple[int, int]]], decimal.Decimal]:
    selected = [
        row for row in rows
        if start <= row.timestamp < end and is_fm6_register(row.address, row.value)
    ]
    groups: list[list[tuple[int, int]]] = []
    group_times: list[list[decimal.Decimal]] = []
    previous_time: decimal.Decimal | None = None
    for row in selected:
        if previous_time is None or row.timestamp - previous_time > EFFECT_BURST_GAP_MS:
            groups.append([])
            group_times.append([])
        groups[-1].append((row.address, row.value))
        group_times[-1].append(row.timestamp)
        previous_time = row.timestamp
    first_index, last_index = select_effect_group_range(groups, "PC-98")
    effect_groups = groups[first_index:last_index + 1]
    keyon_time = next(
        row.timestamp for row in selected
        if row.timestamp in group_times[first_index] and
        is_fm6_keyon(row.address, row.value)
    )
    restoration_time = next(
        row.timestamp for row in selected
        if row.timestamp in group_times[last_index] and
        (row.address, row.value) == (0x1B6, 0xC0)
    )
    return effect_groups, restoration_time - keyon_time


def native_effect_window(
    rows: list[NativeRegister], events: dict[str, tuple[int, int]], clock_hz: int
) -> tuple[list[list[tuple[int, int]]], decimal.Decimal]:
    call_sequence = events["effect-call"][0]
    end_sequence = events["effect-ended"][0]
    selected = [
        row for row in rows
        if call_sequence < row.sequence <= end_sequence and
        is_fm6_register(row.address, row.value)
    ]
    groups: list[list[tuple[int, int]]] = []
    group_clocks: list[int] = []
    for row in selected:
        if not group_clocks or row.chip_clock != group_clocks[-1]:
            groups.append([])
            group_clocks.append(row.chip_clock)
        groups[-1].append((row.address, row.value))
    first_index, last_index = select_effect_group_range(groups, "native")
    effect_groups = groups[first_index:last_index + 1]
    duration = (
        decimal.Decimal(group_clocks[last_index] - group_clocks[first_index]) *
        decimal.Decimal(1000) / decimal.Decimal(clock_hz)
    )
    return effect_groups, duration


def compare_effects(
    markers_path: Path,
    pc_path: Path,
    native_directory: Path,
    effects: list[int],
    clock_hz: int,
    duration_tolerance_percent: decimal.Decimal,
) -> int:
    if clock_hz <= 0:
        raise TraceError("OPNA clock must be positive")
    if duration_tolerance_percent < 0:
        raise TraceError("duration tolerance must not be negative")
    markers = load_markers(markers_path)
    pc_rows = load_timed_registers(pc_path)
    mismatched = 0
    for effect in effects:
        if effect < 1 or effect > 22:
            raise TraceError(f"effect number out of range: {effect}")
        start_code = 0x60 + effect
        end_code = 0x80 + effect
        if start_code not in markers or end_code not in markers:
            raise TraceError(f"marker trace lacks effect {effect}")
        name = f"effect-{effect:02d}"
        native_rows = load_native_registers(native_directory / f"{name}-registers.tsv")
        events = load_native_events(native_directory / f"{name}-events.tsv")
        pc_groups, pc_duration = pc_effect_window(
            pc_rows, markers[start_code], markers[end_code]
        )
        native_groups, native_duration = native_effect_window(
            native_rows, events, clock_hz
        )
        first_difference: int | None = None
        for index, (pc_group, native_group) in enumerate(
            zip(pc_groups, native_groups), start=1
        ):
            if canonical_group(pc_group) != canonical_group(native_group):
                first_difference = index
                break
        if first_difference is None and len(pc_groups) != len(native_groups):
            first_difference = min(len(pc_groups), len(native_groups)) + 1
        duration_delta = abs(pc_duration - native_duration)
        duration_percent = (
            duration_delta * decimal.Decimal(100) / native_duration
            if native_duration else decimal.Decimal(0)
        )
        duration_ok = duration_percent <= duration_tolerance_percent
        status = "match" if first_difference is None and duration_ok else "different"
        print(
            f"effect={effect:02d} status={status} "
            f"pc_groups={len(pc_groups)} native_groups={len(native_groups)} "
            f"pc_ms={pc_duration:.6f} native_ms={native_duration:.6f} "
            f"duration_delta_percent={duration_percent:.6f}"
        )
        if first_difference is not None:
            print(f"effect={effect:02d} first_group={first_difference}")
            group_index = first_difference - 1
            if group_index >= len(pc_groups):
                print(f"effect={effect:02d} pc=<end> native=<group>")
            elif group_index >= len(native_groups):
                print(f"effect={effect:02d} pc=<group> native=<end>")
            else:
                pc_final = dict(canonical_group(pc_groups[group_index]))
                native_final = dict(canonical_group(native_groups[group_index]))
                address = next(
                    candidate for candidate in sorted(pc_final.keys() | native_final.keys())
                    if pc_final.get(candidate) != native_final.get(candidate)
                )
                pc_value = (
                    "<absent>" if address not in pc_final else f"{address:03X}:{pc_final[address]:02X}"
                )
                native_value = (
                    "<absent>" if address not in native_final else
                    f"{address:03X}:{native_final[address]:02X}"
                )
                print(
                    f"effect={effect:02d} pc={pc_value} native={native_value}"
                )
        if not duration_ok:
            print(f"effect={effect:02d} duration_out_of_tolerance=true")
        if status != "match":
            mismatched += 1
    print(
        f"status={'match' if mismatched == 0 else 'different'}\n"
        f"effects={len(effects)}\n"
        f"mismatched={mismatched}"
    )
    return 0 if mismatched == 0 else 1


def compare(reference: list[Register], candidate: list[Register]) -> int:
    for index, (left, right) in enumerate(zip(reference, candidate), start=1):
        if (left.address, left.value) != (right.address, right.value):
            print(
                "status=different\n"
                f"first_sequence={index}\n"
                f"reference={left.address:03X}:{left.value:02X}\n"
                f"candidate={right.address:03X}:{right.value:02X}\n"
                f"reference_writes={len(reference)}\n"
                f"candidate_writes={len(candidate)}"
            )
            return 1
    if len(reference) != len(candidate):
        # The matched prefix is still reported; the next sequence is the absent row.
        ended = "reference=<end>" if len(reference) < len(candidate) else \
            "candidate=<end>"
        print(
            "status=different\n"
            f"first_sequence={min(len(reference), len(candidate)) + 1}\n"
            f"{ended}\n"
            f"reference_writes={len(reference)}\n"
            f"candidate_writes={len(candidate)}"
        )
        return 1
    print(f"status=match\nregister_writes={len(reference)}")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Normalize PC-98 OPNA I/O or compare register sequences."
    )
    commands = parser.add_subparsers(dest="command", required=True)
    normalize_parser = commands.add_parser("normalize")
    normalize_parser.add_argument("--input", required=True, type=Path)
    normalize_parser.add_argument("--output", required=True, type=Path)
    compare_parser = commands.add_parser("compare")
    compare_parser.add_argument("--reference", required=True, type=Path)
    compare_parser.add_argument("--candidate", required=True, type=Path)
    effects_parser = commands.add_parser("compare-effects")
    effects_parser.add_argument("--markers", required=True, type=Path)
    effects_parser.add_argument("--pc-registers", required=True, type=Path)
    effects_parser.add_argument("--native-directory", required=True, type=Path)
    effects_parser.add_argument("--effect", action="append", type=int)
    effects_parser.add_argument("--clock-hz", type=int, default=OPNA_CLOCK_HZ)
    effects_parser.add_argument(
        "--duration-tolerance-percent", type=decimal.Decimal,
        default=decimal.Decimal("0.25"),
    )
    arguments = parser.parse_args()
    try:
        if arguments.command == "normalize":
            rows = normalize(load_raw(arguments.input))
            write_normalized(arguments.output, rows)
            print(f"status=complete\nregister_writes={len(rows)}")
            return 0
        if arguments.command == "compare":
            return compare(
                load_registers(arguments.reference),
                load_registers(arguments.candidate),
            )
        return compare_effects(
            arguments.markers,
            arguments.pc_registers,
            arguments.native_directory,
            arguments.effect or list(range(1, 23)),
            arguments.clock_hz,
            arguments.duration_tolerance_percent,
        )
    except TraceError as error:
        print(f"status=error\nstage=opna-trace\ndetail={error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
