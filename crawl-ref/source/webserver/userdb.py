import contextlib
import crypt
import logging
import os.path
import random
import re
import sqlite3
import time

from conf import config


@contextlib.contextmanager
def cursor():
    """Simple contextmanager for mediated sqlite3 db access.

    Commits at the end and closes the cursor/connection immediately.
    """
    dbpath = config["password_db"]
    try:
        conn = sqlite3.connect(dbpath)
        try:
            c = conn.cursor()
            yield c
            conn.commit()
        finally:
            c.close()
    finally:
        conn.close()


def user_passwd_match(username, passwd):
    """Return the correctly cased username."""
    passwd = passwd[:config["max_passwd_length"]]

    with cursor() as c:
        c.execute("select username,password from dglusers where username=? "
                  "collate nocase", (username,))
        result = c.fetchone()

        if result is None:
            return None
        elif crypt.crypt(passwd, result[1]) == result[1]:
            return result[0]


def ensure_user_db_exists():
    """Ensure sqlite3 DB exists, and create if it does not.

    Should be run once on startup.
    """
    if not os.path.exists(config["password_db"]):
        logging.warn("User database didn't exist; creating it now.")
        with cursor() as c:
            c.execute("CREATE TABLE dglusers (id integer primary key, username "
                      "text, email text, env text, password text, flags "
                      "integer);")

    with cursor() as c:
        c.execute("CREATE TABLE IF NOT EXISTS login_tokens (username text, "
                  "seqid text, token text, expires integer, PRIMARY KEY "
                  "(username, seqid));")


saltchars = "./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"


def make_salt(saltlen):
    """Generate a random human-readable salt."""
    return "".join(random.choice(saltchars) for x in xrange(0, saltlen))


def get_salt(passwd):
    """Return the salt component of a hashed password.

    Generates a new salt.
    Final format is '$algo$salt$password'.
    """
    algo = config.get("crypt_algorithm", "6")
    if algo:
        salt = "${0}${1}$".format(algo, make_salt(config["crypt_salt_length"]))
    else:
        salt = make_salt(2)
    return salt


def username_exists(username, c):
    """Return True if given username exists or is illegal."""
    if not username:
        return True
    if len(username) < config["min_username_length"]:
        return True
    c.execute("select username from dglusers where username=? collate "
              "nocase", (username,))
    result = c.fetchone()
    if result:
        return True


def find_unused_guest_name():
    """Return an unused guest name or None if we can't."""
    with cursor() as c:
        name = None
        loops = 0
        while username_exists(name, c):
            loops += 1
            if loops == 10:
                logging.warning("Couldn't find an unused guest username. "
                                "(hint: try increasing guest_name_suffix_len)")
                name = None
                break
            name = config["guest_name_prefix"]
            for _ in range(config["guest_name_suffix_len"]):
                name += random.choice('abcdefghijklmnopqrstuvwxyz0123456789')
    return name


def register_user(username, passwd, email):
    """Register a new user account.

    Returns None on success or error string.
    Set passwd to None to disable password authentication (ie, the user cannot
    log in).
    Note: email is not checked for validity."""
    # passwd can be '' or None (which mean different things). Check for ''
    # here, and None further down. Bit messy.
    if passwd == '':
        return "The password can't be None!"
    if email == '':
        return "Email cannot be blank!"
    if email is not None and not re.match(config["email_regex"], email):
        return "Invalid email address!"
    username = username.strip()
    if len(username) < config["min_username_length"]:
        return "Username too short."
    if not re.match(config["nick_regex"], username):
        return "Invalid username!"

    if not config.get('dev_nicks_can_be_registered') \
       and config.get_devname(username):
        return "Reserved username!"

    if passwd:
        passwd = passwd[0:config["max_passwd_length"]]
        salt = get_salt(passwd)
        crypted_pw = crypt.crypt(passwd, salt)
        # crypt.crypt can fail if you pass in a salt it doesn't like
        if not crypted_pw:
            logging.warning("User registration failed; "
                            "crypt_algorithm setting?"
                            "Tested salt was '%s'", salt)
            return "Internal registration error."  # deliberately generic
    else:
        crypted_pw = 'No Password'

    with cursor() as c:
        if username_exists(username, c):
            return "User already exists!"
        c.execute("insert into dglusers(username, email, password, flags, env)"
                  " values (?,?,?,0,'')", (username, email, crypted_pw))


def set_password(username, passwd):
    """Set password for an account. Returns True or False."""
    if passwd == "":
        return False
    passwd = passwd[0:config["max_passwd_length"]]

    salt = get_salt(passwd)
    crypted_pw = crypt.crypt(passwd, salt)

    with cursor() as c:
        c.execute("update dglusers set password=? where username=?",
                  (crypted_pw, username))
        return True

sessions = {}
rand = random.SystemRandom()


def pack_sid(sid):
    """Convert int -> hex -> string."""
    return "{0:x}".format(sid)


def session_info(sid):
    """Return session info matching sid or None."""
    s = sessions.get(sid)
    if (s is None or
            s["expires"] < time.time() or
            s["forceexpires"] < time.time()):
        return None
    return s


def renew_session(sid):
    """Extend sid's session expiry or return False."""
    if sid not in sessions:
        return False
    sessions[sid]["expires"] = (time.time() + 5 * 60)
    return True


def new_session():
    """Generate a new session and return (sid, session)."""
    sid = pack_sid(rand.getrandbits(128))
    sessions[sid] = {"forceexpires": time.time()
                     + config.get("session_lifetime", 24 * 60 * 60)}
    renew_session(sid)
    return sid, sessions[sid]


def delete_session(sid):
    """Delete a session if it exists."""
    if sid in sessions:
        del sessions[sid]


def purge_login_tokens():
    """Delete all login tokens & expired sessions."""s
    with cursor() as c:
        c.execute("DELETE FROM login_tokens WHERE expires<?",
                  (int(time.time()),))

    for sid in sessions:
        session = sessions[sid]
        if (session["expires"] < time.time() or
                session["forceexpires"] < time.time()):
            del sessions[sid]


def delete_logins(username):
    """Delete all login tokens for a given username."""
    with cursor() as c:
        c.execute("DELETE FROM login_tokens WHERE username=?", (username,))

    # This is a python3-compatible way to list copy the items so we can delete
    # keys.
    for sid, session in list(sessions.items()):
        if session.get("username") == username:
            del sessions[sid]


def token_login(cookie, logger=logging):
    """Login based on client-cookie-supplied SID."""
    try:
        username, token, seqid = cookie.split(":")
    except ValueError:
        return None, None
    with cursor() as c:
        c.execute("SELECT token, expires FROM login_tokens WHERE username=? "
                  "AND seqid=?", (username, seqid))
        result = c.fetchone()
        # XXX is this a bug? should we *always* delete here even if login fails?
        c.execute("DELETE FROM login_tokens WHERE username=? AND seqid=?",
                  (username, seqid))

    if result:
        true_token, expires = result
        if expires < time.time():
            return None, None

        if true_token == token:
            logger.info("User {0} logged in (via token).".format(username))
            return username, get_login_cookie(username, seqid=seqid)
        else:
            logger.warning("Bad login token for user {0}!".format(username))
            delete_logins(username)
            return None, None
    else:
        return None, None


def get_login_cookie(username, seqid=None):
    token = pack_sid(rand.getrandbits(128))
    if seqid is None:
        seqid = pack_sid(rand.getrandbits(128))
    expires = int(time.time()) + config["login_token_lifetime"] * 24 * 60 * 60
    with cursor() as c:
        c.execute("INSERT INTO login_tokens(username, seqid, token, expires) "
                  "VALUES (?,?,?,?)", (username, seqid, token, expires))
    cookie = "{0}:{1}:{2}".format(username, token, seqid)
    return cookie, expires


def forget_login_cookie(cookie):
    username, token, seqid = cookie.split(":")
    with cursor() as c:
        c.execute("DELETE FROM login_tokens WHERE username=? AND seqid=?",
                  (username, seqid))
