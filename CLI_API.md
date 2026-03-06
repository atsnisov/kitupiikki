# Kitsas CLI: Professional Bookkeeping API for LLM Agents

This document defines the interface for interacting with the Kitsas bookkeeping engine via the command line. It is designed for LLM agents to perform automated bookkeeping, auditing, and reporting.

## 1. Execution Model

Kitsas CLI acts as a bridge to the internal SQLite-based bookkeeping engine. All commands require a path to a Kitsas SQLite database file.

**Command Syntax:**
```bash
./kitsas --command "[METHOD] [PATH]" --data '[JSON_PAYLOAD]' [PATH_TO_SQLITE_FILE]
```

- **METHOD:** GET (default), POST, PUT, PATCH, DELETE.
- **PATH:** Resource path (e.g., `tilit`, `tositteet`). Can include query parameters.
- **--data:** JSON-formatted payload for POST, PUT, and PATCH requests.
- **Output:** Structured JSON to `stdout`.
- **Errors:** JSON error objects to `stderr` with non-zero exit codes.

---

## 2. Core Resources

### A. Chart of Accounts (`GET tilit`)
Fetch the list of accounts to identify where to categorize transactions.
- **Fields:** `numero` (ID), `nimi` (Name), `tyyppi` (Type).
- **Strategy:** Always fetch this first to map user intent to valid account numbers.

### B. Vouchers and Transactions (`/tositteet`)

#### List Vouchers (`GET tositteet`)
Browse existing entries.
- **Query Parameters:**
  - `alkaa` / `loppuu`: Date range (YYYY-MM-DD).
  - `huomio=1`: **(CRITICAL)** Filters for vouchers requiring attention (missing entries or manual flags).
  - `luonnos=1`, `saapuneet=1`: Filter by status.
- **Response Fields:**
  - `id`: Internal voucher ID.
  - `tilioimatta`: Number of entries (viennit) missing an account number (tili=0).
  - `json`: Metadata containing the `huomio` flag and other details.

#### Get Detailed Voucher (`GET tositteet/ID`)
Fetch a single voucher with all its accounting entries (`viennit`).
- **Entry Fields:** `tili`, `selite`, `debet`, `kredit`, `alvkoodi`, `alvprosentti`.

#### Create Voucher (`POST tositteet`)
Record a new transaction.
```json
{
  "pvm": "2024-03-05",
  "otsikko": "Office Supplies",
  "viennit": [
    { "tili": 3000, "selite": "Paper", "debet": "50.00", "alvkoodi": 1 },
    { "tili": 1910, "kredit": "50.00" }
  ]
}
```

#### Update Voucher (`PUT tositteet/ID`)
Modify an existing voucher. Useful for fixing missing account numbers discovered via the `huomio` filter.

#### Delete Voucher (`DELETE tositteet/ID`)
Remove a voucher from the ledger.

---

## 3. The "AI Accountant" Workflow

To perform automated bookkeeping, the LLM agent should follow this protocol:

1.  **Auditing:** Run `GET tositteet?huomio=1` to find "To-Do" items.
2.  **Inspection:** For each result, run `GET tositteet/ID` to see exactly what is missing (e.g., a row where `tili` is 0).
3.  **Contextual Mapping:** Run `GET tilit` to find the most appropriate account for the unallocated entry based on its `selite` (description) or partner name.
4.  **Correction:** Use `PUT tositteet/ID` with the corrected `viennit` array to finalize the entry.
5.  **Verification:** Re-run the audit to ensure the `tilioimatta` count for that ID is now 0.

---

## 4. Technical Constraints

- **Amounts:** Handled as strings or decimals (e.g., `"12.50"`). Internally converted to cents.
- **Statuses:** 
  - `0`: Deleted
  - `1-49`: Templates/Saapuneet
  - `50-99`: Drafts (Luonnokset)
  - `100+`: Finalized in ledger (Kirjanpidossa)
- **VAT:** `alvkoodi` refers to the internal VAT mapping. `alvprosentti` is the tax rate as a float.

## 5. Error Codes
- `400`: Validation error (e.g., debits do not match credits).
- `404`: Resource not found.
- `403`: Period locked or insufficient permissions.
