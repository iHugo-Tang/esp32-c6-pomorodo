#!/usr/bin/env python3
"""Check flash argument forwarding without connecting to a device."""

import os
from pathlib import Path
import subprocess
import tempfile

script = Path(__file__).resolve().with_name("flash.sh")
with tempfile.TemporaryDirectory() as directory:
    fake_idf = Path(directory) / "idf.py"
    fake_idf.write_text('#!/bin/sh\nprintf "%s\\n" "$@"\nexit "${FLASH_EXIT:-0}"\n')
    fake_idf.chmod(0o755)
    environment = dict(os.environ, PATH=f"{directory}:{os.environ['PATH']}")
    environment.pop("POMODORO_PORT", None)
    environment.pop("FLASH_EXIT", None)

    def run(arguments, **overrides):
        return subprocess.run(
            ["bash", str(script), *arguments],
            env=dict(environment, **overrides), capture_output=True, text=True,
        )

    result = run([])
    assert result.returncode == 0 and result.stdout.splitlines() == ["flash"]
    result = run(["--baud", "460800"], POMODORO_PORT="/dev/timer port")
    assert result.stdout.splitlines() == [
        "-p", "/dev/timer port", "--baud", "460800", "flash",
    ]
    for arguments in [["-p", "/dev/explicit"], ["--port", "/dev/explicit"],
                      ["--port=/dev/explicit"]]:
        result = run(arguments, POMODORO_PORT="/dev/ignored")
        assert result.returncode == 0
        assert result.stdout.splitlines() == [*arguments, "flash"]
    assert run([], FLASH_EXIT="7").returncode == 7

print("flash check: all checks passed")
