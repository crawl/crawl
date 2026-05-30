# Checklist for adding a new developer into the team.

* Commit rights: add their GitHub account to the DCSS group.
* Ask advil, gammafunk, or another Crawl Dev discord admin to give them the
  Crawl Dev role.
* Add them to .mailmap (in the repository root) and CREDITS.txt.
* Add them to the committer group on Libera IRC:
       /msg chanserv flags #crawl-dev NICK committer
  They should first register with NickServ (/msg nickserv help register) to
  prevent others from using that nick.  /msg nickserv info NICK  will show an
  "account" field if they are registered.
* Upgrade their Crawl server accounts (CAO, et al) to administrator. This
  lets them use wizmode, and on most servers gives them access to save backups
  and the rebuild trigger:
       dgl admin add <username>
* Create a WordPress account for them.
* Ask them to register on [Tavern](https://tavern.dcss.io) discussion forum
  if they haven't done so, then add them to the Dungeon Master group.
* Send an announcement/welcome message on the crawl.develz.org Dev Blog.
