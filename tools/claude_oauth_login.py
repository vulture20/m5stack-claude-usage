#!/usr/bin/env python3
"""Erzeugt per OAuth-Login (PKCE, wie `claude` beim manuellen Login) ein eigenes
Token-Paar fuer den M5Stack. Dadurch teilt sich das Geraet keinen Refresh-Token
mit Claude Code; beide koennen unabhaengig voneinander erneuern.

Ablauf: URL im Browser oeffnen, mit dem Claude-Konto anmelden, den angezeigten
Code hier einfuegen. Das ausgegebene JSON auf der Setup-Seite des Geraets unter
"...oder JSON einfuegen" eintragen.

Nur Python-Standardbibliothek.
"""
import argparse
import base64
import hashlib
import json
import secrets
import sys
import time
import urllib.error
import urllib.parse
import urllib.request

CLIENT_ID = "9d1c250a-e61b-44d9-88ed-5944d1962f5e"
AUTHORIZE_URL = "https://claude.ai/oauth/authorize"
TOKEN_URL = "https://platform.claude.com/v1/oauth/token"
REDIRECT_URI = "https://platform.claude.com/oauth/code/callback"
SCOPES = "org:create_api_key user:profile user:inference"
USAGE_URL = "https://api.anthropic.com/api/oauth/usage"


def b64url(data: bytes) -> str:
    return base64.urlsafe_b64encode(data).rstrip(b"=").decode()


def post_json(url: str, body: dict) -> dict:
    req = urllib.request.Request(
        url, data=json.dumps(body).encode(), method="POST",
        headers={"Content-Type": "application/json", "Accept": "application/json",
                 "User-Agent": "m5stack-claude-usage-login/1.0"})
    try:
        with urllib.request.urlopen(req, timeout=20) as r:
            return json.load(r)
    except urllib.error.HTTPError as e:
        sys.exit(f"Token-Abruf fehlgeschlagen: HTTP {e.code}: {e.read().decode(errors='replace')[:400]}")


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--authorize-url", default=AUTHORIZE_URL)
    ap.add_argument("--token-url", default=TOKEN_URL)
    ap.add_argument("--redirect-uri", default=REDIRECT_URI)
    ap.add_argument("--client-id", default=CLIENT_ID)
    ap.add_argument("--scope", default=SCOPES)
    ap.add_argument("--no-verify", action="store_true", help="Usage-Endpunkt nicht testweise abfragen")
    a = ap.parse_args()

    verifier = b64url(secrets.token_bytes(32))
    challenge = b64url(hashlib.sha256(verifier.encode()).digest())
    state = b64url(secrets.token_bytes(32))
    params = {
        "code": "true",
        "client_id": a.client_id,
        "response_type": "code",
        "redirect_uri": a.redirect_uri,
        "scope": a.scope,
        "code_challenge": challenge,
        "code_challenge_method": "S256",
        "state": state,
    }
    print("1. Diese URL im Browser oeffnen und anmelden:\n")
    print(a.authorize_url + "?" + urllib.parse.urlencode(params, quote_via=urllib.parse.quote))
    print()
    raw = input("2. Angezeigten Code hier einfuegen: ").strip()
    code, _, got_state = raw.partition("#")
    if got_state and got_state != state:
        sys.exit("State stimmt nicht ueberein - bitte neu starten.")

    tok = post_json(a.token_url, {
        "grant_type": "authorization_code",
        "code": code,
        "state": got_state or state,
        "client_id": a.client_id,
        "redirect_uri": a.redirect_uri,
        "code_verifier": verifier,
    })
    if "access_token" not in tok:
        sys.exit(f"Unerwartete Antwort: {json.dumps(tok)[:400]}")

    out = {
        "accessToken": tok["access_token"],
        "refreshToken": tok.get("refresh_token"),
        "expiresAt": int((time.time() + tok.get("expires_in", 0)) * 1000) if tok.get("expires_in") else 0,
    }

    if not a.no_verify:
        req = urllib.request.Request(USAGE_URL, headers={
            "Authorization": "Bearer " + out["accessToken"], "anthropic-beta": "oauth-2025-04-20",
            "Accept": "application/json", "User-Agent": "m5stack-claude-usage-login/1.0"})
        try:
            with urllib.request.urlopen(req, timeout=20) as r:
                usage = json.load(r)
            limits = {k: v.get("utilization") for k, v in usage.items()
                      if isinstance(v, dict) and v.get("utilization") is not None}
            print(f"\nTest der Usage-Abfrage erfolgreich: {limits}", file=sys.stderr)
        except urllib.error.HTTPError as e:
            print(f"\nWARNUNG: Usage-Abfrage mit dem neuen Token fehlgeschlagen (HTTP {e.code}): "
                  f"{e.read().decode(errors='replace')[:300]}", file=sys.stderr)

    print("\n3. Dieses JSON auf der Setup-Seite unter '...oder JSON einfuegen' eintragen:\n", file=sys.stderr)
    print(json.dumps(out))


if __name__ == "__main__":
    main()
