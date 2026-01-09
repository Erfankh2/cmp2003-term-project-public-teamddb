#include "analyzer.h"

std::string TripAnalyzer::trim_(const std::string& s) {
    size_t i = 0, j = s.size();
    while (i < j && is_space_(static_cast<unsigned char>(s[i]))) ++i;
    while (j > i && is_space_(static_cast<unsigned char>(s[j - 1]))) --j;

    std::string t = s.substr(i, j - i);

    // Handle CR from Windows line endings
    if (!t.empty() && t.back() == '\r') t.pop_back();

    // If quoted, strip quotes and re-trim inside
    if (t.size() >= 2 && t.front() == '"' && t.back() == '"') {
        t = t.substr(1, t.size() - 2);
        size_t a = 0, b = t.size();
        while (a < b && is_space_(static_cast<unsigned char>(t[a]))) ++a;
        while (b > a && is_space_(static_cast<unsigned char>(t[b - 1]))) --b;
        t = t.substr(a, b - a);
    }
    return t;
}

bool TripAnalyzer::splitCSVLine_(const std::string& line, std::vector<std::string>& out) {
    out.clear();
    std::string cell;
    std::stringstream ss(line);
    while (std::getline(ss, cell, ',')) out.push_back(trim_(cell));
    return !out.empty();
}

bool TripAnalyzer::parseHourFromTimestamp_(const std::string& raw, int& outHour) {
    std::string s = trim_(raw);
    if (s.empty()) return false;

    size_t colon = s.find(':');
    if (colon == std::string::npos || colon == 0) return false;

    int i = static_cast<int>(colon) - 1;
    while (i >= 0 && is_space_(static_cast<unsigned char>(s[static_cast<size_t>(i)]))) --i;
    if (i < 0) return false;

    if (!std::isdigit(static_cast<unsigned char>(s[static_cast<size_t>(i)]))) return false;
    int ones = s[static_cast<size_t>(i)] - '0';
    --i;

    while (i >= 0 && is_space_(static_cast<unsigned char>(s[static_cast<size_t>(i)]))) --i;

    int tens = -1;
    if (i >= 0 && std::isdigit(static_cast<unsigned char>(s[static_cast<size_t>(i)]))) {
        tens = s[static_cast<size_t>(i)] - '0';
    }

    int hour = (tens >= 0) ? (tens * 10 + ones) : ones;
    if (hour < 0 || hour > 23) return false;

    outHour = hour;
    return true;
}

bool TripAnalyzer::looksLikeHeader_(const std::vector<std::string>& cols) {
    if (cols.empty()) return true;

    auto lower = [](std::string x) {
        for (char& c : x) c = char(std::tolower((unsigned char)c));
        return x;
    };

    std::string c0 = lower(cols[0]);
    if (c0.find("trip") != std::string::npos) return true;

    if (cols.size() >= 2) {
        std::string c1 = lower(cols[1]);
        if (c1.find("pickup") != std::string::npos || c1.find("zone") != std::string::npos) return true;
    }
    if (cols.size() >= 3) {
        std::string c2 = lower(cols[2]);
        if (c2.find("time") != std::string::npos || c2.find("date") != std::string::npos) return true;
    }
    return false;
}

bool TripAnalyzer::parseRow_(const std::string& line, std::string& outZone, int& outHour) {
    std::vector<std::string> cols;
    if (!splitCSVLine_(line, cols)) return false;

    std::string zone, timeField;

    // Match original logic
    if (cols.size() >= 6) {
        zone = cols[1];
        timeField = cols[3];
    } else if (cols.size() == 3) {
        zone = cols[1];
        timeField = cols[2];
    } else {
        return false;
    }

    if (zone.empty() || timeField.empty()) return false;

    int hour;
    if (!parseHourFromTimestamp_(timeField, hour)) return false;

    outZone = zone;
    outHour = hour;
    return true;
}

void TripAnalyzer::processParsed_(const std::string& zone, int hour) {
    ++pickupZoneTripCounts[zone];

    auto it = zoneHourlyTripCounts.find(zone);
    if (it == zoneHourlyTripCounts.end()) {
        std::array<long long, 24> arr;
        arr.fill(0);
        arr[hour] = 1;
        zoneHourlyTripCounts.emplace(zone, arr);
    } else {
        ++(it->second[hour]);
    }
}

void TripAnalyzer::ingestStream_(std::istream& in) {
    pickupZoneTripCounts.clear();
    zoneHourlyTripCounts.clear();

    std::string line;
    std::vector<std::string> firstCols;

    // Read first non-empty line
    if (!std::getline(in, line)) return;
    while (trim_(line).empty()) {
        if (!std::getline(in, line)) return;
    }

    // Decide header vs data (ONLY for the first line, like original code)
    if (!splitCSVLine_(line, firstCols)) return;

    if (!looksLikeHeader_(firstCols)) {
        // Treat it as data
        std::string z;
        int h;
        // We already split once, but keep logic consistent: re-parse via parseRow_
        if (parseRow_(line, z, h)) processParsed_(z, h);
    }

    // Remaining lines are treated as data rows
    while (std::getline(in, line)) {
        if (trim_(line).empty()) continue;

        std::string z;
        int h;
        if (parseRow_(line, z, h)) processParsed_(z, h);
    }
}

void TripAnalyzer::ingestStdin() {
    ingestStream_(std::cin);
}

void TripAnalyzer::ingestFile(const std::string& csvPath) {
    std::ifstream in(csvPath);
    if (!in.is_open()) {
        pickupZoneTripCounts.clear();
        zoneHourlyTripCounts.clear();
        return;
    }
    ingestStream_(in);
}

std::vector<ZoneCount> TripAnalyzer::topZones(int k) const {
    if (k <= 0) return {};

    std::vector<ZoneCount> v;
    v.reserve(pickupZoneTripCounts.size());
    for (const auto& p : pickupZoneTripCounts) v.push_back({p.first, p.second});

    std::sort(v.begin(), v.end(),
              [](const ZoneCount& a, const ZoneCount& b) {
                  if (a.count != b.count) return a.count > b.count;
                  return a.zone < b.zone;
              });

    if ((int)v.size() > k) v.resize(k);
    return v;
}

std::vector<SlotCount> TripAnalyzer::topBusySlots(int k) const {
    if (k <= 0) return {};

    std::vector<SlotCount> v;
    v.reserve(zoneHourlyTripCounts.size() * 24ULL);

    for (const auto& p : zoneHourlyTripCounts) {
        for (int h = 0; h < 24; ++h) {
            long long cnt = p.second[h];
            if (cnt > 0) v.push_back({p.first, h, cnt});
        }
    }

    std::sort(v.begin(), v.end(),
              [](const SlotCount& a, const SlotCount& b) {
                  if (a.count != b.count) return a.count > b.count;
                  if (a.zone != b.zone) return a.zone < b.zone;
                  return a.hour < b.hour;
              });

    if ((int)v.size() > k) v.resize(k);
    return v;
}
