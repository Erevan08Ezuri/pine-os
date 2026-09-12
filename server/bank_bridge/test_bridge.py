import copy
import json
import tempfile
import time
import unittest
from pathlib import Path
from unittest.mock import patch

from cryptography.fernet import Fernet, InvalidToken
from fastapi.testclient import TestClient
from bridge import Bridge, BridgeError, Store, create_app, minor


class FakePlaid:
    environment = "sandbox"

    def __init__(self):
        self.calls = []
        self.responses = {}

    def call(self, endpoint, **payload):
        self.calls.append((endpoint, payload))
        result = self.responses[endpoint]
        if isinstance(result, list):
            result = result.pop(0)
        if isinstance(result, Exception):
            raise result
        return copy.deepcopy(result)


def transaction(id, amount="12.34", pending=False, previous=None):
    return dict(transaction_id=id, account_id="account", name="Private merchant", date="2026-09-12",
                amount=amount, iso_currency_code="USD", pending=pending, pending_transaction_id=previous)


def page(cursor, added=(), modified=(), removed=(), more=False):
    return dict(next_cursor=cursor, added=list(added), modified=list(modified),
                removed=[dict(transaction_id=x) for x in removed], has_more=more)


class BridgeTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.path = Path(self.temp.name) / "bank.sqlite3"
        self.key = Fernet.generate_key()
        self.store = Store(self.path, self.key)
        self.plaid = FakePlaid()
        self.bridge = Bridge(self.store, self.plaid, "test-owner")
        self.token = "test-device-" + "x" * 40
        self.client = TestClient(create_app(self.bridge, self.token, run_worker=False))
        self.auth = {"Authorization": "Bearer " + self.token}

    def seed(self):
        self.store.put("item", "item", dict(access_token="access-secret", institution="Capital One", cursor="old",
            accounts=[], transactions={}, synced_at="2026-09-11T00:00:00+00:00", error=""))
        self.plaid.responses["/accounts/get"] = dict(accounts=[dict(account_id="account", name="Card", mask="1234",
            type="credit", balances=dict(iso_currency_code="USD", current="107.01", available=None, limit="300"))])

    def link(self):
        self.plaid.responses["/link/token/create"] = dict(link_token="link-secret", hosted_link_url="https://secure.plaid.com/hl/test")
        return self.bridge.start_link()

    def test_all_endpoints_require_auth(self):
        for method, route in [("GET", "/v1/banks"), ("POST", "/v1/banks/link"), ("POST", "/v1/banks/sync"),
                              ("POST", "/v1/banks/item/reconnect"), ("DELETE", "/v1/banks/item")]:
            self.assertEqual(self.client.request(method, route).status_code, 401)
            self.assertEqual(self.client.request(method, route, headers={"Authorization": "Bearer wrong"}).status_code, 401)
        response = self.client.get("/v1/banks", headers=self.auth)
        self.assertEqual(response.status_code, 200)
        self.assertEqual(response.headers["Cache-Control"], "no-store")
        self.assertEqual(self.plaid.calls, [])

    def test_link_qr_and_restart_do_not_exchange_twice(self):
        link = self.link()
        self.assertEqual(self.bridge.start_link()["session_id"], link["session_id"])
        self.assertTrue(all(len(row) == len(link["qr"]) for row in link["qr"]))
        self.assertNotIn("link-secret", json.dumps(link))
        self.plaid.responses["/link/token/get"] = dict(link_sessions=[dict(results=dict(item_add_results=[
            dict(public_token="public-secret", institution=dict(name="Chase"))]))])
        self.plaid.responses["/item/public_token/exchange"] = dict(item_id="new", access_token="access-secret")
        self.bridge.poll_links()
        bridge = Bridge(Store(self.path, self.key), self.plaid, "test-owner")
        bridge.poll_links()
        self.assertEqual(sum(endpoint == "/item/public_token/exchange" for endpoint, _ in self.plaid.calls), 1)
        self.assertEqual(bridge.snapshot()["sessions"][0]["state"], "connected")
        self.assertEqual(bridge.snapshot()["items"][0]["institution"], "Chase")
        raw = self.path.read_bytes()
        for value in (b"access-secret", b"public-secret", b"link-secret", b"Chase"):
            self.assertNotIn(value, raw)
        self.assertNotIn("access-secret", json.dumps(bridge.snapshot()))

    def test_sync_pending_modified_removed_and_null_balance(self):
        self.seed()
        self.plaid.responses["/transactions/sync"] = page("one", added=[transaction("pending", pending=True), transaction("removed")])
        self.bridge.sync_item("item")
        self.plaid.responses["/transactions/sync"] = page("two", added=[transaction("posted", "13.01", previous="pending")], removed=["removed"])
        self.bridge.sync_item("item")
        self.plaid.responses["/transactions/sync"] = page("three", modified=[transaction("posted", "-2.10")])
        self.bridge.sync_item("item")
        snapshot = self.bridge.snapshot()["items"][0]
        self.assertEqual(len(snapshot["transactions"]), 1)
        self.assertEqual(snapshot["transactions"][0]["amount_minor"], -210)
        self.assertEqual(snapshot["accounts"][0]["current_minor"], 10701)
        self.assertIsNone(snapshot["accounts"][0]["available_minor"])
        self.assertEqual(self.store.all("item")["item"]["cursor"], "three")
        self.assertNotIn(b"Private merchant", self.path.read_bytes())

    def test_pagination_mutation_restarts_from_original_cursor(self):
        self.seed()
        self.plaid.responses["/transactions/sync"] = [page("partial", added=[transaction("discard")], more=True),
            BridgeError("TRANSACTIONS_SYNC_MUTATION_DURING_PAGINATION"),
            page("retry", added=[transaction("keep")], more=True), page("done")]
        self.bridge.sync_item("item")
        cursors = [args["cursor"] for endpoint, args in self.plaid.calls if endpoint == "/transactions/sync"]
        self.assertEqual(cursors, ["old", "partial", "old", "retry"])
        self.assertEqual(list(self.store.all("item")["item"]["transactions"]), ["keep"])

    def test_failure_keeps_last_complete_snapshot_and_cursor(self):
        self.seed()
        self.plaid.responses["/transactions/sync"] = [page("partial", added=[transaction("not-committed")], more=True),
                                                       BridgeError("ITEM_LOGIN_REQUIRED")]
        self.bridge.sync_item("item")
        item = self.store.all("item")["item"]
        self.assertEqual(item["cursor"], "old")
        self.assertEqual(item["transactions"], {})
        self.assertEqual(item["synced_at"], "2026-09-11T00:00:00+00:00")
        self.assertEqual(item["error"], "ITEM_LOGIN_REQUIRED")

    def test_reconnect_keeps_access_token_and_requests_sync(self):
        self.seed()
        self.plaid.responses["/link/token/create"] = dict(link_token="update-token", hosted_link_url="https://secure.plaid.com/hl/update")
        self.bridge.start_link("item")
        payload = self.plaid.calls[-1][1]
        self.assertEqual(payload["access_token"], "access-secret")
        self.assertNotIn("products", payload)
        self.plaid.responses["/link/token/get"] = dict(link_sessions=[dict(finished_at="2026-09-12", results={}, events=[dict(event_name="HANDOFF")])])
        self.plaid.responses["/item/get"] = dict(item=dict(error=None))
        self.bridge.poll_links()
        self.assertEqual(self.bridge.snapshot()["sessions"][0]["state"], "connected")
        self.assertTrue(self.store.all("item")["item"]["needs_sync"])
        self.assertFalse(any(endpoint == "/item/public_token/exchange" for endpoint, _ in self.plaid.calls))

    def test_expired_link_does_not_poll_provider(self):
        self.link()
        count = len(self.plaid.calls)
        with patch("bridge.time.time", return_value=time.time() + 1900):
            self.bridge.poll_links()
        self.assertEqual(len(self.plaid.calls), count)
        self.assertEqual(self.bridge.snapshot()["sessions"][0]["state"], "expired")

    def test_disconnect_preserves_data_until_revocation_succeeds(self):
        self.seed()
        self.plaid.responses["/item/remove"] = BridgeError("PROVIDER_UNAVAILABLE")
        response = self.client.delete("/v1/banks/item", headers=self.auth)
        self.assertEqual(response.status_code, 502)
        self.assertIn("item", self.store.all("item"))
        self.plaid.responses["/item/remove"] = {}
        self.assertEqual(self.client.delete("/v1/banks/item", headers=self.auth).status_code, 200)
        self.assertEqual(self.store.all("item"), {})

    def test_missing_reconnect_target_and_weak_credentials_rejected(self):
        self.assertEqual(self.client.post("/v1/banks/missing/reconnect", headers=self.auth).status_code, 404)
        with self.assertRaises(ValueError):
            create_app(self.bridge, "weak")

    def test_wrong_key_and_environment_cannot_reuse_database(self):
        self.seed()
        with self.assertRaises(InvalidToken):
            Store(self.path, Fernet.generate_key())
        self.plaid.environment = "production"
        with self.assertRaises(ValueError):
            Bridge(self.store, self.plaid, "test-owner")

    def test_integer_money_validation(self):
        self.assertEqual(minor("1.005", "USD"), 101)
        self.assertEqual(minor("-0.01", "USD"), -1)
        self.assertIsNone(minor(None, "USD"))
        for value in ("NaN", "Infinity", "1000000000001"):
            with self.assertRaises(BridgeError):
                minor(value, "USD")
        with self.assertRaises(BridgeError):
            minor(10, "JPY")


if __name__ == "__main__":
    unittest.main()
