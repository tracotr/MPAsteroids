#include "include/Player.h"
#include "include/Models.h"




void Player::Update()
{   
    // Rotation controls
    // ----------------------
    // Pitch
    if (IsKeyDown(KEY_W)) Rotation = MatrixMultiply(MatrixRotateX(RotationSpeed), Rotation);
    if (IsKeyDown(KEY_S)) Rotation = MatrixMultiply(MatrixRotateX(-RotationSpeed), Rotation);

    // Yaw
    if (IsKeyDown(KEY_A)) Rotation = MatrixMultiply(MatrixRotateY(RotationSpeed), Rotation);
    if (IsKeyDown(KEY_D)) Rotation = MatrixMultiply(MatrixRotateY(-RotationSpeed), Rotation);

    // Roll
    if (IsKeyDown(KEY_Q)) Rotation = MatrixMultiply(MatrixRotateZ(RotationSpeed), Rotation);
    if (IsKeyDown(KEY_E)) Rotation = MatrixMultiply(MatrixRotateZ(-RotationSpeed), Rotation);


    // Movement Controls
    // ----------------------
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

    // Apply friction to slow ship
	Vector3 friction = Vector3Scale(Vector3Normalize(Velocity), -2.5f * GetFrameTime());

    // Apply friction to our velocity, and eventually stop ship
	if (Vector3LengthSqr(friction) < Vector3LengthSqr(Velocity))
        Velocity = Vector3Add(Velocity, friction);
	else	
        Velocity = { 0.0f, 0.0f, 0.0f };


    // Update position based off velocity
    Position = Vector3Add(Position, Vector3Scale(Velocity, GetFrameTime()));

    if (IsKeyDown(KEY_Y)) Reset();
}

void Player::Draw() const
{
    Models::Draw(Models::ShipModel, this->Position, this->Rotation);
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
}