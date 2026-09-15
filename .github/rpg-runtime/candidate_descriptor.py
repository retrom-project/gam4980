#!/usr/bin/env python3
from __future__ import annotations
import argparse
import hashlib
import io
import json
import os
import stat
import subprocess
import tarfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

def git_bytes(*args: str) -> bytes:
    return subprocess.run(["git", "-C", str(ROOT), *args], capture_output=True, check=True).stdout

def digest(path: Path) -> str:
    with path.open("rb") as source:
        return hashlib.file_digest(source, "sha256").hexdigest()

def source_paths() -> list[bytes]:
    paths = git_bytes("ls-files", "--cached", "--others", "--exclude-standard", "-z")
    return sorted({raw for raw in paths.split(b"\0") if raw and os.path.lexists(ROOT / os.fsdecode(raw))})

def source_digest() -> str:
    records = []
    for raw in source_paths():
        relative = raw.decode()
        target = ROOT / relative
        info = target.lstat()
        if stat.S_ISLNK(info.st_mode):
            mode, sha = "120000", hashlib.sha256(os.readlink(target).encode()).hexdigest()
        elif stat.S_ISREG(info.st_mode):
            mode = "100755" if info.st_mode & stat.S_IXUSR else "100644"
            sha = digest(target)
        else:
            raise SystemExit("PFB_WORKTREE_INVALID")
        records.append({"mode": mode, "path": relative, "sha256": sha})
    return hashlib.sha256(json.dumps(records, ensure_ascii=False, separators=(",", ":"), sort_keys=True).encode()).hexdigest()

def checked_output(raw: str, empty: bool) -> Path:
    path = Path(raw)
    if not path.is_absolute() or path.is_symlink() or not path.is_dir():
        raise SystemExit("PFB_CANDIDATE_OUTPUT_INVALID")
    if empty and any(path.iterdir()):
        raise SystemExit("PFB_CANDIDATE_OUTPUT_INVALID")
    return path

def archive(output: Path) -> None:
    # Upstream carries games and firmware. They are not corresponding build source.
    roots = {"src", "tests", "docs", ".github", "build"}
    names = {"AGENTS.md", "COPYING", "README.md", ".gitignore", "retrom-fork.json", "Makefile.libretro"}
    with tarfile.open(output, "w", format=tarfile.PAX_FORMAT) as stream:
        for raw in source_paths():
            name = raw.decode()
            if name.split("/")[0] not in roots and name not in names:
                continue
            path = ROOT / name
            if not path.is_file() or path.is_symlink():
                raise SystemExit("PFB_WORKTREE_INVALID")
            contents = path.read_bytes()
            info = tarfile.TarInfo(name)
            info.size = len(contents)
            info.mode = 0o755 if path.stat().st_mode & stat.S_IXUSR else 0o644
            info.mtime = 0
            stream.addfile(info, io.BytesIO(contents))

def finalize(output: Path, core_id: str) -> None:
    fork = json.loads((ROOT / "retrom-fork.json").read_text())
    names = sorted(name for name in fork["releaseAssets"] if name != "rpg-runtime-release.json")
    if sorted(path.name for path in output.iterdir()) != names:
        raise SystemExit("PFB_CANDIDATE_OUTPUT_INVALID")
    files = []
    for name in names:
        path = output / name
        if not path.is_file() or path.is_symlink() or path.stat().st_size < 1:
            raise SystemExit("PFB_CANDIDATE_OUTPUT_INVALID")
        files.append({"filename": name, "sizeBytes": path.stat().st_size, "sha256": digest(path)})
    metadata = {
        "schemaVersion": 1, "kind": "RETROM_CORE_CANDIDATE_V1", "coreId": core_id,
        "repository": fork["forkRepository"], "adapterAbi": fork["adapterAbi"],
        "branch": git_bytes("symbolic-ref", "--quiet", "--short", "HEAD").decode().strip(),
        "commit": git_bytes("rev-parse", "HEAD").decode().strip(),
        "dirty": bool(git_bytes("status", "--porcelain=v1", "-z")),
        "sourceTreeSha256": source_digest(), "files": files,
    }
    (output / "retrom-core-candidate.json").write_text(json.dumps(metadata, separators=(",", ":"), sort_keys=True) + "\n")

parser = argparse.ArgumentParser()
parser.add_argument("action", choices=("prepare", "finalize", "digest", "archive"))
parser.add_argument("output")
parser.add_argument("--core-id")
args = parser.parse_args()
if args.action == "digest":
    print(source_digest())
elif args.action == "archive":
    archive(Path(args.output))
else:
    destination = checked_output(args.output, args.action == "prepare")
    if args.action == "finalize":
        if args.core_id != "gam4980":
            raise SystemExit("PFB_CANDIDATE_OUTPUT_INVALID")
        finalize(destination, args.core_id)
