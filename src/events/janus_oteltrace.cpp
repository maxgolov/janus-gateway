#include "janus_oteltrace.hpp"

#include <map>
#include <string>
#include <algorithm>

namespace sdktrace = opentelemetry::sdk::trace;
namespace nostd = opentelemetry::nostd;

using Span = opentelemetry::trace::Span;

namespace detail
{

	/// <summary>
	/// Compile-time constexpr djb2 hash function for strings
	/// </summary>
	static constexpr uint32_t hashCode(const char *str, uint32_t h = 0)
	{
		return (uint32_t)(!str[h] ? 5381 : ((uint32_t)hashCode(str, h + 1) * (uint32_t)33) ^ str[h]);
	}

}

#define CONST_UINT32_T(x) std::integral_constant<uint32_t, (uint32_t)x>::value

#define CONST_HASHCODE(name) CONST_UINT32_T(detail::hashCode(#name))

void print_janus_event_t(janus_event_t *value)
{
	printf("\n");
	printf("***************************************************\n");
	printf("Janus OpenTelemetry event:\n");
	printf("emitter     = %s\n", value->emitter.c_str());
	printf("type        = %10" PRId64 "\n", value->type);
	printf("subtype     = %10" PRId64 "\n", value->subtype);
	printf("timestamp   = %10" PRId64 "\n", value->timestamp);
	printf("session_id  = %10" PRId64 "\n", value->session_id);
	printf("handle_id   = %10" PRId64 "\n", value->handle_id);
	printf("event       = %s\n", value->event.c_str());
	printf("***************************************************\n");
}

janus_event_t to_janus_event_t(json_t *obj)
{
	janus_event_t result = {"", 0, 0, 0, 0, 0, ""};

	if (obj == nullptr)
	{
		// invalid object
		return result;
	}

	const char *key;
	json_t *value;

	json_object_foreach(obj, key, value)
	{
		switch (detail::hashCode(key))
		{
		case CONST_HASHCODE(emitter):
			result.emitter = json_string_value(value);
			break;
		case CONST_HASHCODE(type):
			result.type = json_integer_value(value);
			break;
		case CONST_HASHCODE(subtype):
			result.subtype = json_integer_value(value);
			break;
		case CONST_HASHCODE(timestamp):
			result.timestamp = json_integer_value(value);
			break;
		case CONST_HASHCODE(session_id):
			result.session_id = json_integer_value(value);
			break;
		case CONST_HASHCODE(handle_id):
			result.handle_id = json_integer_value(value);
			break;
		case CONST_HASHCODE(event):
		{
			auto s = json_dumps(value, JSON_COMPACT | JSON_ENSURE_ASCII);
			result.event = s;
			free(s);
		}
		break;
		default:
			// drop any unknown fields
			break;
		}
	}

	return result;
}

bool otel_tracer_init()
{

	opentelemetry::exporter::fluentd::common::FluentdExporterOptions options;
	options.endpoint = "tcp://localhost:24222";
	options.convert_event_to_trace = true;

	auto exporter = std::unique_ptr<sdktrace::SpanExporter>(
		new opentelemetry::exporter::fluentd::trace::FluentdExporter(options));
	auto processor = std::unique_ptr<sdktrace::SpanProcessor>(
		new sdktrace::SimpleSpanProcessor(std::move(exporter)));
	auto provider = nostd::shared_ptr<opentelemetry::trace::TracerProvider>(
		new sdktrace::TracerProvider(std::move(processor)));

	// Set the global trace provider
	opentelemetry::trace::Provider::SetTracerProvider(provider);

	return true;
}

static bool otel_tracer_inited = otel_tracer_init();
static auto provider = opentelemetry::trace::Provider::GetTracerProvider();
static auto tracer = provider->GetTracer("janus_gateway");

std::map<uint8_t, nostd::shared_ptr<Span>> sessions;

auto StartSpan(uint64_t session_id)
{
	nostd::shared_ptr<Span> &result = sessions[session_id];
	if (result == nullptr)
	{
		result = tracer->StartSpan("span");
	}
	return result;
}

void EndSpan(uint64_t session_id)
{
	nostd::shared_ptr<Span> &result = sessions[session_id];
	if (result != nullptr)
	{
		result->End();
	}
	sessions.erase(session_id);
}

void prepare_stats_event(
	std::map<std::string, opentelemetry::common::AttributeValue>& evt,
	json_t *event)
{
	// Clear JSON-encoded event column
	evt.erase("event");

	// Upgrade 2nd level (Part B) JSON contents of event to main event Part A columns
	const char *key;
	json_t *value;
	json_object_foreach(event, key, value)
	{
		std::string sKey = key;
		std::replace(sKey.begin(), sKey.end(), '-', '_'); // replace all '-' to '_'

		switch (json_typeof(value))
		{
		case JSON_OBJECT:
			// We don't support nested objects
			evt[sKey] = "<object>";
			break;
		case JSON_ARRAY:
			// We don't support nested arrays
			evt[sKey] = "<array>";
			break;
		case JSON_STRING:
			evt[sKey] = json_string_value(value);
			break;
		case JSON_INTEGER:
			evt[sKey] = static_cast<uint64_t>(json_integer_value(value));
			break;
		case JSON_REAL:
			evt[sKey] = json_real_value(value);
			break;
		case JSON_TRUE:
			evt[sKey] = 1;
			break;
		case JSON_FALSE:
			evt[sKey] = 0;
			break;
		case JSON_NULL:
			evt[sKey] = "";
			break;
		default:
			// skip unknown types
			break;
		}
	}
}

void janus_otel_trace(json_t *obj)
{
	janus_event_t jevt = to_janus_event_t(obj);
	// print_janus_event_t(&jevt);

	// Common events:
	//
	// 1   - * created|destroyed
	// 2   - * attached
	// 8   - 0 connection detail
	//
	// 16  - 1 ice=gathering|connecting
	// 16  - 3 ip address negotiate
	// 16  - 4 ip address negotiate
	// 16  - 5 dtls=trying|connected
	// 16  - 6 connection=hangup
	//
	// 32  - 3 stats
	// 32  - 1 heartbeat
	//
	// 128 - * connected|disconnected

	auto span = StartSpan(jevt.session_id);

	// TODO #1: figure out what additional attributes we want to append to Span event
	// TODO #2: determine if we need to match "scope" by subtype
	span->SetAttribute("attribute_key", "attribute_value");

	std::map<std::string, opentelemetry::common::AttributeValue> baseEvent = 
	{
		{"emitter", jevt.emitter.c_str()},
		{"type", jevt.type},
		{"subtype", jevt.subtype},
		{"timestamp", jevt.timestamp},
		{"session_id", jevt.session_id},
		{"handle_id", jevt.handle_id},
		{"event", jevt.event.c_str()}
	};

	span->AddEvent("JanusTraceEvent", baseEvent);

	auto evtType = static_cast<janus_event_type_t>(jevt.type);
	switch (evtType)
	{

	case janus_event_type_t::EVENT_TYPE_SESSION:
	{
		// Session event
		json_t *event = json_object_get(obj, "event");
		if (event != nullptr)
		{
			// event.name
			json_t *eventName = json_object_get(event, "name");
			if (eventName != nullptr)
			{
				switch (detail::hashCode(json_string_value(eventName)))
				{
				case CONST_HASHCODE(destroyed):
					EndSpan(jevt.session_id);
					break;
				default:
					break;
				}
				json_decref(eventName);
			}
			json_decref(event);
		}
	}
	break;

	case janus_event_type_t::EVENT_TYPE_MEDIA:
	{
		// Stats event
		json_t *event = json_object_get(obj, "event");
		if (event != nullptr)
		{
			prepare_stats_event(baseEvent, event);
			span->AddEvent("JanusStatsEvent", baseEvent);
			json_decref(event);
		}
	}
	break;

	default:
		break;
	}
}
