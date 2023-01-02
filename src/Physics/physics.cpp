#include <iostream>

#include "physics.h"
#include <algorithm>

#define ROOTS_PRECISION 0.001f
#define TIME_MULTIPLIER 10000UL

using namespace Physics;

const int margin_bounds = 0, margin_obstacles = 0;

bool wasSimPausedLastStep = false;

void threadLoop(void(*OnStep)(), std::atomic<bool>& isReading, std::atomic<std::uint64_t>& sim_steps)
{
	using namespace std::chrono_literals;
	std::chrono::high_resolution_clock::time_point lastPhysicsStep;
	while (true)
	{
		if (!(isReading))
		{
			auto timeNow = std::chrono::high_resolution_clock::now();

			auto simStep = std::chrono::duration_cast<std::chrono::nanoseconds>(timeNow - lastPhysicsStep).count();
			last_step = simStep;
			StepSimulation(simStep/1000.f);
			sim_steps++;

			lastPhysicsStep = timeNow;

			if (!isSimPlaying)
				std::this_thread::sleep_for(1ms);
		}
	}
}

bool Physics::Setup(void(*OnStep)(), std::atomic<bool>& isReading, std::atomic<uint64_t>& sim_steps)
{
	physicsThread = std::thread(threadLoop, std::ref(OnStep), std::ref(isReading), std::ref(sim_steps));
	physicsThread.detach();
	return true;
}

void Physics::StepSimulation(float timeDelta)
{
	if (!isSimPaused && isSimPlaying)
		simulationTime += (timeDelta * simulationSpeed);
	else
		return;

	if (particles.size() == 0)
		return;

	static bool _last_should_buffer_collisions{ false };
	static int _last_time_sign = 1;

	int timeSign = simulationSpeed >= 0 ? 1 : -1;

	if (delete_intent)
	{
		particles.erase(std::remove_if(particles.begin(), particles.end(),
			[](const PhysicsObject& obj) {
				return (obj.is_selected);
			}
		), particles.end());

		circles.erase(std::remove_if(circles.begin(), circles.end(),
			[](const PhysicsCircle& obj) {
				return (obj.is_selected);
			}
		), circles.end());

		obstacles.erase(std::remove_if(obstacles.begin(), obstacles.end(),
			[](const PhysicsObstacle& obj) {
				return (obj.is_selected);
			}
		), obstacles.end());

		delete_intent = false;
	}

	long long obstaclesNum = obstacles.size() - 1;

	bool size_changed = was_size_changed;
	bool sim_changed = was_simulation_changed;
	bool time_sign_changed = false;

	if (timeSign != _last_time_sign) {
		time_sign_changed = true;
		sim_changed = true;
	}

	float energy_conservation = 1 - (collisionEnergyLoss.load() / 100.f);

	bool now_buffer_collisions = bufferCollisions; // kopiujemy wartosc, gdyz jest ona globalna i moze zostac mnieniona przez inny watek w czasie pracy

	auto rebuffer_changed = (!_last_should_buffer_collisions && now_buffer_collisions);

	for (long long particle_index = particles.size()-1; particle_index >= 0; --particle_index)
	{
		PhysicsObject& physObj = particles[particle_index];

		bool should_rebuffer_collisions = rebuffer_changed;

	Recalculate_Collision: // Bardzo prosze mnie nie bic za uzywanie goto. Uzycie go w tej sytuacji jest uzasadnione i poprawia czytolnosc kodu.

		auto time = simulationTime - physObj.timeCreated;

		if (now_buffer_collisions && ((physObj.buffered_collision_time >= (time * timeSign)) && !should_rebuffer_collisions && !sim_changed))
			continue;

		if ((_last_should_buffer_collisions && !now_buffer_collisions) || should_rebuffer_collisions || (sim_changed && !time_sign_changed))
			physObj.buffered_collision_time = FLT_MAX * timeSign;

		if (now_buffer_collisions && time_sign_changed)
			physObj.buffered_collision_time = 0;

		if (now_buffer_collisions && (fabs(physObj.buffered_collision_time) == (FLT_MAX * timeSign))) {
			should_rebuffer_collisions = true;
			physObj.buffered_collision_time = FLT_MAX * timeSign;
		}

		MotionEquation& me = physObj.me;
		MotionEquation me_copy = me;
		float adjusted_time = (float)(time / TIME_MULTIPLIER);

		Vector2 localPos;
		localPos.x = (me_copy.x_multiplier * adjusted_time) + me_copy.x_offset;
		localPos.y = (me_copy.y_multiplier * adjusted_time) + me_copy.y_offset;
		//y = a * adjusted_time + c => adjusted_time = (y - c)/a

		bool isOutside = !IsPointInsideRect(localPos, { {0,0}, boundsExtend }, margin_bounds);

		bool is_x_mult_small = fabs(me_copy.x_multiplier) < ROOTS_PRECISION;

		// wzkaznik kierunkwoy prostej
		float a = me_copy.y_multiplier / (is_x_mult_small ? ROOTS_PRECISION : me_copy.x_multiplier);
		// offset prostej
		float c = (-me_copy.x_offset * a) + me_copy.y_offset;

		// Buforowanie kolizji
		if (should_rebuffer_collisions)
		{
			CollisionSide colSide;
			if (me_copy.x_multiplier * timeSign <= 0) { // Ruch w lewo
				if (me_copy.y_multiplier * timeSign <= 0) { // Ruch w gore
					float y1 = a * margin_bounds + c;
					if (y1 >= margin_bounds)
						colSide = ColSide_Left;
					else
						colSide = ColSide_Top;
				}
				else {
					float y1 = a * margin_bounds + c;
					if (y1 <= (boundsExtend.y - margin_bounds))
						colSide = ColSide_Left;
					else
						colSide = ColSide_Bottom;
				}
			}
			else { // Ruch w prawo
				if (me_copy.y_multiplier * timeSign >= 0) { // Ruch w dol
					float y1 = a * (boundsExtend.x - margin_bounds) + c;
					if (y1 <= (boundsExtend.y - margin_bounds))
						colSide = ColSide_Right;
					else
						colSide = ColSide_Bottom;
				}
				else {
					float y1 = a * (boundsExtend.x - margin_bounds) + c;
					if (y1 >= margin_bounds)
						colSide = ColSide_Right;
					else
						colSide = ColSide_Top;
				}
			}

			float fixed_x_mult = is_x_mult_small ? ROOTS_PRECISION : me.x_multiplier;
			float fixed_y_mult = (fabs(me_copy.y_multiplier) < ROOTS_PRECISION) ? ROOTS_PRECISION : me_copy.y_multiplier;

			float col_time = FLT_MAX;

			switch (colSide)
			{
			case ColSide_Left:
				col_time = (margin_bounds - me_copy.x_offset) / fixed_x_mult;
				break;
			case ColSide_Top:
				col_time = (margin_bounds - me_copy.y_offset) / fixed_y_mult;
				break;
			case ColSide_Right:
				col_time = ((boundsExtend.x - margin_bounds) - me_copy.x_offset) / fixed_x_mult;
				break;
			case ColSide_Bottom:
				col_time = ((boundsExtend.y - margin_bounds) - me_copy.y_offset) / fixed_y_mult;
				break;
			}

			if(timeSign > 0)
				physObj.buffered_collision_time = fminf(col_time * TIME_MULTIPLIER, physObj.buffered_collision_time);
			else
				physObj.buffered_collision_time = fmaxf(col_time * TIME_MULTIPLIER, physObj.buffered_collision_time);
			//printf("collision time: %f (%d)\n", col_time + (physObj.timeCreated / 10000), colSide);

			//continue;
		}

		//printf("adjusted time: %f\n", adjusted_time);
		if (isOutside && !should_rebuffer_collisions)
		{
			if (infiniteBounds) {
				particles.erase(particles.begin() + particle_index);
				particle_index--;
				continue;
			}

			if (bool wasLeft = (localPos.x <= margin_bounds); wasLeft || (localPos.x + margin_bounds) >= boundsExtend.x)
			{

				me.x_multiplier *= -1 * energy_conservation;
				me.y_multiplier *= energy_conservation;
				physObj.initialVelocity *= energy_conservation;
				me.x_offset = wasLeft ? margin_bounds : (boundsExtend.x - margin_bounds);
				me.y_offset = wasLeft ? (a * margin_bounds + c) : (a * (boundsExtend.x - margin_bounds) + c);
				//me.x_offset = (localPos.x <= margin_bounds ? margin_bounds : boundsExtend.x - margin_bounds);//+ (localPos.x <= margin_bounds ? 1 : -1);
				//me.y_offset = localPos.y;
				physObj.timeCreated = simulationTime;
			}

			if (bool wasTop = (localPos.y <= margin_bounds); wasTop || (localPos.y + margin_bounds) >= boundsExtend.y)
			{

				me.y_multiplier *= -1 * energy_conservation;
				me.x_multiplier *= energy_conservation;
				physObj.initialVelocity *= energy_conservation;
				me.x_offset = wasTop ? ((margin_bounds - c) / a) : ((boundsExtend.y - margin_bounds - c) / a);
				me.y_offset = wasTop ? margin_bounds : (boundsExtend.y - margin_bounds);
				//me.x_offset = localPos.x;
				//me.y_offset = (localPos.y <= margin_bounds ? margin_bounds : boundsExtend.y - margin_bounds);
				physObj.timeCreated = simulationTime;
			}

			if (bufferCollisions) {
				should_rebuffer_collisions = true;
				goto Recalculate_Collision;
			}
			continue;
		}

		for (long long obsi = obstaclesNum; obsi >= 0; --obsi)
		{
			PhysicsObstacle curObstacle = Physics::obstacles[obsi];

			curObstacle.rect.Min.x -= margin_obstacles;
			curObstacle.rect.Min.y -= margin_obstacles;
			curObstacle.rect.Max.x += margin_obstacles;
			curObstacle.rect.Max.y += margin_obstacles;

			//bool is_inside_x = (localPos.x + margin_obstacles) >= curObstacle.rect.Min.x && (localPos.x - margin_obstacles) <= curObstacle.rect.Max.x;

			if (!should_rebuffer_collisions)
			{
				if ((localPos.x) < curObstacle.rect.Min.x)
					continue;

				if ((localPos.x) > curObstacle.rect.Max.x)
					continue;

				//bool is_inside_y = (localPos.y + margin_obstacles) >= curObstacle.rect.Min.y && (localPos.y - margin_obstacles) <= curObstacle.rect.Max.y;

				if ((localPos.y) < curObstacle.rect.Min.y)
					continue;

				if ((localPos.y) > curObstacle.rect.Max.y)
					continue;
			}


			// miejsce (x) styku 2 prostych (y = ax + c, oraz y = bx + d) to (d - c)/(a - b). Zatem y = (a - b) * (d - c)

			Vector2 hitPoint{};
			CollisionSide colSide = ColSide_None; // Problem: Niezaleznie od tego, czy linia intersectuje obstacle to i tak bedzie ustawiona jakas wartosc.
			if (me_copy.x_multiplier * timeSign >= 0) {
				if (me_copy.y_multiplier * timeSign >= 0) { // Z lewego gornego rogu
					float y1 = a * curObstacle.rect.Min.x + c;
					if (y1 >= curObstacle.rect.Min.y && y1 <= curObstacle.rect.Max.y) {
						hitPoint = { curObstacle.rect.Min.x, y1 };
						colSide = ColSide_Left;
					}
					else {
						float x1 = (curObstacle.rect.Min.y - c) / a;
						if (x1 <= curObstacle.rect.Max.x && x1 >= curObstacle.rect.Min.x) {
							hitPoint = { x1, curObstacle.rect.Min.y };
							colSide = ColSide_Top;
						}
					}
				}
				else { // Z lewego dolnego rogu
					float y1 = a * curObstacle.rect.Min.x + c;
					if (y1 >= curObstacle.rect.Min.y && y1 <= curObstacle.rect.Max.y) {
						hitPoint = { curObstacle.rect.Min.x, y1 };
						colSide = ColSide_Left;
					}
					else {
						float x1 = (curObstacle.rect.Max.y - c) / a;
						if (x1 <= curObstacle.rect.Max.x && x1 >= curObstacle.rect.Min.x) {
							hitPoint = { x1, curObstacle.rect.Max.y };
							colSide = ColSide_Bottom;
						}
					}
				}
			}
			else {
				if (me_copy.y_multiplier * timeSign < 0) { // Z prawego dolnego rogu
					float y1 = a * curObstacle.rect.Max.x + c;
					if (y1 >= curObstacle.rect.Min.y && y1 <= curObstacle.rect.Max.y) {
						hitPoint = { curObstacle.rect.Max.x, y1 };
						colSide = ColSide_Right;
					}
					else {
						float x1 = (curObstacle.rect.Max.y - c) / a;
						if (x1 <= curObstacle.rect.Max.x && x1 >= curObstacle.rect.Min.x) {
							hitPoint = { x1, curObstacle.rect.Max.y };
							colSide = ColSide_Bottom;
						}
					}
				}
				else { // Z prawego gornego rogu
					float y1 = a * curObstacle.rect.Max.x + c;
					if (y1 >= curObstacle.rect.Min.y && y1 <= curObstacle.rect.Max.y) {
						hitPoint = { curObstacle.rect.Max.x, y1 };
						colSide = ColSide_Right;
					}
					else {
						float x1 = (curObstacle.rect.Min.y - c) / a;
						if (x1 <= curObstacle.rect.Max.x && x1 >= curObstacle.rect.Min.x) {
							hitPoint = { x1, curObstacle.rect.Min.y };
							colSide = ColSide_Top;
						}
					}
				}
			}
			//printf("Hit Point: (%f, %f), %d\n", hitPoint.x, hitPoint.y, colSide);
			//printf("local pos: (%f, %f)\n", localPos.x, localPos.y);
			//
			//if (me.x_multiplier >= 0) {
			//	if (me.y_multiplier >= 0) {
			//		if()
			//	}
			//}
			//
			//bool is_inside_x = margin_obstacles >= -x_min_dist && -margin_obstacles <= -x_max_dist;
			//bool is_inside_y = margin_obstacles >= -y_min_dist && -margin_obstacles <= -y_max_dist;
			//
			//bool isInside = IsPointInsideRect(localPos, curObstacle.rect);
			//
			//float x_max_dist = localPos.x - curObstacle.rect.Max.x;
			//float x_min_dist = localPos.x - curObstacle.rect.Min.x;
			//
			//float y_max_dist = localPos.y - curObstacle.rect.Max.y;
			//float y_min_dist = localPos.y - curObstacle.rect.Min.y;
			//
			//bool is_horizontal_closer = false;
			//if (fabs(x_max_dist) < fabs(x_min_dist))
			//{
			//	if (fabs(x_max_dist) < fabs(y_max_dist) && fabs(x_max_dist) < fabs(y_min_dist))
			//		is_horizontal_closer = true;
			//}
			//else if (fabs(x_min_dist) < fabs(y_max_dist) && fabs(x_min_dist) < fabs(y_min_dist))
			//	is_horizontal_closer = true;
			//
			//float y1 = a * curObstacle.rect.Min.x + c;
			//float y2 = a * curObstacle.rect.Max.x + c;
			//
			//float x1 = (curObstacle.rect.Min.y - c) / a;
			//float x2 = (curObstacle.rect.Max.y - c) / a;
			//
			//printf("HP1 = (%f, %f)\n", curObstacle.rect.Min.x, y1);
			//printf("HP2 = (%f, %f)\n", curObstacle.rect.Max.x, y2);
			//printf("HP3 = (%f, %f)\n", x1, curObstacle.rect.Min.y);
			//printf("HP4 = (%f, %f)\n", x2, curObstacle.rect.Max.y);
			//
			//
			//int half_width = _obstacles_size_cache[(obstaclesNum - obsi) * 2];//(int)curObstacle.rect.GetWidth() / 2;
			//int half_height = _obstacles_size_cache[(obstaclesNum - obsi) * 2 + 1];//(int)curObstacle.rect.GetHeight() / 2;

			int horizontal_collision = (colSide & ColSide_Left) ? -1 : (colSide & ColSide_Right) ? 1 : 0;
			int vertical_collision = (colSide & ColSide_Top) ? -1 : (colSide & ColSide_Bottom) ? 1 : 0;
			//int horizontal_collision = is_horizontal_closer ? ((x_max_dist <= margin_obstacles && x_max_dist > -half_width) ? -1 : (x_min_dist >= -margin_obstacles && x_min_dist < half_width)) : 0;
			//int vertical_collision = !is_horizontal_closer ? ((y_max_dist <= margin_obstacles && y_max_dist > -half_height) ? -1 : (y_min_dist >= -margin_obstacles && y_min_dist < half_height)) : 0;

			if (!horizontal_collision && !vertical_collision)
				continue;

			// Buforowanie kolizji
			if (should_rebuffer_collisions)
			{
				float fixed_x_mult = is_x_mult_small ? ROOTS_PRECISION : me_copy.x_multiplier;
				float fixed_y_mult = (fabs(me_copy.y_multiplier) < ROOTS_PRECISION) ? ROOTS_PRECISION : me_copy.y_multiplier;

				float col_time = FLT_MAX;

				switch (colSide)
				{
				case ColSide_Left:
					col_time = (curObstacle.rect.Min.x - me_copy.x_offset) / fixed_x_mult;
					break;
				case ColSide_Top:
					col_time = (curObstacle.rect.Min.y - me_copy.y_offset) / fixed_y_mult;
					break;
				case ColSide_Right:
					col_time = (curObstacle.rect.Max.x - me_copy.x_offset) / fixed_x_mult;
					break;
				case ColSide_Bottom:
					col_time = (curObstacle.rect.Max.y - me_copy.y_offset) / fixed_y_mult;
					break;
				}

				if (col_time * timeSign < 0)
					continue;

				if (timeSign > 0)
					physObj.buffered_collision_time = fminf(col_time * TIME_MULTIPLIER, physObj.buffered_collision_time);
				else
					physObj.buffered_collision_time = fmaxf(col_time * TIME_MULTIPLIER, physObj.buffered_collision_time);
				//printf("collision time: %f (%d)\n", col_time + (physObj.timeCreated/10000), colSide);
				//recalculate_buffered_col = true;
				//particle_index++;
				continue;
			}

			me.x_offset = hitPoint.x;
			me.y_offset = hitPoint.y;
			physObj.timeCreated = simulationTime;
			me.x_multiplier *= (horizontal_collision ? -1 : 1) * energy_conservation;
			me.y_multiplier *= (vertical_collision ? -1 : 1) * energy_conservation;
			physObj.initialVelocity *= energy_conservation;

			//printf("obstacle hit\n");

			if (bufferCollisions) {
				should_rebuffer_collisions = true;
				goto Recalculate_Collision;
			}

			break;

			//if (horizontal_collision)
			//{
			//	me.x_offset = hitPoint.x;// +((colSide & ColSide_Left) ? -margin_obstacles : margin_obstacles);
			//	me.y_offset = hitPoint.y;
			//	//me.x_offset = localPos.x - ((horizontal_collision > 0 ? (x_min_dist + margin_obstacles) : (x_max_dist - margin_obstacles)));//(localPos.x - curObstacle.rect.Min.x > margin_bounds ? (localPos.x + 1) : (localPos.x - 1));// +((localPos.x - curObstacle.rect.Min.x) <= margin_bounds ? 1 : -1);
			//	//me.y_offset = localPos.y;
			//	me.x_multiplier *= -1;
			//	physObj.timeCreated = simulationTime;
			//	break;
			//}
			//else if (vertical_collision)
			//{
			//	//me.x_offset = localPos.x;
			//	//me.y_offset = localPos.y - ((vertical_collision > 0 ? (y_min_dist + margin_obstacles) : (y_max_dist - margin_obstacles)));
			//	me.y_multiplier *= -1;
			//	physObj.timeCreated = simulationTime;
			//	break;
			//}
		}
		//if (recalculate_buffered_col) // Czy to jest bardziej czytelnie, niz goto? Nie wiem (osobiscie tak nie uwazam), ale wszyscy bez powodu.. nie lubia goto, wiec go nie uzyje
		//  // Edit: Jednak nie poddam sie presci spoleczenstwa i uzyje goto.
		//{
		//	should_rebuffer_collisions = true;
		//	particle_index++;
		//	continue;
		//}

		for (int circtleIdx = 0; circtleIdx < circles.size(); circtleIdx++)
		{
			PhysicsCircle circle = circles[circtleIdx];

			// Styczna do okregu to: a^2 + x (w - a) - a w + b^2 + y (t - b) - b t = r^2 gdzie (a,b) - pozycja okregu, r - promien okregu, (w, t) - pozycja styku z okregiem
			// Styczna mozna zapisac w posaci ogolnej: 0 = ax + by + c
			// tg^-1(a/b) - kat funkcji wzgledem OX

			//if (IsPointInsideCircle(localPos, circle.circleEq, -50.f))
			//	auto x = 0;

			if (!should_rebuffer_collisions && !IsPointInsideCircle(localPos, circle, 0.f))
				continue;

			float w = circle.circleEq.pos.x, t = circle.circleEq.pos.y, r = circle.circleEq.radius;

			float root = 0;
			double p1 = 0; // part1 - pierwsza czesc rownania kwadratowego w obliczaniu x1 / x2
			float denominator = 0;
			if (is_x_mult_small) { // Dla takich przypadkow nowe "a" powinno byc rowne "a" prostej stycznej do okregu w punkci styku
				// Mozna by tutaj obliczac wszystko z wieksza dokladnoscia, ale mozna tez przyjac, ze x1 i x2 sa po prostu rowne wartosci x aktualnej pozycji, a y wyliczamy z R^2 = (x-w)^2 + (y-t)^2 (kilkana¶cie linijek ni¿ej)
				//double deltaD = pow((-2.0 * w) - (2.0 * (double)a * t) + (2.0 * (double)a * c), 2) - (((double)4.0 * (1 + pow(a, 2))) * ((w * w) - (2.0 * (double)c * t) + pow(t, 2) + pow(c, 2) - pow(r, 2)));
				//if (deltaD < 0)
				//	continue;
				//root = sqrtf(deltaD);
				//p1 = (-((-2.0 * w) - (2.0 * a * t) + (2.0 * a * c)));
				//denominator = 2 * (1 + (a * a));
			}
			else {
				float delta = pow((-2 * w) - (2 * a * t) + (2.0 * a * c), 2) - ((4.0 * (1 + pow(a, 2))) * ((w * w) - (2.0 * (double)c * t) + (t * t) + pow(c, 2) - (r * r)));
				if (delta < 0)
					continue;
				root = sqrtf(delta);
				p1 = (-((-2 * w) - (2 * a * t) + (2 * a * c)));
				denominator = 2 * (1 + (a * a));
			}
			//double p1D = (-((-2 * w) - (2.0 * (double)a * t) + (2.0 * (double)a * c)));

			//printf("a = %20.15f; c = %20.15f; w = %20.15f; t = %20.15f; r = %20.15f\n", a, c, w, t, r);

			float x1 = 0, x2 = 0, y1 = 0, y2 = 0;

			if (is_x_mult_small)
			{
				x1 = localPos.x;
				x2 = localPos.x;

				y1 = t - sqrtf((r * r) - powf(w - x1, 2));
				y2 = t + sqrtf((r * r) - powf(w - x2, 2));

				if(isnan(y1))
					y1 = (x1 * a) + c;
				if (isnan(y2))
					y2 = (x2 * a) + c;
			}
			else
			{
				x1 = (p1 + root) / denominator;
				x2 = (p1 - root) / denominator;

				y1 = (x1 * a) + c;
				y2 = (x2 * a) + c;
			}

			//Physics::Distance({ x1, y1 }, localPos) < Physics::Distance({ x2, y2 }, localPos);
			// =>
			bool useX1 = is_x_mult_small ? (me_copy.y_multiplier * timeSign > 0 ? (y1 < y2) : (y1 > y2)) :
				(me_copy.x_multiplier * timeSign > 0 ? (x1 < x2) : (x1 > x2));

			auto selected_x = useX1 ? x1 : x2;
			auto selected_y = useX1 ? y1 : y2;

			// Buforowanie kolizji
			if (should_rebuffer_collisions)
			{
				float col_time = 0;

				// y = (a - b) * (d - c)
				if (is_x_mult_small) // zeby uniknac dzielenia przez 0 => dla malych wartosci x_mult prostej wyliczamy czas zderzenia z osi y.
					col_time = (me_copy.y_multiplier) * (selected_y - me_copy.y_offset);
				else
					col_time = (selected_x - me_copy.x_offset) / me_copy.x_multiplier;

				if (col_time * timeSign < -0.01f)
					continue;

				if (timeSign > 0)
					physObj.buffered_collision_time = fminf(col_time * TIME_MULTIPLIER, physObj.buffered_collision_time);
				else
					physObj.buffered_collision_time = fmaxf(col_time * TIME_MULTIPLIER, physObj.buffered_collision_time);

				//printf("circ y col time: %f\n", col_time + (physObj.timeCreated / 10000));
				continue;
			}

			//printf("circle hit\n");

			// Styczna do okregu
			float aTangent = (selected_x - w) / (t - selected_y);

			// Prosta odbita od stycznej
			float aReflectedNom = (2 * aTangent) + (a * (aTangent * aTangent)) - a;
			float aReflectedDen = (2 * aTangent * a) - (aTangent * aTangent) + 1;
			float aReflected = aReflectedNom / aReflectedDen;

			bool is_on_left_side = localPos.x < circle.circleEq.pos.x;

			// trzeba czasami obrocic kierunek prostej, gdyz normalnie kierunek prostej nie ma zadnego zanczenia, ale w tym przypadku ma (-:
			bool flip = (is_on_left_side && is_x_mult_small) || !is_x_mult_small && ((me.x_multiplier < 0) && aReflectedDen > 0) || (me_copy.x_multiplier > 0 && aReflectedDen < 0);

			//if (Physics::Distance({ w, t }, localPos) <= r) // && (fabs(Physics::simulationTime - physObj.timeCreated) > 20000)
			{
				me.x_offset = selected_x;
				me.y_offset = selected_y;
				float ang = atanf(aReflected);
				me.x_multiplier = cosf(ang) * (flip ? -1 : 1) * physObj.initialVelocity * energy_conservation;
				me.y_multiplier = sinf(ang) * (flip ? -1 : 1) * physObj.initialVelocity * energy_conservation;
				physObj.initialVelocity *= energy_conservation;

				physObj.timeCreated = Physics::simulationTime;

				if (bufferCollisions) {
					should_rebuffer_collisions = true;
					goto Recalculate_Collision;
				}

				//if (bufferCollisions)
				//	recalculate_buffered_col = true;

				break;
			}
		}

//#ifdef MULTI_THREADED
//		if (particle_index < particlesPositions.size())
//			particlesPositions[particle_index] = localPos;
//		else
//			particlesPositions.push_back(localPos);
//#endif
	}

	//isPositionUpdateRequested = false;
	if(sim_changed)
		was_simulation_changed = false;
	if (size_changed)
		was_size_changed = false;

	_last_time_sign = timeSign;
	_last_should_buffer_collisions = now_buffer_collisions;
}

// Przed zawo³aniem nale¿y ustawiæ is_accessing na true
Vector2 Physics::GetObjectPosition(PhysicsObject& physObj)
{
	MotionEquation me = physObj.me;
	auto time = simulationTime - physObj.timeCreated;

	Vector2 finalPos;
	finalPos.x = (me.x_multiplier * time) / 10000 + me.x_offset;
	finalPos.y = (me.y_multiplier * time) / 10000 + me.y_offset;

	return finalPos;
}

bool Physics::AddObstacle(PhysicsObstacle obstacle)
{
#ifdef MULTI_THREADED
	is_accessing = true;
#endif
	obstacles.push_back(obstacle);
	was_simulation_changed = true;
#ifdef MULTI_THREADED
	is_accessing = false;
#endif
	return true;
}

bool Physics::AddObstacle(PhysicsCircle circle)
{
#ifdef MULTI_THREADED
	is_accessing = true;
#endif
	circles.push_back(circle);
	was_simulation_changed = true;
#ifdef MULTI_THREADED
	is_accessing = false;
#endif
	return true;
}

bool Physics::RemoveObstacle(size_t idx)
{
#ifdef MULTI_THREADED
	is_accessing = true;
#endif
	obstacles.erase(obstacles.begin() + idx);
	was_simulation_changed = true;
#ifdef MULTI_THREADED
	is_accessing = false;
#endif
	return true;
}

bool Physics::RemoveCircle(size_t idx)
{
#ifdef MULTI_THREADED
	is_accessing = true;
#endif
	circles.erase(circles.begin() + idx);
	was_simulation_changed = true;
#ifdef MULTI_THREADED
	is_accessing = false;
#endif
	return true;
}

bool Physics::Clear()
{
#ifdef MULTI_THREADED
	is_accessing = true;
#endif
	was_simulation_changed = true;
	obstacles.clear();
	circles.clear();
	particles.clear();
#ifdef MULTI_THREADED
	is_accessing = false;
#endif
	return true;
}

bool Physics::Clear(ObjType type)
{
#ifdef MULTI_THREADED
	is_accessing = true;
#endif
	was_simulation_changed = true;
	if((type & Obj_Rectangle))
		obstacles.clear();
	if ((type & Obj_Circle))
		circles.clear();
	if ((type & Obj_Particle))
		particles.clear();
#ifdef MULTI_THREADED
	is_accessing = false;
#endif
	return true;
}