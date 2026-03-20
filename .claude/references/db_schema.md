# Database Schema Reference

Verified from live databases. Last updated: 2026-03-20.

---

## DB: `account` (connection: `account`)

### `account.account` — read/write
| Column               | Type         | Notes                                             |
|----------------------|--------------|---------------------------------------------------|
| id                   | int(11)      | PK, auto_increment                                |
| login                | varchar(30)  | Username — UNIQUE index                           |
| password             | varchar(45)  | MySQL PASSWORD() hash — 41 chars, starts with `*` |
| social_id            | varchar(13)  | Required non-null — generate on register          |
| email                | varchar(64)  |                                                   |
| create_time          | datetime     |                                                   |
| status               | varchar(8)   | `OK`, `BLOCK`, `QUIT`                             |
| empire               | tinyint(4)   | 1=Red, 2=Yellow, 3=Blue — default 0 (often unreliable, use player_index.empire) |
| availDt              | datetime     | Ban expiry — **column is `availDt` camelCase**    |
| gold_expire          | datetime     | Premium expiry                                    |
| silver_expire        | datetime     |                                                   |
| safebox_expire       | datetime     |                                                   |
| autoloot_expire      | datetime     |                                                   |
| cash                 | int(11)      | Premium currency                                  |
| mileage              | int(11)      |                                                   |
| is_testor            | tinyint(1)   |                                                   |
| last_play            | datetime     |                                                   |
| ip                   | varchar(255) |                                                   |

**Notes:**
- Ban: `status = 'BLOCK'`, `availDt` = expiry datetime
- Register: `status = 'OK'`, `social_id` = `substr(md5(uniqid()), 0, 13)`, `create_time` = now()
- `empire` default is 0 — **not reliable for display**; use `player.player_index.empire` instead

---

## DB: `player` (connection: `player`)

### `player.player` — read/write
| Column          | Type                | Notes                                                     |
|-----------------|---------------------|-----------------------------------------------------------|
| id              | int(11)             | PK, auto_increment                                        |
| account_id      | int(11)             | FK → account.account.id — indexed                         |
| name            | varchar(24)         | indexed                                                   |
| job             | tinyint(2) unsigned | 0=Warrior,1=Assassin,2=Sura,3=Shaman (4–7 = female variants) |
| level           | tinyint(2) unsigned | 1–120+                                                    |
| exp             | int(11)             |                                                           |
| gold            | int(11)             | Yang                                                      |
| playtime        | int(11)             | Minutes played                                            |
| last_play       | datetime            |                                                           |
| hp / mp         | smallint(4)         |                                                           |
| x / y / z       | int(11)             | World position                                            |
| map_index       | int(11)             |                                                           |
| alignment       | int(11)             |                                                           |
| skill_group     | tinyint(4)          |                                                           |
| part_main       | int(11)             | Equipped armor vnum                                       |
| part_hair       | int(11)             | Equipped hair vnum                                        |

**⚠️ NO `empire` column on `player.player`** — empire is on `player_index`

### `player.player_index` — read/write
| Column  | Type       | Notes                                                        |
|---------|------------|--------------------------------------------------------------|
| id      | int(11)    | PK = account_id (FK → account.account.id)                    |
| pid1    | int(11)    | Character slot 1 → player.player.id                          |
| pid2    | int(11)    | Character slot 2 → player.player.id                          |
| pid3    | int(11)    | Character slot 3 → player.player.id                          |
| pid4    | int(11)    | Character slot 4 → player.player.id                          |
| empire  | tinyint(4) | **Authoritative empire** — 1=Red, 2=Yellow, 3=Blue           |

**Usage for empire in queries:**
```php
->leftJoin('player_index', 'player_index.id', '=', 'player.account_id')
```

### `player.item`
| Column      | Type                                                                              | Notes                       |
|-------------|-----------------------------------------------------------------------------------|-----------------------------|
| id          | int unsigned                                                                      | PK                          |
| owner_id    | int unsigned                                                                      | FK → player.player.id       |
| window      | enum(INVENTORY, EQUIPMENT, SAFEBOX, MALL, DRAGON_SOUL_INVENTORY, BELT_INVENTORY) |                             |
| pos         | smallint unsigned                                                                 | Slot position               |
| count       | tinyint unsigned                                                                  |                             |
| vnum        | int unsigned                                                                      | Item type → item_proto.vnum |
| socket0–5   | int unsigned                                                                      | Socket values               |
| attrtype0–6 | tinyint                                                                           | Bonus type                  |
| attrvalue0–6| smallint                                                                          | Bonus value                 |

### `player.guild`
| Column       | Type         | Notes                           |
|--------------|--------------|---------------------------------|
| id           | int unsigned | PK                              |
| name         | varchar(12)  |                                 |
| master       | int unsigned | FK → player.player.id           |
| level        | tinyint(2)   |                                 |
| exp          | int(11)      |                                 |
| sp           | smallint(6)  | Guild skill points              |
| ladder_point | int(11)      |                                 |
| gold         | int(11)      |                                 |
| win/draw/loss| int(11)      | Guild war stats                 |

### `player.guild_member`
| Column    | Notes                              |
|-----------|------------------------------------|
| pid       | FK → player.player.id              |
| guild_id  | FK → player.guild.id               |
| grade     | 1–9                                |
| is_general| tinyint(1)                         |
| offer     | EXP offered to guild               |

### `player.safebox`
| Column     | Notes                    |
|------------|--------------------------|
| account_id | FK → account.account.id  |
| size       | tinyint                  |
| password   | varchar(6)               |
| gold       | int(11)                  |

### `player.quest`
| Column  | Notes                      |
|---------|----------------------------|
| dwPID   | FK → player.player.id      |
| szName  | Quest name                 |
| szState | Quest state                |
| lValue  | Quest value                |

### `player.affect` — active buffs
| Column      | Notes                      |
|-------------|----------------------------|
| dwPID       | FK → player.player.id      |
| bType       | Affect type                |
| bApplyOn    | Stat affected              |
| lApplyValue | Value                      |
| lDuration   | Remaining seconds          |

### `player.change_empire`
- `account_id`, `change_count`, `timestamp`

### `player.monarch`
- `empire` (1-3), `pid` → player.id, `windate`, `money`

---

## DB: `common` (connection: `common`, read-only)

### `common.item_proto`
| Column               | Notes                              |
|----------------------|------------------------------------|
| vnum                 | PK — item type ID                  |
| name                 | varbinary(24)                      |
| locale_name          | varbinary(24)                      |
| type / subtype       | Item category                      |
| gold                 | NPC sell price                     |
| shop_buy_price       | NPC buy price                      |
| limittype0/1         | 0=NONE,1=LEVEL,2=STR,4=DEX...      |
| limitvalue0/1        | Limit value                        |
| applytype0–2         | Base bonus type                    |
| applyvalue0–2        | Base bonus value                   |
| value0–5             | Type-specific values               |
| socket0–5            | Socket count per slot              |

### `common.mob_proto`
| Column      | Notes                                           |
|-------------|-------------------------------------------------|
| vnum        | PK — mob type ID                                |
| name        | varchar(24)                                     |
| level       | smallint                                        |
| rank        | 0=PAWN,1=S_PAWN,2=KNIGHT,3=S_KNIGHT,4=BOSS,5=KING |
| type        | 0=MONSTER,1=NPC,2=STONE,4=WARP,5=GOTO          |
| max_hp      | int unsigned                                    |
| exp         | int unsigned                                    |
| empire      | tinyint — which empire the mob belongs to       |
| damage_min/max | smallint                                     |

### `common.gmlist` — GM accounts
| Column      | Notes                                                      |
|-------------|------------------------------------------------------------|
| mAccount    | Login (FK → account.account.login)                         |
| mName       | Character name                                             |
| mAuthority  | IMPLEMENTOR / HIGH_WIZARD / GOD / LOW_WIZARD / PLAYER      |

---

## DB: `log` (connection: `log`, read-only)

| Table           | Key columns                                           | Purpose                    |
|-----------------|-------------------------------------------------------|----------------------------|
| `loginlog`      | type(LOGIN/LOGOUT), account_id, pid, level, job, playtime, time | Login/logout events |
| `loginlog2`     | account_id, pid, ip, client_version, login_time, logout_time | Extended login log |
| `levellog`      | name, level, time, playtime, account_id, pid          | Level-up events            |
| `goldlog`       | pid, what(amount), how(BUY/SELL/SHOP_SELL/EXCHANGE...) | Yang transactions         |
| `command_log`   | userid, command, date, ip                             | GM commands                |
| `hack_log`      | login, name, ip, why                                  | Hack attempts              |
| `hack_crc_log`  | login, name, ip, crc                                  | CRC hack attempts          |
| `refinelog`     | pid, item_name, step, is_success, setType             | Refine attempts            |
| `playercount`   | date, count_red, count_yellow, count_blue, count_total | Periodic player counts    |
| `shout_log`     | empire, channel, shout                                | Chat shouts                |
| `money_log`     | type(MONSTER/SHOP/REFINE/QUEST...), vnum, gold        | Yang economy               |

---

## DB: `metin2_web` (connection: `mysql`, local Laravel)

| Table           | Notes                                          |
|-----------------|------------------------------------------------|
| `admins`        | Filament admin users — bcrypt password         |
| `news`          | CMS news — title/body/excerpt as JSON multilang|
| `sessions`      | Laravel sessions (user_id = metin2 account id) |
| `cache`         | Laravel cache                                  |
| `jobs`          | Queue jobs                                     |
| `failed_jobs`   | Failed queue jobs                              |
