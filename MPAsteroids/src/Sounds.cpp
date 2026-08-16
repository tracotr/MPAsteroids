#include "include/Sounds.h"
#include "include/GameApp.h"

#include <cstdio>

namespace Sounds
{
    float MasterVolume = 0.3f;
    Sound LaserSounds[LASER_SOUND_COUNT] = { 0 };
    Sound ExplosionSounds[EXPLOSION_SOUND_COUNT] = { 0 };
    Sound HurtSounds[HURT_SOUND_COUNT] = { 0 };
    Sound BoosterSound = { 0 };

    static bool g_initialized = false;
    static bool g_processorAttached = false;

    static void ProcessAudio(void *bufferData, unsigned int frames)
    {
        (void)bufferData;
        (void)frames;
    }

    static void ApplySpatialMix(Sound sound, Vector3 sourcePosition, Vector3 listenerPosition)
    {
        Camera3D camera = GameApp::GetInstance()->GetCamera();
        
        Vector3 delta = Vector3Subtract(sourcePosition, listenerPosition);
        float distance = Vector3Length(delta);

        float pan = 0.5f;
        if (distance > 0.0001f)
        {
            // Get the camera's right vector
            Vector3 forward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
            Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, camera.up));
            
            // Project sound direction onto camera's right vector
            float dotRight = Vector3DotProduct(Vector3Normalize(delta), right);
            pan = 0.5f + (dotRight * 0.5f);
        }

        float spatialVolume = Clamp(1.0f / (1.0f + distance * 0.12f), 0.0f, 1.0f);
        float finalVolume = spatialVolume * MasterVolume;

        SetSoundPan(sound, Clamp(pan, 0.0f, 1.0f));
        SetSoundVolume(sound, finalVolume);
        SetSoundPitch(sound, 1.0f + Clamp(distance * 0.02f, 0.0f, 0.4f));
    }

    static void PlayRandomSound(const Sound* sounds, int count, Vector3 sourcePosition, Vector3 listenerPosition)
    {
        if (sounds == nullptr || count <= 0)
            return;

        int index = GetRandomValue(0, count - 1);
        ApplySpatialMix(sounds[index], sourcePosition, listenerPosition);
        PlaySound(sounds[index]);
    }

    void SetMasterVolume(float volume)
    {
        MasterVolume = Clamp(volume, 0.0f, 1.0f);
        ::SetMasterVolume(MasterVolume);
    }

    void Init()
    {
        if (g_initialized)
            return;

        InitAudioDevice();
        AttachAudioMixedProcessor(ProcessAudio);
        g_processorAttached = true;
        SetMasterVolume(MasterVolume);

        LaserSounds[0] = LoadSound("resources/sounds/laser/laser1.wav");
        LaserSounds[1] = LoadSound("resources/sounds/laser/laser2.wav");
        LaserSounds[2] = LoadSound("resources/sounds/laser/laser3.wav");
        LaserSounds[3] = LoadSound("resources/sounds/laser/laser4.wav");
        LaserSounds[4] = LoadSound("resources/sounds/laser/laser5.wav");

        ExplosionSounds[0] = LoadSound("resources/sounds/explosion/explosion1.wav");
        ExplosionSounds[1] = LoadSound("resources/sounds/explosion/explosion2.wav");
        ExplosionSounds[2] = LoadSound("resources/sounds/explosion/explosion3.wav");
        ExplosionSounds[3] = LoadSound("resources/sounds/explosion/explosion4.wav");

        HurtSounds[0] = LoadSound("resources/sounds/hurt/hurt.wav");
        BoosterSound = LoadSound("resources/sounds/ship/booster.wav");
        SetSoundVolume(BoosterSound, 0.6f);
        SetSoundPitch(BoosterSound, 1.0f);

        g_initialized = true;
    }

    void Deinit()
    {
        if (!g_initialized)
            return;

        for (int i = 0; i < LASER_SOUND_COUNT; ++i)
        {
            if (IsSoundValid(LaserSounds[i]))
                UnloadSound(LaserSounds[i]);
        }

        for (int i = 0; i < EXPLOSION_SOUND_COUNT; ++i)
        {
            if (IsSoundValid(ExplosionSounds[i]))
                UnloadSound(ExplosionSounds[i]);
        }

        for (int i = 0; i < HURT_SOUND_COUNT; ++i)
        {
            if (IsSoundValid(HurtSounds[i]))
                UnloadSound(HurtSounds[i]);
        }

        if (IsSoundValid(BoosterSound))
            UnloadSound(BoosterSound);

        if (g_processorAttached)
        {
            DetachAudioMixedProcessor(ProcessAudio);
            g_processorAttached = false;
        }

        CloseAudioDevice();
        g_initialized = false;
    }

    void Shutdown()
    {
        Deinit();
    }

    void PlayLaser(Vector3 sourcePosition, Vector3 listenerPosition)
    {
        if (!g_initialized)
            return;

        PlayRandomSound(LaserSounds, LASER_SOUND_COUNT, sourcePosition, listenerPosition);
    }

    void PlayExplosion(Vector3 sourcePosition, Vector3 listenerPosition)
    {
        if (!g_initialized)
            return;

        PlayRandomSound(ExplosionSounds, EXPLOSION_SOUND_COUNT, sourcePosition, listenerPosition);
    }

    void PlayHurt(Vector3 sourcePosition, Vector3 listenerPosition)
    {
        if (!g_initialized)
            return;

        PlayRandomSound(HurtSounds, HURT_SOUND_COUNT, sourcePosition, listenerPosition);
    }

    void PlayBooster(Vector3 sourcePosition, Vector3 listenerPosition)
    {
        if (!g_initialized || !IsSoundValid(BoosterSound))
            return;

        ApplySpatialMix(BoosterSound, sourcePosition, listenerPosition);
        if (!IsSoundPlaying(BoosterSound))
            PlaySound(BoosterSound);
    }

    void StopBooster()
    {
        if (!g_initialized || !IsSoundValid(BoosterSound))
            return;

        StopSound(BoosterSound);
    }
}