#include "include/Models.h"

// WebGL1 needs GLSL ES 100 shaders; the glsl/ set is desktop GLSL 330.
#if defined(PLATFORM_WEB)
    #define SHADER_DIR "resources/shaders/glsl100"
#else
    #define SHADER_DIR "resources/shaders/glsl"
#endif

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
        Skybox.materials[0].shader = LoadShader(SHADER_DIR "/skybox.vs", SHADER_DIR "/skybox.fs");

        int cubemapMapIndex = MATERIAL_MAP_CUBEMAP;
        int gammaOff = 0;
        int flipOff = 0;
        SetShaderValue(Skybox.materials[0].shader, GetShaderLocation(Skybox.materials[0].shader, "environmentMap"), &cubemapMapIndex, SHADER_UNIFORM_INT);
        SetShaderValue(Skybox.materials[0].shader, GetShaderLocation(Skybox.materials[0].shader, "doGamma"), &gammaOff, SHADER_UNIFORM_INT);
        SetShaderValue(Skybox.materials[0].shader, GetShaderLocation(Skybox.materials[0].shader, "vflipped"), &flipOff, SHADER_UNIFORM_INT);

        Shader shdrCubemap = LoadShader(SHADER_DIR "/cubemap.vs", SHADER_DIR "/cubemap.fs");
        int equirectangularOff = 0;
        SetShaderValue(shdrCubemap, GetShaderLocation(shdrCubemap, "equirectangularMap"), &equirectangularOff, SHADER_UNIFORM_INT);

        Image img = LoadImage("resources/skybox/StarrySky.png");
        Skybox.materials[0].maps[MATERIAL_MAP_CUBEMAP].texture = LoadTextureCubemap(img, CUBEMAP_LAYOUT_AUTO_DETECT);    // CUBEMAP_LAYOUT_PANORAMA
        UnloadImage(img);

        // Load player ship model & texture
        ShipModel = LoadModel("resources/models/player_ship/spaceship.obj");
        Texture2D shipTexture = LoadTexture("resources/models/player_ship/ShipTextureDiffuse.png");
        ShipModel.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = shipTexture;
        ShipBoxLocal = GetModelBoundingBox(ShipModel);
        GenTextureMipmaps(&shipTexture);
        SetTextureFilter(shipTexture, TEXTURE_FILTER_POINT);

        // Load asteroid model & texture
        AsteroidModel = LoadModel("resources/models/asteroid/asteroid.obj");
        Texture2D asteroidTexture = LoadTexture("resources/models/asteroid/AsteroidTextureDiffuse.png");
        AsteroidModel.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = asteroidTexture;
        AsteroidBoxLocal = GetModelBoundingBox(AsteroidModel);
        GenTextureMipmaps(&asteroidTexture);
        SetTextureFilter(asteroidTexture, TEXTURE_FILTER_POINT);
    }

    void DrawModel(Model model, const Vector3& position, const Matrix& rotation, float scale)
    {
        // Rotate model off pitch, yaw, and roll
        model.transform = rotation;
        DrawModel(model, position, scale, WHITE);
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
    
    void DrawUI(Camera camera, Vector3 velocity, Vector3 position, int id, int (&scoreboard)[MAX_PLAYERS], char (&names)[MAX_PLAYERS][MAX_PLAYER_NAME_LENGTH])
    {
        // Rank only the players actually in the session; with MAX_PLAYERS scaled
        // up for a public server, listing every empty slot would fill the screen.
        int ranked[MAX_PLAYERS];
        int rankedCount = 0;
        for (int i = 0; i < MAX_PLAYERS; i++)
        {
            if (names[i][0] != '\0')
                ranked[rankedCount++] = i;
        }

        for (int i = 1; i < rankedCount; i++)
        {
            int current = ranked[i];
            int j = i - 1;
            while (j >= 0 && scoreboard[ranked[j]] < scoreboard[current])
            {
                ranked[j + 1] = ranked[j];
                j--;
            }
            ranked[j + 1] = current;
        }

        const int panelX = GetScreenWidth() - 180;
        DrawText(TextFormat("Leaderboard (%i)", rankedCount), panelX, 20, 20, RAYWHITE);

        int visibleRows = rankedCount < LEADERBOARD_VISIBLE_ROWS ? rankedCount : LEADERBOARD_VISIBLE_ROWS;
        int localRank = -1;

        for (int row = 0; row < visibleRows; row++)
        {
            int playerId = ranked[row];
            if (playerId == id) localRank = row;

            Color rowColor = (playerId == id) ? GREEN : RAYWHITE;
            DrawText(TextFormat("%i. %s: %i", row + 1, names[playerId], scoreboard[playerId]),
                     panelX, 45 + 20 * row, 18, rowColor);
        }

        // Keep the local player visible even when they're outside the top rows.
        if (localRank == -1)
        {
            for (int row = visibleRows; row < rankedCount; row++)
            {
                if (ranked[row] != id) continue;

                DrawText("...", panelX, 45 + 20 * visibleRows, 18, DARKGRAY);
                DrawText(TextFormat("%i. %s: %i", row + 1, names[id], scoreboard[id]),
                         panelX, 45 + 20 * (visibleRows + 1), 18, GREEN);
                break;
            }
        }
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