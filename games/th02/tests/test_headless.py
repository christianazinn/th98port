"""Executable wiring checks with synthetic, empty stage assets (not parity)."""
import pathlib
import struct
import subprocess
import sys
import tempfile


def main():
    executable = pathlib.Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="th02-headless-") as temporary:
        data = pathlib.Path(temporary)
        header = bytearray(32)
        header[:6] = b"BFNT\x1a\x03"
        struct.pack_into("<HHHH", header, 8, 8, 8, 0, 0)
        sprite = header + bytes(32)
        for name in ("MIKO.BFT", "MIKO32.BFT", "MIKO16.BFT"):
            (data / name).write_bytes(sprite)
        # No templates or spawn rows; a long valid map keeps this test in the
        # stage portion for its short input sequence.
        body = struct.pack("<H", 320) + bytes(320) + struct.pack("<HH", 0, 0)
        for stage in range(6):
            for extension in ("BFT", "BMT", "BBT"):
                (data / f"STAGE{stage}.{extension}").write_bytes(sprite)
            (data / f"STAGE{stage}.MAP").write_bytes(bytes(3082))
            (data / f"STAGE{stage}.DT1").write_bytes(struct.pack("<I", len(body)) + body)
        inputs = data / "input.bin"
        inputs.write_bytes(bytes(64))

        def run(*options, success=True):
            result = subprocess.run([str(executable), "--data", str(data), *options],
                                    text=True, capture_output=True, timeout=20)
            assert (result.returncode == 0) == success, (result.returncode, result.stdout, result.stderr)
            return dict(line.split("=", 1) for line in result.stdout.splitlines() if "=" in line)

        for stage in range(6):
            result = run("--input", str(inputs), "--stage", str(stage))
            assert result["boundary"] == "input-exhausted", result
            assert result["stage-frame"] == "32", result
            assert result["input-consumed"] == "32", result
            assert result["rank"] == ("4" if stage == 5 else "1"), result
        inputs.write_bytes(struct.pack("<H", 64))
        result = run("--input", str(inputs))
        assert result["boundary"] == "pause" and result["stage-frame"] == "0", result
        # Demo input is loaded from its selected file, not the raw input path.
        for demo in range(1, 4):
            (data / f"DEMO{demo}.REC").write_bytes(struct.pack("<H", 64) + bytes(13998))
            result = run("--demo", str(demo))
            assert result["boundary"] == "pause" and result["stage-frame"] == "0", result
            assert result["stage"] == str((3, 2, 1)[demo - 1]), result
            assert result["shot"] == str((0, 2, 1)[demo - 1]), result
        run("--input", str(inputs), "--stage", "6", success=False)
        run("--input", str(inputs), "--power", "81", success=False)
        inputs.write_bytes(b"\0")
        run("--input", str(inputs), success=False)
        (data / "STAGE0.MAP").write_bytes(b"short")
        inputs.write_bytes(b"\0\0")
        run("--input", str(inputs), success=False)
    print("headless executable checks passed (synthetic assets)")


if __name__ == "__main__":
    main()
