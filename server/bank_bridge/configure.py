"""Generate private server/device config locally without printing secrets."""
import getpass
import json
import os
import re
import secrets
from pathlib import Path
from urllib.parse import urlparse
from cryptography.fernet import Fernet


def main():
    os.umask(0o077)
    if Path(".env").exists():
        raise SystemExit(".env already exists. Keep its encryption key; edit it locally to change configuration.")
    url = input("Public HTTPS bank bridge URL: ").strip().rstrip("/")
    parsed = urlparse(url)
    if parsed.scheme != "https" or not parsed.hostname or parsed.username or parsed.password or parsed.query or parsed.fragment:
        raise SystemExit("An HTTPS URL without credentials, query, or fragment is required.")
    client_id = getpass.getpass("Plaid Sandbox client ID (hidden): ").strip()
    secret = getpass.getpass("Plaid Sandbox secret (hidden): ").strip()
    if not all(re.fullmatch(r"[A-Za-z0-9_-]+", v) for v in (client_id, secret)):
        raise SystemExit("Invalid Plaid credentials format.")
    token = secrets.token_urlsafe(32)
    values = dict(PLAID_ENV="sandbox", PLAID_CLIENT_ID=client_id, PLAID_SECRET=secret,
                  BANK_ENCRYPTION_KEY=Fernet.generate_key().decode(), BANK_DEVICE_TOKEN=token,
                  BANK_OWNER_ID=secrets.token_hex(16), BANK_DB="private/banks.sqlite3")
    with open(".env", "x", encoding="utf-8") as stream:
        stream.write("\n".join(f"{k}={v}" for k, v in values.items()) + "\n")
    Path("private").mkdir(exist_ok=True)
    Path("private/bank-connection.json").write_text(json.dumps(dict(url=url, token=token)), encoding="utf-8")
    print("Created .env and private/bank-connection.json. Keep both private; see docs/BANK-LINKING.md.")


if __name__ == "__main__":
    main()
