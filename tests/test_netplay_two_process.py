#!/usr/bin/env python3
"""Hermetic two-process acceptance for the frozen New Classic netplay wires."""

from __future__ import annotations

import hashlib
import os
import subprocess
import sys
from dataclasses import dataclass


@dataclass
class Peer:
    process: subprocess.Popen[str]
    role: str

    @classmethod
    def start(cls, executable: str, role: str) -> "Peer":
        creationflags = 0x08000000 if os.name == "nt" else 0
        process = subprocess.Popen(
            [executable, role],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            encoding="ascii",
            bufsize=1,
            creationflags=creationflags,
        )
        return cls(process=process, role=role)

    def query(self, command: str) -> str:
        if self.process.stdin is None or self.process.stdout is None:
            raise AssertionError(f"{self.role}: missing process pipe")
        if self.process.poll() is not None:
            raise AssertionError(self.failure("exited before command"))
        self.process.stdin.write(command + "\n")
        self.process.stdin.flush()
        response = self.process.stdout.readline()
        if not response:
            raise AssertionError(self.failure(f"no response to {command!r}"))
        return response.rstrip("\r\n")

    def failure(self, message: str) -> str:
        error = ""
        if self.process.stderr is not None and self.process.poll() is not None:
            error = self.process.stderr.read()
        return f"{self.role}: {message}; code={self.process.poll()} stderr={error!r}"

    def close(self) -> None:
        if self.process.poll() is None:
            try:
                assert self.query("EXIT") == "BYE"
                self.process.wait(timeout=5)
            except (AssertionError, subprocess.TimeoutExpired):
                self.process.kill()
                self.process.wait(timeout=5)
        if self.process.returncode != 0:
            raise AssertionError(self.failure("worker did not exit cleanly"))


def wire(response: str, byte_count: int, transcript: list[bytes]) -> str:
    prefix = "WIRE "
    assert response.startswith(prefix), response
    value = response[len(prefix) :]
    assert len(value) == byte_count * 2, (len(value), byte_count * 2)
    payload = bytes.fromhex(value)
    transcript.append(payload)
    return value


def expect(peer: Peer, command: str, expected: str) -> None:
    actual = peer.query(command)
    assert actual == expected, (peer.role, command, expected, actual)


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: test_netplay_two_process.py <peer-worker>", file=sys.stderr)
        return 2
    executable = os.path.abspath(sys.argv[1])
    assert os.path.isfile(executable), executable
    host = Peer.start(executable, "host")
    join = Peer.start(executable, "join")
    transcript: list[bytes] = []
    try:
        expect(host, "OUTBOUND", "NONE")
        request = wire(join.query("OUTBOUND"), 64, transcript)
        expect(host, f"RECEIVE {request}", "LOBBY 1")
        offer = wire(host.query("OUTBOUND"), 64, transcript)
        expect(join, f"RECEIVE {offer}", "LOBBY 2")
        ready = wire(join.query("OUTBOUND"), 64, transcript)
        expect(host, f"RECEIVE {ready}", "LOBBY 3")
        accepted = wire(host.query("OUTBOUND"), 64, transcript)
        expect(join, f"RECEIVE {accepted}", "LOBBY 4")

        host_contract = host.query("CONTRACT")
        join_contract = join.query("CONTRACT")
        assert host_contract == join_contract
        assert host_contract.endswith(" 4"), host_contract

        p1_input = wire(host.query("MAKE_INPUT 0 32 1 0"), 104, transcript)
        expect(join, f"RECEIVE {p1_input}", "TYPE 3 FRAME 0 WORD 32 STATUS 255")
        p2_input = wire(join.query("MAKE_INPUT 0 4 0 0"), 104, transcript)
        expect(host, f"RECEIVE {p2_input}", "TYPE 3 FRAME 0 WORD 4 STATUS 255")

        proof = wire(host.query("MAKE_PROOF 120 120 1 0 1"), 104, transcript)
        expect(join, f"RECEIVE {proof}", "TYPE 5 FRAME 120 WORD 0 STATUS 255")
        proof_ack = wire(join.query("ACK_LAST"), 104, transcript)
        expect(host, f"RECEIVE {proof_ack}", "TYPE 6 FRAME 120 WORD 0 STATUS 255")

        for peer in (host, join):
            expect(peer, "PAUSE_CREATE 121 120", "STATUS 0")
            expect(peer, "PAUSE_RELEASE", "STATUS 1")
            expect(peer, "PAUSE_RESUME", "STATUS 2")
        host_ready = wire(host.query("PAUSE_WIRE"), 104, transcript)
        join_ready = wire(join.query("PAUSE_WIRE"), 104, transcript)
        expect(join, f"RECEIVE {host_ready}", "TYPE 8 FRAME 121 WORD 0 STATUS 3")
        expect(host, f"RECEIVE {join_ready}", "TYPE 8 FRAME 121 WORD 0 STATUS 3")

        for peer in (host, join):
            expect(peer, "PAUSE_CREATE 240 240", "STATUS 0")
            expect(peer, "PAUSE_RELEASE", "STATUS 1")
        expect(host, "PAUSE_FORFEIT", "STATUS 4")
        forfeit = wire(host.query("PAUSE_WIRE"), 104, transcript)
        expect(join, f"RECEIVE {forfeit}", "TYPE 9 FRAME 240 WORD 0 STATUS 5")
        forfeit_ack = wire(join.query("PAUSE_WIRE"), 104, transcript)
        expect(host, f"RECEIVE {forfeit_ack}", "TYPE 10 FRAME 240 WORD 0 STATUS 7")

        terminal = wire(join.query("MAKE_PROOF 500 500 2 3 64"), 104, transcript)
        expect(host, f"RECEIVE {terminal}", "TYPE 5 FRAME 500 WORD 0 STATUS 7")
        terminal_ack = wire(host.query("ACK_LAST"), 104, transcript)
        expect(join, f"RECEIVE {terminal_ack}", "TYPE 6 FRAME 500 WORD 0 STATUS 5")
        disconnect = wire(host.query("MAKE_DISCONNECT"), 104, transcript)
        expect(join, f"RECEIVE {disconnect}", "TYPE 4 FRAME 0 WORD 0 STATUS 5")

        digest = hashlib.sha256(b"".join(transcript)).hexdigest().upper()
        expected_digest = "79AA0569321FD587A47DBBFF0319DE284F1D6674D41BA56D1E32D0EF57F5A290"
        assert digest == expected_digest, digest
    finally:
        host.close()
        join.close()
    print(f"two-process-netplay=accepted transcript={digest}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
