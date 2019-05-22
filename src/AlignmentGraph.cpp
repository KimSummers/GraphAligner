#include <iostream>
#include <limits>
#include <algorithm>
#include <queue>
#include "AlignmentGraph.h"
#include "CommonUtils.h"
#include "ThreadReadAssertion.h"

AlignmentGraph::AlignmentGraph() :
nodeLength(),
nodeLookup(),
nodeIDs(),
inNeighbors(),
nodeSequences(),
ambiguousNodeSequences(),
firstAmbiguous(std::numeric_limits<size_t>::max()),
finalized(false)
{
}

void AlignmentGraph::ReserveNodes(size_t numNodes, size_t numSplitNodes)
{
	nodeSequences.reserve(numSplitNodes);
	ambiguousNodeSequences.reserve(numSplitNodes);
	nodeLookup.reserve(numNodes);
	nodeIDs.reserve(numSplitNodes);
	nodeLength.reserve(numSplitNodes);
	inNeighbors.reserve(numSplitNodes);
	outNeighbors.reserve(numSplitNodes);
	reverse.reserve(numSplitNodes);
	nodeOffset.reserve(numSplitNodes);
}

void AlignmentGraph::AddNode(int nodeId, const std::string& sequence, const std::string& name, bool reverseNode, const std::vector<size_t>& breakpoints)
{
	assert(firstAmbiguous == std::numeric_limits<size_t>::max());
	assert(!finalized);
	//subgraph extraction might produce different subgraphs with common nodes
	//don't add duplicate nodes
	if (nodeLookup.count(nodeId) != 0) return;
	originalNodeSize[nodeId] = sequence.size();
	originalNodeName[nodeId] = name;
	assert(breakpoints.size() >= 2);
	assert(breakpoints[0] == 0);
	assert(breakpoints.back() == sequence.size());
	for (size_t breakpoint = 1; breakpoint < breakpoints.size(); breakpoint++)
	{
		if (breakpoints[breakpoint] == breakpoints[breakpoint-1]) continue;
		assert(breakpoints[breakpoint] > breakpoints[breakpoint-1]);
		for (size_t offset = breakpoints[breakpoint-1]; offset < breakpoints[breakpoint]; offset += SPLIT_NODE_SIZE)
		{
			size_t size = SPLIT_NODE_SIZE;
			if (breakpoints[breakpoint] - offset < size) size = breakpoints[breakpoint] - offset;
			assert(size > 0);
			AddNode(nodeId, offset, sequence.substr(offset, size), reverseNode);
			if (offset > 0)
			{
				assert(outNeighbors.size() >= 2);
				assert(outNeighbors.size() == inNeighbors.size());
				assert(nodeIDs.size() == outNeighbors.size());
				assert(nodeOffset.size() == outNeighbors.size());
				assert(nodeIDs[outNeighbors.size()-2] == nodeIDs[outNeighbors.size()-1]);
				assert(nodeOffset[outNeighbors.size()-2] + nodeLength[outNeighbors.size()-2] == nodeOffset[outNeighbors.size()-1]);
				outNeighbors[outNeighbors.size()-2].push_back(outNeighbors.size()-1);
				inNeighbors[inNeighbors.size()-1].push_back(inNeighbors.size()-2);
			}
		}
	}
}

void AlignmentGraph::AddNode(int nodeId, int offset, const std::string& sequence, bool reverseNode)
{
	assert(firstAmbiguous == std::numeric_limits<size_t>::max());
	assert(!finalized);
	assert(sequence.size() <= SPLIT_NODE_SIZE);

	nodeLookup[nodeId].push_back(nodeLength.size());
	nodeLength.push_back(sequence.size());
	nodeIDs.push_back(nodeId);
	inNeighbors.emplace_back();
	outNeighbors.emplace_back();
	reverse.push_back(reverseNode);
	nodeOffset.push_back(offset);
	NodeChunkSequence normalSeq;
	for (size_t i = 0; i < CHUNKS_IN_NODE; i++)
	{
		normalSeq[i] = 0;
	}
	AmbiguousChunkSequence ambiguousSeq;
	ambiguousSeq.A = 0;
	ambiguousSeq.C = 0;
	ambiguousSeq.G = 0;
	ambiguousSeq.T = 0;
	bool ambiguous = false;
	assert(sequence.size() <= sizeof(size_t)*8);
	for (size_t i = 0; i < sequence.size(); i++)
	{
		size_t chunk = i / BP_IN_CHUNK;
		assert(chunk < CHUNKS_IN_NODE);
		size_t offset = (i % BP_IN_CHUNK) * 2;
		switch(sequence[i])
		{
			case 'a':
			case 'A':
				ambiguousSeq.A |= ((size_t)1) << (i);
				normalSeq[chunk] |= ((size_t)0) << offset;
				break;
			case 'c':
			case 'C':
				ambiguousSeq.C |= ((size_t)1) << (i);
				normalSeq[chunk] |= ((size_t)1) << offset;
				break;
			case 'g':
			case 'G':
				ambiguousSeq.G |= ((size_t)1) << (i);
				normalSeq[chunk] |= ((size_t)2) << offset;
				break;
			case 't':
			case 'T':
			case 'u':
			case 'U':
				ambiguousSeq.T |= ((size_t)1) << (i);
				normalSeq[chunk] |= ((size_t)3) << offset;
				break;
			case 'r':
			case 'R':
				ambiguousSeq.A |= ((size_t)1) << (i);
				ambiguousSeq.G |= ((size_t)1) << (i);
				ambiguous = true;
				break;
			case 'y':
			case 'Y':
				ambiguousSeq.C |= ((size_t)1) << (i);
				ambiguousSeq.T |= ((size_t)1) << (i);
				ambiguous = true;
				break;
			case 's':
			case 'S':
				ambiguousSeq.G |= ((size_t)1) << (i);
				ambiguousSeq.C |= ((size_t)1) << (i);
				ambiguous = true;
				break;
			case 'w':
			case 'W':
				ambiguousSeq.A |= ((size_t)1) << (i);
				ambiguousSeq.T |= ((size_t)1) << (i);
				ambiguous = true;
				break;
			case 'k':
			case 'K':
				ambiguousSeq.G |= ((size_t)1) << (i);
				ambiguousSeq.T |= ((size_t)1) << (i);
				ambiguous = true;
				break;
			case 'm':
			case 'M':
				ambiguousSeq.A |= ((size_t)1) << (i);
				ambiguousSeq.C |= ((size_t)1) << (i);
				ambiguous = true;
				break;
			case 'b':
			case 'B':
				ambiguousSeq.C |= ((size_t)1) << (i);
				ambiguousSeq.G |= ((size_t)1) << (i);
				ambiguousSeq.T |= ((size_t)1) << (i);
				ambiguous = true;
				break;
			case 'd':
			case 'D':
				ambiguousSeq.A |= ((size_t)1) << (i);
				ambiguousSeq.G |= ((size_t)1) << (i);
				ambiguousSeq.T |= ((size_t)1) << (i);
				ambiguous = true;
				break;
			case 'h':
			case 'H':
				ambiguousSeq.A |= ((size_t)1) << (i);
				ambiguousSeq.C |= ((size_t)1) << (i);
				ambiguousSeq.T |= ((size_t)1) << (i);
				ambiguous = true;
				break;
			case 'v':
			case 'V':
				ambiguousSeq.A |= ((size_t)1) << (i);
				ambiguousSeq.C |= ((size_t)1) << (i);
				ambiguousSeq.G |= ((size_t)1) << (i);
				ambiguous = true;
				break;
			case 'n':
			case 'N':
				ambiguousSeq.A |= ((size_t)1) << (i);
				ambiguousSeq.C |= ((size_t)1) << (i);
				ambiguousSeq.G |= ((size_t)1) << (i);
				ambiguousSeq.T |= ((size_t)1) << (i);
				ambiguous = true;
				break;
			default:
				assert(false);
		}
	}
	ambiguousNodes.push_back(ambiguous);
	if (ambiguous)
	{
		ambiguousNodeSequences.emplace_back(ambiguousSeq);
	}
	else
	{
		nodeSequences.emplace_back(normalSeq);
	}
	assert(nodeIDs.size() == nodeLength.size());
	assert(nodeLength.size() == inNeighbors.size());
	assert(inNeighbors.size() == outNeighbors.size());
}

void AlignmentGraph::AddEdgeNodeId(int node_id_from, int node_id_to, size_t startOffset)
{
	assert(firstAmbiguous == std::numeric_limits<size_t>::max());
	assert(!finalized);
	assert(nodeLookup.count(node_id_from) > 0);
	assert(nodeLookup.count(node_id_to) > 0);
	size_t from = nodeLookup.at(node_id_from).back();
	size_t to = std::numeric_limits<size_t>::max();
	assert(nodeOffset[from] + nodeLength[from] == originalNodeSize[node_id_from]);
	for (auto node : nodeLookup[node_id_to])
	{
		if (nodeOffset[node] == startOffset)
		{
			to = node;
		}
	}
	assert(to != std::numeric_limits<size_t>::max());
	//don't add double edges
	if (std::find(inNeighbors[to].begin(), inNeighbors[to].end(), from) == inNeighbors[to].end()) inNeighbors[to].push_back(from);
	if (std::find(outNeighbors[from].begin(), outNeighbors[from].end(), to) == outNeighbors[from].end()) outNeighbors[from].push_back(to);
}

void AlignmentGraph::Finalize(int wordSize, bool doComponents)
{
	assert(nodeSequences.size() + ambiguousNodeSequences.size() == nodeLength.size());
	assert(inNeighbors.size() == nodeLength.size());
	assert(outNeighbors.size() == nodeLength.size());
	assert(reverse.size() == nodeLength.size());
	assert(nodeIDs.size() == nodeLength.size());
	RenumberAmbiguousToEnd();
	ambiguousNodes.clear();
	findLinearizable();
	std::cout << nodeLookup.size() << " original nodes" << std::endl;
	std::cout << nodeLength.size() << " split nodes" << std::endl;
	std::cout << ambiguousNodeSequences.size() << " ambiguous split nodes" << std::endl;
	finalized = true;
	int specialNodes = 0;
	size_t edges = 0;
	for (size_t i = 0; i < inNeighbors.size(); i++)
	{
		inNeighbors[i].shrink_to_fit();
		outNeighbors[i].shrink_to_fit();
		if (inNeighbors[i].size() >= 2) specialNodes++;
		edges += inNeighbors[i].size();
	}
	std::cout << edges << " edges" << std::endl;
	std::cout << specialNodes << " nodes with in-degree >= 2" << std::endl;
	assert(nodeSequences.size() + ambiguousNodeSequences.size() == nodeLength.size());
	assert(inNeighbors.size() == nodeLength.size());
	assert(outNeighbors.size() == nodeLength.size());
	assert(reverse.size() == nodeLength.size());
	assert(nodeIDs.size() == nodeLength.size());
	assert(nodeOffset.size() == nodeLength.size());
	nodeLength.shrink_to_fit();
	nodeIDs.shrink_to_fit();
	inNeighbors.shrink_to_fit();
	outNeighbors.shrink_to_fit();
	reverse.shrink_to_fit();
	nodeSequences.shrink_to_fit();
	ambiguousNodeSequences.shrink_to_fit();
	if (doComponents)
	{
		std::cout << "use component ordering" << std::endl;
		doComponentOrder();
	}
#ifndef NDEBUG
	for (auto pair : nodeLookup)
	{
		for (size_t i = 1; i < pair.second.size(); i++)
		{
			assert(nodeOffset[pair.second[i-1]] < nodeOffset[pair.second[i]]);
		}
	}
#endif
}

void AlignmentGraph::findLinearizable()
{
	linearizable.resize(nodeLength.size(), false);
	std::vector<bool> checked;
	checked.resize(nodeLength.size(), false);
	std::vector<size_t> stack;
	std::vector<bool> onStack;
	onStack.resize(nodeLength.size(), false);
	for (size_t node = 0; node < nodeLength.size(); node++)
	{
		if (checked[node]) continue;
		if (inNeighbors[node].size() != 1)
		{
			checked[node] = true;
			continue;
		}
		checked[node] = true;
		assert(inNeighbors[node].size() == 1);
		assert(stack.size() == 0);
		stack.push_back(node);
		onStack[node] = true;
		while (true)
		{
			assert(stack.size() <= nodeLength.size());
			if (inNeighbors[stack.back()].size() != 1)
			{
				for (size_t i = 0; i < stack.size()-1; i++)
				{
					assert(inNeighbors[stack[i]].size() == 1);
					checked[stack[i]] = true;
					linearizable[stack[i]] = true;
					onStack[stack[i]] = false;
				}
				linearizable[stack.back()] = false;
				checked[stack.back()] = true;
				onStack[stack.back()] = false;
				stack.clear();
				break;
			}
			assert(inNeighbors[stack.back()].size() == 1);
			if (checked[stack.back()])
			{
				for (size_t i = 0; i < stack.size()-1; i++)
				{
					assert(inNeighbors[stack[i]].size() == 1);
					checked[stack[i]] = true;
					linearizable[stack[i]] = true;
					onStack[stack[i]] = false;
				}
				linearizable[stack.back()] = false;
				checked[stack.back()] = true;
				onStack[stack.back()] = false;
				stack.clear();
				break;
			}
			assert(inNeighbors[stack.back()].size() == 1);
			auto neighbor = inNeighbors[stack.back()][0];
			if (neighbor == node)
			{
				for (size_t i = 0; i < stack.size(); i++)
				{
					checked[stack[i]] = true;
					linearizable[stack[i]] = false;
					onStack[stack[i]] = false;
				}
				stack.clear();
				break;
			}
			if (onStack[neighbor])
			{
				assert(neighbor != node);
				size_t i = stack.size();
				for (; i > 0; i--)
				{
					if (stack[i] == neighbor) break;
					checked[stack[i]] = true;
					linearizable[stack[i]] = false;
					onStack[stack[i]] = false;
				}
				for (size_t j = 0; j < i; j++)
				{
					checked[stack[j]] = true;
					linearizable[stack[j]] = true;
					onStack[stack[j]] = false;
				}
				stack.clear();
				break;
			}
			stack.push_back(inNeighbors[stack.back()][0]);
			onStack[stack.back()] = true;
		}
	}
}

#ifdef NDEBUG
	__attribute__((always_inline))
#endif
size_t AlignmentGraph::NodeLength(size_t index) const
{
	return nodeLength[index];
}

char AlignmentGraph::NodeSequences(size_t node, size_t pos) const
{
	assert(pos < nodeLength[node]);
	if (node < firstAmbiguous)
	{
		assert(node < nodeSequences.size());
		size_t chunk = pos / BP_IN_CHUNK;
		size_t offset = (pos % BP_IN_CHUNK) * 2;
		return "ACGT"[(nodeSequences[node][chunk] >> offset) & 3];
	}
	else
	{
		assert(node >= firstAmbiguous);
		assert(node - firstAmbiguous < ambiguousNodeSequences.size());
		assert(pos < sizeof(size_t) * 8);
		bool A = (ambiguousNodeSequences[node - firstAmbiguous].A >> pos) & 1;
		bool C = (ambiguousNodeSequences[node - firstAmbiguous].C >> pos) & 1;
		bool G = (ambiguousNodeSequences[node - firstAmbiguous].G >> pos) & 1;
		bool T = (ambiguousNodeSequences[node - firstAmbiguous].T >> pos) & 1;
		assert(A + C + G + T >= 1);
		assert(A + C + G + T <= 4);
		if ( A && !C && !G && !T) return 'A';
		if (!A &&  C && !G && !T) return 'C';
		if (!A && !C &&  G && !T) return 'G';
		if (!A && !C && !G &&  T) return 'T';
		if ( A && !C &&  G && !T) return 'R';
		if (!A &&  C && !G &&  T) return 'Y';
		if (!A &&  C &&  G && !T) return 'S';
		if ( A && !C && !G &&  T) return 'W';
		if (!A && !C &&  G &&  T) return 'K';
		if ( A &&  C && !G && !T) return 'M';
		if (!A &&  C &&  G &&  T) return 'B';
		if ( A && !C &&  G &&  T) return 'D';
		if ( A &&  C && !G &&  T) return 'H';
		if ( A &&  C &&  G && !T) return 'V';
		if ( A &&  C &&  G &&  T) return 'N';
		assert(false);
		return 'N';
	}
}

#ifdef NDEBUG
	__attribute__((always_inline))
#endif
AlignmentGraph::NodeChunkSequence AlignmentGraph::NodeChunks(size_t index) const
{
	assert(index < nodeSequences.size());
	return nodeSequences[index];
}

#ifdef NDEBUG
	__attribute__((always_inline))
#endif
AlignmentGraph::AmbiguousChunkSequence AlignmentGraph::AmbiguousNodeChunks(size_t index) const
{
	assert(index >= firstAmbiguous);
	assert(index - firstAmbiguous < ambiguousNodeSequences.size());
	return ambiguousNodeSequences[index - firstAmbiguous];
}

size_t AlignmentGraph::NodeSize() const
{
	return nodeLength.size();
}

class NodeWithDistance
{
public:
	NodeWithDistance(size_t node, bool start, size_t distance) : node(node), start(start), distance(distance) {};
	bool operator>(const NodeWithDistance& other) const
	{
		return distance > other.distance;
	}
	size_t node;
	bool start;
	size_t distance;
};

size_t AlignmentGraph::GetUnitigNode(int nodeId, size_t offset) const
{
	const auto& nodes = nodeLookup.at(nodeId);
	assert(nodes.size() > 0);
	//guess the index
	size_t index = nodes.size() * ((double)offset / (double)originalNodeSize.at(nodeId));
	if (index >= nodes.size()) index = nodes.size()-1;
	//go to the exact index
	while (index < nodes.size()-1 && (nodeOffset[nodes[index]] + NodeLength(nodes[index]) <= offset)) index++;
	while (index > 0 && (nodeOffset[nodes[index]] > offset)) index--;
	assert(index != nodes.size());
	size_t result = nodes[index];
	assert(nodeOffset[result] <= offset);
	assert(nodeOffset[result] + NodeLength(result) > offset);
	return result;
}

std::pair<int, size_t> AlignmentGraph::GetReversePosition(int nodeId, size_t offset) const
{
	assert(nodeLookup.count(nodeId) == 1);
	const auto& nodes = nodeLookup.at(nodeId);
	size_t originalSize = originalNodeSize.at(nodeId);
	assert(offset < originalSize);
	size_t newOffset = originalSize - offset - 1;
	assert(newOffset < originalSize);
	int reverseNodeId;
	if (nodeId % 2 == 0)
	{
		reverseNodeId = (nodeId / 2) * 2 + 1;
	}
	else
	{
		reverseNodeId = (nodeId / 2) * 2;
	}
	return std::make_pair(reverseNodeId, newOffset);
}

AlignmentGraph::MatrixPosition::MatrixPosition(size_t node, size_t nodeOffset, size_t seqPos) :
	node(node),
	nodeOffset(nodeOffset),
	seqPos(seqPos)
{
}

bool AlignmentGraph::MatrixPosition::operator==(const AlignmentGraph::MatrixPosition& other) const
{
	return node == other.node && nodeOffset == other.nodeOffset && seqPos == other.seqPos;
}

bool AlignmentGraph::MatrixPosition::operator!=(const AlignmentGraph::MatrixPosition& other) const
{
	return !(*this == other);
}

std::string AlignmentGraph::OriginalNodeName(int nodeId) const
{
	auto found = originalNodeName.find(nodeId);
	if (found == originalNodeName.end()) return "";
	return found->second;
}

std::vector<size_t> renumber(const std::vector<size_t>& vec, const std::vector<size_t>& renumbering)
{
	std::vector<size_t> result;
	result.reserve(vec.size());
	for (size_t i = 0; i < vec.size(); i++)
	{
		assert(vec[i] < renumbering.size());
		result.push_back(renumbering[vec[i]]);
	}
	return result;
}

template <typename T>
std::vector<T> reorder(const std::vector<T>& vec, const std::vector<size_t>& renumbering)
{
	assert(vec.size() == renumbering.size());
	std::vector<T> result;
	result.resize(vec.size());
	for (size_t i = 0; i < vec.size(); i++)
	{
		result[renumbering[i]] = vec[i];
	}
	return result;
}

void AlignmentGraph::RenumberAmbiguousToEnd()
{
	assert(nodeSequences.size() + ambiguousNodeSequences.size() == nodeLength.size());
	assert(inNeighbors.size() == nodeLength.size());
	assert(outNeighbors.size() == nodeLength.size());
	assert(reverse.size() == nodeLength.size());
	assert(nodeIDs.size() == nodeLength.size());
	assert(ambiguousNodes.size() == nodeLength.size());
	assert(firstAmbiguous == std::numeric_limits<size_t>::max());
	assert(!finalized);
	std::vector<size_t> renumbering;
	renumbering.reserve(ambiguousNodes.size());
	size_t nonAmbiguousCount = 0;
	size_t ambiguousCount = 0;
	for (size_t i = 0; i < ambiguousNodes.size(); i++)
	{
		if (!ambiguousNodes[i])
		{
			renumbering.push_back(nonAmbiguousCount);
			nonAmbiguousCount++;
		}
		else
		{
			assert(ambiguousCount < ambiguousNodes.size());
			assert(ambiguousNodes.size()-1-ambiguousCount >= nonAmbiguousCount);
			renumbering.push_back(ambiguousNodes.size()-1-ambiguousCount);
			ambiguousCount++;
		}
	}
	assert(renumbering.size() == ambiguousNodes.size());
	assert(nonAmbiguousCount + ambiguousCount == ambiguousNodes.size());
	assert(ambiguousCount == ambiguousNodeSequences.size());
	assert(nonAmbiguousCount == nodeSequences.size());
	firstAmbiguous = nonAmbiguousCount;

	if (ambiguousCount == 0) return;

	//the ambiguous nodes were added in the reverse order, reverse the sequence containers too
	std::reverse(ambiguousNodeSequences.begin(), ambiguousNodeSequences.end());

	nodeLength = reorder(nodeLength, renumbering);
	nodeOffset = reorder(nodeOffset, renumbering);
	nodeIDs = reorder(nodeIDs, renumbering);
	inNeighbors = reorder(inNeighbors, renumbering);
	outNeighbors = reorder(outNeighbors, renumbering);
	reverse = reorder(reverse, renumbering);
	for (auto& pair : nodeLookup)
	{
		pair.second = renumber(pair.second, renumbering);
	}
	assert(inNeighbors.size() == outNeighbors.size());
	for (size_t i = 0; i < inNeighbors.size(); i++)
	{
		inNeighbors[i] = renumber(inNeighbors[i], renumbering);
		outNeighbors[i] = renumber(outNeighbors[i], renumbering);
	}

#ifndef NDEBUG
	assert(inNeighbors.size() == outNeighbors.size());
	for (size_t i = 0; i < inNeighbors.size(); i++)
	{
		for (auto neighbor : inNeighbors[i])
		{
			assert(std::find(outNeighbors[neighbor].begin(), outNeighbors[neighbor].end(), i) != outNeighbors[neighbor].end());
		}
		for (auto neighbor : outNeighbors[i])
		{
			assert(std::find(inNeighbors[neighbor].begin(), inNeighbors[neighbor].end(), i) != inNeighbors[neighbor].end());
		}
	}
	for (auto pair : nodeLookup)
	{
		size_t foundSize = 0;
		std::set<size_t> offsets;
		size_t lastOffset = 0;
		for (auto node : pair.second)
		{
			assert(offsets.count(nodeOffset[node]) == 0);
			assert(offsets.size() == 0 || nodeOffset[node] > lastOffset);
			lastOffset = nodeOffset[node];
			offsets.insert(nodeOffset[node]);
			assert(nodeIDs[node] == pair.first);
			foundSize += nodeLength[node];
		}
		assert(foundSize == originalNodeSize[pair.first]);
	}
#endif
}

void AlignmentGraph::doComponentOrder()
{
	std::vector<std::tuple<size_t, int, size_t>> callStack;
	size_t i = 0;
	std::vector<size_t> index;
	std::vector<size_t> lowlink;
	std::vector<bool> onStack;
	std::vector<size_t> stack;
	index.resize(nodeLength.size(), std::numeric_limits<size_t>::max());
	lowlink.resize(nodeLength.size(), std::numeric_limits<size_t>::max());
	onStack.resize(nodeLength.size(), false);
	size_t checknode = 0;
	size_t nextComponent = 0;
	componentNumber.resize(nodeLength.size(), std::numeric_limits<size_t>::max());
	while (true)
	{
		if (callStack.size() == 0)
		{
			while (checknode < nodeLength.size() && index[checknode] != std::numeric_limits<size_t>::max())
			{
				checknode++;
			}
			if (checknode == nodeLength.size()) break;
			callStack.emplace_back(checknode, 0, 0);
			checknode++;
		}
		auto top = callStack.back();
		const size_t v = std::get<0>(top);
		int state = std::get<1>(top);
		size_t w;
		size_t neighborI = std::get<2>(top);
		callStack.pop_back();
		switch(state)
		{
			case 0:
				assert(index[v] == std::numeric_limits<size_t>::max());
				assert(lowlink[v] == std::numeric_limits<size_t>::max());
				assert(!onStack[v]);
				index[v] = i;
				lowlink[v] = i;
				i += 1;
				stack.push_back(v);
				onStack[v] = true;
				[[fallthrough]];
			startloop:
			case 1:
				if (neighborI >= outNeighbors[v].size()) goto endloop;
				assert(neighborI < outNeighbors[v].size());
				w = outNeighbors[v][neighborI];
				if (index[w] == std::numeric_limits<size_t>::max())
				{
					assert(lowlink[w] == std::numeric_limits<size_t>::max());
					assert(!onStack[w]);
					callStack.emplace_back(v, 2, neighborI);
					callStack.emplace_back(w, 0, 0);
					continue;
				}
				else if (onStack[w])
				{
					lowlink[v] = std::min(lowlink[v], index[w]);
					neighborI += 1;
					goto startloop;
				}
				else
				{
					neighborI += 1;
					goto startloop;
				}
			case 2:
				assert(neighborI < outNeighbors[v].size());
				w = outNeighbors[v][neighborI];
				assert(index[w] != std::numeric_limits<size_t>::max());
				assert(lowlink[w] != std::numeric_limits<size_t>::max());
				lowlink[v] = std::min(lowlink[v], lowlink[w]);
				neighborI++;
				goto startloop;
			endloop:
			case 3:
				if (lowlink[v] == index[v])
				{
					do
					{
						w = stack.back();
						stack.pop_back();
						onStack[w] = false;
						componentNumber[w] = nextComponent;
					} while (w != v);
					nextComponent++;
				}
		}
	}
	assert(stack.size() == 0);
	for (size_t i = 0; i < componentNumber.size(); i++)
	{
		assert(componentNumber[i] != std::numeric_limits<size_t>::max());
		assert(componentNumber[i] <= nextComponent-1);
		componentNumber[i] = nextComponent-1-componentNumber[i];
	}
#ifdef EXTRACORRECTNESSASSERTIONS
	for (size_t i = 0; i < nodeLength.size(); i++)
	{
		for (auto neighbor : outNeighbors[i])
		{
			assert(componentNumber[neighbor] >= componentNumber[i]);
		}
	}
#endif
}

size_t AlignmentGraph::ComponentSize() const
{
	return componentNumber.size();
}
