# dk-crawl
A fork of [crawl](https://github.com/crawl/crawl.git) to explore the feasibility of adding Azure Active Directory B2C support for user authorization.

## Setup dev environment

* Set up a Python virtualenv. From ./crawl-ref/source/

    ```sh
    python3 -m virtualenv -p python3 webserver/venv
    . ./webserver/venv/bin/activate
    pip install -r webserver/requirements/dev.py3.txt
    ```