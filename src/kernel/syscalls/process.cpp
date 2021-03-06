#include "j6/errors.h"
#include "j6/types.h"

#include "log.h"
#include "objects/process.h"
#include "objects/thread.h"
#include "scheduler.h"

namespace syscalls {

j6_status_t
process_koid(j6_koid_t *koid)
{
	if (koid == nullptr) {
		return j6_err_invalid_arg;
	}

	TCB *tcb = scheduler::get().current();
	process &p = thread::from_tcb(tcb)->parent();

	*koid = p.koid();
	return j6_status_ok;
}

j6_status_t
process_exit(int64_t status)
{
	auto &s = scheduler::get();
	TCB *tcb = s.current();
	process &p = thread::from_tcb(tcb)->parent();

	log::debug(logs::syscall, "Process %llx exiting with code %d", p.koid(), status);

	p.exit(status);
	s.schedule();

	log::error(logs::syscall, "returned to exit syscall");
	return j6_err_unexpected;
}

} // namespace syscalls
