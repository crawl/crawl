# Security Policy for DCSS

Last updated Apr 2020.

## Supported Versions

Generally, we fully support the current unstable version, and the most recent
stable version. At time of writing, this is:

| Version | Supported          |
| ------- | ------------------ |
| 0.25-a  | :white_check_mark: |
| 0.24.1  | :white_check_mark: |
| <0.24.1 | :x:                |

We may, if the vulnerability is severe and affects online play, attempt to
patch earlier versions. Versions before around 0.14 do not reliably build any
more and may be impossible to patch regardless of severity.

Online servers generally run a webtiles server version drawn from trunk, even
if they allow play on older versions of dcss, so any vulnerability in an
up-to-date webtiles server is covered (e.g. in the python code).

## Reporting a Vulnerability

Open an issue on github in this repository, or contact the devteam in
`##crawl-dev` on freenode. If you would prefer to report the issue in private,
we recommend either contacting one of the currently active devs directly (e.g.
via an IRC private message), or sending an email to [security@dcss.io] with a
subject line including the phrase `dcss security report`. Currently this email
forwards to @rawlins / advil, who will send an acknowledgement and report the
issue to the devteam more generally.

If you have access (devteam and server owners) you can directly report a
security issue in private by opening an issue in the https://github.com/crawl/dcss-security
repository.
