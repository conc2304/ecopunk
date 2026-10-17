# Predeploy Blob detection preset coverage check (see BlobPresetCoverage.h).
# openFrameworks-free: links the production MediaCatalog / catalog-override
# loader / BlobDetectionProfileStore sources directly, plus the header-only
# nlohmann/json openFrameworks ships in libs/json/include.
#
# Usage (from this directory):
#   make                 # builds ./blob_preset_coverage
#   make test            # core tests + deploy-gate wrapper tests

CXX ?= c++

ifndef OF_ROOT
	OF_ROOT = ../../../../..
endif

SHARED = ../../shared/src
CXXFLAGS = -std=c++17 -Wall -Wextra -I. -I$(SHARED) -I$(SHARED)/video-playback -I$(OF_ROOT)/libs/json/include

CORE_SRC = BlobPresetCoverage.cpp $(SHARED)/video-playback/MediaCatalog.cpp \
	$(SHARED)/video-playback/MediaCatalogJsonLoader.cpp $(SHARED)/BlobDetectionProfileStore.cpp
CORE_HDR = BlobPresetCoverage.h $(SHARED)/BlobDetectionProfileStore.h $(SHARED)/video-playback/MediaCatalog.h

BIN = blob_preset_coverage
TEST_BIN = test/blob_preset_coverage_tests

.PHONY: all test clean

all: $(BIN)

$(BIN): main.cpp $(CORE_SRC) $(CORE_HDR)
	$(CXX) $(CXXFLAGS) main.cpp $(CORE_SRC) -o $(BIN)

$(TEST_BIN): test/blob_preset_coverage_tests.cpp $(CORE_SRC) $(CORE_HDR)
	$(CXX) $(CXXFLAGS) test/blob_preset_coverage_tests.cpp $(CORE_SRC) -o $(TEST_BIN)

test: $(BIN) $(TEST_BIN)
	./$(TEST_BIN)
	./test/deploy_gate_tests.sh

clean:
	rm -f $(BIN) $(TEST_BIN)
