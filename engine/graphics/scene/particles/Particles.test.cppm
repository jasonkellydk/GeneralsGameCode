module;

#define BOOST_TEST_MODULE GraphicsParticlesTests

#include <boost/test/included/unit_test.hpp>

#include <type_traits>

export module Graphics.Scene.Particles.Tests;

import Graphics.Scene.Particles;

static_assert(!std::is_convertible_v<ParticleEmitterHandle, MaterialHandle>);
static_assert(std::is_nothrow_move_constructible_v<ParticleEmitter>);
static_assert(std::is_nothrow_move_assignable_v<ParticleEmitter>);
static_assert(std::is_nothrow_move_constructible_v<GPUParticleData>);

BOOST_AUTO_TEST_CASE(particles_spawn_into_soa_storage_and_simulate)
{
	ParticleSystem particles;
	particles.Reserve(1, 4);

	ParticleEmitter emitter;
	emitter.position = {1.0f, 2.0f, 3.0f};
	emitter.velocity = {0.0f, 0.0f, 4.0f};
	emitter.particle_lifetime = 1.0f;
	emitter.particle_size = 2.0f;
	emitter.color = {0.25f, 0.5f, 0.75f, 1.0f};
	emitter.material = MaterialHandle(6, 1);
	emitter.max_particles = 4;
	const ParticleEmitterHandle handle = particles.Create_Emitter(emitter);

	BOOST_REQUIRE(particles.Spawn(handle, 2));
	BOOST_CHECK(particles.Particle_Count() == 2);
	BOOST_CHECK(particles.Particle_Count(handle) == 2);
	const ParticleData before_update = particles.Particles();
	BOOST_CHECK(before_update.position_z[0] == 3.0f);
	BOOST_CHECK(before_update.velocity_z[0] == 4.0f);
	BOOST_CHECK(before_update.lifetimes[0] == 1.0f);
	BOOST_CHECK(before_update.sizes[0] == 2.0f);
	BOOST_CHECK(before_update.color_g[0] == 0.5f);
	BOOST_CHECK(before_update.materials[0] == emitter.material);

	BOOST_REQUIRE(particles.Update(0.25f));
	const ParticleData after_update = particles.Particles();
	BOOST_CHECK(after_update.position_z[0] == 4.0f);
	BOOST_CHECK(after_update.lifetimes[0] == 0.75f);

	const GPUParticleData gpu_particle = Pack_GPU_Particle(after_update, 0, 11);
	BOOST_CHECK(gpu_particle.position_lifetime[2] == 4.0f);
	BOOST_CHECK(gpu_particle.position_lifetime[3] == 0.75f);
	BOOST_CHECK(gpu_particle.velocity_size[3] == 2.0f);
	BOOST_CHECK(gpu_particle.material_index == 11);
}

BOOST_AUTO_TEST_CASE(expired_particles_are_removed_and_emitters_reject_stale_handles)
{
	ParticleSystem particles;
	particles.Reserve(1, 2);
	ParticleEmitter emitter;
	emitter.particle_lifetime = 0.5f;
	emitter.max_particles = 2;
	const ParticleEmitterHandle old_handle = particles.Create_Emitter(emitter);
	BOOST_REQUIRE(particles.Spawn(old_handle, 2));
	BOOST_REQUIRE(particles.Update(0.5f));
	BOOST_CHECK(particles.Particle_Count() == 0);
	BOOST_CHECK(particles.Particle_Count(old_handle) == 0);

	BOOST_REQUIRE(particles.Spawn(old_handle, 1));
	BOOST_REQUIRE(particles.Destroy_Emitter(old_handle));
	BOOST_CHECK(!particles.Is_Emitter_Valid(old_handle));
	BOOST_CHECK(!particles.Spawn(old_handle, 1));

	const ParticleEmitterHandle new_handle = particles.Create_Emitter(emitter);
	BOOST_CHECK(new_handle.Get_Index() == old_handle.Get_Index());
	BOOST_CHECK(new_handle != old_handle);
	BOOST_CHECK(particles.Emitter_Count() == 1);
	BOOST_CHECK(particles.Particle_Count() == 0);
}

BOOST_AUTO_TEST_CASE(emitter_updates_propagate_render_state_to_live_particles)
{
	ParticleSystem particles;
	particles.Reserve(1, 1);
	const ParticleEmitterHandle handle = particles.Create_Emitter();
	BOOST_REQUIRE(particles.Spawn(handle, 1));

	ParticleEmitter updated;
	updated.material = MaterialHandle(12, 1);
	updated.flags = ParticleEmitterFlags::None;
	BOOST_REQUIRE(particles.Update_Emitter(handle, updated));

	const ParticleData data = particles.Particles();
	BOOST_CHECK(data.materials[0] == updated.material);
	BOOST_CHECK(data.emitter_flags[0] == ParticleEmitterFlags::None);
	BOOST_CHECK(!particles.Update(-1.0f));
}
