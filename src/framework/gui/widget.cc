
#include <algorithm>
#include <functional>
#include <memory>

#include <boost/foreach.hpp>

#include <framework/gui/gui.h>
#include <framework/gui/widget.h>

namespace fw { namespace gui {

dimension::dimension() {
}

dimension::~dimension() {
}

pixel_dimension::pixel_dimension(float value) :
    _value(value) {
}

pixel_dimension::~pixel_dimension() {
}

float pixel_dimension::get_value(float parent_value) {
  return _value;
}

percent_dimension::percent_dimension(float value) :
    _value(value) {
}

percent_dimension::~percent_dimension() {
}

float percent_dimension::get_value(float parent_value) {
  return parent_value * (_value / 100.0f);
}

sum_dimension::sum_dimension(std::shared_ptr<dimension> one, std::shared_ptr<dimension> two) :
    _one(one), _two(two) {
}

sum_dimension::~sum_dimension() {
}

float sum_dimension::get_value(float parent_value) {
  return _one->get_value(parent_value) + _two->get_value(parent_value);
}

//-----------------------------------------------------------------------------

position_property::position_property(std::shared_ptr<dimension> x, std::shared_ptr<dimension> y) :
    _x(x), _y(y) {
}

void position_property::apply(widget *widget) {
  widget->_x = _x;
  widget->_y = _y;
}

size_property::size_property(std::shared_ptr<dimension> width, std::shared_ptr<dimension> height) :
    _width(width), _height(height) {
}

void size_property::apply(widget *widget) {
  widget->_width = _width;
  widget->_height = _height;
}

//-----------------------------------------------------------------------------
class widget_click_property : public property {
private:
  std::function<bool(widget *)> _on_click;
public:
  widget_click_property(std::function<bool(widget *)> on_click)
      : _on_click(on_click) {
  }

  void apply(widget *widget) {
    widget->_on_click = _on_click;
  }
};

//-----------------------------------------------------------------------------

widget::widget(gui *gui) :
    _gui(gui), _parent(nullptr) {
}

widget::~widget() {
}

property *widget::click(std::function<bool(widget *)> on_click) {
  return new widget_click_property(on_click);
}

void widget::attach_child(widget *child) {
  if (child->_parent != nullptr) {
    child->_parent->detach_child(child);
  }
  child->_parent = this;
  _children.push_back(child);
  child->on_attached_to_parent(this);
}

void widget::detach_child(widget *child) {
  _children.erase(std::find(_children.begin(), _children.end(), child));
  child->_parent = nullptr;
}

void widget::on_attached_to_parent(widget *parent) {
}

void widget::render() {
  BOOST_FOREACH(widget *child, _children) {
    child->render();
  }
}

bool widget::on_mouse_down() {
  return _on_click != nullptr;
}

bool widget::on_mouse_up() {
  if (_on_click) {
    return _on_click(this);
  }
  return false;
}

widget *widget::get_child_at(float x, float y) {
  float left = get_left();
  float top = get_top();
  float right = left + get_width();
  float bottom = top + get_height();

  // If we're outside the given (x,y) then return null.
  if (x < left || y < top || x >= right || y >= bottom) {
    return nullptr;
  }

  // If one of our children is within the (x,y) then return that.
  BOOST_FOREACH(widget *child, _children) {
    widget *found = child->get_child_at(x, y);
    if (found != nullptr) {
      return found;
    }
  }

  // Otherwise, return ourselves.
  return this;
}

float widget::get_top() {
  float parent_top = (_parent != nullptr) ? _parent->get_top() : 0;
  float parent_size = (_parent != nullptr) ? _parent->get_height() : _gui->get_height();
  return parent_top + _y->get_value(parent_size);
}

float widget::get_left() {
  float parent_left = (_parent != nullptr) ? _parent->get_left() : 0;
  float parent_size = (_parent != nullptr) ? _parent->get_width() : _gui->get_width();
  return parent_left + _x->get_value(parent_size);
}

float widget::get_width() {
  float parent_size = (_parent != nullptr) ? _parent->get_width() : _gui->get_width();
  return _width->get_value(parent_size);
}

float widget::get_height() {
  float parent_size = (_parent != nullptr) ? _parent->get_height() : _gui->get_height();
  return _height->get_value(parent_size);
}

} }
