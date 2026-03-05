# Kitsas CLI: Headless Bookkeeping API Specification

Kitsas now supports a headless CLI mode designed for LLM-driven bookkeeping. This allows an external agent to read the Chart of Accounts, browse transactions, and record new entries using structured JSON.

## 1. Execution Model
The CLI uses the same internal "Route API" as the graphical interface, ensuring all business logic and validations are preserved.

**Command Syntax:**
```bash
./kitsas --command "[METHOD] [PATH]" --data '[JSON_PAYLOAD]' [PATH_TO_SQLITE_FILE]
```

- **METHOD:** GET, POST, PUT, PATCH, DELETE (Defaults to GET if omitted).
- **PATH:** The internal resource path (e.g., `tilit`, `tositteet`).
- **--data:** (Optional) A JSON string containing the request payload.
- **Output:** Returns a JSON object to `stdout`. Errors are returned as JSON to `stderr`.

---

## 2. Core API Routes for LLMs

### A. Chart of Accounts (`GET tilit`)
Fetch the list of accounts to understand where to categorize transactions.
- **Command:** `./kitsas --command "GET tilit" ledger.sqlite`
- **Output:** A list of account objects containing `numero` (account number), `nimi` (name), and `tyyppi` (type).

### B. Vouchers / Transactions (`GET tositteet`)
Browse existing transactions. Supports query parameters for filtering.
- **Command:** `./kitsas --command "GET tositteet?alkaa=2024-01-01&loppuu=2024-03-31" ledger.sqlite`
- **Query Params:** `alkaa` (start date), `loppuu` (end date), `tila` (status).

### C. Create a Transaction (`POST tositteet`)
Record a new bookkeeping entry.
- **Command:** `./kitsas --command "POST tositteet" --data '{...}' ledger.sqlite
- **Payload Structure:**
  ```json
  {
    "pvm": "2024-03-05",
    "otsikko": "Office Supplies",
    "viennit": [
      {
        "tili": 3000,
        "selite": "Paper and Pens",
        "debet": 124.00,
        "alvkoodi": 1
      },
      {
        "tili": 1910,
        "kredit": 124.00
      }
    ]
  }
  ```
  *Note: Amounts are handled in Euros as decimals or strings (e.g., 124.00 = 124.00€).*

### D. Settings and Info (`GET info`)
Retrieve metadata about the bookkeeping, such as the organization name and current fiscal year status.
- **Command:** `./kitsas --command "GET info" ledger.sqlite`

---

## 3. Error Handling
If a command fails (e.g., unbalanced debits/credits or locked period), the CLI returns a non-zero exit code and a JSON error object:

```json
{
  "code": 400,
  "message": "Debet ja kredit eivät täsmää"
}
```

## 4. LLM Implementation Strategy
1.  **Discovery:** Call `GET info` and `GET tilit` to map the environment.
2.  **Verification:** Call `GET tositteet` to check if a transaction (like a specific invoice) has already been recorded.
3.  **Action:** Formulate a `POST tositteet` JSON payload based on the user's natural language input and the discovered account numbers.
4.  **Confirmation:** Parse the CLI output to confirm the transaction was assigned an ID and valid voucher number.
