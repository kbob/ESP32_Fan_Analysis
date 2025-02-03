#include "trace_buffer.h"

portMUX_TYPE TraceBuffer::spinlock = portMUX_INITIALIZER_UNLOCKED;
bool TraceBuffer::tracing_enabled;
size_t TraceBuffer::trace_index;
TraceEvent *TraceBuffer::psram_trace_buffer;
