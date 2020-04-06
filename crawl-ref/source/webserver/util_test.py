import pytest

import util


class Test_validate_email_address:
    @pytest.mark.parametrize("email, valid", [
        ("user@gmail.com", True),
        ("user.name@gmail.domain.com", True),
        ("", True),  # !!!
        ("user name@gmail.com", False),
        ("user", False),
        ("user@domain", False),
        ("u" * 80 + "@gmail.com", False),
    ])
    def test_validate_email_address(self, email, valid):
        result = util.validate_email_address(email)
        if valid:
            assert result is None
        else:
            assert result is not None


class Test_humanise_bytes:
    @pytest.mark.parametrize("num, expected", [
        (1, "1 bytes"),
        (1000, "1000 bytes"),
        (1000000, "1000.0 kilobytes"),
        (1000000000, "1000.0 megabytes"),
        (1000000000000, "1000.0 gigabytes"),
    ])
    def test_humanise_bytes(self, num, expected):
        assert util.humanise_bytes(num) == expected
