#include "include/Player.h"
#include "include/Models.h"
#include "include/Sounds.h"
#include "include/GameApp.h"
#include <iostream>

void Player::Update(double delta)
{   
    bool thrusting = IsKeyDown(KEY_R) || IsKeyDown(KEY_F);
    if (thrusting && !boosterActive)
    {
        boosterActive = true;
        Sounds::PlayBooster(Position, Position); 
    }
    else if (!thrusting && boosterActive)
    {
        boosterActive = false;
        Sounds::StopBooster();
    }

    laserCooldown += delta;
    laserCooldown = Clamp(laserCooldown, 0.0, LASER_COOLDOWN_DURATION);

    isFiring = false;
    if (IsKeyDown(KEY_SPACE))
    {
        if(laserCooldown >= LASER_COOLDOWN_DURATION)
        {
            laserCooldown = 0;
            Sounds::PlayLaser(Position, Position);
            isFiring = true;
        }
    }
    
    float currentRotationSpeed = RotationSpeed;

    // Our ship rotates slower when holding down the shift key
    if (IsKeyDown(KEY_LEFT_SHIFT))
    {
        currentRotationSpeed = slowRotationSpeed;
    }

    if (IsKeyDown(KEY_W)) Rotation = MatrixMultiply(MatrixRotateX(currentRotationSpeed), Rotation);
    if (IsKeyDown(KEY_S)) Rotation = MatrixMultiply(MatrixRotateX(-currentRotationSpeed), Rotation);

    if (IsKeyDown(KEY_A)) Rotation = MatrixMultiply(MatrixRotateY(currentRotationSpeed), Rotation);
    if (IsKeyDown(KEY_D)) Rotation = MatrixMultiply(MatrixRotateY(-currentRotationSpeed), Rotation);

    if (IsKeyDown(KEY_Q)) Rotation = MatrixMultiply(MatrixRotateZ(currentRotationSpeed), Rotation);
    if (IsKeyDown(KEY_E)) Rotation = MatrixMultiply(MatrixRotateZ(-currentRotationSpeed), Rotation);

    if (IsKeyDown(KEY_R) && Vector3LengthSqr(Velocity) <= MaxSpeed)
    {
        Vector3 forward = Vector3Transform(Forward, Rotation);
        Velocity = Vector3Add(Velocity, forward);
    }

    if (IsKeyDown(KEY_F) && Vector3LengthSqr(Velocity) <= MaxSpeed / 2)
    {
        Vector3 forward = Vector3Transform(Backward, Rotation);
        Velocity = Vector3Add(Velocity, forward);
    }

	Vector3 friction = Vector3Scale(Vector3Normalize(Velocity), -2.5f * (float)delta);

	if (Vector3LengthSqr(friction) < Vector3LengthSqr(Velocity))
        Velocity = Vector3Add(Velocity, friction);
	else	
        Velocity = { 0.0f, 0.0f, 0.0f };

    Position = Vector3Add(Position, Vector3Scale(Velocity, (float)delta));

    if (IsKeyDown(KEY_Y)) Reset();
}

void Player::Draw()
{
    Models::DrawModel(Models::ShipModel, this->Position, this->Rotation);
}

void Player::Reset()
{
    Alive = true;
    Respawn();
}

void Player::Respawn()
{
    Position = Vector3{ 0.0f, 0.0f, 0.0f };
    Rotation = MatrixIdentity();
    Velocity = Vector3{ 0.0f, 0.0f, 0.0f };
    boosterActive = false;
    Sounds::StopBooster();
}

Vector3 Player::GetForwardVector()
{
    return Vector3Normalize(Vector3Transform((Vector3){ 0, 0, -1 }, this->Rotation));
}