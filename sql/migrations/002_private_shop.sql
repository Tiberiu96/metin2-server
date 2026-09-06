-- Migration 002: Add Sonitex-style premium offline private shop storage.
-- This is an additive migration. It does not replace the classic online private shop.

ALTER TABLE `account`.`account`
    ADD COLUMN IF NOT EXISTS `premium_privateshop_expire` DATETIME NOT NULL DEFAULT '0000-00-00 00:00:00'
    AFTER `money_drop_rate_expire`;

CREATE TABLE IF NOT EXISTS `player`.`private_shop` (
  `owner_id` int(11) unsigned NOT NULL,
  `owner_name` varchar(25) CHARACTER SET latin1 COLLATE latin1_swedish_ci NOT NULL DEFAULT '',
  `state` enum('CLOSED','OPEN','MODIFY') CHARACTER SET latin1 COLLATE latin1_swedish_ci NOT NULL DEFAULT 'CLOSED',
  `title` varchar(33) CHARACTER SET latin1 COLLATE latin1_swedish_ci NOT NULL DEFAULT '',
  `title_type` tinyint(1) unsigned NOT NULL DEFAULT 0,
  `vnum` int(7) unsigned NOT NULL DEFAULT 30000,
  `x` int(11) NOT NULL DEFAULT 0,
  `y` int(11) NOT NULL DEFAULT 0,
  `map_index` int(11) NOT NULL DEFAULT 0,
  `channel` tinyint(1) unsigned NOT NULL DEFAULT 0,
  `port` int(5) unsigned DEFAULT NULL,
  `gold` bigint(20) unsigned NOT NULL DEFAULT 0,
  `cheque` int(10) unsigned NOT NULL DEFAULT 0,
  `page_count` tinyint(1) unsigned NOT NULL DEFAULT 1,
  `premium_time` int(11) unsigned NOT NULL DEFAULT 0,
  PRIMARY KEY (`owner_id`) USING BTREE,
  KEY `state_idx` (`state`) USING BTREE,
  KEY `map_channel_idx` (`map_index`,`channel`) USING BTREE
) ENGINE=MyISAM DEFAULT CHARSET=latin1 COLLATE=latin1_swedish_ci ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `player`.`private_shop_item` (
  `id` int(11) unsigned NOT NULL AUTO_INCREMENT,
  `owner_id` int(11) unsigned NOT NULL DEFAULT 0,
  `pos` smallint(5) unsigned NOT NULL DEFAULT 0,
  `count` tinyint(3) unsigned NOT NULL DEFAULT 0,
  `vnum` int(11) unsigned NOT NULL DEFAULT 0,
  `gold` bigint(20) unsigned NOT NULL DEFAULT 0,
  `cheque` int(10) unsigned NOT NULL DEFAULT 0,
  `checkin` int(11) unsigned NOT NULL DEFAULT 0,
  `socket0` int(10) unsigned NOT NULL DEFAULT 0,
  `socket1` int(10) unsigned NOT NULL DEFAULT 0,
  `socket2` int(10) unsigned NOT NULL DEFAULT 0,
  `attrtype0` tinyint(4) NOT NULL DEFAULT 0,
  `attrvalue0` smallint(6) NOT NULL DEFAULT 0,
  `attrtype1` tinyint(4) NOT NULL DEFAULT 0,
  `attrvalue1` smallint(6) NOT NULL DEFAULT 0,
  `attrtype2` tinyint(4) NOT NULL DEFAULT 0,
  `attrvalue2` smallint(6) NOT NULL DEFAULT 0,
  `attrtype3` tinyint(4) NOT NULL DEFAULT 0,
  `attrvalue3` smallint(6) NOT NULL DEFAULT 0,
  `attrtype4` tinyint(4) NOT NULL DEFAULT 0,
  `attrvalue4` smallint(6) NOT NULL DEFAULT 0,
  `attrtype5` tinyint(4) NOT NULL DEFAULT 0,
  `attrvalue5` smallint(6) NOT NULL DEFAULT 0,
  `attrtype6` tinyint(4) NOT NULL DEFAULT 0,
  `attrvalue6` smallint(6) NOT NULL DEFAULT 0,
  PRIMARY KEY (`id`) USING BTREE,
  KEY `owner_id_idx` (`owner_id`) USING BTREE,
  KEY `item_vnum_idx` (`vnum`) USING BTREE,
  UNIQUE KEY `owner_pos_idx` (`owner_id`,`pos`) USING BTREE
) ENGINE=MyISAM DEFAULT CHARSET=latin1 COLLATE=latin1_swedish_ci ROW_FORMAT=FIXED;

SET @private_shop_item_auto_increment := (
    SELECT GREATEST(
        IFNULL((SELECT MAX(`id`) + 100000 FROM `player`.`item`), 0),
        IFNULL((SELECT MAX(`id`) + 1 FROM `player`.`private_shop_item`), 0),
        70000046
    )
);
SET @private_shop_item_sql := CONCAT('ALTER TABLE `player`.`private_shop_item` AUTO_INCREMENT = ', @private_shop_item_auto_increment);
PREPARE private_shop_item_stmt FROM @private_shop_item_sql;
EXECUTE private_shop_item_stmt;
DEALLOCATE PREPARE private_shop_item_stmt;

INSERT INTO `player`.`item_proto`
    (`vnum`, `name`, `locale_name`, `type`, `subtype`, `weight`, `size`, `antiflag`, `flag`, `wearflag`, `immuneflag`, `gold`, `shop_buy_price`,
     `refined_vnum`, `refine_set`, `refine_set2`, `magic_pct`, `limittype0`, `limitvalue0`, `limittype1`, `limitvalue1`,
     `applytype0`, `applyvalue0`, `applytype1`, `applyvalue1`, `applytype2`, `applyvalue2`,
     `value0`, `value1`, `value2`, `value3`, `value4`, `value5`,
     `socket0`, `socket1`, `socket2`, `socket3`, `socket4`, `socket5`, `specular`, `socket_pct`, `addon_type`)
VALUES
    (60004, 'Looking Glass', 'Looking Glass', 3, 10, 0, 1, 123264, 0, 0, '', 0, 0, 0, 0, 0, 0, 7, 3600, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, -1, -1, -1, -1, -1, 0, 0, 0),
    (60005, 'Trading Glass', 'Trading Glass', 3, 10, 0, 1, 123264, 0, 0, '', 0, 0, 0, 0, 0, 0, 7, 604800, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, -1, -1, -1, -1, -1, 0, 0, 0),
    (71221, 'Kashmir Bundle', 'Kashmir Bundle', 3, 10, 0, 1, 123264, 8192, 0, '', 2500, 2000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, -1, -1, -1, -1, -1, 0, 0, 0),
    (72355, 'Premium Private Shop 7d', 'Premium Private Shop 7d', 3, 8, 0, 1, 123264, 8196, 0, '', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 507, 0, 0, 604800, 0, 0, -1, -1, -1, -1, -1, -1, 0, 0, 0),
    (72356, 'Premium Private Shop 1d', 'Premium Private Shop 1d', 3, 8, 0, 1, 123264, 8196, 0, '', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 507, 0, 0, 86400, 0, 0, -1, -1, -1, -1, -1, -1, 0, 0, 0)
ON DUPLICATE KEY UPDATE
    `name` = VALUES(`name`),
    `locale_name` = VALUES(`locale_name`),
    `type` = VALUES(`type`),
    `subtype` = VALUES(`subtype`),
    `antiflag` = VALUES(`antiflag`),
    `flag` = VALUES(`flag`),
    `gold` = VALUES(`gold`),
    `shop_buy_price` = VALUES(`shop_buy_price`),
    `limittype0` = VALUES(`limittype0`),
    `limitvalue0` = VALUES(`limitvalue0`),
    `value0` = VALUES(`value0`),
    `value3` = VALUES(`value3`);
