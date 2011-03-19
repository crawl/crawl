As of 14/2/2011, we no longer use SourceForge as our repository hosting
provider. Therefore, the following instructions are outdated. CIA notices and
crawl-ref-commit emails are sent from crawl.develz.org.

To update the user->username table, checkout:

    git@gitorious.org:+dcss-developers/gitorious-cia/dcss-gitorious.cia

and alter the "crawl.py" file. Commit and push. Then ask someone who has shell
access to crawl.develz.org -- or if you have shell access -- to execute:

    cd ~crawl/source/dcss-gitorious-cia
    git pull

###############################################################################
###############################################################################
###############################################################################

Old instructions:

This directory contains copies of the git hook scripts.  The actual hooks can
be edited by getting an SHH shell on the sourceforge machine:

    ssh -t USER,crawl-ref@shell.sourceforge.net create

See http://sourceforge.net/apps/trac/sourceforge/wiki/Shell%20service

Once there, cd to "/home/scm_git/c/cr/crawl-ref/crawl-ref/hooks"
