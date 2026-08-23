#!/usr/bin/env python3
"""Normalize PC-98 OPNA port I/O and compare exact register sequences."""

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


class TraceError(RuntimeError):
    pass


@dataclass(frozen=True)
class Register:
    sequence: int
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
    arguments = parser.parse_args()
    try:
        if arguments.command == "normalize":
            rows = normalize(load_raw(arguments.input))
            write_normalized(arguments.output, rows)
            print(f"status=complete\nregister_writes={len(rows)}")
            return 0
        return compare(
            load_registers(arguments.reference),
            load_registers(arguments.candidate),
        )
    except TraceError as error:
        print(f"status=error\nstage=opna-trace\ndetail={error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
