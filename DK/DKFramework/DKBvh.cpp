//
//  File: DKBvh.cpp
//  Author: Hongtae Kim (tiff2766@gmail.com)
//
//  Copyright (c) 2015 Hongtae Kim. All rights reserved.
//

#include <algorithm>
#include <cstdlib>
#include "DKMath.h"
#include "DKBvh.h"

using namespace DKFoundation;
using namespace DKFramework;

#define MAX_NODE_COUNT (0x7fffffff >> 1)

DKBvh::DKBvh(void) : volume(NULL)
{
}

DKBvh::~DKBvh(void)
{
}

void DKBvh::Build(VolumeInterface* vi)
{
	this->volume = vi;
	BuildInternal();
}

void DKBvh::Rebuild(void)
{
	BuildInternal();
}

void DKBvh::BuildInternal(void)
{
	if (this->volume)
	{
		this->volume->Lock();
		nodes.Clear();

		DKArray<QuantizedAabbNode> quantizedLeafNodes;

		// Query all leaf-nodes (all triangles)
		int numTriangles = this->volume->NumberOfObjects();
		if (numTriangles > 0)
		{
			struct LeafNode
			{
				DKAabb aabb;
				int triangleIndex;
			};


			DKAabb aabb;
			aabb.positionMin = DKVector3(FLT_MAX, FLT_MAX, FLT_MAX);
			aabb.positionMax = DKVector3(-FLT_MAX, -FLT_MAX, -FLT_MAX);

			DKArray<LeafNode> leafNodes;
			leafNodes.Reserve(numTriangles);
			for (int i = 0; i < numTriangles; ++i)
			{
				DKAabb box = this->volume->AabbForObjectAtIndex(i);
				if (box.IsValid())
				{
					LeafNode info = { box, i };
					leafNodes.Add(info);

					// Update AABB
					for (int k = 0; k < 3; ++k)
					{
						if (aabb.positionMin.val[k] > info.aabb.positionMin.val[k])
							aabb.positionMin.val[k] = info.aabb.positionMin.val[k];
						if (aabb.positionMax.val[k] < info.aabb.positionMax.val[k])
							aabb.positionMax.val[k] = info.aabb.positionMax.val[k];
					}
				}
			}

			DKVector3 scale = aabb.positionMax - aabb.positionMin;

			this->aabbScale.x = Max(scale.x, 0.00001);
			this->aabbScale.y = Max(scale.y, 0.00001);
			this->aabbScale.z = Max(scale.z, 0.00001);
			this->aabbOffset = aabb.positionMin;

			quantizedLeafNodes.Reserve(leafNodes.Count());
			for (LeafNode& n : leafNodes)
			{
				DKVector3 aabbMin = (n.aabb.positionMin - this->aabbOffset) / this->aabbScale * float(0xffff);
				DKVector3 aabbMax = (n.aabb.positionMax - this->aabbOffset) / this->aabbScale * float(0xffff);

				QuantizedAabbNode node;
				node.aabbMin[0] = aabbMin.val[0];
				node.aabbMin[1] = aabbMin.val[1];
				node.aabbMin[2] = aabbMin.val[2];
				node.aabbMax[0] = aabbMax.val[0];
				node.aabbMax[1] = aabbMax.val[1];
				node.aabbMax[2] = aabbMax.val[2];
				node.triangleIndex = n.triangleIndex;
				quantizedLeafNodes.Add(node);
			}
		}

		if (quantizedLeafNodes.Count() > 0 && quantizedLeafNodes.Count() < MAX_NODE_COUNT)
		{
			nodes.Reserve(quantizedLeafNodes.Count() * 2);
			BuildTree(quantizedLeafNodes, quantizedLeafNodes.Count());
		}

		this->volume->Unlock();
	}
	else
		nodes.Clear();
}

DKAabb DKBvh::Aabb(void) const
{
	if (volume)
	{
		return DKAabb(aabbOffset, aabbOffset + aabbScale);
	}
	return DKAabb();
}

#define BVH_PARTITION_FULL_SORT	1

void DKBvh::BuildTree(QuantizedAabbNode* leafNodes, int count)
{
	DKASSERT_DEBUG(leafNodes);
	DKASSERT_DEBUG(count > 0);

	if (count == 1)	// leaf-node
	{
		this->nodes.Add(leafNodes[0]);
		return;
	}

	using VectorI64 = int64_t[3];
	using VectorI32 = int32_t[3];
	using VectorI16 = int16_t[3];

	// calculate means
	VectorI32 means;
	if (1)
	{
		VectorI64 tmp = { 0, 0, 0 };
		for (int i = 0; i < count; ++i)
		{
			QuantizedAabbNode& node = leafNodes[i];
			tmp[0] += node.aabbMin[0] + node.aabbMax[0];
			tmp[1] += node.aabbMin[1] + node.aabbMax[1];
			tmp[2] += node.aabbMin[2] + node.aabbMax[2];
		}
		int c = count << 1;
		means[0] = tmp[0] / c;
		means[1] = tmp[1] / c;
		means[2] = tmp[2] / c;
	}

	// calculate axis (0: x-axis, 1: y-axis, 2: z-axis, DKVector3::val order)
	int splitAxis = 0;
	if (1)
	{
		VectorI32 center, variance = { 0, 0, 0 };
		for (int i = 0; i < count; ++i)
		{
			QuantizedAabbNode& node = leafNodes[i];
			center[0] = (node.aabbMin[0] + node.aabbMax[0]) >> 1;
			center[1] = (node.aabbMin[1] + node.aabbMax[1]) >> 1;
			center[2] = (node.aabbMin[2] + node.aabbMax[2]) >> 1;
			variance[0] += std::abs(center[0] - means[0]);
			variance[1] += std::abs(center[1] - means[1]);
			variance[2] += std::abs(center[2] - means[2]);
		}
		splitAxis = (variance[0] < variance[1] ? (variance[1] < variance[2] ? 2 : 1) : (variance[0] < variance[2] ? 2 : 0));
	}

	// partitioning
	int splitValue = means[splitAxis];
	int splitIndex = 0;

#if BVH_PARTITION_FULL_SORT
	// soft leaf-nodes, place larger value front.
	std::sort(leafNodes, leafNodes + count,
		[splitAxis](const QuantizedAabbNode& a, const QuantizedAabbNode& b)->bool
	{
		return (a.aabbMax[splitAxis] + a.aabbMin[splitAxis]) > (b.aabbMax[splitAxis] + b.aabbMin[splitAxis]);
	});
	splitIndex = count / 2;
#else
	// sort leaf-nodes, place larger than splitValue to left.
	for (int i = 0; i < count; ++i)
	{
		QuantizedAabbNode& node = leafNodes[i];
		if (node.aabbMax[splitAxis] + node.aabbMin[splitAxis] > splitValue)
		{
			QuantizedAabbNode tmp = leafNodes[i];
			leafNodes[i] = leafNodes[splitIndex];
			leafNodes[splitIndex] = tmp;
			splitIndex++;
		}
	}
	int balancedRangeMin = count / 3;
	int balancedRangeMax = count - balancedRangeMin - 1;

	if (splitIndex <= balancedRangeMin || splitIndex >= balancedRangeMax)
	{
		splitIndex = count / 2;
	}
	DKASSERT_DEBUG(splitIndex < count);
#endif
	// recursion
	int currentNodeIndex = nodes.Add(QuantizedAabbNode());

	// build left sub tree
	int leftChildNodeIndex = nodes.Count();
	BuildTree(leafNodes, splitIndex);

	// build right sub tree
	int rightChildNodeIndex = nodes.Count();
	BuildTree(&leafNodes[splitIndex], count - splitIndex);

	QuantizedAabbNode& node = nodes.Value(currentNodeIndex);
	QuantizedAabbNode& left = nodes.Value(leftChildNodeIndex);
	QuantizedAabbNode& right = nodes.Value(rightChildNodeIndex);
	for (int i = 0; i < 3; ++i)
	{
		node.aabbMin[i] = Min(left.aabbMin[i], right.aabbMin[i]);
		node.aabbMax[i] = Max(left.aabbMax[i], right.aabbMax[i]);
	}
	node.negativeTreeSize = currentNodeIndex - static_cast<int>(nodes.Count());
}

bool DKBvh::RayTest(const DKLine& ray, RayCastResultCallback* cb) const
{
	if (this->volume)
	{
		DKAabb bvhAabb = this->Aabb();
		DKVector3 p1, p2;
		if (bvhAabb.RayTest(ray, &p1) && bvhAabb.RayTest(DKLine(ray.end, ray.begin), &p2))
		{
			DKVector3 offset = this->aabbOffset;
			DKVector3 scale = this->aabbScale;

			unsigned short rayAabbMin[3];
			unsigned short rayAabbMax[3];

			DKAabb rayOverlapAabb;
			rayOverlapAabb.Expand(p1);
			rayOverlapAabb.Expand(p2);
			for (int i = 0; i < 3; ++i)
			{
				rayAabbMin[i] = (rayOverlapAabb.positionMin.val[i] - offset.val[i]) / scale.val[i] * float(0xffff);
				rayAabbMax[i] = (rayOverlapAabb.positionMax.val[i] - offset.val[i]) / scale.val[i] * float(0xffff);
			}

			auto isQuantizedAabbOverlapped = [](const unsigned short* min1, const unsigned short* max1, const unsigned short* min2, const unsigned short* max2)
			{
				if (min1[0] > max2[0] || max1[0] < min2[0] ||
					min1[1] > max2[1] || max1[1] < min2[1] ||
					min1[2] > max2[2] || max1[2] < min2[2])
					return false;
				return true;
			};

			int currentNodeIndex = 0;
			int nodeCount = this->nodes.Count();
			bool isLeafNode = false;
			bool isOverlapped = false;
			DKAabb aabb;

			while (currentNodeIndex < nodeCount)
			{
				const QuantizedAabbNode& node = nodes.Value(currentNodeIndex);
				isOverlapped = isQuantizedAabbOverlapped(rayAabbMin, rayAabbMax, node.aabbMin, node.aabbMax);
				isLeafNode = node.triangleIndex >= 0;

				if (isLeafNode)
				{
					if (isOverlapped)
					{
						// un-quantize
						aabb.positionMin.val[0] = (float(node.aabbMin[0]) / float(0xffff) * scale.val[0]) + offset.val[0];
						aabb.positionMin.val[1] = (float(node.aabbMin[1]) / float(0xffff) * scale.val[1]) + offset.val[1];
						aabb.positionMin.val[2] = (float(node.aabbMin[2]) / float(0xffff) * scale.val[2]) + offset.val[2];
						aabb.positionMax.val[0] = (float(node.aabbMax[0]) / float(0xffff) * scale.val[0]) + offset.val[0];
						aabb.positionMax.val[1] = (float(node.aabbMax[1]) / float(0xffff) * scale.val[1]) + offset.val[1];
						aabb.positionMax.val[2] = (float(node.aabbMax[2]) / float(0xffff) * scale.val[2]) + offset.val[2];

						if (aabb.RayTest(ray))
						{
							if (cb == NULL || !cb->Invoke(node.triangleIndex, ray))
								return true;
						}
					}
					currentNodeIndex++;
				}
				else
				{
					if (isOverlapped)
						currentNodeIndex++;
					else
						currentNodeIndex -= node.negativeTreeSize;
				}
			}
		}
	}
	return false;
}
