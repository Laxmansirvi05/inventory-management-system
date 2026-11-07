# 01 · Product Requirements Document (PRD)
**Project:** Inventory Management System (IMS) · **Courses:** Advanced Data Structures (ADS) + Database Management Systems (DBMS, 25CS21003)
**Team:** D. Yashwin (25R11A66U2), K. Krithik (25R11A66V2), Lakshman (25R11A66V5) · **Status:** DRAFT for team review

## 1. Problem
Small stores and college stores track stock in registers or spreadsheets: duplicated data, wrong stock counts, slow searching, no protection against invalid entries.

## 2. Goal
A web application that manages products, stock, purchases, sales, suppliers and customers, using **C++ data structures and algorithms** for in-memory processing and **MySQL** for permanent storage, and that **visibly demonstrates the DBMS syllabus** while it runs.

## 3. Users
| Role | Can do |
|---|---|
| Admin | Everything: manage products, suppliers, customers, users, view all reports, open the DBMS Lab |
| Employee | Search products, record sales and purchases, view stock and low-stock alerts (no deletes, no user management) |

## 4. Features
**Must have (M), Should have (S), Optional (O)**
| ID | Feature | Priority |
|---|---|---|
| F1 | Login with Admin / Employee roles | M |
| F2 | Add, update, delete, search products (by ID, name, category) | M |
| F3 | Sort products by price, quantity or name | M |
| F4 | Categories, suppliers, customers management | M |
| F5 | Record purchases (stock in) with several items | M |
| F6 | Record sales (stock out) with several items; reject if stock is insufficient | M |
| F7 | Customer orders processed in FIFO order (queue) | M |
| F8 | Low-stock alerts, most urgent first (priority queue) | M |
| F9 | Reports: stock value, top sellers, sales/purchases per customer/supplier, never-sold products | M |
| F10 | Undo last stock transaction (stack) | S |
| F11 | **DBMS Lab:** Database Model page, Query Explorer, live SQL panel, trigger/transaction/constraint/DCL/index/BLOB demos | M |
| F12 | Product photo upload and display (BLOB) | S |
| F13 | Expiry-date tracking, invoice view | O |

## 5. Non-functional requirements
- Simple, professional UI in HTML, CSS, JavaScript; no front-end framework required.
- Responds in under about 1 s for normal actions on the demo data set.
- Passwords stored hashed; SQL always parameterised; secrets only in environment variables.
- The whole project can be explained line by line by the team (small files, comments).

## 6. Constraints
- Backend in **C++** (no Java). Database **MySQL 8.x**. SQL limited to the syllabus (see TRD §7).
- Deployment: GitHub (code), Render (app), Aiven MySQL (database). Must also run locally.

## 7. Out of scope
Payments and taxes, barcode hardware, multi-warehouse stock, demand forecasting, mobile app.

## 8. Success criteria
1. Every must-have feature works end to end in a browser.
2. For any add/update/delete/sale, the live SQL panel shows the SQL, trigger activity and before/after rows.
3. Each ADS structure/algorithm and each DBMS syllabus topic can be pointed to in the running app (see `07-Syllabus-Mapping.md`).
