import crypt
import sqlite3
import re
import os.path
import logging
import random
import datetime

from conf import config

def user_passwd_match(username, passwd): # Returns the correctly cased username.
    try:
        passwd = passwd[0:config.max_passwd_length]
    except:
        return None

    try:
        conn = sqlite3.connect(config.password_db)
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
    if os.path.exists(config.password_db): return
    logging.warn("User database didn't exist; creating it now.")
    c = None
    conn = None
    try:
        conn = sqlite3.connect(config.password_db)
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
    passwd = passwd[0:config.max_passwd_length]
    username = username.strip()
    if not re.match(config.nick_regex, username): return "Invalid username!"

    if config.crypt_algorithm == "broken":
        salt = passwd
    elif config.crypt_algorithm:
        salt = "$%s$%s$" % (config.crypt_algorithm,
                            make_salt(config.crypt_salt_length))
    else:
        salt = make_salt(2)

    crypted_pw = crypt.crypt(passwd, salt)

    try:
        conn = sqlite3.connect(config.password_db)
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

login_tokens = {}
rand = random.SystemRandom()

def purge_login_tokens():
    for k in list(login_tokens.keys()):
        expires, token = login_tokens[k]
        if datetime.datetime.now() > login_tokens[k]:
            del login_tokens[k]

def delete_logins(username):
    for (seqid, un) in list(login_tokens.keys()):
        if un == username:
            del login_tokens[(seqid, un)]

def token_login(cookie, logger=logging):
    username, token, seqid = cookie.split(":")
    try:
        token = long(token)
        seqid = long(seqid)
    except ValueError:
        return None, None
    if (seqid, username) in login_tokens:
        expires, true_token = login_tokens[(seqid, username)]
        del login_tokens[(seqid, username)]
        if true_token == token:
            logger.info("User %s logged in (via token).", username)
            return username, get_login_cookie(username, seqid=seqid)
        else:
            logger.warning("Bad login token for user %s!", username)
            delete_logins(username)
            return None, None
    else:
        return None, None

def get_login_cookie(username, seqid=None):
    token = rand.getrandbits(128)
    if seqid is None: seqid = rand.getrandbits(128)
    expires = datetime.datetime.now() + datetime.timedelta(config.login_token_lifetime)
    login_tokens[(seqid, username)] = (expires, token)
    cookie = username + ":" + str(token) + ":" + str(seqid)
    return cookie

def forget_login_cookie(cookie):
    username, token, seqid = cookie.split(":")
    try:
        token = long(token)
        seqid = long(seqid)
    except ValueError:
        return
    if (seqid, username) in login_tokens:
        del login_tokens[(seqid, username)]
