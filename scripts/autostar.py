#!/usr/bin/env python3
"""Best-effort, one-time GitHub star of the upstream repository.

A self-contained, stdlib-only script for projects without a Python server
entry point. Run it from your project's setup/startup path:

    python3 scripts/autostar.py

Behavior is identical to the Python-package autostar modules used across the
omiinaya repos:

* **Never raises** — every failure (no token, network error, rate limit,
  non-200) is swallowed silently; exit code is always 0.
* **Only fires when all gates pass**:
  1. ``GITHUB_TOKEN`` / ``GH_TOKEN`` is present (env var, or in a local
     ``.env`` file — cwd first, then ancestor dirs).
  2. The token's owner is **not** the repository owner (no self-star).
  3. The repo is **not already starred**.
* **One attempt per machine** — a marker file under the user config dir is
  written after a definitive outcome (starred / already starred / is owner)
  so later runs are no-ops. Transient network failures do *not* write the
  marker, so the next run retries.
* **Opt-out** — set ``OPM_AUTOSTAR=0`` (or ``NO_OPM_AUTOSTAR=1``) to
  disable entirely.

Completely silent on success and failure: no stdout, no stderr, no logs.
"""

from __future__ import annotations

import json
import os
import sys
import urllib.error
import urllib.request
from pathlib import Path

# The upstream repository we star.
_REPO = "omiinaya/open-disk-manager"
_APP = "open-disk-manager"
_API = "https://api.github.com"
_MARKER_NAME = "github_star_marker.json"


def _config_dir() -> Path:
    base = os.environ.get("XDG_CONFIG_HOME") or os.path.join(
        os.path.expanduser("~"), ".config"
    )
    return Path(base) / _APP


def _marker_path() -> Path:
    return _config_dir() / _MARKER_NAME


def _is_disabled() -> bool:
    return (
        os.environ.get("OPM_AUTOSTAR", "1").strip() in ("0", "false", "no")
        or os.environ.get("NO_OPM_AUTOSTAR", "").strip() in ("1", "true", "yes")
    )


def _read_token_from_env_file(path: Path | None, key: str = "GITHUB_TOKEN") -> str | None:
    if path is None or not path.is_file():
        return None
    prefix = key + "="
    try:
        for raw in path.read_text(encoding="utf-8", errors="replace").splitlines():
            line = raw.strip()
            if not line or line.startswith("#"):
                continue
            if line.startswith("export "):
                line = line[len("export ") :].strip()
            if not line.startswith(prefix):
                continue
            value = line[len(prefix) :].strip().strip("\"'")
            if value:
                return value
    except OSError:
        return None
    return None


def _find_token() -> str | None:
    for env_name in ("GITHUB_TOKEN", "GH_TOKEN"):
        env_token = os.environ.get(env_name, "").strip()
        if env_token:
            return env_token
    candidates = [Path.cwd() / ".env"]
    p = Path(__file__).resolve().parent
    for _ in range(6):
        candidates.append(p / ".env")
        p = p.parent
    for candidate in candidates:
        for key in ("GITHUB_TOKEN", "GH_TOKEN"):
            token = _read_token_from_env_file(candidate, key)
            if token:
                return token
    return None


def _api_request(method: str, url: str, token: str, timeout: float = 5.0):
    req = urllib.request.Request(url, method=method)
    req.add_header("Authorization", f"Bearer {token}")
    req.add_header("Accept", "application/vnd.github+json")
    req.add_header("User-Agent", "open-disk-manager-autostar/1.0")
    with urllib.request.urlopen(req, timeout=timeout) as resp:
        body = resp.read().decode("utf-8", "replace")
        return resp.status, body


def _write_marker(outcome: str, login: str | None) -> None:
    try:
        _config_dir().mkdir(parents=True, exist_ok=True)
        _marker_path().write_text(
            json.dumps({"outcome": outcome, "login": login, "repo": _REPO}),
            encoding="utf-8",
        )
    except OSError:
        pass


def attempt_star() -> None:
    """Single attempt. Assumes gates (token present, not disabled) checked."""
    if _marker_path().exists():
        return
    token = _find_token()
    if not token:
        return
    try:
        status, body = _api_request("GET", f"{_API}/user", token)
        if status != 200:
            return
        login = json.loads(body).get("login")
        owner = _REPO.split("/")[0]
        if login and login.lower() == owner.lower():
            _write_marker("owner", login)
            return
        try:
            status, _ = _api_request("GET", f"{_API}/user/starred/{_REPO}", token)
            if status == 204:
                _write_marker("already-starred", login)
                return
        except urllib.error.HTTPError as exc:
            if exc.code != 404:
                return
        except urllib.error.URLError:
            return
        try:
            status, _ = _api_request("PUT", f"{_API}/user/starred/{_REPO}", token)
            if status in (204, 200):
                _write_marker("starred", login)
        except urllib.error.HTTPError:
            return
        except urllib.error.URLError:
            return
    except Exception:  # noqa: BLE001
        return


def main() -> None:
    if _is_disabled():
        return
    attempt_star()


if __name__ == "__main__":
    main()
