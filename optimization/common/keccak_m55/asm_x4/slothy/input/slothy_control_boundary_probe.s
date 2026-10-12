.syntax unified
.thumb
.arch armv8.1-m.main
.arch_extension mve

// These deliberately unsupported regions document why function ABI,
// subroutine calls, and multi-exit control flow remain outside Slothy blocks.
probe_abi_start:
  push {r4, lr}
  mov r4, r0
  pop {r4, pc}
probe_abi_end:

probe_call_start:
  mov r0, r1
  bl m55_keccakx4_permute
  mov r2, r0
probe_call_end:

probe_branch_start:
  cmp r0, #0
  beq .Lprobe_done
  add r1, r1, #1
.Lprobe_done:
  mov r2, r1
probe_branch_end:
