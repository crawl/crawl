import crypt
import sqlite3
import re
import os.path
import logging

from config import max_passwd_length, nick_regex, password_db

def user_passwd_match(username, passwd): # Returns the correctly cased username.
    try:
        passwd = passwd[0:max_passwd_length]
        crypted_pw = crypt.crypt(passwd, passwd)
    except:
        return None

    try:
        conn = sqlite3.connect(password_db)
        c = conn.cursor()
        c.execute("select username,password from dglusers where username=? collate nocase",
                  (username,))
        result = c.fetchone()

        if result is None or crypted_pw != result[1]:
            return None
        else:
            return result[0]
    finally:
        if c: c.close()
        if conn: conn.close()

def ensure_user_db_exists():
    if os.path.exists(password_db): return
    logging.warn("User database didn't exist; creating it now.")
    try:
        conn = sqlite3.connect(password_db)
        c = conn.cursor()
        schema = ("CREATE TABLE dglusers (id integer primary key," +
                  " username text, email text, env text," +
                  " password text, flags integer);")
        c.execute(schema)
        conn.commit()
    finally:
        if c: c.close()
        if conn: conn.close()


def register_user(username, passwd, email): # Returns an error message or None
    if passwd == "": return "The password can't be empty!"
    passwd = passwd[0:max_passwd_length]
    username = username.strip()
    if not re.match(nick_regex, username): return "Invalid username!"

    crypted_pw = crypt.crypt(passwd, passwd)

    try:
        conn = sqlite3.connect(password_db)
        c = conn.cursor()
        c.execute("select username from dglusers where username=? collate nocase",
                  (username,))
        result = c.fetchone()

        if result: return "User already exists!"

        c.execute("insert into dglusers(username, email, password, flags, env) values (?,?,?,0,'')",
                  (username, email, crypted_pw))

        conn.commit()

        return None
    finally:
        if c: c.close()
        if conn: conn.close()
