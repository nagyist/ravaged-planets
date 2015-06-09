#pragma once

#include <game/editor/tools/tools.h>
/*
class heightfield_tool_window;

// the heightfield tool is actually made up of a number of different
// "brushes" which actually control how we adjust the terrain
class heightfield_brush;

namespace fw {
	class bitmap;
}

namespace ed {

	// this tool modifies the heightfield, letting to raise/lower it via the
	// mouse.
	class heightfield_tool : public tool
	{
	public:
		static float max_radius;

	private:
		int _radius;
		heightfield_tool_window *_wnd;
		heightfield_brush *_brush;

		void on_key(int key, bool is_down);

	public:
		heightfield_tool(ww::world *wrld);
		virtual ~heightfield_tool();

		virtual void activate();
		virtual void deactivate();

		void set_radius(int value) { _radius = value; }
		int get_radius() const { return _radius; }

		// imports the height data from the given fw::bitmap
		void import_heightfield(fw::bitmap &bm);

		// sets the brush to the current value (this is done by the tool window)
		void set_brush(heightfield_brush *brush);

		virtual void update();
		virtual void render(fw::sg::scenegraph &scenegraph);
	};

}
*/
