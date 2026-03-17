-- Rulat o singura data la setup initial, ca root:
-- mysql -u root -p < sql/init_db_user.sql

CREATE USER IF NOT EXISTS 'metin2'@'localhost' IDENTIFIED BY 'password';

GRANT ALL PRIVILEGES ON account.*   TO 'metin2'@'localhost';
GRANT ALL PRIVILEGES ON common.*    TO 'metin2'@'localhost';
GRANT ALL PRIVILEGES ON player.*    TO 'metin2'@'localhost';
GRANT ALL PRIVILEGES ON log.*       TO 'metin2'@'localhost';
GRANT ALL PRIVILEGES ON hotbackup.* TO 'metin2'@'localhost';

FLUSH PRIVILEGES;
