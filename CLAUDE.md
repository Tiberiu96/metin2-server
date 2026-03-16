# metin2-server

Metin2 Reference Server r40250 by TMP4. Server privat MMORPG rulat pe **FreeBSD 13.5+** (32-bit).

## Structura repo

```
server/   → runtime server (auth, channel1-4, db, game99, share/)
src/      → sursa C++ (game, db, libgame, liblua, libthecore, etc.)
sql/      → schema baze de date (account, player, common, log)
```

## Server path pe FreeBSD

```
/usr/metin2/server/   → server/
/usr/metin2/src/      → src/
```

## Comenzi server

```sh
start       # cd /usr/metin2/server && sh start.sh
close       # cd /usr/metin2/server && sh close.sh
clean       # cd /usr/metin2/server && sh clean.sh
backup      # cd /usr/metin2/server && sh backup.sh
```

## Compilare sursa

Compilarea este posibila **doar pe FreeBSD 32-bit**. Pachete necesare:
```sh
pkg install llvm-devel gmake makedepend
```

```sh
# Compilare db
cd /usr/metin2/src/server/db/src && gmake dep && gmake -j9

# Compilare game
cd /usr/metin2/src/server/game/src && gmake dep && gmake -j9

# Binarele generate merg in: server/share/bin/
```

## Compilare quests

```sh
cd /usr/metin2/server/share/locale/english/quest && python2.7 make.py
```

## Configuratie IP (CONFIG files)

Fiecare server (auth, channel*/first, channel*/game1, channel*/game2, game99) are un fisier CONFIG cu:
```
BIND_IP: 192.168.x.x     # IP intern/privat (ifconfig)
PROXY_IP: 77.88.99.111   # IP extern/public
```
Decomentati liniile (stergeti `#`) si setati IP-urile corecte.

## Baze de date

- **MariaDB 10.6** (recomandat) sau MySQL 5.6
- Baze de date: `account`, `player`, `common`, `log`
- SQL dumps in `sql/`
- User MySQL: `metin2@localhost` / `root@%`
- `sql/my.cnf` → `/usr/local/etc/mysql/my.cnf`

Setup MariaDB:
```sh
pkg install mariadb106-client mariadb106-server
echo 'mysql_enable="yes"' >> /etc/rc.conf
service mysql-server start
```

## Limbi disponibile

EN/DE/HU/FR/CZ/DK/ES/GR/IT/NL/PL/PT/RO/RU/TR (default: EN)

Pentru schimbare limba (ex: DE):
1. `server/share/conf/item_names_de.txt` → `item_names.txt`
2. `server/share/conf/mob_names_de.txt` → `mob_names.txt`
3. `server/share/locale/english/translate_de.lua` → activ
4. Recompileaza quests: `questcompile`
5. Redenumeste `server/share/locale/english/` → `germany/`
6. Actualizeaza `common.locale` table in DB: `LOCALE` → `germany`

## Credentiale default

| Serviciu | User | Parola |
|---|---|---|
| SSH | root | 123456789 |
| MySQL | root | 123456789 |
| Ingame GM | admin | 123456789 |

## Troubleshooting

- **Connection refused**: verifica `log/syserr` in db, auth si channels
- **Eroare frecventa**: db nu se poate conecta la MySQL → `socket_connect: HOST 127.0.0.1:15000`
- **Jucatori kickati dupa charselect**: BIND_IP/PROXY_IP neconfigurat corect
- **Compilare pe x64**: foloseste jail 32-bit
