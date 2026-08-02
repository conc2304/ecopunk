#include "VideoEffectLoadReport.h"
#include <sstream>

namespace videoeffects {

	namespace {
		void appendList(std::ostringstream & out, const char * label, const std::vector<std::string> & ids) {
			out << label << "=" << ids.size();
			if (!ids.empty()) {
				out << " (";
				for (size_t i = 0; i < ids.size(); ++i) {
					if (i > 0) out << ", ";
					out << ids[i];
				}
				out << ")";
			}
			out << " ";
		}
	}

	std::string VideoEffectLoadReport::summary() const {
		std::ostringstream out;
		appendList(out, "requested", requested);
		appendList(out, "registered", registered);
		appendList(out, "skipped", skipped);
		appendList(out, "missing", missing);
		appendList(out, "failed", failed);
		appendList(out, "unsupported", unsupported);
		appendList(out, "fallback", fallback);
		return out.str();
	}

} // namespace videoeffects
