-- migrate:up
CREATE TABLE auth_user (
    id       INTEGER PRIMARY KEY AUTOINCREMENT,
    username TEXT    NOT NULL UNIQUE,
    password TEXT    NOT NULL
);

CREATE TABLE story (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    title       TEXT    NOT NULL,
    body        TEXT    NOT NULL DEFAULT '',
    link        TEXT,
    created_by  INTEGER NOT NULL REFERENCES auth_user(id),
    created_at  INTEGER NOT NULL DEFAULT (unixepoch())
);

-- migrate:down
DROP TABLE story;
DROP TABLE auth_user;

