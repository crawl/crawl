import datetime
import random
import time

import tornado.ioloop
import tornado.websocket
from tornado.ioloop import IOLoop

import config

try:
    from typing import Dict, Tuple
except ImportError:
    pass

login_tokens = {}  # type: Dict[Tuple[str,str],datetime.datetime]
rand = random.SystemRandom()


def purge_login_tokens():
    for token in list(login_tokens):
        if datetime.datetime.now() > login_tokens[token]:
            del login_tokens[token]


def purge_login_tokens_timeout():
    purge_login_tokens()
    IOLoop.current().add_timeout(time.time() + 60 * 60 * 1000,
                                 purge_login_tokens_timeout)


def log_in_as_user(request, username):
    token = rand.getrandbits(128)
    expires = datetime.datetime.now() + datetime.timedelta(config.login_token_lifetime)
    login_tokens[(token, username)] = expires
    cookie = username + "%20" + str(token)
    if not isinstance(request, tornado.websocket.WebSocketHandler):
        request.set_cookie("login", cookie)
    return cookie


def _parse_login_cookie(cookie):
    username, _, token = cookie.partition('%20')
    try:
        token = int(token)
    except ValueError:
        token = None
    return username, token


def check_login_cookie(cookie):
    username, token = _parse_login_cookie(cookie)
    ok = (token, username) in login_tokens
    return username, ok


def forget_login_cookie(cookie):
    username, token = _parse_login_cookie(cookie)
    if (token, username) in login_tokens:
        del login_tokens[(token, username)]
