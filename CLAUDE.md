# metin2-server

Metin2 Reference Server r40250 by TMP4. FreeBSD 13.5+ (32-bit).

## Structura repo

```
server/   → runtime (auth, channel1-4, db, game99, share/)
src/      → sursa C++ (game, db, libgame, liblua, libthecore)
sql/      → schema DB (account, player, common, log) + migrations/
```

## Paths FreeBSD

```
/usr/metin2/server/   → server/
/usr/metin2/src/      → src/
```

## Comenzi server

```sh
cd /usr/metin2/server && sh start.sh   # pornire
cd /usr/metin2/server && sh close.sh   # oprire
cd /usr/metin2/server && sh clean.sh   # curatare
```

## Compilare

Doar pe FreeBSD 32-bit (`pkg install llvm-devel gmake makedepend`).

```sh
cd /usr/metin2/src/server/db/src   && gmake dep && gmake -j9
cd /usr/metin2/src/server/game/src && gmake dep && gmake -j9
# Binarele merg in: server/share/bin/
```

Quests: `cd /usr/metin2/server/share/locale/english/quest && python2.7 make.py`

## Configuratie IP

Fiecare CONFIG (auth, channel*/first, game1, game2, game99):
```
BIND_IP: 192.168.x.x     # IP intern (ifconfig)
PROXY_IP: 77.88.99.111   # IP extern/public
```

## Baze de date

- MariaDB 10.6 — baze: `account`, `player`, `common`, `log`
- User MySQL: `metin2@localhost` / `root@%`
- `sql/my.cnf` → `/usr/local/etc/mysql/my.cnf`

## Migratii DB

Schema change → fisier nou in `sql/migrations/` (ex: `001_descriere.sql`).

- Numerotare 3 cifre, idempotent (`IF NOT EXISTS`)
- Nu modifica fisier deja aplicat

```sh
mysql -u root player < /usr/metin2/sql/migrations/001_descriere.sql
```

## Limbi disponibile

EN/DE/HU/FR/CZ/DK/ES/GR/IT/NL/PL/PT/RO/RU/TR (default: EN)
Per-limba: `locale_string_ro.txt`, `translate_ro.lua`, `item_names_ro.txt`, `mob_names_ro.txt`

## Credentiale default

| Serviciu | User | Parola |
|---|---|---|
| SSH | root | 123456789 |
| MySQL | root | 123456789 |
| Ingame GM | admin | 123456789 |

## GM Commands

Vezi **`.claude/references/commands_gm.md`**.
Spawn mob: `/mob <vnum>` (nu `/spawn`).

## Database Schema

Schema verificata: **`.claude/references/db_schema.md`**

Fapte critice:

- `account.account` (nu `accounts`) — coloane: login, password, email, status, availDt, empire
- `player.player` — fara coloana empire
- `player.player_index` — empire autoritar; `id` = account_id, `pid1–4` = sloturi personaje
- Empire: `LEFT JOIN player_index ON player_index.id = player.account_id`
- `common.gmlist` — conturi GM cu mAuthority

## Sistem traduceri (LC_TEXT)

Sursa C++ foloseste stringuri **engleze ASCII**. Flux:

1. `LC_TEXT("English")` → cauta in `locale_string.txt` (en→en, identitate)
2. `LC_TEXT_LANG(text, lang)` → cauta in `locale_string_ro.txt` etc. per jucator

`LOCALE_ERROR` in syserr = cheia lipseste din `locale_string.txt`.

## Verificare inainte de git commit

Verifica fisierele tracked (`.cpp`, `.h`, `.txt`, `.lua`) pentru coruptie encoding:

- **Semn coruptie:** secvente `?` sau `\x80-\x9F` in loc de coreene originale
- **Verifica:** `git diff` — daca afiseaza `?` in loc de coreene, analizeaza:
  - Doar **comentarii** corupte (linii `//` sau `/* */`) → OK, commit permis
  - Stringuri in cod, define-uri, nume variabile → NU da commit, revert + re-aplica pastrand encoding
- **Cauza:** Edit tool scrie UTF-8 peste EUC-KR
- **Fix (cand e cod afectat):** `perl -i` sau `sed` pastrand encoding original, sau editeaza direct pe FreeBSD
- **Sensibile:** `.cpp`/`.h` din `src/` cu coreene, `locale_string*.txt`, `translate_*.lua`

## Structura runtime FreeBSD (reala)

```text
/usr/metin2/server/db/          → syslog, syserr (direct in folder, NU in log/)
/usr/metin2/server/auth/        → syslog, syserr
/usr/metin2/server/channel1/game1/  → syslog, syserr
/usr/metin2/server/channel2/game1/  → syslog, syserr
/usr/metin2/server/channel{1-4}/game{1-2}/
/usr/metin2/server/db/log/      → doar core dumps
/usr/metin2/src/server/db/src/  → sursa db (compilare)
/usr/metin2/src/server/game/src/ → sursa game (compilare)
```

## Troubleshooting

- **Syserr/syslog paths**: direct in `/usr/metin2/server/db/`, `/usr/metin2/server/channel1/game1/` etc. (NU in subdosarul `log/`)
- **Connection refused**: verifica syserr db/auth/channels
- **Jucatori kickati dupa charselect**: BIND_IP/PROXY_IP gresit
- **Compilare pe x64**: foloseste jail 32-bit
