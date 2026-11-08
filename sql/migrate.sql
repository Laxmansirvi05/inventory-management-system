-- migrate.sql : bring a database made by an EARLIER version up to date WITHOUT touching its data. Safe to run twice.
-- Run it through scripts/migrate_db.sh, which also re-creates the triggers (triggers.sql) afterwards.
-- What changed: item rows no longer cascade-delete with their sale or purchase (a cascade fires no triggers), and
-- there are new triggers for deleting and editing lines. Password hashes are left alone: old ones are replaced by
-- PBKDF2 hashes when each user next signs in.
ALTER TABLE Sale_Items DROP FOREIGN KEY fk_si_sale;
ALTER TABLE Sale_Items ADD CONSTRAINT fk_si_sale FOREIGN KEY (sale_id) REFERENCES Sales(sale_id)
    ON DELETE RESTRICT ON UPDATE CASCADE;
ALTER TABLE Purchase_Items DROP FOREIGN KEY fk_pi_pur;
ALTER TABLE Purchase_Items ADD CONSTRAINT fk_pi_pur FOREIGN KEY (purchase_id) REFERENCES Purchases(purchase_id)
    ON DELETE RESTRICT ON UPDATE CASCADE;
