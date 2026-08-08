#include "HudWidgetRegistry.h"

#include "AmbientFieldWidget.h"
#include "BindingPlaceholderWidget.h"
#include "ChannelStripWidget.h"
#include "EffectChipsWidget.h"
#include "LabelWidget.h"
#include "MetadataCardWidget.h"
#include "NumericValueWidget.h"
#include "ProgressBarWidget.h"
#include "ProgressRingWidget.h"
#include "SparklineWidget.h"
#include "StatusBadgeWidget.h"
#include "TimelineWidget.h"

#include <stdexcept>

namespace hudpresent {

struct HudWidgetRegistry::Impl {
	LabelWidget label;
	StatusBadgeWidget statusBadge;
	NumericValueWidget numericValue;
	ProgressBarWidget progressBar;
	ProgressRingWidget progressRing;
	SparklineWidget sparkline;
	EffectChipsWidget effectChips;
	MetadataCardWidget metadataCard;
	ChannelStripWidget channelStrip;
	AmbientFieldWidget ambientField;
	TimelineWidget timeline;
	BindingPlaceholderWidget bindingPlaceholder;
};

HudWidgetRegistry::HudWidgetRegistry() : impl_(new Impl()) {}
HudWidgetRegistry::~HudWidgetRegistry() { delete impl_; }

const IHudWidget& HudWidgetRegistry::widgetFor(HudWidgetType type) const {
	switch (type) {
		case HudWidgetType::Label: return impl_->label;
		case HudWidgetType::StatusBadge: return impl_->statusBadge;
		case HudWidgetType::NumericValue: return impl_->numericValue;
		case HudWidgetType::ProgressBar: return impl_->progressBar;
		case HudWidgetType::ProgressRing: return impl_->progressRing;
		case HudWidgetType::Sparkline: return impl_->sparkline;
		case HudWidgetType::EffectChips: return impl_->effectChips;
		case HudWidgetType::MetadataCard: return impl_->metadataCard;
		case HudWidgetType::ChannelStrip: return impl_->channelStrip;
		case HudWidgetType::AmbientField: return impl_->ambientField;
		case HudWidgetType::Timeline: return impl_->timeline;
		case HudWidgetType::BindingPlaceholder: return impl_->bindingPlaceholder;
	}
	throw std::logic_error("HudWidgetRegistry::widgetFor: unhandled HudWidgetType");
}

} // namespace hudpresent
