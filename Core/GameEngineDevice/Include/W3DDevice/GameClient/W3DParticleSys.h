#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "GameClient/ParticleSys.h"
#include "WW3D2/RInfo.h"

import Graphics.Scene.Particles.Renderer;
import Graphics.Scene.Beams;
import Graphics.RHI.Frame;

class W3DParticleSystemManager : public ParticleSystemManager
{
public:
	W3DParticleSystemManager();
	virtual ~W3DParticleSystemManager() override;

	virtual void doParticles(RenderInfoClass &rinfo) override;
	virtual void queueParticleRender() override;
	virtual Int getOnScreenParticleCount() override { return m_onScreenParticleCount; }

	void Reset_Modern_Particle_Bindings() noexcept;
	bool Set_Modern_Particle_View(const Graphics::View &view) noexcept;
	bool Render_Modern_Particles(Graphics::CommandList &commands, const Graphics::FrameTargets &targets) noexcept;

private:
	static constexpr std::size_t MAX_PARTICLES_PER_SYSTEM = 512;
	static constexpr std::size_t MAX_VOLUME_PARTICLES_PER_SYSTEM = MAX_PARTICLES_PER_SYSTEM * 16;
	static constexpr std::size_t MAX_MODERN_SNOW_PARTICLES = 65536;
	static constexpr std::size_t MAX_MODERN_SMUDGES = 512;

	struct ModernEmitterBinding final
	{
		ParticleSystem *legacy_system = nullptr;
		Graphics::ParticleEmitterHandle modern_emitter{};
		std::uint32_t sync_stamp = 0;
	};

	struct ModernStreakBinding final
	{
		ParticleSystem *legacy_system = nullptr;
		std::string texture_name;
		Graphics::TextureHandle texture{};
		Graphics::MaterialHandle material{};
		std::vector<Graphics::BeamHandle> beams;
		std::uint32_t sync_stamp = 0;
	};

	struct ModernMaterialBinding final
	{
		std::string texture_name;
		Graphics::TextureHandle texture{};
		Graphics::MaterialHandle material{};
	};

	void Prepare_Modern_Particles();
	bool Is_Modern_Particle_System(const ParticleSystem &system) const noexcept;
	Graphics::ParticleEmitterHandle Find_Modern_Emitter(ParticleSystem *system) const noexcept;
	Graphics::ParticleEmitterHandle Ensure_Modern_Emitter(ParticleSystem &system);
	ModernStreakBinding *Find_Modern_Streak(ParticleSystem *system) noexcept;
	ModernStreakBinding *Ensure_Modern_Streak(ParticleSystem &system);
	Graphics::BeamFlags Modern_Streak_Flags(const ParticleSystem &system) const noexcept;
	void Update_Modern_Streak(ParticleSystem &system, ModernStreakBinding &binding) noexcept;
	Graphics::MaterialHandle Ensure_Modern_Material(const char *texture_name);
	Graphics::ParticleEmitterFlags Modern_Particle_Flags(const ParticleSystem &system) const noexcept;
	bool Passes_Terrain_Bounds(float x, float y, float z, float radius) const noexcept;
	void Prepare_Modern_Snow();
	void Prepare_Modern_Smudges();

	std::vector<ModernEmitterBinding> m_modernEmitters;
	std::vector<ModernStreakBinding> m_modernStreaks;
	std::vector<ModernMaterialBinding> m_modernMaterials;
	Graphics::ParticleEmitterHandle m_modernSnowEmitter{};
	std::uint32_t m_modernSyncStamp = 0;
	std::array<float, MAX_VOLUME_PARTICLES_PER_SYSTEM> m_modernPositionX{};
	std::array<float, MAX_VOLUME_PARTICLES_PER_SYSTEM> m_modernPositionY{};
	std::array<float, MAX_VOLUME_PARTICLES_PER_SYSTEM> m_modernPositionZ{};
	std::array<float, MAX_VOLUME_PARTICLES_PER_SYSTEM> m_modernVelocityX{};
	std::array<float, MAX_VOLUME_PARTICLES_PER_SYSTEM> m_modernVelocityY{};
	std::array<float, MAX_VOLUME_PARTICLES_PER_SYSTEM> m_modernVelocityZ{};
	std::array<float, MAX_VOLUME_PARTICLES_PER_SYSTEM> m_modernLifetimes{};
	std::array<float, MAX_VOLUME_PARTICLES_PER_SYSTEM> m_modernSizes{};
	std::array<float, MAX_VOLUME_PARTICLES_PER_SYSTEM> m_modernColorR{};
	std::array<float, MAX_VOLUME_PARTICLES_PER_SYSTEM> m_modernColorG{};
	std::array<float, MAX_VOLUME_PARTICLES_PER_SYSTEM> m_modernColorB{};
	std::array<float, MAX_VOLUME_PARTICLES_PER_SYSTEM> m_modernColorA{};
	std::array<float, MAX_VOLUME_PARTICLES_PER_SYSTEM> m_modernAngles{};
	std::array<Graphics::MaterialHandle, MAX_VOLUME_PARTICLES_PER_SYSTEM> m_modernParticleMaterials{};
	std::array<Graphics::ParticleEmitterFlags, MAX_VOLUME_PARTICLES_PER_SYSTEM> m_modernEmitterFlags{};
	std::array<Graphics::PipelineHandle, MAX_VOLUME_PARTICLES_PER_SYSTEM> m_modernPipelines{};
	std::array<float, MAX_MODERN_SNOW_PARTICLES> m_modernSnowPositionX{};
	std::array<float, MAX_MODERN_SNOW_PARTICLES> m_modernSnowPositionY{};
	std::array<float, MAX_MODERN_SNOW_PARTICLES> m_modernSnowPositionZ{};
	std::array<float, MAX_MODERN_SNOW_PARTICLES> m_modernSnowSizes{};
	std::array<float, MAX_MODERN_SNOW_PARTICLES> m_modernSnowZeros{};
	std::array<float, MAX_MODERN_SNOW_PARTICLES> m_modernSnowOnes{};
	std::array<Graphics::MaterialHandle, MAX_MODERN_SNOW_PARTICLES> m_modernSnowMaterials{};
	std::array<Graphics::ParticleEmitterFlags, MAX_MODERN_SNOW_PARTICLES> m_modernSnowFlags{};
	std::array<float, MAX_MODERN_SMUDGES> m_modernSmudgePositionX{};
	std::array<float, MAX_MODERN_SMUDGES> m_modernSmudgePositionY{};
	std::array<float, MAX_MODERN_SMUDGES> m_modernSmudgePositionZ{};
	std::array<float, MAX_MODERN_SMUDGES> m_modernSmudgeOffsetX{};
	std::array<float, MAX_MODERN_SMUDGES> m_modernSmudgeOffsetY{};
	std::array<float, MAX_MODERN_SMUDGES> m_modernSmudgeSizes{};
	std::array<float, MAX_MODERN_SMUDGES> m_modernSmudgeOpacities{};
	std::size_t m_modernSnowCount = 0;
	std::size_t m_modernSmudgeCount = 0;
	Graphics::View m_modernView{};
	bool m_modernViewValid = false;
	float m_terrainCenterX = 0.0f;
	float m_terrainCenterY = 0.0f;
	float m_terrainCenterZ = 0.0f;
	float m_terrainExtentX = 0.0f;
	float m_terrainExtentY = 0.0f;
	float m_terrainExtentZ = 0.0f;
	bool m_terrainBoundsValid = false;
	Int m_onScreenParticleCount = 0;
	Bool m_readyToRender = false;
	Bool m_modernParticlesPrepared = false;
};
