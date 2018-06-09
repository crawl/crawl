import crypt
import sqlite3
import re
import os.path
import logging
import random
import smtplib
import hashlib

from config import (max_passwd_length, nick_regex, password_db, settings_db,
                    crypt_algorithm, crypt_salt_length,
                    lobby_url,
                    smtp_use_ssl, smtp_host, smtp_port, smtp_user, smtp_password, smtp_from_addr)

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
                  " password text, flags integer," +
                  " password_reset_token text NULL, password_reset_time text NULL);")
        c.execute(schema)
        conn.commit()
    finally:
        if c: c.close()
        if conn: conn.close()

saltchars = "./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"

def make_salt(saltlen):
    return ''.join(random.choice(saltchars) for x in xrange(0,saltlen))

def encrypt_pw(passwd):
    passwd = passwd[0:max_passwd_length]
    if crypt_algorithm == "broken":
        salt = passwd
    elif crypt_algorithm:
        salt = "$%s$%s$" % (crypt_algorithm, make_salt(crypt_salt_length))
    else:
        salt = make_salt(2)
    return crypt.crypt(passwd, salt)

def register_user(username, passwd, email): # Returns an error message or None
    if passwd == "": return "The password can't be empty!"
    username = username.strip()
    if not re.match(nick_regex, username): return "Invalid username!"

    crypted_pw = encrypt_pw(passwd)

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

def update_user_password_from_token(token, passwd): # Returns a tuple where item 1 is the username that was found for the given token, and item 2 is an error message or None
    if passwd == "": return None, "The password can't be empty!"
    crypted_pw = encrypt_pw(passwd)

    token_hash_obj = hashlib.sha256(token)
    token_hash = token_hash_obj.hexdigest()

    try:
        conn = sqlite3.connect(password_db)
        c = conn.cursor()
        c.execute("select username from dglusers where password_reset_token=? and password_reset_time > datetime('now','-1 hour') collate nocase",
                  (token_hash,))
        result = c.fetchone()

        if not result:
            return None, "Invalid token"
        else:
            c.execute("update dglusers set password=?, password_reset_token=null, password_reset_time=null where password_reset_token=?",
                      (crypted_pw, token_hash))
            conn.commit()

            return result[0], None
    finally:
        if c: c.close()
        if conn: conn.close()

def send_forgot_password(email): # Returns a tuple where item 1 is a truthy value when an email was sent, and item 2 is an error message or None
    if email == "": return False, "The email can't be empty!"

    try:
        conn = sqlite3.connect(password_db)
        c = conn.cursor()
        c.execute("select email from dglusers where email=? collate nocase",
                  (email,))
        result = c.fetchone()

        if result:
            token = str(random.getrandbits(64))
            token_hash_obj = hashlib.sha256(token)
            token_hash = token_hash_obj.hexdigest()
            c.execute("update dglusers set password_reset_token=?, password_reset_time=datetime('now') where email=?", (token_hash, email))
            conn.commit()

            if smtp_use_ssl:
                server = smtplib.SMTP_SSL(smtp_host, smtp_port)
            else:
                server = smtplib.SMTP(smtp_host, smtp_port)

            server.login(smtp_user, smtp_password)
 
            msg = """Someone (hopefully you) has requested to reset the password for your account at """ + lobby_url + """.

If you initiated this request, please use this link to reset your password:

    """ + lobby_url + """?ResetToken=""" + token + """

If you did not ask to reset your password, feel free to ignore this email."""
            server.sendmail(smtp_from_addr, email, msg)
            server.quit()

            return True, None

        return False, None
    finally:
        if c: c.close()
        if conn: conn.close()
