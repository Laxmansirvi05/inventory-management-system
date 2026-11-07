# 05 · Database Architecture and Workflow

## 1. Layers
```mermaid
flowchart TB
  UI[Browser UI + DBMS Lab pages] --> API[C++ REST API]
  API --> CORE[Inventory core<br/>vector · unordered_map · queue · priority_queue · stack]
  API --> DBL[Database layer<br/>prepared statements · transactions · SQL logger]
  DBL --> |SSL| SQL[(MySQL 8 · InnoDB)]
  subgraph MySQL
    SQL --> T[Tables + constraints]
    SQL --> TR[Triggers]
    SQL --> V[Views]
    SQL --> IX[Indexes]
  end
```

## 2. What happens on Add / Update / Delete / Sell (shown live in the SQL panel)
```mermaid
sequenceDiagram
  participant U as User (browser)
  participant A as C++ API
  participant D as DB layer
  participant M as MySQL
  U->>A: click Sell → POST /api/sales
  A->>A: role check, FIFO queue
  A->>D: process order
  D->>M: START TRANSACTION
  D->>M: INSERT Sales
  D->>M: INSERT Sale_Items
  M->>M: BEFORE trigger: stock enough?
  alt enough stock
    M->>M: AFTER trigger: update stock, total, ledger
    D->>M: COMMIT
  else not enough
    M-->>D: error 1644 (SIGNAL)
    D->>M: ROLLBACK
  end
  D-->>A: result + sql_log + before/after rows
  A-->>U: JSON → UI updates, SQL panel lists every step
```

## 3. Live SQL panel contents
For each action: (1) SQL text in execution order, (2) trigger notes, (3) rows before and after, (4) MySQL error and `ROLLBACK` when something fails, (5) time taken and rows affected. The C++ DB layer appends every statement to a per-request log (`sqllog.cpp`) and returns it with the response.

## 4. Startup and sync
1. Connect to MySQL (SSL). 2. `SELECT` products and load the vector, hash map and tree map. 3. Build the low-stock priority queue. 4. Serve requests. All writes go to MySQL first; memory is updated only after a successful COMMIT, so memory never holds data the database rejected.

## 5. Deployment
```mermaid
flowchart LR
  G[GitHub repo] -->|auto deploy| R[Render<br/>Docker: C++ app + /public]
  R -->|SSL, env vars| A[(Aiven MySQL)]
  W[Workbench] -->|SSL| A
  U[Browser] --> R
```

## 6. What cannot be shown live
Internal MySQL mechanisms (undo/redo logs, lock manager, ARIES recovery) are not visible from the application; they are explained in the DBMS report. Lock-wait and deadlock demos are possible from two sessions but are optional.
