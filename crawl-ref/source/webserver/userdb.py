import crypt
import hashlib
import logging
import os.path
import random
import re
import sqlite3
from base64 import urlsafe_b64encode

import config
from config import crypt_algorithm
from config import crypt_salt_length
from config import max_passwd_length
from config import nick_regex
from config import password_db
from util import send_email
from util import validate_email_address


# TODO: based on userdb; why doesn't this just keep the database open?
# I think sqlite can handle that...
class crawl_db(object):
    def __init__(self, name):
        self.name = name

    def __enter__(self):
        self.conn = sqlite3.connect(self.name)
        self.c = self.conn.cursor()
        return self

    def __exit__(self, *_):
        if self.c:
            self.c.close()
        if self.conn:
            self.conn.close()


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
    create_settings_db()


def create_settings_db():
    schema = ("CREATE TABLE mutesettings (username text primary key not null unique, mutelist text default '');")
    with crawl_db(settings_db) as db:
        db.c.execute(schema)
        db.conn.commit()


def get_mutelist(username):
    with crawl_db(settings_db) as db:
        db.c.execute("select mutelist from mutesettings where username=? collate nocase",
                     (username,))
        result = db.c.fetchone()
    return result[0] if result is not None else None


def set_mutelist(username, mutelist):
    if mutelist is None:
        mutelist = ""

    # n.b. the following will wipe out any columns not mentioned, if there
    # ever are any...
    with crawl_db(settings_db) as db:
        db.c.execute("insert or replace into mutesettings(username, mutelist) values (?,?)",
                     (username, mutelist))
        db.conn.commit()


# from dgamelaunch.h
DGLACCT_ADMIN = 1
DGLACCT_LOGIN_LOCK = 2
DGLACCT_PASSWD_LOCK = 4
DGLACCT_EMAIL_LOCK = 8


def dgl_is_admin(flags):
    return bool(flags & DGLACCT_ADMIN)


def dgl_is_banned(flags):
    return bool(flags & DGLACCT_LOGIN_LOCK)

# TODO: something with other lock flags?


def get_user_info(username):
    """Returns user data in a tuple (userid, email)."""
    with crawl_db(password_db) as db:
        db.c.execute("select id,email,flags from dglusers where username=? collate nocase",
                     (username,))
        result = db.c.fetchone()
    return (result[0], result[1], result[2]) if result is not None else None


def user_passwd_match(username, passwd):
    """Returns the correctly cased username."""
    passwd = passwd[0:max_passwd_length]

    with crawl_db(password_db) as db:
        db.c.execute("select username,password from dglusers where username=? collate nocase",
                     (username,))
        result = db.c.fetchone()

    if result is None:
        return None
    elif crypt.crypt(passwd, result[1]) == result[1]:
        return result[0]


def ensure_user_db_exists():
    if os.path.exists(password_db):
        return
    logging.warn("User database didn't exist; creating it now.")
    create_user_db()


def create_user_db():
    with crawl_db(password_db) as db:
        schema = ("CREATE TABLE dglusers (id integer primary key," +
                  " username text, email text, env text," +
                  " password text, flags integer);")
        db.c.execute(schema)
        schema = ("CREATE TABLE recovery_tokens (token text primary key,"
                  " token_time text, user_id integer not null,"
                  " foreign key(user_id) references dglusers(id));")
        db.c.execute(schema)
        db.conn.commit()


def upgrade_user_db():
    """Automatically upgrades the database."""
    with crawl_db(password_db) as db:
        tables = [i[0] for i in db.c.execute("SELECT name FROM sqlite_master WHERE type='table';")]

        if "recovery_tokens" not in tables:
            logging.warn("User database missing table 'recovery_tokens'; adding now")
            schema = ("CREATE TABLE recovery_tokens (token text primary key,"
                      " token_time text, user_id integer not null,"
                      " foreign key(user_id) references dglusers(id));")
            db.c.execute(schema)
            db.conn.commit()


_SALTCHARS = "./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"


def make_salt(saltlen):
    return ''.join(random.choice(_SALTCHARS) for x in range(0, saltlen))


def encrypt_pw(passwd):
    passwd = passwd[0:max_passwd_length]
    if crypt_algorithm == "broken":
        salt = passwd
    elif crypt_algorithm:
        salt = "$%s$%s$" % (crypt_algorithm, make_salt(crypt_salt_length))
    else:
        salt = make_salt(2)
    return crypt.crypt(passwd, salt)


def register_user(username, passwd, email):  # Returns an error message or None
    if passwd == "":
        return "The password can't be empty!"
    if email:  # validate the email only if it is provided
        result = validate_email_address(email)
        if result:
            return result
    username = username.strip()
    if not re.match(nick_regex, username):
        return "Invalid username!"

    crypted_pw = encrypt_pw(passwd)

    with crawl_db(password_db) as db:
        db.c.execute("select username from dglusers where username=? collate nocase",
                     (username,))
        result = db.c.fetchone()

    if result:
        return "User already exists!"

    with crawl_db(password_db) as db:
        db.c.execute("insert into dglusers(username, email, password, flags, env) values (?,?,?,0,'')",
                     (username, email, crypted_pw))
        db.conn.commit()

    return None


def change_email(user_id, email):  # Returns an error message or None
    result = validate_email_address(email)
    if result:
        return result

    with crawl_db(password_db) as db:
        db.c.execute("update dglusers set email=? where id=?", (email, user_id))
        db.conn.commit()

    return None


def find_recovery_token(token):
    # Returns tuple (userid, username, error)
    token_hash_obj = hashlib.sha256(token)
    token_hash = token_hash_obj.hexdigest()

    with crawl_db(password_db) as db:
        db.c.execute("""\
select u.id, u.username, case when t.token_time > datetime('now','-1 hour') then 'N' else 'Y' end as Expired
from recovery_tokens t
join dglusers u on u.id = t.user_id
where t.token = ?
collate rtrim
""", (token_hash,))
        result = db.c.fetchone()

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


def update_user_password_from_token(token, passwd):
    """
    Returns:
      (None, error string)
      (username, error string)
      (username, None)
    """
    if passwd == "":
        return None, "The password can't be empty!"
    crypted_pw = encrypt_pw(passwd)

    userid, username, token_error = find_recovery_token(token)

    if userid and not token_error:
        with crawl_db(password_db) as db:
            db.c.execute("update dglusers set password=? where id=?",
                         (crypted_pw, userid))
            db.c.execute("delete from recovery_tokens where user_id=?",
                         (userid,))
            db.conn.commit()

    return username, token_error


def send_forgot_password(email):
    """
    Returns:
        (email_sent: bool, error: string)
    """
    if not email:
        return False, "Email address can't be empty"
    email_error = validate_email_address(email)
    if email_error:
        return False, email_error

    with crawl_db(password_db) as db:
        db.c.execute("select id from dglusers where email=? collate nocase", (email,))
        result = db.c.fetchone()
    if not result:
        return False, None

    userid = result[0]
    # generate random token
    token_bytes = os.urandom(32)
    token = urlsafe_b64encode(token_bytes)
    # hash token
    token_hash_obj = hashlib.sha256(token)
    token_hash = token_hash_obj.hexdigest()
    # store hash in db
    with crawl_db(password_db) as db:
        db.c.execute("insert into recovery_tokens(token, token_time, user_id) "
                     "values (?,datetime('now'),?)", (token_hash, userid))
        db.conn.commit()

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
