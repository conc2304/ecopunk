#include "SystemClockService.h"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <set>
#include <sstream>

#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

namespace systemclock {

namespace {

const char* const kTimedatectl = "/usr/bin/timedatectl";

// Curated shortlist shown first in the timezone editor.
const char* const kCommonZones[] = {
	"UTC",
	"America/New_York", "America/Chicago", "America/Denver", "America/Phoenix", "America/Los_Angeles",
	"America/Anchorage", "Pacific/Honolulu",
	"America/Toronto", "America/Mexico_City", "America/Sao_Paulo",
	"Europe/London", "Europe/Madrid", "Europe/Paris", "Europe/Berlin",
	"Asia/Tokyo", "Asia/Shanghai", "Asia/Kolkata", "Australia/Sydney",
};

bool zoneNameSafe(const std::string& z) {
	if (z.empty() || z.size() > 64 || z.front() == '/' || z.front() == '-' || z.find("..") != std::string::npos) return false;
	for (char c : z) {
		const bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '+' || c == '-' || c == '/';
		if (!ok) return false;
	}
	return true;
}

std::string trim(const std::string& s) {
	size_t a = 0, b = s.size();
	while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r' || s[a] == '\n')) ++a;
	while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r' || s[b - 1] == '\n')) --b;
	return s.substr(a, b - a);
}

} // namespace

SystemClockService::SystemClockService() : SystemClockService(&SystemClockService::runFixedArgv) {}

SystemClockService::SystemClockService(Runner runner, std::string zoneinfoDir)
	: runner_(std::move(runner)), zoneinfoDir_(std::move(zoneinfoDir)) {
	loadZones();
}

void SystemClockService::loadZones() {
	std::set<std::string> all;
	std::ifstream tab(zoneinfoDir_ + "/zone1970.tab");
	std::string line;
	while (std::getline(tab, line)) {
		if (line.empty() || line[0] == '#') continue;
		std::istringstream in(line);
		std::string codes, coords, zone;
		if (!(in >> codes >> coords >> zone)) continue;
		if (zoneNameSafe(zone)) all.insert(zone);
	}
	all.insert("UTC");
	zones_.clear();
	std::set<std::string> seen;
	for (const char* z : kCommonZones) {
		// With a real zoneinfo the shortlist must exist; without one, keep it all.
		if (all.size() > 1 && all.find(z) == all.end() && std::string(z) != "UTC") {
			struct stat st;
			if (stat((zoneinfoDir_ + "/" + z).c_str(), &st) != 0) continue;
		}
		zones_.push_back(z);
		seen.insert(z);
	}
	commonCount_ = zones_.size();
	for (const auto& z : all) zones_.push_back(z); // std::set = sorted
}

bool SystemClockService::isKnownZone(const std::string& zone) const {
	return std::find(zones_.begin(), zones_.end(), zone) != zones_.end();
}

bool SystemClockService::timedatectlPresent() {
	struct stat st;
	return stat(kTimedatectl, &st) == 0 && (st.st_mode & S_IXUSR);
}

std::string SystemClockService::zoneFromLocaltimeLink(const std::string& linkTarget) {
	const std::string marker = "zoneinfo/";
	const size_t pos = linkTarget.rfind(marker);
	if (pos == std::string::npos) return std::string();
	const std::string zone = linkTarget.substr(pos + marker.size());
	return zoneNameSafe(zone) ? zone : std::string();
}

void SystemClockService::parseTimedatectlShow(const std::string& text, Snapshot& out) {
	std::istringstream in(text);
	std::string line;
	while (std::getline(in, line)) {
		const size_t eq = line.find('=');
		if (eq == std::string::npos) continue;
		const std::string key = trim(line.substr(0, eq));
		const std::string value = trim(line.substr(eq + 1));
		if (key == "Timezone" && zoneNameSafe(value)) out.timezone = value;
		else if (key == "NTP") out.ntp = value == "yes" ? NtpState::On : value == "no" ? NtpState::Off : NtpState::Unknown;
		else if (key == "NTPSynchronized") { out.ntpSynchronized = value == "yes"; out.ntpSynchronizedKnown = value == "yes" || value == "no"; }
	}
}

Snapshot SystemClockService::read() const {
	Snapshot s;
	const std::time_t now = std::time(nullptr);
	tzset(); // honour a changed TZ or /etc/localtime
	std::tm lt{};
	if (localtime_r(&now, &lt) != nullptr) {
		s.local = {lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday, lt.tm_hour, lt.tm_min};
		s.second = lt.tm_sec;
	}
	s.source = "none";
	if (runner_ && timedatectlPresent()) {
		std::string out;
		if (runner_({kTimedatectl, "show", "-p", "Timezone", "-p", "NTP", "-p", "NTPSynchronized"}, out) == 0) {
			parseTimedatectlShow(out, s);
			if (!s.timezone.empty() || s.ntp != NtpState::Unknown) s.source = "timedatectl";
		}
	}
	if (s.timezone.empty()) {
		char buf[256];
		const ssize_t n = readlink("/etc/localtime", buf, sizeof(buf) - 1);
		if (n > 0) {
			buf[n] = '\0';
			s.timezone = zoneFromLocaltimeLink(buf);
			if (!s.timezone.empty() && s.source == "none") s.source = "/etc/localtime";
		}
	}
	return s;
}

WriteCapability SystemClockService::writeCapability() const {
	if (writeOverride_) return *writeOverride_ ? WriteCapability{true, std::string()} : WriteCapability{false, "test: writes refused"};
#if defined(__linux__)
	const char* flag = std::getenv("EXPERIENCE_RUNTIME_CLOCK_WRITE");
	if (!timedatectlPresent()) return {false, "timedatectl is not installed on this system."};
	if (flag == nullptr || std::string(flag) != "1") {
		return {false, "Platform must authorise clock writes (polkit rule for the kiosk user, then EXPERIENCE_RUNTIME_CLOCK_WRITE=1)."};
	}
	return {true, std::string()};
#else
	return {false, "Clock changes are only available on the Rock Pi (Linux); this system is read-only."};
#endif
}

ApplyResult SystemClockService::apply(const ClockChange& change) const {
	const WriteCapability cap = writeCapability();
	if (!cap.enabled) return {false, "Requires platform setup: " + cap.note};
	if (change.empty()) return {false, "Nothing to apply."};
	if (change.timezone && !isKnownZone(*change.timezone)) return {false, "Unknown timezone '" + *change.timezone + "'."};
	if (change.dateTime && !validDateTime(*change.dateTime)) return {false, "Invalid date/time."};
	if (!runner_) return {false, "No process runner."};

	std::string summary;
	auto run = [&](std::vector<std::string> argv, const std::string& what) -> bool {
		std::vector<std::string> full = {kTimedatectl, "--no-ask-password"};
		full.insert(full.end(), argv.begin(), argv.end());
		std::string out;
		const int status = runner_(full, out);
		if (status != 0) {
			std::string detail = trim(out);
			if (detail.size() > 120) detail = detail.substr(0, 120);
			summary = "FAILED: " + what + (detail.empty() ? std::string() : " - " + detail);
			return false;
		}
		return true;
	};
	if (change.ntp && !run({"set-ntp", *change.ntp ? "true" : "false"}, "set automatic time")) return {false, summary};
	if (change.timezone && !run({"set-timezone", *change.timezone}, "set timezone")) return {false, summary};
	if (change.dateTime) {
		char buf[32];
		const DateTime& d = *change.dateTime;
		std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:00", d.year, d.month, d.day, d.hour, d.minute);
		if (!run({"set-time", buf}, "set date/time")) return {false, summary};
	}
	return {true, "Applied."};
}

int SystemClockService::runFixedArgv(const std::vector<std::string>& argv, std::string& output) {
	output.clear();
	if (argv.empty()) return -1;
	int fds[2];
	if (pipe(fds) != 0) return -1;
	const pid_t pid = fork();
	if (pid < 0) { close(fds[0]); close(fds[1]); return -1; }
	if (pid == 0) {
		dup2(fds[1], STDOUT_FILENO);
		dup2(fds[1], STDERR_FILENO);
		close(fds[0]);
		close(fds[1]);
		std::vector<char*> args;
		for (const auto& a : argv) args.push_back(const_cast<char*>(a.c_str()));
		args.push_back(nullptr);
		execv(args[0], args.data()); // absolute path, fixed argv: no shell, no PATH search
		_exit(127);
	}
	close(fds[1]);
	const int flags = fcntl(fds[0], F_GETFL, 0);
	fcntl(fds[0], F_SETFL, flags | O_NONBLOCK);
	const time_t deadline = std::time(nullptr) + 5;
	bool done = false;
	while (!done) {
		struct pollfd p = {fds[0], POLLIN, 0};
		const int r = poll(&p, 1, 200);
		if (r > 0) {
			char buf[512];
			const ssize_t n = ::read(fds[0], buf, sizeof(buf));
			if (n > 0) output.append(buf, size_t(n));
			else if (n == 0) done = true;
		}
		if (std::time(nullptr) > deadline) { kill(pid, SIGKILL); break; }
	}
	close(fds[0]);
	int status = 0;
	if (waitpid(pid, &status, 0) < 0) return -1;
	return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

} // namespace systemclock
