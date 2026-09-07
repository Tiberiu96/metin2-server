-- Stop DB and all game processes before applying and deploying the new protocol.
ALTER TABLE player.private_shop
    MODIFY COLUMN state ENUM('CLOSED','OPEN','MODIFY','RECOVERY') NOT NULL DEFAULT 'CLOSED',
    ADD COLUMN IF NOT EXISTS lifetime_seconds INT UNSIGNED NOT NULL DEFAULT 172800;
-- Existing deadlines are deliberately preserved; no stock or earnings are deleted.
