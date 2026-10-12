.syntax unified
.thumb

.section .isr_vector, "a", %progbits
.align 2
.global __isr_vector
.type __isr_vector, %object
__isr_vector:
    .word _estack
    .word Reset_Handler
    .word Default_Handler
    .word HardFault_Handler
    .word Default_Handler
    .word Default_Handler
    .word Default_Handler
    .word Default_Handler
    .word Default_Handler
    .word Default_Handler
    .word Default_Handler
    .word Default_Handler
    .word Default_Handler
    .word Default_Handler
    .word Default_Handler
    .word SysTick_Handler
.size __isr_vector, . - __isr_vector

.section .text.Reset_Handler, "ax", %progbits
.align 2
.global Reset_Handler
.type Reset_Handler, %function
.thumb_func
Reset_Handler:
    /* Enable CP10/CP11 before any hard-float or MVE instruction executes. */
    ldr r0, =0xE000ED88
    ldr r1, [r0]
    ldr r2, =0x00F00000
    orrs r1, r2
    str r1, [r0]

    /* Preserve exception latency by enabling automatic lazy FP/MVE stacking. */
    ldr r0, =0xE000EF34
    ldr r1, [r0]
    ldr r2, =0xC0000000
    orrs r1, r2
    str r1, [r0]
    dsb
    isb

    ldr r0, =_sidata
    ldr r1, =_sdata
    ldr r2, =_edata
1:
    cmp r1, r2
    bcs 2f
    ldr r3, [r0], #4
    str r3, [r1], #4
    b 1b
2:
    ldr r0, =_sbss
    ldr r1, =_ebss
    movs r2, #0
3:
    cmp r0, r1
    bcs 4f
    str r2, [r0], #4
    b 3b
4:
    bl main
    bl platform_exit
5:
    b 5b
.size Reset_Handler, . - Reset_Handler

.section .text.Default_Handler, "ax", %progbits
.align 2
.global Default_Handler
.type Default_Handler, %function
.thumb_func
Default_Handler:
    b Default_Handler
.size Default_Handler, . - Default_Handler

.weak HardFault_Handler
.thumb_set HardFault_Handler, Default_Handler
