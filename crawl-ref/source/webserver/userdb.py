import crypt
import sqlite3
import re
import os.path
import logging
import random
import hashlib

from base64 import urlsafe_b64encode

import config
from config import (max_passwd_length, nick_regex, password_db,
                    crypt_algorithm, crypt_salt_length)

from util import (send_email, validate_email_address)

def setup_settings_path():
    global settings_db
    try:
        from config import settings_db as sdb_config
        settings_db = sdb_config
    except:
        settings_db = os.path.join(os.path.dirname(password_db), "user_settings.db3")

setup_settings_path()

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
        schema = ("CREATE TABLE recovery_tokens (token text primary key,"
                   " token_time text, user_id integer not null,"
                   " foreign key(user_id) references dglusers(id));")
        c.execute(schema)
        conn.commit()
    finally:
        if c: c.close()
        if conn: conn.close()

# automatically upgrades database
def upgrade_user_db():
    c = None
    conn = None
    try:
        conn = sqlite3.connect(password_db)
        c = conn.cursor()
        tables = [i[0] for i in c.execute("SELECT name FROM sqlite_master WHERE type='table';")]

        if "recovery_tokens" not in tables:
            logging.warn("User database missing table 'recovery_tokens'; adding now")
            schema = ("CREATE TABLE recovery_tokens (token text primary key,"
                      " token_time text, user_id integer not null,"
                      " foreign key(user_id) references dglusers(id));")
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
    if email: # validate the email only if it is provided
        result = validate_email_address(email)
        if result: return result
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

def find_recovery_token(token): # Returns tuple (userid, username, error)
    token_hash_obj = hashlib.sha256(token)
    token_hash = token_hash_obj.hexdigest()

    try:
        conn = sqlite3.connect(password_db)
        c = conn.cursor()
        c.execute("""\
select u.id, u.username, case when t.token_time > datetime('now','-1 hour') then 'N' else 'Y' end as Expired
from recovery_tokens t
join dglusers u on u.id = t.user_id
where t.token = ?
collate rtrim
""", (token_hash,))
        result = c.fetchone()

        if not result:
            return None, None, "Invalid token"
        else:
            userid = result[0]
            username = result[1]
            expired = result[2]
            if expired == 'Y':
                return userid, username, "Expired token"
            else:
                return userid, username, None
    finally:
        if c: c.close()
        if conn: conn.close()

def update_user_password_from_token(token, passwd): # Returns a tuple where item 1 is the username that was found for the given token, and item 2 is an error message or None
    if passwd == "": return None, "The password can't be empty!"
    crypted_pw = encrypt_pw(passwd)

    userid, username, token_error = find_recovery_token(token)

    if userid and not token_error:
        try:
            conn = sqlite3.connect(password_db)
            c = conn.cursor()
            c.execute("update dglusers set password=? where id=?",
                    (crypted_pw, userid))
            c.execute("delete from recovery_tokens where user_id=?",
                    (userid,))
            conn.commit()
        finally:
            if c: c.close()
            if conn: conn.close()

    return username, token_error

def send_forgot_password(email): # Returns a tuple where item 1 is a truthy value when an email was sent, and item 2 is an error message or None
    email_error = validate_email_address(email)
    if email_error: return False, email_error

    try:
        # lookup user-provided email
        conn = sqlite3.connect(password_db)
        c = conn.cursor()
        c.execute("select id from dglusers where email=? collate nocase",
                  (email,))
        result = c.fetchone()

        # user was found
        if result:
            userid = result[0]
            # generate random token
            token_bytes = os.urandom(32)
            token = urlsafe_b64encode(token_bytes)
            # hash token
            token_hash_obj = hashlib.sha256(token)
            token_hash = token_hash_obj.hexdigest()
            # store hash in db
            c.execute("insert into recovery_tokens(token, token_time, user_id) "
                      "values (?,datetime('now'),?)", (token_hash, userid))
            conn.commit()

            # send email
            url_text = config.lobby_url + "?ResetToken=" + token

            msg_body_plaintext = """Someone (hopefully you) has requested to reset the password for your account at """ + config.lobby_url + """.

If you initiated this request, please use this link to reset your password:

    """ + url_text + """

If you did not ask to reset your password, feel free to ignore this email.
"""

            msg_body_html = """<html>
  <head></head>
  <body>
    <p>Someone (hopefully you) has requested to reset the password for your account at """ + config.lobby_url + """.<br /><br />
       If you initiated this request, please use this link to reset your password:<br /><br />
       &emsp;<a href='""" + url_text + """'>""" + url_text + """</a><br /><br />
       If you did not ask to reset your password, feel free to ignore this email.
    </p>
  </body>
</html>"""

            send_email(email, 'Request to reset your password',
                        msg_body_plaintext, msg_body_html)

            return True, None

        # email was not found, do nothing
        return False, None
    finally:
        if c: c.close()
        if conn: conn.close()
