CREATE TABLE "schema_migrations" (version varchar(128) primary key);
CREATE TABLE auth_user (
    id       INTEGER PRIMARY KEY AUTOINCREMENT,
    username TEXT    NOT NULL UNIQUE,
    password TEXT    NOT NULL
);
CREATE TABLE a_post (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    title       TEXT    NOT NULL,
    body        TEXT    NOT NULL DEFAULT '',
    link        TEXT,
    created_by  INTEGER NOT NULL REFERENCES auth_user(id),
    created_at  INTEGER NOT NULL DEFAULT (unixepoch())
);
-- Dbmate schema migrations
INSERT INTO "schema_migrations" (version) VALUES
  ('20261122180853');
