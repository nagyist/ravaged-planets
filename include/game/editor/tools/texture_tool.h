#pragma once

#include <game/editor/tools/tools.h>
/*
class texture_tool_window;

namespace ed {

	// This tool lets you draw the texture on the terrain
	class texture_tool : public tool
	{
	public:
		static float max_radius;

	private:
		int _radius;
		texture_tool_window *_wnd;
		bool _is_painting;

		void on_key(int key, bool is_down);
		uint32_t get_selected_splatt_mask();

	public:
		texture_tool(ww::world *wrld);
		virtual ~texture_tool();

		virtual void activate();
		virtual void deactivate();

		void set_radius(int value) { _radius = value; }
		int get_radius() const { return _radius; }

		virtual void update();
		virtual void render(fw::sg::scenegraph &scenegraph);
	};

}
*/