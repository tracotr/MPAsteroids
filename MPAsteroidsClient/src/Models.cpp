#include "include/Models.h"

#include <stdio.h>
#include <float.h>

namespace Models
{
    Model Skybox;
    Model ShipModel;
    BoundingBox ShipBoxLocal;
    Model AsteroidModel;
    BoundingBox AsteroidBoxLocal;

    void Init()
    {
        // Load skybox
        Mesh skyboxCube = GenMeshCube(1.0f, 1.0f, 1.0f);
        Skybox = LoadModelFromMesh(skyboxCube);
        Skybox.materials[0].shader = LoadShader("resources/shaders/glsl/skybox.vs", "resources/shaders/glsl/skybox.fs");
        SetShaderValue(Skybox.materials[0].shader, GetShaderLocation(Skybox.materials[0].shader, "environmentMap"), (int[1]){ MATERIAL_MAP_CUBEMAP }, SHADER_UNIFORM_INT);
        SetShaderValue(Skybox.materials[0].shader, GetShaderLocation(Skybox.materials[0].shader, "doGamma"), (int[1]) { 0 }, SHADER_UNIFORM_INT);
        SetShaderValue(Skybox.materials[0].shader, GetShaderLocation(Skybox.materials[0].shader, "vflipped"), (int[1]) { 0 }, SHADER_UNIFORM_INT);

        Shader shdrCubemap = LoadShader("resources/shaders/glsl/cubemap.vs", "resources/shaders/glsl/cubemap.fs");
        SetShaderValue(shdrCubemap, GetShaderLocation(shdrCubemap, "equirectangularMap"), (int[1]) { 0 }, SHADER_UNIFORM_INT);

        Image img = LoadImage("resources/skybox/StarrySky.png");
        Skybox.materials[0].maps[MATERIAL_MAP_CUBEMAP].texture = LoadTextureCubemap(img, CUBEMAP_LAYOUT_AUTO_DETECT);    // CUBEMAP_LAYOUT_PANORAMA
        UnloadImage(img);

        // Load player ship model & texture
        ShipModel = LoadModel("resources/models/player_ship/spaceship.obj");
        Texture2D shipTexture = LoadTexture("resources/models/player_ship/ShipTextureDiffuse.png");
        ShipModel.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = shipTexture;
        ShipBoxLocal = GetModelBoundingBox(ShipModel);

        // Load asteroid model & texture
        AsteroidModel = LoadModel("resources/models/asteroid/asteroid.obj");
        Texture2D asteroidTexture = LoadTexture("resources/models/asteroid/AsteroidTextureDiffuse.png");
        AsteroidModel.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = asteroidTexture;
        AsteroidBoxLocal = GetModelBoundingBox(AsteroidModel);
    }

    void Draw(Model model, const Vector3& position, const Matrix& rotation)
    {
        // Rotate model off pitch, yaw, and roll
        model.transform = rotation;
        DrawModel(model, position, 1.0f, WHITE);
    }

    void DrawSkybox()
    {
        // flip box and draw skybox
        rlDisableBackfaceCulling();
        rlDisableDepthMask();
            DrawModel(Models::Skybox, (Vector3){ 0.0f, 0.0f, 0.0f }, 1.0f, WHITE);
        rlEnableBackfaceCulling();
        rlEnableDepthMask();
    }

    BoundingBox GetWorldBoundingBox(BoundingBox localBox, Vector3 position, Matrix rotation)
    {
        Vector3 corners[8] = {
            { localBox.min.x, localBox.min.y, localBox.min.z },
            { localBox.max.x, localBox.min.y, localBox.min.z },
            { localBox.min.x, localBox.max.y, localBox.min.z },
            { localBox.max.x, localBox.max.y, localBox.min.z },
            { localBox.min.x, localBox.min.y, localBox.max.z },
            { localBox.max.x, localBox.min.y, localBox.max.z },
            { localBox.min.x, localBox.max.y, localBox.max.z },
            { localBox.max.x, localBox.max.y, localBox.max.z }
        };

        BoundingBox result;
        result.min = (Vector3){ FLT_MAX, FLT_MAX, FLT_MAX };
        result.max = (Vector3){ -FLT_MAX, -FLT_MAX, -FLT_MAX };

        for (int i = 0; i < 8; i++)
        {
            // Apply rotation then translate to position
            Vector3 worldCorner = Vector3Transform(corners[i], rotation);
            worldCorner = Vector3Add(worldCorner, position);

            result.min = Vector3Min(result.min, worldCorner);
            result.max = Vector3Max(result.max, worldCorner);
        }

        return result;
    }
}