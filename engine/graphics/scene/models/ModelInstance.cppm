module;

#include <type_traits>

export module Graphics.Scene.Models.ModelInstance;

export import Graphics.Scene.Models.Skeleton;

namespace Graphics
{

export struct ModelInstance final
{
	SkeletonHandle skeleton{};
	RenderTransform transform{};

	bool Get_Bone_Transform(const SkeletonPool &skeletons, BoneHandle bone, RenderTransform &result) const noexcept
	{
		const Skeleton *resource = skeletons.Resolve(skeleton);
		if (resource == nullptr || !resource->Rest_Transform(bone, result))
			return false;
		result = Multiply(transform, result);
		return true;
	}

	bool Get_Attachment_Transform(const SkeletonPool &skeletons, AttachmentHandle attachment, RenderTransform &result) const noexcept
	{
		const Skeleton *resource = skeletons.Resolve(skeleton);
		if (resource == nullptr || !resource->Attachment_Transform(attachment, result))
			return false;
		result = Multiply(transform, result);
		return true;
	}

private:
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
