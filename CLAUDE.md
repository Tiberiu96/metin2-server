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

## Setup de la zero (server nou)

1. **Instalezi pachetele necesare** (FreeBSD 32-bit):
```sh
pkg install llvm-devel gmake makedepend python27 mariadb106-client mariadb106-server git
```

2. **Pornesti MariaDB:**
```sh
echo 'mysql_enable="yes"' >> /etc/rc.conf
service mysql-server start
```

3. **Copiezi my.cnf:**
```sh
cp /usr/metin2/sql/my.cnf /usr/local/etc/mysql/my.cnf
service mysql-server restart
```

4. **Clonezi repo-ul:**
```sh
cd /usr && git clone https://github.com/Tiberiu96/metin2-server.git metin2
```

5. **Importi bazele de date:**
```sh
mysql -u root < /usr/metin2/sql/account.sql
mysql -u root < /usr/metin2/sql/common.sql
mysql -u root < /usr/metin2/sql/player.sql
mysql -u root < /usr/metin2/sql/log.sql
```

6. **Compilezi binarele:**
```sh
cd /usr/metin2/src/server/db/src && gmake dep && gmake -j9
cd /usr/metin2/src/server/game/src && gmake dep && gmake -j9
```

7. **Copiezi binarele:**
```sh
cp /usr/metin2/src/server/db/src/db /usr/metin2/server/share/bin/
cp /usr/metin2/src/server/game/src/game /usr/metin2/server/share/bin/
```

8. **Configurezi IP-urile** in toate fisierele CONFIG (vezi sectiunea Configuratie IP)

9. **Pornesti serverul:**
```sh
cd /usr/metin2/server && sh start.sh
```

---

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

## Migratii baza de date

Orice modificare la schema sau datele MySQL se adauga ca fisier incremental in `sql/migrations/`.

**Conventie de denumire:**
```
sql/migrations/001_descriere_scurta.sql
sql/migrations/002_alta_modificare.sql
```

**Reguli:**
- Numerotare secventiala cu 3 cifre: `001`, `002`, `003`...
- Numele descrie ce face modificarea (ex: `001_add_skill_table.sql`, `002_update_player_exp.sql`)
- Fiecare fisier trebuie sa fie idempotent unde e posibil (`IF NOT EXISTS`, `IF EXISTS`)
- Nu modifica niciodata un fisier de migratie deja aplicat — adauga unul nou

**Aplicare migratie pe server:**
```sh
mysql -u root player < /usr/metin2/sql/migrations/001_add_skill_table.sql
```

**Urmareste ce migratii au fost aplicate** — noteaza in commit message si in numele fisierului ordinea.

---

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
