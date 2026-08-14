#pragma once

#include "raylib/raylib.h"
#include "raylib/raymath.h"

namespace Sounds
{
    constexpr int LASER_SOUND_COUNT = 5;
    constexpr int EXPLOSION_SOUND_COUNT = 4;
    constexpr int HURT_SOUND_COUNT = 1;
    constexpr float DEFAULT_MASTER_VOLUME = 0.8f;

    extern float MasterVolume;
    extern Sound LaserSounds[LASER_SOUND_COUNT];
    extern Sound ExplosionSounds[EXPLOSION_SOUND_COUNT];
    extern Sound HurtSounds[HURT_SOUND_COUNT];
    extern Sound BoosterSound;

    void Init();
    void Deinit();
    void Shutdown();
    void SetMasterVolume(float volume);

    void PlayLaser(Vector3 sourcePosition, Vector3 listenerPosition = { 0.0f, 0.0f, 0.0f });
    void PlayExplosion(Vector3 sourcePosition, Vector3 listenerPosition = { 0.0f, 0.0f, 0.0f });
    void PlayHurt(Vector3 sourcePosition, Vector3 listenerPosition = { 0.0f, 0.0f, 0.0f });
    void PlayBooster(Vector3 sourcePosition, Vector3 listenerPosition = { 0.0f, 0.0f, 0.0f });
    void StopBooster();
}