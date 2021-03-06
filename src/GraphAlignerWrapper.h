//split this here so modifying GraphAligner.h doesn't require recompiling every cpp file

#ifndef GraphAlignerWrapper_h
#define GraphAlignerWrapper_h

#include <tuple>
#include "vg.pb.h"
#include "GraphAlignerCommon.h"
#include "AlignmentGraph.h"

class SeedHit
{
public:
	SeedHit(int nodeID, size_t nodeOffset, size_t seqPos, size_t matchLen, bool reverse) :
	nodeID(nodeID),
	nodeOffset(nodeOffset),
	seqPos(seqPos),
	matchLen(matchLen),
	reverse(reverse)
	{
	}
	int nodeID;
	size_t nodeOffset;
	size_t seqPos;
	size_t matchLen;
	bool reverse;
};

AlignmentResult AlignOneWay(const AlignmentGraph& graph, const std::string& seq_id, const std::string& sequence, size_t initialBandwidth, size_t rampBandwidth, bool quietMode, GraphAlignerCommon<size_t, int32_t, uint64_t>::AlignerGraphsizedState& reusableState, bool lowMemory, bool forceGlobal, bool preciseClipping);
AlignmentResult AlignOneWay(const AlignmentGraph& graph, const std::string& seq_id, const std::string& sequence, size_t initialBandwidth, size_t rampBandwidth, size_t maxCellsPerSlice, bool quietMode, bool sloppyOptimizations, const std::vector<SeedHit>& seedHits, GraphAlignerCommon<size_t, int32_t, uint64_t>::AlignerGraphsizedState& reusableState, bool lowMemory, bool forceGlobal, bool preciseClipping);

void AddAlignment(const std::string& seq_id, const std::string& sequence, AlignmentResult::AlignmentItem& alignment);
void AddGAFLine(const AlignmentGraph& graph, const std::string& seq_id, const std::string& sequence, AlignmentResult::AlignmentItem& alignment);
void AddCorrected(AlignmentResult::AlignmentItem& alignment);

#endif