#!/usr/bin/env python3
import os
import subprocess
import sys

allowed_subject_len = 72
allowed_body_len = 72

# Handy function borrowed from coverage.py
def die(msg: str):
    print(msg, file=sys.stderr)
    sys.exit(1)

def contains_newline(s):
    return '\r' in s or '\n' in s
def print_err(msg: str):
    print(msg, file=sys.stderr)
def main():
    # https://git-scm.com/docs/git-log
    # HEAD ^master gives us all commits not on master
    # The pretty argument lets us make out own more parsing friendly output.
    # %xNN = literal byte %xNN.
    # %x1E = ASCII byte 30, aka "Record Separator". We're using it to separate commits
    # %x1F = ASCII byte 31, aka "Unit Separator". We're using it to separate hash/committer/title/body
    # We're using these because we can be pretty sure they won't show up in commit messages proper.
    # %h = commit hash
    # %cn = committer name
    # %s = subject
    # %b = body
    pretty = '--pretty=%h%x1F%cn%x1F%s%x1F%b%x1E'
    branch_to_check = os.environ.get("GITHUB_HEAD_REF","HEAD")
    base_branch = os.environ.get("GITHUB_BASE_REF", "origin/master")
    print(f"Checking commits being merged from {branch_to_check} to {base_branch}")

    git_args = [ "git", "log", "--no-merges", pretty, branch_to_check, f"^{base_branch}" ]
    command_to_print = " ".join(git_args)
    print(f"Running: {command_to_print}")
    ret = subprocess.run(git_args, capture_output = True, timeout=15)

    if ret.returncode != 0:
        print("Command failed, stdout:", file=sys.stderr)
        print(ret.stdout, file=sys.stderr)
        print("Command failed, stderr:", file=sys.stderr)
        print(ret.stderr, file=sys.stderr)
        die(f"Command failed, exiting {sys.argv[0]}")

    if ret.stdout is None:
        die(f"Command returned no output, exiting {sys.argv[0]}")

    output = ret.stdout.decode("utf-8")
    commits = output.split('\x1E')
    failure = False
    for commit in commits:
        pieces = commit.split('\x1F')
        if len(pieces) == 1 and (pieces[0] == '\n' or pieces[0] == '\r\n'):
            # Only should happen e.g. after the last commit
            # Probably is a better way...
            continue

        if len(pieces) != 4:
            print_err(f"Command output had unexpected number of results ({len(pieces)}), exiting")
            failure = True
            continue

        (commit_hash, committer_name, subject, body) = pieces
        commit_hash = commit_hash.replace('\r\n', '').replace('\n', '')
        print(f"Checking commit {commit_hash} written by {committer_name}")
        if len(subject) > allowed_subject_len:
            print_err(f"Commit {committer_name} from {committer_name} contained a subject "
                f"{len(subject)} characters long,\n"
                f"maximum allowed is {allowed_subject_len}.")
            failure = True
            continue

        if contains_newline(subject):
            print_err(f"Commit {committer_name} from {committer_name} contained a newline in the subject!")
            failure = True
            continue

        for line_number, line in enumerate(body.split('\n')):
            if len(line) > allowed_body_len:
                print_err(f"Commit {committer_name} from {committer_name} "
                    f"has an overly long line in its body,\n"
                    f"body line {line_number}, length {len(line)},\n"
                    f"maximum allowed is {allowed_body_len}\n"
                    f"'{line}'")
                failure = True
                continue

    if failure:
        die("Failed commit message validation!")
    else:
        print("Passed commit message validation!")

if __name__ == "__main__":
    main()
