#pragma once

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

struct ZoneCount {
    std::string zone;
    long long count;
};

struct SlotCount {
    std::string zone;
    int hour;
    long long count;
};

class TripAnalyzer {
public:
    void ingestFile(const std::string& csvPath);
    void ingestStdin();

    std::vector<ZoneCount> topZones(int k = 10) const;
    std::vector<SlotCount> topBusySlots(int k = 10) const;

private:
    std::unordered_map<std::string, long long> pickupZoneTripCounts;
    std::unordered_map<std::string, std::array<long long, 24>> zoneHourlyTripCounts;

private:
    static inline bool is_space_(unsigned char c) { return std::isspace(c) != 0; }

    static std::string trim_(const std::string& s);
    static bool splitCSVLine_(const std::string& line, std::vector<std::string>& out);
    static bool parseHourFromTimestamp_(const std::string& raw, int& outHour);
    static bool looksLikeHeader_(const std::vector<std::string>& cols);

    bool parseRow_(const std::string& line, std::string& outZone, int& outHour);
    void processParsed_(const std::string& zone, int hour);
    void ingestStream_(std::istream& in);
};
