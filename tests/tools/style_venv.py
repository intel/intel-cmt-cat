#!/usr/bin/env python3
"""
Helper to run commands in a local venv under tests/ without relying on global pipenv.

Key behaviors:
- Default (venv mode): reuse existing venv if present (--reuse recommended by Makefile)
- setup/setup-dev: create+install once, keep venv (--install-only --keep)
- style: ensure venv+deps, then run commands
- Optional cleanup: remove venv after run (--cleanup), controllable via STYLE_CLEAN=1 in Makefile
- External mode: if --python is provided, skip venv create/install/cleanup and run command directly
"""

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path


def run(cmd, **kw):
    return subprocess.check_call(cmd, **kw)


def build_requirements(py: Path, pipfile: Path) -> list:
    """
    Use toml in the venv to parse Pipfile and emit pip requirements from:
      [packages] + [dev-packages]
    """
    dump_script = f"""
import toml
p = toml.load("{pipfile.as_posix()}")

def emit(section):
    for name, spec in p.get(section, {{}}).items():
        if isinstance(spec, str):
            s = spec.strip()
            print(name if s in ("*", "") else f"{{name}}{{s}}")
        elif isinstance(spec, dict):
            v = spec.get("version", "*")
            print(name if (not v or v == "*") else f"{{name}}{{v}}")
        else:
            print(name)

emit("packages")
emit("dev-packages")
"""
    out = subprocess.check_output([str(py), "-c", dump_script], text=True)
    reqs = []
    seen = set()
    for line in out.splitlines():
        r = line.strip()
        if not r or r in seen:
            continue
        seen.add(r)
        reqs.append(r)
    return reqs


def ensure_venv(venv_dir: Path, pipfile: Path, reuse: bool):
    """
    Ensure venv exists (create if missing). If reuse is False, always recreate.
    Install deps from Pipfile (packages + dev-packages).
    """
    if venv_dir.exists() and not reuse:
        shutil.rmtree(venv_dir)

    if not venv_dir.exists():
        run([sys.executable, "-m", "venv", str(venv_dir)])

    py = venv_dir / "bin" / "python"

    # Ensure pip is present/up-to-date enough to install wheels.
    run([str(py), "-m", "pip", "install", "-U", "pip"])

    # toml is needed to parse Pipfile.
    run([str(py), "-m", "pip", "install", "toml"])

    reqs = build_requirements(py, pipfile)
    if reqs:
        run([str(py), "-m", "pip", "install", *reqs])


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--venv-dir", default=".style-venv")
    ap.add_argument("--pipfile", default="Pipfile")

    ap.add_argument(
        "--python",
        dest="external_python",
        default="",
        help="Use an existing Python interpreter; skip venv create/install/cleanup.",
    )

    ap.add_argument(
        "--reuse",
        action="store_true",
        help="Reuse existing venv if it exists (do not delete/recreate).",
    )
    ap.add_argument(
        "--keep",
        action="store_true",
        help="Do not delete venv at the end (even if --cleanup is set).",
    )
    ap.add_argument(
        "--cleanup",
        action="store_true",
        help="Delete venv at the end (ignored if --keep is set).",
    )
    ap.add_argument(
        "--install-only",
        action="store_true",
        help="Only ensure venv+deps and exit (no command execution).",
    )

    ap.add_argument("cmd", nargs=argparse.REMAINDER, help="Command to run (prefix with --)")
    args = ap.parse_args()

    # External python mode: do not touch any venv, just run command (if provided).
    if args.external_python:
        if args.install_only:
            return 0
        if not args.cmd or args.cmd[0] != "--":
            ap.error("Command must be provided after -- (or use --install-only).")
        cmd = args.cmd[1:]
        env = os.environ.copy()
        return run(cmd, env=env)

    venv_dir = Path(args.venv_dir)
    pipfile = Path(args.pipfile)

    # In non-external mode, always ensure deps are installed.
    ensure_venv(venv_dir, pipfile, reuse=args.reuse)

    if args.install_only:
        return 0

    if not args.cmd or args.cmd[0] != "--":
        ap.error("Command must be provided after -- (or use --install-only).")
    cmd = args.cmd[1:]

    # Run inside venv: prepend venv/bin to PATH so console scripts resolve correctly.
    env = os.environ.copy()
    env["VIRTUAL_ENV"] = str(venv_dir)
    env["PATH"] = f"{(venv_dir / 'bin').as_posix()}:{env.get('PATH', '')}"

    try:
        run(cmd, env=env)
    finally:
        # Cleanup only when requested and not kept.
        if args.cleanup and not args.keep and venv_dir.exists():
            shutil.rmtree(venv_dir)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
