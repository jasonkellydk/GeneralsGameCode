module;

#define BOOST_TEST_MODULE GraphicsModelInstanceTests

#include <boost/test/included/unit_test.hpp>

#include <array>

export module Graphics.Scene.Models.ModelInstance.Tests;

import Graphics.Scene.Models.ModelInstance;

using namespace Graphics;

static RenderTransform Translation(float x, float y, float z) noexcept
{
	RenderTransform transform;
	transform.matrix[0] = 1.0f;
	transform.matrix[5] = 1.0f;
	transform.matrix[10] = 1.0f;
	transform.matrix[15] = 1.0f;
	transform.matrix[3] = x;
	transform.matrix[7] = y;
	transform.matrix[11] = z;
	return transform;
}

BOOST_AUTO_TEST_CASE(model_instance_composes_model_transform_with_bone)
{
	std::array<SkeletonBone, 1> bones{
		SkeletonBone{Invalid_Bone_Index, Translation(1.0f, 2.0f, 3.0f)}
	};
	SkeletonPool skeletons;
	const SkeletonHandle skeleton = Create_Skeleton(skeletons, {bones, {}});
	BOOST_REQUIRE(skeleton.Is_Valid());

	ModelInstance instance;
	instance.skeleton = skeleton;
	instance.transform = Translation(10.0f, 20.0f, 30.0f);
	RenderTransform result;
	BOOST_REQUIRE(instance.Get_Bone_Transform(skeletons, BoneHandle(0, 1), result));
	BOOST_CHECK(result.matrix[3] == 11.0f);
	BOOST_CHECK(result.matrix[7] == 22.0f);
	BOOST_CHECK(result.matrix[11] == 33.0f);
}

BOOST_AUTO_TEST_CASE(model_instance_rejects_invalid_resources_and_attachment_handles)
{
	SkeletonPool skeletons;
	ModelInstance instance;
	RenderTransform result;
	BOOST_CHECK(!instance.Get_Bone_Transform(skeletons, BoneHandle(0, 1), result));

	std::array<SkeletonBone, 1> bones{
		SkeletonBone{Invalid_Bone_Index, Translation(0.0f, 0.0f, 0.0f)}
	};
	instance.skeleton = Create_Skeleton(skeletons, {bones, {}});
	BOOST_REQUIRE(instance.skeleton.Is_Valid());
	BOOST_CHECK(!instance.Get_Attachment_Transform(skeletons, AttachmentHandle(0, 1), result));
	BOOST_CHECK(!instance.Get_Bone_Transform(skeletons, BoneHandle(0, 2), result));
}
