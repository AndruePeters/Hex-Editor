// Core Library Header: ParserEngine.h
#pragma once
#include <vector>
#include <string>

struct FieldResult {
    int startOffset;
    int length;
    std::string name;
    std::string value;
    bool hasError;
    std::string errorMessage;
};

class ParserEngine {
public:
    virtual ~ParserEngine() = default;
    virtual std::vector<FieldResult> parseBuffer(const char* data, size_t size) = 0;
};
