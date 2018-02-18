import crypt
import sqlite3
import re
import os.path
import logging
import random

from config import (max_passwd_length, nick_regex, password_db, settings_db,
                    crypt_algorithm, crypt_salt_length)

def ensure_settings_db_exists():
    if os.path.exists(settings_db):
        return
    logging.warn("User settings database didn't exist at '%s'; creating it now." % settings_db)
    c = None
    conn = None
    try:
        conn = sqlite3.connect(settings_db)
        c = conn.cursor()
        schema = ("CREATE TABLE mutesettings (username text primary key not null unique, mutelist text default '');")
        c.execute(schema)
        conn.commit()
    finally:
        if c:
            c.close()
        if conn:
            conn.close()

def get_mutelist(username):
    # TODO: based on userdb; why doesn't this just keep the database open?
    # I think sqlite can handle that...
    try:
        conn = sqlite3.connect(settings_db)
        c = conn.cursor()
        c.execute("select mutelist from mutesettings where username=? collate nocase",
                  (username,))
        result = c.fetchone()

        if result is None:
            return None
        else:
            return result[0]
    finally:
        if c: c.close()
        if conn: conn.close()

def set_mutelist(username, mutelist):
    # TODO: based on userdb; why doesn't this just keep the database open?
    # I think sqlite can handle that...
    try:
        conn = sqlite3.connect(settings_db)
        c = conn.cursor()
        if mutelist is None:
            mutelist = ""

        # n.b. the following will wipe out any columns not mentioned, if there
        # ever are any...
        c.execute("insert or replace into mutesettings(username, mutelist) values (?,?)",
                  (username, mutelist))

        conn.commit()
    finally:
        if c:
            c.close()
        if conn:
            conn.close()


def user_passwd_match(username, passwd): # Returns the correctly cased username.
    try:
        passwd = passwd[0:max_passwd_length]
    except:
        return None

    try:
        conn = sqlite3.connect(password_db)
        c = conn.cursor()
        c.execute("select username,password from dglusers where username=? collate nocase",
                  (username,))
        result = c.fetchone()

        if result is None:
            return None
        elif crypt.crypt(passwd, result[1]) == result[1]:
            return result[0]
    finally:
        if c: c.close()
        if conn: conn.close()

def ensure_user_db_exists():
    if os.path.exists(password_db): return
    logging.warn("User database didn't exist; creating it now.")
    c = None
    conn = None
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

saltchars = "./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"

def make_salt(saltlen):
    return ''.join(random.choice(saltchars) for x in xrange(0,saltlen))

def register_user(username, passwd, email): # Returns an error message or None
    if passwd == "": return "The password can't be empty!"
    passwd = passwd[0:max_passwd_length]
    username = username.strip()
    if not re.match(nick_regex, username): return "Invalid username!"

    if crypt_algorithm == "broken":
        salt = passwd
    elif crypt_algorithm:
        salt = "$%s$%s$" % (crypt_algorithm, make_salt(crypt_salt_length))
    else:
        salt = make_salt(2)

    crypted_pw = crypt.crypt(passwd, salt)

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
