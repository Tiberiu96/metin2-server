-- Migration 001: Add language column to account.account
-- Stores per-account language preference for dynamic quest translations
-- Valid values: en, de, hu, fr, cz, dk, es, gr, it, nl, pl, pt, ro, ru, tr
-- Default: 'en' (English)

ALTER TABLE `account`.`account`
    ADD COLUMN IF NOT EXISTS `language` VARCHAR(2) NOT NULL DEFAULT 'en'
    AFTER `last_play`;
