-- Run against player after migrations 002 and 003, with game/DB stopped.
-- The premium bundle is reusable in game code; its existing lifetime is unchanged.
UPDATE `player`.`item_proto`
SET `gold` = 500000
WHERE `vnum` = 71221;

-- Add one bundle to each shop selling the standard bundle; reruns add no duplicates.
INSERT INTO `player`.`shop_item` (`shop_vnum`, `item_vnum`, `count`)
SELECT DISTINCT source.`shop_vnum`, 71221, 1
FROM `player`.`shop_item` AS source
WHERE source.`item_vnum` = 50200
  AND EXISTS (SELECT 1 FROM `player`.`item_proto` WHERE `vnum` = 71221)
  AND NOT EXISTS (
      SELECT 1 FROM `player`.`shop_item` AS existing
      WHERE existing.`shop_vnum` = source.`shop_vnum`
        AND existing.`item_vnum` = 71221
  );

SELECT s.`vnum` AS shop_vnum, s.`npc_vnum`, si.`item_vnum`, si.`count`, ip.`gold`
FROM `player`.`shop` AS s
JOIN `player`.`shop_item` AS si ON si.`shop_vnum` = s.`vnum`
JOIN `player`.`item_proto` AS ip ON ip.`vnum` = si.`item_vnum`
WHERE si.`item_vnum` = 71221;
