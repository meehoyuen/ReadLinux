#ifndef _ASM_IRQ_H
#define _ASM_IRQ_H

/*
 *	linux/include/asm/irq.h
 *
 *	(C) 1992, 1993 Linus Torvalds
 */

#include <linux/segment.h>
#include <linux/linkage.h>

extern void disable_irq(unsigned int);
extern void enable_irq(unsigned int);

#define __STR(x) #x
#define STR(x) __STR(x)
 
#define SAVE_ALL \
	"cld\n\t" \
	"push %gs\n\t" \
	"push %fs\n\t" \
	"push %es\n\t" \
	"push %ds\n\t" \
	"pushl %eax\n\t" \
	"pushl %ebp\n\t" \
	"pushl %edi\n\t" \
	"pushl %esi\n\t" \
	"pushl %edx\n\t" \
	"pushl %ecx\n\t" \
	"pushl %ebx\n\t" \
	"movl $" STR(KERNEL_DS) ",%edx\n\t" \
	"mov %dx,%ds\n\t" \
	"mov %dx,%es\n\t" \
	"movl $" STR(USER_DS) ",%edx\n\t" \
	"mov %dx,%fs\n\t"   \
	"movl $0,%edx\n\t"  \
	"movl %edx,%db7\n\t"

/*
 * SAVE_MOST/RESTORE_MOST is used for the faster version of IRQ handlers,
 * installed by using the SA_INTERRUPT flag. These kinds of IRQ's don't
 * call the routines that do signal handling etc on return, and can have
 * more relaxed register-saving etc. They are also atomic, and are thus
 * suited for small, fast interrupts like the serial lines or the harddisk
 * drivers, which don't actually need signal handling etc.
 *
 * Also note that we actually save only those registers that are used in
 * C subroutines (%eax, %edx and %ecx), so if you do something weird,
 * you're on your own. The only segments that are saved (not counting the
 * automatic stack and code segment handling) are %ds and %es, and they
 * point to kernel space. No messing around with %fs here.
 */
#define SAVE_MOST \
	"cld\n\t" \
	"push %es\n\t" \
	"push %ds\n\t" \
	"pushl %eax\n\t" \
	"pushl %edx\n\t" \
	"pushl %ecx\n\t" \
	"movl $" STR(KERNEL_DS) ",%edx\n\t" \
	"mov %dx,%ds\n\t" \
	"mov %dx,%es\n\t"

#define RESTORE_MOST \
	"popl %ecx\n\t" \
	"popl %edx\n\t" \
	"popl %eax\n\t" \
	"pop %ds\n\t" \
	"pop %es\n\t" \
	"iret"

/*
 * The "inb" instructions are not needed, but seem to change the timings
 * a bit - without them it seems that the harddisk driver won't work on
 * all hardware. Arghh.
 */
#define ACK_FIRST(mask) \
	"inb $0x21,%al\n\t" \
	"jmp 1f\n" \
	"1:\tjmp 1f\n" \
	"1:\torb $" #mask ",_cache_21\n\t" \
	"movb _cache_21,%al\n\t" \
	"outb %al,$0x21\n\t" \
	"jmp 1f\n" \
	"1:\tjmp 1f\n" \
	"1:\tmovb $0x20,%al\n\t" \
	"outb %al,$0x20\n\t"

#define ACK_SECOND(mask) \
	"inb $0xA1,%al\n\t" \
	"jmp 1f\n" \
	"1:\tjmp 1f\n" \
	"1:\torb $" #mask ",_cache_A1\n\t" \
	"movb _cache_A1,%al\n\t" \
	"outb %al,$0xA1\n\t" \
	"jmp 1f\n" \
	"1:\tjmp 1f\n" \
	"1:\tmovb $0x20,%al\n\t" \
	"outb %al,$0xA0\n\t" \
	"jmp 1f\n" \
	"1:\tjmp 1f\n" \
	"1:\toutb %al,$0x20\n\t"

#define UNBLK_FIRST(mask) \
	"inb $0x21,%al\n\t" \
	"jmp 1f\n" \
	"1:\tjmp 1f\n" \
	"1:\tandb $~(" #mask "),_cache_21\n\t" \
	"movb _cache_21,%al\n\t" \
	"outb %al,$0x21\n\t"

#define UNBLK_SECOND(mask) \
	"inb $0xA1,%al\n\t" \
	"jmp 1f\n" \
	"1:\tjmp 1f\n" \
	"1:\tandb $~(" #mask "),_cache_A1\n\t" \
	"movb _cache_A1,%al\n\t" \
	"outb %al,$0xA1\n\t"

#define IRQ_NAME2(nr) nr##_interrupt(void)
#define IRQ_NAME(nr) IRQ_NAME2(IRQ##nr)
#define FAST_IRQ_NAME(nr) IRQ_NAME2(fast_IRQ##nr)
#define BAD_IRQ_NAME(nr) IRQ_NAME2(bad_IRQ##nr)

/*
 * 三个函数fast_irq_interrupt, irq_interrupt, 
 * bad_irq_interrupt对中断有不同的响应方式

 * 对于irq_interrupt, 首先将全部上下文入栈，这里构造出的栈帧结构
 * 和system_call、trap等的堆栈结构是一样的。
 * 接下来的事情是给外设发送ACK信号，表示中断接收完成。然后再构造do_IRQ的的堆栈帧，
 * 跳到irq.c中的C函数中处理
 
 * 对于fast_irq_interrupt, 保存的寄存器组要少一些，并且不允许中断嵌套
 
 * 对于bad_irq_interrupt, 回复完硬件中断直接返回，不做任何处理。
 */
#define BUILD_IRQ(chip,nr,mask) \
asmlinkage void IRQ_NAME(nr); \
asmlinkage void FAST_IRQ_NAME(nr); \
asmlinkage void BAD_IRQ_NAME(nr); \
__asm__( \
"\n.align 4\n" \
"_IRQ" #nr "_interrupt:\n\t" \
/* 注意这里的ORIG_EAX的值为-nr-2 */
	"pushl $-"#nr"-2\n\t" \
	SAVE_ALL \
	ACK_##chip(mask) \
/* intr_count是用来记录中断的嵌套层数的 */
	"incl _intr_count\n\t"\
	/* 对于所有的中断门，切换到中断处理程序时，CPU是默认清IF位的，
	 * 所以所有中断处理程序，在保存完上下文之后做的第一件事就应该是
	 * 开中断, 这样可以允许中断嵌套 */
	"sti\n\t" \
	/* 构造的栈顶为irq_number, 上下文地址 */
	"movl %esp,%ebx\n\t" \
	"pushl %ebx\n\t" \
	"pushl $" #nr "\n\t" \
	"call _do_IRQ\n\t" \
	"addl $8,%esp\n\t" \
	"cli\n\t" \
	UNBLK_##chip(mask) \
	"decl _intr_count\n\t" \
	/* 在中断结束后跳到ret_from_sys_call去处理 */
	"jmp ret_from_sys_call\n" \
"\n.align 4\n" \
"_fast_IRQ" #nr "_interrupt:\n\t" \
	SAVE_MOST \
	ACK_##chip(mask) \
	"incl _intr_count\n\t" \
	"pushl $" #nr "\n\t" \
	"call _do_fast_IRQ\n\t" \
	"addl $4,%esp\n\t" \
	"cli\n\t" \
	UNBLK_##chip(mask) \
	"decl _intr_count\n\t" \
	RESTORE_MOST \
"\n\n.align 4\n" \
"_bad_IRQ" #nr "_interrupt:\n\t" \
	SAVE_MOST \
	ACK_##chip(mask) \
	RESTORE_MOST);

#endif
