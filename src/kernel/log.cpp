#include "kutil/constexpr_hash.h"
#include "kutil/memory.h"
#include "console.h"
#include "log.h"

namespace logs {

static uint8_t log_buffer[0x10000];
static log::logger g_logger(log_buffer, sizeof(log_buffer));

#define LOG(name, lvl) \
	log::area_t name = #name ## _h; \
	const char * name ## _name = #name;
#include "log_areas.inc"
#undef LOG

static const uint8_t level_colors[] = {0x07, 0x07, 0x0f, 0x0b, 0x09};

void
output_logs()
{
	uint8_t buffer[257];
	auto *ent = reinterpret_cast<log::logger::entry *>(buffer);
	auto *cons = console::get();

	while (true) {
		if(g_logger.get_entry(buffer, sizeof(buffer))) {
			buffer[ent->bytes] = 0;
			cons->set_color(level_colors[static_cast<int>(ent->severity)]);
			cons->printf("%9s %8s: %s\n",
					g_logger.area_name(ent->area),
					g_logger.level_name(ent->severity),
					ent->message);
			cons->set_color();
		}
	}
}

void init()
{
	new (&g_logger) log::logger(log_buffer, sizeof(log_buffer));

#define LOG(name, lvl) \
	g_logger.register_area(name, name ## _name, log::level ::lvl);
#include "log_areas.inc"
#undef LOG
}

} // namespace logs
