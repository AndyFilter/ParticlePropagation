#pragma once

#include <cstdlib>
#include <vector>
#include <thread>
#include <atomic>

#include "physics_definitions.h"

#define RAD_2_DEG 57.295779513f
#define M_PI (3.1415927f)

#define MULTI_THREADED // Bez tego program sie NIE skompiluje
//#define parallel_multi_threaded

namespace Physics
{
	bool Setup(void(*OnStep)(), std::atomic<bool>& isReading, std::atomic<uint64_t> &sim_steps);

	//Vector2 GetObjectPosition(MotionEquation& me, unsigned long long time);
	Vector2 GetObjectPosition(PhysicsObject& physObj);

	//void RequestPositionsUpdate();

	void StepSimulation(float time);

	bool AddObstacle(PhysicsObstacle obstacle);
	bool AddObstacle(PhysicsCircle circle);

	bool RemoveCircle(size_t idx);
	bool RemoveObstacle(size_t idx);

	bool Clear();
	bool Clear(ObjType);

	inline double GetRandomValueInRange(float min, float max)
	{
		//return fmod(rand(), (double)(max - min)) + min;
		return (((float)rand() / RAND_MAX) * (max - min)) + min;
	}

	inline double GetRandomAngle()
	{
		//return fmod(rand(), (double)M_PI * 2) - M_PI;
		return GetRandomValueInRange(-M_PI, M_PI);
	}

	inline bool IsPointInsideRect(Vector2 p, Rectangle rect)
	{
		// General equation:
		//return (p.x > rect.Min.x && p.x < rect.Max.x&& p.y > rect.Min.y && p.y < rect.Max.y);

		if (p.x <= rect.Min.x)
			return false;

		if (p.x >= rect.Max.x)
			return false;

		if (p.y <= rect.Min.y)
			return false;

		if (p.y >= rect.Max.y)
			return false;

		return true;
		// Both of these ways whould be equally fast with -O2 argument
	}

	inline bool IsPointInsideRect(Vector2 &p, Rectangle rect, float margin)
	{
		//return (p.x > (rect.Min.x - margin) && p.x < (rect.Max.x + margin) && p.y > (rect.Min.y - margin) && p.y < (rect.Max.y + margin));

		if (p.x <= (rect.Min.x - margin))
			return false;

		if (p.x >= (rect.Max.x + margin))
			return false;

		if (p.y <= (rect.Min.y - margin))
			return false;

		if (p.y >= (rect.Max.y + margin))
			return false;

		return true;
	}

	inline bool IsPointInsideRect_ref(Vector2& p, Rectangle &rect, float margin)
	{
		//return (p.x > (rect.Min.x - margin) && p.x < (rect.Max.x + margin) && p.y > (rect.Min.y - margin) && p.y < (rect.Max.y + margin));

		if (p.x <= (rect.Min.x - margin))
			return false;

		if (p.x >= (rect.Max.x + margin))
			return false;

		if (p.y <= (rect.Min.y - margin))
			return false;

		if (p.y >= (rect.Max.y + margin))
			return false;

		return true;
	}

	inline bool IsPointInsideCircle(Vector2 p, CircleEquation circle)
	{
		return (Distance(p, circle.pos) <= circle.radius);

		//return false;
	}
	inline bool IsPointInsideCircle(Vector2 &p, CircleEquation circle, float margin)
	{
		return (Distance(p, circle.pos) <= (circle.radius + margin));
	}

	inline bool IsPointInsideCircle(Vector2 &p, PhysicsCircle &circle, float margin)
	{
		return IsPointInsideRect_ref(p, circle.m_bounds, margin) ? (Distance(p, circle.circleEq.pos) <= (circle.circleEq.radius + margin)) : 0;
	}

	inline Vector2 RotatePointAroundPoint(Vector2 rootPoint, Vector2 targetPoint, float angle)
	{
		float a1 = sinf(angle);
		float a2 = cosf(angle);

		targetPoint.x -= rootPoint.x;
		targetPoint.y -= rootPoint.y;

		auto newX = (targetPoint.x * a2 - (targetPoint.y * a1)) + rootPoint.x;
		float newY = (targetPoint.x * a1 + (targetPoint.y * a2)) + rootPoint.y;

		return { newX, newY };
	}

	inline float simulationSpeed = 1;
	inline bool isSimPlaying;
	inline bool isSimPaused = false; // True dla krotkiej przerwy, np. podczas przesuwania okna
	inline double simulationTime = 0;
	inline bool bufferCollisions = true;

	inline std::atomic<bool> was_simulation_changed = false; // To decycuje o tym, czy trzeba jeszcze raz obliczyc kolizje w przyszlosc
	inline std::atomic<bool> was_size_changed = false;

	inline Vector2 boundsExtend;
	inline bool infiniteBounds = false;
	inline std::atomic<float> collisionEnergyLoss;

	//inline std::vector<Vector2> particlesPositions;

	inline std::vector<PhysicsObject> particles;
	inline std::vector<PhysicsObstacle> obstacles;
	inline std::vector<PhysicsCircle> circles;


#ifdef MULTI_THREADED
	inline std::thread physicsThread;
	inline std::atomic<bool> is_accessing;
	inline std::atomic<bool> delete_intent;
	inline std::atomic<long long> last_step;
#endif

}