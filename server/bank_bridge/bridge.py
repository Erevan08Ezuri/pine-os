"""Single-owner, read-only Plaid bridge. Never deploy without HTTPS and authentication."""
import copy
import hashlib
import json
import os
import secrets
import sqlite3
import threading
import time
import urllib.error
import urllib.request
from datetime import datetime, timezone
from decimal import Decimal, ROUND_HALF_UP
from pathlib import Path

import qrcode
from cryptography.fernet import Fernet


class BridgeError(Exception):
    def __init__(self, code, status=502):
        self.code, self.status = code, status
        super().__init__(code)


def utcnow():
    return datetime.now(timezone.utc).isoformat(timespec="seconds")


def minor(value, currency):
    # This first release supports the user's USD accounts only. Preserve missing balances.
    if currency != "USD":
        raise BridgeError("UNSUPPORTED_CURRENCY", 422)
    if value is None:
        return None
    value = Decimal(str(value))
    if not value.is_finite() or abs(value) > Decimal("1000000000000"):
        raise BridgeError("INVALID_AMOUNT")
    return int((value * 100).quantize(Decimal("1"), rounding=ROUND_HALF_UP))


class Store:
    """Encrypt tokens AND financial payloads before they reach SQLite, WAL, or backups."""
    def __init__(self, path, key):
        self.path, self.cipher = str(path), Fernet(key)
        Path(path).parent.mkdir(parents=True, exist_ok=True)
        with self.connect() as db:
            db.execute("CREATE TABLE IF NOT EXISTS records (kind TEXT, id TEXT, value BLOB, PRIMARY KEY(kind,id))")
        os.chmod(path, 0o600)
        # Fail at startup if the key was changed; never silently replace encrypted data.
        self.all("item")
        self.all("session")

    def connect(self):
        return sqlite3.connect(self.path, timeout=10)

    def all(self, kind):
        with self.connect() as db:
            return {key: json.loads(self.cipher.decrypt(value)) for key, value in
                    db.execute("SELECT id,value FROM records WHERE kind=?", (kind,))}

    def put(self, kind, key, value):
        self.put_many([(kind, key, value)])

    def put_many(self, records):
        rows = [(kind, key, self.cipher.encrypt(json.dumps(value, separators=(",", ":")).encode()))
                for kind, key, value in records]
        with self.connect() as db:
            db.executemany("INSERT OR REPLACE INTO records VALUES (?,?,?)", rows)

    def delete(self, kind, key):
        with self.connect() as db:
            db.execute("DELETE FROM records WHERE kind=? AND id=?", (kind, key))


class Plaid:
    def __init__(self, client_id, secret, environment):
        if environment not in ("sandbox", "production"):
            raise ValueError("PLAID_ENV must be sandbox or production")
        if not client_id or not secret:
            raise ValueError("Set PLAID_CLIENT_ID and PLAID_SECRET on the server")
        self.client_id, self.secret = client_id, secret
        self.environment = environment

    def call(self, endpoint, **payload):
        payload.update(client_id=self.client_id, secret=self.secret)
        request = urllib.request.Request(
            "https://" + self.environment + ".plaid.com" + endpoint,
            json.dumps(payload).encode(), {"Content-Type": "application/json", "Plaid-Version": "2020-09-14"})
        try:
            with urllib.request.urlopen(request, timeout=25) as response:
                return json.loads(response.read(8_000_000), parse_float=Decimal)
        except urllib.error.HTTPError as error:
            try:
                code = json.loads(error.read(65536)).get("error_code", "PROVIDER_ERROR")
            except (ValueError, TypeError):
                code = "PROVIDER_ERROR"
            # Do not expose request bodies, credentials, or raw provider error messages.
            raise BridgeError(code) from None
        except (OSError, ValueError):
            raise BridgeError("PROVIDER_UNAVAILABLE") from None


class Bridge:
    def __init__(self, store, plaid, owner_id):
        self.store, self.plaid, self.owner_id = store, plaid, owner_id
        self.lock = threading.Lock()
        identity = dict(environment=plaid.environment, owner=owner_id)
        saved = store.all("config").get("identity")
        if saved is not None and saved != identity:
            raise ValueError("Use a separate BANK_DB for each environment and owner")
        store.put("config", "identity", identity)

    def start_link(self, item_id=""):
        with self.lock:
            sessions = self.store.all("session")
            # Reuse the active link instead of creating duplicate bank Items on repeated taps.
            for session_id, session in sessions.items():
                if session["state"] == "waiting" and session["expires"] > time.time() and session["item_id"] == item_id:
                    return self.link_response(session_id, session)
            payload = dict(client_name="PineOS Finance", language="en", country_codes=["US"],
                           user={"client_user_id": self.owner_id}, hosted_link={"url_lifetime_seconds": 1800})
            if item_id:
                item = self.store.all("item").get(item_id)
                if not item:
                    raise BridgeError("ITEM_NOT_FOUND", 404)
                payload["access_token"] = item["access_token"]
            else:
                if len(self.store.all("item")) >= 10:
                    raise BridgeError("ITEM_LIMIT", 409)
                payload["products"] = ["transactions"]
            result = self.plaid.call("/link/token/create", **payload)
            session_id = secrets.token_urlsafe(24)
            session = dict(link_token=result["link_token"], url=result["hosted_link_url"],
                           expires=time.time() + 1800, state="waiting", item_id=item_id,
                           processed=[], error="")
            if not session["url"].startswith("https://secure.plaid.com/"):
                raise BridgeError("INVALID_LINK_URL")
            self.store.put("session", session_id, session)
            return self.link_response(session_id, session)

    @staticmethod
    def link_response(session_id, session):
        qr = qrcode.QRCode(border=4, error_correction=qrcode.constants.ERROR_CORRECT_M)
        qr.add_data(session["url"])
        qr.make(fit=True)
        return dict(session_id=session_id, state=session["state"], expires=session["expires"],
                    qr=["".join("1" if pixel else "0" for pixel in row) for row in qr.get_matrix()])

    def poll_links(self):
        with self.lock:
            for session_id, session in self.store.all("session").items():
                if session["state"] != "waiting":
                    if time.time() > session["expires"] + 86400:
                        self.store.delete("session", session_id)
                    continue
                if time.time() > session["expires"]:
                    session.update(state="expired", link_token="", url="")
                    self.store.put("session", session_id, session)
                    continue
                try:
                    response = self.plaid.call("/link/token/get", link_token=session["link_token"])
                    for attempt in response.get("link_sessions", []):
                        results = attempt.get("results") or {}
                        additions = results.get("item_add_results", [])
                        legacy = attempt.get("on_success")
                        if not additions and legacy and legacy.get("public_token"):
                            additions = [dict(public_token=legacy["public_token"], institution=(legacy.get("metadata") or {}).get("institution"))]
                        for result in ([] if session["item_id"] else additions):
                            public_token = result["public_token"]
                            fingerprint = hashlib.sha256(public_token.encode()).hexdigest()
                            if fingerprint in session["processed"]:
                                continue
                            # Save exchange before downstream retrieval; persisted exchanges are not repeated.
                            exchange = self.plaid.call("/item/public_token/exchange", public_token=public_token)
                            item_id = exchange["item_id"]
                            records = []
                            if item_id not in self.store.all("item"):
                                records.append(("item", item_id, dict(access_token=exchange["access_token"],
                                    institution=(result.get("institution") or {}).get("name", "Bank"),
                                    cursor="", accounts=[], transactions={}, synced_at="", error="", needs_sync=True)))
                            session["processed"].append(fingerprint)
                            records.append(("session", session_id, session))
                            self.store.put_many(records)
                        # Update mode retains the existing access token. HANDOFF is the successful Link event.
                        updated = session["item_id"] and attempt.get("finished_at") and (legacy is not None or any(
                            event.get("event_name") == "HANDOFF" for event in attempt.get("events", [])))
                        if updated:
                            item = self.store.all("item")[session["item_id"]]
                            status = self.plaid.call("/item/get", access_token=item["access_token"])["item"]
                            updated = status.get("error") is None
                            if updated:
                                item.update(needs_sync=True)
                                self.store.put("item", session["item_id"], item)
                        if session["processed"] or updated:
                            session.update(state="connected", error="", link_token="", url="")
                            self.store.put("session", session_id, session)
                            break
                except BridgeError as error:
                    session["error"] = error.code
                    self.store.put("session", session_id, session)

    def sync_item(self, item_id):
        with self.lock:
            original = self.store.all("item").get(item_id)
            if not original:
                return
            try:
                # All pages and cursor commit together. Restart pagination from the original cursor on mutation.
                for retry in range(3):
                    item = copy.deepcopy(original)
                    try:
                        for page in range(1000):
                            result = self.plaid.call("/transactions/sync", access_token=item["access_token"],
                                                     cursor=item["cursor"], count=500)
                            for row in result.get("removed", []):
                                item["transactions"].pop(row["transaction_id"], None)
                            for row in result.get("added", []) + result.get("modified", []):
                                currency = row.get("iso_currency_code")
                                if currency != "USD":
                                    continue
                                pending_id = row.get("pending_transaction_id")
                                if pending_id:
                                    item["transactions"].pop(pending_id, None)
                                item["transactions"][row["transaction_id"]] = dict(
                                    id=row["transaction_id"], account_id=row["account_id"],
                                    name=row.get("merchant_name") or row.get("name", "Transaction"),
                                    date=row["date"], amount_minor=minor(row["amount"], currency),
                                    currency=currency, pending=bool(row.get("pending")))
                            item["cursor"] = result["next_cursor"]
                            if not result["has_more"]:
                                break
                        else:
                            raise BridgeError("SYNC_PAGE_LIMIT")
                        # Cached balances: deliberately avoid unsupported Capital One credit-card refresh calls.
                        accounts = self.plaid.call("/accounts/get", access_token=item["access_token"])
                        item["accounts"] = []
                        for account in accounts["accounts"]:
                            balances = account["balances"]
                            currency = balances.get("iso_currency_code")
                            if currency != "USD":
                                continue
                            item["accounts"].append(dict(id=account["account_id"], name=account["name"],
                                mask=account.get("mask") or "", type=account["type"], currency=currency,
                                current_minor=minor(balances.get("current"), currency),
                                available_minor=minor(balances.get("available"), currency),
                                limit_minor=minor(balances.get("limit"), currency)))
                        allowed = {account["id"] for account in item["accounts"]}
                        item["transactions"] = {key: row for key, row in item["transactions"].items()
                                                if row["account_id"] in allowed}
                        item.update(synced_at=utcnow(), error="", needs_sync=False)
                        self.store.put("item", item_id, item)
                        return
                    except BridgeError as error:
                        if error.code != "TRANSACTIONS_SYNC_MUTATION_DURING_PAGINATION" or retry == 2:
                            raise
            except BridgeError as error:
                original["error"] = error.code
                self.store.put("item", item_id, original)

    def snapshot(self):
        items = []
        for item_id, item in self.store.all("item").items():
            items.append(dict(id=item_id, institution=item["institution"], accounts=item["accounts"],
                synced_at=item["synced_at"], error=item["error"],
                transactions=sorted(item["transactions"].values(), key=lambda t: (t["date"], t["id"]), reverse=True)[:30]))
        sessions = [dict(id=sid, state=s["state"], error=s["error"], expires=s["expires"])
                    for sid, s in self.store.all("session").items()]
        return dict(environment=self.plaid.environment, items=items, sessions=sessions)

    def disconnect(self, item_id):
        with self.lock:
            item = self.store.all("item").get(item_id)
            if not item:
                raise BridgeError("ITEM_NOT_FOUND", 404)
            self.plaid.call("/item/remove", access_token=item["access_token"])
            self.store.delete("item", item_id)
            for sid, session in self.store.all("session").items():
                if session["item_id"] == item_id:
                    self.store.delete("session", sid)


def create_app(bridge, device_token, run_worker=True):
    from contextlib import asynccontextmanager
    from fastapi import Depends, FastAPI, Header, HTTPException, Response
    from fastapi.responses import JSONResponse

    if len(device_token) < 32 or any(c.isspace() for c in device_token):
        raise ValueError("BANK_DEVICE_TOKEN must be a random token of at least 32 characters")
    stop, refresh = threading.Event(), threading.Event()

    def worker():
        last_sync = 0
        last_attempt = 0
        while not stop.is_set():
            try:
                bridge.poll_links()
                items = bridge.store.all("item")
                if time.monotonic() - last_attempt >= 60 and (refresh.is_set() or time.monotonic() - last_sync >= 3600 or any(x.get("needs_sync") for x in items.values())):
                    last_attempt = time.monotonic()
                    refresh.clear()
                    for item_id in items:
                        if stop.is_set():
                            break
                        bridge.sync_item(item_id)
                    last_sync = time.monotonic()
            except Exception:
                # No exception payload logging: provider payloads may contain financial data.
                print("Bank worker operation failed; will retry", flush=True)
            stop.wait(15)

    @asynccontextmanager
    async def lifespan(app):
        thread = threading.Thread(target=worker, daemon=True)
        if run_worker:
            thread.start()
        yield
        stop.set()
        if run_worker:
            thread.join(timeout=30)

    app = FastAPI(title="PineOS Bank Bridge", docs_url=None, redoc_url=None, openapi_url=None, lifespan=lifespan)

    def authorize(authorization: str = Header(default="")):
        if not secrets.compare_digest(authorization.encode(), ("Bearer " + device_token).encode()):
            raise HTTPException(401, "Unauthorized")

    @app.middleware("http")
    async def private_responses(request, call_next):
        response = await call_next(request)
        response.headers["Cache-Control"] = "no-store"
        response.headers["Referrer-Policy"] = "no-referrer"
        response.headers["X-Content-Type-Options"] = "nosniff"
        return response

    @app.exception_handler(BridgeError)
    async def bridge_error(request, error):
        return JSONResponse({"error": error.code}, status_code=error.status)

    @app.get("/v1/banks", dependencies=[Depends(authorize)])
    def snapshot():
        return bridge.snapshot()

    @app.post("/v1/banks/link", dependencies=[Depends(authorize)])
    def link():
        return bridge.start_link()

    @app.post("/v1/banks/{item_id}/reconnect", dependencies=[Depends(authorize)])
    def reconnect(item_id: str):
        return bridge.start_link(item_id)

    @app.post("/v1/banks/sync", dependencies=[Depends(authorize)])
    def sync(response: Response):
        refresh.set()
        response.status_code = 202
        return {"state": "queued"}

    @app.delete("/v1/banks/{item_id}", dependencies=[Depends(authorize)])
    def disconnect(item_id: str):
        bridge.disconnect(item_id)
        return {"state": "disconnected"}

    return app


def from_environment():
    os.umask(0o077)
    plaid = Plaid(os.environ["PLAID_CLIENT_ID"], os.environ["PLAID_SECRET"], os.environ.get("PLAID_ENV", "sandbox"))
    store = Store(os.environ.get("BANK_DB", "private/banks.sqlite3"), os.environ["BANK_ENCRYPTION_KEY"].encode())
    bridge = Bridge(store, plaid, os.environ.get("BANK_OWNER_ID", "pine-owner"))
    return create_app(bridge, os.environ["BANK_DEVICE_TOKEN"])
