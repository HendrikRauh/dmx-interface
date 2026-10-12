FROM mcr.microsoft.com/devcontainers/base:ubuntu-24.04

# Pre-seed /etc/nix/nix.conf before the Nix feature installs: the feature's
# multi-user installer bootstraps a build sandbox (sethostname, needs
# CAP_SYS_ADMIN) and only writes `sandbox = false` *afterwards* — Podman build
# steps have no CAP_SYS_ADMIN, so the install fails with "cannot set host name:
# Operation not permitted". Pre-seeding yields the exact end state the feature
# writes anyway; Docker builds (full caps) are unaffected either way.
RUN mkdir -p /etc/nix && echo 'sandbox = false' > /etc/nix/nix.conf
