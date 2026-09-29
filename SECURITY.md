# Security policy

## Supported versions

Only the latest release gets security fixes.

## Reporting a vulnerability

Please report security problems privately through
[GitHub's private vulnerability reporting](https://github.com/crevasse3523/plasma-taildrop/security/advisories/new),
not in a public issue. Describe the problem, how to reproduce it, and what an attacker could gain. You should get an
answer within a week.

## Scope

plasma-taildrop runs as your user and talks only to the local tailscaled through its LocalAPI socket. It never
needs root and does not listen on the network. Tailscale decides who may send files (the operator setting) and to
which devices; problems in Tailscale itself belong to [Tailscale's security team](https://tailscale.com/security).

Relevant here are, for example, files being sent that the user did not choose, archives of shared folders that
include files from outside the folder, or temporary archives that other users can read.
