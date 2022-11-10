import crypt
import hashlib
import logging
import os
import os.path
import random
import re
import sqlite3
from base64 import urlsafe_b64encode
import contextlib
import collections

from tornado.escape import to_unicode
from tornado.escape import utf8

from webtiles import config, util
from webtiles.util import send_email
from webtiles.util import validate_email_address

try:
    from typing import Any, Optional, Tuple, Union
except ImportError:
    pass

# convenience wrapper to make various context manager cases a little smoother,
# and handle error cases uniformly
class crawl_db(object):
    def __init__(self, name):  # type: (str) -> None
        self.name = name
        self.conn = sqlite3.connect(self.name) if self.name else None

    # needs to be manually closed, or used with contextlib.closing. However,
    # sqlite3 connection objects close on gc as well.
    def close(self):
        if self.conn:
            self.conn.close()

    def cursor(self):
        """Cursor in context manager form"""
        if not self.conn:
            raise sqlite3.ProgrammingError("Database connection not initialized!")
        return contextlib.closing(self.conn.cursor())

    # when used as a context manager itself, acts like the connection context
    # manager. In particular, if you use the db object as a context manager,
    # it will autocommit on exiting the context.
    def __enter__(self):
        if not self.conn:
            raise sqlite3.ProgrammingError("Database connection not initialized!")
        return self.conn.__enter__()

    def __exit__(self, *_):
        # XX I think the commit that this may entail is potentially blocking...
        # hopefully doesn't matter in practice
        return self.conn.__exit__(*_)

    # convenience wrapper for Connection.execute with context management
    # signature should maybe just be: execute(sql, parameters=(), /)
    def execute(self, *args, **kwargs):
        if not self.conn:
            raise sqlite3.ProgrammingError("Database connection not initialized!")
        return contextlib.closing(self.conn.execute(*args, **kwargs))

def create_settings_db(filename):  # type: () -> None
    schema = """
        CREATE TABLE mutesettings (
            username TEXT PRIMARY KEY NOT NULL UNIQUE,
            mutelist TEXT DEFAULT ''
        );
    """
    with contextlib.closing(crawl_db(filename)) as c:
        with c:
            c.execute(schema)


def ensure_settings_db_exists(quiet=False):
    dbname = config.get('settings_db')
    if not os.path.exists(dbname):
        if not quiet:
            logging.warning("User settings database didn't exist at '%s'; creating it now.",
                     dbname)
        create_settings_db(dbname)
    return crawl_db(dbname)


recovery_schema = """
    CREATE TABLE recovery_tokens (
        token TEXT PRIMARY KEY,
        token_time TEXT,
        user_id INTEGER NOT NULL,
        FOREIGN KEY(user_id) REFERENCES dglusers(id)
    );
"""
user_index_schema = """
    CREATE UNIQUE INDEX index_username
    ON dglusers (username COLLATE NOCASE);
    """


def create_user_db(filename):
    global recovery_schema, user_index_schema
    dglusers_schema = """
        CREATE TABLE dglusers (
            id INTEGER PRIMARY KEY,
            username TEXT,
            email TEXT,
            env TEXT,
            password TEXT,
            flags INTEGER
        );
    """
    with contextlib.closing(crawl_db(filename)) as c:
        with c:
            # maybe executescript would be better?
            c.execute(dglusers_schema)
            c.execute(user_index_schema)
            c.execute(recovery_schema)


def ensure_user_db_exists(quiet=False):  # type: () -> None
    dbname = config.get('password_db')
    if not os.path.exists(dbname):
        if not quiet:
            logging.warning("User database '%s' didn't exist; creating it now.", dbname)
        create_user_db(dbname)
    return crawl_db(dbname)


settings_db = crawl_db("")
user_db = crawl_db("")


def upgrade_user_db():  # type: () -> None
    """Automatically upgrades the database."""
    global user_db, recovery_schema
    # possibly CREATE .. IF NOT EXISTS would be more idiomatic sql...
    with user_db.cursor() as c:
        query = "SELECT name FROM sqlite_master WHERE type='table' or type='index';"
        tables = [i[0] for i in c.execute(query)]

    if "recovery_tokens" not in tables:
        logging.warning("User database missing table 'recovery_tokens'; adding now")
        # very old databases may be missing the password recovery table.
        # XX: do any of these still exist in practice?
        with user_db:
            user_db.execute(recovery_schema)
    if "index_username" not in tables:
        logging.warning(
            "User database missing index 'index_username'; adding now")
        try:
            with user_db:
                user_db.execute(user_index_schema)
        except sqlite3.DatabaseError:
            # this may fail if the database fails to satisfy the uniqueness
            # constraint (e.g. due to very old bugs)
            logging.warning(
                "Creation of index 'index_username' failed, manual intervention recommended",
                exc_info=True)


def init_db_connections(quiet=False):
    """Ensure databases exist, and init persistent db connections"""
    global settings_db, user_db
    if config.get('dgl_mode'):
        user_db = ensure_user_db_exists(quiet=quiet)
        upgrade_user_db()
    settings_db = ensure_settings_db_exists(quiet=quiet)



def get_mutelist(username):  # type: (str) -> Optional[str]
    with settings_db.execute(
            "SELECT mutelist FROM mutesettings WHERE username=? COLLATE NOCASE",
            (username,)) as c:
        result = c.fetchone()
    return result[0] if result is not None else None


def set_mutelist(username, mutelist):  # type: (str, Optional[str]) -> None
    if mutelist is None:
        mutelist = ""

    # n.b. the following will wipe out any columns not mentioned, if there
    # ever are any...
    query = """
        INSERT OR REPLACE INTO mutesettings
            (username, mutelist)
        VALUES
            (?,?);
    """
    with settings_db as db:
        db.execute(query, (username, mutelist))


# from dgamelaunch.h
DGLACCT_ADMIN = 1
DGLACCT_LOGIN_LOCK = 2
DGLACCT_PASSWD_LOCK = 4 # used by webtiles only for account holds
DGLACCT_EMAIL_LOCK = 8  # used by webtiles only for account holds

# not used by dgamelaunch, used by webtiles
DGLACCT_ACCOUNT_HOLD = 16
DGLACCT_WIZARD = 32 # may not be in use
DGLACCT_BOT = 64 # may not be in use


def flag_description(flags):
    s = []
    if (flags & DGLACCT_ADMIN):
        s.append("admin")
    if (flags & DGLACCT_LOGIN_LOCK):
        s.append("ban")
    if (flags & DGLACCT_ACCOUNT_HOLD):
        s.append("hold")
    else:
        # account is in a wierd state if it has these set...
        if (flags & DGLACCT_PASSWD_LOCK):
            s.append("passwd lock")
        if (flags & DGLACCT_EMAIL_LOCK):
            s.append("email lock")
    if (flags & DGLACCT_WIZARD):
        s.append("wizard")
    if (flags & DGLACCT_BOT):
        s.append("bot")
    return "|".join(s)


def dgl_is_admin(flags):  # type: (int) -> bool
    return bool(flags & DGLACCT_ADMIN)


def dgl_is_banned(flags):  # type: (int) -> bool
    return bool(flags & DGLACCT_LOGIN_LOCK) and not dgl_account_hold(flags)


def dgl_account_hold(flags):  # type: (int) -> bool
    return bool(flags & DGLACCT_ACCOUNT_HOLD)


UserInfo = collections.namedtuple("UserInfo", ["id", "username", "email", "flags"])


def get_user_info(username):
    """Returns user data in a named tuple of type UserInfo."""
    query = """
        SELECT id, username, email, flags
        FROM dglusers
        WHERE username=?
        COLLATE NOCASE
    """
    with user_db.execute(query, (username,)) as c:
        result = c.fetchone()

    if result:
        return UserInfo(*result)
    else:
        return None


def get_user_by_id(id):  # type: (str) -> Optional[Tuple[int, str, int]]
    """Returns user data in a tuple (userid, email)."""
    query = """
        SELECT id, username, email, flags
        FROM dglusers
        WHERE id=?
        COLLATE NOCASE
    """
    with user_db.execute(query, (id,)) as c:
        result = c.fetchone()

    if result:
        return UserInfo(*result)
    else:
        return None



def user_passwd_match(username, passwd):  # type: (str, str) -> Optional[Tuple[str, str]]
    """Returns whether the check succeeded, the correctly cased username and a
    reason for failure."""
    passwd = passwd[0:config.get('max_passwd_length')]

    # XX code semi-duplication
    query = """
        SELECT username, password, flags
        FROM dglusers
        WHERE username=?
        COLLATE NOCASE
    """
    with user_db.execute(query, (username,)) as c:
        result = c.fetchone()  # type: Optional[Tuple[str, str, str]]

    # XX should a successful login clear any password reset tokens?

    # a banned player should never return true for the password check
    if result and dgl_is_banned(result[2]):
        return False, result[0], 'Account is disabled.'
    elif result and crypt.crypt(passwd, result[1]) == result[1]:
        return True, result[0], None
    else:
        return False, None, None


_SALTCHARS = "./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"


def make_salt(saltlen):  # type: (int) -> str
    return ''.join(random.choice(_SALTCHARS) for x in range(0, saltlen))


def encrypt_pw(passwd):  # type: (str) -> str
    passwd = passwd[0:config.get('max_passwd_length')]
    crypt_algorithm = config.get('crypt_algorithm')
    if crypt_algorithm == "broken":
        salt = passwd
    elif crypt_algorithm:
        salt = "${}${}$".format(crypt_algorithm, make_salt(config.get('crypt_salt_length')))
    else:
        salt = make_salt(2)
    return crypt.crypt(passwd, salt)


def register_user(username, passwd, email):  # type: (str, str, str) -> Optional[str]
    """Returns an error message or None on success."""
    if config.get('new_accounts_disabled'):
        # XX show a message before they enter form data...
        return "New account creation is disabled."

    if passwd == "":
        return "The password can't be empty!"
    if email:  # validate the email only if it is provided
        result = validate_email_address(email)
        if result:
            return result
    username = username.strip()
    if not config.check_name(username):
        return "Account creation failed."

    crypted_pw = encrypt_pw(passwd)

    if get_user_info(username):
        return "User already exists!"

    flags = 0
    if config.get('new_accounts_hold'):
        flags = (flags | DGLACCT_LOGIN_LOCK | DGLACCT_EMAIL_LOCK
                       | DGLACCT_PASSWD_LOCK | DGLACCT_ACCOUNT_HOLD)

    query = """
        INSERT INTO dglusers
            (username, email, password, flags, env)
        VALUES
            (?, ?, ?, ?, '')
    """
    with user_db as db:
        db.execute(query, (username, email, crypted_pw, flags))

    return None

def change_password(user_id, passwd):
    if passwd == "":
        return "The password can't be empty."

    crypted_pw = encrypt_pw(passwd)

    user_info = get_user_by_id(user_id)

    if not user_info:
        return "Invalid account!"
    if (user_info.flags & DGLACCT_PASSWD_LOCK):
        return "Account has a password lock!"

    with user_db as db:
        db.execute("UPDATE dglusers SET password=? WHERE id=?",
                     (crypted_pw, user_info.id))
        # invalidate any recovery tokens that might exist, even for normal
        # password changes:
        db.execute("DELETE FROM recovery_tokens WHERE user_id=?", (user_info.id,))

    return None

def change_email(user_id, email):  # type: (str, str) -> Optional[str]
    """Returns an error message or None on success."""
    result = validate_email_address(email)
    if result:
        return result

    user_info = get_user_by_id(user_id)
    if not user_info:
        return "Invalid account!"

    if user_info.flags & DGLACCT_EMAIL_LOCK:
        return "Account has an email lock!"

    with user_db as db:
        db.execute("UPDATE dglusers SET email=? WHERE id=?", (email, user_info.id))

    return None

def set_ban(username, banned=True):
    user_info = get_user_info(username)

    if not user_info:
        return "Invalid username!"

    if banned and dgl_is_banned(user_info.flags):
        return "User '%s' is already banned." % username
    elif not banned and not dgl_is_banned(user_info.flags):
        return "User '%s' isn't currently banned." % username
    if banned and dgl_is_admin(user_info.flags):
        return "Remove admin flag before banning."

    # this is more of a sanity check than anything, the cli clears both
    if not banned and (user_info.flags & DGLACCT_ACCOUNT_HOLD):
        return "Clear account hold to clear ban flag"

    if banned:
        # escalate from an account hold to a ban if necessary
        new_flags = user_info.flags & ~DGLACCT_ACCOUNT_HOLD
        new_flags = new_flags | DGLACCT_LOGIN_LOCK
    else:
        # clean up any lingering lock flags
        new_flags = user_info.flags & ~(DGLACCT_LOGIN_LOCK
                                        | DGLACCT_EMAIL_LOCK
                                        | DGLACCT_PASSWD_LOCK)

    with user_db as db:
        db.execute("UPDATE dglusers SET flags=? WHERE id=?",
                    (new_flags, user_info.id))
        db.execute("DELETE FROM recovery_tokens WHERE user_id=?",
                     (user_info.id,))

    return None


def set_account_hold(username, hold=True):
    user_info = get_user_info(username)
    if not user_info:
        return "Invalid username!"
    # this will override bans
    if hold and dgl_account_hold(user_info.flags):
        return "User '%s' is already on hold." % username
    elif not hold and not dgl_account_hold(user_info.flags):
        return "User '%s' isn't currently on hold." % username
    if hold and dgl_is_admin(user_info.flags):
        return "Remove admin flag before setting an account hold."

    # account holds set the ban flag so that dgamelaunch can't be used to
    # get around the restrictions. XX dgamelaunch messaging wording?
    hold_flags = (DGLACCT_LOGIN_LOCK | DGLACCT_EMAIL_LOCK
                    | DGLACCT_PASSWD_LOCK | DGLACCT_ACCOUNT_HOLD)
    if hold:
        new_flags = user_info.flags | hold_flags
    else:
        new_flags = user_info.flags & ~hold_flags

    with user_db as db:
        db.execute("UPDATE dglusers SET flags=? WHERE id=?",
                    (new_flags, user_info.id))
        db.execute("DELETE FROM recovery_tokens WHERE user_id=?",
                     (user_info.id,))

    return None


# lower level interface for setting arbitrary flag values
def set_flags(username, flags, mask=~0):
    user_info = get_user_info(username)
    if not user_info:
        return "Invalid username!"

    new_flags = (user_info.flags & ~mask) | (flags & mask)

    with user_db as db:
        db.execute("UPDATE dglusers SET flags=? WHERE id=?",
                    (new_flags, user_info.id))

    return None


# try to avoid using this -- db on long-running servers can be extremely
# large and it uses fetchall...
def get_all_users():
    query = """
        SELECT id, username, email, flags
        FROM dglusers
        NOCASE
        ORDER BY username ASC
    """
    # XX refactor as generator? afaict the resulting generator might need
    # an explicit close() call in order to finalize the cursor here (otherwise
    # it will rely on gc...)
    with user_db.execute(query) as c:
        return [UserInfo(*t) for t in c.fetchall()]


def get_users_by_flag(flag):
    query = """
        SELECT id, username, email, flags
        FROM dglusers
        WHERE (flags & ?) <> 0
        COLLATE NOCASE
    """
    # XX refactor as generator?
    with user_db.execute(query, (flag,)) as c:
        return [UserInfo(*t) for t in c.fetchall()]


def get_bans():
    result = get_users_by_flag(DGLACCT_LOGIN_LOCK)
    bans = [x.username for x in result if dgl_is_banned(x.flags)]
    holds = [x.username for x in result if dgl_account_hold(x.flags)]
    return (bans, holds)


def find_recovery_token(token):
    # type: (str) -> Union[Tuple[None, None, str], Tuple[int, str, Optional[str]]]
    """Returns tuple (userid, username, error)"""

    lifetime = int(config.get('recovery_token_lifetime'))
    if lifetime < 1:
        return None, None, "Recovery tokens disabled"
    token_hash_obj = hashlib.sha256(utf8(token))
    token_hash = token_hash_obj.hexdigest()

    query = """
        SELECT
            u.id,
            u.username,
            CASE
                WHEN t.token_time > datetime('now','-%d hours') THEN 'N'
                ELSE 'Y'
            END AS Expired
        FROM recovery_tokens t
        JOIN dglusers u ON u.id = t.user_id
        WHERE t.token = ?
        COLLATE RTRIM
    """ % lifetime
    with user_db.execute(query, (token_hash, )) as c:
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


def update_user_password_from_token(token, passwd):
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

    userid, username, token_error = find_recovery_token(token)

    if userid and not token_error:
        with user_db:
            user_db.execute("UPDATE dglusers SET password=? WHERE id=?",
                            (crypted_pw, userid))
            user_db.execute("DELETE FROM recovery_tokens WHERE user_id=?",
                            (userid,))

    return username, token_error


def clear_password_token(username):
    if not username:
        return False, "Invalid username"

    user_info = get_user_info(username)
    if not user_info:
        return False, "Invalid username"

    with user_db:
        user_db.execute("DELETE FROM recovery_tokens WHERE user_id=?",
                        (user_info.id,))

    return True, ""

def create_password_token(user_id):
    # userid and lifetime are not checked here, use `generate_forgot_password`
    token_bytes = os.urandom(32)
    token = urlsafe_b64encode(token_bytes)
    # hash token
    token_hash_obj = hashlib.sha256(token)
    token_hash = token_hash_obj.hexdigest()
    # store hash in db
    with user_db:
        # clear out any existing tokens
        user_db.execute("DELETE FROM recovery_tokens WHERE user_id=?",
                (user_id,))
        user_db.execute("""
            INSERT INTO recovery_tokens(token, token_time, user_id)
            VALUES (?,datetime('now'),?)
            """, (token_hash, user_id))
    return token

def generate_token_email(token):
    # send email
    lobby_url = config.get('lobby_url', '')
    url_text = lobby_url + "?ResetToken=" + to_unicode(token)

    msg_body_plaintext = """Someone (hopefully you) has requested to reset the password for your account at %s.

If you initiated this request, please use this link to reset your password:

    %s

This link will expire in %d hour(s). If you did not ask to reset your password,
feel free to ignore this email.
""" % (lobby_url, url_text, config.get('recovery_token_lifetime'))

    msg_body_html = """<html>
  <head></head>
  <body>
    <p>Someone (hopefully you) has requested to reset the password for your
       account at %s.<br /><br />
       If you initiated this request, please use this link to reset your
       password:<br /><br />
       &emsp;<a href='%s'>%s</a><br /><br />
       This link will expire in %d hour(s). If you did not ask to reset your
       password, feel free to ignore this email.
    </p>
  </body>
</html>""" % (lobby_url, url_text, url_text, config.get('recovery_token_lifetime'))

    return msg_body_plaintext, msg_body_html

def generate_forgot_password(username):
    if not username:
        return False, "Empty username"

    user_info = get_user_info(username)
    if not user_info:
        return False, "Invalid username"

    lifetime = int(config.get('recovery_token_lifetime'))
    if lifetime < 1:
        return False, "Recovery tokens disabled"

    token = create_password_token(user_info.id)
    msg_body_plaintext, msg_body_html = generate_token_email(token)
    return True, msg_body_plaintext

def send_forgot_password(email):  # type: (str) -> Tuple[bool, Optional[str]]
    """
    Returns:
        (email_sent: bool, error: string)
    """
    if not email:
        return False, "Email address can't be empty"
    email_error = validate_email_address(email)
    if email_error:
        return False, email_error

    with user_db.cursor() as c:
        c.execute("SELECT id FROM dglusers WHERE email=? COLLATE NOCASE", (email,))
        result = c.fetchone()
    if not result:
        return False, None

    userid = result[0]
    token = create_password_token(userid)
    msg_body_plaintext, msg_body_html = generate_token_email(token)

    send_email(email, 'Request to reset your password',
               msg_body_plaintext, msg_body_html)

    return True, None


import unittest

class UserDBTest(unittest.TestCase):
    logging_init = False
    def setUp(self):
        if not self.logging_init:
            import webtiles.server as server
            server.init_logging(config.get('logging_config'))
            self.logging_init = True

        # XX it would be nice if webtiles.config were a bit more flexible
        # about this
        self.config_shim = dict(
            settings_db="./unittest_settings.db3",
            password_db="./unittest_passwd.db3",
            dgl_mode=True)
        config.server_config = self.config_shim
        # set quiet to prevent a lot of annoying logging
        try:
            init_db_connections(quiet=True)
        except (sqlite3.Warning, sqlite3.Error):
            # the testbed won't call tearDown if setUp doesn't finish, and any
            # db files will linger
            self.tearDown()
            raise

    def test_00init_state(self):
        # exercise basic db creation
        self.assertFalse(get_all_users())
        self.assertFalse(get_user_info("test"))
        self.assertFalse(get_user_by_id(1))

    def test_create_user(self):
        # test basic user creation, as well as user info and some misc user
        # data code
        register_user("test", "hunter2", "test@example.com")
        u = get_user_info("TEST")
        self.assertEqual(u.username, "test")
        self.assertEqual(u.email, "test@example.com")
        self.assertEqual(get_user_by_id(u.id), u)
        self.assertEqual(u.flags, 0)
        self.assertEqual(get_all_users()[0], u)

        change_email(u.id, "test2@example.com")
        u2 = get_user_info("test")
        self.assertEqual(u2.email, "test2@example.com")
        self.assertEqual(u.id, u2.id)
        self.assertEqual(u.username, u2.username)
        self.assertEqual(u.flags, u2.flags)

    def test_passwords(self):
        register_user("test", "hunter2", "test@example.com")
        u = get_user_info("test")

        # basic setup
        self.assertTrue(user_passwd_match("test", "hunter2")[0])
        self.assertFalse(user_passwd_match("test", "hunter3")[0])
        self.assertEqual(user_passwd_match("test", "hunter2")[1], "test")
        self.assertFalse(user_passwd_match("test", "")[0])

        # direct pw changes
        change_password(u.id, "hunter3")
        self.assertTrue(user_passwd_match("test", "hunter3")[0])
        self.assertFalse(user_passwd_match("test", "hunter2")[0])


    def test_tokens(self):
        register_user("test", "hunter2", "test@example.com")
        u = get_user_info("test")

        # basic token functions
        token = create_password_token(u.id)
        r = find_recovery_token(token)
        self.assertFalse(r[2]) # no error message set
        self.assertEqual(r[0], u.id)
        update_user_password_from_token(token, "hunter2")
        self.assertTrue(user_passwd_match("test", "hunter2")[0])
        self.assertFalse(user_passwd_match("test", "hunter3")[0])
        # token should now be disabled
        r = find_recovery_token(token)
        self.assertTrue(r[2]) # error message set
        r = find_recovery_token("hacking")
        self.assertTrue(r[2]) # error message set
        # hard to test `generate_forgot_password` directly. But, at least
        # exercise it.
        self.assertTrue(generate_forgot_password(u.username)[0])
        # new tokens should be possible, and replace unused tokens
        self.assertTrue(generate_forgot_password(u.username)[0])
        token = create_password_token(u.id)
        t2 = create_password_token(u.id)
        self.assertTrue(find_recovery_token(token)[2]) # error message set
        self.assertFalse(find_recovery_token(t2)[2]) # should be ok

        # token expiry
        def_life = config.get('recovery_token_lifetime')
        token = create_password_token(u.id)
        config.set('recovery_token_lifetime', 1)
        # force adjust the set time to before the lifetime
        with user_db:
            user_db.execute("""
                UPDATE recovery_tokens
                    SET token_time=datetime('now', '-2 hours')
                    WHERE user_id=?
                """, (u.id,))
        self.assertTrue(find_recovery_token(token)[2]) # error message set
        config.set('recovery_token_lifetime', 0) # disabled
        self.assertTrue(find_recovery_token(token)[2]) # error message set
        self.assertFalse(generate_forgot_password(u.username)[0]) # can't set
        # restore default config, just in case:
        config.set('recovery_token_lifetime', def_life)


    def test_restrictions(self):
        register_user("test", "hunter2", "test@example.com")
        self.assertTrue(user_passwd_match("test", "hunter2")[0])

        # banning
        set_ban("test")
        u = get_user_info("test")
        self.assertTrue(dgl_is_banned(u.flags))
        self.assertFalse(user_passwd_match("test", "hunter2")[0])
        self.assertEqual(get_bans()[0][0], u.username)

        # unbanning
        set_ban("test", False)
        u = get_user_info("test")
        self.assertFalse(dgl_is_banned(u.flags))
        self.assertEqual(u.flags, 0)
        self.assertTrue(user_passwd_match("test", "hunter2")[0])
        self.assertFalse(get_bans()[0])

        # holds
        set_account_hold("test")
        u = get_user_info("test")
        self.assertFalse(dgl_is_banned(u.flags))
        self.assertTrue(dgl_account_hold(u.flags))
        # login is still possible
        self.assertTrue(user_passwd_match("test", "hunter2")[0])
        self.assertEqual(get_bans()[1][0], u.username)

        # clearing holds
        set_account_hold("test", False)
        u = get_user_info("test")
        self.assertEqual(u.flags, 0)
        self.assertFalse(dgl_is_banned(u.flags))
        self.assertFalse(dgl_account_hold(u.flags))
        self.assertTrue(user_passwd_match("test", "hunter2")[0])
        self.assertFalse(get_bans()[0])

        # hold -> ban
        set_account_hold("test")
        set_ban("test")
        u = get_user_info("test")
        self.assertTrue(dgl_is_banned(u.flags))
        self.assertFalse(dgl_account_hold(u.flags))
        self.assertFalse(user_passwd_match("test", "hunter2")[0])
        self.assertEqual(get_bans()[0][0], u.username)
        # finally, try clearing
        set_ban(u.username, False)
        u = get_user_info("test")
        self.assertEqual(u.flags, 0)
        self.assertFalse(dgl_is_banned(u.flags))
        self.assertFalse(dgl_account_hold(u.flags))

        # XX not tested: admin/wizard/bot flags

    def tearDown(self):
        if os.path.exists(self.config_shim['settings_db']):
            os.unlink(self.config_shim['settings_db'])
        if os.path.exists(self.config_shim['password_db']):
            os.unlink(self.config_shim['password_db'])
