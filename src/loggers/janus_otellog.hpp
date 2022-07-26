#pragma once

#include <cstdint>

/* Structure we use for queueing log lines */
typedef struct janus_jsonlog_line {
	int64_t timestamp;		/* When the log line was printed */
	char *line;				/* Content of the log line */
	int8_t level;
	char* traceid;
	int64_t spanid;
	int traceflags;
	int tracestate;
	int64_t tid;
} janus_jsonlog_line;

extern "C" void otel_log_line(janus_jsonlog_line *jline);