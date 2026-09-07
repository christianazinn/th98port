#!/usr/bin/env python3
"""Validate New Classic source-presentation rules across locale catalogs."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

FORMAT = "T3LSP001"
AUTHOR_FORMAT = "T3GTL-AUTHOR-1"
PROFILE_KINDS = {
    "authenticated-original",
    "authenticated-english-patch",
    "catalog-rendered-from-english-patch",
    "generated-source-compatible-asset",
    "source-english-invariant",
}


class PolicyError(ValueError):
    pass


def load_json(path: Path) -> object:
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        raise PolicyError(f"{path}: {error}") from error


def load_policy(path: Path) -> tuple[tuple[str, ...], tuple[str, ...]]:
    raw = load_json(path)
    if not isinstance(raw, dict) or raw.get("format") != FORMAT or set(raw) != {
        "format", "source_english_invariants", "source_profiles"
    }:
        raise PolicyError("source-presentation policy has the wrong shape")
    invariants = raw["source_english_invariants"]
    profiles = raw["source_profiles"]
    if not isinstance(invariants, list) or not invariants or any(
        not isinstance(identifier, str) or not identifier for identifier in invariants
    ) or len(set(invariants)) != len(invariants):
        raise PolicyError("source-English invariants must be unique message IDs")
    if not isinstance(profiles, list) or not profiles:
        raise PolicyError("source profiles must be a nonempty array")
    surfaces: list[str] = []
    for index, profile in enumerate(profiles):
        if not isinstance(profile, dict) or set(profile) != {
            "surface", "japanese", "english", "other"
        }:
            raise PolicyError(f"source profile {index} has the wrong shape")
        surface = profile["surface"]
        if not isinstance(surface, str) or not surface:
            raise PolicyError(f"source profile {index} has no surface ID")
        for locale_class in ("japanese", "english", "other"):
            if profile[locale_class] not in PROFILE_KINDS:
                raise PolicyError(
                    f"source profile {surface} has invalid {locale_class} policy"
                )
        surfaces.append(surface)
    if len(set(surfaces)) != len(surfaces):
        raise PolicyError("source profile surface IDs must be unique")
    return tuple(invariants), tuple(surfaces)


def load_authors(localization: Path) -> dict[str, tuple[str, dict[str, str]]]:
    result: dict[str, tuple[str, dict[str, str]]] = {}
    for path in sorted(localization.glob("game-text-*.json")):
        raw = load_json(path)
        if not isinstance(raw, dict) or raw.get("format") != AUTHOR_FORMAT:
            continue
        if set(raw) != {"format", "locale", "fallback", "messages"}:
            raise PolicyError(f"{path}: game-text author has the wrong shape")
        locale = raw["locale"]
        fallback = raw["fallback"]
        messages = raw["messages"]
        if not isinstance(locale, str) or not locale or not isinstance(fallback, str) or \
                not isinstance(messages, dict) or any(
                    not isinstance(key, str) or not isinstance(value, str)
                    for key, value in messages.items()
                ):
            raise PolicyError(f"{path}: invalid game-text author")
        if locale in result:
            raise PolicyError(f"duplicate game-text locale {locale}")
        result[locale] = (fallback, messages)
    if "en-US" not in result:
        raise PolicyError("source-presentation validation requires en-US")
    return result


def resolve_message(
    authors: dict[str, tuple[str, dict[str, str]]],
    locale: str,
    identifier: str,
    chain: tuple[str, ...] = (),
) -> str:
    if locale in chain:
        raise PolicyError(f"game-text fallback cycle at {locale}")
    author = authors.get(locale)
    if author is None:
        raise PolicyError(f"missing game-text fallback locale {locale}")
    fallback, messages = author
    if identifier in messages:
        return messages[identifier]
    if not fallback:
        raise PolicyError(f"{locale}: missing source-English invariant {identifier}")
    return resolve_message(authors, fallback, identifier, chain + (locale,))


def validate(root: Path) -> tuple[int, int]:
    localization = root / "localization"
    invariants, surfaces = load_policy(
        localization / "source-presentation-policy.json"
    )
    authors = load_authors(localization)
    for identifier in invariants:
        reference = resolve_message(authors, "en-US", identifier)
        for locale in authors:
            if resolve_message(authors, locale, identifier) != reference:
                raise PolicyError(
                    f"{locale}: source-English invariant changed: {identifier}"
                )
    return len(invariants), len(surfaces)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path(__file__).parents[2])
    arguments = parser.parse_args()
    try:
        invariants, surfaces = validate(arguments.root.resolve())
    except PolicyError as error:
        print(f"status=error\nstage=source-presentation\ndetail={error}")
        return 1
    print(
        "status=ok\n"
        f"source-english-invariants={invariants}\n"
        f"source-profile-surfaces={surfaces}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
