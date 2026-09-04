module;

#include <cmath>
#include <cstddef>
#include <type_traits>

export module Graphics.Scene.Models.ModelInstance;

export import Graphics.Scene.Models.Skeleton;
export import Graphics.Scene.Models.Animation;

namespace Graphics
{

export struct ModelInstance final
{
	SkeletonHandle skeleton{};
	RenderTransform transform{};
	AnimationClipHandle animation{};
	float animation_time = 0.0f;
	AnimationPlaybackMode animation_mode = AnimationPlaybackMode::Loop;
	Pose pose{};

	bool Set_Animation(const SkeletonPool &skeletons, const AnimationClipPool &animations,
		AnimationClipHandle animation_handle, AnimationPlaybackMode mode, float time_seconds = 0.0f)
	{
		const Skeleton *skeleton_resource = skeletons.Resolve(skeleton);
		const AnimationClip *clip = animations.Resolve(animation_handle);
		if (skeleton_resource == nullptr || clip == nullptr
			|| clip->Bone_Count() != skeleton_resource->Bone_Count()
			|| !std::isfinite(time_seconds) || !pose.Initialize(skeleton_resource->Bone_Count()))
			return false;

		animation = animation_handle;
		animation_mode = mode;
		animation_time = time_seconds;
		return Evaluate_Animation(skeletons, animations);
	}

	void Clear_Animation() noexcept
	{
		animation = {};
		animation_time = 0.0f;
		animation_mode = AnimationPlaybackMode::Loop;
	}

	bool Set_Animation_Time(const SkeletonPool &skeletons, const AnimationClipPool &animations,
		float time_seconds) noexcept
	{
		if (!std::isfinite(time_seconds) || !animation.Is_Valid() || !pose.Is_Valid())
			return false;
		const float previous_time = animation_time;
		animation_time = time_seconds;
		if (Evaluate_Animation(skeletons, animations))
			return true;
		animation_time = previous_time;
		return false;
	}

	bool Advance_Animation(const SkeletonPool &skeletons, const AnimationClipPool &animations,
		float delta_seconds) noexcept
	{
		if (!std::isfinite(delta_seconds))
			return false;
		const float next_time = animation_time + delta_seconds;
		if (!std::isfinite(next_time) || !animation.Is_Valid() || !pose.Is_Valid())
			return false;
		animation_time = next_time;
		if (Evaluate_Animation(skeletons, animations))
			return true;
		animation_time -= delta_seconds;
		return false;
	}

	bool Set_Animation_Mode(const SkeletonPool &skeletons, const AnimationClipPool &animations,
		AnimationPlaybackMode mode) noexcept
	{
		if (!animation.Is_Valid() || !pose.Is_Valid())
			return false;
		const AnimationPlaybackMode previous_mode = animation_mode;
		animation_mode = mode;
		if (Evaluate_Animation(skeletons, animations))
			return true;
		animation_mode = previous_mode;
		return false;
	}

	bool Get_Bone_Transform(const SkeletonPool &skeletons, BoneHandle bone, RenderTransform &result) const noexcept
	{
		const Skeleton *resource = skeletons.Resolve(skeleton);
		if (resource == nullptr || !resource->Is_Valid_Bone(bone))
			return false;
		if (animation.Is_Valid() && pose.Is_Valid())
			result = pose.World_Transforms()[bone.Get_Index()];
		else if (!resource->Rest_Transform(bone, result))
			return false;
		result = Multiply(transform, result);
		return true;
	}

	bool Get_Attachment_Transform(const SkeletonPool &skeletons, AttachmentHandle attachment, RenderTransform &result) const noexcept
	{
		const Skeleton *resource = skeletons.Resolve(skeleton);
		if (resource == nullptr || !resource->Is_Valid_Attachment(attachment))
			return false;
		const SkeletonAttachment &point = resource->Attachments()[attachment.Get_Index()];
		if (animation.Is_Valid() && pose.Is_Valid()) {
			result = Multiply(pose.World_Transforms()[point.bone.Get_Index()], point.local_transform);
		} else if (!resource->Attachment_Transform(attachment, result)) {
			return false;
		}
		result = Multiply(transform, result);
		return true;
	}

private:
	bool Evaluate_Animation(const SkeletonPool &skeletons, const AnimationClipPool &animations) noexcept
	{
		const Skeleton *skeleton_resource = skeletons.Resolve(skeleton);
		const AnimationClip *clip = animations.Resolve(animation);
		if (skeleton_resource == nullptr || clip == nullptr || !pose.Is_Valid()
			|| clip->Bone_Count() != skeleton_resource->Bone_Count())
			return false;
		return clip->Sample(animation_time, animation_mode, pose.Local_Transforms())
			&& pose.Evaluate(*skeleton_resource, pose.Local_Transforms());
	}

	static RenderTransform Multiply(const RenderTransform &left, const RenderTransform &right) noexcept
	{
		RenderTransform result;
		for (std::size_t row = 0; row < 4; ++row) {
			for (std::size_t column = 0; column < 4; ++column) {
				float value = 0.0f;
				for (std::size_t element = 0; element < 4; ++element)
					value += left.matrix[row * 4 + element] * right.matrix[element * 4 + column];
				result.matrix[row * 4 + column] = value;
			}
		}
		return result;
	}
};

static_assert(std::is_nothrow_move_constructible_v<ModelInstance>);
static_assert(std::is_nothrow_move_assignable_v<ModelInstance>);

}
