# Vendored: xv6-riscv

Upstream: https://github.com/mit-pdos/xv6-riscv.git
Branch:   riscv
Commit:   1982fd12595f52a0e5ef8db466257a01fb1fbfef
Describe: xv6-riscv-rev5-36-g1982fd1   (2026-07-03)

This is a **pristine** copy of upstream xv6-riscv at the commit above, vendored
directly into this repository (the upstream `.git` history has been removed).
**Do not edit this tree.** The labs apply their overlays to a *copy* of it — see
`../lab0-toolchain/README.md` and `../README.md`.

## Refreshing to a newer upstream commit

    git clone https://github.com/mit-pdos/xv6-riscv.git /tmp/xv6-new
    cd /tmp/xv6-new && git checkout <new-commit>
    rm -rf .git
    # replace this directory's contents with /tmp/xv6-new, then update the
    # Commit / Describe / date fields above.
