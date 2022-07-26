// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include "opentelemetry/exporters/fluentd/log/fluentd_exporter.h"
#include "opentelemetry/logs/provider.h"
#include "opentelemetry/sdk/logs/logger_provider.h"
#include "opentelemetry/sdk/logs/simple_log_processor.h"

#include "janus_otellog.hpp"

namespace sdk_logs = opentelemetry::sdk::logs;
namespace nostd = opentelemetry::nostd;

opentelemetry::logs::Severity otel_get_severity(char severity)
{
    opentelemetry::logs::Severity result = opentelemetry::logs::Severity::kDebug;
    switch(severity)
    {
        case 'F':
            result = opentelemetry::logs::Severity::kFatal;
            break;
	    case 'E':
            result = opentelemetry::logs::Severity::kError;
            break;
	    case 'W':
            result = opentelemetry::logs::Severity::kWarn;
            break;
	    case 'I':
            result = opentelemetry::logs::Severity::kInfo;
            break;
	    case 'V':
            result = opentelemetry::logs::Severity::kDebug3;
            break;
	    case 'H':
            result = opentelemetry::logs::Severity::kDebug2;
            break;
	    case 'D':
            result = opentelemetry::logs::Severity::kDebug;
            break;
        default:
            break;
    }
    return result;
}

bool otel_logger_init() {

    opentelemetry::exporter::fluentd::common::FluentdExporterOptions options;
    options.endpoint = "tcp://localhost:24222";

    auto exporter = std::unique_ptr<sdk_logs::LogExporter>(
        new opentelemetry::exporter::fluentd::logs::FluentdExporter(options));

    auto processor = std::unique_ptr<sdk_logs::LogProcessor>(
        new sdk_logs::SimpleLogProcessor(std::move(exporter)));

    auto provider = std::shared_ptr<opentelemetry::logs::LoggerProvider>(
        new opentelemetry::sdk::logs::LoggerProvider(std::move(processor)));

    auto pr =
        static_cast<opentelemetry::sdk::logs::LoggerProvider*>(provider.get());

    // Set the global logger provider
    opentelemetry::logs::Provider::SetLoggerProvider(provider);
    return true;
}

static bool otel_inited = otel_logger_init();

void otel_log_line(janus_jsonlog_line *jline)
{
    // TODO: it might be slightly more optimal to cache the provider
    // and logger rather than performing look-up each time.
    auto provider = opentelemetry::logs::Provider::GetLoggerProvider();
    auto logger = provider->GetLogger("JanusLogger", "", "janus_gateway", "v1.0.0");

    // Severity is embedded into jline->line string.
    // Extract severity code prior to first '|' if applicable.
    int8_t offset = 0;
    auto severity = opentelemetry::logs::Severity::kInfo;
    if ((jline->line[0]!=0)&&(jline->line[1]=='|'))
    {
        offset=2;
        severity = otel_get_severity(jline->line[0]);
    }

    logger->Log(severity,
        jline->line+offset, // Skip severity code if applicable
        {
            {"timestamp", jline->timestamp},    // Orig timestamp
            {"tid", jline->tid}                 // Thread ID
            // TODO: add additional attributes
            // - traceid
            // - spanid
            // - traceflags
            // - tracestate
        });
}
