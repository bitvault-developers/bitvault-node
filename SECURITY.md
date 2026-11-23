# Security policy

## Supported versions
| Version | Supported |
|---|---|
| v8.26.14 (latest release) | Yes |
| v8.26.13 | Yes |
| v8.26.12 and older | No. They stop at block 43,076, so please upgrade. |

## Reporting a vulnerability
Please do **not** open a public issue for a security problem.

Email **support@bitvault.club** with a subject that starts with `SECURITY:` and include:
- what is affected (node version, service or web address),
- the steps to reproduce it,
- what an attacker could do with it.

If you want to send the details encrypted, say so in your first email and we will reply with a key.

## What happens next
- We confirm that we received your report within 3 days.
- We investigate, keep you informed, and fix confirmed problems in a new release.
- Please give us reasonable time to ship a fix before you publish anything. 90 days is our default.
- With your permission, we thank you by name (or handle) in the release notes.

## Scope
In scope: this node software, and the public BitVault services listed at https://bitvault.club/security/report/.

Out of scope: denial-of-service or load testing against our public servers, spam, social engineering, and physical attacks.

## Upstream fixes
BitVault is built on Bitcoin Core and DigiByte Core. We follow the Bitcoin Core security advisories
(https://bitcoincore.org/en/security-advisories/) and apply the fixes that affect BitVault in our releases.

Full policy: https://bitvault.club/security/report/
