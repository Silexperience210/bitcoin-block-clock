#pragma once
class MDNSResponder { public: bool begin(const char *) { return true; } void addService(const char *, const char *, int) {} };
extern MDNSResponder MDNS;
