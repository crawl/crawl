import datetime

import pytest

import auth

try:
    import mock
except ImportError:
    from unittest import mock


class Test_purge_login_tokens:

    @mock.patch('auth.datetime')
    def test_not_yet_expired_tokens_are_kept(self, stub_datetime):
        auth.login_tokens = {
            ('token', 'username'): datetime.datetime.max,
        }
        stub_datetime.datetime.now.return_value = datetime.datetime.min

        auth.purge_login_tokens()

        assert auth.login_tokens

    @mock.patch('auth.datetime')
    def test_expired_tokens_are_discarded(self, stub_datetime):
        auth.login_tokens = {
            ('token', 'username'): datetime.datetime.min,
        }
        stub_datetime.datetime.now.return_value = datetime.datetime.max

        auth.purge_login_tokens()

        assert not auth.login_tokens


class Test_log_in_as_user:

    @mock.patch('auth.config')
    @mock.patch('auth.datetime')
    def test_cookies_have_correct_lifetime(self, stub_datetime, stub_config):
        stub_config.login_token_lifetime = 42
        stub_datetime.datetime.now.return_value = datetime.datetime.min
        stub_datetime.timedelta = datetime.timedelta
        stub_request = mock.Mock()

        auth.log_in_as_user(stub_request, 'user')

        expires = list(auth.login_tokens.values())[0]
        lifetime = expires - datetime.datetime.min

        assert lifetime == datetime.timedelta(42)


class Test__parse_login_cookie:

    @pytest.mark.parametrize("cookie", [
        "",
        " ",
        "    ",
        " asdf",
        "asdf",
        "asdf ",
        "asdf asdf",
        "asdf asdf asdf",
    ])
    def test_returns_none_for_wrong_format(self, cookie):
        username, token = auth._parse_login_cookie(cookie)
        assert token is None

    @pytest.mark.parametrize("cookie", [
        "abc 123",
    ])
    def test_returns_username_and_token(self, cookie):
        username, token = auth._parse_login_cookie(cookie)
        assert username and token
