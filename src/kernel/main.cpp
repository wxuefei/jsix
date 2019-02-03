#include <stddef.h>
#include <stdint.h>

#include "initrd/initrd.h"
#include "kutil/assert.h"
#include "apic.h"
#include "block_device.h"
#include "console.h"
#include "cpu.h"
#include "device_manager.h"
#include "gdt.h"
#include "interrupts.h"
#include "io.h"
#include "kernel_data.h"
#include "log.h"
#include "page_manager.h"
#include "scheduler.h"
#include "screen.h"
#include "serial.h"
#include "syscall.h"

extern "C" {
	void kernel_main(popcorn_data *header);
	void *__bss_start, *__bss_end;
}

extern void __kernel_assert(const char *, unsigned, const char *);

void
init_console()
{
	serial_port *com1 = new (&g_com1) serial_port(COM1);
	console *cons = new (&g_console) console(com1);

	cons->set_color(0x21, 0x00);
	cons->puts("Popcorn OS ");
	cons->set_color(0x08, 0x00);
	cons->puts(GIT_VERSION " booting...\n");

	log::init(cons);
	//log::enable(logs::apic, log::level::debug);
	//log::enable(logs::device, log::level::info);
	log::enable(logs::driver, log::level::debug);
	log::enable(logs::memory, log::level::info);
	log::enable(logs::fs, log::level::debug);
	log::enable(logs::task, log::level::debug);
	//log::enable(logs::boot, log::level::debug);
}

void
kernel_main(popcorn_data *header)
{
#ifdef DEBUG
	// Run `waf configure --debug` to enable compiling with DEBUG turned on.
	// Then attach to QEMU's gdb server and `set waiting = false` to start
	// the kernel. This compensates for GDB's poor handling of QEMU going
	// through the x86 PC startup and switching to 64 bit mode when you
	// attach to qemu with the -S option.
	bool waiting = true;
	while (waiting);
#endif

	kutil::assert_set_callback(__kernel_assert);

	gdt_init();
	interrupts_init();

	page_manager *pager = new (&g_page_manager) page_manager;

	memory_initialize(
			header->memory_map,
			header->memory_map_length,
			header->memory_map_desc_size);

	pager->map_offset_pointer(
			&header->frame_buffer,
			header->frame_buffer_length);

	init_console();

	log::debug(logs::boot, " Popcorn header is at: %016lx", header);
	log::debug(logs::boot, "    Framebuffer is at: %016lx", header->frame_buffer);
	log::debug(logs::boot, "    Kernel data is at: %016lx", header->data);
	log::debug(logs::boot, "     Memory map is at: %016lx", header->memory_map);
	log::debug(logs::boot, "ACPI root table is at: %016lx", header->acpi_table);
	log::debug(logs::boot, "Runtime service is at: %016lx", header->runtime);

	initrd::disk ird(header->initrd);
	log::info(logs::boot, "initrd loaded with %d files.", ird.files().count());
	for (auto &f : ird.files())
		log::info(logs::boot, "  %s%s (%d bytes).", f.executable() ? "*" : "", f.name(), f.size());

	/*
	pager->dump_pml4(nullptr, 0);
	pager->dump_blocks(true);
	*/

	device_manager *devices =
		new (&device_manager::get()) device_manager(header->acpi_table);

	interrupts_enable();

	/*
	cpu_id cpu;
	log::info(logs::boot, "CPU Vendor: %s", cpu.vendor_id());
	log::info(logs::boot, "CPU Family %x Model %x Stepping %x",
			cpu.family(), cpu.model(), cpu.stepping());

	auto r = cpu.get(0x15);
	log::info(logs::boot, "CPU Crystal: %dHz", r.ecx);

	uintptr_t cr4 = 0;
	__asm__ __volatile__ ( "mov %%cr4, %0" : "=r" (cr4) );
	log::info(logs::boot, "cr4: %016x", cr4);
	*/

	devices->init_drivers();

	/*
	block_device *disk = devices->get_block_device(0);
	if (disk) {
		for (int i=0; i<1; ++i) {
			uint8_t buf[512];
			kutil::memset(buf, 0, 512);

			kassert(disk->read(0x200, sizeof(buf), buf),
					"Disk read returned 0");

			console *cons = console::get();
			uint8_t *p = &buf[0];
			for (int i = 0; i < 8; ++i) {
				for (int j = 0; j < 16; ++j) {
					cons->printf(" %02x", *p++);
				}
				cons->putc('\n');
			}
		}
	} else {
		log::warn(logs::boot, "No block devices present.");
	}
	*/

	devices->get_lapic()->calibrate_timer();

	syscall_enable();
	scheduler *sched = new (&scheduler::get()) scheduler(devices->get_lapic());

	for (auto &f : ird.files()) {
		if (f.executable())
			sched->create_process(f.name(), f.data(), f.size());
	}

	sched->start();
}
