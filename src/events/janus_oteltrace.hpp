#pragma once

#include <jansson.h>

#include <stdio.h>
#include <stdlib.h>

#include <map>
#include <string>
#include <cstdint>
#include <cinttypes>
#include <type_traits>

#include "opentelemetry/exporters/fluentd/trace/fluentd_exporter.h"
#include "opentelemetry/trace/provider.h"
#include "opentelemetry/sdk/trace/tracer_provider.h"
#include "opentelemetry/sdk/trace/simple_processor.h"

typedef enum: uint64_t
{
	EVENT_TYPE_NONE = (0),
	/*! \brief Session related events (e.g., session created/destroyed, etc.) */
	EVENT_TYPE_SESSION = (1 << 0),
	/*! \brief Handle related events (e.g., handle attached/detached, etc.) */
	EVENT_TYPE_HANDLE = (1 << 1),
	/*! \brief External events originated via Admin API (e.g., custom events coming from external scripts) */
	EVENT_TYPE_EXTERNAL = (1 << 2),
	/*! \brief JSEP related events (e.g., got/sent offer/answer) */
	EVENT_TYPE_JSEP = (1 << 3),
	/*! \brief WebRTC related events (e.g., PeerConnection up/down, ICE updates, DTLS updates, etc.) */
	EVENT_TYPE_WEBRTC = (1 << 4),
	/*! \brief Media related events (e.g., media started/stopped flowing, stats on packets/bytes, etc.) */
	EVENT_TYPE_MEDIA = (1 << 5),
	/*! \brief Events originated by plugins (at the moment, all of them, no way to pick) */
	EVENT_TYPE_PLUGIN = (1 << 6),
	/*! \brief Events originated by transports (at the moment, all of them, no way to pick) */
	EVENT_TYPE_TRANSPORT = (1 << 7),
	/*! \brief Events originated by the core for its own events (e.g., Janus starting/shutting down) */
	EVENT_TYPE_CORE = (1 << 8)
	/* TODO Others? */
} janus_event_type_t;

typedef struct janus_event_t
{
	std::string emitter; // event source.
	uint64_t type;		 // "type" : <numeric event type identifier>,
	uint64_t subtype;	 // ...
	uint64_t timestamp;	 // "timestamp" : <monotonic time of when the event was generated>,
	uint64_t session_id; // "session_id" : <unique session identifier>,
	uint64_t handle_id;	 // "handle_id" : <unique handle identifier, if provided/available>,
	std::string event;	 // <event body, custom depending on event type>
} janus_event_t;

void print_janus_event_t(janus_event_t *value);

janus_event_t to_janus_event_t(json_t *obj);

extern "C" void janus_otel_trace(json_t *obj);