#include "VideoSampler.h"
#include "ofPixels.h"
#include "ofFileUtils.h"
#include "ofUtils.h"
#include "ofLog.h"
#include <algorithm>
#include <cstdlib>

namespace {
	constexpr int SETTLE_FRAMES_MAX = 60;  // max frames to wait after seek before giving up (~2.5s at 24fps)
	constexpr size_t MAX_QUEUE_DEPTH = 2;  // doc: "Maximum 2 live (non-frozen) fragments"
	constexpr size_t LATENCY_SAMPLE_COUNT = 20;
}

void VideoSampler::setup(const std::string& mediaPath){
	ofDirectory dir;
	dir.allowExt("mp4");
	dir.listDir(mediaPath);

	for(const auto& file : dir.getFiles()){
		mediaFiles.push_back(file.getAbsolutePath());
	}

	if(mediaFiles.empty()){
		ofLogWarning("VideoSampler") << "no .mp4 files found in " << mediaPath
			<< " - fragments will use placeholder colors";
	} else {
		ofLogNotice("VideoSampler") << "found " << mediaFiles.size() << " media file(s) in " << mediaPath;
	}
}

void VideoSampler::selectVideoForCycle(){
	if(!hasMedia()){
		return;
	}

	currentFileIndex = (currentFileIndex + 1) % static_cast<int>(mediaFiles.size());
	player.close();
	player.load(mediaFiles[currentFileIndex]);
	player.setLoopState(OF_LOOP_NORMAL);
	player.play(); // keep playing — paused mode on macOS AVFoundation never decodes frames on seek

	ofLogNotice("VideoSampler") << "cycle video: " << mediaFiles[currentFileIndex];
}

bool VideoSampler::requestCapture(float normalizedOffset, CaptureCallback callback){
	if(!hasMedia() || queue.size() >= MAX_QUEUE_DEPTH){
		return false;
	}
	queue.push_back({normalizedOffset, std::move(callback)});
	return true;
}

void VideoSampler::cancelPending(){
	queue.clear();
	state = State::IDLE;
}

void VideoSampler::update(){
	if(!hasMedia()){
		return;
	}

	player.update();

	if(state == State::IDLE && !queue.empty()){
		player.setPosition(queue.front().offset);
		seekRequestTime = ofGetElapsedTimef();
		settleFramesRemaining = SETTLE_FRAMES_MAX;
		state = State::SEEKING;
		return;
	}

	if(state == State::SEEKING){
		settleFramesRemaining--;

		bool frameReady = player.isFrameNew();
		bool timedOut = settleFramesRemaining <= 0;

		if(frameReady || timedOut){
			const ofPixels& pixels = player.getPixels();
			if(pixels.isAllocated() && pixels.getWidth() > 0){
				recordLatency((ofGetElapsedTimef() - seekRequestTime) * 1000.0f);
				CaptureCallback callback = std::move(queue.front().callback);
				queue.pop_front();
				state = State::IDLE;
				callback(pixels);
			} else {
				ofLogWarning("VideoSampler") << "seek produced no pixels (allocated="
					<< pixels.isAllocated() << " w=" << pixels.getWidth()
					<< " frameReady=" << frameReady << " timedOut=" << timedOut
					<< ") — using placeholder";
				queue.pop_front();
				state = State::IDLE;
			}
		}
	}
}

void VideoSampler::recordLatency(float ms){
	seekLatenciesMs.push_back(ms);
	if(!latencyLogged && seekLatenciesMs.size() >= LATENCY_SAMPLE_COUNT){
		logLatencyStats();
		latencyLogged = true;
	}
}

void VideoSampler::logLatencyStats() const{
	std::vector<float> sorted = seekLatenciesMs;
	std::sort(sorted.begin(), sorted.end());

	size_t p50Index = sorted.size() / 2;
	size_t p95Index = std::min(sorted.size() - 1, static_cast<size_t>(sorted.size() * 0.95f));

	ofLogNotice("VideoSampler") << "seek latency over " << sorted.size() << " seeks: "
		<< "P50=" << sorted[p50Index] << "ms, P95=" << sorted[p95Index] << "ms";
}
