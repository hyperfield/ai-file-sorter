# Security Policy

## Reporting a vulnerability

Please report suspected security vulnerabilities through
[GitHub's private vulnerability reporting form](https://github.com/hyperfield/ai-file-sorter/security/advisories/new).
You can also find this under **Security and quality → Advisories → Report a vulnerability**.

This provides a private channel for discussing the report with maintainers.
Please keep vulnerability details, exploit instructions, and sensitive attachments
out of public issues and pull requests while we investigate and coordinate a fix.
If you are unsure whether a problem has security implications, report it privately.

For ordinary bugs, feature requests, and security improvements that do not expose
a vulnerability, use the [public issue tracker](https://github.com/hyperfield/ai-file-sorter/issues).

## What to include

To help us reproduce and assess the issue, include:

- The affected AI File Sorter version or commit, operating system, and architecture.
- The affected feature or component and any relevant configuration.
- Reproduction steps and a minimal proof of concept or sample file, where possible.
- The potential security impact and any conditions needed to trigger the issue.
- Relevant logs, screenshots, or a suggested fix, if available.

Redact API keys, passwords, personal information, and confidential file contents.
Use synthetic test data where possible, and share only what is necessary to
demonstrate the problem.

## Supported versions

Security maintenance focuses on the
[latest stable release](https://github.com/hyperfield/ai-file-sorter/releases/latest)
and the current development branch, `dev`.

Reports affecting older versions are welcome, but we may ask whether the issue
also affects the latest release. Security fixes may be delivered in a newer
release rather than backported to every historical version.

## Review and coordinated disclosure

Maintainers will review the report and follow up through the private reporting
channel. Response and fix timelines depend on severity, reproducibility, and
maintainer availability.

We will coordinate disclosure timing with the reporter as we investigate and
prepare a fix. Please allow time for that process before publishing technical
details. Where appropriate, we will publish a security advisory with affected
versions, available fixes or workarounds, and reporter credit with their consent.
