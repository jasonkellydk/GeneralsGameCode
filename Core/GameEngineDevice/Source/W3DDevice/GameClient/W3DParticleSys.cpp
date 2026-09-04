#include "W3DDevice/GameClient/W3DParticleSys.h"
#include "W3DDevice/GameClient/HeightMap.h"
#include "W3DDevice/GameClient/W3DSmudge.h"
#include "W3DDevice/GameClient/W3DSnow.h"
#include "W3DDevice/GameClient/W3DAssetManager.h"
#include "Common/GlobalData.h"
#include "WW3D2/Camera.h"
#include "WW3D2/Texture.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string_view>
#include <vector>

#if defined(RTS_PROFILE_TRACY)
#include <tracy/Tracy.hpp>
#define GENERALS_GRAPHICS_PROFILE_SCOPE(name) ZoneScopedN(name)
#else
#define GENERALS_GRAPHICS_PROFILE_SCOPE(name) ((void)0)
#endif

import Graphics.Scene.Particles.Renderer;
import Graphics.Scene.Beams;
import Graphics.Scene.Screen.Distortion;

namespace
{

bool Build_Modern_Particle_Texture(const char *texture_name, Graphics::Texture &description, std::vector<std::byte> &pixels)
{
	GENERALS_GRAPHICS_PROFILE_SCOPE("Build_Modern_Particle_Texture");
	if (texture_name == nullptr || *texture_name == '\0')
		return false;

	WW3DAssetManager *assets = WW3DAssetManager::Get_Instance();
	if (assets == nullptr)
		return false;

	TextureClass *texture = assets->Get_Texture(texture_name, MIP_LEVELS_1);
	if (texture == nullptr)
		return false;
	SurfaceClass *surface = texture->Get_Surface_Level(0);
	if (surface == nullptr)
		return false;

	SurfaceClass::SurfaceDescription surface_description{};
	surface->Get_Description(surface_description);
	const std::size_t width = surface_description.Width;
	const std::size_t height = surface_description.Height;
	if (width == 0 || height == 0)
		return false;

	int source_pitch = 0;
	void *source_bits = surface->Lock(&source_pitch);
	if (source_bits == nullptr || source_pitch <= 0) {
		if (source_bits != nullptr)
			surface->Unlock();
		return false;
	}

	const auto *source = static_cast<const std::byte *>(source_bits);
	const auto Copy_RGBA = [&](std::size_t source_bytes_per_pixel) {
		if (static_cast<std::size_t>(source_pitch) < width * source_bytes_per_pixel)
			return false;
		pixels.resize(width * height * 4);
		for (std::size_t y = 0; y < height; ++y) {
			const std::byte *source_row = source + y * source_pitch;
			std::byte *destination_row = pixels.data() + y * width * 4;
			for (std::size_t x = 0; x < width; ++x) {
				const std::byte *source_pixel = source_row + x * source_bytes_per_pixel;
				std::byte *destination_pixel = destination_row + x * 4;
				destination_pixel[0] = source_pixel[0];
				destination_pixel[1] = source_pixel[source_bytes_per_pixel > 1 ? 1 : 0];
				destination_pixel[2] = source_pixel[source_bytes_per_pixel > 2 ? 2 : 0];
				destination_pixel[3] = source_bytes_per_pixel > 3 ? source_pixel[3] : std::byte{0xff};
			}
		}
		return true;
	};

	bool converted = false;
	switch (surface_description.Format) {
	case WW3D_FORMAT_A8R8G8B8:
	case WW3D_FORMAT_X8R8G8B8:
		if (static_cast<std::size_t>(source_pitch) >= width * 4) {
			pixels.resize(static_cast<std::size_t>(source_pitch) * height);
			std::memcpy(pixels.data(), source, pixels.size());
			description.format = Graphics::TextureFormat::BGRA8_UNorm;
			description.row_pitch = static_cast<std::uint32_t>(source_pitch);
			converted = true;
		}
		break;
	case WW3D_FORMAT_R8G8B8:
		converted = Copy_RGBA(3);
		description.format = Graphics::TextureFormat::RGBA8_UNorm;
		description.row_pitch = static_cast<std::uint32_t>(width * 4);
		break;
	case WW3D_FORMAT_A8:
		if (static_cast<std::size_t>(source_pitch) >= width) {
			pixels.resize(width * height * 4);
			for (std::size_t y = 0; y < height; ++y)
				for (std::size_t x = 0; x < width; ++x) {
					const std::byte alpha = source[y * source_pitch + x];
					std::byte *destination = pixels.data() + (y * width + x) * 4;
					destination[0] = std::byte{0xff};
					destination[1] = std::byte{0xff};
					destination[2] = std::byte{0xff};
					destination[3] = alpha;
				}
			converted = true;
			description.format = Graphics::TextureFormat::RGBA8_UNorm;
			description.row_pitch = static_cast<std::uint32_t>(width * 4);
			break;
		}
	case WW3D_FORMAT_L8:
		if (static_cast<std::size_t>(source_pitch) >= width) {
			pixels.resize(width * height * 4);
			for (std::size_t y = 0; y < height; ++y)
				for (std::size_t x = 0; x < width; ++x) {
					const std::byte luminance = source[y * source_pitch + x];
					std::byte *destination = pixels.data() + (y * width + x) * 4;
					destination[0] = luminance;
					destination[1] = luminance;
					destination[2] = luminance;
					destination[3] = std::byte{0xff};
				}
			converted = true;
			description.format = Graphics::TextureFormat::RGBA8_UNorm;
			description.row_pitch = static_cast<std::uint32_t>(width * 4);
			break;
		}
	case WW3D_FORMAT_A8L8:
		if (static_cast<std::size_t>(source_pitch) >= width * 2) {
			pixels.resize(width * height * 4);
			for (std::size_t y = 0; y < height; ++y)
				for (std::size_t x = 0; x < width; ++x) {
					const std::byte *source_pixel = source + y * source_pitch + x * 2;
					std::byte *destination = pixels.data() + (y * width + x) * 4;
					destination[0] = source_pixel[0];
					destination[1] = source_pixel[0];
					destination[2] = source_pixel[0];
					destination[3] = source_pixel[1];
				}
			converted = true;
			description.format = Graphics::TextureFormat::RGBA8_UNorm;
			description.row_pitch = static_cast<std::uint32_t>(width * 4);
			break;
		}
	default:
		break;
	}
	surface->Unlock();
	if (!converted)
		return false;

	description.width = static_cast<std::uint32_t>(width);
	description.height = static_cast<std::uint32_t>(height);
	description.depth = 1;
	description.mip_count = 1;
	description.usage = Graphics::TextureUsage::Sampled;
	description.pixel_data = std::span<const std::byte>(pixels.data(), pixels.size());
	return true;
}

}

W3DParticleSystemManager::W3DParticleSystemManager()
{
	m_modernEmitters.reserve(1024);
	m_modernStreaks.reserve(256);
	m_modernMaterials.reserve(256);
	m_modernSnowOnes.fill(1.0f);
	m_modernSnowFlags.fill(Graphics::ParticleEmitterFlags::Enabled | Graphics::ParticleEmitterFlags::Billboard);
	m_readyToRender = false;
	m_modernParticlesPrepared = false;
	m_onScreenParticleCount = 0;
}

W3DParticleSystemManager::~W3DParticleSystemManager()
{
	Reset_Modern_Particle_Bindings();
}

void W3DParticleSystemManager::queueParticleRender()
{
	m_readyToRender = true;
}

void DoParticles(RenderInfoClass &rinfo)
{
	if (TheParticleSystemManager)
		TheParticleSystemManager->doParticles(rinfo);
}

void W3DParticleSystemManager::doParticles(RenderInfoClass &rinfo)
{
	GENERALS_GRAPHICS_PROFILE_SCOPE("W3DParticleSystemManager::doParticles");
	if (!m_readyToRender)
		return;

	m_readyToRender = false;
	m_onScreenParticleCount = 0;
	m_fieldParticleCount = 0;
	m_terrainBoundsValid = false;
	Matrix3D legacy_view;
	Matrix4x4 legacy_projection;
	rinfo.Camera.Get_View_Matrix(&legacy_view);
	rinfo.Camera.Get_Projection_Matrix(&legacy_projection);
	Graphics::Matrix4x4 modern_view;
	Graphics::Matrix4x4 modern_projection;
	for (std::size_t row = 0; row < 4; ++row) {
		for (std::size_t column = 0; column < 4; ++column) {
			modern_view.values[row * 4 + column] = legacy_view[row][column];
			modern_projection.values[row * 4 + column] = legacy_projection[row][column];
		}
	}
	const Vector3 camera_position = rinfo.Camera.Get_Position();
	Set_Modern_Particle_View(Graphics::View(
		modern_view,
		modern_projection,
		{camera_position.X, camera_position.Y, camera_position.Z},
		m_modernView.viewport));
	if (TheTerrainRenderObject != nullptr) {
		AABoxClass bounds;
		TheTerrainRenderObject->getMaximumVisibleBox(rinfo.Camera.Get_Frustum(), &bounds, TRUE);
		m_terrainCenterX = bounds.Center.X;
		m_terrainCenterY = bounds.Center.Y;
		m_terrainCenterZ = bounds.Center.Z;
		m_terrainExtentX = bounds.Extent.X;
		m_terrainExtentY = bounds.Extent.Y;
		m_terrainExtentZ = bounds.Extent.Z;
		m_terrainBoundsValid = true;
	}
	Prepare_Modern_Particles();
	TheParticleSystemManager->setOnScreenParticleCount(m_onScreenParticleCount);
}

void W3DParticleSystemManager::Reset_Modern_Particle_Bindings() noexcept
{
	Graphics::ParticleRenderer &renderer = Graphics::GetParticleRenderer();
	for (const ModernEmitterBinding &binding : m_modernEmitters)
		if (renderer.Is_Initialized())
			renderer.Destroy_Emitter(binding.modern_emitter);
	for (ModernStreakBinding &binding : m_modernStreaks)
		if (Graphics::GetBeamRenderer().Is_Initialized()) {
			for (Graphics::BeamHandle beam : binding.beams)
				Graphics::GetBeamRenderer().Destroy(beam);
			if (binding.material.Is_Valid())
				Graphics::GetBeamRenderer().Destroy_Material(binding.material);
			if (binding.texture.Is_Valid())
				Graphics::GetBeamRenderer().Destroy_Texture(binding.texture);
		}
	if (renderer.Is_Initialized())
		renderer.Destroy_Emitter(m_modernSnowEmitter);
	for (const ModernMaterialBinding &binding : m_modernMaterials) {
		if (renderer.Is_Initialized()) {
			renderer.Destroy_Material(binding.material);
			renderer.Destroy_Texture(binding.texture);
		}
	}
	m_modernSnowEmitter = {};
	m_modernEmitters.clear();
	m_modernStreaks.clear();
	m_modernMaterials.clear();
	m_modernSyncStamp = 0;
	m_modernParticlesPrepared = false;
}

bool W3DParticleSystemManager::Set_Modern_Particle_View(const Graphics::View &view) noexcept
{
	m_modernView = view;
	m_modernViewValid = true;
	return Graphics::GetParticleRenderer().Set_View(view);
}

bool W3DParticleSystemManager::Render_Modern_Particles(Graphics::CommandList &commands, const Graphics::FrameTargets &targets) noexcept
{
	if (!m_modernParticlesPrepared)
		return true;

	if (!Graphics::GetParticleRenderer().Render(
		commands,
		targets.backbuffer.texture,
		targets.depth.texture,
		{0, 0, targets.backbuffer.width, targets.backbuffer.height, 0.0f, 1.0f}))
		return false;

	return Graphics::GetScreenDistortionRenderer().Render(
			commands,
			targets.backbuffer.texture,
			targets.depth.texture,
			{0, 0, targets.backbuffer.width, targets.backbuffer.height, 0.0f, 1.0f},
			{
				std::span<const float>(m_modernSmudgePositionX.data(), m_modernSmudgeCount),
				std::span<const float>(m_modernSmudgePositionY.data(), m_modernSmudgeCount),
				std::span<const float>(m_modernSmudgePositionZ.data(), m_modernSmudgeCount),
				std::span<const float>(m_modernSmudgeOffsetX.data(), m_modernSmudgeCount),
				std::span<const float>(m_modernSmudgeOffsetY.data(), m_modernSmudgeCount),
				std::span<const float>(m_modernSmudgeSizes.data(), m_modernSmudgeCount),
				std::span<const float>(m_modernSmudgeOpacities.data(), m_modernSmudgeCount)
			});
}

void W3DParticleSystemManager::Prepare_Modern_Particles()
{
	GENERALS_GRAPHICS_PROFILE_SCOPE("W3DParticleSystemManager::Prepare_Modern_Particles");
	Graphics::ParticleRenderer &renderer = Graphics::GetParticleRenderer();
	m_modernParticlesPrepared = renderer.Is_Initialized();
	if (!m_modernParticlesPrepared)
		return;

	++m_modernSyncStamp;
	if (m_modernSyncStamp == 0)
		m_modernSyncStamp = 1;
	{
		GENERALS_GRAPHICS_PROFILE_SCOPE("W3DParticleSystemManager::Reset_Modern_Particles");
		renderer.Reset_Particles();
	}
	m_modernSnowCount = 0;
	m_modernSmudgeCount = 0;
	{
		GENERALS_GRAPHICS_PROFILE_SCOPE("W3DParticleSystemManager::Prepare_Modern_Snow");
		Prepare_Modern_Snow();
	}
	if (TheSmudgeManager != nullptr && TheGlobalData != nullptr && TheGlobalData->m_useHeatEffects) {
		static_cast<W3DSmudgeManager *>(TheSmudgeManager)->resetDraw();
	}

	ParticleSystemManager::ParticleSystemList &systems = TheParticleSystemManager->getAllParticleSystems();
	for (ParticleSystem *system : systems) {
		GENERALS_GRAPHICS_PROFILE_SCOPE("W3DParticleSystemManager::Prepare_Modern_System");
		if (system == nullptr || system->isUsingDrawables())
			continue;
		if (system->isUsingSmudge()) {
			if (TheSmudgeManager != nullptr && TheGlobalData != nullptr && TheGlobalData->m_useHeatEffects) {
				for (Particle *particle = system->getFirstParticle(); particle != nullptr; particle = particle->m_systemNext) {
					const Coord3D *position = particle->getPosition();
					if (position != nullptr && Passes_Terrain_Bounds(position->x, position->y, position->z, std::max(0.0f, particle->getSize()))) {
						if (Smudge *smudge = TheSmudgeManager->findSmudge(particle))
							smudge->m_draw = true;
					}
				}
			}
			continue;
		}
		if (system->isUsingStreak()) {
			ModernStreakBinding *streak = Ensure_Modern_Streak(*system);
			if (streak == nullptr)
				continue;
			streak->sync_stamp = m_modernSyncStamp;
			Update_Modern_Streak(*system, *streak);
			continue;
		}
		if (!Is_Modern_Particle_System(*system))
			continue;

		const Graphics::ParticleEmitterHandle emitter_handle = Ensure_Modern_Emitter(*system);
		if (!emitter_handle.Is_Valid())
			continue;

		ModernEmitterBinding *binding = nullptr;
		for (ModernEmitterBinding &candidate : m_modernEmitters) {
			if (candidate.legacy_system == system) {
				binding = &candidate;
				break;
			}
		}
		if (binding == nullptr)
			continue;
		binding->sync_stamp = m_modernSyncStamp;

		Coord3D system_position;
		system->getPosition(&system_position);
		const Coord3D *drift = system->getDriftVelocity();
		Graphics::ParticleEmitter emitter;
		emitter.position = {system_position.x, system_position.y, system_position.z};
		emitter.velocity = drift != nullptr ? Graphics::Vector3{drift->x, drift->y, drift->z} : Graphics::Vector3{};
		emitter.material = Ensure_Modern_Material(system->getParticleTypeName().str());
		emitter.flags = Modern_Particle_Flags(*system);
		emitter.pipeline = renderer.Pipeline_For_Flags(emitter.flags);
		emitter.max_particles = static_cast<std::uint32_t>(MAX_PARTICLES_PER_SYSTEM);
		if (!renderer.Update_Emitter(emitter_handle, emitter))
			continue;

		const std::size_t layer_count = system->isUsingVolumeParticles()
			? std::clamp<std::size_t>(system->getVolumeParticleDepth(), 1, 16)
			: 1;
		const float layer_scale = layer_count > 1 ? 0.1f / static_cast<float>(layer_count) : 0.0f;
		const bool billboard = Graphics::Has_Particle_Emitter_Flag(emitter.flags, Graphics::ParticleEmitterFlags::Billboard);
		std::size_t source_count = 0;
		std::size_t count = 0;
		{
			GENERALS_GRAPHICS_PROFILE_SCOPE("W3DParticleSystemManager::Prepare_Modern_Particle_Data");
			for (Particle *particle = system->getFirstParticle(); particle != nullptr && source_count < MAX_PARTICLES_PER_SYSTEM; particle = particle->m_systemNext) {
			const Coord3D *position = particle->getPosition();
			const RGBColor *color = particle->getColor();
			if (position == nullptr || color == nullptr)
				continue;

			const float size = std::max(0.0f, particle->getSize());
			if (!Passes_Terrain_Bounds(position->x, position->y, position->z, size))
				continue;
			++source_count;
			const float to_camera_x = m_modernView.position.x - position->x;
			const float to_camera_y = m_modernView.position.y - position->y;
			const float to_camera_z = m_modernView.position.z - position->z;
			const float distance_squared = to_camera_x * to_camera_x + to_camera_y * to_camera_y + to_camera_z * to_camera_z;
			const float inverse_distance = distance_squared > 1.0e-12f ? 1.0f / std::sqrt(distance_squared) : 0.0f;
			for (std::size_t layer = 0; layer < layer_count && count < MAX_VOLUME_PARTICLES_PER_SYSTEM; ++layer) {
				const float shift = billboard ? static_cast<float>(layer) * size * layer_scale : 0.0f;
				m_modernPositionX[count] = position->x + to_camera_x * inverse_distance * shift;
				m_modernPositionY[count] = position->y + to_camera_y * inverse_distance * shift;
				m_modernPositionZ[count] = position->z + to_camera_z * inverse_distance * shift;
				m_modernVelocityX[count] = 0.0f;
				m_modernVelocityY[count] = 0.0f;
				m_modernVelocityZ[count] = 0.0f;
				m_modernLifetimes[count] = 1.0f;
				m_modernSizes[count] = size;
				m_modernColorR[count] = color->red;
				m_modernColorG[count] = color->green;
				m_modernColorB[count] = color->blue;
				m_modernColorA[count] = particle->getAlpha();
				m_modernAngles[count] = particle->getAngle();
				m_modernParticleMaterials[count] = emitter.material;
				m_modernEmitterFlags[count] = emitter.flags;
				m_modernPipelines[count] = emitter.pipeline;
				++count;
			}
			}
		}
		m_fieldParticleCount += system->getPriority() == AREA_EFFECT && system->m_isGroundAligned != FALSE ? static_cast<Int>(source_count) : 0;

		const Graphics::ParticleData data{
			std::span<const float>(m_modernPositionX.data(), count),
			std::span<const float>(m_modernPositionY.data(), count),
			std::span<const float>(m_modernPositionZ.data(), count),
			std::span<const float>(m_modernVelocityX.data(), count),
			std::span<const float>(m_modernVelocityY.data(), count),
			std::span<const float>(m_modernVelocityZ.data(), count),
			std::span<const float>(m_modernLifetimes.data(), count),
			std::span<const float>(m_modernSizes.data(), count),
			std::span<const float>(m_modernColorR.data(), count),
			std::span<const float>(m_modernColorG.data(), count),
			std::span<const float>(m_modernColorB.data(), count),
			std::span<const float>(m_modernColorA.data(), count),
			std::span<const float>(m_modernAngles.data(), count),
			std::span<const Graphics::MaterialHandle>(m_modernParticleMaterials.data(), count),
			std::span<const Graphics::ParticleEmitterFlags>(m_modernEmitterFlags.data(), count),
			{},
			std::span<const Graphics::PipelineHandle>(m_modernPipelines.data(), count)
		};
		if (renderer.Append_Particles(emitter_handle, data))
			m_onScreenParticleCount += static_cast<Int>(source_count);
	}
	{
		GENERALS_GRAPHICS_PROFILE_SCOPE("W3DParticleSystemManager::Prepare_Modern_Smudges");
		Prepare_Modern_Smudges();
	}

	for (std::size_t index = 0; index < m_modernEmitters.size();) {
		ModernEmitterBinding &binding = m_modernEmitters[index];
		if (binding.sync_stamp == m_modernSyncStamp) {
			++index;
			continue;
		}
		renderer.Destroy_Emitter(binding.modern_emitter);
		m_modernEmitters[index] = m_modernEmitters.back();
		m_modernEmitters.pop_back();
	}

	for (std::size_t index = 0; index < m_modernStreaks.size();) {
		ModernStreakBinding &binding = m_modernStreaks[index];
		if (binding.sync_stamp == m_modernSyncStamp) {
			++index;
			continue;
		}
		for (Graphics::BeamHandle beam : binding.beams)
			Graphics::GetBeamRenderer().Destroy(beam);
		if (binding.material.Is_Valid())
			Graphics::GetBeamRenderer().Destroy_Material(binding.material);
		if (binding.texture.Is_Valid())
			Graphics::GetBeamRenderer().Destroy_Texture(binding.texture);
		m_modernStreaks[index] = std::move(m_modernStreaks.back());
		m_modernStreaks.pop_back();
	}
}

void W3DParticleSystemManager::Prepare_Modern_Snow()
{
	if (TheSnowManager == nullptr || !Graphics::GetParticleRenderer().Is_Initialized())
		return;

	Graphics::ParticleRenderer &renderer = Graphics::GetParticleRenderer();
	W3DSnowManager *snow = static_cast<W3DSnowManager *>(TheSnowManager);
	const bool point_sprites = snow->Modern_Uses_Point_Sprites();
	const float point_sprite_size = snow->Modern_Point_Sprite_Size();
	Graphics::ParticleEmitter snow_emitter;
	snow_emitter.material = TheWeatherSetting != nullptr ? Ensure_Modern_Material(TheWeatherSetting->m_snowTexture.str()) : renderer.Default_Material();
	snow_emitter.flags = Graphics::ParticleEmitterFlags::Enabled | Graphics::ParticleEmitterFlags::Billboard;
	if (point_sprites)
		snow_emitter.flags = snow_emitter.flags | Graphics::ParticleEmitterFlags::PointSprite;
	snow_emitter.pipeline = renderer.Pipeline_For_Flags(snow_emitter.flags);
	snow_emitter.max_particles = static_cast<std::uint32_t>(MAX_MODERN_SNOW_PARTICLES);
	if (!m_modernSnowEmitter.Is_Valid()) {
		m_modernSnowEmitter = renderer.Create_Emitter(snow_emitter);
	}
	if (!m_modernSnowEmitter.Is_Valid())
		return;
	if (!renderer.Update_Emitter(m_modernSnowEmitter, snow_emitter))
		return;

	m_modernSnowCount = snow->Build_Modern_Particles(
		m_modernView.position.x,
		m_modernView.position.y,
		m_modernView.position.z,
		std::span<float>(m_modernSnowPositionX),
		std::span<float>(m_modernSnowPositionY),
		std::span<float>(m_modernSnowPositionZ),
		std::span<float>(m_modernSnowSizes));
	std::size_t compact_count = 0;
	for (std::size_t index = 0; index < m_modernSnowCount; ++index) {
		if (!Passes_Terrain_Bounds(m_modernSnowPositionX[index], m_modernSnowPositionY[index], m_modernSnowPositionZ[index], snow->Modern_Cull_Radius()))
			continue;
		m_modernSnowPositionX[compact_count] = m_modernSnowPositionX[index];
		m_modernSnowPositionY[compact_count] = m_modernSnowPositionY[index];
		m_modernSnowPositionZ[compact_count] = m_modernSnowPositionZ[index];
		m_modernSnowSizes[compact_count] = point_sprites ? point_sprite_size : m_modernSnowSizes[index];
		++compact_count;
	}
	m_modernSnowCount = compact_count;
	const Graphics::MaterialHandle material = TheWeatherSetting != nullptr
		? Ensure_Modern_Material(TheWeatherSetting->m_snowTexture.str())
		: renderer.Default_Material();
	const Graphics::ParticleEmitterFlags snow_flags = snow_emitter.flags;
	std::fill_n(m_modernSnowFlags.data(), m_modernSnowCount, snow_flags);
	std::fill_n(m_modernSnowMaterials.data(), m_modernSnowCount, material);
	const Graphics::ParticleData data{
		std::span<const float>(m_modernSnowPositionX.data(), m_modernSnowCount),
		std::span<const float>(m_modernSnowPositionY.data(), m_modernSnowCount),
		std::span<const float>(m_modernSnowPositionZ.data(), m_modernSnowCount),
		std::span<const float>(m_modernSnowZeros.data(), m_modernSnowCount),
		std::span<const float>(m_modernSnowZeros.data(), m_modernSnowCount),
		std::span<const float>(m_modernSnowZeros.data(), m_modernSnowCount),
		std::span<const float>(m_modernSnowOnes.data(), m_modernSnowCount),
		std::span<const float>(m_modernSnowSizes.data(), m_modernSnowCount),
		std::span<const float>(m_modernSnowOnes.data(), m_modernSnowCount),
		std::span<const float>(m_modernSnowOnes.data(), m_modernSnowCount),
		std::span<const float>(m_modernSnowOnes.data(), m_modernSnowCount),
		std::span<const float>(m_modernSnowOnes.data(), m_modernSnowCount),
		std::span<const float>(m_modernSnowZeros.data(), m_modernSnowCount),
		std::span<const Graphics::MaterialHandle>(m_modernSnowMaterials.data(), m_modernSnowCount),
		std::span<const Graphics::ParticleEmitterFlags>(m_modernSnowFlags.data(), m_modernSnowCount),
		{}
	};
	renderer.Append_Particles(m_modernSnowEmitter, data);
}

void W3DParticleSystemManager::Prepare_Modern_Smudges()
{
	m_modernSmudgeCount = 0;
	if (TheSmudgeManager == nullptr || TheGlobalData == nullptr || !TheGlobalData->m_useHeatEffects)
		return;
	m_modernSmudgeCount = static_cast<W3DSmudgeManager *>(TheSmudgeManager)->Collect_Modern_Smudges(
		std::span<float>(m_modernSmudgePositionX),
		std::span<float>(m_modernSmudgePositionY),
		std::span<float>(m_modernSmudgePositionZ),
		std::span<float>(m_modernSmudgeOffsetX),
		std::span<float>(m_modernSmudgeOffsetY),
		std::span<float>(m_modernSmudgeSizes),
		std::span<float>(m_modernSmudgeOpacities));
}

bool W3DParticleSystemManager::Is_Modern_Particle_System(const ParticleSystem &system) const noexcept
{
	return system.m_particleType == ParticleSystemInfo::PARTICLE || system.m_particleType == ParticleSystemInfo::VOLUME_PARTICLE;
}

Graphics::ParticleEmitterHandle W3DParticleSystemManager::Find_Modern_Emitter(ParticleSystem *system) const noexcept
{
	GENERALS_GRAPHICS_PROFILE_SCOPE("W3DParticleSystemManager::Find_Modern_Emitter");
	for (const ModernEmitterBinding &binding : m_modernEmitters)
		if (binding.legacy_system == system)
			return binding.modern_emitter;
	return {};
}

Graphics::ParticleEmitterHandle W3DParticleSystemManager::Ensure_Modern_Emitter(ParticleSystem &system)
{
	GENERALS_GRAPHICS_PROFILE_SCOPE("W3DParticleSystemManager::Ensure_Modern_Emitter");
	const Graphics::ParticleEmitterHandle existing = Find_Modern_Emitter(&system);
	if (existing.Is_Valid())
		return existing;

	Graphics::ParticleEmitter emitter;
	emitter.material = Ensure_Modern_Material(system.getParticleTypeName().str());
	emitter.flags = Modern_Particle_Flags(system);
	emitter.pipeline = Graphics::GetParticleRenderer().Pipeline_For_Flags(emitter.flags);
	emitter.max_particles = static_cast<std::uint32_t>(MAX_PARTICLES_PER_SYSTEM);
	const Graphics::ParticleEmitterHandle handle = Graphics::GetParticleRenderer().Create_Emitter(emitter);
	if (!handle.Is_Valid())
		return {};

	m_modernEmitters.push_back({&system, handle, 0});
	return handle;
}

Graphics::MaterialHandle W3DParticleSystemManager::Ensure_Modern_Material(const char *texture_name)
{
	GENERALS_GRAPHICS_PROFILE_SCOPE("W3DParticleSystemManager::Ensure_Modern_Material");
	if (texture_name == nullptr || *texture_name == '\0')
		return Graphics::GetParticleRenderer().Default_Material();

	for (const ModernMaterialBinding &binding : m_modernMaterials)
		if (binding.texture_name == texture_name)
			return binding.material.Is_Valid() ? binding.material : Graphics::GetParticleRenderer().Default_Material();

	Graphics::ParticleRenderer &renderer = Graphics::GetParticleRenderer();
	const Graphics::MaterialHandle fallback_material = renderer.Default_Material();
	Graphics::Texture texture_description;
	std::vector<std::byte> pixels;
	if (!Build_Modern_Particle_Texture(texture_name, texture_description, pixels)) {
		m_modernMaterials.push_back({texture_name, {}, fallback_material});
		return fallback_material;
	}

	const Graphics::TextureHandle texture = renderer.Create_Texture(texture_description, texture_description.pixel_data);
	if (!texture.Is_Valid()) {
		m_modernMaterials.push_back({texture_name, {}, fallback_material});
		return fallback_material;
	}

	Graphics::Material material;
	material.shader = renderer.Particle_Shader();
	material.textures[0] = texture;
	material.parameters.values[0] = 1.0f;
	material.parameters.values[1] = 1.0f;
	material.parameters.values[2] = 1.0f;
	material.parameters.values[3] = 1.0f;
	const Graphics::MaterialHandle material_handle = renderer.Create_Material(material);
	if (!material_handle.Is_Valid()) {
		renderer.Destroy_Texture(texture);
		m_modernMaterials.push_back({texture_name, {}, fallback_material});
		return fallback_material;
	}

	m_modernMaterials.push_back({texture_name, texture, material_handle});
	return material_handle;
}

Graphics::ParticleEmitterFlags W3DParticleSystemManager::Modern_Particle_Flags(const ParticleSystem &system) const noexcept
{
	Graphics::ParticleEmitterFlags flags = Graphics::ParticleEmitterFlags::Enabled;
	if (system.m_isGroundAligned == FALSE)
		flags = flags | Graphics::ParticleEmitterFlags::Billboard;

	switch (system.m_shaderType) {
	case ParticleSystemInfo::ADDITIVE:
		flags = flags | Graphics::ParticleEmitterFlags::Additive;
		break;
	case ParticleSystemInfo::ALPHA_TEST:
		flags = flags | Graphics::ParticleEmitterFlags::AlphaTest;
		break;
	case ParticleSystemInfo::MULTIPLY:
		flags = flags | Graphics::ParticleEmitterFlags::Multiply;
		break;
	case ParticleSystemInfo::ALPHA:
	case ParticleSystemInfo::INVALID_SHADER:
	case ParticleSystemInfo::PARTICLE_SHADER_TYPE_COUNT:
		break;
	}
	return flags;
}

W3DParticleSystemManager::ModernStreakBinding *W3DParticleSystemManager::Find_Modern_Streak(ParticleSystem *system) noexcept
{
	GENERALS_GRAPHICS_PROFILE_SCOPE("W3DParticleSystemManager::Find_Modern_Streak");
	for (ModernStreakBinding &binding : m_modernStreaks)
		if (binding.legacy_system == system)
			return &binding;
	return nullptr;
}

W3DParticleSystemManager::ModernStreakBinding *W3DParticleSystemManager::Ensure_Modern_Streak(ParticleSystem &system)
{
	GENERALS_GRAPHICS_PROFILE_SCOPE("W3DParticleSystemManager::Ensure_Modern_Streak");
	if (ModernStreakBinding *existing = Find_Modern_Streak(&system))
		return existing;

	ModernStreakBinding binding;
	binding.legacy_system = &system;
	binding.texture_name = system.getParticleTypeName().str();
	Graphics::BeamRenderer &renderer = Graphics::GetBeamRenderer();
	Graphics::Texture texture_description;
	std::vector<std::byte> pixels;
	if (Build_Modern_Particle_Texture(binding.texture_name.c_str(), texture_description, pixels)) {
		binding.texture = renderer.Create_Texture(texture_description, texture_description.pixel_data);
		if (binding.texture.Is_Valid()) {
			Graphics::Material material;
			material.shader = renderer.Beam_Shader();
			material.textures[0] = binding.texture;
			binding.material = renderer.Create_Material(material);
			if (!binding.material.Is_Valid()) {
				renderer.Destroy_Texture(binding.texture);
				binding.texture = {};
			}
		}
	}
	if (!binding.material.Is_Valid())
		binding.material = renderer.Default_Material();
	binding.beams.reserve(MAX_PARTICLES_PER_SYSTEM - 1);
	m_modernStreaks.push_back(std::move(binding));
	return &m_modernStreaks.back();
}

Graphics::BeamFlags W3DParticleSystemManager::Modern_Streak_Flags(const ParticleSystem &system) const noexcept
{
	Graphics::BeamFlags flags = Graphics::BeamFlags::Enabled;
	switch (system.m_shaderType) {
	case ParticleSystemInfo::ADDITIVE:
		flags = flags | Graphics::BeamFlags::Additive;
		break;
	case ParticleSystemInfo::ALPHA_TEST:
		flags = flags | Graphics::BeamFlags::AlphaTest;
		break;
	case ParticleSystemInfo::MULTIPLY:
		flags = flags | Graphics::BeamFlags::Multiply;
		break;
	case ParticleSystemInfo::ALPHA:
	case ParticleSystemInfo::INVALID_SHADER:
	case ParticleSystemInfo::PARTICLE_SHADER_TYPE_COUNT:
		break;
	}
	return flags;
}

void W3DParticleSystemManager::Update_Modern_Streak(ParticleSystem &system, ModernStreakBinding &binding) noexcept
{
	GENERALS_GRAPHICS_PROFILE_SCOPE("W3DParticleSystemManager::Update_Modern_Streak");
	Graphics::BeamRenderer &renderer = Graphics::GetBeamRenderer();
	std::array<Particle *, MAX_PARTICLES_PER_SYSTEM> points{};
	std::size_t point_count = 0;
	for (Particle *particle = system.getFirstParticle(); particle != nullptr && point_count < MAX_PARTICLES_PER_SYSTEM; particle = particle->m_systemNext) {
		const Coord3D *position = particle->getPosition();
		if (position != nullptr && Passes_Terrain_Bounds(position->x, position->y, position->z, std::max(0.0f, particle->getSize())))
			points[point_count++] = particle;
	}
	const std::size_t segment_count = point_count > 1 ? point_count - 1 : 0;
	while (binding.beams.size() < segment_count) {
		Graphics::BeamDescription description;
		description.material = binding.material;
		description.flags = Modern_Streak_Flags(system);
		const Graphics::BeamHandle beam = renderer.Create(description);
		if (!beam.Is_Valid())
			return;
		binding.beams.push_back(beam);
	}
	while (binding.beams.size() > segment_count) {
		renderer.Destroy(binding.beams.back());
		binding.beams.pop_back();
	}

	const Graphics::BeamFlags flags = Modern_Streak_Flags(system);
	for (std::size_t segment = 0; segment < segment_count; ++segment) {
		const Coord3D *start = points[segment]->getPosition();
		const Coord3D *end = points[segment + 1]->getPosition();
		const RGBColor *start_color = points[segment]->getColor();
		const RGBColor *end_color = points[segment + 1]->getColor();
		if (start == nullptr || end == nullptr || start_color == nullptr || end_color == nullptr)
			continue;
		Graphics::BeamDescription description;
		description.start = {start->x, start->y, start->z};
		description.end = {end->x, end->y, end->z};
		description.width = 2.0f * std::max(0.0f, points[segment + 1]->getSize());
		description.color = {end_color->red, end_color->green, end_color->blue, 1.0f};
		description.opacity = 1.0f;
		description.start_color = segment == 0
			? Graphics::Color4{}
			: Graphics::Color4{start_color->red, start_color->green, start_color->blue, points[segment]->getAlpha()};
		description.end_color = {end_color->red, end_color->green, end_color->blue, points[segment + 1]->getAlpha()};
		description.color_gradient = true;
		description.uv_offset = 0.0f;
		description.uv_scale = 0.0f;
		description.material = binding.material;
		description.flags = flags;
		renderer.Update(binding.beams[segment], description);
	}
	m_onScreenParticleCount += static_cast<Int>(point_count);
}

bool W3DParticleSystemManager::Passes_Terrain_Bounds(float x, float y, float z, float radius) const noexcept
{
	return !m_terrainBoundsValid
		|| std::fabs(x - m_terrainCenterX) <= m_terrainExtentX + radius
		&& std::fabs(y - m_terrainCenterY) <= m_terrainExtentY + radius
		&& std::fabs(z - m_terrainCenterZ) <= m_terrainExtentZ + radius;
}
