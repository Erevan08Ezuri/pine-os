# Bank linking: Capital One and Chase

This first bank integration adds **Finance → Accounts → Linked Banks**. It reads USD bank accounts,
credit-card balances, and the latest 30 transactions per bank connection. Connect each bank separately
using a QR code and your phone's browser. The existing manual ledger, budgets, and manual dashboard
totals are unchanged: bank data lives in a separate read-only view to avoid counting manually entered
transactions twice. No payments, transfers, routing numbers, or bank passwords are collected by PineOS.

## What runs where

- **Tab5:** C++ Finance screen and a certificate-verified ESP-IDF HTTPS client; displays a QR matrix
  returned by the server. No browser engine or Plaid SDK is needed on the microcontroller.
- **Phone:** Plaid Hosted Link and the bank's sign-in/consent screens. Select Capital One or Chase
  inside Link. PineOS never asks for your bank password.
- **Server:** the separate Python 3.11+ bridge in `server/bank_bridge`; suitable for the existing
  Windows VPS or Linux. Stores access/link tokens and financial payloads encrypted with Fernet in
  SQLite. Only the server holds Plaid API secrets and bank access tokens.

## Start with Sandbox

1. Create a [Plaid developer account](https://dashboard.plaid.com/signup) and obtain Sandbox keys.
2. On the server, open a terminal in `server/bank_bridge`:

   ```sh
   python -m venv .venv
   ```

   Activate with `.venv\Scripts\activate` on Windows cmd, `.venv\Scripts\Activate.ps1` in PowerShell,
   or `source .venv/bin/activate` on Linux/macOS, then:

   ```sh
   python -m pip install -r requirements.txt
   python configure.py
   python -m uvicorn bridge:from_environment --factory --env-file .env --host 127.0.0.1 --port 8765 --workers 1 --no-access-log
   ```

   The setup tool prompts locally for keys and generates `.env` plus `private/bank-connection.json`.
   Do not paste their contents into chat, commit them, or bake them into firmware. On Windows,
   restrict the directory's NTFS permissions to your service account; Unix creation uses mode 0600.
3. Put an HTTPS reverse proxy in front of the loopback listener using a domain you control and a
   publicly trusted certificate. For example, a Caddy site block on that same server:

   ```caddyfile
   banks.your-domain.example {
       reverse_proxy 127.0.0.1:8765
   }
   ```

   Replace the domain, configure DNS and ports 80/443, and run the bridge/proxy as supervised services
   for unattended use. Keep port 8765 private. Run exactly **one worker/process** for this single-owner
   bridge. No public webhook or OAuth callback endpoint is required: Hosted Link handles the browser
   flow, and the bridge polls `/link/token/get` server-to-server.
4. On PineOS, open **Finance → Accounts → Linked Banks → Server Setup**. Enter the HTTPS URL and the
   generated `BANK_DEVICE_TOKEN` from `.env` locally. Alternatively, copy `private/bank-connection.json`
   into PineOS's **data root** (alongside `settings.json`, not inside the user Files storage) before
   launching. The same config works in the desktop simulator when built with libcurl.
5. Ensure Wi-Fi is connected and the device clock is correct for TLS verification. Tap **Connect Bank**,
   scan with the phone, and finish Link. Sandbox uses Plaid test credentials, not your actual bank login.
   The label **SANDBOX / TEST DATA ONLY** remains visible until a Production bridge is configured.
6. The server checks Link completion every 15 seconds and performs the initial data sync automatically.
   If the phone flow expires, request a new code. Repeated Connect taps reuse the active code.

## Enable actual Capital One and Chase accounts

Real bank data needs approved Plaid Production access for **Transactions** and the relevant OAuth
institutions. Complete the institution/application setup in the Plaid Dashboard, and check the plan's
pricing before enabling real connections. A Sandbox connection is not a live bank connection.

Use a **separate** `BANK_DB` for Production, change `PLAID_ENV` to `production`, and supply Production
credentials in the server environment. The bridge rejects a database belonging to another environment
or owner. Retain the encryption key for each database. A new device token is recommended when switching
environments; update the device's Server Setup to match. Connect Capital One, wait for completion, then
repeat for Chase. Use **Reconnect** for an existing connection rather than adding it again.

Institution-specific behavior, verified against [Plaid's OAuth guide](https://plaid.com/docs/link/oauth/):

- **Capital One:** pending transactions are not supplied. Consent needs renewal after one year.
  Credit-card-only Items do not support `/transactions/refresh`; this bridge deliberately uses
  `/transactions/sync` and cached `/accounts/get` balances instead. Past-due cards may not be linkable.
- **Chase:** Production access normally requires the Security Questionnaire (Plaid currently lists a
  Trial-plan exception). Account/permission revocation or restriction must also be managed in the
  Chase Security Center; recreating a connection does not reset Chase's bank-side permissions.

## Sync, privacy, and current limits

- Server sync runs hourly, and **Sync** queues a check (at most once per minute). This downloads what
  Plaid already has; it does not force the bank to refresh. Plaid normally checks transactions one to
  four times daily. **Retrieved** is the bridge retrieval time, not proof of a real-time bank balance.
- Added/modified/removed transactions and pending-to-posted replacements are applied by stable ID.
  All pages and the cursor commit together. Pagination mutation retries start from the old cursor.
  Errors preserve the previous complete snapshot and surface a provider error code for reconnecting.
- A credit-card balance is money owed; the displayed amount is not an asset or cash available.
  Missing balances show **UNAVAILABLE**, never zero. USD only; non-USD records are excluded.
- The Tab5 holds its last fetched snapshot in memory. It is not persisted into the manual ledger;
  after a restart it needs the bridge again. Existing Finance PIN/auto-lock and hide-balances apply.
- Disconnect requires typing `REMOVE`, calls Plaid `/item/remove`, and deletes the local bridge copy
  only on success. It does not erase manual records. Manage bank-side consent in your bank portal.
- The device token grants access to this owner's complete bank snapshot and must be protected like
  a password. The Tab5 config filesystem is not hardware-encrypted by this feature; Finance PIN locking
  protects the UI, not a physically extracted flash image. Rotate the token server-side if the device
  is lost. Store encrypted database backups and their encryption key separately.
- This is a single-owner bridge, not a multi-user financial service. No real credentials, live bank
  authorization, or physical Tab5 TLS/QR scan can be tested without the owner's setup and hardware.
- Link completion uses polling during the 30-minute link lifetime. If the bridge is down past expiry,
  start a new session and check Plaid Dashboard for any orphaned Item before linking again. There is
  an unavoidable recovery window if a provider exchange succeeds but the server loses power before
  saving the response. Successful saved exchanges and local session updates commit atomically.

## Validation

Bridge tests use a deterministic fake provider; they do not call banks or prove Production approval:

```sh
python -m pip install httpx
python -m unittest discover -s server/bank_bridge -p 'test_*.py' -v
```

Run that command from the repository root using the bridge virtual environment. Desktop builds use
libcurl when CMake finds it; otherwise the rest of PineOS still builds and bank networking reports an
explicit unavailable error. Tab5 always uses ESP-IDF `esp_http_client` and the public CA bundle.

References: [Hosted Link](https://plaid.com/docs/link/hosted-link/),
[Transactions sync](https://plaid.com/docs/api/products/transactions/#transactionssync),
[Link API](https://plaid.com/docs/api/link/), [Transactions freshness](https://plaid.com/docs/transactions/).
