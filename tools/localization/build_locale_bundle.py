#!/usr/bin/env python3
"""Build and independently verify deterministic New Classic locale bundles."""

from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import shutil
import subprocess
import sys
import tempfile
import uuid
from pathlib import Path
from types import ModuleType


FORMAT = "T3LCB001"
COMPLETE_LOCALES = ("en-US", "ja-JP")
MENU_BACKGROUND_NAMES = tuple(
    ["BG2A.BMP", "BG2B.BMP"]
    + [
        f"M{index:03}{half}.BMP"
        for index in range(1, 8)
        for half in ("A", "B")
    ]
)
ROOT = Path(__file__).resolve().parents[2]


class BundleError(RuntimeError):
    pass


def load_module(name: str, filename: str) -> ModuleType:
    path = Path(__file__).with_name(filename)
    specification = importlib.util.spec_from_file_location(name, path)
    if specification is None or specification.loader is None:
        raise BundleError(f"unable to load compiler {path.name}")
    module = importlib.util.module_from_spec(specification)
    sys.modules[specification.name] = module
    specification.loader.exec_module(module)
    return module


LANGUAGE = load_module("th03_bundle_language", "compile_language_pack.py")
GAME_TEXT = load_module("th03_bundle_game_text", "compile_game_text_pack.py")
CUTSCENE = load_module("th03_bundle_cutscene", "compile_cutscene_text_pack.py")


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest().upper()


def file_identity(path: Path, relative: str) -> dict[str, object]:
    try:
        data = path.read_bytes()
    except OSError as error:
        raise BundleError(f"cannot read {path}: {error}") from error
    return {"path": relative, "bytes": len(data), "sha256": sha256(data)}


def canonical_json(value: object) -> bytes:
    return (json.dumps(
        value, ensure_ascii=False, sort_keys=True, indent=2
    ) + "\n").encode("utf-8")


def locale_paths(locale: str) -> dict[str, Path]:
    return {
        "language_schema": ROOT / "localization/schema.json",
        "language_source": ROOT / f"localization/{locale}.json",
        "game_text_schema": ROOT / "localization/game-text-schema.json",
        "game_text_source": ROOT / f"localization/game-text-{locale}.json",
        "cutscene_schema": ROOT / f"localization/cutscene-text-{locale}-schema.json",
        "cutscene_source": ROOT / f"localization/cutscene-text-{locale}.json",
    }


def source_record(domain: str, schema: Path, source: Path) -> dict[str, object]:
    return {
        "domain": domain,
        "schema": file_identity(schema, f"localization/{schema.name}"),
        "source": file_identity(source, f"localization/{source.name}"),
    }


def run_preview(tool: Path, font: Path, pack: Path, output: Path) -> None:
    try:
        completed = subprocess.run(
            [
                str(tool), "--font", str(font), "--output", str(output),
                "--game-text-pack", str(pack),
            ],
            text=True,
            capture_output=True,
            check=False,
        )
    except OSError as error:
        raise BundleError(f"cannot run preview renderer: {error}") from error
    if completed.returncode != 0:
        detail = completed.stderr.strip() or completed.stdout.strip()
        raise BundleError(
            f"preview renderer failed with code {completed.returncode}: {detail}"
        )


def run_menu_preview(
    tool: Path,
    font: Path,
    pack: Path,
    output: Path,
    menu_font: Path | None,
    replay_patch_assets: Path | None,
    menu_backgrounds: Path | None,
) -> None:
    command = [
        str(tool), "--font", str(font), "--output", str(output),
        "--language-pack", str(pack),
    ]
    if menu_font is not None:
        command.extend(("--menu-font", str(menu_font)))
    else:
        assert replay_patch_assets is not None and menu_backgrounds is not None
        command.extend((
            "--replay-patch-assets", str(replay_patch_assets),
            "--menu-backgrounds", str(menu_backgrounds),
        ))
    try:
        completed = subprocess.run(
            command, text=True, capture_output=True, check=False
        )
    except OSError as error:
        raise BundleError(f"cannot run menu preview renderer: {error}") from error
    if completed.returncode != 0:
        detail = completed.stderr.strip() or completed.stdout.strip()
        raise BundleError(
            f"menu preview renderer failed with code {completed.returncode}: {detail}"
        )


def build_locale(
    locale: str,
    root: Path,
    font: Path,
    preview_tool: Path,
    menu_preview_tool: Path,
    menu_font: Path | None,
    replay_patch_assets: Path | None,
    menu_backgrounds: Path | None,
) -> dict[str, object]:
    paths = locale_paths(locale)
    missing = [path for path in paths.values() if not path.is_file()]
    if missing:
        raise BundleError(f"{locale} is incomplete: missing {missing[0]}")
    output = root / locale
    output.mkdir(parents=True)

    try:
        language_schema = LANGUAGE.load_schema(paths["language_schema"])
        language_author = LANGUAGE.load_author(
            paths["language_source"], language_schema
        )
        language_pack = LANGUAGE.compile_pack(language_schema, language_author)
        game_schema = GAME_TEXT.load_schema(paths["game_text_schema"])
        game_author = GAME_TEXT.load_author(paths["game_text_source"], game_schema)
        game_pack = GAME_TEXT.compile_pack(game_schema, game_author)
        cutscene_catalog = CUTSCENE.load_catalog(
            paths["cutscene_schema"], paths["cutscene_source"]
        )
        cutscene_pack = CUTSCENE.compile_pack(cutscene_catalog)
    except (LANGUAGE.PackError, GAME_TEXT.PackError, CUTSCENE.PackError) as error:
        raise BundleError(f"{locale}: {error}") from error

    language_output = output / f"{locale}.t3lang"
    game_output = output / f"game-text-{locale}.t3gtl"
    cutscene_output = output / f"cutscene-text-{locale}.t3ctp"
    language_output.write_bytes(language_pack)
    game_output.write_bytes(game_pack)
    cutscene_output.write_bytes(cutscene_pack)
    preview_output = output / "preview"
    run_preview(preview_tool, font, game_output, preview_output)

    menu_preview_output = output / "menu-preview"
    run_menu_preview(
        menu_preview_tool,
        font,
        language_output,
        menu_preview_output,
        menu_font,
        replay_patch_assets,
        menu_backgrounds,
    )

    report_path = preview_output / "preview.json"
    try:
        report = json.loads(report_path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        raise BundleError(f"{locale}: invalid preview report: {error}") from error
    messages = report.get("messages") if isinstance(report, dict) else None
    if (
        not isinstance(report, dict)
        or report.get("format") != "T3GTP001"
        or report.get("locale") != locale
        or not isinstance(messages, list)
        or len(messages) != len(game_schema.messages)
    ):
        raise BundleError(f"{locale}: preview report does not cover the compiled pack")
    if any(
        not isinstance(entry, dict) or not isinstance(entry.get("page"), str)
        for entry in messages
    ):
        raise BundleError(f"{locale}: preview report has an invalid page identity")
    expected_pages = {entry["page"] for entry in messages}
    actual_pages = {path.stem for path in preview_output.glob("*.ppm")}
    if len(expected_pages) == 0 or actual_pages != expected_pages:
        raise BundleError(f"{locale}: preview pages do not match the bounds report")

    menu_report_path = menu_preview_output / "preview.json"
    try:
        menu_report = json.loads(menu_report_path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        raise BundleError(f"{locale}: invalid menu preview report: {error}") from error
    menu_messages = (
        menu_report.get("messages") if isinstance(menu_report, dict) else None
    )
    if (
        not isinstance(menu_report, dict)
        or menu_report.get("format") != "T3MTP001"
        or menu_report.get("locale") != locale
        or not isinstance(menu_messages, list)
        or len(menu_messages) != len(language_schema.messages)
        or any(
            not isinstance(entry, dict) or not isinstance(entry.get("page"), str)
            for entry in menu_messages
        )
    ):
        raise BundleError(
            f"{locale}: menu preview report does not cover the compiled pack"
        )
    expected_menu_pages = {entry["page"] for entry in menu_messages}
    actual_menu_pages = {
        path.stem for path in menu_preview_output.glob("*.ppm")
    }
    if len(expected_menu_pages) == 0 or actual_menu_pages != expected_menu_pages:
        raise BundleError(
            f"{locale}: menu preview pages do not match the bounds report"
        )

    artifacts = [
        file_identity(path, path.relative_to(root).as_posix())
        for path in sorted(
            (candidate for candidate in output.rglob("*") if candidate.is_file()),
            key=lambda candidate: candidate.relative_to(root).as_posix(),
        )
    ]
    cutscene_runs = sum(len(script.runs) for script in cutscene_catalog.scripts)
    cutscene_glyphs = sum(
        run.glyphs
        for script in cutscene_catalog.scripts
        for run in script.runs
    )
    return {
        "locale": locale,
        "counts": {
            "language_messages": len(language_schema.messages),
            "game_text_messages": len(game_schema.messages),
            "cutscene_scripts": len(cutscene_catalog.scripts),
            "cutscene_runs": cutscene_runs,
            "cutscene_glyphs": cutscene_glyphs,
            "preview_pages": len(actual_pages),
            "menu_preview_pages": len(actual_menu_pages),
        },
        "inputs": [
            source_record(
                "language", paths["language_schema"], paths["language_source"]
            ),
            source_record(
                "game-text", paths["game_text_schema"], paths["game_text_source"]
            ),
            source_record(
                "cutscene", paths["cutscene_schema"], paths["cutscene_source"]
            ),
        ],
        "artifacts": artifacts,
    }


def build_staging(
    root: Path,
    locales: tuple[str, ...],
    font: Path,
    preview_tool: Path,
    menu_preview_tool: Path,
    menu_font: Path | None,
    replay_patch_assets: Path | None,
    menu_backgrounds: Path | None,
) -> None:
    if not font.is_file():
        raise BundleError(f"FONT.ROM is not a file: {font}")
    if font.stat().st_size != 288_768:
        raise BundleError("FONT.ROM must contain exactly 288768 bytes")
    if not preview_tool.is_file():
        raise BundleError(f"preview renderer is not a file: {preview_tool}")
    if not menu_preview_tool.is_file():
        raise BundleError(f"menu preview renderer is not a file: {menu_preview_tool}")
    if (menu_font is None) == (replay_patch_assets is None):
        raise BundleError("select exactly one menu-font source")
    if menu_font is not None and not menu_font.is_file():
        raise BundleError(f"menu font is not a file: {menu_font}")
    if replay_patch_assets is not None and not replay_patch_assets.is_file():
        raise BundleError(
            f"Replay Patch assets are not a file: {replay_patch_assets}"
        )
    if replay_patch_assets is not None and (
        menu_backgrounds is None or not menu_backgrounds.is_dir()
    ):
        raise BundleError("Replay Patch assets require a menu-background directory")
    if menu_backgrounds is not None:
        for name in MENU_BACKGROUND_NAMES:
            path = menu_backgrounds / name
            if not path.is_file() or path.stat().st_size != 64_118:
                raise BundleError(
                    f"menu-background input is missing or malformed: {path}"
                )
    root.mkdir(parents=True)
    records = [
        build_locale(
            locale,
            root,
            font,
            preview_tool,
            menu_preview_tool,
            menu_font,
            replay_patch_assets,
            menu_backgrounds,
        )
        for locale in locales
    ]
    menu_source = (
        file_identity(menu_font, "MNUFONT.PF")
        if menu_font is not None
        else file_identity(replay_patch_assets, "AZINN.DAT")
    )
    background_inputs = []
    if menu_backgrounds is not None:
        background_inputs = [
            file_identity(path, f"PODDUI/{path.name}")
            for path in (menu_backgrounds / name for name in MENU_BACKGROUND_NAMES)
        ]
    manifest = {
        "format": FORMAT,
        "locales": records,
        "external_inputs": {
            "font_rom": file_identity(font, "FONT.ROM"),
            "preview_renderer": file_identity(preview_tool, preview_tool.name),
            "menu_preview_renderer": file_identity(
                menu_preview_tool, menu_preview_tool.name
            ),
            "menu_font_source": menu_source,
            "menu_backgrounds": background_inputs,
        },
    }
    (root / "manifest.json").write_bytes(canonical_json(manifest))


def differences(expected: Path, candidate: Path) -> list[str]:
    expected_files = {
        path.relative_to(expected).as_posix(): path
        for path in expected.rglob("*") if path.is_file()
    }
    candidate_files = {
        path.relative_to(candidate).as_posix(): path
        for path in candidate.rglob("*") if path.is_file()
    }
    expected_names = expected_files.keys()
    candidate_names = candidate_files.keys()
    result = [
        f"missing:{name}" for name in sorted(expected_names - candidate_names)
    ]
    result.extend(
        f"extra:{name}" for name in sorted(candidate_names - expected_names)
    )
    for name in sorted(expected_files.keys() & candidate_files.keys()):
        if expected_files[name].read_bytes() != candidate_files[name].read_bytes():
            result.append(f"changed:{name}")
    return result


def publish(staging: Path, output: Path) -> bool:
    if output.exists() and not output.is_dir():
        raise BundleError(f"output exists and is not a directory: {output}")
    if output.exists() and not differences(staging, output):
        return False
    output.parent.mkdir(parents=True, exist_ok=True)
    backup = output.with_name(f".{output.name}.old-{uuid.uuid4().hex}")
    if output.exists():
        output.rename(backup)
    try:
        staging.rename(output)
    except BaseException:
        if backup.exists() and not output.exists():
            backup.rename(output)
        raise
    if backup.exists():
        shutil.rmtree(backup)
    return True


def selected_locales(values: list[str] | None) -> tuple[str, ...]:
    chosen = tuple(values or COMPLETE_LOCALES)
    if len(set(chosen)) != len(chosen):
        raise BundleError("each locale may be selected only once")
    unsupported = [locale for locale in chosen if locale not in COMPLETE_LOCALES]
    if unsupported:
        raise BundleError(
            f"{unsupported[0]} does not have all three production domains"
        )
    return tuple(sorted(chosen))


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Compile, preview, and verify complete New Classic locales."
    )
    parser.add_argument("command", choices=("build", "check"))
    parser.add_argument("--locale", action="append", choices=COMPLETE_LOCALES)
    parser.add_argument("--font", required=True, type=Path)
    parser.add_argument("--preview-tool", required=True, type=Path)
    parser.add_argument("--menu-preview-tool", required=True, type=Path)
    menu_source = parser.add_mutually_exclusive_group(required=True)
    menu_source.add_argument("--menu-font", type=Path)
    menu_source.add_argument("--replay-patch-assets", type=Path)
    parser.add_argument("--menu-backgrounds", type=Path)
    parser.add_argument("--output", required=True, type=Path)
    arguments = parser.parse_args()
    try:
        locales = selected_locales(arguments.locale)
        output = arguments.output.resolve()
        menu_font = arguments.menu_font.resolve() if arguments.menu_font else None
        replay_patch_assets = (
            arguments.replay_patch_assets.resolve()
            if arguments.replay_patch_assets else None
        )
        menu_backgrounds = (
            arguments.menu_backgrounds.resolve()
            if arguments.menu_backgrounds else None
        )
        output.parent.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(
            prefix="th03-locale-bundle-", dir=output.parent
        ) as temporary:
            staging = Path(temporary) / "bundle"
            build_staging(
                staging,
                locales,
                arguments.font.resolve(),
                arguments.preview_tool.resolve(),
                arguments.menu_preview_tool.resolve(),
                menu_font,
                replay_patch_assets,
                menu_backgrounds,
            )
            if arguments.command == "check":
                if not output.is_dir():
                    raise BundleError(f"candidate bundle is not a directory: {output}")
                changed = differences(staging, output)
                if changed:
                    for entry in changed:
                        print(entry)
                    raise BundleError(f"candidate differs in {len(changed)} file(s)")
                print(f"status=complete\noperation=check\nlocales={len(locales)}")
            else:
                changed = publish(staging, output)
                print(
                    "status=complete\noperation=build\n"
                    f"published={'true' if changed else 'false'}\n"
                    f"locales={len(locales)}"
                )
    except (BundleError, OSError) as error:
        print(f"status=error\nstage=locale-bundle\ndetail={error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
