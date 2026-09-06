-- Migration 003: Make private shop bundles expire in real time.
-- 50200 opens the standard shop, 71221 opens the premium two-page/decorated shop.

UPDATE `player`.`item_proto`
SET
    `limittype0` = 7,
    `limitvalue0` = 604800,
    `limittype1` = 0,
    `limitvalue1` = 0
WHERE `vnum` IN (50200, 71221);

UPDATE `player`.`item_proto`
SET `flag` = 0
WHERE `vnum` = 50200;

UPDATE `player`.`item`
SET `socket0` = UNIX_TIMESTAMP() + 604800
WHERE `vnum` IN (50200, 71221)
  AND `socket0` = 0;

UPDATE `player`.`private_shop_item`
SET `socket0` = UNIX_TIMESTAMP() + 604800
WHERE `vnum` IN (50200, 71221)
  AND `socket0` = 0;
