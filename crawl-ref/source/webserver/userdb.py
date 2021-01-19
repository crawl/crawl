import crypt
import hashlib
import logging
import os.path
import random
import re
import aiosqlite
from base64 import urlsafe_b64encode

from tornado.escape import to_unicode
from tornado.escape import utf8

import config
import util

try:
    from typing import Any, Optional, Tuple, Union
except ImportError:
    pass


class crawl_db(object):
    # TODO: based on userdb; why doesn't this just keep the database open?
    # I think sqlite can handle that...
    def __init__(self, name):  # type: (str) -> None
        self.name = name

    async def __aenter__(self):  # type: () -> 'crawl_db'
        self.conn = await aiosqlite.connect(self.name)
        self.c = await self.conn.cursor()
        return self

    async def __aexit__(self, *_):  # type: (Any) -> None
        if self.c:
            await self.c.close()
        if self.conn:
            await self.conn.close()


def setup_settings_path():  # type: () -> str
    try:
        return str(config.settings_db)  # type: ignore
    except AttributeError:
        return os.path.join(os.path.dirname(config.password_db),
                            "user_settings.db3")


settings_db = setup_settings_path()


async def ensure_settings_db_exists():  # type: () -> None
    if os.path.exists(settings_db):
        return
    logging.warn("User settings database didn't exist at '%s'; creating it now.",
                 settings_db)
    await create_settings_db()


async def create_settings_db():  # type: () -> None
    schema = """
        CREATE TABLE mutesettings (
            username TEXT PRIMARY KEY NOT NULL UNIQUE,
            mutelist TEXT DEFAULT ''
        );
    """
    async with crawl_db(settings_db) as db:
        await db.c.execute(schema)
        await db.conn.commit()


async def get_mutelist(username):  # type: (str) -> Optional[str]
    async with crawl_db(settings_db) as db:
        await db.c.execute("select mutelist from mutesettings where username=? collate nocase",
                     (username,))
        result = await db.c.fetchone()
    return result[0] if result is not None else None


async def set_mutelist(username, mutelist):  # type: (str, Optional[str]) -> None
    if mutelist is None:
        mutelist = ""

    # n.b. the following will wipe out any columns not mentioned, if there
    # ever are any...
    async with crawl_db(settings_db) as db:
        query = """
            INSERT OR REPLACE INTO mutesettings
                (username, mutelist)
            VALUES
                (?,?);
        """
        await db.c.execute(query, (username, mutelist))
        await db.conn.commit()


# from dgamelaunch.h
DGLACCT_ADMIN = 1
DGLACCT_LOGIN_LOCK = 2
DGLACCT_PASSWD_LOCK = 4
DGLACCT_EMAIL_LOCK = 8


def dgl_is_admin(flags):  # type: (int) -> bool
    return bool(flags & DGLACCT_ADMIN)


def dgl_is_banned(flags):  # type: (int) -> bool
    return bool(flags & DGLACCT_LOGIN_LOCK)

# TODO: something with other lock flags?


async def get_user_info(username):  # type: (str) -> Optional[Tuple[int, str, int]]
    """Returns user data in a tuple (userid, email)."""
    async with crawl_db(config.password_db) as db:
        query = """
            SELECT id, email, flags
            FROM dglusers
            WHERE username=?
            COLLATE NOCASE
        """
        await db.c.execute(query, (username,))
        result = await db.c.fetchone()  # type: Optional[Tuple[int, str, int]]
    if result:
        return (result[0], result[1], result[2])
    else:
        return None


async def user_passwd_match(username, passwd):  # type: (str, str) -> Optional[str]
    """Returns the correctly cased username."""
    passwd = passwd[0:config.max_passwd_length]

    async with crawl_db(config.password_db) as db:
        query = """
            SELECT username, password
            FROM dglusers
            WHERE username=?
            COLLATE NOCASE
        """
        await db.c.execute(query, (username,))
        result = await db.c.fetchone()  # type: Optional[Tuple[str, str]]

    if result and crypt.crypt(passwd, result[1]) == result[1]:
        return result[0]
    else:
        return None


async def ensure_user_db_exists():  # type: () -> None
    if os.path.exists(config.password_db):
        return
    logging.warn("User database didn't exist; creating it now.")
    await create_user_db()


async def create_user_db():  # type: () -> None
    async with crawl_db(config.password_db) as db:
        schema = ("CREATE TABLE dglusers (id integer primary key," +
                  " username text, email text, env text," +
                  " password text, flags integer);")
        await db.c.execute(schema)
        schema = ("CREATE TABLE recovery_tokens (token text primary key,"
                  " token_time text, user_id integer not null,"
                  " foreign key(user_id) references dglusers(id));")
        await db.c.execute(schema)
        await db.conn.commit()


async def upgrade_user_db():  # type: () -> None
    """Automatically upgrades the database."""
    async with crawl_db(config.password_db) as db:
        query = "SELECT name FROM sqlite_master WHERE type='table';"
        await db.c.execute(query)
        tables = [i[0] async for i in db.c]

        if "recovery_tokens" not in tables:
            logging.warn("User database missing table 'recovery_tokens'; adding now")
            schema = ("CREATE TABLE recovery_tokens (token text primary key,"
                      " token_time text, user_id integer not null,"
                      " foreign key(user_id) references dglusers(id));")
            await db.c.execute(schema)
            await db.conn.commit()


_SALTCHARS = "./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"


def make_salt(saltlen):  # type: (int) -> str
    return ''.join(random.choice(_SALTCHARS) for x in range(0, saltlen))


def encrypt_pw(passwd):  # type: (str) -> str
    passwd = passwd[0:config.max_passwd_length]
    if config.crypt_algorithm == "broken":
        salt = passwd
    elif crypt_algorithm:
        salt = "${}${}$".format(config.crypt_algorithm,
                                make_salt(config.crypt_salt_length))
    else:
        salt = make_salt(2)
    return crypt.crypt(passwd, salt)


async def register_user(username, passwd, email):  # type: (str, str, str) -> Optional[str]
    """Returns an error message or None on success."""
    if passwd == "":
        return "The password can't be empty!"
    if email:  # validate the email only if it is provided
        result = util.validate_email_address(email)
        if result:
            return result
    username = username.strip()
    if not re.match(config.nick_regex, username):
        return "Invalid username!"

    crypted_pw = encrypt_pw(passwd)

    async with crawl_db(config.password_db) as db:
        await db.c.execute("select username from dglusers where username=? collate nocase",
                     (username,))
        result = await db.c.fetchone()

    if result:
        return "User already exists!"

    async with crawl_db(config.password_db) as db:
        query = """
            INSERT INTO dglusers
                (username, email, password, flags, env)
            VALUES
                (?, ?, ?, 0, '')
        """
        await db.c.execute(query, (username, email, crypted_pw))
        await db.conn.commit()

    return None

async def change_password(userid, passwd):
    if passwd == "":
        return "The password can't be empty."

    crypted_pw = encrypt_pw(passwd)

    async with crawl_db(config.password_db) as db:
        await db.c.execute("update dglusers set password=? where id=?",
                     (crypted_pw, userid))
        # invalidate any recovery tokens that might exist, even for normal
        # password changes:
        await db.c.execute("delete from recovery_tokens where user_id=?",
                     (userid,))
        await db.conn.commit()

    return None

async def change_email(user_id, email):  # type: (str, str) -> Optional[str]
    """Returns an error message or None on success."""
    result = util.validate_email_address(email)
    if result:
        return result

    async with crawl_db(config.password_db) as db:
        await db.c.execute("update dglusers set email=? where id=?", (email, user_id))
        await db.conn.commit()

    return None


async def find_recovery_token(token):
    # type: (str) -> Union[Tuple[None, None, str], Tuple[int, str, Optional[str]]]
    """Returns tuple (userid, username, error)"""
    token_hash_obj = hashlib.sha256(utf8(token))
    token_hash = token_hash_obj.hexdigest()

    async with crawl_db(config.password_db) as db:
        query = """
            SELECT
                u.id,
                u.username,
                CASE
                    WHEN t.token_time > datetime('now','-1 hour') THEN 'N'
                    ELSE 'Y'
                END AS Expired
            FROM recovery_tokens t
            JOIN dglusers u ON u.id = t.user_id
            WHERE t.token = ?
            COLLATE RTRIM
        """
        await db.c.execute(query, (token_hash,))
        result = await db.c.fetchone()

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


async def update_user_password_from_token(token, passwd):
    # type: (str, str) -> Tuple[Optional[str], Optional[str]]
    """
    Returns:
      (None, error string)
      (username, error string)
      (username, None)
    """
    if passwd == "":
        return None, "The password can't be empty!"
    crypted_pw = encrypt_pw(passwd)

    userid, username, token_error = await find_recovery_token(token)

    if userid and not token_error:
        async with crawl_db(config.password_db) as db:
            await db.c.execute("update dglusers set password=? where id=?",
                         (crypted_pw, userid))
            await db.c.execute("delete from recovery_tokens where user_id=?",
                         (userid,))
            await db.conn.commit()

    return username, token_error

async def clear_password_token(username):
    if not username:
        return False, "Invalid username"
    async with crawl_db(config.password_db) as db:
        await db.c.execute("select id from dglusers where username=? collate nocase", (username,))
        result = await db.c.fetchone()
    if not result:
        return False, "Invalid username"
    async with crawl_db(config.password_db) as db:
        await db.c.execute("delete from recovery_tokens where user_id=?",
                     (result[0],))
        await db.conn.commit()
    return True, ""

async def create_password_token(userid):
    # userid is not checked here
    token_bytes = os.urandom(32)
    token = urlsafe_b64encode(token_bytes)
    # hash token
    token_hash_obj = hashlib.sha256(token)
    token_hash = token_hash_obj.hexdigest()
    # store hash in db
    async with crawl_db(config.password_db) as db:
        await db.c.execute("insert into recovery_tokens(token, token_time, user_id) "
                     "values (?,datetime('now'),?)", (token_hash, userid))
        await db.conn.commit()
    return token

def generate_token_email(token):
    # send email
    lobby_url = getattr(config, 'lobby_url', '')  # note: hack to satisfy mypy
    url_text = lobby_url + "?ResetToken=" + to_unicode(token)

    msg_body_plaintext = """Someone (hopefully you) has requested to reset the password for your account at """ + lobby_url + """.

If you initiated this request, please use this link to reset your password:

    """ + url_text + """

If you did not ask to reset your password, feel free to ignore this email.
"""

    msg_body_html = """<html>
  <head></head>
  <body>
    <p>Someone (hopefully you) has requested to reset the password for your
       account at """ + lobby_url + """.<br /><br />
       If you initiated this request, please use this link to reset your
       password:<br /><br />
       &emsp;<a href='""" + url_text + """'>""" + url_text + """</a><br /><br />
       If you did not ask to reset your password, feel free to ignore this email.
    </p>
  </body>
</html>"""

    return msg_body_plaintext, msg_body_html

async def generate_forgot_password(username):
    if not username:
        return False, "Empty username"

    async with crawl_db(config.password_db) as db:
        await db.c.execute(
            "select id from dglusers where username=? collate nocase",
            (username,))
        result = await db.c.fetchone()
    if not result:
        return False, "Invalid username"

    userid = result[0]
    token = await create_password_token(userid)
    msg_body_plaintext, msg_body_html = generate_token_email(token)
    return True, msg_body_plaintext

async def send_forgot_password(email):  # type: (str) -> Tuple[bool, Optional[str]]
    """
    Returns:
        (email_sent: bool, error: string)
    """
    if not email:
        return "Email address can't be empty"
    email_error = util.validate_email_address(email)
    if email_error:
        return email_error

    async with crawl_db(config.password_db) as db:
        await db.c.execute(
            "select id from dglusers where email=? collate nocase", (email,))
        result = await db.c.fetchone()

    if not result:
        return "Unknown email '%s' for password reset" % email

    userid = result[0]
    token = await create_password_token(userid)
    msg_body_plaintext, msg_body_html = generate_token_email(token)

    await util.send_email(email, 'Request to reset your password',
               msg_body_plaintext, msg_body_html)

    return None
