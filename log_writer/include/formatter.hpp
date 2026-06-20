#pragma once

#include "log_types.hpp"

#include <string>

namespace naviai::log {

class Formatter {
  public:
    virtual ~Formatter() = default;
    virtual std::string Format(const LogRecord& record) const = 0;
};

class TextFormatter final : public Formatter {
  public:
    std::string Format(const LogRecord& record) const override;
};

class JsonFormatter final : public Formatter {
  public:
    std::string Format(const LogRecord& record) const override;
};

class FormatterSelector {
  public:
    explicit FormatterSelector(OutputFormat output_format);

    const Formatter& Get() const;
    OutputFormat GetOutputFormat() const;

  private:
    OutputFormat output_format_;
    TextFormatter text_formatter_;
    JsonFormatter json_formatter_;
};

}  // namespace naviai::log
