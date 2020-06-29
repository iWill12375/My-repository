#include <defs.h>
#include <x86.h>
#include <elf.h>

/* *********************************************************************
 * This a dirt simple boot loader, whose sole job is to boot
 * an ELF kernel image from the first IDE hard disk.
 *
 * DISK LAYOUT
 *  * This program(bootasm.S and bootmain.c) is the bootloader.
 *    It should be stored in the first sector of the disk.
 *它应该存放在硬盘第一扇区
 *  * The 2nd sector onward holds the kernel image.
 *第二扇区存放的是内核映像
 *  * The kernel image must be in ELF format.
 *内核映像必须是ELF格式的
 * BOOT UP STEPS
 *  * when the CPU boots it loads the BIOS into memory and executes it
 *当CPU启动，它加载BIOS到内存并且执行BIOS
 *  * the BIOS intializes devices, sets of the interrupt routines, and
 *    reads the first sector of the boot device(e.g., hard-drive)
 *    into memory and jumps to it.
 *BIOS初始化设备，设置中断例程，并加载启动设备（比如硬盘）的第一扇区到内存，然后跳转到那里
 *  * Assuming this boot loader is stored in the first sector of the
 *    hard-drive, this code takes over...
 *假设这个bootloader存储在硬盘第一扇区，这段代码就接管控制权了
 *  * control starts in bootasm.S -- which sets up protected mode,
 *    and a stack so C code then run, then calls bootmain()
 *控制开始于bootmain.S，它启动保护模式，并设置C代码的栈，然后运行C代码，调用bootmain()
 *  * bootmain() in this file takes over, reads in the kernel and jumps to it.
这个文件中的bootmain()函数接管控制权，它加载操作系统内核并跳转到那里执行内核
 * */

#define SECTSIZE        512//设置 sector size 为512单位大小
#define ELFHDR          ((struct elfhdr *)0x10000)      // scratch space

/* waitdisk - 等待磁盘就绪 */
static void waitdisk(void) {
    while ((inb(0x1F7) & 0xC0) != 0x40)
        /* do nothing */;
}

/* readsect - 读取一个单个扇区 at @secno into @dst */
static void readsect(void *dst, uint32_t secno) {
    // wait for disk to be ready
    waitdisk();

    outb(0x1F2, 1);                         // count = 1
    outb(0x1F3, secno & 0xFF);
    outb(0x1F4, (secno >> 8) & 0xFF);
    outb(0x1F5, (secno >> 16) & 0xFF);
    outb(0x1F6, ((secno >> 24) & 0xF) | 0xE0);
    outb(0x1F7, 0x20);                      // cmd 0x20 - read sectors

    // wait for disk to be ready
    waitdisk();

    // read a sector
    insl(0x1F0, dst, SECTSIZE / 4);
}

/* *
 * readseg - read @count bytes at @offset from kernel into virtual address @va,
 * might copy more than asked.
 * */
static void readseg(uintptr_t va, uint32_t count, uint32_t offset) {
    uintptr_t end_va = va + count;

    // round down to sector boundary
    va -= offset % SECTSIZE;

    // translate from bytes to sectors; kernel starts at sector 1
    uint32_t secno = (offset / SECTSIZE) + 1;

    // If this is too slow, we could read lots of sectors at a time.
    // We'd write more to memory than asked, but it doesn't matter --
    // we load in increasing order.
    for (; va < end_va; va += SECTSIZE, secno ++) {
        readsect((void *)va, secno);
    }
}

/* bootmain - bootloader的入口 */
void bootmain(void) {
    // 读取磁盘的第一页
    readseg((uintptr_t)ELFHDR, SECTSIZE * 8, 0);

    // 是否为有效的 ELF 文件?
    if (ELFHDR->e_magic != ELF_MAGIC) {
        goto bad;
    }

    struct proghdr *ph, *eph;

    // load each program segment (ignores ph flags)
    ph = (struct proghdr *)((uintptr_t)ELFHDR + ELFHDR->e_phoff);
    eph = ph + ELFHDR->e_phnum;
    for (; ph < eph; ph ++) {
        readseg(ph->p_va & 0xFFFFFF, ph->p_memsz, ph->p_offset);
    }

    // call the entry point from the ELF header
    // note: does not return
    ((void (*)(void))(ELFHDR->e_entry & 0xFFFFFF))();

bad:
    outw(0x8A00, 0x8A00);
    outw(0x8A00, 0x8E00);

    /* do nothing */
    while (1);
}

