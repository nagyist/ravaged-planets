
#include <framework/framework.h>
#include <framework/gui/gui.h>
#include <framework/gui/builder.h>
#include <framework/gui/button.h>
#include <framework/gui/drawable.h>
#include <framework/gui/listbox.h>
#include <framework/timer.h>

using namespace std::placeholders;

namespace fw { namespace gui {

enum ids {
  THUMB = 56756,
  UP_BUTTON = 56757,
  DOWN_BUTTON = 56758
};

/** If you click again within this amount of time, then we count your click as "activate" (i.e. double-click) */
static const float ACTIVATE_TIME_MS = 600.0f;

//-----------------------------------------------------------------------------

class listbox_item_selected_property : public property {
private:
  std::function<void(int index)> _on_selected;
public:
  listbox_item_selected_property(std::function<void(int index)> on_selected)
      : _on_selected(on_selected) {
  }

  void apply(widget *widget) {
    dynamic_cast<listbox *>(widget)->sig_item_selected.connect(_on_selected);
  }
};

class listbox_item_activated_property : public property {
private:
  std::function<void(int index)> _on_activated;
public:
  listbox_item_activated_property(std::function<void(int index)> on_activated)
      : _on_activated(on_activated) {
  }

  void apply(widget *widget) {
    dynamic_cast<listbox *>(widget)->sig_item_activated.connect(_on_activated);
  }
};

//-----------------------------------------------------------------------------

/** A special widget that we add children to. This item handles the selection colours and positioning of the item. */
class listbox_item : public widget {
private:
  listbox *_listbox;
  int _index;
  std::shared_ptr<state_drawable> _background;
  float _last_down_time;
public:
  listbox_item(gui *gui);
  virtual ~listbox_item();

  virtual bool on_mouse_down(float x, float y);

  void setup(listbox *listbox, int index);
  int get_index() const;
  void set_selected(bool selected);
  widget *get_widget();

  void render();
};

listbox_item::listbox_item(gui *gui) : widget(gui), _listbox(nullptr), _index(-1), _last_down_time(0) {
  _background = std::shared_ptr<state_drawable>(new state_drawable());
  _background->add_drawable(state_drawable::normal, _gui->get_drawable_manager()->get_drawable("listbox_item_normal"));
  _background->add_drawable(state_drawable::selected, _gui->get_drawable_manager()->get_drawable("listbox_item_selected"));
}

listbox_item::~listbox_item() {
}

/** When you click a listbox item, we want to make sure it's the selected one. */
bool listbox_item::on_mouse_down(float x, float y) {
  _listbox->select_item(_index);
  float now = fw::framework::get_instance()->get_timer()->get_total_time();
  if (now - _last_down_time < (ACTIVATE_TIME_MS / 1000.0f)) {
    _listbox->activate_item(_index);
  }
  _last_down_time = now;
  return true;
}

void listbox_item::setup(listbox *listbox, int index) {
  _listbox = listbox;
  _index = index;
}

int listbox_item::get_index() const {
  return _index;
}

widget *listbox_item::get_widget() {
  return _children[0];
}

void listbox_item::set_selected(bool selected) {
  if (selected) {
    _background->set_current_state(state_drawable::selected);
  } else {
    _background->set_current_state(state_drawable::normal);
  }
}

void listbox_item::render() {
  _background->render(get_left(), get_top(), get_width(), get_height());
  widget::render();
}

//-----------------------------------------------------------------------------

listbox::listbox(gui *gui) : widget(gui), _selected_item(nullptr), _scrollbar_visible(false) {
  _background = gui->get_drawable_manager()->get_drawable("listbox_background");

  std::shared_ptr<state_drawable> bkgnd = std::shared_ptr<state_drawable>(new state_drawable());
  bkgnd->add_drawable(state_drawable::normal, _gui->get_drawable_manager()->get_drawable("listbox_up_normal"));
  bkgnd->add_drawable(state_drawable::hover, _gui->get_drawable_manager()->get_drawable("listbox_up_hover"));
  attach_child(builder<button>(sum(pct(100), px(-19)), px(0), px(19), px(19))
      << button::background(bkgnd) << widget::id(UP_BUTTON) << widget::visible(false)
      << button::click(std::bind(&listbox::on_up_button_click, this, _1)));
  bkgnd = std::shared_ptr<state_drawable>(new state_drawable());
  bkgnd->add_drawable(state_drawable::normal, _gui->get_drawable_manager()->get_drawable("listbox_down_normal"));
  bkgnd->add_drawable(state_drawable::hover, _gui->get_drawable_manager()->get_drawable("listbox_down_hover"));
  attach_child(builder<button>(sum(pct(100), px(-19)), sum(pct(100), px(-19)), px(19), px(19))
      << button::background(bkgnd) << widget::id(DOWN_BUTTON) << widget::visible(false)
      << button::click(std::bind(&listbox::on_down_button_click, this, _1)));
  bkgnd = std::shared_ptr<state_drawable>(new state_drawable());
  bkgnd->add_drawable(state_drawable::normal, _gui->get_drawable_manager()->get_drawable("listbox_thumb_normal"));
  bkgnd->add_drawable(state_drawable::hover, _gui->get_drawable_manager()->get_drawable("listbox_thumb_hover"));
  attach_child(builder<button>(sum(pct(100), px(-19)), px(19), px(19), sum(pct(100), px(-38)))
      << button::background(bkgnd) << widget::id(THUMB) << widget::visible(false));

  _item_container = builder<widget>(px(0), px(0), pct(100), px(0));
  attach_child(_item_container);
}

listbox::~listbox() {
}

property *listbox::item_selected(std::function<void(int index)> on_selected) {
  return new listbox_item_selected_property(on_selected);
}

property *listbox::item_activated(std::function<void(int index)> on_activated) {
  return new listbox_item_activated_property(on_activated);
}

void listbox::add_item(widget *w) {
  int top = _item_container->get_height();
  listbox_item *item = builder<listbox_item>(px(0), px(top), pct(100), px(w->get_height()));
  item->attach_child(w);
  item->setup(this, _items.size());
  _item_container->attach_child(item);
  _item_container->set_height(px(_item_container->get_height() + w->get_height()));
  _items.push_back(item);
  update_thumb_button(true);
}

void listbox::clear() {
  _item_container->clear_children();
  _item_container->set_height(px(0));
  _item_container->set_top(px(0));
  _items.clear();
  update_thumb_button(true);
}

void listbox::update_thumb_button(bool adjust_height) {
  float widget_height = get_height();
  float content_height = _item_container->get_height();
  float thumb_max_height = widget_height - 38.0f; // the up/down buttons
  float thumb_height;
  button *thumb = find<button>(THUMB);

  if (adjust_height) {
    if (content_height <= widget_height) {
      thumb_height = thumb_max_height;
      _scrollbar_visible = false;
    } else {
      // This will be 0.5 when content_height is twice widget height,
      float ratio = widget_height / content_height;
      thumb_height = thumb_max_height * ratio;
      if (thumb_height < 30) {
        thumb_height = 30;
      }
      if (thumb_height >= thumb_max_height) {
        thumb_height = thumb_max_height;
        _scrollbar_visible = false;
      } else {
        _scrollbar_visible = true;
      }
    }
    thumb->set_height(px(thumb_height));
  } else {
    thumb_height = thumb->get_height();
  }

  float offset = get_top() - _item_container->get_top();
  float max_offset = _item_container->get_height() - get_height();
  float offset_ratio = offset / max_offset;

  float max_thumb_offset = thumb_max_height - thumb_height;
  float thumb_offset = max_thumb_offset * offset_ratio;
  thumb->set_top(px(19 + thumb_offset));

  button *up_button = find<button>(UP_BUTTON);
  button *down_button = find<button>(DOWN_BUTTON);
  if (_scrollbar_visible) {
    _item_container->set_width(sum(pct(100), px(-20)));
    thumb->set_visible(true);
    up_button->set_visible(true);
    down_button->set_visible(true);
  } else {
    _item_container->set_width(pct(100));
    thumb->set_visible(false);
    up_button->set_visible(false);
    down_button->set_visible(false);
  }
}

bool listbox::on_down_button_click(widget *w) {
  float current_top = _item_container->get_top() - get_top();
  current_top -= get_height() / 4.0f;
  float max_offset = _item_container->get_height() - get_height();
  if (current_top < -max_offset) {
    current_top = -max_offset;
  }
  _item_container->set_top(px(current_top));
  update_thumb_button(false);
  return true;
}

bool listbox::on_up_button_click(widget *w) {
  float current_top = _item_container->get_top() - get_top();
  current_top += get_height() / 4.0f;
  if (current_top > 0) {
    current_top = 0.0f;
  }
  _item_container->set_top(px(current_top));
  update_thumb_button(false);
  return true;
}

void listbox::select_item(int index) {
  if (_selected_item != nullptr) {
    _selected_item->set_selected(false);
  }
  _selected_item = _items[index];
  _selected_item->set_selected(true);
  sig_item_selected(index);
}

void listbox::activate_item(int index) {
  sig_item_activated(index);
}

int listbox::get_selected_index() {
  if (_selected_item == nullptr) {
    return -1;
  }
  return _selected_item->get_index();
}

widget *listbox::get_item(int index) {
  if (index < 0 || index >= _items.size()) {
    return nullptr;
  }
  return _items[index]->get_widget();
}

widget *listbox::get_selected_item() {
  return _selected_item == nullptr ? nullptr : _selected_item->get_widget();
}

void listbox::render() {
  _background->render(get_left(), get_top(), get_width(), get_height());

  widget::render();
}

} }
