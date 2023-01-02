#pragma once

#include "../../External/ImGui/imgui.h"
#include "../../External/ImGui/imgui_internal.h"

namespace Physics
{
	typedef ImVec2 Vector2;
	typedef ImRect Rectangle;

	inline float Distance(Vector2 me, Vector2 other)
	{
		return sqrtf(powf(me.x - other.x, 2) + powf(me.y - other.y, 2));
	}

	struct MotionEquation
	{
		float x_multiplier = 1;
		float y_multiplier = 1;

		float x_offset = 0;
		float y_offset = 0;

		MotionEquation(float xMulti, float yMulti)
		{
			x_multiplier = xMulti;
			y_multiplier = yMulti;
		};

		MotionEquation(float xMulti, float yMulti, float xOffset, float yOffset)
		{
			x_multiplier = xMulti;
			y_multiplier = yMulti;

			x_offset = xOffset;
			y_offset = yOffset;
		};

		MotionEquation()
		{

		}
	};

	struct CircleEquation
	{
		Vector2 pos;
		float radius = 0;

		CircleEquation(float _x, float _y, float _r)
		{
			pos.x = _x;
			pos.y = _y;
			radius = _r;
		}

		CircleEquation(Vector2 _pos, float _r)
		{
			pos = _pos;
			radius = _r;
		}

		CircleEquation()
		{

		}
	};

	struct PhysicsObject
	{
		MotionEquation me;
		float timeCreated = 0;

		float buffered_collision_time = -FLT_MAX;

		float initialVelocity = 1.f;

		bool is_selected = false;
		// bool deleted = false; // Moze czasteczki powinny zawierac taki parametr, aby unikac usuwanie wielu czasteczek osobno
		// nie zwiekszyl by on rozmiaru struktury (przez padding) ale znaczaco przyspieszyl by sytuacje, w ktorej granice sa wylaczone

		PhysicsObject(MotionEquation motion_eq, float timeAdded, float Vo = 1.f)
		{
			me = motion_eq;
			timeCreated = timeAdded;
			initialVelocity = Vo;
		}
	};

	struct PhysicsObstacle
	{
		Rectangle rect;
		bool is_selected = false;

		PhysicsObstacle(Rectangle _rect)
		{
			rect = _rect;
		}
	};

	struct PhysicsCircle
	{
		CircleEquation circleEq;

		Rectangle m_bounds;

		bool is_selected = false;

		PhysicsCircle(CircleEquation _circleEq)
		{
			circleEq = _circleEq;

			m_bounds.Min.x = _circleEq.pos.x - (_circleEq.radius);
			m_bounds.Min.y = _circleEq.pos.y - (_circleEq.radius);

			m_bounds.Max.x = _circleEq.pos.x + (_circleEq.radius);
			m_bounds.Max.y = _circleEq.pos.y + (_circleEq.radius);
		}
	};

	enum CollisionSide {
		ColSide_None = 0,
		ColSide_Top = 1,
		ColSide_Bottom = 2,
		ColSide_Left = 4,
		ColSide_Right = 8,
		ColSide_H_Middle = 16,
		ColSide_V_Middle = 32,
	};

	enum ObjType {
		Obj_Particle = 1,
		Obj_Rectangle = 2,
		Obj_Circle = 4,
	};
}