#include <string>

#include "Gui/gui.h"
#include "../External/ImGui/imgui.h"
#include "../External/ImGui/imgui_internal.h"
#include "../External/ImGui/imgui_extensions.h"
#include "Physics/physics.h"

uint64_t lastFrameTime = 1, lastFrame = 0, totalFrames = 0, lastPhysicsStep = 0, totalSimStartTime = 0;
int FramesPerSecond = 72;
int physicsUpdateFrequency = 360;

int tutorialStep = 1;

std::atomic<uint64_t> totalSimSteps = 1;

bool hideUI = false;

uint64_t last_ui_hide_time = 0;
bool is_placing_object = false;

bool intent_to_clear_last_frame = false;

INT8 last_select_type = 0;

#define sign(n) (n < 0 ? -1 : 1)

#define SELECT_RECT_COLOR ImColor(0.5f, 0.7f, 0.5f)
#define TUTORIAL_TEXT_WIDHT 45

struct AdvancedSettings
{
	// Renderowanie
	bool useLowQuality = false;
	bool useLineRendering = false;
	ImColor particleRenderColor = ImColor(205, 205, 255, 220);
	float particleRenderSize = 1.5f;
	bool velocityColorRendering = false;
	bool discoRender = false;

	// Fizyka
	bool randomPartcileDistribution = true;
	float particleVelocityDistribution = 0.0f;

	// Inne
	bool hideUiWhenPlacing = true;
};

AdvancedSettings advSettings{};

static LONGLONG StartingTime{ 0 };
uint64_t micros() // Sposób na zdobywanie mikrosekund sugerowny przez Microsoft
{
	LARGE_INTEGER EndingTime, ElapsedMicroseconds;
	LARGE_INTEGER Frequency;

	QueryPerformanceFrequency(&Frequency);

	QueryPerformanceCounter(&EndingTime);
	ElapsedMicroseconds.QuadPart = EndingTime.QuadPart - StartingTime;

	ElapsedMicroseconds.QuadPart *= 1000000;
	ElapsedMicroseconds.QuadPart /= Frequency.QuadPart;

	return ElapsedMicroseconds.QuadPart;
}


int OnGui()
{
	ImGuiIO& io = ImGui::GetIO();
	ImGuiStyle& style = ImGui::GetStyle();
	ImDrawList* draw_list = ImGui::GetWindowDrawList();

	bool isSimulationAreaHovered = false;

	auto timeNow = micros();

	ImVec2 MousePos = io.MousePos;
	ImVec2 ScreenSize = io.DisplaySize;

	static float ui_hide_progress = 0;
	if (advSettings.hideUiWhenPlacing && !tutorialStep)
	{
		static float _progress = 0;
		if (is_placing_object)
			_progress = ImClamp(_progress + 6 * io.DeltaTime, 0.f, 1.f);
		else
			_progress = ImClamp(_progress - 6 * io.DeltaTime, 0.f, 1.f);

		ui_hide_progress = sinf(_progress * (M_PI / 2));
	}

	// ----------------------------------------- Draw Grid -----------------------------------------
	if (!hideUI)
	{
		int _max_x = Physics::boundsExtend.x;
		int _max_y = Physics::boundsExtend.y;
		for (int x = 0; x < _max_x; x += 100) {
			draw_list->AddLine({ (float)x, 0 }, { (float)x, (float)_max_y }, ImColor(255, 255, 255, 30), 1.5f);
		}
		for (int y = 0; y < _max_y; y += 100) {
			draw_list->AddLine({ 0, (float)y }, { (float)_max_x, (float)y }, ImColor(255, 255, 255, 30), 1.5f);
		}
	}

	static const ImColor backup_color_modal_dim{ style.Colors[ImGuiCol_ModalWindowDimBg] };

	static int selectedTool = 0;

	// Musimy tak usuwać elementy fizyczne, przez wielo-wątkowość.
	// Najpierw informujemy o intencjach wątek fizyczny a w następnej klatce zmieniamy symulacje.
	// Fragmenty kodu podobne do tego będą się pojawiać w kodzie, ale nie będę ich tłumaczył dla każdego przypadku.
	if (intent_to_clear_last_frame)
	{
		Physics::Clear();
		Physics::is_accessing = false;
		intent_to_clear_last_frame = false;
	}

	if (tutorialStep)
	{
		style.Colors[ImGuiCol_ModalWindowDimBg].w = 0.02f;
		char _buffer[15];
		sprintf_s(_buffer, "Poradnik 0%i", tutorialStep);
		ImGui::OpenPopup(_buffer);
	}
	else // Handle shortcuts
	{
		if (ImGui::IsKeyPressed(ImGuiKey_Space, false))
			Physics::isSimPlaying ^= true;

		if (ImGui::IsKeyPressed(ImGuiKey_H, false))
			hideUI ^= true;

		if (ImGui::IsKeyPressed(ImGuiKey_C, false))
		{
			intent_to_clear_last_frame = true;
			Physics::is_accessing = true;
		}

		if (ImGui::IsKeyPressed(ImGuiKey_1, false))
			selectedTool = 0;
		if (ImGui::IsKeyPressed(ImGuiKey_2, false))
			selectedTool = 1;
		if (ImGui::IsKeyPressed(ImGuiKey_3, false))
			selectedTool = 2;
		if (ImGui::IsKeyPressed(ImGuiKey_4, false))
			selectedTool = 3;
	}

	// Poradnik 1
	if (ImGui::BeginPopupModal("Poradnik 01", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings))
	{
		ImGui::Text(u8"Poradnik do Programu symulacji cząsteczek");

		ImGui::Separator(); ImGui::Spacing();
		if (ImGui::Button(u8"Pomiń")) {
			ImGui::CloseCurrentPopup();
			tutorialStep = 0;
		}

		ImGui::SameLine(ImGui::GetContentRegionAvail().x - 83);

		if (ImGui::Button("Zaczynajmy!"))
		{
			tutorialStep++;
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
		return 0;
	}


	// ----------------------------------------- Stats Window -----------------------------------------
	static bool showStats = false;

	if (showStats && !hideUI) {
		ImGui::SetNextWindowBgAlpha(0.3f);
		ImGui::SetNextWindowPos({ 15,15 });
	}

	if (showStats && !hideUI && ImGui::Begin("Stats##StatsWindow", &showStats, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar))
	{
		ImGui::Text(u8"Średnie FPS: %.2f", io.Framerate);

		ImGui::Text(u8"Cząsteczki: %lld", Physics::particles.size());

		ImGui::Text(u8"Częst. Symulacji: %.0fHz", ((float)totalSimSteps / (timeNow - totalSimStartTime)) * 1000000.f);
		if (ImGui::IsItemClicked(1))
		{
			totalSimStartTime = timeNow;
			totalSimSteps = 0;
		}
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
			ImGui::SetTooltip(u8"Kliknij prawym przyciskiem myszy aby zresetować średnią");

		ImGui::Text("Czas Symulacji: %.0lfms", Physics::simulationTime / 1000);

		//ImGui::Text("Particles mem useed: %lluMB", (sizeof(Physics::PhysicsObject) * Physics::particles.size()) / 1000000);
	}

	if (showStats && !hideUI)
		ImGui::End();

	if (ImGui::BeginPopupModal("Poradnik 02", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings) && !hideUI)
	{
		ImGui::Text(u8"Na samym środku programu znajduje się \"piaskownica\" do dodawania elementów.\nNa niej będą zachodziły wszystkie kolizje i inne zdarzenia fizyczne");

		ImGui::Separator(); ImGui::Spacing();
		if (ImGui::Button(u8"Wstecz"))
			tutorialStep--;

		ImGui::SameLine(ImGui::GetContentRegionAvail().x - TUTORIAL_TEXT_WIDHT);

		if (ImGui::Button("Dalej!"))
		{
			tutorialStep++;
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
		return 0;
	}

	// Tworzymy niewidoczny obiekt na powierzchni całego okna, aby sprawdzić, czy użytkownik nie jest przypadkiem myszką na jakimś innym oknie
	// Dlatego też dodajemy ten obiekt na początku, tak, aby był pod wszystimi innymi elementami interfejsu.
	auto _touch_padding = style.TouchExtraPadding;
	style.TouchExtraPadding = { 1,1 };
	ImGui::ItemAdd({ {0,0}, ScreenSize + ImVec2(5,5) }, ImGui::GetID("SimulationArea"), nullptr, ImGuiItemFlags_NoTabStop);
	if (ImGui::IsItemHovered())
		isSimulationAreaHovered = true;
	style.TouchExtraPadding = _touch_padding;


	// ----------------------------------------- Tool selection window -----------------------------------------
	static int particlesToSpawn = 1;
	static size_t last_time_particles_amount_changed = -10000000;
	static bool isToolsWindowCollapsed = false;
	const int toolsNum = 4;
	const char* tools[] = { "Zaznacz", u8"Cząsteczki", u8"Prostokąt", u8"Okrąg" };

	float tool_window_width = (isToolsWindowCollapsed ? 20.f : 100.f);
	ImGui::SetNextWindowPos({ ImGui::GetWindowWidth() - tool_window_width + (tool_window_width * ui_hide_progress), ScreenSize.y / 2 - 150 });
	ImGui::SetNextWindowSize({ tool_window_width + 20, 300 });
	ImGui::SetNextWindowBgAlpha(0.3f);

	if (!hideUI && ImGui::Begin(u8"Narzędzia##ToolsWindow", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove))
	{
		for (int i = 0; i < toolsNum; i++)
		{
			if (i == 2) {
				float _alpha = style.Colors[ImGuiCol_Separator].w;
				style.Colors[ImGuiCol_Separator].w = 200;
				//ImGui::PushStyleColor(ImGuiCol_Separator, { 200,200,200,250 });
				ImGui::Spacing();
				ImGui::HeaderTitle("Bariery", -18);
				style.Colors[ImGuiCol_Separator].w = _alpha;
				//ImGui::PopStyleColor();
			}
			if (ImGui::Selectable(tools[i], selectedTool == i))
			{
				if (selectedTool == i)
					selectedTool = -1;
				else
					selectedTool = i;

				// Pokaż ilość cząsteczek po zaznaczeniu cząsteczek z menu
				if (selectedTool == 1)
					last_time_particles_amount_changed = timeNow;
			}
		}
	}
	isToolsWindowCollapsed = ImGui::IsWindowCollapsed();

	bool isToolsFocused = false;

	if (ImGui::IsWindowFocused())
		isToolsFocused = true;

	if (!hideUI)
		ImGui::End();

	if (ImGui::BeginPopupModal("Poradnik 03", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings))
	{
		selectedTool = 1;
		ImGui::Text(u8"Aby dodać cząsteczki i bariery można skorzystać z panelu narzędzi po prawej stronie, \nnależy wybrać element z listy, a następie lewym przyciskiem myszy dodać go do symulacji.");
		ImGui::Text(u8"W celu zwiększenia ilości dodawanych elementów użyj scroll'a.");

		ImGui::Separator(); ImGui::Spacing();
		if (ImGui::Button(u8"Wstecz"))
			tutorialStep--;

		ImGui::SameLine(ImGui::GetContentRegionAvail().x - TUTORIAL_TEXT_WIDHT);

		if (ImGui::Button("Dalej!"))
		{
			tutorialStep++;
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
		return 0;
	}


	// ----------------------------------------- Item Selection -----------------------------------------
	ImRect selectedArea;
	if (selectedTool == 0)
	{
		static ImVec2 startPos;
		static ImVec2 mouseDrag{ 0,0 };
		static bool isCancelled = false;
		auto curMouseDrag = ImGui::GetMouseDragDelta(0, 0);
		if (!isCancelled)
		{
			if (ImGui::IsMouseReleased(0) && fabs(mouseDrag.x + mouseDrag.y) < 2)
			{
				selectedArea = { startPos.x, startPos.y, 0, 0 };
				last_select_type = io.KeyShift ? -1 : io.KeyCtrl;
				ZeroMemory(&mouseDrag, sizeof(ImVec2));
			}

			if (ImGui::IsMouseDragging(0, 0))
			{
				if (ImGui::IsMouseClicked(1))
					isCancelled = true;
				mouseDrag = curMouseDrag;
				if (fabs(mouseDrag.x + mouseDrag.y) > 2)
					draw_list->AddRect(startPos, startPos + mouseDrag, SELECT_RECT_COLOR);
				else if (!isSimulationAreaHovered)
					isCancelled = true;
			}
			else if (mouseDrag.x == 0 && mouseDrag.y == 0)
				startPos = ImGui::GetMousePos();
			else
			{
				ImRect rect = { startPos, startPos + mouseDrag };
				if (mouseDrag.x < 0)
				{
					rect.Min.x = rect.Max.x - 1;
					rect.Max.x = startPos.x + 1;
				}
				if (mouseDrag.y < 0)
				{
					rect.Min.y = rect.Max.y - 1;
					rect.Max.y = startPos.y + 1;
				}
				selectedArea = rect;
				last_select_type = io.KeyShift ? -1 : io.KeyCtrl;
				ZeroMemory(&mouseDrag, sizeof(ImVec2));
			}
		}
		else
		{
			if (ImGui::IsMouseReleased(0))
			{
				ZeroMemory(&mouseDrag, sizeof(ImVec2));
				isCancelled = false;
			}
		}
	}


	// ----------------------------------------- Spawn a new object -----------------------------------------
	static bool isItemSpwanPending = false;
	static Physics::Vector2 ItemSpawnPos;
	static float angleRangeMultiplier = 0.1f;

	auto scrollDelta = io.MouseWheel;
	if (selectedTool == 1 && scrollDelta != 0 && !isItemSpwanPending)
	{ // Zwiększanie ilości cząsteczek do dodania. Przy dużych wartościach dodajemy znacznie więcej cząsteczek, przy przpszym scrollowaniu.
		auto factor = ImClamp(log10f(abs(particlesToSpawn)) + io.KeyShift, 1.f, 4.f);
		// wartości w potędze dobrane są arbitralnie. Takie wartości po prostu są przyjemne w użytkowaniu
		particlesToSpawn = max(particlesToSpawn + ((int)powf(abs(scrollDelta), factor + (sign(scrollDelta) == 1 ? 1.3 : 0.35f)) * factor) * sign(scrollDelta), 0);
		last_time_particles_amount_changed = timeNow;
	}

	// Tekst informujący o ilości cząsteczek do stworzenia
	ImGui::PushStyleColor(ImGuiCol_Text, ImColor(220, 220, 220, (int)ImClamp((600000.f - (timeNow - last_time_particles_amount_changed)) / 2352.f, 0.f, 255.f)).Value);
	ImGui::SetWindowFontScale(2.f);
	ImGui::RenderTextClipped({ 0, 140 }, { io.DisplaySize.x, 180 }, std::to_string(particlesToSpawn).c_str(), nullptr, nullptr, style.ButtonTextAlign);
	ImGui::SetWindowFontScale(1.f);
	ImGui::PopStyleColor();

	// Początek dodawania cząsteczki (initial press)
	if (selectedTool == 1 && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
	{
		ItemSpawnPos = { io.MousePos.x, io.MousePos.y };
		isItemSpwanPending = true;
	}

	if (isItemSpwanPending && (!isSimulationAreaHovered && !tutorialStep)) {
		isItemSpwanPending = false;
		is_placing_object = false;
	}

	if (isItemSpwanPending)
	{
		is_placing_object = true;
		//Physics::is_accessing = true;
		auto length = ImVec2(ItemSpawnPos - io.MousePos).Magnitude();
		if (length > 10)
		{
			// Rysowanie lini pokazującej kierunek ruchu dodawanej cząsteczki / cząsteczek
			draw_list->AddLine(io.MousePos, ItemSpawnPos, ImColor(0.3f, 0.3f, 0.4f));

			if (fabs(scrollDelta) > 0) {
				angleRangeMultiplier += scrollDelta / sqrtf(length) / 2.f;
				angleRangeMultiplier = ImClamp(angleRangeMultiplier, 0.f, M_PI);
			}

			// Rysowanie dwóch linii reprezentujących kąt pod jakim polecą cząsteczki, oraz tekst zawierający owy kąt.
			int is_left = (io.MousePos.x >= ItemSpawnPos.x ? 0 : -1);
			double angle = atanf((io.MousePos.y - ItemSpawnPos.y) / (io.MousePos.x - ItemSpawnPos.x)) + (is_left ? (M_PI) : 0);

			Physics::Vector2 translatedPos = Physics::RotatePointAroundPoint(ItemSpawnPos, io.MousePos, -angleRangeMultiplier);
			draw_list->AddLine(ItemSpawnPos, translatedPos, ImColor(0.32f, 0.25f, 0.3f));

			translatedPos = Physics::RotatePointAroundPoint(ItemSpawnPos, io.MousePos, angleRangeMultiplier);
			draw_list->AddLine(ItemSpawnPos, translatedPos, ImColor(0.32f, 0.25f, 0.3f));

			float a1 = angleRangeMultiplier + angle;
			float a2 = -angleRangeMultiplier + angle;

			draw_list->PathArcTo(ItemSpawnPos, 35, a1 < a2 ? a1 : a2, a1 > a2 ? a1 : a2, 50);
			draw_list->PathStroke(ImColor(0.32f, 0.25f, 0.3f));

			auto angle_text_pos = ItemSpawnPos - (ImVec2(ItemSpawnPos - io.MousePos).Normalized() * 50);
			char _buffer[20];
			sprintf_s(_buffer, u8"%.1f\xB0", angleRangeMultiplier * 2 * RAD_2_DEG);
			ImGui::RenderTextClipped({ angle_text_pos.x - 50, angle_text_pos.y - 10 }, { angle_text_pos.x + 50, angle_text_pos.y + 10 }, _buffer, nullptr, nullptr, style.ButtonTextAlign);
		}
	}



	// COLLAPSE
	if (ImGui::BeginPopupModal("Poradnik 04", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings))
	{
		isItemSpwanPending = true;
		ItemSpawnPos = { 150, 150 };
		ImGui::Text(u8"Aby natomiast stworzyć cząsteczki poruszające się w danym kierunku wybierz cząsteczki z menu narzędzi,\nnastępnie trzymając lewy przycisk myszy (np. punkt [150,150]) na \"piaskownicy\" przejedź myszką w jakąkolwiek stronę.\nAby potwierdzić wybór wystarczy puścić lewy przycisk myszy.");
		ImGui::Text(u8"W celu zwiększenia kąta użyj scroll'a.");

		ImGui::Separator(); ImGui::Spacing();
		if (ImGui::Button(u8"Wstecz"))
			tutorialStep--;

		ImGui::SameLine(ImGui::GetContentRegionAvail().x - TUTORIAL_TEXT_WIDHT);

		if (ImGui::Button("Dalej!"))
		{
			isItemSpwanPending = false;
			selectedTool = 0;
			tutorialStep++;
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
		return 0;
	}

	if (ImGui::BeginPopupModal("Poradnik 05", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings))
	{
		ImGui::Text(u8"Teraz żeby pozbyć się cząsteczek należy użyć narzędzia \"Zaznacz\", którym jak sama nazwa wskazuje\ntrzeba zaznaczyć niechciane cząsteczki/bariery trzymając lewy przycisk myszy a następnie pozbyć się ich kilkacją klawisz DEL.");

		ImGui::Separator(); ImGui::Spacing();
		if (ImGui::Button(u8"Wstecz"))
			tutorialStep--;

		ImGui::SameLine(ImGui::GetContentRegionAvail().x - TUTORIAL_TEXT_WIDHT);

		if (ImGui::Button("Dalej!"))
		{
			tutorialStep++;
			ImGui::CloseCurrentPopup();
		}

		//float t = (sin((float)timeNow/600000)+1)/2.f;
		//draw_list->AddRect({ 80, 80 }, { 120 + (t * 300), 120 + (t * 300) }, SELECT_RECT_COLOR);
		draw_list->AddRect({ 80, 80 }, io.MousePos, SELECT_RECT_COLOR);

		ImGui::EndPopup();
		return 0;
	}

	if (ImGui::BeginPopupModal("Poradnik 06", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings))
	{
		ImGui::Text(u8"Lista najważniejszych skrótów klawiszowych:\nSPACJA - Włączanie/Zatrzymywanie Symylacji\nH - Ukrywanie/Pokazywanie UI\nC - Czyszczenie \"Piaskownicy\"\n1-4 - Narzędzia\nSHIFT - Szybsze Dodawanie Cząsteczek Scroll'em");

		ImGui::Separator(); ImGui::Spacing();
		if (ImGui::Button(u8"Wstecz"))
			tutorialStep--;

		ImGui::SameLine(ImGui::GetContentRegionAvail().x - TUTORIAL_TEXT_WIDHT);

		if (ImGui::Button("Dalej!"))
		{
			tutorialStep++;
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
		return 0;
	}



	static bool was_mouse_released_last_frame{ false };

	// Dodawanie cząsteczki / cząsteczek do symulacji
	if (was_mouse_released_last_frame)
	{
		struct DoubleVector { double x = 0; double y = 0; };
		isItemSpwanPending = false;
		is_placing_object = false;
		last_ui_hide_time = timeNow;
		double angle = atanf((io.MousePos.y - ItemSpawnPos.y) / (io.MousePos.x - ItemSpawnPos.x));
		DoubleVector angleRange{ angle - (angleRangeMultiplier), angle + (angleRangeMultiplier) };
		bool useRandom = (Physics::Distance(io.MousePos, ItemSpawnPos) < 10);
		int is_left = (io.MousePos.x >= ItemSpawnPos.x ? 1 : -1);
		for (int i = 0; i < particlesToSpawn; i++)
		{
			float _angle = angle;
			if (useRandom)
				if (advSettings.randomPartcileDistribution)
					_angle = Physics::GetRandomAngle();
				else
					_angle = (((float)i / particlesToSpawn) * M_PI * 2) - M_PI;
			else
				if (advSettings.randomPartcileDistribution)
					_angle = Physics::GetRandomValueInRange(angleRange.x, angleRange.y);
				else
					_angle = (((float)i / particlesToSpawn) * (angleRange.y - angleRange.x)) + angleRange.x;

			float initial_velocity{ 1 };
			if (advSettings.particleVelocityDistribution > 1)
				initial_velocity = Physics::GetRandomValueInRange(1.f, advSettings.particleVelocityDistribution);

			//_angle = M_PI/2 - 0.00001; // ~ x = 0 => ruch pionowy

			Physics::MotionEquation me = Physics::MotionEquation(cos(_angle) * is_left * initial_velocity, sin(_angle) * is_left * initial_velocity, ItemSpawnPos.x, ItemSpawnPos.y);
			Physics::particles.push_back(Physics::PhysicsObject(me, Physics::simulationTime, initial_velocity));

			//auto a = (me.y_multiplier / me.x_multiplier);
			//printf("a = %f, c = %f\n", a, sqrt(pow(finalPos.x, 2) + pow(finalPos.y, 2)));
			//float c = me.x_multiplier > 0 ? (me.x_multiplier * me.y_offset) - (me.y_multiplier * me.x_offset) : (me.y_multiplier * me.y_offset) - (me.x_multiplier * me.x_offset);
			//printf("a = %f, c = %f; (x_offset: %f, y_offset: %f), angle = %.4f\n", a, c, me.x_offset, me.y_offset, angle * 180/M_PI);
			//Physics::particlesPositions.push_back(io.MousePos);
			//Physics::MotionEquation me2 = Physics::MotionEquation(sinf(angle), -cosf(angle), io.MousePos.x, io.MousePos.y);
			//particles.push_back(Physics::PhysicsObject(me2, timeNow));
		}
		Physics::is_accessing = false;
		//Physics::last_added_particles_num = particlesToSpawn;
		was_mouse_released_last_frame = false;
	}

	// bufferowanie dodania cząsteczki w następnej klatce
	if (ImGui::IsMouseReleased(0) && isItemSpwanPending) {
		Physics::is_accessing = true;
		was_mouse_released_last_frame = true;
	}


	// ----------------------------------------- Spawn new Rectangle -----------------------------------------
	if (selectedTool == 2)
	{
		static bool last_added_obstacle{ false };
		static ImVec2 startPos;
		static ImVec2 mouseDrag{ 0,0 };
		static bool isCancelled = false;
		auto curMouseDrag = ImGui::GetMouseDragDelta(0, 1);

		if (last_added_obstacle)
		{
			ImRect rect = { startPos, startPos + mouseDrag };
			if (mouseDrag.x < 0)
			{
				rect.Min.x = rect.Max.x - 1;
				rect.Max.x = startPos.x + 1;
			}
			if (mouseDrag.y < 0)
			{
				rect.Min.y = rect.Max.y - 1;
				rect.Max.y = startPos.y + 1;
			}
			Physics::AddObstacle(Physics::PhysicsObstacle({ rect }));
			Physics::is_accessing = false;
			ZeroMemory(&mouseDrag, sizeof(ImVec2));
			last_added_obstacle = false;
			is_placing_object = false;
		}

		if (!isCancelled)
		{
			if (ImGui::IsMouseDragging(0, 1))
			{
				is_placing_object = true;
				if (ImGui::IsMouseClicked(1) || !isSimulationAreaHovered) {
					isCancelled = true;
					is_placing_object = false;
				}
				mouseDrag = curMouseDrag;
				draw_list->AddRect(startPos, startPos + mouseDrag, ImColor(1.f, 1.0f, 0.5f));
			}
			else if (mouseDrag.x == 0 && mouseDrag.y == 0) // początek ruchu
				startPos = ImGui::GetMousePos();
			else {
				Physics::is_accessing = true;
				last_added_obstacle = true;
			}
		}
		else
		{
			is_placing_object = false;
			if (ImGui::IsMouseReleased(0))
			{
				ZeroMemory(&mouseDrag, sizeof(ImVec2));
				isCancelled = false;
			}
		}
	}


	// ----------------------------------------- Spawn new Circle -----------------------------------------
	if (selectedTool == 3)
	{
		static bool last_added_obstacle{ false };
		static ImVec2 startPos;
		static ImVec2 mouseDrag{ 0,0 };
		static bool isCancelled = false;
		auto curMouseDrag = ImGui::GetMouseDragDelta(0, 1);

		if (last_added_obstacle)
		{
			float r = sqrtf(powf(mouseDrag.x, 2) + powf(mouseDrag.y, 2));
			Physics::PhysicsCircle circle = Physics::PhysicsCircle({ startPos.x, startPos.y, r });
			Physics::AddObstacle(circle);
			ZeroMemory(&mouseDrag, sizeof(ImVec2));
			last_added_obstacle = false;
			is_placing_object = false;
		}

		if (!isCancelled)
		{
			if (ImGui::IsMouseDragging(0, 1))
			{
				is_placing_object = true;
				if (ImGui::IsMouseClicked(1) || !isSimulationAreaHovered) {
					isCancelled = true;
					is_placing_object = false;
				}
				mouseDrag = curMouseDrag;
				float r = sqrtf(powf(mouseDrag.x, 2) + powf(mouseDrag.y, 2));
				draw_list->AddCircle(startPos, r, ImColor(0.6f, 0.6f, 0.5f));
			}
			else if (mouseDrag.x == 0 && mouseDrag.y == 0)
				startPos = ImGui::GetMousePos();
			else
			{
				Physics::is_accessing = true;
				last_added_obstacle = true;
			}
		}
		else
		{
			is_placing_object = false;
			//last_ui_hide_time = timeNow;
			if (ImGui::IsMouseReleased(0))
			{
				ZeroMemory(&mouseDrag, sizeof(ImVec2));
				isCancelled = false;
			}
		}
	}



	bool deleteClicked = false;
	if (ImGui::IsKeyPressed(ImGuiKey_Delete)) {
		deleteClicked = true;
	}



	// ----------------------------------------- Draw Obstacles -----------------------------------------
	for (size_t i = 0; i < Physics::obstacles.size(); i++)
	{
		Physics::PhysicsObstacle& curObstacle = Physics::obstacles[i];

		if (selectedTool == 0 && selectedArea.Min.x != 0)
		{
			bool was_selected = (selectedArea.Max.x + selectedArea.Max.y) > 2 ?
				Physics::IsPointInsideRect(curObstacle.rect.GetCenter(), selectedArea) :
				Physics::IsPointInsideRect({ selectedArea.Min.x, selectedArea.Min.y }, curObstacle.rect);

			curObstacle.is_selected = last_select_type == 0 ? was_selected : // Nowe Zaznaczenie
				(last_select_type > 0 ? (curObstacle.is_selected || was_selected) : // Dodawanie zaznaczenia
					(curObstacle.is_selected && was_selected) ? 0 : curObstacle.is_selected); // Odejmowanie zaznaczenia
		}

		if (deleteClicked && curObstacle.is_selected)
			continue;

		draw_list->AddRectFilled(curObstacle.rect.Min, curObstacle.rect.Max, curObstacle.is_selected ? ImColor().HSV(0, 0, 0.12f) : ImColor().HSV(0, 0, 0.08f));
		draw_list->AddRect(curObstacle.rect.Min, curObstacle.rect.Max, curObstacle.is_selected ? ImColor(0.7f, 0.6f, 0.2f) : ImColor(1.f, 1.0f, 0.5f));
	}


	// ----------------------------------------- Draw Physics Objects -----------------------------------------
	static bool _Memory_Asked = false;
	size_t particles_amount = Physics::particles.size();

	if (!_Memory_Asked && particles_amount > 200000)
		ImGui::OpenPopup(u8"Duże zużycie pamięci");

	if (advSettings.useLowQuality) // alokowanie przestrzeni na linie dla renderowanych cząsteczek
	{
		draw_list->_Path.reserve(draw_list->_Path.Size + (particles_amount * (advSettings.useLineRendering ? 1 : 3)));
		ImGui::GetStyle().AntiAliasedFill = false;
	}

	for (size_t idx = 0; idx < Physics::particles.size(); ++idx)
	{
		static ImVec2 lastParticlePos;
		Physics::PhysicsObject& curObj = Physics::particles[idx];

		auto pos = Physics::GetObjectPosition(curObj);

		if (selectedTool == 0 && selectedArea.Min.x != 0)
		{
			bool was_selected = Physics::IsPointInsideRect(pos, selectedArea);
			curObj.is_selected = last_select_type == 0 ? was_selected : // Nowe Zaznaczenie
				(last_select_type > 0 ? (curObj.is_selected || was_selected) : // Dodawanie zaznaczenia
					(curObj.is_selected && was_selected) ? 0 : curObj.is_selected); // Odejmowanie zaznaczenia
		}

		if ((Physics::delete_intent || deleteClicked) && curObj.is_selected && Physics::isSimPlaying)
			continue;

		// Aby wyswietlac czasteczki nie zużywajac tyle pamieci nalezaly by wykozystac gpu instancing. Czego nie planuje robic. Yippe!
		// dzielimy przez 15, bo 15 to jest maksymalna wartość advSettings.particleVelocityDistribution
		float mag_ratio = advSettings.velocityColorRendering ? (ImVec2(curObj.me.x_multiplier, curObj.me.y_multiplier).Magnitude() - 1) / 15 : 0;

		// Jeżeli disco, to disco, jeżeli nie to jeżeli kolory zależne od prędkości, to ImLerp, jeżeli nie zależne od prędkości, to czy zaznaczony, czy nie
		auto color = advSettings.discoRender ?
			ImColor().HSV(fmodf((timeNow + (idx * 2137)) / 1500000.f, 1), 1, 1, advSettings.particleRenderColor.Value.w) :
			advSettings.velocityColorRendering ? ImColor(ImLerp(advSettings.particleRenderColor.Value, ImColor(255, 70, 100, (int)(advSettings.particleRenderColor.Value.w * 255)), mag_ratio)) :
			curObj.is_selected ? ImColor(advSettings.particleRenderColor.Value * ImVec4(2, 2, 2, 2)) : advSettings.particleRenderColor;

		// Kod uzywany do sprawdzenie poprawnosci dzialania kodu odpowiadajacego za zmiany symualcji (te if statemanty na początku for loop'a cząsteczek w physics.cpp)
		//if (fabs(curObj.buffered_collision_time) == FLT_MAX)
		//	color = ImColor(255, 20, 20);

		if (advSettings.useLineRendering)
		{
			if (advSettings.useLowQuality)
				draw_list->_Path.push_back(pos);
			else
				draw_list->AddLine(pos, lastParticlePos, color, advSettings.particleRenderSize);
		}
		else
		{
			if (advSettings.useLowQuality)
			{
				//// Kod poniżej jest odpowiedni funkji "AddNgonFilled", tylko manualnie przed pętlą alokujemy pamięć.
				////->AddNgonFilled(pos, 2, color, 3);
				//for (int i = 0; i <= 2; i++)
				//{
				//	const float a = ((float)i / 2.f) * (4.18879020f); // wartości stałe, więc je obliczyłem wcześniej (więcej w AddNgonFilled)
				//	draw_list->_Path.push_back(ImVec2(pos.x + ImCos(a) * advSettings.particleRenderSize, pos.y + ImSin(a) * advSettings.particleRenderSize));
				//}
				//draw_list->PathFillConvex(color);
			}
			else
				draw_list->AddCircleFilled(pos, advSettings.particleRenderSize, color);
		}

		// ---- Używane do prezentacji ----
		//char _buffer[96];
		//sprintf_s(_buffer, "x_mult: %.2f, y_mult: %.2f", curObj.me.x_multiplier, curObj.me.y_multiplier);
		//sprintf_s(_buffer, "Czas do Kolizji: %.1fs", fabs(curObj.buffered_collision_time) == FLT_MAX ? 0 : (curObj.buffered_collision_time + curObj.timeCreated - Physics::simulationTime) / 1000000.f);
		//ImGui::SetWindowFontScale(1.5f);
		//ImGui::RenderTextClipped(pos - ImVec2(500, 0), pos + ImVec2(500, 40), _buffer, nullptr, nullptr, style.ButtonTextAlign);
		//ImGui::SetWindowFontScale(1.f);

		lastParticlePos = pos;
	}

	if (advSettings.useLowQuality)
		ImGui::GetStyle().AntiAliasedFill = true;

	if (advSettings.useLineRendering && advSettings.useLowQuality)
		draw_list->PathStroke(advSettings.discoRender ? ImColor().HSV(fmodf(timeNow / 1500000.f, 1), 1, 1, advSettings.particleRenderColor.Value.w) : advSettings.particleRenderColor, 0, advSettings.particleRenderSize);

	if (ImGui::BeginPopupModal(u8"Duże zużycie pamięci", NULL, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text(u8"Program używa ok. 1.0GB RAM, czy chcesz zmniejszyć jakość cząsteczek, aby obniżyć zużycie pamięci?");

		if (ImGui::Button("Tak"))
		{
			advSettings.useLowQuality = true;
			_Memory_Asked = true;
			//draw_list->_Path.shrink(draw_list->_Path.Size - (Physics::particles.size() * 2));
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Nie"))
		{
			_Memory_Asked = true;
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}


	// ----------------------------------------- Draw Circles -----------------------------------------
	for (size_t i = 0; i < Physics::circles.size(); i++)
	{
		Physics::PhysicsCircle& curObstacle = Physics::circles[i];

		if (selectedTool == 0 && selectedArea.Min.x != 0)
		{
			bool was_selected = (selectedArea.Max.x + selectedArea.Max.y) > 2 ?
				Physics::IsPointInsideRect(curObstacle.circleEq.pos, selectedArea) :
				Physics::IsPointInsideCircle({ selectedArea.Min.x, selectedArea.Min.y }, curObstacle.circleEq);

			curObstacle.is_selected = last_select_type == 0 ? was_selected : // Nowe Zaznaczenie
				(last_select_type > 0 ? (curObstacle.is_selected || was_selected) : // Dodawanie zaznaczenia
					(curObstacle.is_selected && was_selected) ? 0 : curObstacle.is_selected); // Odejmowanie zaznaczenia
		}

		if (deleteClicked && curObstacle.is_selected)
			continue;

		draw_list->AddCircleFilled(curObstacle.circleEq.pos, curObstacle.circleEq.radius-1, curObstacle.is_selected ? ImColor().HSV(0, 0, 0.12f) : ImColor().HSV(0, 0, 0.08f));
		draw_list->AddCircle(curObstacle.circleEq.pos, curObstacle.circleEq.radius, curObstacle.is_selected ? ImColor(0.7f, 0.6f, 0.2f) : ImColor(1.f, 1.0f, 0.5f));
	}

	if (deleteClicked) {
		Physics::delete_intent = true;

		if (!Physics::isSimPlaying || Physics::particles.size() <= 0)
		{
			Physics::particles.erase(std::remove_if(Physics::particles.begin(), Physics::particles.end(),
				[](const Physics::PhysicsObject& obj) {
					return (obj.is_selected);
				}
			), Physics::particles.end());

			Physics::circles.erase(std::remove_if(Physics::circles.begin(), Physics::circles.end(),
				[](const Physics::PhysicsCircle& obj) {
					return (obj.is_selected);
				}
			), Physics::circles.end());

			Physics::obstacles.erase(std::remove_if(Physics::obstacles.begin(), Physics::obstacles.end(),
				[](const Physics::PhysicsObstacle& obj) {
					return (obj.is_selected);
				}
			), Physics::obstacles.end());
		}
	}


	// ----------------------------------------- Bottom parameters window -----------------------------------------
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { style.WindowPadding.x, 10 });
	auto paramsHeight = style.WindowPadding.y * 2 + ImGui::GetFrameHeight();
	ImGui::SetNextWindowPos({ ScreenSize.x / 2, ImGui::GetWindowHeight() - paramsHeight + (paramsHeight * ui_hide_progress) }, 0, { 0.5f, 0 });
	ImGui::SetNextWindowSize({ 940, paramsHeight + 10 });
	ImGui::SetNextWindowBgAlpha(0.3f);

	static bool showAdvancedSettings = false;

	if (!hideUI && ImGui::Begin("Parameters##ParametersWindow", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar))
	{
		if (ImGui::Button(Physics::isSimPlaying ? "Zatrzymaj" : u8"Wznów", { 75, 0 }))
			Physics::isSimPlaying ^= true; // Odrwacanie bitowe. :Nerd_Emoji:, skullemoji
		ImGui::SameLine();

		ImGui::SetNextItemWidth(200);
		ImGui::DragFloat(u8"Prędkość", &Physics::simulationSpeed, 0.01f, -2, 10);

		//ImGui::Text(u8"Ilość cząsteczek: %i", particlesToSpawn); // Przeniesione na środek ekranu (-;

		ImGui::SameLine();
		ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
		ImGui::SameLine();

		if (ImGui::Button(u8"Wyczyść"))
			Physics::Clear();
		if (ImGui::IsItemActive())
			Physics::is_accessing = true; // Zakladam, ze to wystarczy i nikt nie kliknie przycisku w czasie ~ < 10ms (jedenj klatki)
		else if (ImGui::IsItemHovered())
			Physics::is_accessing = false;
		if (ImGui::IsItemClicked(1))
			ImGui::OpenPopup("DeleteOptions");

		if (ImGui::BeginPopup("DeleteOptions"))
		{
			if (ImGui::Selectable(u8"Cząsteczki"))
				Physics::Clear(Physics::Obj_Particle);
			if (ImGui::IsItemActive())
				Physics::is_accessing = true;
			else if (ImGui::IsItemHovered())
				Physics::is_accessing = false;

			if (ImGui::Selectable(u8"Prostokąty")) Physics::Clear(Physics::Obj_Rectangle);
			if (ImGui::IsItemActive())
				Physics::is_accessing = true;
			else if (ImGui::IsItemHovered())
				Physics::is_accessing = false;

			if (ImGui::Selectable(u8"Okręgi")) Physics::Clear(Physics::Obj_Circle);
			if (ImGui::IsItemActive())
				Physics::is_accessing = true;
			else if (ImGui::IsItemHovered())
				Physics::is_accessing = false;

			ImGui::EndPopup();
		}

		ImGui::SameLine();

		ImGui::Checkbox(u8"Ignoruj granice", &Physics::infiniteBounds);

		ImGui::SameLine();

		ImGui::Checkbox("Buforuj kolizje", &Physics::bufferCollisions);

		ImGui::SameLine();
		ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
		ImGui::SameLine();

		ImGui::Checkbox("Staty", &showStats);

		ImGui::SameLine();

		ImGui::Checkbox("Zaawansowane", &showAdvancedSettings);
	}
	if (!hideUI)
		ImGui::End();

	ImGui::PopStyleVar();

	if (showAdvancedSettings && !hideUI)
	{
		ImGui::SetNextWindowSizeConstraints({ 200, 0 }, { FLT_MAX, FLT_MAX });
		if (ImGui::Begin("Zaawansowane", &showAdvancedSettings, ImGuiWindowFlags_AlwaysAutoResize))
		{
			static bool is_setup_open = true;

			if (is_setup_open) {
				ImGui::TreeNodeSetOpen(ImGui::GetID("Renderowanie"), true);
				ImGui::TreeNodeSetOpen(ImGui::GetID(u8"Przypadkowość"), true);
			}

			if (ImGui::TreeNode(u8"Renderowanie"))
			{
				ImGui::Checkbox(u8"Niska Jakość", &advSettings.useLowQuality);

				ImGui::Checkbox("Renderowanie Liniowe", &advSettings.useLineRendering);

				ImGui::ColorEdit4("Kolor", (float*)(&advSettings.particleRenderColor.Value));
				ImGui::SliderFloat("Rozmiar", &advSettings.particleRenderSize, 0, 5);

				ImGui::Checkbox(u8"Kolor Prękości", &advSettings.velocityColorRendering);
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip(u8"Przypisuje szybszym cząsteczkom bardziej czerwone kolory");

				ImGui::Checkbox("Disco!", &advSettings.discoRender);

				ImGui::TreePop();
			}

			if (ImGui::TreeNode(u8"Fizyka"))
			{
				ImGui::Checkbox(u8"Przypadkowa Dystrybucja Cząsteczek", &advSettings.randomPartcileDistribution);

				ImGui::Indent(4);
				ImGui::Text(u8"Maksymalna Prędkość pocz. (V₀) Cząsteczki"); // \x2080 - zero z dolnym indeksem
				ImGui::Unindent(4);
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip(u8"Stała prędkość dla każdej cząsteczki przy wartości = 1, gdyż minimalna prędkości to również 1");
				ImGui::SliderFloat(u8"V₀###RandomVelocityDistribution", &advSettings.particleVelocityDistribution, 1, 15);

				ImGui::Indent(4);
				ImGui::Text(u8"Utrata Energii w Momencie Zderzenia (F)");
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip(u8"Dla wartości 0 prędkość po odbiciu jest równa prędkości przed odbiciem");
				ImGui::Unindent(4);
				ImGui::SliderFloat("F%###EnergyLossSlider", &Physics::collisionEnergyLoss._Storage._Value, 0, 100, "%.1f%%");

				ImGui::TreePop();
			}

			if (ImGui::TreeNode(u8"Inne"))
			{
				if (ImGui::Button(u8"Włącz Tutorial")) {
					tutorialStep = 1;
					showAdvancedSettings = false;
				}

				ImGui::Checkbox(u8"Chowaj interfejs przy umieszczaniu", &advSettings.hideUiWhenPlacing);

				ImGui::TreePop();
			}

			is_setup_open = false;
		}
		ImGui::End();
	}


	if (ImGui::BeginPopupModal("Poradnik 07", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings))
	{
		ImGui::Text(u8"Na dole programu znajduje się panel kontrolny.\nZ niego można wykonywać większość akcji związanych z symulacją i nie tylko!");
		ImGui::Text(u8"Na przykład aby wystartować symulację należy kliknąć przycisk \"Wznów\", ale można również użyć SPACJI.");

		ImGui::Text(u8"ps. nie zapomnij sprawdzić ustawień zaawansowanych :-P");

		ImGui::Separator(); ImGui::Spacing();
		if (ImGui::Button(u8"Wstecz"))
			tutorialStep--;

		ImGui::SameLine(ImGui::GetContentRegionAvail().x - 54);

		if (ImGui::Button("Koniec!"))
		{
			style.Colors[ImGuiCol_ModalWindowDimBg] = backup_color_modal_dim;
			tutorialStep = 0;
			is_placing_object = false;
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
		return 0;
	}

	ImGui::SetCursorPos({ 5, ImGui::GetContentRegionAvail().y - 20 });

#ifdef _DEBUG
	if (!hideUI && ImGui::IsMousePosValid())
		ImGui::Text("(%.0f,%.0f)", ImGui::GetMousePos().x, ImGui::GetMousePos().y);
#endif

	return 0;
}

void OnPhysicsStep()
{
	totalSimSteps++;
	//lastPhysicsStep = micros();
}

int main(int, char** args)
{
	FILE* hFile;
	if (!fopen_s(&hFile, "imgui.ini", "r"))
	{
		tutorialStep = 0;
		printf("Already set-up!\n");
	}
	else
		printf("No Imgui!\n");

	GUI::Setup(OnGui);

	UINT Vsync = 1;
	GUI::VSyncFrame = &Vsync; // Włączenie synchronizacji pionowej na 1 klatkę

#ifdef MULTI_THREADED
	Physics::Setup(&OnPhysicsStep, Physics::is_accessing, totalSimSteps);
#endif

	GUI::LoadFonts();
	GUI::DrawGui();

	//ImGui::GetStyle().AntiAliasedFill = false;
	Physics::boundsExtend = ImGui::GetIO().DisplaySize;

	printf("Bounds: (%.0f, %.0f)\n", Physics::boundsExtend.x, Physics::boundsExtend.y);

	LARGE_INTEGER _temp;
	QueryPerformanceCounter(&_temp);
	StartingTime = _temp.QuadPart;

	while (true)
	{
		// Nie musimy czekać, aby wyświetlić klatkę, gdyż zajmuje się tym DirectX (Vsync). A na dodatek robi to znacznie dokładniej niż user-mode może
		if (GUI::DrawGui())
			break;
		totalFrames++;

#ifndef MULTI_THREADED

		else if (timeNow - lastPhysicsStep >= (1000000 / physicsUpdateFrequency))
		{
			auto simStep = timeNow - lastPhysicsStep;
			Physics::StepSimulation(simStep);

			lastPhysicsStep = timeNow;
			//totalSimSteps++;
		}
#endif
	}

	//Physics::physicsThread.detach();

	return 0;
}
