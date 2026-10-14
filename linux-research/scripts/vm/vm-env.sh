# Sourced by the vm-*.sh scripts (host side, macOS). Override any of these in the environment.
# bash only: LR comes from BASH_SOURCE, and vm_pull deletes "$LR/<out>" before copying.
[ -n "${BASH_VERSION:-}" ] || { echo "vm-env.sh: source it from bash" >&2; return 1; }
VM_HOST="${VM_HOST:-gene@192.168.64.2}"
VM_KEY="${VM_KEY:-$HOME/.ssh/claude_linux_vm}"
VM_SSH_OPTS=(-i "$VM_KEY" -o BatchMode=yes -o ConnectTimeout=10)
# The harness is mirrored to the VM at ~/$VM_HARNESS/linux-research, so its relative paths
# (scripts/..., evidence/...) are the same on both sides.
VM_HARNESS="${VM_HARNESS:-iplug-harness}"
LR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"   # local linux-research/

vm() { ssh "${VM_SSH_OPTS[@]}" "$VM_HOST" "$@"; }
vm_rsync() { rsync -a -e "ssh ${VM_SSH_OPTS[*]}" "$@"; }

# Copy linux-research/scripts to the VM (cheap; run before every remote harness call).
vm_sync_harness() {
  vm "mkdir -p ~/$VM_HARNESS/linux-research"
  vm_rsync --delete "$LR/scripts/" "$VM_HOST:$VM_HARNESS/linux-research/scripts/"
}

# Pull <path relative to linux-research> back from the VM into the local tree, replacing what was there
# (macOS's openrsync does not honour --delete here).
vm_pull() {
  rm -rf "${LR:?}/$1"; mkdir -p "$LR/$1"
  vm_rsync "$VM_HOST:$VM_HARNESS/linux-research/$1/" "$LR/$1/"
}
